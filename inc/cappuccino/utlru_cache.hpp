#pragma once

#include "cappuccino/allow.hpp"
#include "cappuccino/lock.hpp"
#include "cappuccino/peek.hpp"

#include <chrono>
#include <list>
#include <numeric>
#include <optional>
#include <unordered_map>
#include <vector>

namespace cappuccino
{
/**
 * Uniform time aware LRU (Least Recently Used) Cache.
 * Each key value pair is evicted based on a global cache TTL (time to live)
 * and least recently used policy.  Expired key value pairs are evicted before
 * least recently used.  This cache differs from TlruCache in that it is more
 * efficient in managing the TTLs since they are uniform for all key value pairs
 * in the cache, rather than tracking items individually.
 *
 * This cache is thread_safe aware and can be used concurrently from multiple threads safely.
 * To remove locks/synchronization use NO when creating the cache.
 *
 * @tparam key_type The key type.  Must support std::hash().
 * @tparam value_type The value type.  This is returned by copy on a find, so if your data
 *                   structure value is large it is advisable to store in a shared ptr.
 * @tparam thread_safe_type By default this cache is thread safe, can be disabled for caches specific
 *                  to a single thread.
 */
template<typename key_type, typename value_type, thread_safe thread_safe_type = thread_safe::yes>
class utlru_cache
{
    using keyed_iterator = typename std::unordered_map<key_type, size_t>::iterator;

public:
    /**
     * @param ttl The uniform TTL of every key value inserted into the cache.
     * @param capacity The maximum number of key value pairs allowed in the cache.
     * @param max_load_factor The load factor for the hash map, generally 1 is a good default.
     */
    utlru_cache(std::chrono::milliseconds ttl, size_t capacity, float max_load_factor = 1.0f)
        : m_ttl(ttl),
          m_elements(capacity),
          m_lru_list(capacity)
    {
        std::iota(m_lru_list.begin(), m_lru_list.end(), 0);
        m_lru_end = m_lru_list.begin();

        m_keyed_elements.max_load_factor(max_load_factor);
        m_keyed_elements.reserve(capacity);
    }

    /**
     * Inserts or updates the given key value pair.  On update will reset the TTL.
     * @param key The key to store the value under.
     * @param value The value of data to store.
     * @param a Allowed methods of insertion | update.  Defaults to allowing
     *              insertions and updates.
     * @return True if the operation was successful based on `allow`.
     */
    auto insert(const key_type& key, value_type value, allow a = allow::insert_or_update) -> bool
    {
        auto now         = std::chrono::steady_clock::now();
        auto expire_time = now + m_ttl;

        std::lock_guard guard{m_lock};
        return do_insert_update(key, std::move(value), now, expire_time, a);
    }

    /**
     * Inserts or updates a range of key value pairs.  This expects a container that has
     * 2 values in the {key_type, value_type} ordering.  There is a simple struct
     * LruCacheUniformTtl::KeyValue that can be put into any iterable container to satisfy
     * this requirement.
     * @tparam range_type A container with two items, key_type, value_type.
     * @param key_value_range The elements to insert or update into the cache.
     * @param a Allowed methods of insertion | update.  Defaults to allowing
     *              insertions and updates.
     * @return The number of elements inserted based on `allow`.
     */
    template<typename range_type>
    auto insert_range(range_type&& key_value_range, allow a = allow::insert_or_update) -> size_t
    {
        auto   now         = std::chrono::steady_clock::now();
        auto   expire_time = now + m_ttl;
        size_t inserted{0};

        {
            std::lock_guard guard{m_lock};
            for (auto& [key, value] : key_value_range)
            {
                if (do_insert_update(key, std::move(value), now, expire_time, a))
                {
                    ++inserted;
                }
            }
        }

        return inserted;
    }

    /**
     * Attempts to delete the given key.
     * @param key The key to remove from the lru cache.
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
     * @tparam range_type A container with the set of keys  to delete, e.g. vector<k> or set<k>.
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
     * Removes all elements from the cache (which are destroyed), leaving the container size 0.
     */
    auto clear() -> void
    {
        {
            std::lock_guard guard{m_lock};
            if (m_used_size > 0)
            {
                const auto capacity = m_elements.capacity();
                std::iota(m_lru_list.begin(), m_lru_list.end(), 0);
                m_lru_end = m_lru_list.begin();
                m_keyed_elements.clear();
                m_keyed_elements.reserve(capacity);
                m_ttl_list.clear();
                m_used_size = 0;
            }
        }
    }

    /**
     * Attempts to find the given key's value.
     * @param key The key to lookup its value.
     * @param peek Should the find act like all the item was not used?
     * @return An optional with the key's value if it exists, or an empty optional if it does not.
     */
    auto find(const key_type& key, peek peek = peek::no) -> std::optional<value_type>
    {
        auto now = std::chrono::steady_clock::now();

        std::lock_guard guard{m_lock};
        return do_find(key, now, peek);
    }

    /**
     * Attempts to find all the given keys values.
     * @tparam range_type A container with the set of keys to lookup, e.g. vector<key_type>.
     * @param key_range A container with the set of keys to lookup.
     * @param peek Should the find act like all the items were not used?
     * @return All input keys to either a std::nullopt if it doesn't exist, or the value if it does.
     */
    template<template<class...> typename range_type>
    auto find_range(const range_type<key_type>& key_range, peek peek = peek::no)
        -> std::vector<std::pair<key_type, std::optional<value_type>>>
    {
        std::vector<std::pair<key_type, std::optional<value_type>>> output;
        output.reserve(std::size(key_range));

        auto now = std::chrono::steady_clock::now();

        {
            std::lock_guard guard{m_lock};
            for (auto& key : key_range)
            {
                output.emplace_back(key, do_find(key, now, peek));
            }
        }

        return output;
    }

    /**
     * Attempts to find all given keys values.
     *
     * The user should initialize this container with the keys to lookup with the values all
     * empty optionals.  The keys that are found will have the optionals filled in with the
     * appropriate values from the cache.
     *
     * @tparam range_type A container with a pair of optional items,
     *                   e.g. vector<pair<k, optional<v>>> or map<k, optional<v>>.
     * @param key_optional_value_range The keys to optional values to fill out.
     * @param peek Should the find act like all the items were not used?
     */
    template<typename range_type>
    auto find_range_fill(range_type& key_optional_value_range, peek peek = peek::no) -> void
    {
        auto now = std::chrono::steady_clock::now();

        std::lock_guard guard{m_lock};

        for (auto& [key, optional_value] : key_optional_value_range)
        {
            optional_value = do_find(key, now, peek);
        }
    }

    /**
     * Updates the uniform TTL for all new items or updated items in the cache.
     * Existing items that are not touched will TTL out at the previous uniform
     * TTL time.
     * @param ttl The new uniform TTL value to apply to all new elements.
     */
    auto update_ttl(std::chrono::milliseconds ttl) -> void { m_ttl = ttl; }

    /**
     * Trims the TTL list of items an expunges all expired elements.  This could be useful to use
     * on downtime to make inserts faster if the cache is full by pruning TTL'ed elements.
     * @return The number of elements pruned.
     */
    auto clean_expired_values() -> size_t
    {
        size_t deleted_elements{0};
        auto   now = std::chrono::steady_clock::now();

        {
            std::lock_guard guard{m_lock};

            while (m_used_size > 0)
            {
                size_t   ttl_idx = *m_ttl_list.begin();
                element& e       = m_elements[ttl_idx];
                if (now >= e.m_expire_time)
                {
                    ++deleted_elements;
                    do_erase(ttl_idx);
                }
                else
                {
                    break;
                }
            }
        }

        return deleted_elements;
    }

    /**
     * @return If this cache is currenty empty.
     */
    auto empty() const -> bool { return m_used_size == 0; }

    /**
     * @return The number of elements inside the cache.
     */
    auto size() const -> size_t { return m_used_size; }

    /**
     * @return The maximum capacity of this cache.
     */
    auto capacity() const -> size_t { return m_elements.size(); }

private:
    struct element
    {
        /// The point in time in  which this element's value expires.
        std::chrono::steady_clock::time_point m_expire_time;
        /// The iterator into the keyed data structure.
        keyed_iterator m_keyed_position;
        /// The iterator into the lru data structure.
        std::list<size_t>::iterator m_lru_position;
        /// The iterator into the ttl data structure.
        std::list<size_t>::iterator m_ttl_position;
        /// The element's value.
        value_type m_value;
    };

    auto do_insert_update(
        const key_type&                       key,
        value_type&&                          value,
        std::chrono::steady_clock::time_point now,
        std::chrono::steady_clock::time_point expire_time,
        allow                                 a) -> bool
    {
        auto keyed_position = m_keyed_elements.find(key);
        if (keyed_position != m_keyed_elements.end())
        {
            if (update_allowed(a))
            {
                do_update(keyed_position, std::move(value), expire_time);
                return true;
            }
            else if (insert_allowed(a))
            {
                // If the item has expired and this is INSERT then allow the
                // insert to proceed, this can just be an update in place.
                const auto& [key, element_idx] = *keyed_position;
                element& e                     = m_elements[element_idx];
                if (now >= e.m_expire_time)
                {
                    do_update(keyed_position, std::move(value), expire_time);
                    return true;
                }
            }
        }
        else
        {
            if (insert_allowed(a))
            {
                do_insert(key, std::move(value), now, expire_time);
                return true;
            }
        }
        return false;
    }

    auto do_insert(
        const key_type&                       key,
        value_type&&                          value,
        std::chrono::steady_clock::time_point now,
        std::chrono::steady_clock::time_point expire_time) -> void
    {
        if (m_used_size >= m_elements.size())
        {
            do_prune(now);
        }
        auto element_idx = *m_lru_end;

        auto keyed_position = m_keyed_elements.emplace(key, element_idx).first;

        m_ttl_list.emplace_back(element_idx);

        element& e         = m_elements[element_idx];
        e.m_value          = std::move(value);
        e.m_expire_time    = expire_time;
        e.m_lru_position   = m_lru_end;
        e.m_ttl_position   = std::prev(m_ttl_list.end());
        e.m_keyed_position = keyed_position;

        ++m_lru_end;

        ++m_used_size;

        do_access(e);
    }

    auto do_update(keyed_iterator keyed_position, value_type&& value, std::chrono::steady_clock::time_point expire_time)
        -> void
    {
        size_t element_idx = keyed_position->second;

        element& e      = m_elements[element_idx];
        e.m_expire_time = expire_time;
        e.m_value       = std::move(value);

        // push to the end of the ttl list
        m_ttl_list.splice(m_ttl_list.end(), m_ttl_list, e.m_ttl_position);

        do_access(e);
    }

    auto do_erase(size_t element_idx) -> void
    {
        element& e = m_elements[element_idx];

        if (e.m_lru_position != std::prev(m_lru_end))
        {
            m_lru_list.splice(m_lru_end, m_lru_list, e.m_lru_position);
        }
        --m_lru_end;

        m_ttl_list.erase(e.m_ttl_position);

        m_keyed_elements.erase(e.m_keyed_position);

        --m_used_size;
    }

    auto do_find(const key_type& key, std::chrono::steady_clock::time_point now, peek peek) -> std::optional<value_type>
    {
        auto keyed_position = m_keyed_elements.find(key);
        if (keyed_position != m_keyed_elements.end())
        {
            size_t   element_idx = keyed_position->second;
            element& e           = m_elements[element_idx];

            // Has the element TTL'ed?
            if (now < e.m_expire_time)
            {
                // Do not update items access if peeking.
                if (peek == peek::no)
                {
                    do_access(e);
                }
                return {e.m_value};
            }
            else
            {
                do_erase(element_idx);
            }
        }

        return {};
    }

    auto do_access(element& e) -> void { m_lru_list.splice(m_lru_list.begin(), m_lru_list, e.m_lru_position); }

    auto do_prune(std::chrono::steady_clock::time_point now) -> void
    {
        if (m_used_size > 0)
        {
            size_t   ttl_idx = *m_ttl_list.begin();
            element& e       = m_elements[ttl_idx];

            if (now >= e.m_expire_time)
            {
                do_erase(ttl_idx);
            }
            else
            {
                size_t lru_idx = m_lru_list.back();
                do_erase(lru_idx);
            }
        }
    }

    /// Cache lock for all mutations.
    mutex<thread_safe_type> m_lock;

    /// The uniform TTL for every key value pair inserted into the cache.
    std::chrono::milliseconds m_ttl;

    /// The current number of elements in the cache.
    size_t m_used_size{0};

    /// The main store for the key value pairs and metadata for each e.
    std::vector<element> m_elements;
    /// The keyed lookup data structure, the value is the index into 'm_elements'.
    std::unordered_map<key_type, size_t> m_keyed_elements;
    /// The lru sorted list from most recently used (head) to least recently used (tail).
    std::list<size_t> m_lru_list;
    /// The uniform ttl sorted list.
    std::list<size_t> m_ttl_list;
    /// The lru end/open list end.
    std::list<size_t>::iterator m_lru_end;
};

} // namespace cappuccino
