#pragma once

#include <string>

namespace cappuccino {

/**
 * By default all `Insert()` functions will allow for inserting the key value pair
 * or updating an existing key value pair.  Pass in INSERT or UPDATE to
 * change this behavior to either only allow for inserting if the key doesn't exist
 * or only updating if the key already exists.
 */
enum class InsertMethodEnum {
    /// Insertion will only succeed if the key doesn't exist.
    INSERT = 0x01,
    /// Insertion will only succeed if the key already exists to be updated.
    UPDATE = 0x02,
    /// Insertion up add or update the key.
    INSERT_OR_UPDATE = INSERT | UPDATE
};

inline static auto insert_allowed(
    InsertMethodEnum method) -> bool
{
    return (static_cast<uint64_t>(method) & static_cast<uint64_t>(InsertMethodEnum::INSERT));
}

inline static auto update_allowed(
    InsertMethodEnum method) -> bool
{
    return (static_cast<uint64_t>(method) & static_cast<uint64_t>(InsertMethodEnum::UPDATE));
}

auto to_string(
    InsertMethodEnum method) -> const std::string&;

} // namespace cappuccino
