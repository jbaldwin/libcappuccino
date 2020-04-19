#pragma once

#include <string>

namespace cappuccino {

/**
 * Used in a few caches to allow for the user to 'peek' at an item without
 * updating its access pattern within the cache.
 */
enum class Peek
{
    /// Do normal cache access patterns for this `Find()` call.
    NO = 0,
    /// Ignore normal cache access patterns for this `Find()` call.
    YES = 1
};

auto to_string(
    Peek peek) -> const std::string&;

} // namespace cappuccino
