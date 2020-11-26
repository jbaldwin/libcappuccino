#pragma once

#include <string>

namespace cappuccino
{
/**
 * By default all `Insert()` functions will allow for inserting the key value pair
 * or updating an existing key value pair.  Pass in INSERT or UPDATE to
 * change this behavior to either only allow for inserting if the key doesn't exist
 * or only updating if the key already exists.
 */
enum class Allow
{
    /// Insertion will only succeed if the key doesn't exist.
    INSERT = 0x01,
    /// Insertion will only succeed if the key already exists to be updated.
    UPDATE = 0x02,
    /// Insertion up add or update the key.
    INSERT_OR_UPDATE = INSERT | UPDATE
};

inline static auto insert_allowed(Allow allow) -> bool
{
    return (static_cast<uint64_t>(allow) & static_cast<uint64_t>(Allow::INSERT));
}

inline static auto update_allowed(Allow allow) -> bool
{
    return (static_cast<uint64_t>(allow) & static_cast<uint64_t>(Allow::UPDATE));
}

auto to_string(Allow allow) -> const std::string&;

} // namespace cappuccino
