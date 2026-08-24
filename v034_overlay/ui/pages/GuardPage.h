#pragma once

#include "ui/PageBase.h"
#include "ui/RecoveryControls.h"
#include "modules/FullCore.h"

#include <array>
#include <filesystem>
#include <vector>

namespace dpop::ui {

class GuardPage final : public PageBase {
public:
    ~GuardPage() override { Destroy(); }
    bool Create(HWND parent, SessionLog& log) {
        return PageBase::Create(parent, log, L"DPopGuard");
    }
    void RunQuickScanAtStartup() { if (!IsBusy()) RunQuickScan(); }

protected:
    bool OnCreate() override;
    void OnLayout(int width, int height) noexcept override;
    LRESULT OnMessage(UINT message, WPARAM wParam, LPARAM lParam, bool& handled) override;
    void OnPaint(HDC dc, const RECT& client) noexcept override;
    void OnVisibilityChanged(bool visible) noexcept override;

private:
    void RunQuickScan();
    void RunDefenderQuickScan();
    void ScanFile();
    void ScanFolder();
    void QuarantineSelected();
    void OpenQuarantine();
    void OpenWindowsSecurity();
    void ClearResults();
    void SetSummary(unsigned processes, unsigned startup, unsigned files, unsigned findings);
    void SetProgress(unsigned value) noexcept;

    RecoveryFonts fonts_;
    std::array<HWND, 8> actionButtons_{};
    std::array<HWND, 4> summaryLabels_{};
    HWND progress_{};
    HWND list_{};
    std::vector<std::filesystem::path> rowPaths_;
    std::wstring modeStatus_{L"Готово. DPopGuard использует собственные эвристики, Windows AMSI и Microsoft Defender, если он доступен."};
};

}
