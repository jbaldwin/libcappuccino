#pragma once

#include "cappuccino/SyncImplEnum.h"

#include <mutex>

namespace cappuccino {

template <SyncImplEnum SyncType>
struct LockScopeGuard {
    LockScopeGuard(std::mutex& lock)
        : m_lock(lock)
    {
        if constexpr (SyncType == SyncImplEnum::SYNC) {
            m_lock.lock();
        }
    }
    ~LockScopeGuard()
    {
        if constexpr (SyncType == SyncImplEnum::SYNC) {
            m_lock.unlock();
        }
    }

    std::mutex& m_lock;
};

} // namespace cappuccino
