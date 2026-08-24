#include "ui/pages/SettingsPage.h"

#include "modules/FullCore.h"
#include "ui/Controls.h"
#include "ui/PageLayout.h"
#include "ui/Theme.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>

namespace dpop::ui {
namespace {

constexpr int kSectionBase = 3300;
constexpr int kGeneralBase = 3320;
constexpr int kCleaningBase = 3340;
constexpr int kMemoryBase = 3360;
constexpr int kProtectionBase = 3380;
constexpr int kExclusions = 3400;
constexpr int kExclusionButtonBase = 3410;
constexpr int kActionBase = 3430;

bool ParseUnsigned(HWND edit, unsigned minValue, unsigned maxValue, unsigned& value) {
    const std::wstring text = GetControlText(edit);
    if (text.empty()) return false;
    wchar_t* end = nullptr;
    const unsigned long parsed = wcstoul(text.c_str(), &end, 10);
    if (!end || *end != L'\0' || parsed < minValue || parsed > maxValue) return false;
    value = static_cast<unsigned>(parsed);
    return true;
}

std::wstring CloseBehaviorText(dpop::settings::CloseBehavior value) {
    switch (value) {
    case dpop::settings::CloseBehavior::Exit: return L"При закрытии: выйти";
    case dpop::settings::CloseBehavior::MinimizeToTray: return L"При закрытии: свернуть в трей";
    case dpop::settings::CloseBehavior::Ask: return L"При закрытии: спрашивать";
    }
    return L"При закрытии: выйти";
}

std::wstring MemoryScopeText(dpop::settings::MemoryScope value) {
    return value == dpop::settings::MemoryScope::Advanced
        ? L"Режим памяти: расширенный"
        : L"Режим памяти: безопасный";
}

void SetVisible(HWND hwnd, bool visible) noexcept {
    if (hwnd) ShowWindow(hwnd, visible ? SW_SHOW : SW_HIDE);
}

} // namespace

bool SettingsPage::HasUnsavedChanges() const noexcept {
    return controller_.has_value() && controller_->Dirty();
}

bool SettingsPage::OnCreate() {
    if (!fonts_.Create()) return false;

    const std::array<std::wstring_view, 5> sections = {
        L"Основное", L"Очистка", L"Память", L"Защита", L"Исключения"
    };
    for (std::size_t i = 0; i < sections.size(); ++i) {
        sectionButtons_[i] = CreatePushButton(
            Hwnd(), kSectionBase + static_cast<int>(i), sections[i],
            i == 0 ? ButtonVisual::Accent : ButtonVisual::Normal);
        if (!sectionButtons_[i]) return false;
        ApplyControlFont(sectionButtons_[i], fonts_.body);
    }

    runAtStartup_ = CreateCheckBox(Hwnd(), kGeneralBase + 0, L"Запускать DPopCleaner вместе с Windows", false);
    alwaysAdmin_ = CreateCheckBox(Hwnd(), kGeneralBase + 1, L"Всегда запрашивать права администратора", false);
    checkUpdates_ = CreateCheckBox(Hwnd(), kGeneralBase + 2, L"Проверять обновления DPopCleaner при запуске", true);
    trayEnabled_ = CreateCheckBox(Hwnd(), kGeneralBase + 3, L"Показывать значок DPopCleaner в трее", true);
    closeBehaviorLabel_ = CreateTextLabel(Hwnd(), kGeneralBase + 4, L"Поведение кнопки закрытия", SS_LEFT | SS_NOPREFIX);
    closeBehavior_ = CreatePushButton(Hwnd(), kGeneralBase + 5, L"При закрытии: свернуть в трей", ButtonVisual::Normal);

    confirmDestructive_ = CreateCheckBox(Hwnd(), kCleaningBase + 0, L"Подтверждать опасные и необратимые действия", true);
    backgroundMonitor_ = CreateCheckBox(Hwnd(), kCleaningBase + 1, L"Фоновый анализ объёма мусора", false);
    largeFileLabel_ = CreateTextLabel(Hwnd(), kCleaningBase + 2, L"Порог крупных файлов, МБ", SS_LEFT | SS_NOPREFIX);
    largeFile_ = CreateDarkEdit(Hwnd(), kCleaningBase + 3, L"500");
    duplicateMinLabel_ = CreateTextLabel(Hwnd(), kCleaningBase + 4, L"Минимальный размер дубликата, МБ", SS_LEFT | SS_NOPREFIX);
    duplicateMin_ = CreateDarkEdit(Hwnd(), kCleaningBase + 5, L"10");

    memoryAutoTrim_ = CreateCheckBox(Hwnd(), kMemoryBase + 0, L"Автоматически освобождать память при высоком использовании", false);
    memoryPercentLabel_ = CreateTextLabel(Hwnd(), kMemoryBase + 1, L"Порог использования памяти, %", SS_LEFT | SS_NOPREFIX);
    memoryPercent_ = CreateDarkEdit(Hwnd(), kMemoryBase + 2, L"80");
    memoryIntervalLabel_ = CreateTextLabel(Hwnd(), kMemoryBase + 3, L"Минимальный интервал, минут", SS_LEFT | SS_NOPREFIX);
    memoryInterval_ = CreateDarkEdit(Hwnd(), kMemoryBase + 4, L"15");
    memoryScopeLabel_ = CreateTextLabel(Hwnd(), kMemoryBase + 5, L"Область автоочистки памяти", SS_LEFT | SS_NOPREFIX);
    memoryScope_ = CreatePushButton(Hwnd(), kMemoryBase + 6, L"Режим памяти: безопасный", ButtonVisual::Normal);

    quickGuard_ = CreateCheckBox(Hwnd(), kProtectionBase + 0, L"Быстрый DPopGuard-скан при запуске", false);
    checkUpdateCache_ = CreateCheckBox(Hwnd(), kProtectionBase + 1, L"Проверять размер кэша Windows Update при запуске", false);

    exclusions_ = CreateDarkListView(Hwnd(), kExclusions);
    if (exclusions_) {
        ApplyControlFont(exclusions_, fonts_.smallFont);
        ResetList(exclusions_);
        AddListColumn(exclusions_, 0, L"Файл или папка — DPopCleaner не будет очищать этот путь", 720);
    }
    const std::array<std::wstring_view, 3> exclusionLabels = {
        L"Добавить файл", L"Добавить папку", L"Удалить"
    };
    for (std::size_t i = 0; i < exclusionButtons_.size(); ++i) {
        exclusionButtons_[i] = CreatePushButton(
            Hwnd(), kExclusionButtonBase + static_cast<int>(i), exclusionLabels[i], ButtonVisual::Normal);
        if (!exclusionButtons_[i]) return false;
        ApplyControlFont(exclusionButtons_[i], fonts_.smallFont);
    }

    apply_ = CreatePushButton(Hwnd(), kActionBase + 0, L"Применить", ButtonVisual::Accent);
    save_ = CreatePushButton(Hwnd(), kActionBase + 1, L"Сохранить", ButtonVisual::Normal);
    cancel_ = CreatePushButton(Hwnd(), kActionBase + 2, L"Отмена", ButtonVisual::Normal);
    defaults_ = CreatePushButton(Hwnd(), kActionBase + 3, L"По умолчанию", ButtonVisual::Normal);

    const std::array<HWND, 24> controls = {
        runAtStartup_, alwaysAdmin_, checkUpdates_, trayEnabled_, closeBehaviorLabel_, closeBehavior_,
        confirmDestructive_, backgroundMonitor_, largeFileLabel_, largeFile_, duplicateMinLabel_, duplicateMin_,
        memoryAutoTrim_, memoryPercentLabel_, memoryPercent_, memoryIntervalLabel_, memoryInterval_, memoryScopeLabel_, memoryScope_,
        quickGuard_, checkUpdateCache_, exclusions_, apply_, save_
    };
    for (HWND control : controls) {
        if (!control) return false;
        ApplyControlFont(control, fonts_.body);
    }
    if (!cancel_ || !defaults_) return false;
    ApplyControlFont(cancel_, fonts_.body);
    ApplyControlFont(defaults_, fonts_.body);

    Load();
    SelectSection(Section::General);
    return true;
}

void SettingsPage::Load() {
    const auto loaded = dpop::settings::LoadAppSettings();
    controller_.emplace(loaded.settings);
    PushControls();

    if (!loaded.warning.empty()) {
        SetStatus(loaded.warning);
        Log(EventLevel::Warning, loaded.warning);
    } else if (loaded.migrated) {
        const std::wstring message = L"Старые настройки DPopCleaner перенесены в схему 0.3.5.";
        SetStatus(message);
        Log(EventLevel::Info, message);
    } else {
        SetStatus(L"Настройки загружены: " + dpop::settings::SettingsPath().wstring());
    }
}

void SettingsPage::OnVisibilityChanged(bool visible) noexcept {
    if (visible) {
        UpdateSectionVisibility();
        InvalidateRect(Hwnd(), nullptr, TRUE);
    }
}

void SettingsPage::SelectSection(Section section) noexcept {
    section_ = section;
    UpdateSectionVisibility();
    for (HWND button : sectionButtons_) if (button) InvalidateRect(button, nullptr, TRUE);
    InvalidateRect(Hwnd(), nullptr, TRUE);
}

void SettingsPage::UpdateSectionVisibility() noexcept {
    const bool general = section_ == Section::General;
    for (HWND h : {runAtStartup_, alwaysAdmin_, checkUpdates_, trayEnabled_, closeBehaviorLabel_, closeBehavior_}) SetVisible(h, general);

    const bool cleaning = section_ == Section::Cleaning;
    for (HWND h : {confirmDestructive_, backgroundMonitor_, largeFileLabel_, largeFile_, duplicateMinLabel_, duplicateMin_}) SetVisible(h, cleaning);

    const bool memory = section_ == Section::Memory;
    for (HWND h : {memoryAutoTrim_, memoryPercentLabel_, memoryPercent_, memoryIntervalLabel_, memoryInterval_, memoryScopeLabel_, memoryScope_}) SetVisible(h, memory);

    const bool protection = section_ == Section::Protection;
    for (HWND h : {quickGuard_, checkUpdateCache_}) SetVisible(h, protection);

    const bool exclusions = section_ == Section::Exclusions;
    SetVisible(exclusions_, exclusions);
    for (HWND h : exclusionButtons_) SetVisible(h, exclusions);
}

void SettingsPage::PushControls() {
    if (!controller_) return;
    loadingControls_ = true;
    const auto& settings = controller_->Edit();

    SetChecked(runAtStartup_, settings.runAtStartup);
    SetChecked(alwaysAdmin_, settings.alwaysRunAsAdmin);
    SetChecked(checkUpdates_, settings.checkUpdatesAtStartup);
    SetChecked(trayEnabled_, settings.trayEnabled);
    SetControlText(closeBehavior_, CloseBehaviorText(settings.closeBehavior));

    SetChecked(confirmDestructive_, settings.confirmDestructive);
    SetChecked(backgroundMonitor_, settings.backgroundJunkMonitor);
    SetControlText(largeFile_, std::to_wstring(settings.largeFileMB));
    SetControlText(duplicateMin_, std::to_wstring(settings.duplicateMinMB));

    SetChecked(memoryAutoTrim_, settings.memoryAutoTrimEnabled);
    SetControlText(memoryPercent_, std::to_wstring(settings.memoryAutoTrimPercent));
    SetControlText(memoryInterval_, std::to_wstring(settings.memoryAutoTrimIntervalMinutes));
    SetControlText(memoryScope_, MemoryScopeText(settings.memoryScope));

    SetChecked(quickGuard_, settings.quickGuardAtStartup);
    SetChecked(checkUpdateCache_, settings.checkUpdateCacheAtStartup);
    RefreshExclusions();
    loadingControls_ = false;
}

bool SettingsPage::PullControls(std::wstring& error) {
    if (!controller_) {
        error = L"Редактор настроек не инициализирован.";
        return false;
    }

    auto next = controller_->Edit();
    next.runAtStartup = IsChecked(runAtStartup_);
    next.alwaysRunAsAdmin = IsChecked(alwaysAdmin_);
    next.checkUpdatesAtStartup = IsChecked(checkUpdates_);
    next.trayEnabled = IsChecked(trayEnabled_);

    next.confirmDestructive = IsChecked(confirmDestructive_);
    next.backgroundJunkMonitor = IsChecked(backgroundMonitor_);
    if (!ParseUnsigned(largeFile_, 50, 4096, next.largeFileMB)) {
        error = L"Порог крупных файлов должен быть от 50 до 4096 МБ.";
        return false;
    }
    if (!ParseUnsigned(duplicateMin_, 1, 1024, next.duplicateMinMB)) {
        error = L"Минимальный размер дубликатов должен быть от 1 до 1024 МБ.";
        return false;
    }

    next.memoryAutoTrimEnabled = IsChecked(memoryAutoTrim_);
    if (!ParseUnsigned(memoryPercent_, 50, 98, next.memoryAutoTrimPercent)) {
        error = L"Порог автоочистки памяти должен быть от 50 до 98%.";
        return false;
    }
    if (!ParseUnsigned(memoryInterval_, 1, 1440, next.memoryAutoTrimIntervalMinutes)) {
        error = L"Интервал автоочистки памяти должен быть от 1 до 1440 минут.";
        return false;
    }

    next.quickGuardAtStartup = IsChecked(quickGuard_);
    next.checkUpdateCacheAtStartup = IsChecked(checkUpdateCache_);

    if (!dpop::settings::ValidateSettings(next, error)) return false;
    if (next != controller_->Edit()) {
        controller_->Edit() = std::move(next);
        controller_->MarkDirty();
    }
    return true;
}

void SettingsPage::MarkDirty() noexcept {
    if (!loadingControls_ && controller_) controller_->MarkDirty();
}

void SettingsPage::RefreshExclusions() {
    if (!exclusions_ || !controller_) return;
    ListView_DeleteAllItems(exclusions_);
    for (const auto& value : controller_->Edit().cleanExclusions) AddListRow(exclusions_, {value});
}

void SettingsPage::AddFileExclusion() {
    if (!controller_) return;
    const auto path = ChooseFile(Hwnd(), L"Выберите файл, который DPopCleaner не должен удалять");
    if (path.empty()) return;
    const auto normalized = dpop::settings::NormalizeExclusionPath(path);
    auto& list = controller_->Edit().cleanExclusions;
    if (std::find(list.begin(), list.end(), normalized) == list.end()) {
        list.push_back(normalized);
        controller_->MarkDirty();
        RefreshExclusions();
        SetStatus(L"Файл добавлен в исключения. Изменения ещё не сохранены.");
    }
}

void SettingsPage::AddFolderExclusion() {
    if (!controller_) return;
    const auto path = ChooseFolder(Hwnd(), L"Выберите папку, которую DPopCleaner не должен очищать");
    if (path.empty()) return;
    const auto normalized = dpop::settings::NormalizeExclusionPath(path);
    auto& list = controller_->Edit().cleanExclusions;
    if (std::find(list.begin(), list.end(), normalized) == list.end()) {
        list.push_back(normalized);
        controller_->MarkDirty();
        RefreshExclusions();
        SetStatus(L"Папка добавлена в исключения. Изменения ещё не сохранены.");
    }
}

void SettingsPage::RemoveExclusion() {
    if (!controller_) return;
    const int index = SelectedListIndex(exclusions_);
    auto& list = controller_->Edit().cleanExclusions;
    if (index < 0 || index >= static_cast<int>(list.size())) {
        SetStatus(L"Выберите исключение для удаления.");
        return;
    }
    list.erase(list.begin() + index);
    controller_->MarkDirty();
    RefreshExclusions();
    SetStatus(L"Исключение удалено из рабочего списка. Изменения ещё не сохранены.");
}

void SettingsPage::CycleCloseBehavior() {
    if (!controller_) return;
    auto& value = controller_->Edit().closeBehavior;
    switch (value) {
    case dpop::settings::CloseBehavior::Exit:
        value = dpop::settings::CloseBehavior::MinimizeToTray;
        break;
    case dpop::settings::CloseBehavior::MinimizeToTray:
        value = dpop::settings::CloseBehavior::Ask;
        break;
    case dpop::settings::CloseBehavior::Ask:
        value = dpop::settings::CloseBehavior::Exit;
        break;
    }
    SetControlText(closeBehavior_, CloseBehaviorText(value));
    controller_->MarkDirty();
}

void SettingsPage::CycleMemoryScope() {
    if (!controller_) return;
    auto& value = controller_->Edit().memoryScope;
    value = value == dpop::settings::MemoryScope::Safe
        ? dpop::settings::MemoryScope::Advanced
        : dpop::settings::MemoryScope::Safe;
    SetControlText(memoryScope_, MemoryScopeText(value));
    controller_->MarkDirty();
}

bool SettingsPage::ApplyChanges(bool persist) {
    if (!controller_) return false;

    std::wstring error;
    if (!PullControls(error)) {
        SetStatus(error);
        MessageBoxW(Hwnd(), error.c_str(), L"DPopCleaner — настройки", MB_OK | MB_ICONWARNING);
        return false;
    }

    const auto previous = controller_->Persisted();
    const auto next = controller_->Edit();

    bool startupChanged = false;
    bool adminChanged = false;
    if (previous.runAtStartup != next.runAtStartup) {
        if (!dpop::full::SetRunAtStartup(next.runAtStartup, error)) {
            SetStatus(error); Log(EventLevel::Error, error); return false;
        }
        startupChanged = true;
    }
    if (previous.alwaysRunAsAdmin != next.alwaysRunAsAdmin) {
        if (!dpop::full::SetAlwaysRunAsAdmin(next.alwaysRunAsAdmin, error)) {
            if (startupChanged) {
                std::wstring rollbackError;
                dpop::full::SetRunAtStartup(previous.runAtStartup, rollbackError);
            }
            SetStatus(error); Log(EventLevel::Error, error); return false;
        }
        adminChanged = true;
    }

    if (persist && !dpop::settings::SaveAppSettings(next, error)) {
        if (adminChanged) {
            std::wstring rollbackError;
            dpop::full::SetAlwaysRunAsAdmin(previous.alwaysRunAsAdmin, rollbackError);
        }
        if (startupChanged) {
            std::wstring rollbackError;
            dpop::full::SetRunAtStartup(previous.runAtStartup, rollbackError);
        }
        SetStatus(error); Log(EventLevel::Error, error); return false;
    }

    controller_->CommitInMemory();
    if (onApplied_) onApplied_(controller_->Persisted());

    const std::wstring message = persist
        ? L"Настройки сохранены на диск и применены."
        : L"Настройки применены к текущему сеансу без записи на диск.";
    SetStatus(message);
    Log(EventLevel::Info, message);
    return true;
}

void SettingsPage::CancelChanges() {
    if (!controller_) return;
    controller_->CancelEdits();
    PushControls();
    SetStatus(L"Несохранённые изменения отменены.");
}

void SettingsPage::LoadDefaults() {
    if (!controller_) return;
    controller_->LoadDefaults();
    PushControls();
    SetStatus(L"Загружены безопасные значения по умолчанию. Нажмите «Применить» или «Сохранить».");
}

void SettingsPage::OnLayout(int width, int height) noexcept {
    const UINT rawDpi = GetDpiForWindow(Hwnd());
    const int dpi = rawDpi ? static_cast<int>(rawDpi) : 96;
    const int margin = ScalePageLogical(18, dpi);
    const int gap = ScalePageLogical(8, dpi);
    const int top = ComputePageContentTop(dpi);
    const int sectionH = ScalePageLogical(34, dpi);
    const int actionH = ScalePageLogical(38, dpi);
    const int actionY = std::max(top + sectionH + gap + 180, height - margin - actionH);

    const int sectionW = std::max(100, (width - margin * 2 - gap * 4) / 5);
    for (std::size_t i = 0; i < sectionButtons_.size(); ++i) {
        MoveWindow(sectionButtons_[i], margin + static_cast<int>(i) * (sectionW + gap), top,
                   sectionW, sectionH, TRUE);
    }

    const int contentY = top + sectionH + gap + ScalePageLogical(12, dpi);
    const int contentX = margin + ScalePageLogical(16, dpi);
    const int contentW = std::max(240, width - margin * 2 - ScalePageLogical(32, dpi));
    const int rowH = ScalePageLogical(36, dpi);
    const int editW = ScalePageLogical(90, dpi);

    int y = contentY;
    for (HWND h : {runAtStartup_, alwaysAdmin_, checkUpdates_, trayEnabled_}) {
        MoveWindow(h, contentX, y, contentW, rowH - 4, TRUE); y += rowH;
    }
    MoveWindow(closeBehaviorLabel_, contentX, y + 5, std::max(160, contentW - 330), 24, TRUE);
    MoveWindow(closeBehavior_, contentX + std::max(160, contentW - 320), y, 300, rowH - 4, TRUE);

    y = contentY;
    MoveWindow(confirmDestructive_, contentX, y, contentW, rowH - 4, TRUE); y += rowH;
    MoveWindow(backgroundMonitor_, contentX, y, contentW, rowH - 4, TRUE); y += rowH + gap;
    MoveWindow(largeFileLabel_, contentX, y + 5, contentW - editW - gap, 24, TRUE);
    MoveWindow(largeFile_, contentX + contentW - editW, y, editW, rowH - 4, TRUE); y += rowH;
    MoveWindow(duplicateMinLabel_, contentX, y + 5, contentW - editW - gap, 24, TRUE);
    MoveWindow(duplicateMin_, contentX + contentW - editW, y, editW, rowH - 4, TRUE);

    y = contentY;
    MoveWindow(memoryAutoTrim_, contentX, y, contentW, rowH - 4, TRUE); y += rowH + gap;
    MoveWindow(memoryPercentLabel_, contentX, y + 5, contentW - editW - gap, 24, TRUE);
    MoveWindow(memoryPercent_, contentX + contentW - editW, y, editW, rowH - 4, TRUE); y += rowH;
    MoveWindow(memoryIntervalLabel_, contentX, y + 5, contentW - editW - gap, 24, TRUE);
    MoveWindow(memoryInterval_, contentX + contentW - editW, y, editW, rowH - 4, TRUE); y += rowH + gap;
    MoveWindow(memoryScopeLabel_, contentX, y + 5, std::max(160, contentW - 330), 24, TRUE);
    MoveWindow(memoryScope_, contentX + std::max(160, contentW - 320), y, 300, rowH - 4, TRUE);

    y = contentY;
    MoveWindow(quickGuard_, contentX, y, contentW, rowH - 4, TRUE); y += rowH;
    MoveWindow(checkUpdateCache_, contentX, y, contentW, rowH - 4, TRUE);

    const int exButtonsH = ScalePageLogical(34, dpi);
    const int exListBottom = actionY - gap * 2 - exButtonsH;
    MoveWindow(exclusions_, contentX, contentY, contentW, std::max(120, exListBottom - contentY), TRUE);
    const int exButtonW = std::max(110, (contentW - gap * 2) / 3);
    for (std::size_t i = 0; i < exclusionButtons_.size(); ++i) {
        MoveWindow(exclusionButtons_[i], contentX + static_cast<int>(i) * (exButtonW + gap),
                   exListBottom + gap, exButtonW, exButtonsH, TRUE);
    }

    const int actionW = ScalePageLogical(132, dpi);
    MoveWindow(apply_, margin, actionY, actionW, actionH, TRUE);
    MoveWindow(save_, margin + actionW + gap, actionY, actionW, actionH, TRUE);
    MoveWindow(cancel_, margin + (actionW + gap) * 2, actionY, actionW, actionH, TRUE);
    MoveWindow(defaults_, margin + (actionW + gap) * 3, actionY, ScalePageLogical(154, dpi), actionH, TRUE);
}

void SettingsPage::OnPaint(HDC dc, const RECT& client) noexcept {
    PageBase::OnPaint(dc, client);
    DrawPageHeading(
        dc, 18, 4, L"Настройки",
        L"Пять функциональных разделов. Изменения можно применить только к текущему сеансу или сохранить атомарно в settings.json.",
        fonts_.title, fonts_.body);

    const UINT rawDpi = GetDpiForWindow(Hwnd());
    const int dpi = rawDpi ? static_cast<int>(rawDpi) : 96;
    const int margin = ScalePageLogical(18, dpi);
    const int top = ComputePageContentTop(dpi);
    const int sectionH = ScalePageLogical(34, dpi);
    const int panelTop = top + sectionH + ScalePageLogical(8, dpi);
    const int panelBottom = std::max(panelTop + 100, static_cast<int>(client.bottom) - ScalePageLogical(66, dpi));
    RECT panel{margin, panelTop, static_cast<int>(client.right) - margin, panelBottom};
    DrawPanel(dc, panel, true);

    const std::array<std::wstring_view, 5> sectionTitles = {
        L"Основное", L"Очистка", L"Память", L"Защита", L"Исключения"
    };
    DrawPanelTitle(dc, panel, sectionTitles[static_cast<std::size_t>(section_)], fonts_.section);

    if (HasUnsavedChanges()) {
        RECT dirty{panel.right - 260, panel.top + 8, panel.right - 16, panel.top + 34};
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, MidnightPalette().warning);
        SelectObject(dc, fonts_.smallFont);
        DrawTextW(dc, L"Есть несохранённые изменения", -1, &dirty,
                  DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
}

LRESULT SettingsPage::OnMessage(UINT message, WPARAM wParam, LPARAM lParam, bool& handled) {
    if (message == WM_COMMAND) {
        const int id = LOWORD(wParam);
        const int code = HIWORD(wParam);

        if (id >= kSectionBase && id < kSectionBase + 5 && code == BN_CLICKED) {
            SelectSection(static_cast<Section>(id - kSectionBase));
            handled = true;
            return 0;
        }

        if (id == kGeneralBase + 5 && code == BN_CLICKED) { CycleCloseBehavior(); handled = true; return 0; }
        if (id == kMemoryBase + 6 && code == BN_CLICKED) { CycleMemoryScope(); handled = true; return 0; }

        if (id == kExclusionButtonBase + 0 && code == BN_CLICKED) { AddFileExclusion(); handled = true; return 0; }
        if (id == kExclusionButtonBase + 1 && code == BN_CLICKED) { AddFolderExclusion(); handled = true; return 0; }
        if (id == kExclusionButtonBase + 2 && code == BN_CLICKED) { RemoveExclusion(); handled = true; return 0; }

        if (id == kActionBase + 0 && code == BN_CLICKED) { ApplyChanges(false); handled = true; return 0; }
        if (id == kActionBase + 1 && code == BN_CLICKED) { ApplyChanges(true); handled = true; return 0; }
        if (id == kActionBase + 2 && code == BN_CLICKED) { CancelChanges(); handled = true; return 0; }
        if (id == kActionBase + 3 && code == BN_CLICKED) { LoadDefaults(); handled = true; return 0; }

        if (!loadingControls_) {
            const bool checkClick = code == BN_CLICKED && (
                (id >= kGeneralBase && id <= kGeneralBase + 3) ||
                (id >= kCleaningBase && id <= kCleaningBase + 1) ||
                id == kMemoryBase ||
                (id >= kProtectionBase && id <= kProtectionBase + 1));
            const bool editChange = code == EN_CHANGE && (
                id == kCleaningBase + 3 || id == kCleaningBase + 5 ||
                id == kMemoryBase + 2 || id == kMemoryBase + 4);
            if (checkClick || editChange) {
                MarkDirty();
                InvalidateRect(Hwnd(), nullptr, FALSE);
            }
        }
    }
    return PageBase::OnMessage(message, wParam, lParam, handled);
}

} // namespace dpop::ui
