#include "ui/pages/WindowsPage.h"

#include "ui/Controls.h"
#include "ui/Theme.h"
#include "ui/PageLayout.h"

#include <array>
#include <string>

namespace dpop::ui {
namespace {
constexpr int kButtonBase = 2500;
constexpr int kOutput = 2520;
constexpr std::array<dpop::full::MaintenanceAction, 6> kActions = {
    dpop::full::MaintenanceAction::ClearUpdateCache,
    dpop::full::MaintenanceAction::ComponentCleanup,
    dpop::full::MaintenanceAction::ResetBase,
    dpop::full::MaintenanceAction::DismCheckHealth,
    dpop::full::MaintenanceAction::DismScanHealth,
    dpop::full::MaintenanceAction::DismRestoreHealth
};
}

bool WindowsPage::OnCreate() {
    if (!fonts_.Create()) return false;
    settings_ = dpop::full::LoadSettings();

    const std::array<std::wstring_view, 6> labels = {
        L"Очистить Update cache",
        L"StartComponentCleanup",
        L"StartComponentCleanup /ResetBase",
        L"DISM /CheckHealth",
        L"DISM /ScanHealth",
        L"DISM /RestoreHealth"
    };
    for (std::size_t i = 0; i < labels.size(); ++i) {
        const ButtonVisual visual = i == 0 || i == 1 || i == 3 ? ButtonVisual::Accent : i == 2 ? ButtonVisual::Danger : ButtonVisual::Normal;
        buttons_[i] = CreatePushButton(Hwnd(), kButtonBase + static_cast<int>(i), labels[i], visual);
        if (!buttons_[i]) return false;
        ApplyControlFont(buttons_[i], fonts_.smallFont);
    }

    output_ = CreateReadOnlyLogEdit(Hwnd(), kOutput);
    if (!output_) return false;
    ApplyControlFont(output_, fonts_.smallFont);
    SetControlText(output_, L"Результаты системных операций появятся здесь. DPopCleaner запускает штатные команды Windows и показывает их итоговый exit code.");
    return true;
}

void WindowsPage::OnVisibilityChanged(bool visible) noexcept {
    if (!visible) Cancel();
}

void WindowsPage::CheckUpdateCacheAtStartup() {
    if (IsBusy()) return;
    StartAsync(L"Проверяем кэш Windows Update при запуске…", [this](std::stop_token token) {
        const auto bytes = dpop::full::EstimateUpdateCacheBytes(token);
        QueueApply([this, bytes] {
            const std::wstring line = L"Windows Update cache при запуске: " + dpop::full::FormatBytes(bytes) + L". Автоочистка не выполнялась.";
            AppendResult(line);
            SetStatus(line);
            Log(EventLevel::Info, line);
        });
    });
}

void WindowsPage::OnLayout(int width, int height) noexcept {
    const int margin = 18;
    const int top = dpop::ui::ComputePageContentTop(GetDpiForWindow(Hwnd()));
    const int gap = 12;
    const int cardsH = 158;
    const int cardW = (width - margin * 2 - gap * 2) / 3;
    for (int card = 0; card < 3; ++card) {
        const int x = margin + card * (cardW + gap) + 14;
        const int w = cardW - 28;
        if (card == 0) {
            MoveWindow(buttons_[0], x, top + 44, w, 38, TRUE);
        } else if (card == 1) {
            MoveWindow(buttons_[1], x, top + 44, w, 38, TRUE);
            MoveWindow(buttons_[2], x, top + 88, w, 44, TRUE);
        } else {
            MoveWindow(buttons_[3], x, top + 38, w, 34, TRUE);
            MoveWindow(buttons_[4], x, top + 78, w, 34, TRUE);
            MoveWindow(buttons_[5], x, top + 118, w, 34, TRUE);
        }
    }
    MoveWindow(output_, margin + 10, top + cardsH + gap + 30, width - margin * 2 - 20, std::max(90, height - margin - (top + cardsH + gap + 40)), TRUE);
}

void WindowsPage::OnPaint(HDC dc, const RECT& client) noexcept {
    PageBase::OnPaint(dc, client);
    DrawPageHeading(dc, 18, 4, L"Windows", L"Windows Update, хранилище компонентов и проверка состояния DISM.", fonts_.title, fonts_.body);
    const int width = client.right;
    const int height = client.bottom;
    const int margin = 18;
    const int top = dpop::ui::ComputePageContentTop(GetDpiForWindow(Hwnd()));
    const int gap = 12;
    const int cardsH = 158;
    const int cardW = (width - margin * 2 - gap * 2) / 3;
    RECT update{margin, top, margin + cardW, top + cardsH};
    RECT component{margin + cardW + gap, top, margin + cardW * 2 + gap, top + cardsH};
    RECT health{margin + (cardW + gap) * 2, top, width - margin, top + cardsH};
    RECT output{margin, top + cardsH + gap, width - margin, height - margin};
    DrawPanel(dc, update, true);
    DrawPanel(dc, component, false);
    DrawPanel(dc, health, false);
    DrawPanel(dc, output, false);
    DrawPanelTitle(dc, update, L"Windows Update cache", fonts_.section);
    DrawPanelTitle(dc, component, L"Хранилище компонентов", fonts_.section);
    DrawPanelTitle(dc, health, L"DISM Health", fonts_.section);
    DrawPanelTitle(dc, output, L"Результат", fonts_.section);

    const auto& p = MidnightPalette();
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, p.muted);
    HGDIOBJ old = SelectObject(dc, fonts_.smallFont);
    RECT a{update.left + 14, update.top + 88, update.right - 14, update.bottom - 12};
    std::wstring ta = L"Очистка Download cache может потребовать UAC и временно остановит Windows Update/BITS.";
    DrawTextW(dc, ta.data(), static_cast<int>(ta.size()), &a, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
    RECT b{component.left + 14, component.top + 136, component.right - 14, component.bottom - 8};
    std::wstring tb = L"/ResetBase необратим для удаления заменённых обновлений.";
    SetTextColor(dc, p.warning);
    DrawTextW(dc, tb.data(), static_cast<int>(tb.size()), &b, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(dc, old);
}

void WindowsPage::AppendResult(std::wstring_view text) {
    std::wstring current = GetControlText(output_);
    if (!current.empty()) current += L"\r\n";
    current.append(text);
    SetControlText(output_, current);
    SendMessageW(output_, EM_SETSEL, current.size(), current.size());
    SendMessageW(output_, EM_SCROLLCARET, 0, 0);
}

void WindowsPage::Run(dpop::full::MaintenanceAction action) {
    if (action == dpop::full::MaintenanceAction::ResetBase) {
        if (!ConfirmAction(Hwnd(), L"/ResetBase необратимо удаляет возможность удаления заменённых компонентов Windows. После выполнения этот откат вернуть нельзя. Продолжить?", true)) return;
    }
    if (action == dpop::full::MaintenanceAction::ClearUpdateCache && settings_.confirmDestructive) {
        if (!ConfirmAction(Hwnd(), L"Остановить службы Windows Update/BITS, очистить Download cache и снова запустить службы?")) return;
    }
    const std::wstring label{dpop::full::MaintenanceLabel(action)};
    StartAsync(L"Windows выполняет: " + label, [this, action, label](std::stop_token token) {
        unsigned long exitCode = 0;
        std::wstring error;
        const bool ok = dpop::full::RunMaintenance(action, exitCode, error, token);
        QueueApply([this, ok, exitCode, label, error = std::move(error)] {
            const std::wstring line = ok
                ? label + L" — успешно, exit code " + std::to_wstring(exitCode)
                : label + L" — " + error;
            AppendResult(line);
            SetStatus(line);
            Log(ok ? EventLevel::Info : EventLevel::Error, line);
        });
    });
}

LRESULT WindowsPage::OnMessage(UINT message, WPARAM wParam, LPARAM lParam, bool& handled) {
    if (message == WM_COMMAND) {
        const int id = LOWORD(wParam);
        if (id >= kButtonBase && id < kButtonBase + static_cast<int>(kActions.size())) {
            Run(kActions[static_cast<std::size_t>(id - kButtonBase)]);
            handled = true;
            return 0;
        }
    }
    if (message == WM_DRAWITEM) {
        const auto* draw = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (draw && draw->CtlType == ODT_BUTTON && draw->CtlID >= kButtonBase && draw->CtlID < kButtonBase + 6) {
            ButtonVisual visual = draw->CtlID == kButtonBase + 2 ? ButtonVisual::Danger :
                                  (draw->CtlID == kButtonBase || draw->CtlID == kButtonBase + 1 || draw->CtlID == kButtonBase + 3) ? ButtonVisual::Accent : ButtonVisual::Normal;
            handled = DrawOwnerButton(*draw, GetControlText(draw->hwndItem), visual);
            return handled ? TRUE : 0;
        }
    }
    handled = false;
    return 0;
}

}
