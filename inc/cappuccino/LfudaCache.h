#pragma once

#include "cappuccino/LfuCache.h"

#include <chrono>
#include <list>
#include <map>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace cappuccino {

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
 * To remove locks/synchronization use SyncImplEnum::UNSYNC when creating the cache.
 *
 * @tparam KeyType The key type.  Must support std::hash() and operator<()
 * @tparam ValueType The value type.  This is returned by copy on a find, so if your data
 *                   structure value is large it is advisable to store in a shared ptr.
 * @tparam SyncType By default this cache is thread safe, can be disabled for caches specific
 *                  to a single thread.
 */
template <typename KeyType, typename ValueType, SyncImplEnum SyncType = SyncImplEnum::SYNC>
class LfudaCache {
private:
    using KeyedIterator = typename std::unordered_map<KeyType, size_t>::iterator;

    /// The first item is the 'm_elements' index, the second is when the item was last touched.
    using AgeIterator = std::list<std::pair<size_t, std::chrono::steady_clock::time_point>>::iterator;

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
     * @param dynamic_age_tick The amount of time to pass before dynamically aging items.
     *                         Items that haven't been used for this amount of time will have
     *                         their use count reduced by the dynamic age ratio value.
     * @param dynamic_age_ratio The 'used' amount to age items by when they dynamically age.
     *                          The default is to halve their use count.
     * @param max_load_factor The load factor for the hash map, generally 1 is a good default.
     */
    explicit LfudaCache(
        size_t capacity,
        std::chrono::seconds dynamic_age_tick = std::chrono::minutes { 1 },
        float dynamic_age_ratio = 0.5f,
        float max_load_factor = 1.0f);

    /**
     * Inserts or updates the given key value pair.
     * Note that this function will attempt to dynamically age as many items as it
     * can that are ready to be dynamically aged.  Worst case running time is O(n)
     * if every item was inserted into the cache with the same timestamp.
     * @param key The key to store the value under.
     * @param value The value of the data to store.
     */
    auto Insert(
        const KeyType& key,
        ValueType value) -> void;

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
     * Attempts to dynamically age the elements in the cache.
     * This function has a worst case running time of O(n) IIF every element is inserted
     * with the same timestamp -- as it will dynamically age every item in the cache if they
     * are all ready to be aged.
     * @return The number of items dynamically aged.
     */
    auto DynamicallyAge() -> size_t;

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
        /// The iterator into the dynamic age data structure.
        AgeIterator m_dynamic_age_position;
        /// The element's value.
        ValueType m_value;
    };

    auto doInsertUpdate(
        const KeyType& key,
        ValueType&& value,
        std::chrono::steady_clock::time_point now) -> void;

    auto doInsert(
        const KeyType& key,
        ValueType&& value,
        std::chrono::steady_clock::time_point now) -> void;

    auto doUpdate(
        KeyedIterator keyed_position,
        ValueType&& value,
        std::chrono::steady_clock::time_point now) -> void;

    auto doDelete(
        size_t element_idx) -> void;

    auto doFind(
        const KeyType& key,
        bool peek,
        std::chrono::steady_clock::time_point now) -> std::optional<ValueType>;

    auto doFindWithUseCount(
        const KeyType& key,
        bool peek,
        std::chrono::steady_clock::time_point now) -> std::optional<std::pair<ValueType, size_t>>;

    auto doAccess(
        Element& element,
        std::chrono::steady_clock::time_point now) -> void;

    auto doPrune(
        std::chrono::steady_clock::time_point now) -> void;

    auto doDynamicAge(
        std::chrono::steady_clock::time_point now) -> size_t;

    /// Cache lock for all mutations if sync is enabled.
    std::mutex m_lock;

    /// The current number of elements in the cache.
    size_t m_used_size { 0 };

    /// The amount of time for an element to not be touched to dynamically age.
    std::chrono::seconds m_dynamic_age_tick { std::chrono::minutes { 1 } };
    /// The ratio amount of 'uses' to remove when an element dynamically ages.
    float m_dynamic_age_ratio { 0.5f };

    /// The main store for the key value pairs and metadata for each element.
    std::vector<Element> m_elements;
    /// The keyed lookup data structure, the value is the index into 'm_elements'.
    std::unordered_map<KeyType, size_t> m_keyed_elements;
    /// The lfu sorted map, the key is the number of times the element has been used,
    /// the value is the index into 'm_elements'.
    std::multimap<size_t, size_t> m_lfu_list;
    /// The dynamic age list, this also contains a partition on un-used 'm_elements'.
    std::list<std::pair<size_t, std::chrono::steady_clock::time_point>> m_dynamic_age_list;
    /// The end of the open list to pull open slots from.
    AgeIterator m_open_list_end;
};

} // namespace cappuccino

#include "cappuccino/LfudaCache.tcc"
