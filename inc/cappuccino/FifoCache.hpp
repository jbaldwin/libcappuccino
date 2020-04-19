#pragma once

#include "cappuccino/CappuccinoLock.hpp"
#include "cappuccino/SyncImplEnum.hpp"
#include "cappuccino/InsertMethodEnum.hpp"

#include <list>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace cappuccino {

/**
 * First In First Out (FIFO) Cache.
 * Each key value pair is evicted based on being the first in first out policy, and no other
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
class FifoCache {
private:
    struct Element;

    using FifoIterator = typename std::list<Element>::iterator;
    using KeyedIterator = typename std::unordered_map<KeyType, FifoIterator>::iterator;

public:
    /**
     * @param capacity The maximum number of key value pairs allowed in the cache.
     * @param max_load_factor The load factor for the hash map, generally 1 is a good default.
     */
    explicit FifoCache(
        size_t capacity,
        float max_load_factor = 1.0f);

    /**
     * Inserts or updates the given key value pair.
     * @param key The key to store the value under.
     * @param value The value of the data to store.
     * @param allowed_methods Allowed methods of insertion | update.  Defaults to allowing
     *                        insertions and updates.
     * @return True if the operation was successful based on the `allowed_methods`.
     */
    auto Insert(
        const KeyType& key,
        ValueType value,
        InsertMethodEnum allowed_methods = InsertMethodEnum::INSERT_OR_UPDATE) -> bool;

    /**
     * Inserts or updates a range of key value pairs.  This expects a container
     * that has 2 values in the {KeyType, ValueType} ordering.
     * There is a simple struct provided on the LfuCache::KeyValue that can be put
     * into any iterable container to satisfy this requirement.
     * @tparam RangeType A container with two items, KeyType, ValueType.
     * @param key_value_range The elements to insert or update into the cache.
     * @param allowed_methods Allowed methods of insertion | update.  Defaults to allowing
     *                        insertions and updates.
     * @return The number of elements inserted based on the `allowed_methods`.
     */
    template <typename RangeType>
    auto InsertRange(
        RangeType&& key_value_range,
        InsertMethodEnum allowed_methods = InsertMethodEnum::INSERT_OR_UPDATE) -> size_t;

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
     * @return An optional with the key's value if it exists, or an empty optional if it does not.
     */
    auto Find(
        const KeyType& key) -> std::optional<ValueType>;

    /**
     * Attempts to find all the given keys values.
     * @tparam RangeType A container with the set of keys to find their values, e.g. vector<KeyType>.
     * @param key_range The keys to lookup their pairs.
     * @return The full set of keys to std::nullopt if the key wasn't found, or the value if found.
     */
    template <typename RangeType>
    auto FindRange(
        const RangeType& key_range) -> std::vector<std::pair<KeyType, std::optional<ValueType>>>;

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
        RangeType& key_optional_value_range) -> void;

    /**
     * @return If this cache is currenty empty.
     */
    auto empty() const -> bool { return (m_used_size == 0); }

    /**
     * @return The number of elements in the cache, this does not prune expired elements.
     */
    auto size() const -> size_t { return m_used_size; }

    /**
     * @return The maximum capacity of this cache.
     */
    auto capacity() const -> size_t { return m_fifo_list.size(); }

private:
    struct Element {
        /// The iterator into the keyed data structure, when the cache starts
        /// none of the keyed iterators are set, but also deletes can unset this optional.
        std::optional<KeyedIterator> m_keyed_position;
        /// The element's value.
        ValueType m_value;
    };

    auto doInsertUpdate(
        const KeyType& key,
        ValueType&& value,
        InsertMethodEnum allowed_methods) -> bool;

    auto doInsert(
        const KeyType& key,
        ValueType&& value) -> void;

    auto doUpdate(
        KeyedIterator keyed_position,
        ValueType&& value) -> void;

    auto doDelete(
        FifoIterator fifo_position) -> void;

    auto doFind(
        const KeyType& key) -> std::optional<ValueType>;

    /// Cache lock for all mutations if sync is enabled.
    CappuccinoLock<SyncType> m_lock;

    /// The current number of elements in the cache.
    size_t m_used_size { 0 };

    /// The ordering of first in-first out queue of elements.
    std::list<Element> m_fifo_list;
    /// The keyed lookup data structure, the value is the index into 'm_elements'.
    std::unordered_map<KeyType, FifoIterator> m_keyed_elements;
};

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
FifoCache<KeyType, ValueType, SyncType>::FifoCache(
    size_t capacity,
    float max_load_factor)
    : m_fifo_list(capacity)
{
    m_keyed_elements.max_load_factor(max_load_factor);
    m_keyed_elements.reserve(capacity);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto FifoCache<KeyType, ValueType, SyncType>::Insert(
    const KeyType& key,
    ValueType value,
    InsertMethodEnum allowed_methods) -> bool
{
    std::lock_guard guard { m_lock };
    return doInsertUpdate(key, std::move(value), allowed_methods);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
template <typename RangeType>
auto FifoCache<KeyType, ValueType, SyncType>::InsertRange(
    RangeType&& key_value_range,
    InsertMethodEnum allowed_methods) -> size_t
{
    size_t inserted { 0 };

    {
        std::lock_guard guard { m_lock };
        for (auto& [key, value] : key_value_range) {
            if(doInsertUpdate(key, std::move(value), allowed_methods)) {
                ++inserted;
            }
        }
    }

    return inserted;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto FifoCache<KeyType, ValueType, SyncType>::Delete(
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
auto FifoCache<KeyType, ValueType, SyncType>::DeleteRange(
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
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto FifoCache<KeyType, ValueType, SyncType>::Find(
    const KeyType& key) -> std::optional<ValueType>
{
    std::lock_guard guard { m_lock };
    return doFind(key);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
template <typename RangeType>
auto FifoCache<KeyType, ValueType, SyncType>::FindRange(
    const RangeType& key_range) -> std::vector<std::pair<KeyType, std::optional<ValueType>>>
{
    std::vector<std::pair<KeyType, std::optional<ValueType>>> output;
    output.reserve(std::size(key_range));

    {
        std::lock_guard guard { m_lock };
        for (auto& key : key_range) {
            output.emplace_back(key, doFind(key));
        }
    }

    return output;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
template <typename RangeType>
auto FifoCache<KeyType, ValueType, SyncType>::FindRangeFill(
    RangeType& key_optional_value_range) -> void
{
    std::lock_guard guard { m_lock };
    ;
    for (auto& [key, optional_value] : key_optional_value_range) {
        optional_value = doFind(key);
    }
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto FifoCache<KeyType, ValueType, SyncType>::doInsertUpdate(
    const KeyType& key,
    ValueType&& value,
    InsertMethodEnum allowed_methods) -> bool
{
    auto keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end()) {
        if(update_allowed(allowed_methods)) {
            doUpdate(keyed_position, std::move(value));
            return true;
        }
    } else {
        if(insert_allowed(allowed_methods)) {
            doInsert(key, std::move(value));
            return true;
        }
    }

    return false;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto FifoCache<KeyType, ValueType, SyncType>::doInsert(
    const KeyType& key,
    ValueType&& value) -> void
{
    // Take the head item and replace it at the tail of the fifo list.
    m_fifo_list.splice(m_fifo_list.end(), m_fifo_list, m_fifo_list.begin());

    FifoIterator last_element_position = std::prev(m_fifo_list.end());
    Element& element = *last_element_position;

    // since we 'deleted' the head item, if it has a value in the keyed data
    // structure it needs to be deleted first.
    if (element.m_keyed_position.has_value()) {
        m_keyed_elements.erase(element.m_keyed_position.value());
    }
    else {
        // If the cache isn't 'full' increment its size.
        ++m_used_size;
    }

    element.m_value = std::move(value);
    element.m_keyed_position = m_keyed_elements.emplace(key, last_element_position).first;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto FifoCache<KeyType, ValueType, SyncType>::doUpdate(
    FifoCache::KeyedIterator keyed_position,
    ValueType&& value) -> void
{
    Element& element = *keyed_position->second;
    element.m_value = std::move(value);

    // there is no access update in a fifo cache.
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto FifoCache<KeyType, ValueType, SyncType>::doDelete(
    FifoIterator fifo_position) -> void
{
    Element& element = *fifo_position;

    // Move the item being deleted to the head for re-use, inserts
    // will pull from the head to re-use next.
    if (fifo_position != m_fifo_list.begin()) {
        m_fifo_list.splice(m_fifo_list.begin(), m_fifo_list, fifo_position);
    }

    if (element.m_keyed_position.has_value()) {
        m_keyed_elements.erase(element.m_keyed_position.value());
        element.m_keyed_position = std::nullopt;
    }

    --m_used_size;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto FifoCache<KeyType, ValueType, SyncType>::doFind(
    const KeyType& key) -> std::optional<ValueType>
{
    auto keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end()) {
        FifoIterator fifo_position = keyed_position->second;
        Element& element = *fifo_position;
        return { element.m_value };
    }

    return {};
}

} // namespace cappuccino
