#include "cappuccino/FifoCache.h"
#include "cappuccino/LockScopeGuard.h"

namespace cappuccino {

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
    ValueType value) -> void
{
    LockScopeGuard<SyncType> guard { m_lock };
    doInsertUpdate(key, std::move(value));
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
template <typename RangeType>
auto FifoCache<KeyType, ValueType, SyncType>::InsertRange(
    RangeType&& key_value_range) -> void
{
    LockScopeGuard<SyncType> guard { m_lock };
    for (auto& [key, value] : key_value_range) {
        doInsertUpdate(key, std::move(value));
    }
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto FifoCache<KeyType, ValueType, SyncType>::Delete(
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
auto FifoCache<KeyType, ValueType, SyncType>::DeleteRange(
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
auto FifoCache<KeyType, ValueType, SyncType>::Find(
    const KeyType& key) -> std::optional<ValueType>
{
    LockScopeGuard<SyncType> guard { m_lock };
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
        LockScopeGuard<SyncType> guard { m_lock };
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
    LockScopeGuard<SyncType> guard { m_lock };
    for (auto& [key, optional_value] : key_optional_value_range) {
        optional_value = doFind(key);
    }
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto FifoCache<KeyType, ValueType, SyncType>::GetUsedSize() const -> size_t
{
    m_used_size;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto FifoCache<KeyType, ValueType, SyncType>::GetCapacity() const -> size_t
{
    //    m_elements.size();
    m_fifo_list.size();
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto FifoCache<KeyType, ValueType, SyncType>::doInsertUpdate(
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

    element.m_value = std::move(value);
    element.m_keyed_position = m_keyed_elements.emplace(key, last_element_position).first;
    ;

    ++m_used_size;
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
