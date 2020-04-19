#pragma once

#include "cappuccino/CappuccinoLock.hpp"
#include "cappuccino/SyncImplEnum.hpp"

#include <list>
#include <map>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace cappuccino {

/**
 * Least Frequently Used (LFU) Cache.
 * Each key value pair is evicted based on being the least frequently used, and no other
 * criteria.
 *
 * This cache is sync aware and can be used concurrently from multiple threads safely.
 * To remove locks/synchronization use SyncImplEnum::UNSYNC when creating the cache.
 *
 * @tparam KeyType The key type.  Must support std::hash() and operator<().
 * @tparam ValueType The value type.  This is returned by copy on a find, so if your data
 *                   structure value is large it is advisable to store in a shared ptr.
 * @tparam SyncType By default this cache is thread safe, can be disabled for caches specific
 *                  to a single thread.
 */
template <typename KeyType, typename ValueType, SyncImplEnum SyncType = SyncImplEnum::SYNC>
class LfuCache {
private:
    struct Element;

    using OpenListIterator = typename std::list<Element>::iterator;
    using LfuIterator = typename std::multimap<size_t, OpenListIterator>::iterator;
    using KeyedIterator = typename std::unordered_map<KeyType, OpenListIterator>::iterator;

public:
    struct KeyValue {
        KeyValue(
            KeyType key,
            ValueType value)
            : m_key(std::move(key))
            , m_value(std::move(value))
        {
        }

        KeyType m_key;
        ValueType m_value;
    };

    /**
     * @param capacity The maximum number of key value pairs allowed in the cache.
     * @param max_load_factor The load factor for the hash map, generally 1 is a good default.
     */
    explicit LfuCache(
        size_t capacity,
        float max_load_factor = 1.0f);

    /**
     * Inserts or updates the given key value pair.
     * @param key The key to store the value under.
     * @param value The value of the data to store.
     */
    auto Insert(
        const KeyType& key,
        ValueType value) -> void;

    /**
     * Inserts or updates a range of key value pairs.  This expects a container
     * that has 2 values in the {KeyType, ValueType} ordering.
     * There is a simple struct provided on the LfuCache::KeyValue that can be put
     * into any iterable container to satisfy this requirement.
     * @tparam RangeType A container with two items, KeyType, ValueType.
     * @param key_value_range The elements to insert or update into the cache.
     */
    template <typename RangeType>
    auto InsertRange(
        RangeType&& key_value_range) -> void;

    /**
     * Attempts to delete the given key.
     * @param key The key to delete from the lru cache.
     * @return True if the key was deleted, false if the key does not exist.
     */
    auto Delete(
        const KeyType& key) -> bool;

    /**
     * Attempts to delete all given keys.
     * @tparam RangeType A container with the set of keys to delete, e.g. vector<KeyType>, set<KeyType>.
     * @param key_range The keys to delete from the cache.
     * @return The number of items deleted from the cache.
     */
    template <template <class...> typename RangeType>
    auto DeleteRange(
        const RangeType<KeyType>& key_range) -> size_t;

    /**
     * Attempts to find the given key's value.
     * @param key The key to lookup its value.
     * @param peek Should the find act like the item wasn't used?
     * @return An optional with the key's value if it exists, or an empty optional if it does not.
     */
    auto Find(
        const KeyType& key,
        bool peek = false) -> std::optional<ValueType>;

    /**
     * Attempts to find the given key's value and use count.
     * @param key The key to lookup its value.
     * @param peek Should the find act like the item wasn't used?
     * @return An optional with the key's value and use count if it exists, or empty optional if it does not.
     */
    auto FindWithUseCount(
        const KeyType& key,
        bool peek = false) -> std::optional<std::pair<ValueType, size_t>>;

    /**
     * Attempts to find all the given keys values.
     * @tparam RangeType A container with the set of keys to find their values, e.g. vector<KeyType>.
     * @param key_range The keys to lookup their pairs.
     * @param peek Should the find act like all the items were not used?
     * @return The full set of keys to std::nullopt if the key wasn't found, or the value if found.
     */
    template <typename RangeType>
    auto FindRange(
        const RangeType& key_range,
        bool peek = false) -> std::vector<std::pair<KeyType, std::optional<ValueType>>>;

    /**
     * Attempts to find all the given keys values.
     *
     * The user should initialize this container with the keys to lookup with the values as all
     * empty optionals.  The keys that are found will have the optionals filled in with the
     * appropriate values from the cache.
     *
     * @tparam RangeType A container with a pair of optional items,
     *                   e.g. vector<pair<KeyType, optional<ValueType>>>
     *                   or map<KeyType, optional<ValueType>>
     * @param key_optional_value_range The keys to optional values to fill out.
     * @param peek Should the find act like all the items were not used?
     */
    template <typename RangeType>
    auto FindRangeFill(
        RangeType& key_optional_value_range,
        bool peek = false) -> void;

    /**
     * @return The number of elements inside the cache.
     */
    auto GetUsedSize() const -> size_t;

    /**
     * The maximum capacity of this cache.
     * @return
     */
    auto GetCapacity() const -> size_t;

private:
    struct Element {
        /// The iterator into the keyed data structure.
        KeyedIterator m_keyed_position;
        /// The iterator into the lfu data structure.
        LfuIterator m_lfu_position;
        /// The element's value.
        ValueType m_value;
    };

    auto doInsertUpdate(
        const KeyType& key,
        ValueType&& value) -> void;

    auto doInsert(
        const KeyType& key,
        ValueType&& value) -> void;

    auto doUpdate(
        KeyedIterator keyed_position,
        ValueType&& value) -> void;

    auto doDelete(
        OpenListIterator open_list_position) -> void;

    auto doFind(
        const KeyType& key,
        bool peek) -> std::optional<ValueType>;

    auto doFindWithUseCount(
        const KeyType& key,
        bool peek) -> std::optional<std::pair<ValueType, size_t>>;

    auto doAccess(
        Element& element) -> void;

    auto doPrune() -> void;

    /// Cache lock for all mutations if sync is enabled.
    CappuccinoLock<SyncType> m_lock;

    /// The current number of elements in the cache.
    size_t m_used_size { 0 };

    /// The open list of un-used slots in 'm_elements'.
    std::list<Element> m_open_list;
    /// The end of the open list to pull open slots from.
    typename std::list<Element>::iterator m_open_list_end;
    /// The keyed lookup data structure, the value is the index into 'm_elements'.
    std::unordered_map<KeyType, OpenListIterator> m_keyed_elements;
    /// The lfu sorted map, the key is the number of times the element has been used,
    /// the value is the index into 'm_elements'.
    std::multimap<size_t, OpenListIterator> m_lfu_list;
};

} // namespace cappuccino

namespace cappuccino {

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
LfuCache<KeyType, ValueType, SyncType>::LfuCache(
    size_t capacity,
    float max_load_factor)
    : m_open_list(capacity)
{
    m_open_list_end = m_open_list.begin();

    m_keyed_elements.max_load_factor(max_load_factor);
    m_keyed_elements.reserve(capacity);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto LfuCache<KeyType, ValueType, SyncType>::Insert(
    const KeyType& key, ValueType value) -> void
{
    std::lock_guard guard { m_lock };
    doInsertUpdate(key, std::move(value));
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
template <typename RangeType>
auto LfuCache<KeyType, ValueType, SyncType>::InsertRange(
    RangeType&& key_value_range) -> void
{
    std::lock_guard guard { m_lock };
    for (auto& [key, value] : key_value_range) {
        doInsertUpdate(key, std::move(value));
    }
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto LfuCache<KeyType, ValueType, SyncType>::Delete(
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
auto LfuCache<KeyType, ValueType, SyncType>::DeleteRange(
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
auto LfuCache<KeyType, ValueType, SyncType>::Find(
    const KeyType& key, bool peek) -> std::optional<ValueType>
{
    std::lock_guard guard { m_lock };
    return doFind(key, peek);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto LfuCache<KeyType, ValueType, SyncType>::FindWithUseCount(
    const KeyType& key,
    bool peek) -> std::optional<std::pair<ValueType, size_t>>
{
    std::lock_guard guard { m_lock };
    return doFindWithUseCount(key, peek);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
template <typename RangeType>
auto LfuCache<KeyType, ValueType, SyncType>::FindRange(
    const RangeType& key_range,
    bool peek) -> std::vector<std::pair<KeyType, std::optional<ValueType>>>
{
    std::vector<std::pair<KeyType, std::optional<ValueType>>> output;
    output.reserve(std::size(key_range));

    {
        std::lock_guard guard { m_lock };
        for (auto& key : key_range) {
            output.emplace_back(key, doFind(key, peek));
        }
    }

    return output;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
template <typename RangeType>
auto LfuCache<KeyType, ValueType, SyncType>::FindRangeFill(
    RangeType& key_optional_value_range, bool peek) -> void
{
    std::lock_guard guard { m_lock };
    for (auto& [key, optional_value] : key_optional_value_range) {
        optional_value = doFind(key, peek);
    }
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto LfuCache<KeyType, ValueType, SyncType>::GetUsedSize() const -> size_t
{
    return m_used_size;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto LfuCache<KeyType, ValueType, SyncType>::GetCapacity() const -> size_t
{
    return m_open_list.size();
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto LfuCache<KeyType, ValueType, SyncType>::doInsertUpdate(
    const KeyType& key,
    ValueType&& value) -> void
{
    auto keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end()) {
        doUpdate(keyed_position, std::move(value));
    } else {
        doInsert(key, std::move(value));
    }
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto LfuCache<KeyType, ValueType, SyncType>::doInsert(
    const KeyType& key,
    ValueType&& value) -> void
{
    if (m_used_size >= m_open_list.size()) {
        doPrune();
    }

    Element& element = *m_open_list_end;

    auto keyed_position = m_keyed_elements.emplace(key, m_open_list_end).first;
    auto lfu_position = m_lfu_list.emplace(1, m_open_list_end);

    element.m_value = std::move(value);
    element.m_keyed_position = keyed_position;
    element.m_lfu_position = lfu_position;

    ++m_open_list_end;

    ++m_used_size;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto LfuCache<KeyType, ValueType, SyncType>::doUpdate(
    LfuCache::KeyedIterator keyed_position,
    ValueType&& value) -> void
{
    Element& element = *keyed_position->second;
    element.m_value = std::move(value);

    doAccess(element);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto LfuCache<KeyType, ValueType, SyncType>::doDelete(
    OpenListIterator open_list_position) -> void
{
    Element& element = *open_list_position;

    if (open_list_position != std::prev(m_open_list_end)) {
        m_open_list.splice(m_open_list_end, m_open_list, open_list_position);
    }
    --m_open_list_end;

    m_keyed_elements.erase(element.m_keyed_position);
    m_lfu_list.erase(element.m_lfu_position);

    --m_used_size;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto LfuCache<KeyType, ValueType, SyncType>::doFind(
    const KeyType& key,
    bool peek) -> std::optional<ValueType>
{
    auto keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end()) {
        Element& element = *keyed_position->second;
        // Don't update the elements access in the LRU if peeking.
        if (!peek) {
            doAccess(element);
        }
        return { element.m_value };
    }

    return {};
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto LfuCache<KeyType, ValueType, SyncType>::doFindWithUseCount(
    const KeyType& key,
    bool peek) -> std::optional<std::pair<ValueType, size_t>>
{
    auto keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end()) {
        Element& element = *keyed_position->second;
        // Don't update the elements access in the LRU if peeking.
        if (!peek) {
            doAccess(element);
        }
        return { std::make_pair(element.m_value, element.m_lfu_position->first) };
    }

    return {};
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto LfuCache<KeyType, ValueType, SyncType>::doAccess(
    Element& element) -> void
{
    auto use_count = element.m_lfu_position->first;
    m_lfu_list.erase(element.m_lfu_position);
    element.m_lfu_position = m_lfu_list.emplace(use_count + 1, element.m_keyed_position->second);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto LfuCache<KeyType, ValueType, SyncType>::doPrune() -> void
{
    if (m_used_size > 0) {
        doDelete(m_lfu_list.begin()->second);
    }
}

} // namespace cappuccino
