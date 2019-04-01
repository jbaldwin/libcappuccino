#pragma once

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
 */
template <typename KeyType, typename ValueType, SyncImplEnum SyncType = SyncImplEnum::SYNC>
class UtlruCache {
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
     * @param ttl The uniform TTL of every key value inserted into the cache.
     * @param capacity The maximum number of key value pairs allowed in the cache.
     * @param max_load_factor The load factor for the hash map, generally 1 is a good default.
     */
    UtlruCache(
        std::chrono::seconds ttl,
        size_t capacity,
        float max_load_factor = 1.0f);

    /**
     * Inserts or updates the given key value pair.  On update will reset the TTL.
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
     * @{
     */
    template <template <class...> typename RangeType>
    auto InsertRange(
        RangeType<KeyValue> key_value_range) -> void;
    template <template <class...> typename RangeType, template <class, class> typename PairType>
    auto InsertRange(
        RangeType<PairType<KeyType, ValueType>> key_value_range) -> void;
    /** @} */

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
     * @return An optional with the key's value if it exists, or an empty optional if it does not.
     */
    auto Find(
        const KeyType& key) -> std::optional<ValueType>;

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
     * @{
     */
    template <template <class...> typename RangeType>
    auto FindRange(
        RangeType<KeyType, std::optional<ValueType>>& key_optional_value_range) -> void;
    template <template <class...> typename RangeType, template <class, class> typename PairType>
    auto FindRange(
        RangeType<PairType<KeyType, ValueType>>& key_optional_value_range) -> void;
    /** @} */

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
        typename std::unordered_map<KeyType, size_t>::iterator m_keyed_position;
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
        std::chrono::steady_clock::time_point expire_time) -> void;

    auto doInsert(
        const KeyType& key,
        ValueType&& value,
        std::chrono::steady_clock::time_point expire_time) -> void;

    auto doUpdate(
        typename std::unordered_map<KeyType, size_t>::iterator keyed_position,
        ValueType&& value,
        std::chrono::steady_clock::time_point expire_time) -> void;

    auto doDelete(
        size_t element_idx) -> void;

    auto doFind(
        const KeyType& key,
        std::chrono::steady_clock::time_point now) -> std::optional<ValueType>;

    auto doAccess(
        size_t element_idx) -> void;

    auto doPrune(
        std::chrono::steady_clock::time_point now) -> void;

    /// Cache lock for all mutations.
    std::mutex m_lock;

    /// The uniform TTL for every key value pair inserted into the cache.
    std::chrono::seconds m_ttl;

    size_t m_used_size { 0 };

    std::vector<Element> m_elements;

    std::unordered_map<KeyType, size_t> m_keyed_elements;
    std::list<size_t> m_lru_list;
    std::list<size_t> m_ttl_list;

    std::list<size_t>::iterator m_lru_end;
};

} // namespace cappuccino

#include "cappuccino/UtlruCache.tcc"
