#pragma once

#include "cappuccino/allow.hpp"
#include "cappuccino/lock.hpp"

#include <chrono>
#include <list>
#include <map>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace cappuccino
{
/**
 * Least Frequently Used (LFU) Cache with Dynamic Aging.
 * Each key value pair is evicted based on being the least frequently used, and no other
 * criteria.  However, items will dynamically age based on a time tick frequency and a ratio
 * to reduce its use count.  This means that items that are hot for a short while won't
 * stick around in the cache with high use counts as they will dynamically age out over
 * a period of time.  The dynamic age tick count as well as the ratio can be tuned by the
 * user of the cache.
 *
 * This cache is sync aware and can be used concurrently from multiple threads safely.
 * To remove locks/synchronization use NO when creating the cache.
 *
 * @tparam KeyType The key type.  Must support std::hash() and operator<()
 * @tparam ValueType The value type.  This is returned by copy on a find, so if your data
 *                   structure value is large it is advisable to store in a shared ptr.
 * @tparam sync_type By default this cache is thread safe, can be disabled for caches specific
 *                  to a single thread.
 */
template<typename KeyType, typename ValueType, sync sync_type = sync::yes>
class LfudaCache
{
private:
    struct Element;

    using AgeIterator   = typename std::list<Element>::iterator;
    using KeyedIterator = typename std::unordered_map<KeyType, AgeIterator>::iterator;
    using LfuIterator   = typename std::multimap<size_t, AgeIterator>::iterator;

public:
    struct KeyValue
    {
        KeyValue(KeyType key, ValueType value) : m_key(std::move(key)), m_value(std::move(value)) {}

        KeyType   m_key;
        ValueType m_value;
    };

    /**
     * @param capacity The maximum number of key value pairs allowed in the cache.
     * @param dynamic_age_tick The amount of time to pass before dynamically aging items.
     *                         Items that haven't been used for this amount of time will have
     *                         their use count reduced by the dynamic age ratio value.
     * @param dynamic_age_ratio The 'used' amount to age items by when they dynamically age.
     *                          The default is to halve their use count.
     * @param max_load_factor The load factor for the hash map, generally 1 is a good default.
     */
    explicit LfudaCache(
        size_t                    capacity,
        std::chrono::milliseconds dynamic_age_tick  = std::chrono::minutes{1},
        float                     dynamic_age_ratio = 0.5f,
        float                     max_load_factor   = 1.0f);

    /**
     * Inserts or updates the given key value pair.
     * Note that this function will attempt to dynamically age as many items as it
     * can that are ready to be dynamically aged.  Worst case running time is O(n)
     * if every item was inserted into the cache with the same timestamp.
     * @param key The key to store the value under.
     * @param value The value of the data to store.
     * @param a Allowed methods of insertion | update.  Defaults to allowing
     *              insertions and updates.
     * @return True if the operation was successful based on `allow`.
     */
    auto Insert(const KeyType& key, ValueType value, allow a = allow::insert_or_update) -> bool;

    /**
     * Inserts or updates a range of key value pairs.  This expects a container
     * that has 2 values in the {KeyType, ValueType} ordering.
     * There is a simple struct provided on the LfudaCache::KeyValue that can be put
     * into any iterable container to satisfy this requirement.
     *
     * Note that this function might make future Inserts extremely expensive if
     * it is inserting / updating a lot of items at once.  This is because each item inserted
     * will have the same dynamic age timestamp and could cause Inserts to be very expensive
     * when all of those items dynamically age at the same time.  User beware.
     *
     * @tparam RangeType A container with two items, KeyType, ValueType.
     * @param key_value_range The elements to insert or update into the cache.
     * @param a Allowed methods of insertion | update.  Defaults to allowing
     *              insertions and updates.
     * @return The number of elements inserted based on `allow`.
     */
    template<typename RangeType>
    auto InsertRange(RangeType&& key_value_range, allow a = allow::insert_or_update) -> size_t;

    /**
     * Attempts to delete the given key.
     * @param key The key to delete from the lru cache.
     * @return True if the key was deleted, false if the key does not exist.
     */
    auto Delete(const KeyType& key) -> bool;

    /**
     * Attempts to delete all given keys.
     * @tparam RangeType A container with the set of keys to delete, e.g. vector<KeyType>, set<KeyType>.
     * @param key_range The keys to delete from the cache.
     * @return The number of items deleted from the cache.
     */
    template<template<class...> typename RangeType>
    auto DeleteRange(const RangeType<KeyType>& key_range) -> size_t;

    /**
     * Attempts to find the given key's value.
     * @param key The key to lookup its value.
     * @param peek Should the find act like the item wasn't used?
     * @return An optional with the key's value if it exists, or an empty optional if it does not.
     */
    auto Find(const KeyType& key, bool peek = false) -> std::optional<ValueType>;

    /**
     * Attempts to find the given key's value and use count.
     * @param key The key to lookup its value.
     * @param peek Should the find act like the item wasn't used?
     * @return An optional with the key's value and use count if it exists, or empty optional if it does not.
     */
    auto FindWithUseCount(const KeyType& key, bool peek = false) -> std::optional<std::pair<ValueType, size_t>>;

    /**
     * Attempts to find all the given keys values.
     * @tparam RangeType A container with the set of keys to find their values, e.g. vector<KeyType>.
     * @param key_range The keys to lookup their pairs.
     * @param peek Should the find act like all the items were not used?
     * @return The full set of keys to std::nullopt if the key wasn't found, or the value if found.
     */
    template<template<class...> typename RangeType>
    auto FindRange(const RangeType<KeyType>& key_range, bool peek = false)
        -> std::vector<std::pair<KeyType, std::optional<ValueType>>>;

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
    template<typename RangeType>
    auto FindRangeFill(RangeType& key_optional_value_range, bool peek = false) -> void;

    /**
     * Attempts to dynamically age the elements in the cache.
     * This function has a worst case running time of O(n) IIF every element is inserted
     * with the same timestamp -- as it will dynamically age every item in the cache if they
     * are all ready to be aged.
     * @return The number of items dynamically aged.
     */
    auto DynamicallyAge() -> size_t;

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
    auto capacity() const -> size_t { return m_dynamic_age_list.size(); }

private:
    struct Element
    {
        /// The iterator into the keyed data structure.
        KeyedIterator m_keyed_position;
        /// The iterator into the lfu data structure.
        LfuIterator m_lfu_position;
        /// The dynamic age timestamp.
        std::chrono::steady_clock::time_point m_dynamic_age;
        /// The element's value.
        ValueType m_value;
    };

    auto doInsertUpdate(const KeyType& key, ValueType&& value, std::chrono::steady_clock::time_point now, allow a)
        -> bool;

    auto doInsert(const KeyType& key, ValueType&& value, std::chrono::steady_clock::time_point now) -> void;

    auto doUpdate(KeyedIterator keyed_position, ValueType&& value, std::chrono::steady_clock::time_point now) -> void;

    auto doDelete(AgeIterator age_iterator) -> void;

    auto doFind(const KeyType& key, bool peek, std::chrono::steady_clock::time_point now) -> std::optional<ValueType>;

    auto doFindWithUseCount(const KeyType& key, bool peek, std::chrono::steady_clock::time_point now)
        -> std::optional<std::pair<ValueType, size_t>>;

    auto doAccess(Element& element, std::chrono::steady_clock::time_point now) -> void;

    auto doPrune(std::chrono::steady_clock::time_point now) -> void;

    auto doDynamicAge(std::chrono::steady_clock::time_point now) -> size_t;

    /// Cache lock for all mutations if sync is enabled.
    mutex<sync_type> m_lock;

    /// The current number of elements in the cache.
    size_t m_used_size{0};

    /// The amount of time for an element to not be touched to dynamically age.
    std::chrono::milliseconds m_dynamic_age_tick{std::chrono::minutes{1}};
    /// The ratio amount of 'uses' to remove when an element dynamically ages.
    float m_dynamic_age_ratio{0.5f};

    /// The keyed lookup data structure, the value is the index into 'm_elements'.
    std::unordered_map<KeyType, AgeIterator> m_keyed_elements;
    /// The lfu sorted map, the key is the number of times the element has been used,
    /// the value is the index into 'm_elements'.
    std::multimap<size_t, AgeIterator> m_lfu_list;
    /// The dynamic age list, this also contains a partition on un-used 'm_elements'.
    std::list<Element> m_dynamic_age_list;
    /// The end of the open list to pull open slots from.
    AgeIterator m_open_list_end;
};

template<typename KeyType, typename ValueType, sync sync_type>
LfudaCache<KeyType, ValueType, sync_type>::LfudaCache(
    size_t capacity, std::chrono::milliseconds dynamic_age_tick, float dynamic_age_ratio, float max_load_factor)
    : m_dynamic_age_tick(dynamic_age_tick),
      m_dynamic_age_ratio(dynamic_age_ratio),
      m_dynamic_age_list(capacity)
{
    m_open_list_end = m_dynamic_age_list.begin();

    m_keyed_elements.max_load_factor(max_load_factor);
    m_keyed_elements.reserve(capacity);
}

template<typename KeyType, typename ValueType, sync sync_type>
auto LfudaCache<KeyType, ValueType, sync_type>::Insert(const KeyType& key, ValueType value, allow a) -> bool
{
    auto now = std::chrono::steady_clock::now();

    std::lock_guard guard{m_lock};
    return doInsertUpdate(key, std::move(value), now, a);
}

template<typename KeyType, typename ValueType, sync sync_type>
template<typename RangeType>
auto LfudaCache<KeyType, ValueType, sync_type>::InsertRange(RangeType&& key_value_range, allow a) -> size_t
{
    auto now = std::chrono::steady_clock::now();

    size_t inserted{0};

    {
        std::lock_guard guard{m_lock};
        for (auto& [key, value] : key_value_range)
        {
            if (doInsertUpdate(key, std::move(value), now, a))
            {
                ++inserted;
            }
        }
    }

    return inserted;
}

template<typename KeyType, typename ValueType, sync sync_type>
auto LfudaCache<KeyType, ValueType, sync_type>::Delete(const KeyType& key) -> bool
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
auto LfudaCache<KeyType, ValueType, sync_type>::DeleteRange(const RangeType<KeyType>& key_range) -> size_t
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
auto LfudaCache<KeyType, ValueType, sync_type>::Find(const KeyType& key, bool peek) -> std::optional<ValueType>
{
    auto now = std::chrono::steady_clock::now();

    std::lock_guard guard{m_lock};
    return doFind(key, peek, now);
}

template<typename KeyType, typename ValueType, sync sync_type>
auto LfudaCache<KeyType, ValueType, sync_type>::FindWithUseCount(const KeyType& key, bool peek)
    -> std::optional<std::pair<ValueType, size_t>>
{
    auto now = std::chrono::steady_clock::now();

    std::lock_guard guard{m_lock};
    return doFindWithUseCount(key, peek, now);
}

template<typename KeyType, typename ValueType, sync sync_type>
template<template<class...> typename RangeType>
auto LfudaCache<KeyType, ValueType, sync_type>::FindRange(const RangeType<KeyType>& key_range, bool peek)
    -> std::vector<std::pair<KeyType, std::optional<ValueType>>>
{
    auto now = std::chrono::steady_clock::now();

    std::vector<std::pair<KeyType, std::optional<ValueType>>> output;
    output.reserve(std::size(key_range));

    {
        std::lock_guard guard{m_lock};
        for (auto& key : key_range)
        {
            output.emplace_back(key, doFind(key, peek, now));
        }
    }

    return output;
}

template<typename KeyType, typename ValueType, sync sync_type>
template<typename RangeType>
auto LfudaCache<KeyType, ValueType, sync_type>::FindRangeFill(RangeType& key_optional_value_range, bool peek) -> void
{
    auto now = std::chrono::steady_clock::now();

    std::lock_guard guard{m_lock};
    for (auto& [key, optional_value] : key_optional_value_range)
    {
        optional_value = doFind(key, peek, now);
    }
}

template<typename KeyType, typename ValueType, sync sync_type>
auto LfudaCache<KeyType, ValueType, sync_type>::DynamicallyAge() -> size_t
{
    auto now = std::chrono::steady_clock::now();

    std::lock_guard guard{m_lock};
    return doDynamicAge(now);
}

template<typename KeyType, typename ValueType, sync sync_type>
auto LfudaCache<KeyType, ValueType, sync_type>::doInsertUpdate(
    const KeyType& key, ValueType&& value, std::chrono::steady_clock::time_point now, allow a) -> bool
{
    auto keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end())
    {
        if (update_allowed(a))
        {
            doUpdate(keyed_position, std::move(value), now);
            return true;
        }
    }
    else
    {
        if (insert_allowed(a))
        {
            doInsert(key, std::move(value), now);
            return true;
        }
    }

    return false;
}

template<typename KeyType, typename ValueType, sync sync_type>
auto LfudaCache<KeyType, ValueType, sync_type>::doInsert(
    const KeyType& key, ValueType&& value, std::chrono::steady_clock::time_point now) -> void
{
    if (m_used_size >= m_dynamic_age_list.size())
    {
        doPrune(now);
    }

    Element& element = *m_open_list_end;

    auto keyed_position = m_keyed_elements.emplace(key, m_open_list_end).first;
    auto lfu_position   = m_lfu_list.emplace(1, m_open_list_end);

    element.m_value          = std::move(value);
    element.m_keyed_position = keyed_position;
    element.m_lfu_position   = lfu_position;
    element.m_dynamic_age    = now;

    ++m_open_list_end;

    ++m_used_size;
}

template<typename KeyType, typename ValueType, sync sync_type>
auto LfudaCache<KeyType, ValueType, sync_type>::doUpdate(
    LfudaCache::KeyedIterator keyed_position, ValueType&& value, std::chrono::steady_clock::time_point now) -> void
{
    Element& element = *keyed_position->second;
    element.m_value  = std::move(value);

    doAccess(element, now);
}

template<typename KeyType, typename ValueType, sync sync_type>
auto LfudaCache<KeyType, ValueType, sync_type>::doDelete(AgeIterator age_iterator) -> void
{
    Element& element = *age_iterator;

    if (age_iterator != std::prev(m_open_list_end))
    {
        m_dynamic_age_list.splice(m_open_list_end, m_dynamic_age_list, age_iterator);
    }
    --m_open_list_end;

    m_keyed_elements.erase(element.m_keyed_position);
    m_lfu_list.erase(element.m_lfu_position);

    --m_used_size;
}

template<typename KeyType, typename ValueType, sync sync_type>
auto LfudaCache<KeyType, ValueType, sync_type>::doFind(
    const KeyType& key, bool peek, std::chrono::steady_clock::time_point now) -> std::optional<ValueType>
{
    auto keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end())
    {
        Element& element = *keyed_position->second;
        if (!peek)
        {
            doAccess(element, now);
        }
        return {element.m_value};
    }

    return {};
}

template<typename KeyType, typename ValueType, sync sync_type>
auto LfudaCache<KeyType, ValueType, sync_type>::doFindWithUseCount(
    const KeyType& key, bool peek, std::chrono::steady_clock::time_point now)
    -> std::optional<std::pair<ValueType, size_t>>
{
    auto keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end())
    {
        Element& element = *keyed_position->second;
        if (!peek)
        {
            doAccess(element, now);
        }
        return {std::make_pair(element.m_value, element.m_lfu_position->first)};
    }

    return {};
}

template<typename KeyType, typename ValueType, sync sync_type>
auto LfudaCache<KeyType, ValueType, sync_type>::doAccess(
    LfudaCache::Element& element, std::chrono::steady_clock::time_point now) -> void
{
    // Update lfu position.
    auto use_count = element.m_lfu_position->first;
    m_lfu_list.erase(element.m_lfu_position);
    element.m_lfu_position = m_lfu_list.emplace(use_count + 1, element.m_keyed_position->second);

    // Update dynamic aging position.
    auto last_aged_item = std::prev(m_open_list_end);
    // swap to the end of the aged list and update its time.
    if (element.m_keyed_position->second != last_aged_item)
    {
        m_dynamic_age_list.splice(last_aged_item, m_dynamic_age_list, element.m_keyed_position->second);
    }
    element.m_dynamic_age = now;
}

template<typename KeyType, typename ValueType, sync sync_type>
auto LfudaCache<KeyType, ValueType, sync_type>::doPrune(std::chrono::steady_clock::time_point now) -> void
{
    if (m_used_size > 0)
    {
        doDynamicAge(now);

        // Now delete the least frequently used item after dynamically aging.
        doDelete(m_lfu_list.begin()->second);
    }
}

template<typename KeyType, typename ValueType, sync sync_type>
auto LfudaCache<KeyType, ValueType, sync_type>::doDynamicAge(std::chrono::steady_clock::time_point now) -> size_t
{
    size_t aged{0};

    // For all items in the DA list that are old enough and in use, DA them!
    auto da_start = m_dynamic_age_list.begin();
    auto da_last  = m_open_list_end; // need the item previous to the end to splice properly
    while (da_start != m_open_list_end && (*da_start).m_dynamic_age + m_dynamic_age_tick < now)
    {
        // swap to after the last item (if it isn't the last item..)
        if (da_start != da_last)
        {
            m_dynamic_age_list.splice(da_last, m_dynamic_age_list, da_start);
        }

        // Update its dynamic age time to now.
        Element& element      = *da_start;
        element.m_dynamic_age = now;
        // Now *= ratio its use count to actually age it.  This requires
        // deleting from and re-inserting into the lfu data structure.
        size_t use_count = element.m_lfu_position->first;
        m_lfu_list.erase(element.m_lfu_position);
        use_count *= m_dynamic_age_ratio;
        element.m_lfu_position = m_lfu_list.emplace(use_count, da_start);

        // The last item is now this item!  This will maintain the insertion order.
        da_last = da_start;

        // Keep pruning from the beginning until we are m_open_list_end or not aged enough.
        da_start = m_dynamic_age_list.begin();
        ++aged;
    }

    return aged;
}

} // namespace cappuccino
