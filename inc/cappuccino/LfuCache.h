#pragma once

#include "cappuccino/SyncImplEnum.h"

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
 * To remove locks/synchronization used SyncImplEnum::UNSYNC when creating the cache.
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
        std::multimap<size_t, size_t>::iterator m_lfu_position;
        /// The iterator into the open list data structure.
        std::list<size_t>::iterator m_open_list_position;
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
        size_t element_idx) -> void;

    auto doFind(
        const KeyType& key,
        bool peek) -> std::optional<ValueType>;

    auto doAccess(
        Element& element) -> void;

    auto doPrune() -> void;

    /// Cache lock for all mutations if sync is enabled.
    std::mutex m_lock;

    /// The current number of elements in the cache.
    size_t m_used_size { 0 };

    /// The main store for the key value pairs and metadata for each element.
    std::vector<Element> m_elements;
    /// The keyed lookup data structure, the value is the index into 'm_elements'.
    std::unordered_map<KeyType, size_t> m_keyed_elements;
    /// The lfu sorted map, the key is the number of times the element has been used,
    /// the value is the index into 'm_elements'.
    std::multimap<size_t, size_t> m_lfu_list;
    /// The open list of un-used slots in 'm_elements'.
    std::list<size_t> m_open_list;
    /// The end of the open list to pull open slots from.
    std::list<size_t>::iterator m_open_list_end;
};

} // namespace cappuccino

#include "cappuccino/LfuCache.tcc"
