#include "ui/pages/OverviewPage.h"

#include <iostream>

namespace {

bool Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "OverviewModelTests FAILED: "
                  << message << '\n';
        return false;
    }
    return true;
}

}

int main() {
    dpop::system_info::Snapshot s{};
    s.cpuCount = 12;
    s.ramTotal = 16ull * 1024 * 1024 * 1024;
    s.ramAvailable = 10ull * 1024 * 1024 * 1024;
    s.systemDriveTotal = 500ull * 1024 * 1024 * 1024;
    s.systemDriveFree = 100ull * 1024 * 1024 * 1024;
    s.processCount = 280;
    s.gpuName = L"Test GPU";

    const auto m = dpop::ui::BuildOverviewModel(
        s,
        76,
        0,
        false,
        false
    );

    if (!Check(
            m.ramUsedBytes ==
                6ull * 1024 * 1024 * 1024,
            "RAM used bytes mismatch")) return 1;
    if (!Check(m.ramUsedPercent == 38,
               "RAM percent must round to 38")) return 2;
    if (!Check(m.driveUsedPercent == 80,
               "drive percent must be 80")) return 3;
    if (!Check(m.appCount == 76,
               "app count mismatch")) return 4;
    if (!Check(m.recycleEmpty,
               "zero recycle bytes must mean empty")) return 5;
    if (!Check(
            m.zapretText ==
                L"Сервис не установлен • winws: OFF",
            "Zapret OFF text mismatch")) return 6;
    if (!Check(m.guardText == L"QuickScan • AMSI",
               "Guard capability text mismatch")) return 7;
    if (!Check(m.processCount == 280,
               "process count mismatch")) return 8;
    if (!Check(m.gpuName == L"Test GPU",
               "GPU name mismatch")) return 9;

    const auto active = dpop::ui::BuildOverviewModel(
        s,
        1,
        128,
        true,
        true
    );
    if (!Check(
            active.zapretText ==
                L"Сервис установлен • winws: ON",
            "Zapret ON text mismatch")) return 10;
    if (!Check(!active.recycleEmpty,
               "non-zero recycle bytes must not be empty")) return 11;

    dpop::system_info::Snapshot zero{};
    const auto zeroModel = dpop::ui::BuildOverviewModel(
        zero, 0, 0, false, true
    );
    if (!Check(zeroModel.ramUsedPercent == 0,
               "zero RAM total must not divide by zero")) return 12;
    if (!Check(zeroModel.driveUsedPercent == 0,
               "zero drive total must not divide by zero")) return 13;
    if (!Check(
            zeroModel.zapretText ==
                L"Сервис не установлен • winws: ON",
            "mixed Zapret state must remain truthful")) return 14;

    std::cout << "OverviewModelTests PASS\n";
    return 0;
}
