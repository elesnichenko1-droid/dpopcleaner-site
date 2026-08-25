#pragma once

#include "modules/SettingsStore.h"
#include "ui/PageBase.h"
#include "ui/RecoveryControls.h"
#include "ui/settings/SettingsController.h"

#include <array>
#include <functional>
#include <optional>

namespace dpop::ui {

class SettingsPage final : public PageBase {
public:
    using ApplyCallback = std::function<void(const dpop::settings::AppSettings&)>;

    ~SettingsPage() override { Destroy(); }

    bool Create(HWND parent, SessionLog& log, ApplyCallback onApplied = {}) {
        onApplied_ = std::move(onApplied);
        return PageBase::Create(parent, log, L"Настройки");
    }

    bool HasUnsavedChanges() const noexcept;

protected:
    bool OnCreate() override;
    void OnLayout(int width, int height) noexcept override;
    LRESULT OnMessage(UINT message, WPARAM wParam, LPARAM lParam, bool& handled) override;
    void OnPaint(HDC dc, const RECT& client) noexcept override;
    void OnVisibilityChanged(bool visible) noexcept override;

private:
    enum class Section : unsigned {
        General = 0,
        Cleaning = 1,
        Memory = 2,
        Protection = 3,
        Exclusions = 4,
    };

    void Load();
    void SelectSection(Section section) noexcept;
    void UpdateSectionVisibility() noexcept;
    void PushControls();
    bool PullControls(std::wstring& error);
    void MarkDirty() noexcept;

    void RefreshExclusions();
    void AddFileExclusion();
    void AddFolderExclusion();
    void RemoveExclusion();
    void CycleCloseBehavior();
    void CycleMemoryScope();

    bool ApplyChanges(bool persist);
    void CancelChanges();
    void LoadDefaults();

    RecoveryFonts fonts_;
    std::array<HWND, 5> sectionButtons_{};

    // Основное
    HWND runAtStartup_{};
    HWND alwaysAdmin_{};
    HWND checkUpdates_{};
    HWND trayEnabled_{};
    HWND closeBehaviorLabel_{};
    HWND closeBehavior_{};

    // Очистка
    HWND confirmDestructive_{};
    HWND backgroundMonitor_{};
    HWND largeFileLabel_{};
    HWND largeFile_{};
    HWND duplicateMinLabel_{};
    HWND duplicateMin_{};

    // Память
    HWND memoryAutoTrim_{};
    HWND memoryPercentLabel_{};
    HWND memoryPercent_{};
    HWND memoryIntervalLabel_{};
    HWND memoryInterval_{};
    HWND memoryScopeLabel_{};
    HWND memoryScope_{};

    // Защита
    HWND quickGuard_{};
    HWND checkUpdateCache_{};

    // Исключения
    HWND exclusions_{};
    std::array<HWND, 3> exclusionButtons_{};

    // Нижние действия
    HWND apply_{};
    HWND save_{};
    HWND cancel_{};
    HWND defaults_{};

    std::optional<SettingsController> controller_;
    ApplyCallback onApplied_;
    Section section_{Section::General};
    bool loadingControls_{false};
};

} // namespace dpop::ui
