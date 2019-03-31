#include "cappuccino/TlruCache.h"
#include "cappuccino/LockScopeGuard.h"

#include <numeric>

namespace cappuccino {

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
TlruCache<KeyType, ValueType, SyncType>::TlruCache(
    size_t capacity,
    float max_load_factor)
    : m_elements(capacity)
    , m_keyed_elements(capacity)
    , m_lru_list(capacity)
{
    std::iota(m_lru_list.begin(), m_lru_list.end(), 0);

    m_lru_end = m_lru_list.begin();

    m_keyed_elements.max_load_factor(max_load_factor);
    m_keyed_elements.reserve(capacity);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto TlruCache<KeyType, ValueType, SyncType>::Insert(
    std::chrono::seconds ttl,
    const KeyType& key,
    ValueType value) -> void
{
    auto now = std::chrono::steady_clock::now();
    auto expire_time = now + ttl;

    LockScopeGuard<SyncType> guard{m_lock};
    doInsertUpdate(key, std::move(value), now, expire_time);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
template <template <class...> typename RangeType>
auto TlruCache<KeyType, ValueType, SyncType>::InsertRange(
    RangeType<KeyValue> key_value_range) -> void
{
    auto now = std::chrono::steady_clock::now();

    LockScopeGuard<SyncType> guard{m_lock};
    for (auto& [ttl, key, value] : key_value_range) {
        auto expired_time = now + ttl;
        doInsertUpdate(key, std::move(value), now, expired_time);
    }
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
template <template <class...> typename RangeType, template <class, class, class> typename TupleType>
auto TlruCache<KeyType, ValueType, SyncType>::InsertRange(
    RangeType<TupleType<std::chrono::seconds, KeyValue, ValueType>> key_value_range) -> void
{
    auto now = std::chrono::steady_clock::now();

    LockScopeGuard<SyncType> guard{m_lock};
    for (auto& [ttl, key, value] : key_value_range) {
        auto expired_time = now + ttl;
        doInsertUpdate(key, std::move(value), now, expired_time);
    }
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto TlruCache<KeyType, ValueType, SyncType>::Delete(
    const KeyType& key) -> bool
{
    LockScopeGuard<SyncType> guard{m_lock};
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

    LockScopeGuard<SyncType> guard{m_lock};
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
    const KeyType& key) -> std::optional<ValueType>
{
    auto now = std::chrono::steady_clock::now();

    LockScopeGuard<SyncType> guard{m_lock};
    return doFind(key, now);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
template <template <class, class...> typename RangeType>
auto TlruCache<KeyType, ValueType, SyncType>::FindRange(
    RangeType<KeyType, std::optional<ValueType>>& key_optional_value_range) -> void
{
    LockScopeGuard<SyncType> guard{m_lock};
    for (auto& [key, optional_value] : key_optional_value_range) {
        optional_value = doFind(key);
    }
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
template <template <class...> typename RangeType, template <class, class> typename PairType>
auto TlruCache<KeyType, ValueType, SyncType>::FindRange(
    RangeType<PairType<KeyType, std::optional<ValueType>>>& key_optional_value_range) -> void
{
    LockScopeGuard<SyncType> guard{m_lock};
    for (auto& [key, optional_value] : key_optional_value_range) {
        optional_value = doFind(key);
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

    LockScopeGuard<SyncType> guard{m_lock};
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
    std::chrono::steady_clock::time_point expire_time) -> void
{
    auto keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end()) {
        // If the key already exists this is an update, this won't require a prune.
        doUpdate(keyed_position, std::move(value), expire_time);
    } else {
        // Inserts might require an item to be pruned, check that first before inserting the new key/value.
        if (m_used_size >= m_elements.size()) {
            doPrune(now);
        }
        doInsert(key, std::move(value), expire_time);
    }
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto TlruCache<KeyType, ValueType, SyncType>::doInsert(
    const KeyType& key,
    ValueType&& value,
    std::chrono::steady_clock::time_point expire_time) -> void
{
    auto lru_position = m_lru_end;
    auto element_idx = *lru_position;

    auto keyed_position = m_keyed_elements.emplace(key, element_idx).first;

    // Insert the element_idx into the TTL list.
    auto ttl_position = m_ttl_list.insert({ expire_time, element_idx });

    // Update the element's appropriate fields across the datastructures.
    Element& element = m_elements[element_idx];
    element.m_value = std::move(value);
    element.m_expire_time = expire_time;
    element.m_lru_position = lru_position;
    element.m_ttl_position = ttl_position;
    element.m_keyed_position = keyed_position;

    // Update the LRU position.
    ++m_lru_end;

    ++m_used_size;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto TlruCache<KeyType, ValueType, SyncType>::doUpdate(
    typename std::unordered_map<KeyType, size_t>::iterator keyed_position,
    ValueType&& value,
    std::chrono::steady_clock::time_point expire_time) -> void
{
    size_t element_idx = keyed_position->second;

    doAccess(element_idx);

    Element& element = m_elements[element_idx];
    element.m_expire_time = expire_time;

    // Reinsert into TTL list with the new TTL.
    m_ttl_list.erase(element.m_ttl_position);
    element.m_ttl_position = m_ttl_list.insert({expire_time, element_idx});
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
    std::chrono::steady_clock::time_point now) -> std::optional<ValueType>
{
    auto keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end()) {
        size_t element_idx = keyed_position->second;
        Element& element = m_elements[element_idx];

        // Has the element TTL'ed?
        if (now < element.m_expire_time) {
            doAccess(element_idx);
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
    size_t element_idx) -> void
{
    // This function will put the item at the most recently used side of the LRU list.
    Element& element = m_elements[element_idx];
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
