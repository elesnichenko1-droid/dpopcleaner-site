#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace dpop::zapret {

struct StrategyEntry {
    std::filesystem::path relativeScript;
    std::wstring displayName;
    bool isDefault{};
};

bool IsLaunchableStrategyPath(const std::filesystem::path& relative) noexcept;
std::vector<StrategyEntry> EnumerateStrategiesAt(const std::filesystem::path& bundleRoot);

}
