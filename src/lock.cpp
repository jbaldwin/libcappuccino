#include <cappuccino/lock.hpp>

namespace cappuccino
{
using namespace std::string_literals;

static const std::string sync_invalid_value = "invalid_value"s;
static const std::string sync_no            = "no"s;
static const std::string sync_yes           = "yes"s;

auto to_string(sync s) -> const std::string&
{
    switch (s)
    {
        case sync::no:
            return sync_no;
        case sync::yes:
            return sync_yes;
        default:
            return sync_invalid_value;
    }
}

} // namespace cappuccino
