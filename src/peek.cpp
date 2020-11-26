#include "cappuccino/peek.hpp"

namespace cappuccino
{
using namespace std::string_literals;

static const std::string PEEK_INVALID_VALUE = "INVALID_VALUE"s;
static const std::string PEEK_YES           = "YES"s;
static const std::string PEEK_NO            = "NO"s;

auto to_string(Peek peek) -> const std::string&
{
    switch (peek)
    {
        case Peek::YES:
            return PEEK_YES;
        case Peek::NO:
            return PEEK_NO;
        default:
            return PEEK_INVALID_VALUE;
    }
}

} // namespace cappuccino
