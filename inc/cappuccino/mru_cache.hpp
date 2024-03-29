#pragma once

#include "cappuccino/lock.hpp"
#include "cappuccino/peek.hpp"

#include <list>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace cappuccino
{
/**
 * Most Recently Used (MRU) Cache.
 * Each key value pair is evicted based on being the most recently used, and no other
 * criteria.
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
class mru_cache
{
private:
    using keyed_iterator = typename std::unordered_map<key_type, size_t>::iterator;
    using mru_Iterator   = std::list<size_t>::iterator;

public:
    /**
     * @param capacity The maximum number of key value pairs allowed in the cache.
     * @param max_load_factor The load factor for the hash map, generally 1 is a good default.
     */
    explicit mru_cache(size_t capacity, float max_load_factor = 1.0f) : m_elements(capacity), m_mru_list(capacity)
    {
        std::iota(m_mru_list.begin(), m_mru_list.end(), 0);
        m_mru_end = m_mru_list.begin();

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
     * @param key The key to delete from the mru cache.
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
    };

    /**
     * Attempts to find the given key's value.
     * @param key The key to lookup its value.
     * @param peek  Should the find act like all the item was not used?
     * @return An optional with the key's value if it exists, or an empty optional if it does not.
     */
    auto find(const key_type& key, peek peek = peek::no) -> std::optional<value_type>
    {
        std::lock_guard guard{m_lock};
        return do_find(key, peek);
    }

    /**
     * Attempts to find all the given keys values.
     * @tparam range_type A container with the set of keys to find their values, e.g. vector<key_type>.
     * @param key_range The keys to lookup their pairs.
     * @param peek Should the find act like all the items were not used?
     * @return The full set of keys to std::nullopt if the key wasn't found, or the value if found.
     */
    template<typename range_type>
    auto find_range(const range_type& key_range, peek peek = peek::no)
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
     * Attemps to find all the given keys values.
     *
     * The user should initialize this container with the keys to lookup with the values as all
     * empty optionals.  The keys that are found will have the optionals filled in with the
     * appropriate values from the cache.
     *
     * @tparam range_type A container with a pair of optional items,
     *                   e.g. map<key_type, optional<value_type>>.
     * @param key_optional_value_range The keys to optional values to fill out.
     * @param peek Should the find act like all the items were not used?
     */
    template<typename range_type>
    auto find_range_fill(range_type& key_optional_value_range, peek peek = peek::no) -> void
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
    auto empty() const -> bool { return (m_used_size == 0); }

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
        /// The iterator into the keyed data structure.
        keyed_iterator m_keyed_position;
        /// The iterator into the mru data structure.
        mru_Iterator m_mru_position;
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
        if (m_used_size >= m_elements.size())
        {
            do_prune();
        }

        auto element_idx = *m_mru_end;

        auto keyed_position = m_keyed_elements.emplace(key, element_idx).first;

        element& e         = m_elements[element_idx];
        e.m_value          = std::move(value);
        e.m_mru_position   = m_mru_end;
        e.m_keyed_position = keyed_position;

        ++m_mru_end;

        ++m_used_size;

        // Don't need to call do_access() since this element is at the most recently used end already.
    }

    auto do_update(keyed_iterator keyed_position, value_type&& value) -> void
    {
        element& e = m_elements[keyed_position->second];
        e.m_value  = std::move(value);

        // Move to most recently used end.
        do_access(e);
    }

    auto do_erase(size_t element_idx) -> void
    {
        element& e = m_elements[element_idx];

        if (e.m_mru_position != std::prev(m_mru_end))
        {
            m_mru_list.splice(m_mru_end, m_mru_list, e.m_mru_position);
        }
        --m_mru_end;

        m_keyed_elements.erase(e.m_keyed_position);

        --m_used_size;
    }

    auto do_find(const key_type& key, peek peek) -> std::optional<value_type>
    {
        auto keyed_position = m_keyed_elements.find(key);
        if (keyed_position != m_keyed_elements.end())
        {
            size_t   element_idx = keyed_position->second;
            element& e           = m_elements[element_idx];
            // Do not update the mru access if peeking.
            if (peek == peek::no)
            {
                do_access(e);
            }
            return {e.m_value};
        }

        return {};
    }

    auto do_access(element& e) -> void
    {
        // The the accessed item at the end of th MRU list, the end is the most recently
        // used, the beginning is the least recently used.
        m_mru_list.splice(m_mru_end, m_mru_list, e.m_mru_position);
    }

    auto do_prune() -> void
    {
        if (m_used_size > 0)
        {
            do_erase(m_mru_list.back());
        }
    }

    /// Cache lock for all mutations if thread_safe is enabled.
    mutex<thread_safe_type> m_lock;

    /// The current number of elements in the cache.
    size_t m_used_size{0};

    /// The main store for the key value pairs and metadata for each e.
    std::vector<element> m_elements;
    /// The keyed lookup data structure, the value is the index into 'm_elements'.
    std::unordered_map<key_type, size_t> m_keyed_elements;
    /// The mru sorted list, the value is the index into 'm_elements'.
    std::list<size_t> m_mru_list;
    /// The current end of the mru list.
    mru_Iterator m_mru_end;
};

} // namespace cappuccino
