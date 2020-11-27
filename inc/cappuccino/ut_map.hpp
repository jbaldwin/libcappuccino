#pragma once

#include "cappuccino/allow.hpp"
#include "cappuccino/lock.hpp"

#include <atomic>
#include <chrono>
#include <list>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace cappuccino
{
/**
 * Uniform time aware associative map for key value pairs. The associative map
 * has no fixed size and will dynamically resize as key values are added. The
 * map is less performant than the cache data structures, but offers unlimited
 * size.
 *
 * Each key value pair is evicted based on an global map TTL (time to live)
 * Expired key value pairs are evicted on the first Insert, Delete, Find, or
 * CleanExpiredValues call after the TTL has elapsed. The map may have O(N)
 * performance during these operations if N elements are reaching their TTLs and
 * being evicted at once.
 *
 * This map is sync aware and can be used concurrently from multiple threads
 * safely. To remove locks/synchronization use sync::no when creating the cache.
 *
 * @tparam KeyType The key type.  Must support std::hash().
 * @tparam ValueType The value type.  This is returned by copy on a find, so if
 * your data structure value is large it is advisable to store in a shared ptr.
 * @tparam sync_type By default this map is thread safe, can be disabled for maps
 * specific to a single thread.
 */
template<typename KeyType, typename ValueType, sync sync_type = sync::yes>
class UtMap
{
public:
    /**
     * A simple {KeyType, ValueType} struct that can be used with InsertRange
     * function.
     */
    struct KeyValue
    {
        KeyValue(KeyType key, ValueType value) : m_key(std::move(key)), m_value(std::move(value)) {}

        KeyType   m_key;
        ValueType m_value;
    };

    /**
     * @param uniform_ttl The uniform TTL for key values inserted into the map.
     * 100ms default.
     */
    UtMap(std::chrono::milliseconds uniform_ttl = std::chrono::milliseconds{100});

    /**
     * Inserts or updates the given key value pair using the uniform TTL.  On
     * update will reset the TTL.
     * @param key The key to store the value under.
     * @param value The value of data to store.
     * @param a Allowed methods of insertion | update.  Defaults to allowing
     *              insertions and updates.
     * @return True if the operation was successful based on `allow`.
     */
    auto Insert(const KeyType& key, ValueType value, allow a = allow::insert_or_update) -> bool;

    /**
     * Inserts or updates a range of key value pairs using the default TTL.
     * This expects a container that has 2 values in the {KeyType, ValueType}
     * ordering.  There is a simple struct UtMap::KeyValue that can be put into
     * any iterable container to satisfy this requirement.
     * @tparam RangeType A container with two items, KeyType, ValueType.
     * @param key_value_range The elements to insert or update into the map.
     * @param a Allowed methods of insertion | update.  Defaults to allowing
     *              insertions and updates.
     * @return The number of elements inserted based on `allow`.
     */
    template<typename RangeType>
    auto InsertRange(RangeType&& key_value_range, allow a = allow::insert_or_update) -> size_t;

    /**
     * Attempts to delete the given key.
     * @param key The key to remove from the map.
     * @return True if the key was deleted, false if the key does not exist.
     */
    auto Delete(const KeyType& key) -> bool;

    /**
     * Attempts to delete all given keys.
     * @tparam RangeType A container with the set of keys to delete, e.g.
     * vector<k> or set<k>.
     * @param key_range The keys to delete from the map.
     * @return The number of items deleted from the map.
     */
    template<template<class...> typename RangeType>
    auto DeleteRange(const RangeType<KeyType>& key_range) -> size_t;

    /**
     * Attempts to find the given key's value.
     * @param key The key to lookup its value.
     * @return An optional with the key's value if it exists, or an empty optional
     * if it does not.
     */
    auto Find(const KeyType& key) -> std::optional<ValueType>;

    /**
     * Attempts to find all the given keys values.
     * @tparam RangeType A container with the set of keys to lookup, e.g.
     * vector<KeyType>.
     * @param key_range A container with the set of keys to lookup.
     * @return All input keys to either a std::nullopt if it doesn't exist, or the
     * value if it does.
     */
    template<template<class...> typename RangeType>
    auto FindRange(const RangeType<KeyType>& key_range) -> std::vector<std::pair<KeyType, std::optional<ValueType>>>;

    /**
     * Attempts to find all given keys values.
     *
     * The user should initialize this container with the keys to lookup with the
     * values all empty optionals.  The keys that are found will have the
     * optionals filled in with the appropriate values from the map.
     *
     * @tparam RangeType A container with a pair of optional items,
     *                   e.g. vector<pair<k, optional<v>>> or map<k, optional<v>>.
     * @param key_optional_value_range The keys to optional values to fill out.
     */
    template<typename RangeType>
    auto FindRangeFill(RangeType& key_optional_value_range) -> void;

    /**
     * Trims the TTL list of items and evicts all expired elements.
     * @return The number of elements pruned.
     */
    auto CleanExpiredValues() -> size_t;

    /**
     * @return If this map is currently empty.
     */
    auto empty() const -> bool { return size() == 0ul; }

    /**
     * @return The number of elements inside the map.
     */
    auto size() const -> size_t;

private:
    struct KeyedElement;
    struct TtlElement;

    using KeyedIterator = typename std::map<KeyType, KeyedElement>::iterator;
    using TtlIterator   = typename std::list<TtlElement>::iterator;

    struct KeyedElement
    {
        /// The element's value.
        ValueType m_value;
        /// The iterator into the ttl data structure.
        TtlIterator m_ttl_position;
    };

    struct TtlElement
    {
        TtlElement(std::chrono::steady_clock::time_point expire_time, KeyedIterator keyed_elements_position)
            : m_expire_time(expire_time),
              m_keyed_elements_position(keyed_elements_position)
        {
        }
        /// The point in time in  which this element's value expires.
        std::chrono::steady_clock::time_point m_expire_time;
        /// The iterator to the Key in the data structure.
        KeyedIterator m_keyed_elements_position;
    };

    auto doInsertUpdate(
        const KeyType& key, ValueType&& value, std::chrono::steady_clock::time_point expire_time, allow a) -> bool;

    auto doInsert(const KeyType& key, ValueType&& value, std::chrono::steady_clock::time_point expire_time) -> void;

    auto doUpdate(KeyedIterator keyed_position, ValueType&& value, std::chrono::steady_clock::time_point expire_time)
        -> void;

    auto doDelete(KeyedIterator keyed_elements_position) -> void;

    auto doFind(const KeyType& key) -> std::optional<ValueType>;

    auto doPrune(std::chrono::steady_clock::time_point now) -> size_t;

    /// Thread lock for all mutations.
    mutex<sync_type> m_lock;

    /// The keyed lookup data structure, the value is the KeyedElement struct
    /// which includes the value and an iterator to the associated m_ttl_list
    /// TTlElement.
    typename std::map<KeyType, KeyedElement> m_keyed_elements;

    /// The uniform ttl sorted list.
    typename std::list<TtlElement> m_ttl_list;

    /// The uniform TTL for every key value pair inserted into the map.
    std::chrono::milliseconds m_uniform_ttl;
};
} // namespace cappuccino

namespace cappuccino
{
template<typename KeyType, typename ValueType, sync sync_type>
UtMap<KeyType, ValueType, sync_type>::UtMap(std::chrono::milliseconds uniform_ttl) : m_uniform_ttl(uniform_ttl)
{
}

template<typename KeyType, typename ValueType, sync sync_type>
auto UtMap<KeyType, ValueType, sync_type>::Insert(const KeyType& key, ValueType value, allow a) -> bool
{
    std::lock_guard guard{m_lock};
    const auto      now         = std::chrono::steady_clock::now();
    const auto      expire_time = now + m_uniform_ttl;

    doPrune(now);

    return doInsertUpdate(key, std::move(value), expire_time, a);
}

template<typename KeyType, typename ValueType, sync sync_type>
template<typename RangeType>
auto UtMap<KeyType, ValueType, sync_type>::InsertRange(RangeType&& key_value_range, allow a) -> size_t
{
    size_t          inserted{0};
    std::lock_guard guard{m_lock};
    const auto      now         = std::chrono::steady_clock::now();
    const auto      expire_time = now + m_uniform_ttl;

    doPrune(now);

    for (auto& [key, value] : key_value_range)
    {
        if (doInsertUpdate(key, std::move(value), expire_time, a))
        {
            ++inserted;
        }
    }

    return inserted;
}

template<typename KeyType, typename ValueType, sync sync_type>
auto UtMap<KeyType, ValueType, sync_type>::Delete(const KeyType& key) -> bool
{
    std::lock_guard guard{m_lock};
    const auto      now = std::chrono::steady_clock::now();

    doPrune(now);

    const auto keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end())
    {
        doDelete(keyed_position);
        return true;
    }
    else
    {
        return false;
    }
};

template<typename KeyType, typename ValueType, sync sync_type>
template<template<class...> typename RangeType>
auto UtMap<KeyType, ValueType, sync_type>::DeleteRange(const RangeType<KeyType>& key_range) -> size_t
{
    size_t          deleted{0};
    std::lock_guard guard{m_lock};
    const auto      now = std::chrono::steady_clock::now();

    doPrune(now);

    for (const auto& key : key_range)
    {
        const auto keyed_position = m_keyed_elements.find(key);
        if (keyed_position != m_keyed_elements.end())
        {
            doDelete(keyed_position);
            ++deleted;
        }
    }

    return deleted;
};

template<typename KeyType, typename ValueType, sync sync_type>
auto UtMap<KeyType, ValueType, sync_type>::Find(const KeyType& key) -> std::optional<ValueType>
{
    std::lock_guard guard{m_lock};
    const auto      now = std::chrono::steady_clock::now();

    doPrune(now);

    return doFind(key);
}

template<typename KeyType, typename ValueType, sync sync_type>
template<template<class...> typename RangeType>
auto UtMap<KeyType, ValueType, sync_type>::FindRange(const RangeType<KeyType>& key_range)
    -> std::vector<std::pair<KeyType, std::optional<ValueType>>>
{
    std::vector<std::pair<KeyType, std::optional<ValueType>>> output;
    output.reserve(std::size(key_range));

    std::lock_guard guard{m_lock};
    const auto      now = std::chrono::steady_clock::now();

    doPrune(now);

    for (const auto& key : key_range)
    {
        output.emplace_back(key, doFind(key));
    }

    return output;
}

template<typename KeyType, typename ValueType, sync sync_type>
template<typename RangeType>
auto UtMap<KeyType, ValueType, sync_type>::FindRangeFill(RangeType& key_optional_value_range) -> void
{
    std::lock_guard guard{m_lock};
    const auto      now = std::chrono::steady_clock::now();

    doPrune(now);

    for (auto& [key, optional_value] : key_optional_value_range)
    {
        optional_value = doFind(key);
    }
}

template<typename KeyType, typename ValueType, sync sync_type>
auto UtMap<KeyType, ValueType, sync_type>::CleanExpiredValues() -> size_t
{
    std::lock_guard guard{m_lock};
    const auto      now = std::chrono::steady_clock::now();
    return doPrune(now);
}

template<typename KeyType, typename ValueType, sync sync_type>
auto UtMap<KeyType, ValueType, sync_type>::size() const -> size_t
{
    std::atomic_thread_fence(std::memory_order_acquire);
    return m_keyed_elements.size();
}

template<typename KeyType, typename ValueType, sync sync_type>
auto UtMap<KeyType, ValueType, sync_type>::doInsertUpdate(
    const KeyType& key, ValueType&& value, std::chrono::steady_clock::time_point expire_time, allow a) -> bool
{
    const auto keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end())
    {
        if (update_allowed(a))
        {
            doUpdate(keyed_position, std::move(value), expire_time);
            return true;
        }
    }
    else
    {
        if (insert_allowed(a))
        {
            doInsert(key, std::move(value), expire_time);
            return true;
        }
    }
    return false;
}

template<typename KeyType, typename ValueType, sync sync_type>
auto UtMap<KeyType, ValueType, sync_type>::doInsert(
    const KeyType& key, ValueType&& value, std::chrono::steady_clock::time_point expire_time) -> void
{
    KeyedElement element;
    element.m_value = std::move(value);

    auto keyed_position = m_keyed_elements.emplace(key, std::move(element)).first;

    m_ttl_list.emplace_back(expire_time, keyed_position);

    // Update the elements iterator to ttl_element.
    keyed_position->second.m_ttl_position = std::prev(m_ttl_list.end());
}

template<typename KeyType, typename ValueType, sync sync_type>
auto UtMap<KeyType, ValueType, sync_type>::doUpdate(
    KeyedIterator keyed_position, ValueType&& value, std::chrono::steady_clock::time_point expire_time) -> void
{
    auto& element   = keyed_position->second;
    element.m_value = std::move(value);

    // Update the TtlElement's expire time.
    element.m_ttl_position->m_expire_time = expire_time;

    // Push to the end of the Ttl list.
    m_ttl_list.splice(m_ttl_list.end(), m_ttl_list, element.m_ttl_position);

    // Update the elements iterator to ttl_element.
    element.m_ttl_position = std::prev(m_ttl_list.end());
}

template<typename KeyType, typename ValueType, sync sync_type>
auto UtMap<KeyType, ValueType, sync_type>::doDelete(KeyedIterator keyed_elements_position) -> void
{
    m_ttl_list.erase(keyed_elements_position->second.m_ttl_position);

    m_keyed_elements.erase(keyed_elements_position);
}

template<typename KeyType, typename ValueType, sync sync_type>
auto UtMap<KeyType, ValueType, sync_type>::doFind(const KeyType& key) -> std::optional<ValueType>
{
    const auto keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end())
    {
        return {keyed_position->second.m_value};
    }

    return {};
}

template<typename KeyType, typename ValueType, sync sync_type>
auto UtMap<KeyType, ValueType, sync_type>::doPrune(std::chrono::steady_clock::time_point now) -> size_t
{
    const auto  ttl_begin = m_ttl_list.begin();
    const auto  ttl_end   = m_ttl_list.end();
    TtlIterator ttl_iter;
    size_t      deleted{0};

    // Delete the keyed elements from the map. Not using doDelete to take
    // advantage of iterator range delete for TTLs.
    for (ttl_iter = ttl_begin; ttl_iter != ttl_end && now >= ttl_iter->m_expire_time; ++ttl_iter)
    {
        m_keyed_elements.erase(ttl_iter->m_keyed_elements_position);
        ++deleted;
    }

    // Delete the range of TTLs.
    if (ttl_iter != ttl_begin)
    {
        m_ttl_list.erase(ttl_begin, ttl_iter);
    }

    return deleted;
}

} // namespace cappuccino
