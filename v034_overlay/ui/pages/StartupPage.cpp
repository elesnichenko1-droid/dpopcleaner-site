#include "ui/pages/StartupPage.h"

#include "ui/Controls.h"
#include "ui/Theme.h"

#include <shellapi.h>
#include <algorithm>
#include <string>

namespace dpop::ui {
namespace {
constexpr int kCount = 3100;
constexpr int kList = 3101;
constexpr int kButtonBase = 3120;
}

bool StartupPage::OnCreate() {
    if (!fonts_.Create()) return false;
    count_ = CreateTextLabel(Hwnd(), kCount, L"0 записей", SS_RIGHT | SS_NOPREFIX);
    list_ = CreateDarkListView(Hwnd(), kList);
    if (!count_ || !list_) return false;
    ApplyControlFont(count_, fonts_.smallFont);
    ApplyControlFont(list_, fonts_.smallFont);

    ResetList(list_);
    AddListColumn(list_, 0, L"Приложение", 230);
    AddListColumn(list_, 1, L"Источник", 190);
    AddListColumn(list_, 2, L"Команда / файл", 520);

    const std::wstring_view labels[] = {
        L"Обновить список", L"Открыть расположение", L"Автозагрузка Windows"
    };
    for (int i = 0; i < 3; ++i) {
        buttons_[static_cast<std::size_t>(i)] = CreatePushButton(
            Hwnd(), kButtonBase + i, labels[i], i == 0 ? ButtonVisual::Accent : ButtonVisual::Normal);
        if (!buttons_[static_cast<std::size_t>(i)]) return false;
        ApplyControlFont(buttons_[static_cast<std::size_t>(i)], fonts_.body);
    }
    return true;
}

void StartupPage::OnVisibilityChanged(bool visible) noexcept {
    if (!visible) { Cancel(); return; }
    if (!loaded_ && !IsBusy()) Refresh();
}

void StartupPage::OnLayout(int width, int height) noexcept {
    const int margin = 18;
    const int top = 68;
    const int actionH = 36;
    const int gap = 10;
    MoveWindow(count_, width - margin - 150, 24, 150, 28, TRUE);
    const int buttonsY = height - margin - actionH;
    const int buttonW = std::max(150, (width - margin * 2 - gap * 2) / 3);
    for (int i = 0; i < 3; ++i) {
        MoveWindow(buttons_[static_cast<std::size_t>(i)], margin + i * (buttonW + gap), buttonsY, buttonW, actionH, TRUE);
    }
    MoveWindow(list_, margin, top, width - margin * 2, std::max(120, buttonsY - gap - top), TRUE);
}

void StartupPage::OnPaint(HDC dc, const RECT& client) noexcept {
    PageBase::OnPaint(dc, client);
    DrawPageHeading(
        dc, 18, 4,
        L"Автозагрузка",
        L"Реальные записи HKCU/HKLM Run и папок Startup пользователя и системы.",
        fonts_.title, fonts_.body);
    RECT panel{18, 58, client.right - 18, client.bottom - 62};
    DrawPanel(dc, panel, false);
    DrawPanelTitle(dc, panel, L"Записи запуска Windows", fonts_.section);
}

void StartupPage::Refresh() {
    StartAsync(L"Читаем автозагрузку Windows…", [this](std::stop_token) {
        auto entries = dpop::startup::EnumerateAll();
        QueueApply([this, entries = std::move(entries)]() mutable {
            entries_ = std::move(entries);
            loaded_ = true;
            ListView_DeleteAllItems(list_);
            for (const auto& entry : entries_) {
                AddListRow(list_, {entry.name, entry.source, entry.command});
            }
            SetControlText(count_, std::to_wstring(entries_.size()) + L" записей");
            SetStatus(L"Автозагрузка: " + std::to_wstring(entries_.size()) + L" записей.");
            Log(EventLevel::Info, L"Список автозагрузки обновлён.");
        });
    });
}

void StartupPage::OpenSelected() {
    const int row = SelectedListIndex(list_);
    if (row < 0 || row >= static_cast<int>(entries_.size())) {
        SetStatus(L"Сначала выберите запись автозагрузки.");
        return;
    }
    const auto& entry = entries_[static_cast<std::size_t>(row)];
    if (!entry.location.empty()) {
        OpenPathInExplorer(Hwnd(), entry.location);
        SetStatus(L"Открыто расположение выбранной записи.");
        return;
    }
    OpenWindowsStartup();
    SetStatus(L"Для Run-записи открыта системная страница управления автозагрузкой.");
}

void StartupPage::OpenWindowsStartup() {
    const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(
        Hwnd(), L"open", L"ms-settings:startupapps", nullptr, nullptr, SW_SHOWNORMAL));
    if (result <= 32) {
        ShellExecuteW(Hwnd(), L"open", L"taskmgr.exe", nullptr, nullptr, SW_SHOWNORMAL);
    }
}

LRESULT StartupPage::OnMessage(UINT message, WPARAM wParam, LPARAM lParam, bool& handled) {
    if (message == WM_COMMAND) {
        const int id = LOWORD(wParam);
        if (id == kButtonBase) { Refresh(); handled = true; return 0; }
        if (id == kButtonBase + 1) { OpenSelected(); handled = true; return 0; }
        if (id == kButtonBase + 2) { OpenWindowsStartup(); SetStatus(L"Открыто управление автозагрузкой Windows."); handled = true; return 0; }
    }
    if (message == WM_DRAWITEM) {
        const auto* draw = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (draw && draw->CtlType == ODT_BUTTON && draw->CtlID >= kButtonBase && draw->CtlID < kButtonBase + 3) {
            handled = DrawOwnerButton(*draw, GetControlText(draw->hwndItem), draw->CtlID == kButtonBase ? ButtonVisual::Accent : ButtonVisual::Normal);
            return handled ? TRUE : 0;
        }
    }
    handled = false;
    return 0;
}

}
