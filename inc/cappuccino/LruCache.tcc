#include "cappuccino/LruCache.h"

namespace cappuccino {

template <typename KeyType, typename ValueType>
LruCache<KeyType, ValueType>::LruCache(
    size_t capacity)
    : m_elements(capacity)
    , m_keyed_elements(capacity)
    , m_lru_list(capacity)
{
    std::iota(m_lru_list.begin(), m_lru_list.end(), 0);

    m_lru_end = m_lru_list.begin();

    m_keyed_elements.rehash(capacity);
}

template <typename KeyType, typename ValueType>
auto LruCache<KeyType, ValueType>::Insert(
    std::chrono::seconds ttl,
    const KeyType& key,
    ValueType value) -> void
{
    auto now = std::chrono::steady_clock::now();
    auto expire_time = now + ttl;

    std::lock_guard guard { m_lock };
    doInsertUpdate(key, std::move(value), now, expire_time);
}

template <typename KeyType, typename ValueType>
template <typename RangeType>
auto LruCache<KeyType, ValueType>::InsertRange(
    RangeType key_value_range) -> void
{
    auto now = std::chrono::steady_clock::now();

    std::lock_guard guard { m_lock };
    for (auto& [ttl, key, value] : key_value_range) {
        auto expired_time = now + ttl;
        doInsertUpdate(key, std::move(value), now, expired_time);
    }
}

template <typename KeyType, typename ValueType>
auto LruCache<KeyType, ValueType>::Delete(
    const KeyType& key) -> bool
{
    std::lock_guard guard { m_lock };
    auto keyed_pos = m_keyed_elements.find(key);
    if (keyed_pos != m_keyed_elements.end()) {
        doDelete(keyed_pos->second);
        return true;
    } else {
        return false;
    }
}

template <typename KeyType, typename ValueType>
template <typename RangeType>
auto LruCache<KeyType, ValueType>::DeleteRange(
    RangeType key_range) -> size_t
{
    size_t deleted_elements { 0 };

    std::lock_guard guard { m_lock };

    for (auto& key : key_range) {
        auto keyed_position = m_keyed_elements.find(key);
        if (keyed_position != m_keyed_elements.end()) {
            ++deleted_elements;

            size_t element_idx = keyed_position->second;
            doDelete(element_idx);
        }
    }

    return deleted_elements;
}

template <typename KeyType, typename ValueType>
auto LruCache<KeyType, ValueType>::Find(
    const KeyType& key) -> std::optional<ValueType>
{
    auto now = std::chrono::steady_clock::now();

    std::lock_guard guard { m_lock };
    return doFind(key, now);
}

template <typename KeyType, typename ValueType>
template <typename RangeType>
auto LruCache<KeyType, ValueType>::FindRange(
    RangeType& key_optional_value_range) -> void
{
    std::lock_guard guard { m_lock };

    for (auto& [key, optional_value] : key_optional_value_range) {
        optional_value = doFind(key);
    }
}

template <typename KeyType, typename ValueType>
auto LruCache<KeyType, ValueType>::doInsertUpdate(
    const KeyType& key,
    ValueType value,
    std::chrono::steady_clock::time_point now,
    std::chrono::steady_clock::time_point expire_time) -> void
{
    auto keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end()) {
        // If the key already exists this is an update, this won't require a prune.
        doUpdate(keyed_position, key, std::move(value), expire_time);
    } else {
        // Inserts might require an item to be pruned, check that first before inserting the new key/value.
        if (m_used_size >= m_elements.size()) {
            doPrune(now);
        }
        doInsert(key, std::move(value), expire_time);
    }
}

template <typename KeyType, typename ValueType>
auto LruCache<KeyType, ValueType>::doInsert(
    const KeyType& key,
    ValueType&& value,
    std::chrono::steady_clock::time_point expire_time) -> void
{
    auto lru_position = m_lru_end;
    auto element_idx = *lru_position;

    auto keyed_position = m_keyed_elements.emplace(key, element_idx).first;

    // Insert the element_idx into the TTL list.
    auto ttl_position = m_ttl_list.insert({ expire_time, element_idx }).first;

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

template <typename KeyType, typename ValueType>
auto LruCache<KeyType, ValueType>::doUpdate(
    typename std::unordered_map<KeyType, size_t>::iterator keyed_position,
    const KeyType& key,
    ValueType&& value,
    std::chrono::steady_clock::time_point expire_time) -> void
{
    size_t element_idx = keyed_position->second;

    doAccess(element_idx);

    Element& element = m_elements[element_idx];
    element.m_expire_time = expire_time;

    // Reinsert into TTL list with the new TTL.
    m_ttl_list.erase(element.m_ttl_position);
    element.m_ttl_position = m_ttl_list.insert({ expire_time, element_idx }).first;
}

template <typename KeyType, typename ValueType>
auto LruCache<KeyType, ValueType>::doFind(
    const KeyType& key,
    std::chrono::steady_clock::time_point now) -> std::optional<ValueType>
{
    auto keyed_pos = m_keyed_elements.find(key);
    if (keyed_pos != m_keyed_elements.end()) {
        size_t element_idx = keyed_pos->second;
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

template <typename KeyType, typename ValueType>
auto LruCache<KeyType, ValueType>::doDelete(
    size_t element_idx) -> void
{
    Element& element = m_elements[element_idx];

    // If this is the last item in the lru, 'delete' it.
    if (element.m_lru_position == std::prev(m_lru_end)) {
        // This will effectively delete the item, by putting it into un-allocated space.
        m_lru_end = std::prev(m_lru_end);
    } else {
        m_lru_list.splice(m_lru_end, m_lru_list, element.m_lru_position);
        m_lru_end = std::prev(m_lru_end);
    }

    m_ttl_list.erase(element.m_ttl_position);
    m_keyed_elements.erase(element.m_keyed_position);

    // destruct element.m_value when re-assigned on an insert.

    --m_used_size;
}

template <typename KeyType, typename ValueType>
auto LruCache<KeyType, ValueType>::doAccess(
    size_t element_idx) -> void
{
    // This function will put the item at the most recently used side of the LRU list.
    Element& element = m_elements[element_idx];
    m_lru_list.splice(m_lru_list.begin(), m_lru_list, element.m_lru_position);
}

template <typename KeyType, typename ValueType>
auto LruCache<KeyType, ValueType>::doPrune(
    std::chrono::steady_clock::time_point now) -> void
{
    if (m_used_size > 0) {
        size_t element_idx = m_ttl_list.begin()->second;
        Element& element = m_elements[element_idx];

        if (now >= element.m_expire_time) {
            // If there is an expired item, prefer to remove that.
            doDelete(element_idx);
        } else {
            // Otherwise pick the least recently used item to prune.
            size_t lru_idx = m_lru_list.back();
            doDelete(lru_idx);
        }
    }
}

template <typename KeyType, typename ValueType>
auto LruCache<KeyType, ValueType>::GetUsedSize() const -> size_t
{
    return m_used_size;
}

template <typename KeyType, typename ValueType>
auto LruCache<KeyType, ValueType>::GetCapacity() const -> size_t
{
    return m_elements.capacity();
}

template <typename KeyType, typename ValueType>
auto LruCache<KeyType, ValueType>::CleanExpiredValues() -> size_t
{
    size_t start_size = m_ttl_list.size();

    {
        std::lock_guard guard { m_lock };
        auto now = std::chrono::steady_clock::now();

        // Loop through and delete all items that are expired.
        while (!m_ttl_list.empty() && now >= m_ttl_list.begin()->first) {
            doDelete(m_ttl_list.begin()->second);
        }
    }

    // return the number of items removed.
    return start_size - m_ttl_list.size();
}

} // namespace cappuccino
