#pragma once

#include "cappuccino/Lock.hpp"
#include "cappuccino/Allow.hpp"

#include <random>
#include <unordered_map>
#include <vector>

namespace cappuccino {

/**
 * Random Replacement (RR) Cache.
 * Each key value pair is evicted based upon a random eviction strategy.
 *
 * This cache is sync aware and can be used concurrently from multiple threads safely.
 * To remove locks/synchronization used NO when creating the cache.
 *
 * @tparam KeyType The key type.  Must support std::hash().
 * @tparam ValueType The value type.  This is returned by copy on a find, so if your data
 *                   structure value is large it is advisable to store in a shared ptr.
 * @tparam SyncType By default this cache is thread safe, can be disabled for caches
 *                  specific to a single thread.
 */
template <typename KeyType, typename ValueType, Sync SyncType = Sync::YES>
class RrCache {
private:
    using KeyedIterator = typename std::unordered_map<KeyType, size_t>::iterator;

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
    explicit RrCache(
        size_t capacity,
        float max_load_factor = 1.0f);

    /**
     * Inserts or updates the given key value pair.
     * @param key The key to store the value under.
     * @param value The value of the data to store.
     * @param allow Allowed methods of insertion | update.  Defaults to allowing
     *              insertions and updates.
     * @return True if the operation was successful based on `allow`.
     */
    auto Insert(
        const KeyType& key,
        ValueType value,
        Allow allow = Allow::INSERT_OR_UPDATE) -> bool;

    /**
     * Inserts or updates a range of key value pairs.  This expects a container
     * that has 2 values in the {KeyType, ValueType} ordering.
     * There is a simple struct provided on the LruCache::KeyValue that can be put
     * into any iterable container to satisfy this requirement.
     * @tparam RangeType A container with two items, KeyType, ValueType.
     * @param key_value_range The elements to insert or update into the cache.
     * @param allow Allowed methods of insertion | update.  Defaults to allowing
     *              insertions and updates.
     * @return The number of elements inserted based on `allow`.
     */
    template <typename RangeType>
    auto InsertRange(
        RangeType&& key_value_range,
        Allow allow = Allow::INSERT_OR_UPDATE) -> size_t;

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
     * @return An optional with the key's value if it exists, or an empty optional if it does not.
     */
    auto Find(
        const KeyType& key) -> std::optional<ValueType>;

    /**
     * Attempts to find all the given keys values.
     * @tparam RangeType A container with the set of keys to find their values, e.g. vector<KeyType>.
     * @param key_range The keys to lookup their pairs.
     * @return The full set of keys to std::nullopt if the key wasn't found, or the value if found.
     */
    template <template <class...> typename RangeType>
    auto FindRange(
        const RangeType<KeyType>& key_range) -> std::vector<std::pair<KeyType, std::optional<ValueType>>>;

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
     */
    template <typename RangeType>
    auto FindRangeFill(
        RangeType& key_optional_value_range) -> void;

    /**
     * @return If this cache is currenty empty.
     */
    auto empty() const -> bool { return (m_open_list_end == 0); }

    /**
     * @return The number of elements inside the cache.
     */
    auto size() const -> size_t { return m_open_list_end; }

    /**
     * @return The maximum capacity of this cache.
     */
    auto capacity() const -> size_t { return m_elements.size(); }

private:
    struct Element {
        KeyedIterator m_keyed_position;
        size_t m_open_list_position;
        ValueType m_value;
    };

    auto doInsertUpdate(
        const KeyType& key,
        ValueType&& value,
        Allow allow) -> bool;

    auto doInsert(
        const KeyType& key,
        ValueType&& value) -> void;

    auto doUpdate(
        KeyedIterator keyed_position,
        ValueType&& value) -> void;

    auto doDelete(
        size_t element_idx) -> void;

    auto doFind(
        const KeyType& key) -> std::optional<ValueType>;

    auto doPrune() -> void;

    /// Cache lock for all mutations if sync is enabled.
    Lock<SyncType> m_lock;

    /// The main store for the key value pairs and metadata for each element.
    std::vector<Element> m_elements;
    /// The keyed lookup data structure, this value is the index into 'm_elements'.
    std::unordered_map<KeyType, size_t> m_keyed_elements;
    /// The open list of free elements to use, the value is the index into 'm_elements'.
    std::vector<size_t> m_open_list;
    /// This is the partition point in the m_open_list, it is also the number of items in the cache.
    size_t m_open_list_end { 0 };

    /// Random device to seed mt19937.
    std::random_device m_random_device;
    /// Random number generator for eviction policy.
    std::mt19937 m_mt;
};

template <typename KeyType, typename ValueType, Sync SyncType>
RrCache<KeyType, ValueType, SyncType>::RrCache(
    size_t capacity,
    float max_load_factor)
    : m_elements(capacity)
    , m_open_list(capacity)
    , m_random_device()
    , m_mt(m_random_device())
{
    std::iota(m_open_list.begin(), m_open_list.end(), 0);

    m_keyed_elements.max_load_factor(max_load_factor);
    m_keyed_elements.reserve(capacity);
}

template <typename KeyType, typename ValueType, Sync SyncType>
auto RrCache<KeyType, ValueType, SyncType>::Insert(
    const KeyType& key,
    ValueType value,
    Allow allow) -> bool
{
    std::lock_guard guard { m_lock };
    return doInsertUpdate(key, std::move(value), allow);
}

template <typename KeyType, typename ValueType, Sync SyncType>
template <typename RangeType>
auto RrCache<KeyType, ValueType, SyncType>::InsertRange(
    RangeType&& key_value_range,
    Allow allow) -> size_t
{
    size_t inserted { 0 };

    {
        std::lock_guard guard { m_lock };
        for (auto& [key, value] : key_value_range) {
            if(doInsertUpdate(key, std::move(value), allow)) {
                ++inserted;
            }
        }
    }

    return inserted;
}

template <typename KeyType, typename ValueType, Sync SyncType>
auto RrCache<KeyType, ValueType, SyncType>::Delete(
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

template <typename KeyType, typename ValueType, Sync SyncType>
template <template <class...> typename RangeType>
auto RrCache<KeyType, ValueType, SyncType>::DeleteRange(
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

template <typename KeyType, typename ValueType, Sync SyncType>
auto RrCache<KeyType, ValueType, SyncType>::Find(
    const KeyType& key) -> std::optional<ValueType>
{
    std::lock_guard guard { m_lock };
    return doFind(key);
}

template <typename KeyType, typename ValueType, Sync SyncType>
template <template <class...> typename RangeType>
auto RrCache<KeyType, ValueType, SyncType>::FindRange(
    const RangeType<KeyType>& key_range) -> std::vector<std::pair<KeyType, std::optional<ValueType>>>
{
    std::vector<std::pair<KeyType, std::optional<ValueType>>> output;
    output.reserve(std::size(key_range));

    {
        std::lock_guard guard { m_lock };
        for (auto& key : key_range) {
            output.emplace_back(key, doFind(key));
        }
    }

    return output;
}

template <typename KeyType, typename ValueType, Sync SyncType>
template <typename RangeType>
auto RrCache<KeyType, ValueType, SyncType>::FindRangeFill(
    RangeType& key_optional_value_range) -> void
{
    std::lock_guard guard { m_lock };
    for (auto& [key, optional_value] : key_optional_value_range) {
        optional_value = doFind(key);
    }
}

template <typename KeyType, typename ValueType, Sync SyncType>
auto RrCache<KeyType, ValueType, SyncType>::doInsertUpdate(
    const KeyType& key,
    ValueType&& value,
    Allow allow) -> bool
{
    auto keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end()) {
        if(update_allowed(allow)) {
            doUpdate(keyed_position, std::move(value));
            return true;
        }
    } else {
        if(insert_allowed(allow)) {
            doInsert(key, std::move(value));
            return true;
        }
    }
    return false;
}

template <typename KeyType, typename ValueType, Sync SyncType>
auto RrCache<KeyType, ValueType, SyncType>::doInsert(
    const KeyType& key,
    ValueType&& value) -> void
{
    if (m_open_list_end >= m_elements.size()) {
        doPrune();
    }

    auto element_idx = m_open_list[m_open_list_end];

    auto keyed_position = m_keyed_elements.emplace(key, element_idx).first;

    Element& element = m_elements[element_idx];
    element.m_value = std::move(value);
    element.m_open_list_position = m_open_list_end;
    element.m_keyed_position = keyed_position;

    ++m_open_list_end;
}

template <typename KeyType, typename ValueType, Sync SyncType>
auto RrCache<KeyType, ValueType, SyncType>::doUpdate(
    RrCache::KeyedIterator keyed_position,
    ValueType&& value) -> void
{
    Element& element = m_elements[keyed_position->second];
    element.m_value = std::move(value);
}

template <typename KeyType, typename ValueType, Sync SyncType>
auto RrCache<KeyType, ValueType, SyncType>::doDelete(
    size_t element_idx) -> void
{
    Element& element = m_elements[element_idx];

    // If this isn't the last item already (which is likely), swap it into that position.
    if (element.m_open_list_position != m_open_list_end - 1) {
        // Since this algo does random replacement eviction, the order of
        // the open list doesn't really matter.  This is different from an LRU
        // where the ordering *does* matter.  Since ordering doesn't matter
        // just swap the indexes in the open list to the partition point 'end'
        // and then delete that item.
        std::swap(m_open_list[element.m_open_list_position], m_open_list[m_open_list_end - 1]);
    }
    --m_open_list_end; // delete the last item

    m_keyed_elements.erase(element.m_keyed_position);
}

template <typename KeyType, typename ValueType, Sync SyncType>
auto RrCache<KeyType, ValueType, SyncType>::doFind(
    const KeyType& key) -> std::optional<ValueType>
{
    auto keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end()) {
        size_t element_idx = keyed_position->second;
        Element& element = m_elements[element_idx];
        return { element.m_value };
    }

    return {};
}

template <typename KeyType, typename ValueType, Sync SyncType>
auto RrCache<KeyType, ValueType, SyncType>::doPrune() -> void
{
    if (m_open_list_end > 0) {
        std::uniform_int_distribution<size_t> dist { 0, m_open_list_end - 1 };
        size_t delete_idx = dist(m_mt);
        doDelete(delete_idx);
    }
}

} // namespace cappuccino
