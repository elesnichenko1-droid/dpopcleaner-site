#pragma once
#include <windows.h>

#include <array>
#include <atomic>
#include <filesystem>
#include <functional>
#include <mutex>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "modules/Applications.h"
#include "modules/FullCore.h"
#include "ui/Controls.h"
#include "ui/Layout.h"
#include "ui/SessionLog.h"
#include "ui/ShellModel.h"

namespace dpop::ui {

inline constexpr UINT kWorkspaceLogChangedMessage = WM_APP + 33;

class WorkspacePage {
public:
    WorkspacePage() = default;
    ~WorkspacePage();

    WorkspacePage(const WorkspacePage&) = delete;
    WorkspacePage& operator=(const WorkspacePage&) = delete;

    bool Create(HWND parent, SessionLog& sessionLog);
    void Destroy() noexcept;
    void Show(bool visible) noexcept;
    void Layout(const Box& box) noexcept;
    void SetPage(Page page);

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    bool RegisterClass() noexcept;
    void ConfigurePage();
    void LayoutChildren() noexcept;
    void SetStatus(std::wstring_view text);
    void Log(EventLevel level, std::wstring_view message);

    void ClearList(bool checkboxes = false);
    void AddColumn(int column, std::wstring_view title, int width);
    int AddRow(const std::vector<std::wstring>& columns);
    int SelectedIndex() const noexcept;
    std::vector<int> SelectedIndices() const;
    void SetButtons(const std::vector<std::wstring>& labels, int accent = 0, int danger = -1);

    std::filesystem::path ChooseFolder(std::wstring_view title);
    std::filesystem::path ChooseFile(std::wstring_view title);
    void OpenPath(const std::filesystem::path& path, bool selectFile = false);

    void CancelWorker() noexcept;
    void StartAsync(std::wstring_view status, std::function<void(std::stop_token)> work);
    void QueueApply(std::function<void()> apply);
    void CompleteAsync();

    void RefreshCleaning();
    void RefreshMemory();
    void RefreshApps();
    void RefreshWindows();
    void RefreshTools();
    void RefreshZapret();
    void RefreshSettings();
    void HandleAction(int buttonIndex);

    HWND parent_{};
    HWND hwnd_{};
    HWND heading_{};
    HWND status_{};
    HWND list_{};
    std::array<HWND, 6> buttons_{};
    std::array<ButtonVisual, 6> visuals_{};

    HFONT headingFont_{};
    HFONT bodyFont_{};
    HFONT listFont_{};

    SessionLog* sessionLog_{};
    Page page_{Page::Cleaning};

    std::jthread worker_;
    std::atomic_bool busy_{false};
    std::mutex pendingMutex_;
    std::function<void()> pendingApply_;

    std::vector<dpop::full::CleanItem> cleanItems_;
    std::vector<dpop::apps::InstalledApp> apps_;
    std::vector<dpop::apps::LeftoverItem> leftovers_;
    bool showingLeftovers_{false};
    std::vector<dpop::full::FileItem> largeFiles_;
    std::vector<dpop::full::DuplicateFile> duplicates_;
    std::vector<dpop::full::GuardHit> guardHits_;
    std::filesystem::path diskRoot_{L"C:\\"};
    std::filesystem::path duplicateRoot_;
    dpop::full::Settings settings_{};
};

}
