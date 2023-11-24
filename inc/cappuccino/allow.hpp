#pragma once

#include <cstdint>
#include <string>

namespace cappuccino
{
/**
 * By default all `Insert()` functions will allow for inserting the key value pair
 * or updating an existing key value pair.  Pass in INSERT or UPDATE to
 * change this behavior to either only allow for inserting if the key doesn't exist
 * or only updating if the key already exists.
 */
enum class allow : uint64_t
{
    /// Insertion will only succeed if the key doesn't exist.
    insert = 0x01,
    /// Insertion will only succeed if the key already exists to be updated.
    update = 0x02,
    /// Will insert or update regardless if the key exists or not.
    insert_or_update = insert | update
};

inline static auto insert_allowed(allow a) -> bool
{
    return (static_cast<uint64_t>(a) & static_cast<uint64_t>(allow::insert));
}

inline static auto update_allowed(allow a) -> bool
{
    return (static_cast<uint64_t>(a) & static_cast<uint64_t>(allow::update));
}

auto to_string(allow a) -> const std::string&;

} // namespace cappuccino
