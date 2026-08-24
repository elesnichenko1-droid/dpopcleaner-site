#pragma once
#include "ui/PageBase.h"
#include "ui/RecoveryControls.h"
#include "modules/ZapretCenterModel.h"

#include <array>
#include <vector>

namespace dpop::ui {

class ZapretPage final : public PageBase {
public:
    ~ZapretPage() override { Destroy(); }

    bool Create(HWND parent, SessionLog& log) {
        return PageBase::Create(parent, log, L"Zapret");
    }

protected:
    bool OnCreate() override;
    void OnLayout(int width, int height) noexcept override;
    LRESULT OnMessage(UINT message, WPARAM wParam, LPARAM lParam, bool& handled) override;
    void OnPaint(HDC dc, const RECT& client) noexcept override;
    void OnVisibilityChanged(bool visible) noexcept override;

private:
    void RefreshStatus();
    void ReloadStrategies();
    void UpdateZapret();
    const dpop::zapret::StrategyEntry* SelectedStrategy() const noexcept;
    bool RunAction(int buttonIndex, std::wstring& error);

    RecoveryFonts fonts_;
    std::array<HWND, 6> statusLabels_{};
    HWND strategyLabel_{};
    HWND strategyCombo_{};
    std::array<HWND, 8> buttons_{};
    std::vector<dpop::zapret::StrategyEntry> strategies_;
    std::wstring diagnostic_{L"Состояние bundled Zapret ещё не проверено."};
};

}
