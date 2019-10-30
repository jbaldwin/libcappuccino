#include "cappuccino/TlruCache.h"

#include <numeric>

namespace cappuccino {

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
TlruCache<KeyType, ValueType, SyncType>::TlruCache(
    size_t capacity,
    float max_load_factor)
    : m_elements(capacity)
    , m_lru_list(capacity)
{
    std::iota(m_lru_list.begin(), m_lru_list.end(), 0);
    m_lru_end = m_lru_list.begin();

    m_keyed_elements.max_load_factor(max_load_factor);
    m_keyed_elements.reserve(capacity);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto TlruCache<KeyType, ValueType, SyncType>::InsertOnly(
    std::chrono::seconds ttl,
    const KeyType& key,
    ValueType value) -> bool
{
    auto now = std::chrono::steady_clock::now();
    auto expire_time = now + ttl;

    std::lock_guard guard { m_lock };
    return doInsertUpdate(key, std::move(value), now, expire_time, true);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto TlruCache<KeyType, ValueType, SyncType>::Insert(
    std::chrono::seconds ttl,
    const KeyType& key,
    ValueType value) -> void
{
    auto now = std::chrono::steady_clock::now();
    auto expire_time = now + ttl;

    std::lock_guard guard { m_lock };
    doInsertUpdate(key, std::move(value), now, expire_time);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
template <typename RangeType>
auto TlruCache<KeyType, ValueType, SyncType>::InsertRange(
    RangeType&& key_value_range) -> void
{
    auto now = std::chrono::steady_clock::now();

    std::lock_guard guard { m_lock };
    for (auto& [ttl, key, value] : key_value_range) {
        auto expired_time = now + ttl;
        doInsertUpdate(key, std::move(value), now, expired_time);
    }
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto TlruCache<KeyType, ValueType, SyncType>::Delete(
    const KeyType& key) -> bool
{
    std::lock_guard guard { m_lock };
    auto keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end()) {
        doDelete(keyed_position->second);
        return true;
    } else {
        return false;
    }
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
template <template <class...> typename RangeType>
auto TlruCache<KeyType, ValueType, SyncType>::DeleteRange(
    const RangeType<KeyType>& key_range) -> size_t
{
    size_t deleted_elements { 0 };

    std::lock_guard guard { m_lock };
    for (auto& key : key_range) {
        auto keyed_position = m_keyed_elements.find(key);
        if (keyed_position != m_keyed_elements.end()) {
            ++deleted_elements;
            doDelete(keyed_position->second);
        }
    }

    return deleted_elements;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto TlruCache<KeyType, ValueType, SyncType>::Find(
    const KeyType& key,
    bool peek) -> std::optional<ValueType>
{
    auto now = std::chrono::steady_clock::now();

    std::lock_guard guard { m_lock };
    return doFind(key, now, peek);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
template <typename RangeType>
auto TlruCache<KeyType, ValueType, SyncType>::FindRange(
    const RangeType& key_range,
    bool peek) -> std::vector<std::pair<KeyType, std::optional<ValueType>>>
{
    std::vector<std::pair<KeyType, std::optional<ValueType>>> output;
    output.reserve(std::size(key_range));

    auto now = std::chrono::steady_clock::now();

    {
        std::lock_guard guard { m_lock };
        for (auto& key : key_range) {
            output.emplace_back(key, doFind(key, now, peek));
        }
    }

    return output;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
template <typename RangeType>
auto TlruCache<KeyType, ValueType, SyncType>::FindRangeFill(
    RangeType& key_optional_value_range,
    bool peek) -> void
{
    auto now = std::chrono::steady_clock::now();

    std::lock_guard guard { m_lock };
    for (auto& [key, optional_value] : key_optional_value_range) {
        optional_value = doFind(key, now, peek);
    }
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto TlruCache<KeyType, ValueType, SyncType>::GetUsedSize() const -> size_t
{
    return m_used_size;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto TlruCache<KeyType, ValueType, SyncType>::GetCapacity() const -> size_t
{
    return m_elements.capacity();
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto TlruCache<KeyType, ValueType, SyncType>::CleanExpiredValues() -> size_t
{
    size_t start_size = m_ttl_list.size();
    auto now = std::chrono::steady_clock::now();

    std::lock_guard guard { m_lock };
    // Loop through and delete all items that are expired.
    while (m_used_size > 0 && now >= m_ttl_list.begin()->first) {
        doDelete(m_ttl_list.begin()->second);
    }

    // return the number of items removed.
    return start_size - m_ttl_list.size();
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto TlruCache<KeyType, ValueType, SyncType>::doInsertUpdate(
    const KeyType& key,
    ValueType&& value,
    std::chrono::steady_clock::time_point now,
    std::chrono::steady_clock::time_point expire_time,
    bool insert_only) -> bool
{
    auto keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end()) {
        // If the key already exists this is an update, this won't require a prune.

        const size_t element_idx = keyed_position->second;
        const bool expired = (now >= m_elements[element_idx].m_expire_time);

        if (!insert_only || expired)
        {
            Element& element = doUpdate(keyed_position, expire_time);
            element.m_value = std::move(value);
            return true;
        }

        return false;
    } else {
        // Inserts might require an item to be pruned, check that first before inserting the new key/value.
        if (m_used_size >= m_elements.size()) {
            doPrune(now);
        }
        doInsert(key, std::move(value), expire_time);
        return true;
    }
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto TlruCache<KeyType, ValueType, SyncType>::doInsert(
    const KeyType& key,
    ValueType&& value,
    std::chrono::steady_clock::time_point expire_time) -> void
{
    auto element_idx = *m_lru_end;

    auto keyed_position = m_keyed_elements.emplace(key, element_idx).first;

    // Insert the element_idx into the TTL list.
    auto ttl_position = m_ttl_list.emplace(expire_time, element_idx);

    // Update the element's appropriate fields across the datastructures.
    Element& element = m_elements[element_idx];
    element.m_value = std::move(value);
    element.m_expire_time = expire_time;
    element.m_lru_position = m_lru_end;
    element.m_ttl_position = ttl_position;
    element.m_keyed_position = keyed_position;

    // Update the LRU position.
    ++m_lru_end;

    ++m_used_size;

    doAccess(element);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto TlruCache<KeyType, ValueType, SyncType>::doUpdate(
    typename std::unordered_map<KeyType, size_t>::iterator keyed_position,
    std::chrono::steady_clock::time_point expire_time) -> Element&
{
    size_t element_idx = keyed_position->second;

    Element& element = m_elements[element_idx];
    element.m_expire_time = expire_time;

    // Reinsert into TTL list with the new TTL.
    m_ttl_list.erase(element.m_ttl_position);
    element.m_ttl_position = m_ttl_list.emplace(expire_time, element_idx);

    doAccess(element);
    return element;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto TlruCache<KeyType, ValueType, SyncType>::doDelete(
    size_t element_idx) -> void
{
    Element& element = m_elements[element_idx];

    // splice into the end position of the LRU if it isn't the last item...
    if (element.m_lru_position != std::prev(m_lru_end)) {
        m_lru_list.splice(m_lru_end, m_lru_list, element.m_lru_position);
    }
    --m_lru_end;

    m_ttl_list.erase(element.m_ttl_position);

    m_keyed_elements.erase(element.m_keyed_position);

    // destruct element.m_value when re-assigned on an insert.

    --m_used_size;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto TlruCache<KeyType, ValueType, SyncType>::doFind(
    const KeyType& key,
    std::chrono::steady_clock::time_point now,
    bool peek) -> std::optional<ValueType>
{
    auto keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end()) {
        size_t element_idx = keyed_position->second;
        Element& element = m_elements[element_idx];

        // Has the element TTL'ed?
        if (now < element.m_expire_time) {
            // Do not update the items access if peeking.
            if (!peek) {
                doAccess(element);
            }
            return { element.m_value };
        } else {
            // Its dead anyways, lets delete it now.
            doDelete(element_idx);
        }
    }

    return {};
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto TlruCache<KeyType, ValueType, SyncType>::doAccess(
    Element& element) -> void
{
    // This function will put the item at the most recently used side of the LRU list.
    m_lru_list.splice(m_lru_list.begin(), m_lru_list, element.m_lru_position);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto TlruCache<KeyType, ValueType, SyncType>::doPrune(
    std::chrono::steady_clock::time_point now) -> void
{
    if (m_used_size > 0) {
        auto& [expire_time, element_idx] = *m_ttl_list.begin();

        if (now >= expire_time) {
            // If there is an expired item, prefer to remove that.
            doDelete(element_idx);
        } else {
            // Otherwise pick the least recently used item to prune.
            size_t lru_idx = m_lru_list.back();
            doDelete(lru_idx);
        }
    }
}

} // namespace cappuccino
