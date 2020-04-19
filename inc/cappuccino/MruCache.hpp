#pragma once

#include "cappuccino/CappuccinoLock.hpp"
#include "cappuccino/SyncImplEnum.hpp"

#include <list>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace cappuccino {

/**
 * Most Recently Used (MRU) Cache.
 * Each key value pair is evicted based on being the most recently used, and no other
 * criteria.
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
class MruCache {
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
    explicit MruCache(
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
     * There is a simple struct provided on MruCache::KeyValue that can be put
     * into any iterarable container to satisfy this requirement.
     * @tparam RangeType A container with two items, KeyType, ValueType.
     * @param key_value_range The elements to insert or update into the cache.
     */
    template <typename RangeType>
    auto InsertRange(
        RangeType&& key_value_range) -> void;

    /**
     * Attempts to delete the given key.
     * @param key The key to delete from the mru cache.
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
     * @param peek  Should the find act like all the item was not used?
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
     * Attemps to find all the given keys values.
     *
     * The user should initialize this container with the keys to lookup with the values as all
     * empty optionals.  The keys that are found will have the optionals filled in with the
     * appropriate values from the cache.
     *
     * @tparam RangeType A container with a pair of optional items,
     *                   e.g. map<KeyType, optional<ValueType>>.
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
     * @return The maximum capacity of this cache.
     */
    auto GetCapacity() const -> size_t;

private:
    struct Element {
        /// The iterator into the keyed data structure.
        KeyedIterator m_keyed_position;
        /// The iterator into the mru data structure.
        std::list<size_t>::iterator m_mru_position;
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
    CappuccinoLock<SyncType> m_lock;

    /// The current number of elements in the cache.
    size_t m_used_size { 0 };

    /// The main store for the key value pairs and metadata for each element.
    std::vector<Element> m_elements;
    /// The keyed lookup data structure, the value is the index into 'm_elements'.
    std::unordered_map<KeyType, size_t> m_keyed_elements;
    /// The mru sorted list, the value is the index into 'm_elements'.
    std::list<size_t> m_mru_list;
    /// The current end of the mru list.
    std::list<size_t>::iterator m_mru_end;
};

} // namespace cappuccino

namespace cappuccino {

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
MruCache<KeyType, ValueType, SyncType>::MruCache(
    size_t capacity,
    float max_load_factor)
    : m_elements(capacity)
    , m_mru_list(capacity)
{
    std::iota(m_mru_list.begin(), m_mru_list.end(), 0);
    m_mru_end = m_mru_list.begin();

    m_keyed_elements.max_load_factor(max_load_factor);
    m_keyed_elements.reserve(capacity);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto MruCache<KeyType, ValueType, SyncType>::Insert(
    const KeyType& key,
    ValueType value) -> void
{
    std::lock_guard guard { m_lock };
    doInsertUpdate(key, std::move(value));
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
template <typename RangeType>
auto MruCache<KeyType, ValueType, SyncType>::InsertRange(
    RangeType&& key_value_range) -> void
{
    std::lock_guard guard { m_lock };
    for (auto& [key, value] : key_value_range) {
        doInsertUpdate(key, std::move(value));
    }
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto MruCache<KeyType, ValueType, SyncType>::Delete(
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

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
template <template <class...> typename RangeType>
auto MruCache<KeyType, ValueType, SyncType>::DeleteRange(
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
};

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto MruCache<KeyType, ValueType, SyncType>::Find(
    const KeyType& key,
    bool peek) -> std::optional<ValueType>
{
    std::lock_guard guard { m_lock };
    return doFind(key, peek);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
template <typename RangeType>
auto MruCache<KeyType, ValueType, SyncType>::FindRange(
    const RangeType& key_range,
    bool peek) -> std::vector<std::pair<KeyType, std::optional<ValueType>>>
{
    std::vector<std::pair<KeyType, std::optional<ValueType>>> output;
    output.reserve(std::size(key_range));

    {
        std::lock_guard guard { m_lock };
        for (auto& key : key_range) {
            output.emplace_back(key, doFind(key, peek));
        }
    }

    return output;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
template <typename RangeType>
auto MruCache<KeyType, ValueType, SyncType>::FindRangeFill(
    RangeType& key_optional_value_range,
    bool peek) -> void
{
    std::lock_guard guard { m_lock };
    for (auto& [key, optional_value] : key_optional_value_range) {
        optional_value = doFind(key, peek);
    }
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto MruCache<KeyType, ValueType, SyncType>::GetUsedSize() const -> size_t
{
    return m_used_size;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto MruCache<KeyType, ValueType, SyncType>::GetCapacity() const -> size_t
{
    return m_elements.size();
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto MruCache<KeyType, ValueType, SyncType>::doInsertUpdate(
    const KeyType& key,
    ValueType&& value) -> void
{
    auto keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end()) {
        doUpdate(keyed_position, std::move(value));
    } else {
        doInsert(key, std::move(value));
    }
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto MruCache<KeyType, ValueType, SyncType>::doInsert(
    const KeyType& key,
    ValueType&& value) -> void
{
    if (m_used_size >= m_elements.size()) {
        doPrune();
    }

    auto element_idx = *m_mru_end;

    auto keyed_position = m_keyed_elements.emplace(key, element_idx).first;

    Element& element = m_elements[element_idx];
    element.m_value = std::move(value);
    element.m_mru_position = m_mru_end;
    element.m_keyed_position = keyed_position;

    ++m_mru_end;

    ++m_used_size;

    // Don't need to call doAccess() since this element is at the most recently used end already.
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto MruCache<KeyType, ValueType, SyncType>::doUpdate(
    MruCache::KeyedIterator keyed_position,
    ValueType&& value) -> void
{
    Element& element = m_elements[keyed_position->second];
    element.m_value = std::move(value);

    // Move to most recently used end.
    doAccess(element);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto MruCache<KeyType, ValueType, SyncType>::doDelete(
    size_t element_idx) -> void
{
    Element& element = m_elements[element_idx];

    if (element.m_mru_position != std::prev(m_mru_end)) {
        m_mru_list.splice(m_mru_end, m_mru_list, element.m_mru_position);
    }
    --m_mru_end;

    m_keyed_elements.erase(element.m_keyed_position);

    --m_used_size;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto MruCache<KeyType, ValueType, SyncType>::doFind(
    const KeyType& key,
    bool peek) -> std::optional<ValueType>
{
    auto keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end()) {
        size_t element_idx = keyed_position->second;
        Element& element = m_elements[element_idx];
        // Do not update the mru access if peeking.
        if (!peek) {
            doAccess(element);
        }
        return { element.m_value };
    }

    return {};
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto MruCache<KeyType, ValueType, SyncType>::doAccess(
    Element& element) -> void
{
    // The the accessed item at the end of th MRU list, the end is the most recently
    // used, the beginning is the least recently used.
    m_mru_list.splice(m_mru_end, m_mru_list, element.m_mru_position);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto MruCache<KeyType, ValueType, SyncType>::doPrune() -> void
{
    if (m_used_size > 0) {
        doDelete(m_mru_list.back());
    }
}

} // namespace cappuccino
