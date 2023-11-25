#pragma once

#include "cappuccino/allow.hpp"
#include "cappuccino/lock.hpp"

#include <list>
#include <map>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace cappuccino
{
/**
 * Least Frequently Used (LFU) Cache.
 * Each key value pair is evicted based on being the least frequently used, and no other
 * criteria.
 *
 * This cache is thread_safe aware and can be used concurrently from multiple threads safely.
 * To remove locks/synchronization use NO when creating the cache.
 *
 * @tparam key_type The key type.  Must support std::hash() and operator<().
 * @tparam value_type The value type.  This is returned by copy on a find, so if your data
 *                   structure value is large it is advisable to store in a shared ptr.
 * @tparam thread_safe_type By default this cache is thread safe, can be disabled for caches specific
 *                  to a single thread.
 */
template<typename key_type, typename value_type, thread_safe thread_safe_type = thread_safe::yes>
class lfu_cache
{
private:
    struct element;

    using open_list_iterator = typename std::list<element>::iterator;
    using lfu_iterator       = typename std::multimap<size_t, open_list_iterator>::iterator;
    using keyed_iterator     = typename std::unordered_map<key_type, open_list_iterator>::iterator;

public:
    /**
     * @param capacity The maximum number of key value pairs allowed in the cache.
     * @param max_load_factor The load factor for the hash map, generally 1 is a good default.
     */
    explicit lfu_cache(size_t capacity, float max_load_factor = 1.0f) : m_open_list(capacity)
    {
        m_open_list_end = m_open_list.begin();

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
    template<typename range_type>
    auto erase_range(const range_type& key_range) -> size_t
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
     * @param peek Should the find act like the item wasn't used?
     * @return An optional with the key's value if it exists, or an empty optional if it does not.
     */
    auto find(const key_type& key, bool peek = false) -> std::optional<value_type>
    {
        std::lock_guard guard{m_lock};
        return do_find(key, peek);
    }

    /**
     * Attempts to find the given key's value and use count.
     * @param key The key to lookup its value.
     * @param peek Should the find act like the item wasn't used?
     * @return An optional with the key's value and use count if it exists, or empty optional if it does not.
     */
    auto find_with_use_count(const key_type& key, bool peek = false) -> std::optional<std::pair<value_type, size_t>>
    {
        std::lock_guard guard{m_lock};
        return do_find_with_use_count(key, peek);
    }

    /**
     * Attempts to find all the given keys values.
     * @tparam range_type A container with the set of keys to find their values, e.g. vector<key_type>.
     * @param key_range The keys to lookup their pairs.
     * @param peek Should the find act like all the items were not used?
     * @return The full set of keys to std::nullopt if the key wasn't found, or the value if found.
     */
    template<typename range_type>
    auto find_range(const range_type& key_range, bool peek = false)
        -> std::vector<std::pair<key_type, std::optional<value_type>>>
    {
        std::vector<std::pair<key_type, std::optional<value_type>>> output;
        output.reserve(std::size(key_range));

        {
            std::lock_guard guard{m_lock};
            for (auto& key : key_range)
            {
                output.emplace_back(key, do_find(key, peek));
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
     * @param peek Should the find act like all the items were not used?
     */
    template<typename range_type>
    auto find_range_fill(range_type& key_optional_value_range, bool peek = false) -> void
    {
        std::lock_guard guard{m_lock};
        for (auto& [key, optional_value] : key_optional_value_range)
        {
            optional_value = do_find(key, peek);
        }
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
    auto capacity() const -> size_t { return m_open_list.size(); }

private:
    struct element
    {
        /// The iterator into the keyed data structure.
        keyed_iterator m_keyed_position;
        /// The iterator into the lfu data structure.
        lfu_iterator m_lfu_position;
        /// The element's value.
        value_type m_value;
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
        if (m_used_size >= m_open_list.size())
        {
            do_prune();
        }

        element& e = *m_open_list_end;

        auto keyed_position = m_keyed_elements.emplace(key, m_open_list_end).first;
        auto lfu_position   = m_lfu_list.emplace(1, m_open_list_end);

        e.m_value          = std::move(value);
        e.m_keyed_position = keyed_position;
        e.m_lfu_position   = lfu_position;

        ++m_open_list_end;

        ++m_used_size;
    }

    auto do_update(keyed_iterator keyed_position, value_type&& value) -> void
    {
        element& e = *keyed_position->second;
        e.m_value  = std::move(value);

        do_access(e);
    }

    auto do_erase(open_list_iterator open_list_position) -> void
    {
        element& e = *open_list_position;

        if (open_list_position != std::prev(m_open_list_end))
        {
            m_open_list.splice(m_open_list_end, m_open_list, open_list_position);
        }
        --m_open_list_end;

        m_keyed_elements.erase(e.m_keyed_position);
        m_lfu_list.erase(e.m_lfu_position);

        --m_used_size;
    }

    auto do_find(const key_type& key, bool peek) -> std::optional<value_type>
    {
        auto keyed_position = m_keyed_elements.find(key);
        if (keyed_position != m_keyed_elements.end())
        {
            element& e = *keyed_position->second;
            // Don't update the elements access in the LRU if peeking.
            if (!peek)
            {
                do_access(e);
            }
            return {e.m_value};
        }

        return {};
    }

    auto do_find_with_use_count(const key_type& key, bool peek) -> std::optional<std::pair<value_type, size_t>>
    {
        auto keyed_position = m_keyed_elements.find(key);
        if (keyed_position != m_keyed_elements.end())
        {
            element& e = *keyed_position->second;
            // Don't update the elements access in the LRU if peeking.
            if (!peek)
            {
                do_access(e);
            }
            return {std::make_pair(e.m_value, e.m_lfu_position->first)};
        }

        return {};
    }

    auto do_access(element& e) -> void
    {
        auto use_count = e.m_lfu_position->first;
        m_lfu_list.erase(e.m_lfu_position);
        e.m_lfu_position = m_lfu_list.emplace(use_count + 1, e.m_keyed_position->second);
    }

    auto do_prune() -> void
    {
        if (m_used_size > 0)
        {
            do_erase(m_lfu_list.begin()->second);
        }
    }

    /// Cache lock for all mutations if thread_safe is enabled.
    mutex<thread_safe_type> m_lock;

    /// The current number of elements in the cache.
    size_t m_used_size{0};

    /// The open list of un-used slots in 'm_elements'.
    std::list<element> m_open_list;
    /// The end of the open list to pull open slots from.
    typename std::list<element>::iterator m_open_list_end;
    /// The keyed lookup data structure, the value is the index into 'm_elements'.
    std::unordered_map<key_type, open_list_iterator> m_keyed_elements;
    /// The lfu sorted map, the key is the number of times the element has been used,
    /// the value is the index into 'm_elements'.
    std::multimap<size_t, open_list_iterator> m_lfu_list;
};

} // namespace cappuccino
