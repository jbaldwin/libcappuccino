#pragma once

#include <mutex>
#include <string>

namespace cappuccino
{
/**
 * Determines if the cache should use synchronization or not.
 * By default all caches use synchronization for multi-threaded accesses.
 * By using UNSYNC you can disable locking internally on the caches, this should
 * really only be used if you implemented your own locking strategy or the cache
 * in question is owned and used by a single thread.
 */
enum class Sync
{
    /// No synchronization.
    NO = 0,
    /// Synchronization via locks.
    YES = 1,
};

auto to_string(Sync sync) -> const std::string&;

/**
 * Creates a lock that based on the Sync will behave correctly.
 * SYNC => Uses a std::mutex
 * UNSYNC => is a no-op.
 *
 * @tparam SyncType The sync type to use.
 * @tparam LockType The underlying lock type to use.  Must support .lock() and .unlock().
 */
template<Sync SyncType, typename LockType = std::mutex>
class Lock
{
public:
    constexpr auto lock() -> void
    {
        if constexpr (SyncType == Sync::YES)
        {
            m_lock.lock();
        }
    }

    constexpr auto unlock() -> void
    {
        if constexpr (SyncType == Sync::YES)
        {
            m_lock.unlock();
        }
    }

private:
    LockType m_lock;
};

} // namespace cappuccino
