#include "cappuccino/LfudaCache.h"

#include <numeric>

namespace cappuccino {

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
LfudaCache<KeyType, ValueType, SyncType>::LfudaCache(
    size_t capacity,
    std::chrono::seconds dynamic_age_tick,
    float dynamic_age_ratio,
    float max_load_factor)
    : m_dynamic_age_tick(dynamic_age_tick)
    , m_dynamic_age_ratio(dynamic_age_ratio)
    , m_elements(capacity)
    , m_dynamic_age_list(capacity)
{
    // Fill out the DA list / open list with indexes into 'm_elements'.
    auto now = std::chrono::steady_clock::now();
    size_t idx { 0 };
    for (auto& age_item : m_dynamic_age_list) {
        age_item.first = idx;
        age_item.second = now;
        ++idx;
    }
    m_open_list_end = m_dynamic_age_list.begin();

    m_keyed_elements.max_load_factor(max_load_factor);
    m_keyed_elements.reserve(capacity);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto LfudaCache<KeyType, ValueType, SyncType>::Insert(
    const KeyType& key,
    ValueType value) -> void
{
    auto now = std::chrono::steady_clock::now();

    LockScopeGuard<SyncType> guard { m_lock };
    doInsertUpdate(key, std::move(value), now);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
template <typename RangeType>
auto LfudaCache<KeyType, ValueType, SyncType>::InsertRange(
    RangeType&& key_value_range) -> void
{
    auto now = std::chrono::steady_clock::now();

    LockScopeGuard<SyncType> guard { m_lock };
    for (auto& [key, value] : key_value_range) {
        doInsertUpdate(key, std::move(value), now);
    }
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto LfudaCache<KeyType, ValueType, SyncType>::Delete(
    const KeyType& key) -> bool
{
    LockScopeGuard<SyncType> guard { m_lock };
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
auto LfudaCache<KeyType, ValueType, SyncType>::DeleteRange(
    const RangeType<KeyType>& key_range) -> size_t
{
    size_t deleted_elements { 0 };

    LockScopeGuard<SyncType> guard { m_lock };
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
auto LfudaCache<KeyType, ValueType, SyncType>::Find(
    const KeyType& key,
    bool peek) -> std::optional<ValueType>
{
    auto now = std::chrono::steady_clock::now();

    LockScopeGuard<SyncType> guard { m_lock };
    return doFind(key, peek, now);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto LfudaCache<KeyType, ValueType, SyncType>::FindWithUseCount(
    const KeyType& key,
    bool peek) -> std::optional<std::pair<ValueType, size_t>>
{
    auto now = std::chrono::steady_clock::now();

    LockScopeGuard<SyncType> guard { m_lock };
    return doFindWithUseCount(key, peek, now);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
template <typename RangeType>
auto LfudaCache<KeyType, ValueType, SyncType>::FindRange(
    const RangeType& key_range,
    bool peek) -> std::vector<std::pair<KeyType, std::optional<ValueType>>>
{
    auto now = std::chrono::steady_clock::now();

    std::vector<std::pair<KeyType, std::optional<ValueType>>> output;
    output.reserve(std::size(key_range));

    {
        LockScopeGuard<SyncType> guard { m_lock };
        for (auto& key : key_range) {
            output.emplace_back(key, doFind(key, peek, now));
        }
    }

    return output;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
template <typename RangeType>
auto LfudaCache<KeyType, ValueType, SyncType>::FindRangeFill(
    RangeType& key_optional_value_range,
    bool peek) -> void
{
    auto now = std::chrono::steady_clock::now();

    LockScopeGuard<SyncType> guard { m_lock };
    for (auto& [key, optional_value] : key_optional_value_range) {
        optional_value = doFind(key, peek, now);
    }
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto LfudaCache<KeyType, ValueType, SyncType>::DynamicallyAge() -> size_t
{
    auto now = std::chrono::steady_clock::now();

    LockScopeGuard<SyncType> guard { m_lock };
    return doDynamicAge(now);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto LfudaCache<KeyType, ValueType, SyncType>::GetUsedSize() const -> size_t
{
    return m_elements.size();
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto LfudaCache<KeyType, ValueType, SyncType>::GetCapacity() const -> size_t
{
    return m_used_size;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto LfudaCache<KeyType, ValueType, SyncType>::doInsertUpdate(
    const KeyType& key,
    ValueType&& value,
    std::chrono::steady_clock::time_point now) -> void
{
    auto keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end()) {
        doUpdate(keyed_position, std::move(value), now);
    } else {
        doInsert(key, std::move(value), now);
    }
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto LfudaCache<KeyType, ValueType, SyncType>::doInsert(
    const KeyType& key,
    ValueType&& value,
    std::chrono::steady_clock::time_point now) -> void
{
    if (m_used_size >= m_elements.size()) {
        doPrune(now);
    }

    auto element_idx = m_open_list_end->first;

    auto keyed_position = m_keyed_elements.emplace(key, element_idx).first;
    auto lfu_position = m_lfu_list.emplace(1, element_idx);

    m_open_list_end->second = now;

    Element& element = m_elements[element_idx];
    element.m_value = std::move(value);
    element.m_keyed_position = keyed_position;
    element.m_lfu_position = lfu_position;
    element.m_dynamic_age_position = m_open_list_end;

    ++m_open_list_end;

    ++m_used_size;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto LfudaCache<KeyType, ValueType, SyncType>::doUpdate(
    LfudaCache::KeyedIterator keyed_position,
    ValueType&& value,
    std::chrono::steady_clock::time_point now) -> void
{
    Element& element = m_elements[keyed_position->second];
    element.m_value = std::move(value);

    doAccess(element, now);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto LfudaCache<KeyType, ValueType, SyncType>::doDelete(
    size_t element_idx) -> void
{
    Element& element = m_elements[element_idx];

    if (element.m_dynamic_age_position != std::prev(m_open_list_end)) {
        m_dynamic_age_list.splice(m_open_list_end, m_dynamic_age_list, element.m_dynamic_age_position);
    }
    --m_open_list_end;

    m_keyed_elements.erase(element.m_keyed_position);
    m_lfu_list.erase(element.m_lfu_position);

    --m_used_size;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto LfudaCache<KeyType, ValueType, SyncType>::doFind(
    const KeyType& key,
    bool peek,
    std::chrono::steady_clock::time_point now) -> std::optional<ValueType>
{
    auto keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end()) {
        size_t element_idx = keyed_position->second;
        Element& element = m_elements[element_idx];
        if (!peek) {
            doAccess(element, now);
        }
        return { element.m_value };
    }

    return {};
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto LfudaCache<KeyType, ValueType, SyncType>::doFindWithUseCount(
    const KeyType& key,
    bool peek,
    std::chrono::steady_clock::time_point now) -> std::optional<std::pair<ValueType, size_t>>
{
    auto keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end()) {
        size_t element_idx = keyed_position->second;
        Element& element = m_elements[element_idx];
        if (!peek) {
            doAccess(element, now);
        }
        return { std::make_pair(element.m_value, element.m_lfu_position->first) };
    }

    return {};
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto LfudaCache<KeyType, ValueType, SyncType>::doAccess(
    LfudaCache::Element& element,
    std::chrono::steady_clock::time_point now) -> void
{
    // Update lfu position.
    auto use_count = element.m_lfu_position->first;
    m_lfu_list.erase(element.m_lfu_position);
    element.m_lfu_position = m_lfu_list.emplace(use_count + 1, element.m_keyed_position->second);

    // Update dynamic aging position.
    auto last_aged_item = std::prev(m_open_list_end);
    // swap to the end of the aged list and update its time.
    if (element.m_dynamic_age_position != last_aged_item) {
        m_dynamic_age_list.splice(last_aged_item, m_dynamic_age_list, element.m_dynamic_age_position);
    }
    element.m_dynamic_age_position->second = now;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto LfudaCache<KeyType, ValueType, SyncType>::doPrune(
    std::chrono::steady_clock::time_point now) -> void
{
    if (m_used_size > 0) {
        doDynamicAge(now);

        // Now delete the least frequently used item after dynamically aging.
        doDelete(m_lfu_list.begin()->second);
    }
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto LfudaCache<KeyType, ValueType, SyncType>::doDynamicAge(
    std::chrono::steady_clock::time_point now) -> size_t
{
    size_t aged { 0 };

    // For all items in the DA list that are old enough and in use, DA them!
    auto da_start = m_dynamic_age_list.begin();
    auto da_last = m_open_list_end; // need the item previous to the end to splice properly
    while (da_start != m_open_list_end && da_start->second + m_dynamic_age_tick < now) {
        // swap to after the last item (if it isn't the last item..)
        if (da_start != da_last) {
            m_dynamic_age_list.splice(da_last, m_dynamic_age_list, da_start);
        }
        // Update its dynamic age time to now.
        da_start->second = now;

        // Now *= ratio its use count to actually age it.  This requires
        // deleting from and re-inserting into the lfu data structure.
        Element& element = m_elements[da_start->first];
        size_t use_count = element.m_lfu_position->first;
        m_lfu_list.erase(element.m_lfu_position);
        use_count *= m_dynamic_age_ratio;
        m_lfu_list.emplace(use_count, da_start->first);

        // The last item is now this item!  This will maintain the insertion order.
        da_last = da_start;

        // Keep pruning from the beginning until we are m_open_list_end or not aged enough.
        da_start = m_dynamic_age_list.begin();
        ++aged;
    }

    return aged;
}

} // namespace cappuccino
