#include "cappuccino/InsertMethodEnum.hpp"

namespace cappuccino {

using namespace std::string_literals;

static std::string INSERT_METHOD_INVALID_VAUE = "INVALID_VALUE"s;
static std::string INSERT_METHOD_INSERT_OR_UPDATE = "INSERT_OR_UPDATE"s;
static std::string INSERT_METHOD_INSERT_ONLY = "INSERT_ONLY"s;
static std::string INSERT_METHOD_UPDATE_ONLY = "UPDATE_ONLY"s;

auto to_string(
    InsertMethodEnum method) -> const std::string&
{
    switch(method)
    {
        case InsertMethodEnum::INSERT_OR_UPDATE:
            return INSERT_METHOD_INSERT_OR_UPDATE;
        case InsertMethodEnum::INSERT:
            return INSERT_METHOD_INSERT_ONLY;
        case InsertMethodEnum::UPDATE:
            return INSERT_METHOD_UPDATE_ONLY;
        default:
            return INSERT_METHOD_INVALID_VAUE;
    }
}

} // namespace cappuccino
