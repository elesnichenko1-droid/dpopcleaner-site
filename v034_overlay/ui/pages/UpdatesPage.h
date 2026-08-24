#pragma once
#include "ui/PageBase.h"
#include "ui/RecoveryControls.h"
#include "update/UpdateClient.h"

#include <array>

namespace dpop::ui {

class UpdatesPage final : public PageBase {
public:
    ~UpdatesPage() override { Destroy(); }
    bool Create(HWND parent, SessionLog& log) {
        return PageBase::Create(parent, log, L"Обновления");
    }
    void CheckAtStartup();

protected:
    bool OnCreate() override;
    void OnLayout(int width, int height) noexcept override;
    LRESULT OnMessage(UINT message, WPARAM wParam, LPARAM lParam, bool& handled) override;
    void OnPaint(HDC dc, const RECT& client) noexcept override;
    void OnVisibilityChanged(bool visible) noexcept override;

private:
    void Check(bool promptWhenAvailable = false);
    void Install(bool startupApproved = false);
    void OpenRelease();
    void RefreshText();

    RecoveryFonts fonts_;
    std::array<HWND, 5> labels_{};
    std::array<HWND, 3> buttons_{};
    dpop::update::CheckResult lastCheck_{};
    bool checked_{false};
};

}
