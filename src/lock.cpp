#include <cappuccino/lock.hpp>

namespace cappuccino
{
using namespace std::string_literals;

static const std::string thread_safe_invalid_value = "invalid_value"s;
static const std::string thread_safe_no            = "no"s;
static const std::string thread_safe_yes           = "yes"s;

auto to_string(thread_safe ts) -> const std::string&
{
    switch (ts)
    {
        case thread_safe::no:
            return thread_safe_no;
        case thread_safe::yes:
            return thread_safe_yes;
        default:
            return thread_safe_invalid_value;
    }
}

} // namespace cappuccino
