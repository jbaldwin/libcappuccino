#pragma once

#include <string>

namespace cappuccino
{
/**
 * Used in a few caches to allow for the user to 'peek' at an item without
 * updating its access pattern within the cache.
 */
enum class peek
{
    /// Do normal cache access patterns for this `Find()` call.
    no = 0,
    /// Ignore normal cache access patterns for this `Find()` call.
    yes = 1
};

auto to_string(peek p) -> const std::string&;

} // namespace cappuccino
