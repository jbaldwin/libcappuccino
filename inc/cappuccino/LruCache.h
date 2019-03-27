#pragma once

#include <chrono>
#include <list>
#include <map>
#include <mutex>
#include <numeric>
#include <optional>
#include <unordered_map>
#include <vector>

namespace cappuccino {

template <typename KeyType, typename ValueType>
class LruCache {
public:
    struct KeyValue {
        KeyValue(
            std::chrono::seconds ttl,
            KeyType key,
            ValueType value)
            : m_ttl(ttl)
            , m_key(std::move(key))
            , m_value(std::move(value))
        {
        }

        std::chrono::seconds m_ttl;
        KeyType m_key;
        ValueType m_value;
    };

    /**
     * @param capacity The maximum number of elements allowed in the cache.
     */
    explicit LruCache(
        size_t capacity);

    /**
     * Inserts or updates the given key value pair with the new TTL.
     * @param ttl The TTL for this key value pair.
     * @param key The key to store the value under.
     * @param value The value of data to store.
     */
    auto Insert(
        std::chrono::seconds ttl,
        const KeyType& key,
        ValueType value) -> void;

    /**
     * Inserts or updates a range of key values pairs with their given TTL.  This expects a container
     * that has 3 values in the {std::chrono:;seconds, KeyType, ValueType} ordering.
     * There is a simple struct provided on the LruCache::KeyValue that can be put into
     * any iterable container to satisfy this requirement.
     * @tparam RangeType A container with three items, std::chrono::seconds, KeyType, ValueType
     * @param key_value_range The elements to insert or update into the cache.
     */
    template <typename RangeType>
    auto InsertRange(
        RangeType key_value_range) -> void;

    /**
     * Attempts to delete the given key.
     * @param key The key to remove from the lru cache.
     * @return True if the key was deleted, false if the key does not exist.
     */
    auto Delete(
        const KeyType& key) -> bool;

    /**
     * Attempts to delete all given keys.
     * @tparam RangeType A container with the set of keys to delete, e.g. vector<k> or set<k>.
     * @param key_range The keys to delete from the cache.
     * @return The number of items deleted from the cache.
     */
    template <typename RangeType>
    auto DeleteRange(
        RangeType key_range) -> size_t;

    /**
     * Attempts to find the given key's value.
     * @param key The key to lookup its data.
     * @return An optional with the key's value if it exists, or an empty optional if it does not.
     */
    auto Find(
        const KeyType& key) -> std::optional<ValueType>;

    /**
     * Attempts to find all the given keys values.
     *
     * The user should initialize this container with the keys to lookup with the values as all
     * empty optionals.  The keys that are found will have the optionals filled in with the
     * appropriate values from the cache.
     *
     * @tparam RangeType A container with a pair of optional items,
     *                   e.g. vector<pair<k, optional<v>>>, or map<k, optional<v>>.
     * @param key_optional_value_range The keys to optional values to fill out.
     */
    template <typename RangeType>
    auto FindRange(
        RangeType& key_optional_value_range) -> void;

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
        /// The point in time in which this element's value expires.
        std::chrono::steady_clock::time_point m_expire_time;
        /// The element's index into the main 'm_elements' store.  This is required for comparison.
        size_t m_element_idx;
        /// The element's value.
        ValueType m_value;

        /// The iterator into the keyed data structure.
        typename std::unordered_map<KeyType, size_t>::iterator m_keyed_position;
        /// The iterator into the lru data structure.
        std::list<size_t>::iterator m_lru_position;
        /// The iterator into the ttl data structure.
        std::map<std::chrono::steady_clock::time_point, size_t>::iterator m_ttl_position;

        auto operator>(const Element& other) const -> bool
        {
            return m_expire_time > other.m_expire_time
                && m_element_idx > other.m_element_idx;
        }
    };

    auto doInsertUpdate(
        const KeyType& key,
        ValueType value,
        std::chrono::steady_clock::time_point now,
        std::chrono::steady_clock::time_point expire_time) -> void;

    auto doInsert(
        const KeyType& key,
        ValueType&& value,
        std::chrono::steady_clock::time_point expire_time) -> void;

    auto doUpdate(
        typename std::unordered_map<KeyType, size_t>::iterator keyed_position,
        const KeyType& key,
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

    /// The current number of elements in the cache.
    size_t m_used_size { 0 };

    /// The main store for the key value pairs and metadata for each element.
    std::vector<Element> m_elements;

    /// The keyed lookup data structure, the value is the index into 'm_elements'.
    std::unordered_map<KeyType, size_t> m_keyed_elements;
    /// The lru sorted list, the value is the index into 'm_elements'.
    std::list<size_t> m_lru_list;
    /// The ttl sorted list, the value is the index into 'm_elements'.
    std::map<std::chrono::steady_clock::time_point, size_t> m_ttl_list;

    /**
     * The current end of the lru list.  This list is special in that it is pre-allocated
     * for the capacity of the entire size of 'm_elements' and this iterator is used
     * to denote how many items are in use.  Each item in this list is pre-assigned an index
     * into 'm_elements' and never has that index changed, this is how open slots into
     * 'm_elements' are determined when inserting a new Element.
     */
    std::list<size_t>::iterator m_lru_end;
};

} // namespace cappuccino

#include "cappuccino/LruCache.tcc"
