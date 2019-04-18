#include "RrCache.h"
#include "cappuccino/RrCache.h"

#include <numeric>

namespace cappuccino {

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
RrCache<KeyType, ValueType, SyncType>::RrCache(
    size_t capacity,
    float max_load_factor)
    : m_elements(capacity)
    , m_keyed_elements(capacity)
    , m_open_list(capacity)
    , m_random_device()
    , m_mt(m_random_device())
{
    std::iota(m_open_list.begin(), m_open_list.end(), 0);

    m_keyed_elements.max_load_factor(max_load_factor);
    m_keyed_elements.reserve(capacity);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto RrCache<KeyType, ValueType, SyncType>::Insert(
    const KeyType& key,
    ValueType value) -> void
{
    LockScopeGuard<SyncType> guard { m_lock };
    doInsertUpdate(key, std::move(value));
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
template <typename RangeType>
auto RrCache<KeyType, ValueType, SyncType>::InsertRange(
    RangeType&& key_value_range) -> void
{
    LockScopeGuard<SyncType> guard { m_lock };
    for (auto& [key, value] : key_value_range) {
        doInsertUpdate(key, std::move(value));
    }
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto RrCache<KeyType, ValueType, SyncType>::Delete(
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
auto RrCache<KeyType, ValueType, SyncType>::DeleteRange(
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
auto RrCache<KeyType, ValueType, SyncType>::Find(
    const KeyType& key) -> std::optional<ValueType>
{
    LockScopeGuard<SyncType> guard { m_lock };
    return doFind(key);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
template <typename RangeType>
auto RrCache<KeyType, ValueType, SyncType>::FindRange(
    const RangeType& key_range) -> std::unordered_map<KeyType, std::optional<ValueType>>
{
    std::unordered_map<KeyType, std::optional<ValueType>> output;
    output.reserve(std::size(key_range));

    {
        LockScopeGuard<SyncType> guard { m_lock };
        for (auto& key : key_range) {
            output[key] = doFind(key);
        }
    }

    return output;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
template <typename RangeType>
auto RrCache<KeyType, ValueType, SyncType>::FindRangeFill(
    RangeType& key_optional_value_range) -> void
{
    LockScopeGuard<SyncType> guard { m_lock };
    for (auto& [key, optional_value] : key_optional_value_range) {
        optional_value = doFind(key);
    }
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto RrCache<KeyType, ValueType, SyncType>::GetUsedSize() const -> size_t
{
    return m_open_list_end;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto RrCache<KeyType, ValueType, SyncType>::GetCapacity() const -> size_t
{
    return m_elements.size();
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto RrCache<KeyType, ValueType, SyncType>::doInsertUpdate(
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
auto RrCache<KeyType, ValueType, SyncType>::doInsert(
    const KeyType& key,
    ValueType&& value) -> void
{
    if (m_open_list_end >= m_elements.size()) {
        doPrune();
    }

    auto element_idx = m_open_list[m_open_list_end];

    auto keyed_position = m_keyed_elements.emplace(key, element_idx).first;

    Element& element = m_elements[element_idx];
    element.m_value = std::move(value);
    element.m_open_list_position = m_open_list_end;
    element.m_keyed_position = keyed_position;

    ++m_open_list_end;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto RrCache<KeyType, ValueType, SyncType>::doUpdate(
    RrCache::KeyedIterator keyed_position,
    ValueType&& value) -> void
{
    Element& element = m_elements[keyed_position->second];
    element.m_value = std::move(value);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto RrCache<KeyType, ValueType, SyncType>::doDelete(
    size_t element_idx) -> void
{
    Element& element = m_elements[element_idx];

    // If this isn't the last item already (which is likely), swap it into that position.
    if (element.m_open_list_position != m_open_list_end - 1) {
        // Since this algo does random replacement eviction, the order of
        // the open list doesn't really matter.  This is different from an LRU
        // where the ordering *does* matter.  Since ordering doesn't matter
        // just swap the indexes in the open list to the partition point 'end'
        // and then delete that item.
        std::swap(element.m_open_list_position, m_open_list_end);
    }
    --m_open_list_end; // delete the last item

    m_keyed_elements.erase(element.m_keyed_position);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto RrCache<KeyType, ValueType, SyncType>::doFind(
    const KeyType& key) -> std::optional<ValueType>
{
    auto keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end()) {
        size_t element_idx = keyed_position->second;
        Element& element = m_elements[element_idx];
        return { element.m_value };
    }

    return {};
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto RrCache<KeyType, ValueType, SyncType>::doPrune() -> void
{
    if (m_open_list_end > 0) {
        std::uniform_int_distribution<size_t> dist { 0, m_open_list_end - 1 };
        size_t delete_idx = dist(m_mt);
        doDelete(delete_idx);
    }
}

} // namespace cappuccino
