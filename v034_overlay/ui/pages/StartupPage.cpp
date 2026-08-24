#include "ui/pages/StartupPage.h"

#include "ui/Controls.h"
#include "ui/PageLayout.h"
#include "ui/Theme.h"

#include <shellapi.h>
#include <algorithm>
#include <string>
#include <vector>

namespace dpop::ui {
namespace {
constexpr int kCount = 3100;
constexpr int kList = 3101;
constexpr int kButtonBase = 3120;

std::wstring StateText(const dpop::startup::Entry& entry) {
    return entry.enabled ? L"Включено" : L"Отключено";
}
}

bool StartupPage::OnCreate() {
    if (!fonts_.Create()) return false;
    count_ = CreateTextLabel(Hwnd(), kCount, L"0 записей", SS_RIGHT | SS_NOPREFIX);
    list_ = CreateDarkListView(Hwnd(), kList);
    if (!count_ || !list_) return false;
    ApplyControlFont(count_, fonts_.smallFont);
    ApplyControlFont(list_, fonts_.smallFont);

    ResetList(list_);
    AddListColumn(list_, 0, L"Приложение", 190);
    AddListColumn(list_, 1, L"Состояние", 95);
    AddListColumn(list_, 2, L"Тип", 145);
    AddListColumn(list_, 3, L"Рекомендация", 220);
    AddListColumn(list_, 4, L"Источник", 160);
    AddListColumn(list_, 5, L"Команда / файл", 420);

    SHFILEINFOW sfi{};
    systemImages_ = reinterpret_cast<HIMAGELIST>(SHGetFileInfoW(
        L"C:\\", FILE_ATTRIBUTE_DIRECTORY, &sfi, sizeof(sfi),
        SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES));
    if (systemImages_) ListView_SetImageList(list_, systemImages_, LVSIL_SMALL);

    const std::array<std::wstring_view, 5> labels = {
        L"Обновить", L"Отключить / включить", L"Открыть расположение", L"Адаптировать", L"Автозагрузка Windows"
    };
    for (std::size_t i = 0; i < labels.size(); ++i) {
        buttons_[i] = CreatePushButton(
            Hwnd(), kButtonBase + static_cast<int>(i), labels[i], i == 0 ? ButtonVisual::Accent : ButtonVisual::Normal);
        if (!buttons_[i]) return false;
        ApplyControlFont(buttons_[i], fonts_.smallFont);
    }
    EnableWindow(buttons_[1], FALSE);
    return true;
}

void StartupPage::OnVisibilityChanged(bool visible) noexcept {
    if (!visible) { Cancel(); return; }
    if (!loaded_ && !IsBusy()) Refresh();
}

void StartupPage::OnLayout(int width, int height) noexcept {
    const UINT dpiRaw = GetDpiForWindow(Hwnd());
    const int dpi = dpiRaw ? static_cast<int>(dpiRaw) : 96;
    const int margin = 18;
    const int top = ComputePageContentTop(dpi);
    const int gap = 8;
    const int actionH = 34;
    const int actionRows = 2;
    const int actionArea = actionRows * actionH + gap;
    const int buttonsTop = std::max(top + 160, height - margin - actionArea);

    MoveWindow(count_, std::max(margin, width - margin - 180), 18, 180, 24, TRUE);
    MoveWindow(list_, margin + 10, top + 28, std::max(120, width - margin * 2 - 20),
        std::max(120, buttonsTop - gap - (top + 28)), TRUE);

    const int innerW = std::max(100, width - margin * 2);
    const int colW = std::max(120, (innerW - gap * 2) / 3);
    for (int i = 0; i < 3; ++i) {
        MoveWindow(buttons_[static_cast<std::size_t>(i)], margin + i * (colW + gap), buttonsTop, colW, actionH, TRUE);
    }
    const int secondW = std::max(150, (innerW - gap) / 2);
    MoveWindow(buttons_[3], margin, buttonsTop + actionH + gap, secondW, actionH, TRUE);
    MoveWindow(buttons_[4], margin + secondW + gap, buttonsTop + actionH + gap,
        std::max(120, innerW - secondW - gap), actionH, TRUE);
}

void StartupPage::OnPaint(HDC dc, const RECT& client) noexcept {
    PageBase::OnPaint(dc, client);
    DrawPageHeading(
        dc, 18, 4,
        L"Автозагрузка",
        L"Иконки, классификация и безопасное управление: системные и vendor-записи защищены от случайного отключения.",
        fonts_.title, fonts_.body);
    const UINT dpiRaw = GetDpiForWindow(Hwnd());
    const int top = ComputePageContentTop(dpiRaw ? static_cast<int>(dpiRaw) : 96);
    RECT panel{18, top, client.right - 18, client.bottom - 100};
    DrawPanel(dc, panel, false);
    DrawPanelTitle(dc, panel, L"Записи запуска Windows", fonts_.section);
}

int StartupPage::IconIndexFor(const dpop::startup::Entry& entry) {
    std::filesystem::path path = !entry.executable.empty() ? entry.executable : entry.location;
    if (path.empty()) return 0;
    SHFILEINFOW sfi{};
    std::error_code ec;
    const bool exists = std::filesystem::exists(path, ec);
    UINT flags = SHGFI_SYSICONINDEX | SHGFI_SMALLICON;
    DWORD attrs = 0;
    if (!exists) {
        flags |= SHGFI_USEFILEATTRIBUTES;
        attrs = FILE_ATTRIBUTE_NORMAL;
    }
    if (!SHGetFileInfoW(path.c_str(), attrs, &sfi, sizeof(sfi), flags)) return 0;
    return sfi.iIcon;
}

void StartupPage::Refresh() {
    StartAsync(L"Читаем и классифицируем автозагрузку Windows…", [this](std::stop_token) {
        auto entries = dpop::startup::EnumerateAll();
        QueueApply([this, entries = std::move(entries)]() mutable {
            entries_ = std::move(entries);
            loaded_ = true;
            ListView_DeleteAllItems(list_);
            unsigned protectedCount = 0;
            unsigned disabledCount = 0;
            for (const auto& entry : entries_) {
                if (entry.protectedEntry) ++protectedCount;
                if (!entry.enabled) ++disabledCount;
                const int row = AddListRow(list_, {
                    entry.name,
                    StateText(entry),
                    entry.category,
                    entry.recommendation,
                    entry.source,
                    entry.command
                });
                if (row >= 0 && systemImages_) {
                    LVITEMW item{};
                    item.mask = LVIF_IMAGE;
                    item.iItem = row;
                    item.iImage = IconIndexFor(entry);
                    ListView_SetItem(list_, &item);
                }
            }
            SetControlText(count_, std::to_wstring(entries_.size()) + L" записей • защищено " +
                std::to_wstring(protectedCount) + L" • отключено " + std::to_wstring(disabledCount));
            SetStatus(L"Автозагрузка классифицирована. Системные записи не меняются автоматически.");
            Log(EventLevel::Info, L"Список автозагрузки обновлён и классифицирован.");
            UpdateActionState();
        });
    });
}

void StartupPage::UpdateActionState() {
    const int row = SelectedListIndex(list_);
    if (row < 0 || row >= static_cast<int>(entries_.size())) {
        EnableWindow(buttons_[1], FALSE);
        SetControlText(buttons_[1], L"Отключить / включить");
        return;
    }
    const auto& entry = entries_[static_cast<std::size_t>(row)];
    EnableWindow(buttons_[1], entry.manageable ? TRUE : FALSE);
    SetControlText(buttons_[1], entry.enabled ? L"Отключить выбранное" : L"Включить выбранное");
}

void StartupPage::ToggleSelected() {
    const int row = SelectedListIndex(list_);
    if (row < 0 || row >= static_cast<int>(entries_.size())) {
        SetStatus(L"Сначала выберите запись автозагрузки.");
        return;
    }
    const auto entry = entries_[static_cast<std::size_t>(row)];
    if (!entry.manageable) {
        const std::wstring reason = entry.protectedEntry
            ? L"Эта запись помечена как системная/vendor и защищена от автоматического изменения."
            : L"DPopCleaner не меняет этот тип записи напрямую. Используйте штатное управление Windows.";
        MessageBoxW(Hwnd(), reason.c_str(), L"Безопасность автозагрузки", MB_OK | MB_ICONINFORMATION);
        SetStatus(reason);
        return;
    }
    const bool next = !entry.enabled;
    if (!next && !ConfirmAction(Hwnd(), L"Отключить выбранную пользовательскую запись? DPopCleaner сохранит резерв для восстановления.", true)) return;
    std::wstring error;
    if (!dpop::startup::SetEnabled(entry, next, error)) {
        SetStatus(error);
        Log(EventLevel::Error, error);
        return;
    }
    SetStatus(next ? L"Запись автозагрузки восстановлена." : L"Запись отключена с резервной копией DPopCleaner.");
    Log(EventLevel::Info, next ? L"Запись автозагрузки включена." : L"Запись автозагрузки отключена.");
    Refresh();
}

void StartupPage::OpenSelected() {
    const int row = SelectedListIndex(list_);
    if (row < 0 || row >= static_cast<int>(entries_.size())) {
        SetStatus(L"Сначала выберите запись автозагрузки.");
        return;
    }
    const auto& entry = entries_[static_cast<std::size_t>(row)];
    std::error_code ec;
    if (!entry.executable.empty() && std::filesystem::exists(entry.executable, ec)) {
        OpenPathInExplorer(Hwnd(), entry.executable, true);
        SetStatus(L"Открыт файл выбранной записи.");
        return;
    }
    if (!entry.location.empty()) {
        OpenPathInExplorer(Hwnd(), entry.location, true);
        SetStatus(L"Открыто расположение выбранной записи.");
        return;
    }
    OpenWindowsStartup();
    SetStatus(L"Путь исполняемого файла не определён; открыто штатное управление автозагрузкой Windows.");
}

void StartupPage::ShowAdaptation() {
    unsigned safeCandidates = 0;
    unsigned protectedCount = 0;
    unsigned disabledCount = 0;
    std::vector<std::wstring> examples;
    for (const auto& entry : entries_) {
        if (!entry.enabled) { ++disabledCount; continue; }
        if (entry.protectedEntry) { ++protectedCount; continue; }
        if (entry.manageable) {
            ++safeCandidates;
            if (examples.size() < 6) examples.push_back(entry.name);
        }
    }
    std::wstring text = L"DPopCleaner не отключает автозагрузку автоматически.\n\n";
    text += L"Пользовательских кандидатов для ручной оптимизации: " + std::to_wstring(safeCandidates) + L"\n";
    text += L"Защищённых системных/vendor-записей: " + std::to_wstring(protectedCount) + L"\n";
    text += L"Уже отключено через DPopCleaner: " + std::to_wstring(disabledCount) + L"\n";
    if (!examples.empty()) {
        text += L"\nМожно рассмотреть, если не нужны сразу после входа:\n";
        for (const auto& name : examples) text += L"• " + name + L"\n";
    }
    text += L"\nСистемные компоненты, драйверы и HKLM-записи оставлены без изменений.";
    MessageBoxW(Hwnd(), text.c_str(), L"Адаптация автозагрузки", MB_OK | MB_ICONINFORMATION);
    SetStatus(L"Анализ автозагрузки завершён без автоматических изменений.");
}

void StartupPage::OpenWindowsStartup() {
    const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(
        Hwnd(), L"open", L"ms-settings:startupapps", nullptr, nullptr, SW_SHOWNORMAL));
    if (result <= 32) ShellExecuteW(Hwnd(), L"open", L"taskmgr.exe", nullptr, nullptr, SW_SHOWNORMAL);
}

LRESULT StartupPage::OnMessage(UINT message, WPARAM wParam, LPARAM lParam, bool& handled) {
    if (message == WM_COMMAND) {
        const int id = LOWORD(wParam);
        if (id == kButtonBase + 0) { Refresh(); handled = true; return 0; }
        if (id == kButtonBase + 1) { ToggleSelected(); handled = true; return 0; }
        if (id == kButtonBase + 2) { OpenSelected(); handled = true; return 0; }
        if (id == kButtonBase + 3) { ShowAdaptation(); handled = true; return 0; }
        if (id == kButtonBase + 4) { OpenWindowsStartup(); SetStatus(L"Открыто управление автозагрузкой Windows."); handled = true; return 0; }
    }
    if (message == WM_NOTIFY) {
        const auto* hdr = reinterpret_cast<const NMHDR*>(lParam);
        if (hdr && hdr->hwndFrom == list_ && (hdr->code == LVN_ITEMCHANGED || hdr->code == NM_CLICK)) {
            UpdateActionState();
        }
    }
    if (message == WM_DRAWITEM) {
        const auto* draw = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (draw && draw->CtlType == ODT_BUTTON && draw->CtlID >= kButtonBase && draw->CtlID < kButtonBase + 5) {
            const ButtonVisual visual = draw->CtlID == kButtonBase ? ButtonVisual::Accent : ButtonVisual::Normal;
            handled = DrawOwnerButton(*draw, GetControlText(draw->hwndItem), visual);
            return handled ? TRUE : 0;
        }
    }
    handled = false;
    return 0;
}

}
