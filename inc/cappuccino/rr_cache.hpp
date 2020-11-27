#pragma once

#include "cappuccino/allow.hpp"
#include "cappuccino/lock.hpp"

#include <random>
#include <unordered_map>
#include <vector>

namespace cappuccino
{
/**
 * Random Replacement (RR) Cache.
 * Each key value pair is evicted based upon a random eviction strategy.
 *
 * This cache is sync aware and can be used concurrently from multiple threads safely.
 * To remove locks/synchronization used NO when creating the cache.
 *
 * @tparam key_type The key type.  Must support std::hash().
 * @tparam value_type The value type.  This is returned by copy on a find, so if your data
 *                   structure value is large it is advisable to store in a shared ptr.
 * @tparam sync_type By default this cache is thread safe, can be disabled for caches
 *                  specific to a single thread.
 */
template<typename key_type, typename value_type, sync sync_type = sync::yes>
class rr_cache
{
private:
    using keyed_iterator = typename std::unordered_map<key_type, size_t>::iterator;

public:
    /**
     * @param capacity The maximum number of key value pairs allowed in the cache.
     * @param max_load_factor The load factor for the hash map, generally 1 is a good default.
     */
    explicit rr_cache(size_t capacity, float max_load_factor = 1.0f)
        : m_elements(capacity),
          m_open_list(capacity),
          m_random_device(),
          m_mt(m_random_device())
    {
        std::iota(m_open_list.begin(), m_open_list.end(), 0);

        m_keyed_elements.max_load_factor(max_load_factor);
        m_keyed_elements.reserve(capacity);
    }

    /**
     * Inserts or updates the given key value pair.
     * @param key The key to store the value under.
     * @param value The value of the data to store.
     * @param a Allowed methods of insertion | update.  Defaults to allowing
     *              insertions and updates.
     * @return True if the operation was successful based on `allow`.
     */
    auto insert(const key_type& key, value_type value, allow a = allow::insert_or_update) -> bool
    {
        std::lock_guard guard{m_lock};
        return do_insert_update(key, std::move(value), a);
    }

    /**
     * Inserts or updates a range of key value pairs.  This expects a container
     * that has 2 values in the {key_type, value_type} ordering.
     * There is a simple struct provided on the LruCache::KeyValue that can be put
     * into any iterable container to satisfy this requirement.
     * @tparam range_type A container with two items, key_type, value_type.
     * @param key_value_range The elements to insert or update into the cache.
     * @param a Allowed methods of insertion | update.  Defaults to allowing
     *              insertions and updates.
     * @return The number of elements inserted based on `allow`.
     */
    template<typename range_type>
    auto insert_range(range_type&& key_value_range, allow a = allow::insert_or_update) -> size_t
    {
        size_t inserted{0};

        {
            std::lock_guard guard{m_lock};
            for (auto& [key, value] : key_value_range)
            {
                if (do_insert_update(key, std::move(value), a))
                {
                    ++inserted;
                }
            }
        }

        return inserted;
    }

    /**
     * Attempts to delete the given key.
     * @param key The key to delete from the lru cache.
     * @return True if the key was deleted, false if the key does not exist.
     */
    auto erase(const key_type& key) -> bool
    {
        std::lock_guard guard{m_lock};
        auto            keyed_position = m_keyed_elements.find(key);
        if (keyed_position != m_keyed_elements.end())
        {
            do_erase(keyed_position->second);
            return true;
        }
        else
        {
            return false;
        }
    }

    /**
     * Attempts to delete all given keys.
     * @tparam range_type A container with the set of keys to delete, e.g. vector<key_type>, set<key_type>.
     * @param key_range The keys to delete from the cache.
     * @return The number of items deleted from the cache.
     */
    template<template<class...> typename range_type>
    auto erase_range(const range_type<key_type>& key_range) -> size_t
    {
        size_t deleted_elements{0};

        std::lock_guard guard{m_lock};
        for (auto& key : key_range)
        {
            auto keyed_position = m_keyed_elements.find(key);
            if (keyed_position != m_keyed_elements.end())
            {
                ++deleted_elements;
                do_erase(keyed_position->second);
            }
        }

        return deleted_elements;
    }

    /**
     * Attempts to find the given key's value.
     * @param key The key to lookup its value.
     * @return An optional with the key's value if it exists, or an empty optional if it does not.
     */
    auto find(const key_type& key) -> std::optional<value_type>
    {
        std::lock_guard guard{m_lock};
        return do_find(key);
    }

    /**
     * Attempts to find all the given keys values.
     * @tparam range_type A container with the set of keys to find their values, e.g. vector<key_type>.
     * @param key_range The keys to lookup their pairs.
     * @return The full set of keys to std::nullopt if the key wasn't found, or the value if found.
     */
    template<template<class...> typename range_type>
    auto find_range(const range_type<key_type>& key_range)
        -> std::vector<std::pair<key_type, std::optional<value_type>>>
    {
        std::vector<std::pair<key_type, std::optional<value_type>>> output;
        output.reserve(std::size(key_range));

        {
            std::lock_guard guard{m_lock};
            for (auto& key : key_range)
            {
                output.emplace_back(key, do_find(key));
            }
        }

        return output;
    }

    /**
     * Attempts to find all the given keys values.
     *
     * The user should initialize this container with the keys to lookup with the values as all
     * empty optionals.  The keys that are found will have the optionals filled in with the
     * appropriate values from the cache.
     *
     * @tparam range_type A container with a pair of optional items,
     *                   e.g. vector<pair<key_type, optional<value_type>>>
     *                   or map<key_type, optional<value_type>>
     * @param key_optional_value_range The keys to optional values to fill out.
     */
    template<typename range_type>
    auto find_range_fill(range_type& key_optional_value_range) -> void
    {
        std::lock_guard guard{m_lock};
        for (auto& [key, optional_value] : key_optional_value_range)
        {
            optional_value = do_find(key);
        }
    }

    /**
     * @return If this cache is currenty empty.
     */
    auto empty() const -> bool { return (m_open_list_end == 0); }

    /**
     * @return The number of elements inside the cache.
     */
    auto size() const -> size_t { return m_open_list_end; }

    /**
     * @return The maximum capacity of this cache.
     */
    auto capacity() const -> size_t { return m_elements.size(); }

private:
    struct element
    {
        keyed_iterator m_keyed_position;
        size_t         m_open_list_position;
        value_type     m_value;
    };

    auto do_insert_update(const key_type& key, value_type&& value, allow a) -> bool
    {
        auto keyed_position = m_keyed_elements.find(key);
        if (keyed_position != m_keyed_elements.end())
        {
            if (update_allowed(a))
            {
                do_update(keyed_position, std::move(value));
                return true;
            }
        }
        else
        {
            if (insert_allowed(a))
            {
                do_insert(key, std::move(value));
                return true;
            }
        }
        return false;
    }

    auto do_insert(const key_type& key, value_type&& value) -> void
    {
        if (m_open_list_end >= m_elements.size())
        {
            do_prune();
        }

        auto element_idx = m_open_list[m_open_list_end];

        auto keyed_position = m_keyed_elements.emplace(key, element_idx).first;

        element& e             = m_elements[element_idx];
        e.m_value              = std::move(value);
        e.m_open_list_position = m_open_list_end;
        e.m_keyed_position     = keyed_position;

        ++m_open_list_end;
    }

    auto do_update(keyed_iterator keyed_position, value_type&& value) -> void
    {
        element& e = m_elements[keyed_position->second];
        e.m_value  = std::move(value);
    }

    auto do_erase(size_t element_idx) -> void
    {
        element& e = m_elements[element_idx];

        // If this isn't the last item already (which is likely), swap it into that position.
        if (e.m_open_list_position != m_open_list_end - 1)
        {
            // Since this algo does random replacement eviction, the order of
            // the open list doesn't really matter.  This is different from an LRU
            // where the ordering *does* matter.  Since ordering doesn't matter
            // just swap the indexes in the open list to the partition point 'end'
            // and then delete that item.
            std::swap(m_open_list[e.m_open_list_position], m_open_list[m_open_list_end - 1]);
        }
        --m_open_list_end; // delete the last item

        m_keyed_elements.erase(e.m_keyed_position);
    }

    auto do_find(const key_type& key) -> std::optional<value_type>
    {
        auto keyed_position = m_keyed_elements.find(key);
        if (keyed_position != m_keyed_elements.end())
        {
            size_t   element_idx = keyed_position->second;
            element& e           = m_elements[element_idx];
            return {e.m_value};
        }

        return {};
    }

    auto do_prune() -> void
    {
        if (m_open_list_end > 0)
        {
            std::uniform_int_distribution<size_t> dist{0, m_open_list_end - 1};
            size_t                                delete_idx = dist(m_mt);
            do_erase(delete_idx);
        }
    }

    /// Cache lock for all mutations if sync is enabled.
    mutex<sync_type> m_lock;

    /// The main store for the key value pairs and metadata for each e.
    std::vector<element> m_elements;
    /// The keyed lookup data structure, this value is the index into 'm_elements'.
    std::unordered_map<key_type, size_t> m_keyed_elements;
    /// The open list of free elements to use, the value is the index into 'm_elements'.
    std::vector<size_t> m_open_list;
    /// This is the partition point in the m_open_list, it is also the number of items in the cache.
    size_t m_open_list_end{0};

    /// Random device to seed mt19937.
    std::random_device m_random_device;
    /// Random number generator for eviction policy.
    std::mt19937 m_mt;
};

} // namespace cappuccino
