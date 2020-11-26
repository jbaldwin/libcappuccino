#pragma once

#include "cappuccino/allow.hpp"
#include "cappuccino/lock.hpp"
#include "cappuccino/peek.hpp"

#include <chrono>
#include <list>
#include <numeric>
#include <optional>
#include <unordered_map>
#include <vector>

namespace cappuccino
{
/**
 * Uniform time aware LRU (Least Recently Used) Cache.
 * Each key value pair is evicted based on a global cache TTL (time to live)
 * and least recently used policy.  Expired key value pairs are evicted before
 * least recently used.  This cache differs from TlruCache in that it is more
 * efficient in managing the TTLs since they are uniform for all key value pairs
 * in the cache, rather than tracking items individually.
 *
 * This cache is sync aware and can be used concurrently from multiple threads safely.
 * To remove locks/synchronization use NO when creating the cache.
 *
 * @tparam KeyType The key type.  Must support std::hash().
 * @tparam ValueType The value type.  This is returned by copy on a find, so if your data
 *                   structure value is large it is advisable to store in a shared ptr.
 * @tparam SyncType By default this cache is thread safe, can be disabled for caches specific
 *                  to a single thread.
 */
template<typename KeyType, typename ValueType, Sync SyncType = Sync::YES>
class UtlruCache
{
public:
    using KeyedIterator = typename std::unordered_map<KeyType, size_t>::iterator;

    struct KeyValue
    {
        KeyValue(KeyType key, ValueType value) : m_key(std::move(key)), m_value(std::move(value)) {}

        KeyType   m_key;
        ValueType m_value;
    };

    /**
     * @param ttl The uniform TTL of every key value inserted into the cache.
     * @param capacity The maximum number of key value pairs allowed in the cache.
     * @param max_load_factor The load factor for the hash map, generally 1 is a good default.
     */
    UtlruCache(std::chrono::milliseconds ttl, size_t capacity, float max_load_factor = 1.0f);

    /**
     * Inserts or updates the given key value pair.  On update will reset the TTL.
     * @param key The key to store the value under.
     * @param value The value of data to store.
     * @param allow Allowed methods of insertion | update.  Defaults to allowing
     *              insertions and updates.
     * @return True if the operation was successful based on `allow`.
     */
    auto Insert(const KeyType& key, ValueType value, Allow allow = Allow::INSERT_OR_UPDATE) -> bool;

    /**
     * Inserts or updates a range of key value pairs.  This expects a container that has
     * 2 values in the {KeyType, ValueType} ordering.  There is a simple struct
     * LruCacheUniformTtl::KeyValue that can be put into any iterable container to satisfy
     * this requirement.
     * @tparam RangeType A container with two items, KeyType, ValueType.
     * @param key_value_range The elements to insert or update into the cache.
     * @param allow Allowed methods of insertion | update.  Defaults to allowing
     *              insertions and updates.
     * @return The number of elements inserted based on `allow`.
     */
    template<typename RangeType>
    auto InsertRange(RangeType&& key_value_range, Allow allow = Allow::INSERT_OR_UPDATE) -> size_t;

    /**
     * Attempts to delete the given key.
     * @param key The key to remove from the lru cache.
     * @return True if the key was deleted, false if the key does not exist.
     */
    auto Delete(const KeyType& key) -> bool;

    /**
     * Attempts to delete all given keys.
     * @tparam RangeType A container with the set of keys  to delete, e.g. vector<k> or set<k>.
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
    auto Find(const KeyType& key, Peek peek = Peek::NO) -> std::optional<ValueType>;

    /**
     * Attempts to find all the given keys values.
     * @tparam RangeType A container with the set of keys to lookup, e.g. vector<KeyType>.
     * @param key_range A container with the set of keys to lookup.
     * @param peek Should the find act like all the items were not used?
     * @return All input keys to either a std::nullopt if it doesn't exist, or the value if it does.
     */
    template<template<class...> typename RangeType>
    auto FindRange(const RangeType<KeyType>& key_range, Peek peek = Peek::NO)
        -> std::vector<std::pair<KeyType, std::optional<ValueType>>>;

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
    template<typename RangeType>
    auto FindRangeFill(RangeType& key_optional_value_range, Peek peek = Peek::NO) -> void;

    /**
     * Updates the uniform TTL for all new items or updated items in the cache.
     * Existing items that are not touched will TTL out at the previous uniform
     * TTL time.
     * @param ttl The new uniform TTL value to apply to all new elements.
     */
    auto UpdateTtl(std::chrono::milliseconds ttl) -> void;

    /**
     * Trims the TTL list of items an expunges all expired elements.  This could be useful to use
     * on downtime to make inserts faster if the cache is full by pruning TTL'ed elements.
     * @return The number of elements pruned.
     */
    auto CleanExpiredValues() -> size_t;

    /**
     * @return If this cache is currenty empty.
     */
    auto empty() const -> bool { return m_used_size == 0; }

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
        const KeyType&                        key,
        ValueType&&                           value,
        std::chrono::steady_clock::time_point now,
        std::chrono::steady_clock::time_point expire_time,
        Allow                                 allow) -> bool;

    auto doInsert(
        const KeyType&                        key,
        ValueType&&                           value,
        std::chrono::steady_clock::time_point now,
        std::chrono::steady_clock::time_point expire_time) -> void;

    auto doUpdate(KeyedIterator keyed_position, ValueType&& value, std::chrono::steady_clock::time_point expire_time)
        -> void;

    auto doDelete(size_t element_idx) -> void;

    auto doFind(const KeyType& key, std::chrono::steady_clock::time_point now, Peek peek) -> std::optional<ValueType>;

    auto doAccess(Element& element) -> void;

    auto doPrune(std::chrono::steady_clock::time_point now) -> void;

    /// Cache lock for all mutations.
    Lock<SyncType> m_lock;

    /// The uniform TTL for every key value pair inserted into the cache.
    std::chrono::milliseconds m_ttl;

    /// The current number of elements in the cache.
    size_t m_used_size{0};

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

namespace cappuccino
{
template<typename KeyType, typename ValueType, Sync SyncType>
UtlruCache<KeyType, ValueType, SyncType>::UtlruCache(
    std::chrono::milliseconds ttl, size_t capacity, float max_load_factor)
    : m_ttl(ttl),
      m_elements(capacity),
      m_lru_list(capacity)
{
    std::iota(m_lru_list.begin(), m_lru_list.end(), 0);
    m_lru_end = m_lru_list.begin();

    m_keyed_elements.max_load_factor(max_load_factor);
    m_keyed_elements.reserve(capacity);
}

template<typename KeyType, typename ValueType, Sync SyncType>
auto UtlruCache<KeyType, ValueType, SyncType>::Insert(const KeyType& key, ValueType value, Allow allow) -> bool
{
    auto now         = std::chrono::steady_clock::now();
    auto expire_time = now + m_ttl;

    std::lock_guard guard{m_lock};
    return doInsertUpdate(key, std::move(value), now, expire_time, allow);
};

template<typename KeyType, typename ValueType, Sync SyncType>
template<typename RangeType>
auto UtlruCache<KeyType, ValueType, SyncType>::InsertRange(RangeType&& key_value_range, Allow allow) -> size_t
{
    auto   now         = std::chrono::steady_clock::now();
    auto   expire_time = now + m_ttl;
    size_t inserted{0};

    {
        std::lock_guard guard{m_lock};
        for (auto& [key, value] : key_value_range)
        {
            if (doInsertUpdate(key, std::move(value), now, expire_time, allow))
            {
                ++inserted;
            }
        }
    }

    return inserted;
};

template<typename KeyType, typename ValueType, Sync SyncType>
auto UtlruCache<KeyType, ValueType, SyncType>::Delete(const KeyType& key) -> bool
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
};

template<typename KeyType, typename ValueType, Sync SyncType>
template<template<class...> typename RangeType>
auto UtlruCache<KeyType, ValueType, SyncType>::DeleteRange(const RangeType<KeyType>& key_range) -> size_t
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
};

template<typename KeyType, typename ValueType, Sync SyncType>
auto UtlruCache<KeyType, ValueType, SyncType>::Find(const KeyType& key, Peek peek) -> std::optional<ValueType>
{
    auto now = std::chrono::steady_clock::now();

    std::lock_guard guard{m_lock};
    return doFind(key, now, peek);
}

template<typename KeyType, typename ValueType, Sync SyncType>
template<template<class...> typename RangeType>
auto UtlruCache<KeyType, ValueType, SyncType>::FindRange(const RangeType<KeyType>& key_range, Peek peek)
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

template<typename KeyType, typename ValueType, Sync SyncType>
template<typename RangeType>
auto UtlruCache<KeyType, ValueType, SyncType>::FindRangeFill(RangeType& key_optional_value_range, Peek peek) -> void
{
    auto now = std::chrono::steady_clock::now();

    std::lock_guard guard{m_lock};

    for (auto& [key, optional_value] : key_optional_value_range)
    {
        optional_value = doFind(key, now, peek);
    }
}

template<typename KeyType, typename ValueType, Sync SyncType>
auto UtlruCache<KeyType, ValueType, SyncType>::UpdateTtl(std::chrono::milliseconds ttl) -> void
{
    m_ttl = ttl;
}

template<typename KeyType, typename ValueType, Sync SyncType>
auto UtlruCache<KeyType, ValueType, SyncType>::CleanExpiredValues() -> size_t
{
    size_t deleted_elements{0};
    auto   now = std::chrono::steady_clock::now();

    {
        std::lock_guard guard{m_lock};

        while (m_used_size > 0)
        {
            size_t   ttl_idx = *m_ttl_list.begin();
            Element& element = m_elements[ttl_idx];
            if (now >= element.m_expire_time)
            {
                ++deleted_elements;
                doDelete(ttl_idx);
            }
            else
            {
                break;
            }
        }
    }

    return deleted_elements;
}

template<typename KeyType, typename ValueType, Sync SyncType>
auto UtlruCache<KeyType, ValueType, SyncType>::doInsertUpdate(
    const KeyType&                        key,
    ValueType&&                           value,
    std::chrono::steady_clock::time_point now,
    std::chrono::steady_clock::time_point expire_time,
    Allow                                 allow) -> bool
{
    auto keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end())
    {
        if (update_allowed(allow))
        {
            doUpdate(keyed_position, std::move(value), expire_time);
            return true;
        }
        else if (insert_allowed(allow))
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
        if (insert_allowed(allow))
        {
            doInsert(key, std::move(value), now, expire_time);
            return true;
        }
    }
    return false;
}

template<typename KeyType, typename ValueType, Sync SyncType>
auto UtlruCache<KeyType, ValueType, SyncType>::doInsert(
    const KeyType&                        key,
    ValueType&&                           value,
    std::chrono::steady_clock::time_point now,
    std::chrono::steady_clock::time_point expire_time) -> void
{
    if (m_used_size >= m_elements.size())
    {
        doPrune(now);
    }
    auto element_idx = *m_lru_end;

    auto keyed_position = m_keyed_elements.emplace(key, element_idx).first;

    m_ttl_list.emplace_back(element_idx);

    Element& element         = m_elements[element_idx];
    element.m_value          = std::move(value);
    element.m_expire_time    = expire_time;
    element.m_lru_position   = m_lru_end;
    element.m_ttl_position   = std::prev(m_ttl_list.end());
    element.m_keyed_position = keyed_position;

    ++m_lru_end;

    ++m_used_size;

    doAccess(element);
}

template<typename KeyType, typename ValueType, Sync SyncType>
auto UtlruCache<KeyType, ValueType, SyncType>::doUpdate(
    KeyedIterator keyed_position, ValueType&& value, std::chrono::steady_clock::time_point expire_time) -> void
{
    size_t element_idx = keyed_position->second;

    Element& element      = m_elements[element_idx];
    element.m_expire_time = expire_time;
    element.m_value       = std::move(value);

    // push to the end of the ttl list
    m_ttl_list.splice(m_ttl_list.end(), m_ttl_list, element.m_ttl_position);

    doAccess(element);
}

template<typename KeyType, typename ValueType, Sync SyncType>
auto UtlruCache<KeyType, ValueType, SyncType>::doDelete(size_t element_idx) -> void
{
    Element& element = m_elements[element_idx];

    if (element.m_lru_position != std::prev(m_lru_end))
    {
        m_lru_list.splice(m_lru_end, m_lru_list, element.m_lru_position);
    }
    --m_lru_end;

    m_ttl_list.erase(element.m_ttl_position);

    m_keyed_elements.erase(element.m_keyed_position);

    --m_used_size;
}

template<typename KeyType, typename ValueType, Sync SyncType>
auto UtlruCache<KeyType, ValueType, SyncType>::doFind(
    const KeyType& key, std::chrono::steady_clock::time_point now, Peek peek) -> std::optional<ValueType>
{
    auto keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end())
    {
        size_t   element_idx = keyed_position->second;
        Element& element     = m_elements[element_idx];

        // Has the element TTL'ed?
        if (now < element.m_expire_time)
        {
            // Do not update items access if peeking.
            if (peek == Peek::NO)
            {
                doAccess(element);
            }
            return {element.m_value};
        }
        else
        {
            doDelete(element_idx);
        }
    }

    return {};
}

template<typename KeyType, typename ValueType, Sync SyncType>
auto UtlruCache<KeyType, ValueType, SyncType>::doAccess(Element& element) -> void
{
    m_lru_list.splice(m_lru_list.begin(), m_lru_list, element.m_lru_position);
}

template<typename KeyType, typename ValueType, Sync SyncType>
auto UtlruCache<KeyType, ValueType, SyncType>::doPrune(std::chrono::steady_clock::time_point now) -> void
{
    if (m_used_size > 0)
    {
        size_t   ttl_idx = *m_ttl_list.begin();
        Element& element = m_elements[ttl_idx];

        if (now >= element.m_expire_time)
        {
            doDelete(ttl_idx);
        }
        else
        {
            size_t lru_idx = m_lru_list.back();
            doDelete(lru_idx);
        }
    }
}

} // namespace cappuccino
