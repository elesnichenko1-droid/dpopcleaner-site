#pragma once
#include <string>

namespace dpop::zapret {
struct Status { bool serviceInstalled{}; bool serviceRunning{}; bool winwsRunning{}; };
Status QueryStatus();
}
