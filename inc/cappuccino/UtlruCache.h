#pragma once

#include "cappuccino/CappuccinoLock.h"
#include "cappuccino/SyncImplEnum.h"

#include <chrono>
#include <list>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace cappuccino {

/**
 * Uniform time aware LRU (Least Recently Used) Cache.
 * Each key value pair is evicted based on a global cache TTL (time to live)
 * and least recently used policy.  Expired key value pairs are evicted before
 * least recently used.  This cache differs from TlruCache in that it is more
 * efficient in managing the TTLs since they are uniform for all key value pairs
 * in the cache, rather than tracking items individually.
 *
 * This cache is sync aware and can be used concurrently from multiple threads safely.
 * To remove locks/synchronization use SyncImplEnum::UNSYNC when creating the cache.
 *
 * @tparam KeyType The key type.  Must support std::hash().
 * @tparam ValueType The value type.  This is returned by copy on a find, so if your data
 *                   structure value is large it is advisable to store in a shared ptr.
 * @tparam SyncType By default this cache is thread safe, can be disabled for caches specific
 *                  to a single thread.
 */
template <typename KeyType, typename ValueType, SyncImplEnum SyncType = SyncImplEnum::SYNC>
class UtlruCache {
public:
    using KeyedIterator = typename std::unordered_map<KeyType, size_t>::iterator;

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
     * @param ttl The uniform TTL of every key value inserted into the cache.
     * @param capacity The maximum number of key value pairs allowed in the cache.
     * @param max_load_factor The load factor for the hash map, generally 1 is a good default.
     */
    UtlruCache(
        std::chrono::seconds ttl,
        size_t capacity,
        float max_load_factor = 1.0f);

    /**
     * Inserts the given key value pair, but only if key doesn't already exist or is expired.
     * @param key The key to store the value under.
     * @param value The value of data to store.
     * @return  True if new key inserted (not found or expired), False if already exists and not inserted
     */
    auto InsertOnly(
        const KeyType& key,
        ValueType value) -> bool;

    /**
     * Inserts or updates the given key value pair.
     * @param key The key to store the value under.
     * @param value The value of data to store.
     */
    auto Insert(
        const KeyType& key,
        ValueType value) -> void;

    /**
     * Inserts or updates a range of key value pairs.  This expects a container that has
     * 2 values in the {KeyType, ValueType} ordering.  There is a simple struct
     * LruCacheUniformTtl::KeyValue that can be put into any iterable container to satisfy
     * this requirement.
     * @tparam RangeType A container with two items, KeyType, ValueType.
     * @param key_value_range The elements to insert or update into the cache.
     */
    template <typename RangeType>
    auto InsertRange(
        RangeType&& key_value_range) -> void;

    /**
     * Attempts to delete the given key.
     * @param key The key to remove from the lru cache.
     * @return True if the key was deleted, false if the key does not exist.
     */
    auto Delete(
        const KeyType& key) -> bool;

    /**
     * Attempts to delete all given keys.
     * @tparam RangeType A container with the set of keys  to delete, e.g. vector<k> or set<k>.
     * @param key_range The keys to delete from the cache.
     * @return The number of items deleted from the cache.
     */
    template <template <class...> typename RangeType>
    auto DeleteRange(
        const RangeType<KeyType>& key_range) -> size_t;

    /**
     * Attempts to find the given key's value.
     * @param key The key to lookup its value.
     * @param peek Should the find act like all the item was not used?
     * @return An optional with the key's value if it exists, or an empty optional if it does not.
     */
    auto Find(
        const KeyType& key,
        bool peek = false) -> std::optional<ValueType>;

    /**
     * Attempts to find all the given keys values.
     * @tparam RangeType A container with the set of keys to lookup, e.g. vector<KeyType>.
     * @param key_range A container with the set of keys to lookup.
     * @param peek Should the find act like all the items were not used?
     * @return All input keys to either a std::nullopt if it doesn't exist, or the value if it does.
     */
    template <typename RangeType>
    auto FindRange(
        const RangeType& key_range,
        bool peek = false) -> std::vector<std::pair<KeyType, std::optional<ValueType>>>;

    /**
     * Attempts to find all given keys values.
     *
     * The user should initialize this container with the keys to lookup with the values all
     * empty optionals.  The keys that are found will have the optionals filled in with the
     * appropriate values from the cache.
     *
     * @tparam RangeType A container with a pair of optional items,
     *                   e.g. vector<pair<k, optional<v>>> or map<k, optional<v>>.
     * @param key_optional_value_range The keys to optional values to fill out.
     * @param peek Should the find act like all the items were not used?
     */
    template <typename RangeType>
    auto FindRangeFill(
        RangeType& key_optional_value_range,
        bool peek = false) -> void;

    /**
     * Updates the uniform TTL for all new items or updated items in the cache.
     * Existing items that are not touched will TTL out at the previous uniform
     * TTL time.
     * @param ttl The new uniform TTL value to apply to all new elements.
     */
    auto UpdateTtl(
        std::chrono::seconds ttl) -> void;

    /**
     * @return The number of elements inside the cache.
     */
    auto GetUsedSize() const -> size_t;

    /**
     * @return The maximum capacity of this cache.
     */
    auto GetCapacity() const -> size_t;

    /**
     * Trims the TTL list of items an expunges all expired elements.  This could be useful to use
     * on downtime to make inserts faster if the cache is full by pruning TTL'ed elements.
     * @return The number of elements pruned.
     */
    auto CleanExpiredValues() -> size_t;

private:
    struct Element {
        /// The point in time in  which this element's value expires.
        std::chrono::steady_clock::time_point m_expire_time;
        /// The iterator into the keyed data structure.
        KeyedIterator m_keyed_position;
        /// The iterator into the lru data structure.
        std::list<size_t>::iterator m_lru_position;
        /// The iterator into the ttl data structure.
        std::list<size_t>::iterator m_ttl_position;
        /// The element's value.
        ValueType m_value;
    };

    auto doInsertUpdate(
        const KeyType& key,
        ValueType&& value,
        std::chrono::steady_clock::time_point now,
        std::chrono::steady_clock::time_point expire_time,
        bool insert_only=false) -> bool;

    auto doInsert(
        const KeyType& key,
        ValueType&& value,
        std::chrono::steady_clock::time_point expire_time) -> void;

    auto doUpdate(
        KeyedIterator keyed_position,
        std::chrono::steady_clock::time_point expire_time) -> Element&;

    auto doDelete(
        size_t element_idx) -> void;

    auto doFind(
        const KeyType& key,
        std::chrono::steady_clock::time_point now,
        bool peek) -> std::optional<ValueType>;

    auto doAccess(
        Element& element) -> void;

    auto doPrune(
        std::chrono::steady_clock::time_point now) -> void;

    /// Cache lock for all mutations.
    CappuccinoLock<SyncType> m_lock;

    /// The uniform TTL for every key value pair inserted into the cache.
    std::chrono::seconds m_ttl;

    /// The current number of elements in the cache.
    size_t m_used_size { 0 };

    /// The main store for the key value pairs and metadata for each element.
    std::vector<Element> m_elements;
    /// The keyed lookup data structure, the value is the index into 'm_elements'.
    std::unordered_map<KeyType, size_t> m_keyed_elements;
    /// The lru sorted list from most recently used (head) to least recently used (tail).
    std::list<size_t> m_lru_list;
    /// The uniform ttl sorted list.
    std::list<size_t> m_ttl_list;
    /// The lru end/open list end.
    std::list<size_t>::iterator m_lru_end;
};

} // namespace cappuccino

#include "cappuccino/UtlruCache.tcc"
