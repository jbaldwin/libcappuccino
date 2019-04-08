#pragma once

#include "cappuccino/SyncImplEnum.h"

#include <list>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace cappuccino {

template <typename KeyType, typename ValueType, SyncImplEnum SyncType = SyncImplEnum::SYNC>
class LruCache {
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

    explicit LruCache(
        size_t capacity,
        float max_load_factor = 1.0f);

    auto Insert(
        const KeyType& key,
        ValueType value) -> void;

    template <typename RangeType>
    auto InsertRange(RangeType&& key_value_range) -> void;

    auto Delete(
        const KeyType& key) -> bool;

    template <template <class...> typename RangeType>
    auto DeleteRange(
        const RangeType<KeyType>& key_range) -> size_t;

    auto Find(
        const KeyType& key) -> std::optional<ValueType>;

    template <typename RangeType>
    auto FindRange(
        const RangeType& keys) -> std::unordered_map<KeyType, std::optional<ValueType>>;

    template <typename RangeType>
    auto FindRangeFill(
        RangeType&& key_optional_value_range) -> void;

    auto GetUsedSize() const -> size_t;

    auto GetCapacity() const -> size_t;

private:
    struct Element {
        KeyedIterator m_keyed_position;
        std::list<size_t>::iterator m_lru_position;
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
        const KeyType& key) -> std::optional<ValueType>;

    auto doAccess(
        Element& element) -> void;

    auto doPrune() -> void;

    std::mutex m_lock;

    size_t m_used_size { 0 };

    std::vector<Element> m_elements;
    std::unordered_map<KeyType, size_t> m_keyed_elements;
    std::list<size_t> m_lru_list;

    std::list<size_t>::iterator m_lru_end;
};

} // namespace cappuccino

#include "cappuccino/LruCache.tcc"
