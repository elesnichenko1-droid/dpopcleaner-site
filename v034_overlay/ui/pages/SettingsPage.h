#pragma once
#include "ui/PageBase.h"
#include "ui/RecoveryControls.h"
#include "modules/FullCore.h"

#include <array>
#include <vector>

namespace dpop::ui {

class SettingsPage final : public PageBase {
public:
    ~SettingsPage() override { Destroy(); }

    bool Create(HWND parent, SessionLog& log) {
        return PageBase::Create(parent, log, L"Настройки");
    }

protected:
    bool OnCreate() override;
    void OnLayout(int width, int height) noexcept override;
    LRESULT OnMessage(UINT message, WPARAM wParam, LPARAM lParam, bool& handled) override;
    void OnPaint(HDC dc, const RECT& client) noexcept override;
    void OnVisibilityChanged(bool visible) noexcept override;

private:
    void Load();
    void Save();
    void RefreshExclusions();
    void AddFileExclusion();
    void AddFolderExclusion();
    void RemoveExclusion();

    RecoveryFonts fonts_;
    std::array<HWND, 4> labels_{};
    std::array<HWND, 8> checks_{};
    HWND exclusions_{};
    std::array<HWND, 3> exclusionButtons_{};
    HWND largeFile_{};
    HWND duplicateMin_{};
    HWND memoryTrim_{};
    HWND save_{};
    HWND reload_{};
    dpop::full::Settings settings_{};
};

}
