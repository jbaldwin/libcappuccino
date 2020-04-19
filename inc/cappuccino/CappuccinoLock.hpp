#pragma once

#include "cappuccino/SyncImplEnum.hpp"

#include <mutex>

namespace cappuccino {

/**
 * Creates a lock that based on the SyncImplEnum will behave correctly.
 * SYNC => Uses a std::mutex
 * UNSYNC => is a no-op.
 *
 * @tparam SyncType The sync type to use.
 * @tparam LockType The underlying lock type to use.  Must support .lock() and .unlock().
 */
template <SyncImplEnum SyncType, typename LockType = std::mutex>
class CappuccinoLock {
public:
    CappuccinoLock() = default;

    constexpr auto lock() -> void
    {
        if constexpr (SyncType == SyncImplEnum::SYNC) {
            m_lock.lock();
        }
    }

    constexpr auto unlock() -> void
    {
        if constexpr (SyncType == SyncImplEnum::SYNC) {
            m_lock.unlock();
        }
    }

private:
    LockType m_lock;
};

} // namespace cappuccino
