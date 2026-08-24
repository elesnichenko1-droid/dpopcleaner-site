#pragma once
#include "ui/PageBase.h"
#include "ui/RecoveryControls.h"
#include "modules/FullCore.h"

#include <array>

namespace dpop::ui {

class WindowsPage final : public PageBase {
public:
    ~WindowsPage() override { Destroy(); }

    bool Create(HWND parent, SessionLog& log) {
        return PageBase::Create(parent, log, L"Windows");
    }
    void CheckUpdateCacheAtStartup();

protected:
    bool OnCreate() override;
    void OnLayout(int width, int height) noexcept override;
    LRESULT OnMessage(UINT message, WPARAM wParam, LPARAM lParam, bool& handled) override;
    void OnPaint(HDC dc, const RECT& client) noexcept override;
    void OnVisibilityChanged(bool visible) noexcept override;

private:
    void Run(dpop::full::MaintenanceAction action);
    void AppendResult(std::wstring_view text);

    RecoveryFonts fonts_;
    std::array<HWND, 6> buttons_{};
    HWND output_{};
    dpop::full::Settings settings_{};
};

}
