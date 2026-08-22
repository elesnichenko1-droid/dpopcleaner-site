#pragma once
#include <cstdint>

namespace dpop::system_info {
struct Snapshot {
    unsigned cpuCount{};
    std::uint64_t ramTotal{};
    std::uint64_t ramAvailable{};
    std::uint64_t systemDriveTotal{};
    std::uint64_t systemDriveFree{};
    unsigned processCount{};
};
Snapshot Collect();
}
