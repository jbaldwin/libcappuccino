#pragma once

#include "cappuccino/allow.hpp"
#include "cappuccino/lock.hpp"
#include "cappuccino/peek.hpp"

#include <chrono>
#include <list>
#include <map>
#include <mutex>
#include <numeric>
#include <optional>
#include <unordered_map>
#include <vector>

namespace cappuccino
{
/**
 * Time aware LRU (Least Recently Used) Cache.
 * Each key value pair is evicted based on an individual TTL (time to live)
 * and least recently used policy.  Expired key value pairs are evicted before
 * least recently used.
 *
 * This cache is sync aware by default and can be used concurrently from multiple threads safely.
 * To remove locks/synchronization use NO when creating the cache.
 *
 * @tparam KeyType The key type.  Must support std::hash() and operator<().
 * @tparam ValueType The value type.  This is returned by copy on a find, so if your data
 *                   structure value is large it is advisable to store in a shared ptr.
 * @tparam sync_type By default this cache is thread safe, can be disabled for caches specific
 *                  to a single thread.
 */
template<typename KeyType, typename ValueType, sync sync_type = sync::yes>
class TlruCache
{
    using KeyedIterator = typename std::unordered_map<KeyType, size_t>::iterator;
    using LruIterator   = std::list<size_t>::iterator;
    using TtlIterator   = std::multimap<std::chrono::steady_clock::time_point, size_t>::iterator;

public:
    struct KeyValue
    {
        KeyValue(std::chrono::milliseconds ttl, KeyType key, ValueType value)
            : m_ttl(ttl),
              m_key(std::move(key)),
              m_value(std::move(value))
        {
        }

        std::chrono::milliseconds m_ttl;
        KeyType                   m_key;
        ValueType                 m_value;
    };

    /**
     * @param capacity The maximum number of key value pairs allowed in the cache.
     * @param max_load_factor The load factor for the hash map, generally 1 is a good default.
     */
    explicit TlruCache(size_t capacity, float max_load_factor = 1.0f);

    /**
     * Inserts or updates the given key value pair with the new TTL.
     * @param ttl The TTL for this key value pair.
     * @param key The key to store the value under.
     * @param value The value of data to store.
     * @param a Allowed methods of insertion | update.  Defaults to allowing
     *              insertions and updates.
     * @return True if the operation was successful based on `allow`.
     */
    auto Insert(std::chrono::milliseconds ttl, const KeyType& key, ValueType value, allow a = allow::insert_or_update)
        -> bool;

    /**
     * Inserts or updates a range of key values pairs with their given TTL.  This expects a container
     * that has 3 values in the {std::chrono::milliseconds, KeyType, ValueType} ordering.
     * There is a simple struct provided on the TlruCache::KeyValue that can be put into
     * any iterable container to satisfy this requirement.
     * @tparam RangeType A container with three items, std::chrono::milliseconds, KeyType, ValueType.
     * @param key_value_range The elements to insert or update into the cache.
     * @param a Allowed methods of insertion | update.  Defaults to allowing
     *              insertions and updates.
     * @return The number of elements inserted based on `allow`.
     */
    template<typename RangeType>
    auto InsertRange(RangeType&& key_value_range, allow a = allow::insert_or_update) -> size_t;

    /**
     * Attempts to delete the given key.
     * @param key The key to remove from the lru cache.
     * @return True if the key was deleted, false if the key does not exist.
     */
    auto Delete(const KeyType& key) -> bool;

    /**
     * Attempts to delete all given keys.
     * @tparam RangeType A container with the set of keys to delete, e.g. vector<k> or set<k>.
     * @param key_range The keys to delete from the cache.
     * @return The number of items deleted from the cache.
     */
    template<template<class...> typename RangeType>
    auto DeleteRange(const RangeType<KeyType>& key_range) -> size_t;

    /**
     * Attempts to find the given key's value.
     * @param key The key to lookup its value.
     * @param peek Should the find act like all the item was not used?
     * @return An optional with the key's value if it exists, or an empty optional if it does not.
     */
    auto Find(const KeyType& key, peek peek = peek::no) -> std::optional<ValueType>;

    /**
     * Attempts to find all the given keys values.
     * @tparam RangeType A container with the set of keys to find their values, e.g. vector<KeyType>.
     * @param key_range The keys to lookup their pairs.
     * @param peek Should the find act like all the items were not used?
     * @return The full set of keys to std::nullopt if the key wasn't found, or the value if found.
     */
    template<template<class...> typename RangeType>
    auto FindRange(const RangeType<KeyType>& key_range, peek peek = peek::no)
        -> std::vector<std::pair<KeyType, std::optional<ValueType>>>;

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
     * @param peek Should the find act like all the items were not used?
     */
    template<typename RangeType>
    auto FindRangeFill(RangeType& key_optional_value_range, peek peek = peek::no) -> void;

    /**
     * Trims the TTL list of items an expunges all expired elements.  This could be useful to use
     * on downtime to make inserts faster if the cache is full by pruning TTL'ed elements.
     * @return The number of elements pruned.
     */
    auto CleanExpiredValues() -> size_t;

    /**
     * @return If this cache is currenty empty.
     */
    auto empty() const -> bool { return (m_used_size == 0); }

    /**
     * @return The number of elements inside the cache.
     */
    auto size() const -> size_t { return m_used_size; }

    /**
     * @return The maximum capacity of this cache.
     */
    auto capacity() const -> size_t { return m_elements.size(); }

private:
    struct Element
    {
        /// The point in time in which this element's value expires.
        std::chrono::steady_clock::time_point m_expire_time;
        /// The iterator into the keyed data structure.
        KeyedIterator m_keyed_position;
        /// The iterator into the lru data structure.
        LruIterator m_lru_position;
        /// The iterator into the ttl data structure.
        TtlIterator m_ttl_position;
        /// The element's value.
        ValueType m_value;
    };

    auto doInsertUpdate(
        const KeyType&                        key,
        ValueType&&                           value,
        std::chrono::steady_clock::time_point now,
        std::chrono::steady_clock::time_point expire_time,
        allow                                 a) -> bool;

    auto doInsert(
        const KeyType&                        key,
        ValueType&&                           value,
        std::chrono::steady_clock::time_point now,
        std::chrono::steady_clock::time_point expire_time) -> void;

    auto doUpdate(
        typename std::unordered_map<KeyType, size_t>::iterator keyed_position,
        ValueType&&                                            value,
        std::chrono::steady_clock::time_point                  expire_time) -> void;

    auto doDelete(size_t element_idx) -> void;

    auto doFind(const KeyType& key, std::chrono::steady_clock::time_point now, peek peek) -> std::optional<ValueType>;

    auto doAccess(Element& element) -> void;

    auto doPrune(std::chrono::steady_clock::time_point now) -> void;

    /// Cache lock for all mutations if sync is enabled.
    mutex<sync_type> m_lock;

    /// The current number of elements in the cache.
    size_t m_used_size{0};

    /// The main store for the key value pairs and metadata for each element.
    std::vector<Element> m_elements;

    /// The keyed lookup data structure, the value is the index into 'm_elements'.
    std::unordered_map<KeyType, size_t> m_keyed_elements;
    /// The lru sorted list, the value is the index into 'm_elements'.
    std::list<size_t> m_lru_list;
    /**
     * The ttl sorted list, the value is the index into 'm_elements'.  Note that it is
     * important to use a multimap as two threads could timestamp the same!
     */
    std::multimap<std::chrono::steady_clock::time_point, size_t> m_ttl_list;

    /**
     * The current end of the lru list.  This list is special in that it is pre-allocated
     * for the capacity of the entire size of 'm_elements' and this iterator is used
     * to denote how many items are in use.  Each item in this list is pre-assigned an index
     * into 'm_elements' and never has that index changed, this is how open slots into
     * 'm_elements' are determined when inserting a new Element.
     */
    LruIterator m_lru_end;
};

template<typename KeyType, typename ValueType, sync sync_type>
TlruCache<KeyType, ValueType, sync_type>::TlruCache(size_t capacity, float max_load_factor)
    : m_elements(capacity),
      m_lru_list(capacity)
{
    std::iota(m_lru_list.begin(), m_lru_list.end(), 0);
    m_lru_end = m_lru_list.begin();

    m_keyed_elements.max_load_factor(max_load_factor);
    m_keyed_elements.reserve(capacity);
}

template<typename KeyType, typename ValueType, sync sync_type>
auto TlruCache<KeyType, ValueType, sync_type>::Insert(
    std::chrono::milliseconds ttl, const KeyType& key, ValueType value, allow a) -> bool
{
    auto now         = std::chrono::steady_clock::now();
    auto expire_time = now + ttl;

    std::lock_guard guard{m_lock};
    return doInsertUpdate(key, std::move(value), now, expire_time, a);
}

template<typename KeyType, typename ValueType, sync sync_type>
template<typename RangeType>
auto TlruCache<KeyType, ValueType, sync_type>::InsertRange(RangeType&& key_value_range, allow a) -> size_t
{
    auto   now = std::chrono::steady_clock::now();
    size_t inserted{0};

    {
        std::lock_guard guard{m_lock};
        for (auto& [ttl, key, value] : key_value_range)
        {
            auto expired_time = now + ttl;
            if (doInsertUpdate(key, std::move(value), now, expired_time, a))
            {
                ++inserted;
            }
        }
    }

    return inserted;
}

template<typename KeyType, typename ValueType, sync sync_type>
auto TlruCache<KeyType, ValueType, sync_type>::Delete(const KeyType& key) -> bool
{
    std::lock_guard guard{m_lock};
    auto            keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end())
    {
        doDelete(keyed_position->second);
        return true;
    }
    else
    {
        return false;
    }
}

template<typename KeyType, typename ValueType, sync sync_type>
template<template<class...> typename RangeType>
auto TlruCache<KeyType, ValueType, sync_type>::DeleteRange(const RangeType<KeyType>& key_range) -> size_t
{
    size_t deleted_elements{0};

    std::lock_guard guard{m_lock};
    for (auto& key : key_range)
    {
        auto keyed_position = m_keyed_elements.find(key);
        if (keyed_position != m_keyed_elements.end())
        {
            ++deleted_elements;
            doDelete(keyed_position->second);
        }
    }

    return deleted_elements;
}

template<typename KeyType, typename ValueType, sync sync_type>
auto TlruCache<KeyType, ValueType, sync_type>::Find(const KeyType& key, peek peek) -> std::optional<ValueType>
{
    auto now = std::chrono::steady_clock::now();

    std::lock_guard guard{m_lock};
    return doFind(key, now, peek);
}

template<typename KeyType, typename ValueType, sync sync_type>
template<template<class...> typename RangeType>
auto TlruCache<KeyType, ValueType, sync_type>::FindRange(const RangeType<KeyType>& key_range, peek peek)
    -> std::vector<std::pair<KeyType, std::optional<ValueType>>>
{
    std::vector<std::pair<KeyType, std::optional<ValueType>>> output;
    output.reserve(std::size(key_range));

    auto now = std::chrono::steady_clock::now();

    {
        std::lock_guard guard{m_lock};
        for (auto& key : key_range)
        {
            output.emplace_back(key, doFind(key, now, peek));
        }
    }

    return output;
}

template<typename KeyType, typename ValueType, sync sync_type>
template<typename RangeType>
auto TlruCache<KeyType, ValueType, sync_type>::FindRangeFill(RangeType& key_optional_value_range, peek peek) -> void
{
    auto now = std::chrono::steady_clock::now();

    std::lock_guard guard{m_lock};
    for (auto& [key, optional_value] : key_optional_value_range)
    {
        optional_value = doFind(key, now, peek);
    }
}

template<typename KeyType, typename ValueType, sync sync_type>
auto TlruCache<KeyType, ValueType, sync_type>::CleanExpiredValues() -> size_t
{
    size_t start_size = m_ttl_list.size();
    auto   now        = std::chrono::steady_clock::now();

    std::lock_guard guard{m_lock};
    // Loop through and delete all items that are expired.
    while (m_used_size > 0 && now >= m_ttl_list.begin()->first)
    {
        doDelete(m_ttl_list.begin()->second);
    }

    // return the number of items removed.
    return start_size - m_ttl_list.size();
}

template<typename KeyType, typename ValueType, sync sync_type>
auto TlruCache<KeyType, ValueType, sync_type>::doInsertUpdate(
    const KeyType&                        key,
    ValueType&&                           value,
    std::chrono::steady_clock::time_point now,
    std::chrono::steady_clock::time_point expire_time,
    allow                                 a) -> bool
{
    auto keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end())
    {
        if (update_allowed(a))
        {
            doUpdate(keyed_position, std::move(value), expire_time);
            return true;
        }
        else if (insert_allowed(a))
        {
            // If the item has expired and this is INSERT then allow the
            // insert to proceed, this can just be an update in place.
            const auto& [key, element_idx] = *keyed_position;
            Element& element               = m_elements[element_idx];
            if (now >= element.m_expire_time)
            {
                doUpdate(keyed_position, std::move(value), expire_time);
                return true;
            }
        }
    }
    else
    {
        if (insert_allowed(a))
        {
            doInsert(key, std::move(value), now, expire_time);
            return true;
        }
    }
    return false;
}

template<typename KeyType, typename ValueType, sync sync_type>
auto TlruCache<KeyType, ValueType, sync_type>::doInsert(
    const KeyType&                        key,
    ValueType&&                           value,
    std::chrono::steady_clock::time_point now,
    std::chrono::steady_clock::time_point expire_time) -> void
{
    // Inserts might require an item to be pruned, check that first before inserting the new key/value.
    if (m_used_size >= m_elements.size())
    {
        doPrune(now);
    }

    auto element_idx = *m_lru_end;

    auto keyed_position = m_keyed_elements.emplace(key, element_idx).first;

    // Insert the element_idx into the TTL list.
    auto ttl_position = m_ttl_list.emplace(expire_time, element_idx);

    // Update the element's appropriate fields across the datastructures.
    Element& element         = m_elements[element_idx];
    element.m_value          = std::move(value);
    element.m_expire_time    = expire_time;
    element.m_lru_position   = m_lru_end;
    element.m_ttl_position   = ttl_position;
    element.m_keyed_position = keyed_position;

    // Update the LRU position.
    ++m_lru_end;

    ++m_used_size;

    doAccess(element);
}

template<typename KeyType, typename ValueType, sync sync_type>
auto TlruCache<KeyType, ValueType, sync_type>::doUpdate(
    typename std::unordered_map<KeyType, size_t>::iterator keyed_position,
    ValueType&&                                            value,
    std::chrono::steady_clock::time_point                  expire_time) -> void
{
    size_t element_idx = keyed_position->second;

    Element& element      = m_elements[element_idx];
    element.m_expire_time = expire_time;
    element.m_value       = std::move(value);

    // Reinsert into TTL list with the new TTL.
    m_ttl_list.erase(element.m_ttl_position);
    element.m_ttl_position = m_ttl_list.emplace(expire_time, element_idx);

    doAccess(element);
}

template<typename KeyType, typename ValueType, sync sync_type>
auto TlruCache<KeyType, ValueType, sync_type>::doDelete(size_t element_idx) -> void
{
    Element& element = m_elements[element_idx];

    // splice into the end position of the LRU if it isn't the last item...
    if (element.m_lru_position != std::prev(m_lru_end))
    {
        m_lru_list.splice(m_lru_end, m_lru_list, element.m_lru_position);
    }
    --m_lru_end;

    m_ttl_list.erase(element.m_ttl_position);

    m_keyed_elements.erase(element.m_keyed_position);

    // destruct element.m_value when re-assigned on an insert.

    --m_used_size;
}

template<typename KeyType, typename ValueType, sync sync_type>
auto TlruCache<KeyType, ValueType, sync_type>::doFind(
    const KeyType& key, std::chrono::steady_clock::time_point now, peek peek) -> std::optional<ValueType>
{
    auto keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end())
    {
        size_t   element_idx = keyed_position->second;
        Element& element     = m_elements[element_idx];

        // Has the element TTL'ed?
        if (now < element.m_expire_time)
        {
            // Do not update the items access if peeking.
            if (peek == peek::no)
            {
                doAccess(element);
            }
            return {element.m_value};
        }
        else
        {
            // Its dead anyways, lets delete it now.
            doDelete(element_idx);
        }
    }

    return {};
}

template<typename KeyType, typename ValueType, sync sync_type>
auto TlruCache<KeyType, ValueType, sync_type>::doAccess(Element& element) -> void
{
    // This function will put the item at the most recently used side of the LRU list.
    m_lru_list.splice(m_lru_list.begin(), m_lru_list, element.m_lru_position);
}

template<typename KeyType, typename ValueType, sync sync_type>
auto TlruCache<KeyType, ValueType, sync_type>::doPrune(std::chrono::steady_clock::time_point now) -> void
{
    if (m_used_size > 0)
    {
        auto& [expire_time, element_idx] = *m_ttl_list.begin();

        if (now >= expire_time)
        {
            // If there is an expired item, prefer to remove that.
            doDelete(element_idx);
        }
        else
        {
            // Otherwise pick the least recently used item to prune.
            size_t lru_idx = m_lru_list.back();
            doDelete(lru_idx);
        }
    }
}

} // namespace cappuccino
