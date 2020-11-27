#include "cappuccino/peek.hpp"

namespace cappuccino
{
static const std::string peek_invalid_value{"invalid_value"};
static const std::string peek_yes{"yes"};
static const std::string peek_no{"no"};

auto to_string(peek p) -> const std::string&
{
    switch (p)
    {
        case peek::yes:
            return peek_yes;
        case peek::no:
            return peek_no;
        default:
            return peek_invalid_value;
    }
}

} // namespace cappuccino
