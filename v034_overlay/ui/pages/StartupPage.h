#pragma once
#include "ui/PageBase.h"
#include "ui/RecoveryControls.h"
#include "modules/StartupManager.h"

#include <array>
#include <vector>

namespace dpop::ui {

class StartupPage final : public PageBase {
public:
    ~StartupPage() override { Destroy(); }
    bool Create(HWND parent, SessionLog& log) {
        return PageBase::Create(parent, log, L"Автозагрузка");
    }

protected:
    bool OnCreate() override;
    void OnLayout(int width, int height) noexcept override;
    LRESULT OnMessage(UINT message, WPARAM wParam, LPARAM lParam, bool& handled) override;
    void OnPaint(HDC dc, const RECT& client) noexcept override;
    void OnVisibilityChanged(bool visible) noexcept override;

private:
    void Refresh();
    void ToggleSelected();
    void OpenSelected();
    void OpenWindowsStartup();
    void ShowAdaptation();
    void UpdateActionState();
    int IconIndexFor(const dpop::startup::Entry& entry);

    RecoveryFonts fonts_;
    HWND count_{};
    HWND list_{};
    std::array<HWND, 5> buttons_{};
    std::vector<dpop::startup::Entry> entries_;
    HIMAGELIST systemImages_{};
    bool loaded_{false};
};

}
