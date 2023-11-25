#pragma once

#include "cappuccino/allow.hpp"
#include "cappuccino/lock.hpp"

#include <list>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace cappuccino
{
/**
 * First In First Out (FIFO) Cache.
 * Each key value pair is evicted based on being the first in first out policy, and no other
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
class fifo_cache
{
private:
    struct element;

    using fifo_iterator  = typename std::list<element>::iterator;
    using keyed_iterator = typename std::unordered_map<key_type, fifo_iterator>::iterator;

public:
    /**
     * @param capacity The maximum number of key value pairs allowed in the cache.
     * @param max_load_factor The load factor for the hash map, generally 1 is a good default.
     */
    explicit fifo_cache(size_t capacity, float max_load_factor = 1.0f) : m_fifo_list(capacity)
    {
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
     * Inserts or updates the given iterator range of key value pairs.
     * @tparam iterator An iterator that contains the set of key values pairs to insert or update.
     * @param begin The first key value to pair to insert.
     * @param end One past the last key value pair to insert.
     * @param a Allowed methods of insertion | update.  Defaults to allowing insertsion and updates.
     * @return The number of elements inserted based on `allow`.
     */
    template<typename iterator>
    auto insert(iterator begin, iterator end, allow a = allow::insert_or_update) -> size_t
    {
        size_t inserted{0};

        {
            std::lock_guard guard{m_lock};
            while (begin != end)
            {
                auto& [key, value] = *begin;
                if (do_insert_update(key, std::move(value), a))
                {
                    ++inserted;
                }

                ++begin;
            }
        }

        return inserted;
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
        return insert(std::begin(key_value_range), std::end(key_value_range), a);
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
     * Attempts to delete the given range of keys.
     * @tparam iterator An iterator that contains the set of keys to delete.
     * @param begin The first key to delete.
     * @param end One past the last key to delete.
     * @return The number of items deleted from the cache.
     */
    template<typename iterator>
    auto erase(iterator begin, iterator end) -> size_t
    {
        size_t deleted_elements{0};

        std::lock_guard{m_lock};
        while (begin != end)
        {
            auto keyed_position = m_keyed_elements.find(*begin);
            if (keyed_position != m_keyed_elements.end())
            {
                ++deleted_elements;
                do_erase(keyed_position->second);
            }
            ++begin;
        }

        return deleted_elements;
    }

    /**
     * Attempts to delete all given keys in the provided container.
     * @tparam range_type A container with the set of keys to delete, e.g. vector<key_type>, set<key_type>.
     * @param key_range The keys to delete from the cache.
     * @return The number of items deleted from the cache.
     */
    template<typename range_type>
    auto erase_range(const range_type& key_range) -> size_t
    {
        return erase(std::begin(key_range), std::end(key_range));
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
     * Attempts to find all given keys in the provided range.
     * @tparam iterator An iterator that contains the set of keys to find.
     * @param begin The first key to find.
     * @param end One past the last key to find.
     * @param distance If std::distance(begin, end) is known the function will reserve the return
     *                 output vector's elements.
     * @return The full set of keys to std::nullopt if the key wasn't found, or the value if found.
     */
    template<typename iterator>
    auto find(iterator begin, iterator end, size_t distance = 0)
        -> std::vector<std::pair<key_type, std::optional<value_type>>>
    {
        std::vector<std::pair<key_type, std::optional<value_type>>> output;
        if (distance > 0)
        {
            output.reserve(distance);
        }

        {
            std::lock_guard guard{m_lock};
            while (begin != end)
            {
                output.emplace_back(*begin, do_find(*begin));

                ++begin;
            }
        }

        return output;
    }

    /**
     * Attempts to find all the given keys values.
     * @tparam range_type A container with the set of keys to find their values, e.g. vector<key_type>.
     * @param key_range The keys to lookup their pairs.
     * @return The full set of keys to std::nullopt if the key wasn't found, or the value if found.
     */
    template<typename range_type>
    auto find_range(const range_type& key_range) -> std::vector<std::pair<key_type, std::optional<value_type>>>
    {
        return find(std::begin(key_range), std::end(key_range), std::size(key_range));
    }

    /**
     * Attempts to find all the given keys values from begin to end.
     *
     * The user should initialize this container with the keys to lookup with the values as all
     * empty optionals.  The keys that are found will have the optionals filled in with the
     * appropriate values from the cache.
     *
     * @tparam iterator The starting iterator to attempt to find and fill.
     * @param begin The first key optional value to try and find + fill.
     * @param end One past the last item to find.
     */
    template<typename iterator>
    auto find_range_fill(iterator begin, iterator end) -> void
    {
        std::lock_guard guard{m_lock};
        while (begin != end)
        {
            auto& [key, value_opt] = *begin;
            value_opt              = do_find(key);

            ++begin;
        }
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
        return find_range_fill(std::begin(key_optional_value_range), std::end(key_optional_value_range));
    }

    /**
     * @return If this cache is currenty empty.
     */
    auto empty() const -> bool { return (m_used_size == 0); }

    /**
     * @return The number of elements in the cache, this does not prune expired elements.
     */
    auto size() const -> size_t { return m_used_size; }

    /**
     * @return The maximum capacity of this cache.
     */
    auto capacity() const -> size_t { return m_fifo_list.size(); }

private:
    struct element
    {
        /// The iterator into the keyed data structure, when the cache starts
        /// none of the keyed iterators are set, but also deletes can unset this optional.
        std::optional<keyed_iterator> m_keyed_position;
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
        // Take the head item and replace it at the tail of the fifo list.
        m_fifo_list.splice(m_fifo_list.end(), m_fifo_list, m_fifo_list.begin());

        fifo_iterator last_element_position = std::prev(m_fifo_list.end());
        element&      e                     = *last_element_position;

        // since we 'deleted' the head item, if it has a value in the keyed data
        // structure it needs to be deleted first.
        if (e.m_keyed_position.has_value())
        {
            m_keyed_elements.erase(e.m_keyed_position.value());
        }
        else
        {
            // If the cache isn't 'full' increment its size.
            ++m_used_size;
        }

        e.m_value          = std::move(value);
        e.m_keyed_position = m_keyed_elements.emplace(key, last_element_position).first;
    }

    auto do_update(keyed_iterator keyed_position, value_type&& value) -> void
    {
        element& e = *keyed_position->second;
        e.m_value  = std::move(value);

        // there is no access update in a fifo cache.
    }

    auto do_erase(fifo_iterator fifo_position) -> void
    {
        element& e = *fifo_position;

        // Move the item being deleted to the head for re-use, inserts
        // will pull from the head to re-use next.
        if (fifo_position != m_fifo_list.begin())
        {
            m_fifo_list.splice(m_fifo_list.begin(), m_fifo_list, fifo_position);
        }

        if (e.m_keyed_position.has_value())
        {
            m_keyed_elements.erase(e.m_keyed_position.value());
            e.m_keyed_position = std::nullopt;
        }

        --m_used_size;
    }

    auto do_find(const key_type& key) -> std::optional<value_type>
    {
        auto keyed_position = m_keyed_elements.find(key);
        if (keyed_position != m_keyed_elements.end())
        {
            fifo_iterator fifo_position = keyed_position->second;
            element&      e             = *fifo_position;
            return {e.m_value};
        }

        return {};
    }

    /// Cache lock for all mutations if thread_safe is enabled.
    mutex<thread_safe_type> m_lock;

    /// The current number of elements in the cache.
    size_t m_used_size{0};

    /// The ordering of first in-first out queue of elements.
    std::list<element> m_fifo_list;
    /// The keyed lookup data structure, the value is the index into 'm_elements'.
    std::unordered_map<key_type, fifo_iterator> m_keyed_elements;
};

} // namespace cappuccino
