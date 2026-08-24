#include "modules/ZapretCenterModel.h"

#include <cassert>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

int main() {
    const fs::path root = fs::temp_directory_path() / "dpop-zapret-model-test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "nested");
    for (const auto& name : {L"general.bat", L"general (ALT).bat", L"GENERAL_TEST.BAT", L"service.bat", L"ipset.bat", L"notes.txt"}) {
        std::ofstream(root / name).put('\n');
    }
    std::ofstream(root / "nested" / "general_nested.bat").put('\n');

    using dpop::zapret::IsLaunchableStrategyPath;
    assert(IsLaunchableStrategyPath(L"general.bat"));
    assert(IsLaunchableStrategyPath(L"GENERAL_TEST.BAT"));
    assert(!IsLaunchableStrategyPath(L"service.bat"));
    assert(!IsLaunchableStrategyPath(L"ipset.bat"));
    assert(!IsLaunchableStrategyPath(L"nested/general.bat"));
    assert(!IsLaunchableStrategyPath(L"../general.bat"));

    const auto strategies = dpop::zapret::EnumerateStrategiesAt(root);
    assert(strategies.size() == 3);
    assert(strategies[0].isDefault);
    assert(strategies[0].relativeScript == fs::path(L"general.bat"));
    for (const auto& strategy : strategies) {
        assert(strategy.relativeScript.parent_path().empty());
    }

    fs::remove_all(root, ec);
    return 0;
}
