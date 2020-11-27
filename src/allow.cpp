#include "cappuccino/allow.hpp"

namespace cappuccino
{
static std::string allow_invalid_value{"invalid_value"};
static std::string allow_insert_or_update{"insert_or_update"};
static std::string allow_insert{"insert"};
static std::string allow_update{"update"};

auto to_string(allow a) -> const std::string&
{
    switch (a)
    {
        case allow::insert_or_update:
            return allow_insert_or_update;
        case allow::insert:
            return allow_insert;
        case allow::update:
            return allow_update;
        default:
            return allow_invalid_value;
    }
}

} // namespace cappuccino
