#pragma once

namespace cappuccino {

/**
 * Determines if the cache should use synchronization or not.
 * By default all caches use synchronization for multi-threaded accesses.
 * By using UNSYNC you can disable locking internally on the caches, this should
 * really only be used if you implemented your own locking strategy or the cache
 * in question is owned and used by a single thread.
 */
enum class SyncImplEnum {
    /// No synchronization.
    UNSYNC = 0,
    /// Synchronization via locks.
    SYNC = 1,
};

} // namespace cappuccino
