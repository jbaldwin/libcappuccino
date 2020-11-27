#pragma once

#include <mutex>
#include <string>

namespace cappuccino
{
/**
 * Determines if the cache should use synchronization or not.
 * By default all caches use synchronization for multi-threaded accesses.
 * By using sync::no you can disable locking internally on the caches, this should
 * really only be used if you implemented your own locking strategy or the cache
 * in question is owned and used by a single thread.
 */
enum class sync
{
    /// No synchronization.
    no = 0,
    /// synchronization via locks.
    yes = 1,
};

auto to_string(sync s) -> const std::string&;

/**
 * Creates a lock that based on the sync will behave correctly.
 * sync::yes => Uses a std::mutex
 * sync::no => is a no-op.
 *
 * @tparam sync_type The sync type to use.
 * @tparam lock_type The underlying lock type to use.  Must support .lock() and .unlock().
 */
template<sync sync_type, typename lock_type = std::mutex>
class mutex
{
public:
    constexpr auto lock() -> void
    {
        if constexpr (sync_type == sync::yes)
        {
            m_lock.lock();
        }
    }

    constexpr auto unlock() -> void
    {
        if constexpr (sync_type == sync::yes)
        {
            m_lock.unlock();
        }
    }

private:
    lock_type m_lock;
};

} // namespace cappuccino
