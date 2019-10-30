#include "cappuccino/UtlruCache.h"

#include <numeric>

namespace cappuccino {

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
UtlruCache<KeyType, ValueType, SyncType>::UtlruCache(
    std::chrono::seconds ttl,
    size_t capacity,
    float max_load_factor)
    : m_ttl(ttl)
    , m_elements(capacity)
    , m_lru_list(capacity)
{
    std::iota(m_lru_list.begin(), m_lru_list.end(), 0);
    m_lru_end = m_lru_list.begin();

    m_keyed_elements.max_load_factor(max_load_factor);
    m_keyed_elements.reserve(capacity);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto UtlruCache<KeyType, ValueType, SyncType>::InsertOnly(
    const KeyType& key,
    ValueType value) -> bool
{
    auto now = std::chrono::steady_clock::now();

    std::lock_guard guard { m_lock };
    auto expire_time = now + m_ttl;
    return doInsertUpdate(key, std::move(value), now, expire_time, true);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto UtlruCache<KeyType, ValueType, SyncType>::Insert(
    const KeyType& key,
    ValueType value) -> void
{
    auto now = std::chrono::steady_clock::now();

    std::lock_guard guard { m_lock };
    auto expire_time = now + m_ttl;
    doInsertUpdate(key, std::move(value), now, expire_time);
};

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
template <typename RangeType>
auto UtlruCache<KeyType, ValueType, SyncType>::InsertRange(
    RangeType&& key_value_range) -> void
{
    auto now = std::chrono::steady_clock::now();
    auto expire_time = now + m_ttl;

    std::lock_guard guard { m_lock };
    for (auto& [key, value] : key_value_range) {
        doInsertUpdate(key, std::move(value), now, expire_time);
    }
};

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto UtlruCache<KeyType, ValueType, SyncType>::Delete(
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
};

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
template <template <class...> typename RangeType>
auto UtlruCache<KeyType, ValueType, SyncType>::DeleteRange(
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
auto UtlruCache<KeyType, ValueType, SyncType>::Find(
    const KeyType& key,
    bool peek) -> std::optional<ValueType>
{
    auto now = std::chrono::steady_clock::now();

    std::lock_guard guard { m_lock };
    return doFind(key, now, peek);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
template <typename RangeType>
auto UtlruCache<KeyType, ValueType, SyncType>::FindRange(
    const RangeType& key_range,
    bool peek) -> std::vector<std::pair<KeyType, std::optional<ValueType>>>
{
    std::vector<std::pair<KeyType, std::optional<ValueType>>> output;
    output.reserve(std::size(key_range));

    auto now = std::chrono::steady_clock::now();

    {
        std::lock_guard guard { m_lock };
        for (auto& key : key_range) {
            output.emplace_back(key, doFind(key, now, peek));
        }
    }

    return output;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
template <typename RangeType>
auto UtlruCache<KeyType, ValueType, SyncType>::FindRangeFill(
    RangeType& key_optional_value_range,
    bool peek) -> void
{
    auto now = std::chrono::steady_clock::now();

    std::lock_guard guard { m_lock };

    for (auto& [key, optional_value] : key_optional_value_range) {
        optional_value = doFind(key, now, peek);
    }
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto UtlruCache<KeyType, ValueType, SyncType>::UpdateTtl(
    std::chrono::seconds ttl) -> void
{
    m_ttl = ttl;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto UtlruCache<KeyType, ValueType, SyncType>::GetUsedSize() const -> size_t
{
    return m_used_size;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto UtlruCache<KeyType, ValueType, SyncType>::GetCapacity() const -> size_t
{
    return m_elements.size();
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto UtlruCache<KeyType, ValueType, SyncType>::CleanExpiredValues() -> size_t
{
    // how to determine when to stop cleaning since ttl list doesn't empty?

    size_t deleted_elements { 0 };
    auto now = std::chrono::steady_clock::now();

    {
        std::lock_guard guard { m_lock };

        while (m_used_size > 0) {
            Element& element = m_elements[m_ttl_list.begin()];
            if (now >= element.m_expire_time) {
                ++deleted_elements;
                doDelete(m_ttl_list.begin());
            } else {
                break;
            }
        }
    }

    return deleted_elements;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto UtlruCache<KeyType, ValueType, SyncType>::doInsertUpdate(
    const KeyType& key,
    ValueType&& value,
    std::chrono::steady_clock::time_point now,
    std::chrono::steady_clock::time_point expire_time,
    bool insert_only) -> bool
{
    auto keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end()) {
        // If the key already exists this is an update, this won't require a prune.

        const size_t element_idx = keyed_position->second;
        const bool expired = (now >= m_elements[element_idx].m_expire_time);

        if (!insert_only || expired)
        {
            Element& element = doUpdate(keyed_position, expire_time);
            element.m_value = std::move(value);
            return true;
        }

        return false;
    } else {
        if (m_used_size >= m_elements.size()) {
            doPrune(now);
        }
        doInsert(key, std::move(value), expire_time);
        return true;
    }
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto UtlruCache<KeyType, ValueType, SyncType>::doInsert(
    const KeyType& key,
    ValueType&& value,
    std::chrono::steady_clock::time_point expire_time) -> void
{
    auto element_idx = *m_lru_end;

    auto keyed_position = m_keyed_elements.emplace(key, element_idx).first;

    m_ttl_list.emplace_back(element_idx);

    Element& element = m_elements[element_idx];
    element.m_value = std::move(value);
    element.m_expire_time = expire_time;
    element.m_lru_position = m_lru_end;
    element.m_ttl_position = std::prev(m_ttl_list.end());
    element.m_keyed_position = keyed_position;

    ++m_lru_end;

    ++m_used_size;

    doAccess(element);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto UtlruCache<KeyType, ValueType, SyncType>::doUpdate(
    KeyedIterator keyed_position,
    std::chrono::steady_clock::time_point expire_time) -> Element&
{
    size_t element_idx = keyed_position->second;

    Element& element = m_elements[element_idx];
    element.m_expire_time = expire_time;

    // push to the end of the ttl list
    m_ttl_list.splice(m_ttl_list.end(), m_ttl_list, element.m_ttl_position);

    doAccess(element);
    return element;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto UtlruCache<KeyType, ValueType, SyncType>::doDelete(
    size_t element_idx) -> void
{
    Element& element = m_elements[element_idx];

    if (element.m_lru_position != std::prev(m_lru_end)) {
        m_lru_list.splice(m_lru_end, m_lru_list, element.m_lru_position);
    }
    --m_lru_end;

    m_ttl_list.erase(element.m_ttl_position);

    m_keyed_elements.erase(element.m_keyed_position);

    --m_used_size;
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto UtlruCache<KeyType, ValueType, SyncType>::doFind(
    const KeyType& key,
    std::chrono::steady_clock::time_point now,
    bool peek) -> std::optional<ValueType>
{
    auto keyed_position = m_keyed_elements.find(key);
    if (keyed_position != m_keyed_elements.end()) {
        size_t element_idx = keyed_position->second;
        Element& element = m_elements[element_idx];

        // Has the element TTL'ed?
        if (now < element.m_expire_time) {
            // Do not update items access if peeking.
            if (!peek) {
                doAccess(element);
            }
            return { element.m_value };
        } else {
            doDelete(element_idx);
        }
    }

    return {};
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto UtlruCache<KeyType, ValueType, SyncType>::doAccess(
    Element& element) -> void
{
    m_lru_list.splice(m_lru_list.begin(), m_lru_list, element.m_lru_position);
}

template <typename KeyType, typename ValueType, SyncImplEnum SyncType>
auto UtlruCache<KeyType, ValueType, SyncType>::doPrune(
    std::chrono::steady_clock::time_point now) -> void
{
    if (m_used_size > 0) {
        size_t ttl_idx = *m_ttl_list.begin();
        Element& element = m_elements[ttl_idx];

        if (now >= element.m_expire_time) {
            doDelete(ttl_idx);
        } else {
            size_t lru_idx = m_lru_list.back();
            doDelete(lru_idx);
        }
    }
}

} // namespace cappuccino
