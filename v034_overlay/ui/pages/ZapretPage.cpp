#include "ui/pages/ZapretPage.h"

#include "modules/ZapretManager.h"
#include "ui/Controls.h"
#include "ui/Theme.h"
#include "ui/pages/ZapretPageLayout.h"

#include <array>
#include <string>

namespace dpop::ui {
namespace {
constexpr int kStatusBase = 2800;
constexpr int kStrategyLabelId = 2810;
constexpr int kStrategyComboId = 2811;
constexpr int kButtonBase = 2820;

ButtonVisual ButtonStyle(int index) noexcept {
    if (index == 2) return ButtonVisual::Danger;
    if (index == 1 || index == 6) return ButtonVisual::Accent;
    return ButtonVisual::Normal;
}
}

bool ZapretPage::OnCreate() {
    if (!fonts_.Create()) return false;

    for (std::size_t index = 0; index < statusLabels_.size(); ++index) {
        statusLabels_[index] = CreateTextLabel(
            Hwnd(), kStatusBase + static_cast<int>(index), L"—", SS_LEFT | SS_NOPREFIX);
        if (!statusLabels_[index]) return false;
        ApplyControlFont(statusLabels_[index], index % 2 == 0 ? fonts_.section : fonts_.smallFont);
    }

    strategyLabel_ = CreateTextLabel(
        Hwnd(), kStrategyLabelId, L"Стратегия запуска (реальные general*.bat из bundle)", SS_LEFT | SS_NOPREFIX);
    strategyCombo_ = CreateDropDown(Hwnd(), kStrategyComboId);
    if (!strategyLabel_ || !strategyCombo_) return false;
    ApplyControlFont(strategyLabel_, fonts_.body);
    ApplyControlFont(strategyCombo_, fonts_.body);

    const std::array<std::wstring_view, 8> labels = {
        L"Обновить статус",
        L"Запустить выбранную",
        L"Остановить bundled winws",
        L"Service Manager",
        L"Default strategy",
        L"Открыть bundle",
        L"Исправление трансляций",
        L"Проверить обновление Zapret"
    };
    for (std::size_t index = 0; index < buttons_.size(); ++index) {
        buttons_[index] = CreatePushButton(
            Hwnd(), kButtonBase + static_cast<int>(index), labels[index], ButtonStyle(static_cast<int>(index)));
        if (!buttons_[index]) return false;
        ApplyControlFont(buttons_[index], fonts_.body);
    }

    ReloadStrategies();
    return true;
}

void ZapretPage::OnVisibilityChanged(bool visible) noexcept {
    if (visible) RefreshStatus();
}

void ZapretPage::OnLayout(int width, int height) noexcept {
    const UINT dpi = GetDpiForWindow(Hwnd());
    const auto regions = ComputeZapretPageLayout(width, height, dpi ? static_cast<int>(dpi) : 96);

    for (int card = 0; card < 3; ++card) {
        const auto& rect = regions.cards[static_cast<std::size_t>(card)];
        const int inset = 14;
        const int firstY = rect.y + 31;
        MoveWindow(
            statusLabels_[static_cast<std::size_t>(card * 2)],
            rect.x + inset, firstY, rect.width - inset * 2, 25, TRUE);
        MoveWindow(
            statusLabels_[static_cast<std::size_t>(card * 2 + 1)],
            rect.x + inset, firstY + 27, rect.width - inset * 2, 22, TRUE);
    }

    MoveWindow(
        strategyLabel_, regions.strategyLabel.x, regions.strategyLabel.y,
        regions.strategyLabel.width, regions.strategyLabel.height, TRUE);
    MoveWindow(
        strategyCombo_, regions.strategyCombo.x, regions.strategyCombo.y,
        regions.strategyCombo.width, 220, TRUE);

    for (std::size_t index = 0; index < buttons_.size(); ++index) {
        const auto& rect = regions.actions[index];
        MoveWindow(buttons_[index], rect.x, rect.y, rect.width, rect.height, TRUE);
    }
}

void ZapretPage::OnPaint(HDC dc, const RECT& client) noexcept {
    PageBase::OnPaint(dc, client);
    DrawPageHeading(
        dc, 18, 4,
        L"Zapret Center",
        L"Выбор bundled-стратегии, диагностика службы/winws и безопасные действия для Discord/RTC.",
        fonts_.title, fonts_.body);

    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const UINT dpi = GetDpiForWindow(Hwnd());
    const auto regions = ComputeZapretPageLayout(width, height, dpi ? static_cast<int>(dpi) : 96);

    DrawPanel(dc, RECT{
        regions.cards[0].x, regions.cards[0].y,
        regions.cards[0].x + regions.cards[0].width,
        regions.cards[0].y + regions.cards[0].height}, true);
    DrawPanel(dc, RECT{
        regions.cards[1].x, regions.cards[1].y,
        regions.cards[1].x + regions.cards[1].width,
        regions.cards[1].y + regions.cards[1].height}, false);
    DrawPanel(dc, RECT{
        regions.cards[2].x, regions.cards[2].y,
        regions.cards[2].x + regions.cards[2].width,
        regions.cards[2].y + regions.cards[2].height}, false);

    RECT service{
        regions.cards[0].x, regions.cards[0].y,
        regions.cards[0].x + regions.cards[0].width,
        regions.cards[0].y + regions.cards[0].height};
    RECT winws{
        regions.cards[1].x, regions.cards[1].y,
        regions.cards[1].x + regions.cards[1].width,
        regions.cards[1].y + regions.cards[1].height};
    RECT bundle{
        regions.cards[2].x, regions.cards[2].y,
        regions.cards[2].x + regions.cards[2].width,
        regions.cards[2].y + regions.cards[2].height};
    RECT diagnostic{
        regions.diagnostic.x, regions.diagnostic.y,
        regions.diagnostic.x + regions.diagnostic.width,
        regions.diagnostic.y + regions.diagnostic.height};

    DrawPanel(dc, diagnostic, false);
    DrawPanelTitle(dc, service, L"Служба", fonts_.section);
    DrawPanelTitle(dc, winws, L"winws", fonts_.section);
    DrawPanelTitle(dc, bundle, L"Bundle", fonts_.section);
    DrawPanelTitle(dc, diagnostic, L"Диагностика и доступные действия", fonts_.section);

    const auto& palette = MidnightPalette();
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, palette.muted);
    HGDIOBJ old = SelectObject(dc, fonts_.body);
    RECT text{
        diagnostic.left + 16,
        diagnostic.top + 38,
        diagnostic.right - 16,
        diagnostic.bottom - 10};
    std::wstring owned = diagnostic_;
    DrawTextW(
        dc, owned.data(), static_cast<int>(owned.size()), &text,
        DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(dc, old);
}

void ZapretPage::ReloadStrategies() {
    if (!strategyCombo_) return;
    strategies_ = dpop::zapret::EnumerateStrategies();
    SendMessageW(strategyCombo_, CB_RESETCONTENT, 0, 0);

    int selected = 0;
    if (strategies_.empty()) {
        const wchar_t* empty = L"general*.bat стратегии не найдены";
        SendMessageW(strategyCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(empty));
        EnableWindow(buttons_[1], FALSE);
        EnableWindow(buttons_[6], FALSE);
    } else {
        for (std::size_t index = 0; index < strategies_.size(); ++index) {
            const auto& strategy = strategies_[index];
            SendMessageW(
                strategyCombo_, CB_ADDSTRING, 0,
                reinterpret_cast<LPARAM>(strategy.displayName.c_str()));
            if (strategy.isDefault) selected = static_cast<int>(index);
        }
        EnableWindow(buttons_[1], TRUE);
        EnableWindow(buttons_[6], TRUE);
    }
    SendMessageW(strategyCombo_, CB_SETCURSEL, static_cast<WPARAM>(selected), 0);
}

const dpop::zapret::StrategyEntry* ZapretPage::SelectedStrategy() const noexcept {
    if (!strategyCombo_ || strategies_.empty()) return nullptr;
    const LRESULT selected = SendMessageW(strategyCombo_, CB_GETCURSEL, 0, 0);
    if (selected == CB_ERR || selected < 0 || selected >= static_cast<LRESULT>(strategies_.size())) return nullptr;
    return &strategies_[static_cast<std::size_t>(selected)];
}

void ZapretPage::RefreshStatus() {
    const auto status = dpop::zapret::QueryStatus();
    ReloadStrategies();

    SetControlText(
        statusLabels_[0],
        status.serviceInstalled
            ? (status.serviceRunning ? L"Установлена • работает" : L"Установлена • остановлена")
            : L"Не установлена");
    SetControlText(
        statusLabels_[1],
        status.serviceInstalled ? L"Состояние читается из Windows SCM" : L"Сервис Zapret отсутствует");
    SetControlText(statusLabels_[2], status.winwsRunning ? L"Работает" : L"Не запущен");
    SetControlText(
        statusLabels_[3],
        status.winwsRunning ? L"Запущен именно bundled winws.exe" : L"Bundled winws.exe не найден");
    SetControlText(statusLabels_[4], status.bundleValid ? L"Готов" : L"Неполный");
    SetControlText(
        statusLabels_[5],
        status.bundleFolder.empty() ? L"Папка не найдена" : status.bundleFolder.wstring());

    diagnostic_ = L"Папка bundle: " +
        (status.bundleFolder.empty() ? std::wstring(L"не найдена") : status.bundleFolder.wstring());
    if (!status.missingBundleFile.empty()) {
        diagnostic_ += L"\nНе хватает обязательного файла: " + status.missingBundleFile.wstring();
    } else if (status.bundleValid) {
        diagnostic_ += L"\nОбязательные bundled-файлы проверены.";
    }
    diagnostic_ += L"\nНайдено стратегий general*.bat: " + std::to_wstring(strategies_.size()) + L".";
    if (const auto* selected = SelectedStrategy()) {
        diagnostic_ += L" Выбрана: " + selected->displayName + L".";
    }
    diagnostic_ +=
        L"\n\nDPopCleaner запускает только скрипты из своего bundle. Остановка standalone winws "
        L"затрагивает только процесс с точным bundled-путём. Если Zapret работает как служба, "
        L"используй Service Manager. «Исправление трансляций» безопасно перезапускает только standalone bundle, "
        L"очищает DNS-кэш и повторно запускает выбранную стратегию; Windows Firewall/Defender не отключаются.";

    SetStatus(
        L"Zapret: " + std::wstring(status.bundleValid ? L"bundle готов" : L"bundle неполный") +
        (status.serviceRunning ? L" • service ON" : L" • service OFF") +
        (status.winwsRunning ? L" • winws ON" : L" • winws OFF"));
    InvalidateRect(Hwnd(), nullptr, TRUE);
}

bool ZapretPage::RunAction(int buttonIndex, std::wstring& error) {
    switch (buttonIndex) {
    case 1: {
        const auto* strategy = SelectedStrategy();
        if (!strategy) {
            error = L"Сначала выбери найденную bundled-стратегию.";
            return false;
        }
        return dpop::zapret::LaunchStrategy(strategy->relativeScript, error);
    }
    case 2:
        return dpop::zapret::StopBundledWinws(error);
    case 3:
        return dpop::zapret::OpenServiceManager(error);
    case 4:
        return dpop::zapret::LaunchDefaultStrategy(error);
    case 5:
        return dpop::zapret::OpenBundledFolder(error);
    case 6: {
        const auto* strategy = SelectedStrategy();
        if (!strategy) {
            error = L"Сначала выбери стратегию для RTC repair.";
            return false;
        }
        return dpop::zapret::RepairRtc(strategy->relativeScript, error);
    }
    case 7:
        return dpop::zapret::OpenZapretUpdatePage(error);
    default:
        error = L"Неизвестное действие Zapret.";
        return false;
    }
}

LRESULT ZapretPage::OnMessage(UINT message, WPARAM wParam, LPARAM lParam, bool& handled) {
    if (message == WM_COMMAND) {
        const int id = LOWORD(wParam);
        const int notification = HIWORD(wParam);
        if (id == kStrategyComboId && notification == CBN_SELCHANGE) {
            if (const auto* selected = SelectedStrategy()) {
                SetStatus(L"Выбрана стратегия: " + selected->displayName);
            }
            handled = true;
            return 0;
        }
        if (id == kButtonBase) {
            RefreshStatus();
            handled = true;
            return 0;
        }
        if (id >= kButtonBase + 1 && id < kButtonBase + static_cast<int>(buttons_.size())) {
            const int buttonIndex = id - kButtonBase;
            std::wstring error;
            const bool ok = RunAction(buttonIndex, error);
            if (ok) {
                SetStatus(error.empty() ? L"Действие Zapret выполнено/открыто." : error);
                Log(EventLevel::Info, error.empty() ? L"Действие Zapret выполнено." : error);
                if (buttonIndex != 5 && buttonIndex != 7) RefreshStatus();
            } else {
                SetStatus(error);
                Log(EventLevel::Warning, error);
            }
            handled = true;
            return 0;
        }
    }

    if (message == WM_DRAWITEM) {
        const auto* draw = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (draw && draw->CtlType == ODT_BUTTON &&
            draw->CtlID >= kButtonBase &&
            draw->CtlID < kButtonBase + static_cast<int>(buttons_.size())) {
            const int index = static_cast<int>(draw->CtlID) - kButtonBase;
            handled = DrawOwnerButton(
                *draw, GetControlText(draw->hwndItem), ButtonStyle(index));
            return handled ? TRUE : 0;
        }
    }

    handled = false;
    return 0;
}

}
