#pragma once

#include "cappuccino/allow.hpp"
#include "cappuccino/lock.hpp"

#include <atomic>
#include <chrono>
#include <list>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace cappuccino
{
/**
 * Uniform time aware associative set for keys. The associative set has no fixed
 * size and will dynamically resize as key values are added. The set is less
 * performant than the cache data structures, but offers but offers unlimited
 * size.
 *
 * Each key is evicted based on a global set TTL (time to live)
 * Expired keys are evicted on the first Insert, Delete, Find, or
 * CleanExpiredValues call after the TTL has elapsed. The set may have O(N)
 * performance during these operations if N elements are reaching their TTLs and
 * being evicted at once.
 *
 * This set is thread_safe aware and can be used concurrently from multiple threads
 * safely. To remove locks/synchronization use thread_safe::no when creating the cache.
 *
 * @tparam key_type The key type.  Must support std::hash().
 * @tparam thread_safe_type By default this set is thread safe, can be disabled for sets
 * specific to a single thread.
 */
template<typename key_type, thread_safe thread_safe_type = thread_safe::yes>
class ut_set
{
public:
    /**
     * @param uniform_ttl The uniform TTL of keys inserted into the set. 100ms
     * default.
     */
    explicit ut_set(std::chrono::milliseconds uniform_ttl = std::chrono::milliseconds{100}) : m_uniform_ttl(uniform_ttl)
    {
    }

    /**
     * Inserts or updates the given key.  On update will reset the TTL.
     * @param key The key to store.
     * @param a Allowed methods of insertion | update.  Defaults to allowing
     *              insertions and updates.
     * @return True if the operation was successful based on `allow`.
     */
    auto insert(const key_type& key, allow a = allow::insert_or_update) -> bool
    {
        std::lock_guard guard{m_lock};
        const auto      now         = std::chrono::steady_clock::now();
        const auto      expire_time = now + m_uniform_ttl;

        do_prune(now);

        return do_insert_update(key, expire_time, a);
    }

    /**
     * Inserts or updates a range of keys with uniform TTL.
     * @tparam range_type A container of key_types.
     * @param key_range The elements to insert or update into the set.
     * @param a Allowed methods of insertion | update.  Defaults to allowing
     *              insertions and updates.
     * @return The number of elements inserted based on `allow`.
     */
    template<typename range_type>
    auto insert_range(range_type&& key_range, allow a = allow::insert_or_update) -> size_t
    {
        size_t          inserted{0};
        std::lock_guard guard{m_lock};
        const auto      now         = std::chrono::steady_clock::now();
        const auto      expire_time = now + m_uniform_ttl;

        do_prune(now);

        for (const auto& key : key_range)
        {
            if (do_insert_update(key, expire_time, a))
            {
                ++inserted;
            }
        }

        return inserted;
    }

    /**
     * Attempts to delete the given key.
     * @param key The key to remove from the set.
     * @return True if the key was deleted, false if the key does not exist.
     */
    auto erase(const key_type& key) -> bool
    {
        std::lock_guard guard{m_lock};
        const auto      now = std::chrono::steady_clock::now();

        do_prune(now);

        const auto keyed_position = m_keyed_elements.find(key);
        if (keyed_position != m_keyed_elements.end())
        {
            do_erase(keyed_position);
            return true;
        }
        else
        {
            return false;
        }
    }

    /**
     * Attempts to delete all given keys.
     * @tparam range_type A container with the set of keys to delete, e.g.
     * vector<k> or set<k>.
     * @param key_range The keys to delete from the set.
     * @return The number of items deleted from the set.
     */
    template<template<class...> typename range_type>
    auto erase_range(const range_type<key_type>& key_range) -> size_t
    {
        size_t          deleted{0};
        std::lock_guard guard{m_lock};
        const auto      now = std::chrono::steady_clock::now();

        do_prune(now);

        for (const auto& key : key_range)
        {
            const auto keyed_position = m_keyed_elements.find(key);
            if (keyed_position != m_keyed_elements.end())
            {
                do_erase(keyed_position);
                ++deleted;
            }
        }

        return deleted;
    }

    /**
     * Attempts to find the given key.
     * @param key The key to lookup.
     * @return True if key exists, or false if it does not.
     */
    auto find(const key_type& key) -> bool
    {
        std::lock_guard guard{m_lock};
        const auto      now = std::chrono::steady_clock::now();

        do_prune(now);

        return do_find(key);
    }

    /**
     * Attempts to find all the given keys presence.
     * @tparam range_type A container with the set of keys to lookup, e.g.
     * vector<key_type>.
     * @param key_range A container with the set of keys to lookup.
     * @return All input keys with a bool indicating if it exists.
     */
    template<template<class...> typename range_type>
    auto find_range(const range_type<key_type>& key_range) -> std::vector<std::pair<key_type, bool>>
    {
        std::vector<std::pair<key_type, bool>> output;
        output.reserve(std::size(key_range));

        std::lock_guard guard{m_lock};
        const auto      now = std::chrono::steady_clock::now();

        do_prune(now);

        for (const auto& key : key_range)
        {
            output.emplace_back(key, do_find(key));
        }

        return output;
    }

    /**
     * Attempts to find all given keys presence.
     *
     * The user should initialize this container with the keys to lookup with the
     * values all bools. The keys that are found will have the bools set
     * indicating presence in the set.
     *
     * @tparam range_type A container with a pair of optional items,
     *                   e.g. vector<pair<k, bool>> or map<k, bool>.
     * @param key_bool_range The keys to bools to fill out.
     */
    template<typename range_type>
    auto find_range_fill(range_type& key_bool_range) -> void
    {
        std::lock_guard guard{m_lock};
        const auto      now = std::chrono::steady_clock::now();

        do_prune(now);

        for (auto& [key, boolean] : key_bool_range)
        {
            boolean = do_find(key);
        }
    }

    /**
     * Trims the TTL list of items and evicts all expired elements.
     * @return The number of elements pruned.
     */
    auto clean_expired_values() -> size_t
    {
        std::lock_guard guard{m_lock};
        const auto      now = std::chrono::steady_clock::now();
        return do_prune(now);
    }

    /**
     * @return If this set is currently empty.
     */
    auto empty() const -> bool { return size() == 0ul; }

    /**
     * @return The number of elements inside the set.
     */
    auto size() const -> size_t
    {
        std::atomic_thread_fence(std::memory_order_acquire);
        return m_keyed_elements.size();
    }

private:
    struct keyed_element;
    struct ttl_element;

    using keyed_iterator = typename std::map<key_type, keyed_element>::iterator;
    using ttl_iterator   = typename std::list<ttl_element>::iterator;

    struct keyed_element
    {
        /// The iterator into the ttl data structure.
        ttl_iterator m_ttl_position;
    };

    struct ttl_element
    {
        ttl_element(std::chrono::steady_clock::time_point expire_time, keyed_iterator keyed_elements_position)
            : m_expire_time(expire_time),
              m_keyed_elements_position(keyed_elements_position)
        {
        }
        /// The point in time in  which this element's key expires.
        std::chrono::steady_clock::time_point m_expire_time;
        /// The iterator to the Key in the data structure.
        keyed_iterator m_keyed_elements_position;
    };

    auto do_insert_update(const key_type& key, std::chrono::steady_clock::time_point expire_time, allow a) -> bool
    {
        const auto keyed_position = m_keyed_elements.find(key);
        if (keyed_position != m_keyed_elements.end())
        {
            if (update_allowed(a))
            {
                do_update(keyed_position, expire_time);
                return true;
            }
        }
        else
        {
            if (insert_allowed(a))
            {
                do_insert(key, expire_time);
                return true;
            }
        }
        return false;
    }

    auto do_insert(const key_type& key, std::chrono::steady_clock::time_point expire_time) -> void
    {
        keyed_element element;

        auto keyed_position = m_keyed_elements.emplace(key, std::move(element)).first;

        m_ttl_list.emplace_back(expire_time, keyed_position);

        // Update the elements iterator to ttl_element.
        keyed_position->second.m_ttl_position = std::prev(m_ttl_list.end());
    }

    auto do_update(keyed_iterator keyed_position, std::chrono::steady_clock::time_point expire_time) -> void
    {
        auto& element = keyed_position->second;

        // Update the ttl_element's expire time.
        element.m_ttl_position->m_expire_time = expire_time;

        // Push to the end of the Ttl list.
        m_ttl_list.splice(m_ttl_list.end(), m_ttl_list, element.m_ttl_position);

        // Update the elements iterator to ttl_element.
        element.m_ttl_position = std::prev(m_ttl_list.end());
    }

    auto do_erase(keyed_iterator keyed_elements_position) -> void
    {
        m_ttl_list.erase(keyed_elements_position->second.m_ttl_position);

        m_keyed_elements.erase(keyed_elements_position);
    }

    auto do_find(const key_type& key) -> bool
    {
        const auto keyed_position = m_keyed_elements.find(key);
        if (keyed_position != m_keyed_elements.end())
        {
            return true;
        }

        return false;
    }

    auto do_prune(std::chrono::steady_clock::time_point now) -> size_t
    {
        const auto   ttl_begin = m_ttl_list.begin();
        const auto   ttl_end   = m_ttl_list.end();
        ttl_iterator ttl_iter;
        size_t       deleted{0};

        // Delete the keyed elements from the set. Not using do_erase to take
        // advantage of iterator range delete for TTLs.
        for (ttl_iter = ttl_begin; ttl_iter != ttl_end && now >= ttl_iter->m_expire_time; ++ttl_iter)
        {
            m_keyed_elements.erase(ttl_iter->m_keyed_elements_position);
            ++deleted;
        }

        // Delete the range of TTLs.
        if (ttl_iter != ttl_begin)
        {
            m_ttl_list.erase(ttl_begin, ttl_iter);
        }

        return deleted;
    }

    /// Thread lock for all mutations.
    mutex<thread_safe_type> m_lock;

    /// The keyed lookup data structure, the value is the keyed_element struct
    /// which is an iterator to the associated m_ttl_list TTlElement.
    typename std::map<key_type, keyed_element> m_keyed_elements;

    /// The uniform ttl sorted list.
    typename std::list<ttl_element> m_ttl_list;

    /// The uniform TTL for every key inserted into the set.
    std::chrono::milliseconds m_uniform_ttl;
};
} // namespace cappuccino
