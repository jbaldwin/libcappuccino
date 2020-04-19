#include <cappuccino/SyncImplEnum.hpp>

namespace cappuccino {

using namespace std::string_literals;

static const std::string SYNC_IMPL_INVALID = "INVALID_VALUE"s;
static const std::string SYNC_IMPL_UNSYNC = "UNSYNC"s;
static const std::string SYNC_IMPL_SYNC = "SYNC"s;

auto to_string(
    SyncImplEnum sync
) -> const std::string&
{
    switch(sync)
    {
        case SyncImplEnum::UNSYNC:
            return SYNC_IMPL_UNSYNC;
        case SyncImplEnum::SYNC:
            return SYNC_IMPL_SYNC;
        default:
            return SYNC_IMPL_INVALID;
    }
}

} // namespace cappuccino
