#include "modules/FullCore.h"
#include <filesystem>
#include <iostream>

namespace {
bool Check(bool value, const char* message) {
    if (!value) std::cerr << "FullCoreTests FAILED: " << message << '\n';
    return value;
}
}

int main() {
    using namespace dpop::full;
    if (!Check(Percent(0, 0) == 0, "zero total")) return 1;
    if (!Check(Percent(50, 100) == 50, "50 percent")) return 2;
    if (!Check(Percent(3, 8) == 38, "rounded percent")) return 3;

    for (int i = 0; i < 13; ++i) {
        if (!Check(!CleanKindLabel(static_cast<CleanKind>(i)).empty(), "clean label")) return 4;
    }

    if (!Check(IsGuardCandidate(std::filesystem::path(L"sample.EXE")), "EXE guard candidate")) return 5;
    if (!Check(IsGuardCandidate(std::filesystem::path(L"script.ps1")), "PS1 guard candidate")) return 6;
    if (!Check(!IsGuardCandidate(std::filesystem::path(L"photo.jpg")), "JPG not guard candidate")) return 7;

    Settings settings{};
    if (!Check(settings.confirmDestructive, "confirm default")) return 8;
    if (!Check(settings.largeFileMB == 500, "large file default")) return 9;
    if (!Check(settings.duplicateMinMB == 10, "duplicate default")) return 10;
    if (!Check(!settings.runAtStartup, "startup default")) return 11;

    if (!Check(!MaintenanceLabel(MaintenanceAction::ResetBase).empty(), "maintenance label")) return 12;
    if (!Check(!ToolLabel(ToolAction::WindowsSecurity).empty(), "tool label")) return 13;

    std::cout << "FullCoreTests PASS\n";
    return 0;
}
