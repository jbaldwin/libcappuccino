#include <cappuccino/lock.hpp>

namespace cappuccino
{
using namespace std::string_literals;

static const std::string SYNC_INVALID_VALUE = "INVALID_VALUE"s;
static const std::string SYNC_NO            = "NO"s;
static const std::string SYNC_YES           = "YES"s;

auto to_string(Sync sync) -> const std::string&
{
    switch (sync)
    {
        case Sync::NO:
            return SYNC_NO;
        case Sync::YES:
            return SYNC_YES;
        default:
            return SYNC_INVALID_VALUE;
    }
}

} // namespace cappuccino
