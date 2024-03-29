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
 * Uniform time aware associative map for key value pairs. The associative map
 * has no fixed size and will dynamically resize as key values are added. The
 * map is less performant than the cache data structures, but offers unlimited
 * size.
 *
 * Each key value pair is evicted based on an global map TTL (time to live)
 * Expired key value pairs are evicted on the first Insert, Delete, Find, or
 * CleanExpiredValues call after the TTL has elapsed. The map may have O(N)
 * performance during these operations if N elements are reaching their TTLs and
 * being evicted at once.
 *
 * This map is thread_safe aware and can be used concurrently from multiple threads
 * safely. To remove locks/synchronization use thread_safe::no when creating the cache.
 *
 * @tparam key_type The key type.  Must support std::hash().
 * @tparam value_type The value type.  This is returned by copy on a find, so if
 * your data structure value is large it is advisable to store in a shared ptr.
 * @tparam thread_safe_type By default this map is thread safe, can be disabled for maps
 * specific to a single thread.
 */
template<typename key_type, typename value_type, thread_safe thread_safe_type = thread_safe::yes>
class ut_map
{
public:
    /**
     * @param uniform_ttl The uniform TTL for key values inserted into the map.
     * 100ms default.
     */
    ut_map(std::chrono::milliseconds uniform_ttl = std::chrono::milliseconds{100}) : m_uniform_ttl(uniform_ttl) {}

    /**
     * Inserts or updates the given key value pair using the uniform TTL.  On
     * update will reset the TTL.
     * @param key The key to store the value under.
     * @param value The value of data to store.
     * @param a Allowed methods of insertion | update.  Defaults to allowing
     *              insertions and updates.
     * @return True if the operation was successful based on `allow`.
     */
    auto insert(const key_type& key, value_type value, allow a = allow::insert_or_update) -> bool
    {
        std::lock_guard guard{m_lock};
        const auto      now         = std::chrono::steady_clock::now();
        const auto      expire_time = now + m_uniform_ttl;

        do_prune(now);

        return do_insert_update(key, std::move(value), expire_time, a);
    }

    /**
     * Inserts or updates a range of key value pairs using the default TTL.
     * This expects a container that has 2 values in the {key_type, value_type}
     * ordering.  There is a simple struct ut_map::KeyValue that can be put into
     * any iterable container to satisfy this requirement.
     * @tparam range_type A container with two items, key_type, value_type.
     * @param key_value_range The elements to insert or update into the map.
     * @param a Allowed methods of insertion | update.  Defaults to allowing
     *              insertions and updates.
     * @return The number of elements inserted based on `allow`.
     */
    template<typename range_type>
    auto insert_range(range_type&& key_value_range, allow a = allow::insert_or_update) -> size_t
    {
        size_t          inserted{0};
        std::lock_guard guard{m_lock};
        const auto      now         = std::chrono::steady_clock::now();
        const auto      expire_time = now + m_uniform_ttl;

        do_prune(now);

        for (auto& [key, value] : key_value_range)
        {
            if (do_insert_update(key, std::move(value), expire_time, a))
            {
                ++inserted;
            }
        }

        return inserted;
    }

    /**
     * Attempts to delete the given key.
     * @param key The key to remove from the map.
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
     * @param key_range The keys to delete from the map.
     * @return The number of items deleted from the map.
     */
    template<typename range_type>
    auto erase_range(const range_type& key_range) -> size_t
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
     * Attempts to find the given key's value.
     * @param key The key to lookup its value.
     * @return An optional with the key's value if it exists, or an empty optional
     * if it does not.
     */
    auto find(const key_type& key) -> std::optional<value_type>
    {
        std::lock_guard guard{m_lock};
        const auto      now = std::chrono::steady_clock::now();

        do_prune(now);

        return do_find(key);
    }

    /**
     * Attempts to find all the given keys values.
     * @tparam range_type A container with the set of keys to lookup, e.g.
     * vector<key_type>.
     * @param key_range A container with the set of keys to lookup.
     * @return All input keys to either a std::nullopt if it doesn't exist, or the
     * value if it does.
     */
    template<typename range_type>
    auto find_range(const range_type& key_range) -> std::vector<std::pair<key_type, std::optional<value_type>>>
    {
        std::vector<std::pair<key_type, std::optional<value_type>>> output;
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
     * Attempts to find all given keys values.
     *
     * The user should initialize this container with the keys to lookup with the
     * values all empty optionals.  The keys that are found will have the
     * optionals filled in with the appropriate values from the map.
     *
     * @tparam range_type A container with a pair of optional items,
     *                   e.g. vector<pair<k, optional<v>>> or map<k, optional<v>>.
     * @param key_optional_value_range The keys to optional values to fill out.
     */
    template<typename range_type>
    auto find_range_fill(range_type& key_optional_value_range) -> void
    {
        std::lock_guard guard{m_lock};
        const auto      now = std::chrono::steady_clock::now();

        do_prune(now);

        for (auto& [key, optional_value] : key_optional_value_range)
        {
            optional_value = do_find(key);
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
     * @return If this map is currently empty.
     */
    auto empty() const -> bool { return size() == 0ul; }

    /**
     * @return The number of elements inside the map.
     */
    auto size() const -> size_t
    {
        std::atomic_thread_fence(std::memory_order_acquire);
        return m_keyed_elements.size();
    }

    /**
     * Removes all elements from the cache (which are destroyed), leaving the container size 0.
     */
    auto clear() -> void
    {
        if (!empty())
        {
            std::lock_guard guard{m_lock};
            m_keyed_elements.clear();
            m_ttl_list.clear();
        }
    }

private:
    struct keyed_element;
    struct ttl_element;

    using keyed_iterator = typename std::map<key_type, keyed_element>::iterator;
    using ttl_iterator   = typename std::list<ttl_element>::iterator;

    struct keyed_element
    {
        /// The element's value.
        value_type m_value;
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
        /// The point in time in  which this element's value expires.
        std::chrono::steady_clock::time_point m_expire_time;
        /// The iterator to the Key in the data structure.
        keyed_iterator m_keyed_elements_position;
    };

    auto do_insert_update(
        const key_type& key, value_type&& value, std::chrono::steady_clock::time_point expire_time, allow a) -> bool
    {
        const auto keyed_position = m_keyed_elements.find(key);
        if (keyed_position != m_keyed_elements.end())
        {
            if (update_allowed(a))
            {
                do_update(keyed_position, std::move(value), expire_time);
                return true;
            }
        }
        else
        {
            if (insert_allowed(a))
            {
                do_insert(key, std::move(value), expire_time);
                return true;
            }
        }
        return false;
    }

    auto do_insert(const key_type& key, value_type&& value, std::chrono::steady_clock::time_point expire_time) -> void
    {
        keyed_element element;
        element.m_value = std::move(value);

        auto keyed_position = m_keyed_elements.emplace(key, std::move(element)).first;

        m_ttl_list.emplace_back(expire_time, keyed_position);

        // Update the elements iterator to ttl_element.
        keyed_position->second.m_ttl_position = std::prev(m_ttl_list.end());
    }

    auto do_update(keyed_iterator keyed_position, value_type&& value, std::chrono::steady_clock::time_point expire_time)
        -> void
    {
        auto& element   = keyed_position->second;
        element.m_value = std::move(value);

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

    auto do_find(const key_type& key) -> std::optional<value_type>
    {
        const auto keyed_position = m_keyed_elements.find(key);
        if (keyed_position != m_keyed_elements.end())
        {
            return {keyed_position->second.m_value};
        }

        return {};
    }

    auto do_prune(std::chrono::steady_clock::time_point now) -> size_t
    {
        const auto   ttl_begin = m_ttl_list.begin();
        const auto   ttl_end   = m_ttl_list.end();
        ttl_iterator ttl_iter;
        size_t       deleted{0};

        // Delete the keyed elements from the map. Not using do_erase to take
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
    /// which includes the value and an iterator to the associated m_ttl_list
    /// TTlElement.
    typename std::map<key_type, keyed_element> m_keyed_elements;

    /// The uniform ttl sorted list.
    typename std::list<ttl_element> m_ttl_list;

    /// The uniform TTL for every key value pair inserted into the map.
    std::chrono::milliseconds m_uniform_ttl;
};

} // namespace cappuccino
