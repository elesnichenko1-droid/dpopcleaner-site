#include "ui/pages/SettingsPage.h"

#include "ui/Controls.h"
#include "ui/PageLayout.h"
#include "ui/Theme.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cwctype>
#include <filesystem>
#include <string>

namespace dpop::ui {
namespace {
constexpr int kLabelBase = 2900;
constexpr int kCheckBase = 2920;
constexpr int kExclusions = 2940;
constexpr int kExclusionButtonBase = 2950;
constexpr int kLargeFile = 2960;
constexpr int kDuplicateMin = 2961;
constexpr int kMemoryTrim = 2962;
constexpr int kSave = 2970;
constexpr int kReload = 2971;

bool ParseThreshold(HWND edit, unsigned minValue, unsigned maxValue, unsigned& value) {
    const std::wstring text = GetControlText(edit);
    if (text.empty()) return false;
    wchar_t* end = nullptr;
    const unsigned long parsed = wcstoul(text.c_str(), &end, 10);
    if (!end || *end != L'\0' || parsed < minValue || parsed > maxValue) return false;
    value = static_cast<unsigned>(parsed);
    return true;
}

bool ContainsPath(const std::vector<std::wstring>& values, const std::wstring& value) {
    const auto normalize = [](std::wstring s) {
        std::replace(s.begin(), s.end(), L'/', L'\\');
        std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
        while (s.size() > 3 && !s.empty() && s.back() == L'\\') s.pop_back();
        return s;
    };
    const auto wanted = normalize(value);
    return std::any_of(values.begin(), values.end(), [&](const std::wstring& current) {
        return normalize(current) == wanted;
    });
}
}

bool SettingsPage::OnCreate() {
    if (!fonts_.Create()) return false;

    const std::array<std::wstring_view, 4> labels = {
        L"Язык: Русский   •   Тема: Midnight",
        L"Порог крупных файлов, МБ",
        L"Мин. размер дубликата, МБ",
        L"Автоочистка RAM при использовании, %"
    };
    for (std::size_t i = 0; i < labels.size(); ++i) {
        labels_[i] = CreateTextLabel(Hwnd(), kLabelBase + static_cast<int>(i), labels[i], SS_LEFT | SS_NOPREFIX);
        if (!labels_[i]) return false;
        ApplyControlFont(labels_[i], i == 0 ? fonts_.body : fonts_.smallFont);
    }

    const std::array<std::wstring_view, 8> checks = {
        L"Подтверждать опасные действия",
        L"Фоновый контроль мусора каждые 30 минут",
        L"Quick DPopGuard-скан при запуске",
        L"Проверять кэш Windows Update при запуске",
        L"Проверять обновления DPopCleaner при запуске",
        L"Работать в трее и отслеживать новые установки",
        L"Запускать DPopCleaner вместе с Windows",
        L"Всегда запускать DPopCleaner от администратора"
    };
    for (std::size_t i = 0; i < checks.size(); ++i) {
        checks_[i] = CreateCheckBox(Hwnd(), kCheckBase + static_cast<int>(i), checks[i], false);
        if (!checks_[i]) return false;
        ApplyControlFont(checks_[i], fonts_.body);
    }

    exclusions_ = CreateDarkListView(Hwnd(), kExclusions);
    if (!exclusions_) return false;
    ApplyControlFont(exclusions_, fonts_.smallFont);
    ResetList(exclusions_);
    AddListColumn(exclusions_, 0, L"Файл / папка, исключённые из очистки", 520);

    const std::array<std::wstring_view, 3> exclusionLabels = {L"Добавить файл", L"Добавить папку", L"Удалить из списка"};
    for (std::size_t i = 0; i < exclusionButtons_.size(); ++i) {
        exclusionButtons_[i] = CreatePushButton(Hwnd(), kExclusionButtonBase + static_cast<int>(i), exclusionLabels[i], ButtonVisual::Normal);
        if (!exclusionButtons_[i]) return false;
        ApplyControlFont(exclusionButtons_[i], fonts_.smallFont);
    }

    largeFile_ = CreateDarkEdit(Hwnd(), kLargeFile, L"500");
    duplicateMin_ = CreateDarkEdit(Hwnd(), kDuplicateMin, L"10");
    memoryTrim_ = CreateDarkEdit(Hwnd(), kMemoryTrim, L"80");
    save_ = CreatePushButton(Hwnd(), kSave, L"Сохранить и применить", ButtonVisual::Accent);
    reload_ = CreatePushButton(Hwnd(), kReload, L"Перечитать", ButtonVisual::Normal);
    if (!largeFile_ || !duplicateMin_ || !memoryTrim_ || !save_ || !reload_) return false;
    for (HWND h : {largeFile_, duplicateMin_, memoryTrim_, save_, reload_}) ApplyControlFont(h, fonts_.body);

    Load();
    return true;
}

void SettingsPage::OnVisibilityChanged(bool visible) noexcept {
    if (visible) Load();
}

void SettingsPage::RefreshExclusions() {
    ListView_DeleteAllItems(exclusions_);
    for (const auto& value : settings_.cleanExclusions) AddListRow(exclusions_, {value});
}

void SettingsPage::Load() {
    settings_ = dpop::full::LoadSettings();
    SetChecked(checks_[0], settings_.confirmDestructive);
    SetChecked(checks_[1], settings_.backgroundJunkMonitor);
    SetChecked(checks_[2], settings_.quickGuardAtStartup);
    SetChecked(checks_[3], settings_.checkUpdateCacheAtStartup);
    SetChecked(checks_[4], settings_.checkUpdatesAtStartup);
    SetChecked(checks_[5], settings_.minimizeToTray && settings_.monitorInstallations);
    SetChecked(checks_[6], settings_.runAtStartup);
    SetChecked(checks_[7], settings_.alwaysRunAsAdmin);
    SetControlText(largeFile_, std::to_wstring(settings_.largeFileMB));
    SetControlText(duplicateMin_, std::to_wstring(settings_.duplicateMinMB));
    SetControlText(memoryTrim_, std::to_wstring(settings_.memoryAutoTrimPercent));
    RefreshExclusions();
    SetStatus(L"Настройки загружены: " + dpop::full::SettingsPath().wstring());
    InvalidateRect(Hwnd(), nullptr, TRUE);
}

void SettingsPage::AddFileExclusion() {
    const auto file = ChooseFile(Hwnd(), L"Выберите файл, который нельзя удалять при очистке");
    if (file.empty()) return;
    const std::wstring value = file.wstring();
    if (!ContainsPath(settings_.cleanExclusions, value)) settings_.cleanExclusions.push_back(value);
    RefreshExclusions();
    SetStatus(L"Файл добавлен в исключения. Нажмите «Сохранить и применить».");
}

void SettingsPage::AddFolderExclusion() {
    const auto folder = ChooseFolder(Hwnd(), L"Выберите папку, которую нельзя очищать");
    if (folder.empty()) return;
    const std::wstring value = folder.wstring();
    if (!ContainsPath(settings_.cleanExclusions, value)) settings_.cleanExclusions.push_back(value);
    RefreshExclusions();
    SetStatus(L"Папка добавлена в исключения. Нажмите «Сохранить и применить».");
}

void SettingsPage::RemoveExclusion() {
    const int index = SelectedListIndex(exclusions_);
    if (index < 0 || index >= static_cast<int>(settings_.cleanExclusions.size())) {
        SetStatus(L"Выберите исключение, которое нужно удалить.");
        return;
    }
    settings_.cleanExclusions.erase(settings_.cleanExclusions.begin() + index);
    RefreshExclusions();
    SetStatus(L"Исключение удалено из списка. Нажмите «Сохранить и применить».");
}

void SettingsPage::Save() {
    unsigned large = 0, duplicate = 0, memory = 0;
    if (!ParseThreshold(largeFile_, 50, 4096, large) ||
        !ParseThreshold(duplicateMin_, 1, 1024, duplicate) ||
        !ParseThreshold(memoryTrim_, 50, 98, memory)) {
        MessageBoxW(Hwnd(), L"Проверьте числовые пороги: крупные файлы 50–4096 МБ, дубликаты 1–1024 МБ, RAM 50–98%.", L"DPopCleaner", MB_OK | MB_ICONWARNING);
        return;
    }

    const bool nextStartup = IsChecked(checks_[6]);
    const bool nextAdmin = IsChecked(checks_[7]);
    std::wstring error;
    if (nextStartup != settings_.runAtStartup && !dpop::full::SetRunAtStartup(nextStartup, error)) {
        SetStatus(error); Log(EventLevel::Error, error); return;
    }
    if (nextAdmin != settings_.alwaysRunAsAdmin && !dpop::full::SetAlwaysRunAsAdmin(nextAdmin, error)) {
        SetStatus(error); Log(EventLevel::Error, error); return;
    }

    settings_.confirmDestructive = IsChecked(checks_[0]);
    settings_.backgroundJunkMonitor = IsChecked(checks_[1]);
    settings_.quickGuardAtStartup = IsChecked(checks_[2]);
    settings_.checkUpdateCacheAtStartup = IsChecked(checks_[3]);
    settings_.checkUpdatesAtStartup = IsChecked(checks_[4]);
    settings_.minimizeToTray = IsChecked(checks_[5]);
    settings_.monitorInstallations = IsChecked(checks_[5]);
    settings_.runAtStartup = nextStartup;
    settings_.alwaysRunAsAdmin = nextAdmin;
    settings_.largeFileMB = large;
    settings_.duplicateMinMB = duplicate;
    settings_.memoryAutoTrimPercent = memory;

    if (!dpop::full::SaveSettings(settings_, error)) {
        SetStatus(error); Log(EventLevel::Error, error); return;
    }
    SetStatus(L"Настройки сохранены и применены. Часть параметров запуска вступит в силу при следующем старте.");
    Log(EventLevel::Info, L"Расширенные настройки R2 сохранены.");
}

void SettingsPage::OnLayout(int width, int height) noexcept {
    const UINT dpiRaw = GetDpiForWindow(Hwnd());
    const int dpi = dpiRaw ? static_cast<int>(dpiRaw) : 96;
    const int top = ComputePageContentTop(dpi);
    const int margin = 18;
    const int gap = 12;
    const int bottomActions = 48;
    const int availableH = std::max(320, height - top - margin - bottomActions);
    const int leftW = std::max(390, (width - margin * 2 - gap) * 48 / 100);
    const int rightX = margin + leftW + gap;
    const int rightW = std::max(320, width - margin - rightX);

    MoveWindow(labels_[0], margin + 16, top + 34, leftW - 32, 24, TRUE);
    const int checkTop = top + 68;
    const int rowH = 30;
    for (int i = 0; i < 8; ++i) {
        MoveWindow(checks_[static_cast<std::size_t>(i)], margin + 16, checkTop + i * rowH, leftW - 32, 26, TRUE);
    }

    const int exclusionPanelH = std::max(240, availableH * 62 / 100);
    MoveWindow(exclusions_, rightX + 14, top + 34, rightW - 28, std::max(120, exclusionPanelH - 88), TRUE);
    const int exButtonY = top + exclusionPanelH - 44;
    const int exGap = 8;
    const int exW = std::max(94, (rightW - 28 - exGap * 2) / 3);
    for (int i = 0; i < 3; ++i) {
        MoveWindow(exclusionButtons_[static_cast<std::size_t>(i)], rightX + 14 + i * (exW + exGap), exButtonY, exW, 30, TRUE);
    }

    const int thresholdTop = top + exclusionPanelH + gap;
    const int editW = 72;
    MoveWindow(labels_[1], rightX + 16, thresholdTop + 38, rightW - 116, 24, TRUE);
    MoveWindow(largeFile_, rightX + rightW - editW - 16, thresholdTop + 34, editW, 30, TRUE);
    MoveWindow(labels_[2], rightX + 16, thresholdTop + 72, rightW - 116, 24, TRUE);
    MoveWindow(duplicateMin_, rightX + rightW - editW - 16, thresholdTop + 68, editW, 30, TRUE);
    MoveWindow(labels_[3], rightX + 16, thresholdTop + 106, rightW - 116, 24, TRUE);
    MoveWindow(memoryTrim_, rightX + rightW - editW - 16, thresholdTop + 102, editW, 30, TRUE);

    const int actionY = height - margin - 38;
    MoveWindow(save_, margin, actionY, 190, 36, TRUE);
    MoveWindow(reload_, margin + 202, actionY, 130, 36, TRUE);
}

void SettingsPage::OnPaint(HDC dc, const RECT& client) noexcept {
    PageBase::OnPaint(dc, client);
    DrawPageHeading(dc, 18, 4, L"Настройки", L"Параметры 0.2.x возвращены как реальные функции: запуск, защита, обновления, трей и исключения очистки.", fonts_.title, fonts_.body);

    const UINT dpiRaw = GetDpiForWindow(Hwnd());
    const int top = ComputePageContentTop(dpiRaw ? static_cast<int>(dpiRaw) : 96);
    const int margin = 18;
    const int gap = 12;
    const int clientWidth = static_cast<int>(client.right - client.left);
    const int clientHeight = static_cast<int>(client.bottom - client.top);
    const int leftW = std::max(390, (clientWidth - margin * 2 - gap) * 48 / 100);
    const int rightX = margin + leftW + gap;
    const int rightW = clientWidth - margin - rightX;
    const int usableBottom = clientHeight - margin - 48;
    const int availableH = std::max(320, usableBottom - top);
    const int exclusionPanelH = std::max(240, availableH * 62 / 100);

    RECT general{margin, top, margin + leftW, usableBottom};
    RECT exclusions{rightX, top, clientWidth - margin, top + exclusionPanelH};
    RECT thresholds{rightX, top + exclusionPanelH + gap, clientWidth - margin, usableBottom};
    DrawPanel(dc, general, true);
    DrawPanel(dc, exclusions, false);
    DrawPanel(dc, thresholds, false);
    DrawPanelTitle(dc, general, L"Основное", fonts_.section);
    DrawPanelTitle(dc, exclusions, L"Исключения очистки", fonts_.section);
    DrawPanelTitle(dc, thresholds, L"Пороги и автоматика", fonts_.section);
}

LRESULT SettingsPage::OnMessage(UINT message, WPARAM wParam, LPARAM lParam, bool& handled) {
    if (message == WM_COMMAND) {
        const int id = LOWORD(wParam);
        if (id == kExclusionButtonBase) { AddFileExclusion(); handled = true; return 0; }
        if (id == kExclusionButtonBase + 1) { AddFolderExclusion(); handled = true; return 0; }
        if (id == kExclusionButtonBase + 2) { RemoveExclusion(); handled = true; return 0; }
        if (id == kSave) { Save(); handled = true; return 0; }
        if (id == kReload) { Load(); handled = true; return 0; }
    }
    if (message == WM_DRAWITEM) {
        const auto* draw = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (draw && draw->CtlType == ODT_BUTTON) {
            const int id = static_cast<int>(draw->CtlID);
            if ((id >= kExclusionButtonBase && id < kExclusionButtonBase + 3) || id == kSave || id == kReload) {
                handled = DrawOwnerButton(*draw, GetControlText(draw->hwndItem), id == kSave ? ButtonVisual::Accent : ButtonVisual::Normal);
                return handled ? TRUE : 0;
            }
        }
    }
    handled = false;
    return 0;
}

}
