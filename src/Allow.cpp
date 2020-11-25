#include "cappuccino/Allow.hpp"

namespace cappuccino
{
using namespace std::string_literals;

static std::string ALLOW_INVALID_VALUE    = "INVALID_VALUE"s;
static std::string ALLOW_INSERT_OR_UPDATE = "INSERT_OR_UPDATE"s;
static std::string ALLOW_INSERT           = "INSERT"s;
static std::string ALLOW_UPDATE           = "UPDATE"s;

auto to_string(Allow allow) -> const std::string&
{
    switch (allow)
    {
        case Allow::INSERT_OR_UPDATE:
            return ALLOW_INSERT_OR_UPDATE;
        case Allow::INSERT:
            return ALLOW_INSERT;
        case Allow::UPDATE:
            return ALLOW_UPDATE;
        default:
            return ALLOW_INVALID_VALUE;
    }
}

} // namespace cappuccino
