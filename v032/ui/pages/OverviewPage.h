#pragma once
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "modules/SystemInfo.h"
#include "ui/Layout.h"
#include "ui/SessionLog.h"
#include "ui/ShellModel.h"

namespace dpop::ui {

inline constexpr UINT kOverviewLogChangedMessage = WM_APP + 32;

struct OverviewModel {
    unsigned cpuCount{};
    unsigned processCount{};
    std::wstring gpuName;

    std::uint64_t ramTotalBytes{};
    std::uint64_t ramAvailableBytes{};
    std::uint64_t ramUsedBytes{};
    unsigned ramUsedPercent{};

    std::uint64_t driveTotalBytes{};
    std::uint64_t driveFreeBytes{};
    std::uint64_t driveUsedBytes{};
    unsigned driveUsedPercent{};

    std::size_t appCount{};
    std::uint64_t recycleBytes{};
    bool recycleEmpty{};

    std::wstring guardText;
    std::wstring zapretText;
};

inline unsigned OverviewPercent(
    std::uint64_t used,
    std::uint64_t total
) noexcept {
    if (total == 0) {
        return 0;
    }

    return static_cast<unsigned>(
        (used * 100 + total / 2) / total
    );
}

inline OverviewModel BuildOverviewModel(
    const dpop::system_info::Snapshot& snapshot,
    std::size_t appCount,
    std::uint64_t recycleBytes,
    bool zapretServiceInstalled,
    bool zapretWinwsRunning
) {
    OverviewModel model{};

    model.cpuCount = snapshot.cpuCount;
    model.processCount = snapshot.processCount;
    model.gpuName = snapshot.gpuName;

    model.ramTotalBytes = snapshot.ramTotal;
    model.ramAvailableBytes =
        snapshot.ramAvailable <= snapshot.ramTotal
            ? snapshot.ramAvailable
            : snapshot.ramTotal;
    model.ramUsedBytes =
        model.ramTotalBytes - model.ramAvailableBytes;
    model.ramUsedPercent =
        OverviewPercent(model.ramUsedBytes, model.ramTotalBytes);

    model.driveTotalBytes = snapshot.systemDriveTotal;
    model.driveFreeBytes =
        snapshot.systemDriveFree <= snapshot.systemDriveTotal
            ? snapshot.systemDriveFree
            : snapshot.systemDriveTotal;
    model.driveUsedBytes =
        model.driveTotalBytes - model.driveFreeBytes;
    model.driveUsedPercent =
        OverviewPercent(model.driveUsedBytes, model.driveTotalBytes);

    model.appCount = appCount;
    model.recycleBytes = recycleBytes;
    model.recycleEmpty = recycleBytes == 0;

    model.guardText = L"QuickScan • AMSI";

    model.zapretText =
        zapretServiceInstalled
            ? L"Сервис установлен"
            : L"Сервис не установлен";
    model.zapretText +=
        zapretWinwsRunning ? L" • winws: ON" : L" • winws: OFF";

    return model;
}

class OverviewPage {
public:
    OverviewPage() = default;
    ~OverviewPage();

    OverviewPage(const OverviewPage&) = delete;
    OverviewPage& operator=(const OverviewPage&) = delete;

    bool Create(
        HWND parent,
        SessionLog& sessionLog,
        std::function<void(Page)> navigate
    );

    void Destroy() noexcept;
    void Show(bool visible) noexcept;
    void Layout(const Box& box) noexcept;
    void Refresh();

private:
    static LRESULT CALLBACK WindowProc(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam
    );

    LRESULT HandleMessage(
        UINT message,
        WPARAM wParam,
        LPARAM lParam
    );

    bool RegisterWindowClass() noexcept;
    void LayoutActions() noexcept;
    void Paint() noexcept;
    void NotifyLogChanged() noexcept;

    HWND parent_{};
    HWND hwnd_{};
    HWND refresh_{};
    HWND cleaning_{};
    HWND guard_{};
    HWND disk_{};
    HWND apps_{};

    HFONT titleFont_{};
    HFONT valueFont_{};
    HFONT detailFont_{};
    HFONT actionFont_{};

    SessionLog* sessionLog_{};
    std::function<void(Page)> navigate_;
    OverviewModel model_{};
};

}
