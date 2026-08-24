#include "ui/Shell.h"

#include "modules/DPopGuard.h"
#include "modules/FullCore.h"
#include "modules/SettingsStore.h"
#include "ui/Controls.h"
#include "ui/Layout.h"
#include "ui/PageBase.h"
#include "ui/SessionLog.h"
#include "ui/ShellModel.h"
#include "ui/StatusBar.h"
#include "ui/Theme.h"
#include "ui/TrayIcon.h"
#include "ui/pages/ApplicationsPage.h"
#include "ui/pages/CleaningPage.h"
#include "ui/pages/DiskPage.h"
#include "ui/pages/DuplicatesPage.h"
#include "ui/pages/GuardPage.h"
#include "ui/pages/MemoryPage.h"
#include "ui/pages/OverviewPage.h"
#include "ui/pages/SettingsPage.h"
#include "ui/pages/ToolsPage.h"
#include "ui/pages/WindowsPage.h"
#include "ui/pages/ZapretPage.h"
#include "update/UpdateClient.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <thread>

#include <dwmapi.h>
#include <shellapi.h>
#include <uxtheme.h>

namespace dpop::ui::shell {
namespace {
namespace fs = std::filesystem;

constexpr wchar_t kWindowClassName[] = L"DPopCleaner035ShellWindow";
constexpr wchar_t kPageHostClassName[] = L"DPopCleaner035PageHost";

constexpr int kProductId = 1300;
constexpr int kSubtitleId = 1301;
constexpr int kBetaId = 1302;
constexpr int kPageHostId = 1303;
constexpr int kMinClientWidth = 1100;
constexpr int kMinClientHeight = 700;
constexpr UINT kTrayMessage = WM_APP + 0x35;
constexpr UINT kRuntimeStatusMessage = WM_APP + 0x36;
constexpr UINT kRunStartupActionsMessage = WM_APP + 0x37;
constexpr UINT_PTR kMemoryTimer = 0x3501;
constexpr UINT_PTR kJunkTimer = 0x3502;
constexpr UINT kJunkIntervalMs = 30u * 60u * 1000u;

struct RuntimeResult {
    EventLevel level{EventLevel::Info};
    std::wstring message;
};

std::wstring WindowText(HWND hwnd) {
    const int length = GetWindowTextLengthW(hwnd);
    if (length <= 0) return {};
    std::wstring text(static_cast<std::size_t>(length + 1), L'\0');
    GetWindowTextW(hwnd, text.data(), length + 1);
    text.resize(static_cast<std::size_t>(length));
    return text;
}

std::wstring_view LabelForPage(Page page) noexcept {
    for (const auto& tab : PrimaryTabs()) {
        if (tab.page == page) return tab.label;
    }
    return page == Page::Settings ? L"Настройки" : L"Обзор";
}

std::uint64_t DirectoryBytes(const fs::path& root, std::stop_token stop) {
    std::uint64_t total = 0;
    std::error_code ec;
    if (root.empty() || !fs::exists(root, ec)) return 0;
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
         it != end && !stop.stop_requested(); it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (!it->is_regular_file(ec)) { ec.clear(); continue; }
        const auto size = it->file_size(ec);
        if (!ec) total += size;
        ec.clear();
    }
    return total;
}

fs::path WindowsUpdateDownloadPath() {
    wchar_t windows[MAX_PATH]{};
    const UINT count = GetWindowsDirectoryW(windows, static_cast<UINT>(std::size(windows)));
    if (!count || count >= std::size(windows)) return {};
    return fs::path(windows) / L"SoftwareDistribution" / L"Download";
}

LRESULT CALLBACK PageHostProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    const auto& palette = MidnightPalette();
    switch (message) {
    case WM_ERASEBKGND: {
        RECT rc{};
        GetClientRect(hwnd, &rc);
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetDCBrushColor(dc, palette.background);
        FillRect(dc, &rc, reinterpret_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
        return 1;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc{};
        GetClientRect(hwnd, &rc);
        SetDCBrushColor(dc, palette.background);
        FillRect(dc, &rc, reinterpret_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
        EndPaint(hwnd, &ps);
        return 0;
    }
    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

class ShellApp {
public:
    explicit ShellApp(HINSTANCE instance) noexcept : instance_(instance) {}

    ~ShellApp() {
        StopRuntimeWorker();
        tray_.Destroy();
        if (productFont_) DeleteObject(productFont_);
        if (subtitleFont_) DeleteObject(subtitleFont_);
        if (tabFont_) DeleteObject(tabFont_);
        if (backgroundBrush_) DeleteObject(backgroundBrush_);
        if (titleBrush_) DeleteObject(titleBrush_);
        if (controlBrush_) DeleteObject(controlBrush_);
    }

    int Run(int showCommand) {
        if (!RegisterClasses()) return 10;
        if (!CreateMainWindow()) return 11;

        ShowWindow(hwnd_, showCommand);
        UpdateWindow(hwnd_);

        MSG message{};
        int result = 0;
        while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        return result == -1 ? 12 : static_cast<int>(message.wParam);
    }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        ShellApp* self = reinterpret_cast<ShellApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            self = static_cast<ShellApp*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            self->hwnd_ = hwnd;
        }
        return self ? self->HandleMessage(message, wParam, lParam)
                    : DefWindowProcW(hwnd, message, wParam, lParam);
    }

    bool RegisterClasses() {
        WNDCLASSEXW shellClass{};
        shellClass.cbSize = sizeof(shellClass);
        shellClass.lpfnWndProc = &ShellApp::WindowProc;
        shellClass.hInstance = instance_;
        shellClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        shellClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        shellClass.hIconSm = shellClass.hIcon;
        shellClass.lpszClassName = kWindowClassName;
        if (!RegisterClassExW(&shellClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

        WNDCLASSEXW pageClass{};
        pageClass.cbSize = sizeof(pageClass);
        pageClass.lpfnWndProc = &PageHostProc;
        pageClass.hInstance = instance_;
        pageClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        pageClass.lpszClassName = kPageHostClassName;
        return RegisterClassExW(&pageClass) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    }

    bool CreateMainWindow() {
        RECT desired{0, 0, 1200, 850};
        AdjustWindowRectEx(&desired, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_APPWINDOW);
        hwnd_ = CreateWindowExW(
            WS_EX_APPWINDOW,
            kWindowClassName,
            Identity().windowTitle.data(),
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            desired.right - desired.left,
            desired.bottom - desired.top,
            nullptr,
            nullptr,
            instance_,
            this
        );
        if (!hwnd_) return false;

        constexpr DWORD dark = 1;
        constexpr DWORD kImmersiveDarkMode = 20;
        DwmSetWindowAttribute(hwnd_, kImmersiveDarkMode, &dark, sizeof(dark));
        SetWindowTheme(hwnd_, L"DarkMode_Explorer", nullptr);
        return true;
    }

    bool CreateChildren() {
        const auto loaded = dpop::settings::LoadAppSettings();
        settings_ = loaded.settings;
        dpop::settings::SetActiveSettings(settings_);

        const auto& palette = MidnightPalette();
        backgroundBrush_ = CreateSolidBrush(palette.background);
        titleBrush_ = CreateSolidBrush(palette.title);
        controlBrush_ = CreateSolidBrush(palette.control);
        productFont_ = CreateUiFont(24, FW_BOLD);
        subtitleFont_ = CreateUiFont(10, FW_NORMAL);
        tabFont_ = CreateUiFont(10, FW_SEMIBOLD);
        if (!backgroundBrush_ || !titleBrush_ || !controlBrush_ ||
            !productFont_ || !subtitleFont_ || !tabFont_) return false;

        product_ = CreateTextLabel(hwnd_, kProductId, Identity().productName);
        subtitle_ = CreateTextLabel(hwnd_, kSubtitleId, Identity().subtitle);
        beta_ = CreateTextLabel(hwnd_, kBetaId, Identity().betaLabel);
        gear_ = CreatePushButton(hwnd_, kSettingsCommandId, L"⚙", ButtonVisual::Normal);
        pageHost_ = CreateWindowExW(
            0,
            kPageHostClassName,
            L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
            0, 0, 0, 0,
            hwnd_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPageHostId)),
            instance_,
            nullptr
        );
        if (!product_ || !subtitle_ || !beta_ || !gear_ || !pageHost_) return false;

        ApplyControlFont(product_, productFont_);
        ApplyControlFont(subtitle_, subtitleFont_);
        ApplyControlFont(beta_, tabFont_);
        ApplyControlFont(gear_, tabFont_);

        std::size_t index = 0;
        for (const auto& tab : PrimaryTabs()) {
            tabButtons_[index] = CreatePushButton(hwnd_, tab.commandId, tab.label, ButtonVisual::Normal);
            if (!tabButtons_[index]) return false;
            ApplyControlFont(tabButtons_[index], tabFont_);
            ++index;
        }

        if (!statusBar_.Create(hwnd_, sessionLog_)) return false;
        statusBar_.AppendLog(L"Shell", EventLevel::Info, L"DPopCleaner 0.3.5 BETA R1 запущен.");
        if (!loaded.warning.empty()) statusBar_.AppendLog(L"Settings", EventLevel::Warning, loaded.warning);
        else if (loaded.migrated) statusBar_.AppendLog(L"Settings", EventLevel::Info, L"Настройки перенесены в схему 2.");

        if (!overviewPage_.Create(pageHost_, sessionLog_, [this](Page page) { ShowPage(page); })) return false;
        if (!cleaningPage_.Create(pageHost_, sessionLog_)) return false;
        if (!memoryPage_.Create(pageHost_, sessionLog_)) return false;
        if (!guardPage_.Create(pageHost_, sessionLog_)) return false;
        if (!diskPage_.Create(pageHost_, sessionLog_)) return false;
        if (!applicationsPage_.Create(pageHost_, sessionLog_)) return false;
        if (!windowsPage_.Create(pageHost_, sessionLog_)) return false;
        if (!duplicatesPage_.Create(pageHost_, sessionLog_)) return false;
        if (!toolsPage_.Create(pageHost_, sessionLog_)) return false;
        if (!zapretPage_.Create(pageHost_, sessionLog_)) return false;
        if (!settingsPage_.Create(pageHost_, sessionLog_, [this](const dpop::settings::AppSettings& settings) {
                settings_ = settings;
                dpop::settings::SetActiveSettings(settings_);
                ApplyRuntimeSettings();
            })) return false;

        if (!tray_.Create(hwnd_, kTrayMessage)) {
            statusBar_.AppendLog(L"Tray", EventLevel::Warning, L"Не удалось инициализировать системный трей.");
        }

        ShowPage(Page::Overview, false);
        LayoutChildren();
        ApplyRuntimeSettings();
        PostMessageW(hwnd_, kRunStartupActionsMessage, 0, 0);
        return true;
    }

    void ApplyRuntimeSettings() {
        if (!hwnd_) return;
        dpop::settings::SetActiveSettings(settings_);

        if (!tray_.SetVisible(settings_.trayEnabled)) {
            statusBar_.AppendLog(L"Tray", EventLevel::Warning, L"Не удалось изменить состояние значка в трее.");
        }

        KillTimer(hwnd_, kMemoryTimer);
        KillTimer(hwnd_, kJunkTimer);
        if (settings_.memoryAutoTrimEnabled) {
            const UINT interval = std::max(1u, settings_.memoryAutoTrimIntervalMinutes) * 60u * 1000u;
            SetTimer(hwnd_, kMemoryTimer, interval, nullptr);
        }
        if (settings_.backgroundJunkMonitor) SetTimer(hwnd_, kJunkTimer, kJunkIntervalMs, nullptr);
    }

    void StartRuntimeJob(std::wstring_view status, std::function<RuntimeResult(std::stop_token)> job) {
        if (runtimeBusy_.exchange(true)) {
            statusBar_.AppendLog(L"Runtime", EventLevel::Info, L"Фоновая задача уже выполняется; новый запуск пропущен.");
            return;
        }
        if (runtimeWorker_.joinable()) runtimeWorker_.join();
        statusBar_.SetStatus(status);
        runtimeWorker_ = std::jthread([this, job = std::move(job)](std::stop_token stop) mutable {
            RuntimeResult result{};
            try {
                result = job(stop);
            } catch (...) {
                result.level = EventLevel::Error;
                result.message = L"Фоновая задача завершилась с внутренней ошибкой.";
            }
            runtimeBusy_.store(false);
            if (!stop.stop_requested() && hwnd_ && IsWindow(hwnd_)) {
                auto* text = new std::wstring(std::move(result.message));
                if (!PostMessageW(hwnd_, kRuntimeStatusMessage, static_cast<WPARAM>(result.level), reinterpret_cast<LPARAM>(text))) {
                    delete text;
                }
            }
        });
    }

    void StopRuntimeWorker() noexcept {
        if (runtimeWorker_.joinable()) {
            runtimeWorker_.request_stop();
            runtimeWorker_.join();
        }
        runtimeBusy_.store(false);
    }

    void RunStartupActions() {
        const auto startup = settings_;
        if (!startup.checkUpdatesAtStartup && !startup.quickGuardAtStartup && !startup.checkUpdateCacheAtStartup) return;

        StartRuntimeJob(L"Выполняем безопасные проверки при запуске…", [startup](std::stop_token stop) {
            std::wstring summary;
            EventLevel level = EventLevel::Info;

            if (startup.checkUpdatesAtStartup && !stop.stop_requested()) {
                const auto checked = dpop::update::CheckForUpdates();
                if (!checked.success) {
                    summary += L"Обновления: ошибка проверки";
                    if (!checked.error.empty()) summary += L" (" + checked.error + L")";
                    level = EventLevel::Warning;
                } else if (checked.updateAvailable) {
                    summary += L"Обновления: доступна версия " + checked.manifest.version;
                } else {
                    summary += L"Обновления: установлена актуальная версия";
                }
            }

            if (startup.quickGuardAtStartup && !stop.stop_requested()) {
                if (!summary.empty()) summary += L" • ";
                const auto scan = dpop::guard::QuickScan();
                summary += L"DPopGuard: находок " + std::to_wstring(scan.findings.size());
                if (!scan.findings.empty()) level = EventLevel::Warning;
            }

            if (startup.checkUpdateCacheAtStartup && !stop.stop_requested()) {
                if (!summary.empty()) summary += L" • ";
                const auto bytes = DirectoryBytes(WindowsUpdateDownloadPath(), stop);
                summary += L"Windows Update cache: " + dpop::full::FormatBytes(bytes);
            }

            if (summary.empty()) summary = L"Проверки при запуске завершены.";
            return RuntimeResult{level, std::move(summary)};
        });
    }

    void RunMemoryAutomation() {
        if (!settings_.memoryAutoTrimEnabled) return;
        const auto memory = dpop::full::QueryMemoryStats();
        if (memory.usedPercent < settings_.memoryAutoTrimPercent) return;

        const bool aggressive = settings_.memoryScope == dpop::settings::MemoryScope::Advanced;
        StartRuntimeJob(L"Автоочистка памяти…", [aggressive](std::stop_token stop) {
            const auto result = dpop::full::TrimWorkingSets(aggressive, stop);
            return RuntimeResult{
                result.failed ? EventLevel::Warning : EventLevel::Info,
                L"Автоочистка памяти: успешно " + std::to_wstring(result.trimmed) +
                    L" из " + std::to_wstring(result.attempted) + L", отказов " + std::to_wstring(result.failed)
            };
        });
    }

    void RunBackgroundJunkAnalysis() {
        if (!settings_.backgroundJunkMonitor) return;
        StartRuntimeJob(L"Фоновый анализ мусора…", [](std::stop_token stop) {
            const auto items = dpop::full::AnalyzeCleaning(stop);
            std::uint64_t total = 0;
            for (const auto& item : items) total += item.bytes;
            return RuntimeResult{EventLevel::Info, L"Фоновый анализ: можно очистить примерно " + dpop::full::FormatBytes(total)};
        });
    }

    void LayoutChildren() noexcept {
        if (!hwnd_) return;
        RECT client{};
        GetClientRect(hwnd_, &client);
        const int width = client.right - client.left;
        const int height = client.bottom - client.top;
        const ShellLayout layout = ComputeShellLayout(width, height);

        if (product_) MoveWindow(product_, 24, 16, 250, 38, TRUE);
        if (beta_) MoveWindow(beta_, 286, 22, 70, 28, TRUE);
        if (subtitle_) MoveWindow(subtitle_, 24, 58, width > 130 ? width - 130 : 0, 24, TRUE);
        if (gear_) MoveWindow(gear_, width > 72 ? width - 68 : 0, 22, 44, 44, TRUE);

        constexpr int tabGap = 6;
        constexpr int tabMargin = 24;
        const int usable = std::max(0, width - tabMargin * 2 - tabGap * 9);
        const int tabWidth = usable / 10;
        const int remainder = usable - tabWidth * 10;
        const int tabY = layout.tabs.y + 8;
        for (int i = 0; i < 10; ++i) {
            const int x = tabMargin + i * (tabWidth + tabGap);
            const int thisWidth = tabWidth + (i == 9 ? remainder : 0);
            if (tabButtons_[static_cast<std::size_t>(i)])
                MoveWindow(tabButtons_[static_cast<std::size_t>(i)], x, tabY, thisWidth, 38, TRUE);
        }

        if (pageHost_) MoveWindow(pageHost_, layout.content.x, layout.content.y, layout.content.width, layout.content.height, TRUE);
        const Box pageBox{0, 0, layout.content.width, layout.content.height};
        overviewPage_.Layout(pageBox);
        cleaningPage_.Layout(pageBox);
        memoryPage_.Layout(pageBox);
        guardPage_.Layout(pageBox);
        diskPage_.Layout(pageBox);
        applicationsPage_.Layout(pageBox);
        windowsPage_.Layout(pageBox);
        duplicatesPage_.Layout(pageBox);
        toolsPage_.Layout(pageBox);
        zapretPage_.Layout(pageBox);
        settingsPage_.Layout(pageBox);
        statusBar_.Layout(layout);
    }

    void SetPageVisibility(Page page) noexcept {
        overviewPage_.Show(page == Page::Overview);
        cleaningPage_.Show(page == Page::Cleaning);
        memoryPage_.Show(page == Page::Memory);
        guardPage_.Show(page == Page::Guard);
        diskPage_.Show(page == Page::Disk);
        applicationsPage_.Show(page == Page::Applications);
        windowsPage_.Show(page == Page::WindowsUpdate);
        duplicatesPage_.Show(page == Page::Duplicates);
        toolsPage_.Show(page == Page::Tools);
        zapretPage_.Show(page == Page::Zapret);
        settingsPage_.Show(page == Page::Settings);
    }

    void ShowPage(Page page, bool writeLog = true) {
        activePage_ = page;
        SetPageVisibility(page);
        statusBar_.SetStatus(L"Готово.");

        if (writeLog) {
            std::wstring message = L"Открыт раздел: ";
            message.append(LabelForPage(page));
            statusBar_.AppendLog(L"Shell", EventLevel::Info, message);
        }

        for (HWND button : tabButtons_) if (button) InvalidateRect(button, nullptr, TRUE);
        if (gear_) InvalidateRect(gear_, nullptr, TRUE);
        if (pageHost_) InvalidateRect(pageHost_, nullptr, TRUE);
    }

    void CancelAllPages() noexcept {
        cleaningPage_.Cancel();
        memoryPage_.Cancel();
        guardPage_.Cancel();
        diskPage_.Cancel();
        applicationsPage_.Cancel();
        windowsPage_.Cancel();
        duplicatesPage_.Cancel();
        toolsPage_.Cancel();
        zapretPage_.Cancel();
        settingsPage_.Cancel();
    }

    bool DrawButton(const DRAWITEMSTRUCT* draw) {
        if (!draw || draw->CtlType != ODT_BUTTON || !draw->hwndItem) return false;
        ButtonVisual visual = ButtonVisual::Normal;
        const int id = static_cast<int>(draw->CtlID);
        if (id >= 1000 && id <= 1009) {
            if (PageForCommand(id) == activePage_) visual = ButtonVisual::Accent;
        } else if (id == kSettingsCommandId && activePage_ == Page::Settings) {
            visual = ButtonVisual::Accent;
        }
        return DrawOwnerButton(*draw, WindowText(draw->hwndItem), visual);
    }

    void OpenSupport() {
        const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(
            hwnd_, L"open", L"https://elesnichenko1-droid.github.io/dpopcleaner-site/",
            nullptr, nullptr, SW_SHOWNORMAL));
        statusBar_.AppendLog(
            L"Shell",
            result <= 32 ? EventLevel::Warning : EventLevel::Info,
            result <= 32 ? L"Не удалось открыть страницу поддержки." : L"Открыта страница поддержки."
        );
    }

    void HideToTray() {
        if (!settings_.trayEnabled || !tray_.Visible()) {
            CloseApplication();
            return;
        }
        ShowWindow(hwnd_, SW_HIDE);
        statusBar_.AppendLog(L"Tray", EventLevel::Info, L"DPopCleaner свёрнут в системный трей.");
    }

    void RestoreFromTray() {
        ShowWindow(hwnd_, SW_SHOW);
        if (IsIconic(hwnd_)) ShowWindow(hwnd_, SW_RESTORE);
        SetForegroundWindow(hwnd_);
    }

    void CloseApplication() {
        if (exiting_) return;
        exiting_ = true;
        CancelAllPages();
        StopRuntimeWorker();
        tray_.Destroy();
        DestroyWindow(hwnd_);
    }

    void HandleClose() {
        switch (settings_.closeBehavior) {
        case dpop::settings::CloseBehavior::Exit:
            CloseApplication();
            return;
        case dpop::settings::CloseBehavior::MinimizeToTray:
            HideToTray();
            return;
        case dpop::settings::CloseBehavior::Ask: {
            const int answer = MessageBoxW(
                hwnd_,
                L"Закрыть DPopCleaner полностью?\n\nДа — выйти.\nНет — свернуть в трей.",
                L"DPopCleaner",
                MB_YESNOCANCEL | MB_ICONQUESTION | MB_DEFBUTTON2);
            if (answer == IDYES) CloseApplication();
            else if (answer == IDNO) HideToTray();
            return;
        }
        }
    }

    void Paint() {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd_, &ps);
        RECT client{};
        GetClientRect(hwnd_, &client);
        FillRect(dc, &client, backgroundBrush_);
        const ShellLayout layout = ComputeShellLayout(client.right, client.bottom);
        RECT header{layout.header.x, layout.header.y, layout.header.x + layout.header.width, layout.header.y + layout.header.height};
        RECT tabs{layout.tabs.x, layout.tabs.y, layout.tabs.x + layout.tabs.width, layout.tabs.y + layout.tabs.height};
        RECT footer{layout.footer.x, layout.footer.y, layout.footer.x + layout.footer.width, layout.footer.y + layout.footer.height};
        FillRect(dc, &header, titleBrush_);
        FillRect(dc, &tabs, titleBrush_);
        FillRect(dc, &footer, titleBrush_);
        EndPaint(hwnd_, &ps);
    }

    void ApplyMinimumTrackSize(MINMAXINFO* info) noexcept {
        if (!info || !hwnd_) return;
        RECT rc{0, 0, kMinClientWidth, kMinClientHeight};
        const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(hwnd_, GWL_STYLE));
        const DWORD exStyle = static_cast<DWORD>(GetWindowLongPtrW(hwnd_, GWL_EXSTYLE));
        AdjustWindowRectEx(&rc, style, FALSE, exStyle);
        info->ptMinTrackSize.x = rc.right - rc.left;
        info->ptMinTrackSize.y = rc.bottom - rc.top;
    }

    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
        const auto& palette = MidnightPalette();
        switch (message) {
        case WM_CREATE:
            return CreateChildren() ? 0 : -1;
        case WM_GETMINMAXINFO:
            ApplyMinimumTrackSize(reinterpret_cast<MINMAXINFO*>(lParam));
            return 0;
        case WM_SIZE:
            LayoutChildren();
            InvalidateRect(hwnd_, nullptr, TRUE);
            return 0;
        case kOverviewLogChangedMessage:
        case kPageLogChangedMessage:
            statusBar_.Refresh();
            return 0;
        case kPageStatusChangedMessage: {
            const auto* text = reinterpret_cast<const wchar_t*>(lParam);
            statusBar_.SetStatus(text ? text : L"");
            return 0;
        }
        case kRuntimeStatusMessage: {
            auto* text = reinterpret_cast<std::wstring*>(lParam);
            const auto level = static_cast<EventLevel>(wParam);
            const std::wstring messageText = text ? *text : L"Фоновая задача завершена.";
            delete text;
            statusBar_.SetStatus(messageText);
            statusBar_.AppendLog(L"Runtime", level, messageText);
            return 0;
        }
        case kRunStartupActionsMessage:
            RunStartupActions();
            return 0;
        case WM_TIMER:
            if (wParam == kMemoryTimer) { RunMemoryAutomation(); return 0; }
            if (wParam == kJunkTimer) { RunBackgroundJunkAnalysis(); return 0; }
            break;
        case kTrayMessage: {
            const auto command = tray_.HandleCallback(lParam);
            if (command == TrayIcon::Command::Restore) RestoreFromTray();
            else if (command == TrayIcon::Command::Exit) CloseApplication();
            return 0;
        }
        case WM_COMMAND: {
            const int id = LOWORD(wParam);
            if (id >= 1000 && id <= 1009) {
                ShowPage(PageForCommand(id));
                return 0;
            }
            if (id == kSettingsCommandId) {
                ShowPage(Page::Settings);
                return 0;
            }
            if (id == kSupportCommandId) {
                OpenSupport();
                return 0;
            }
            break;
        }
        case WM_DRAWITEM:
            if (DrawButton(reinterpret_cast<const DRAWITEMSTRUCT*>(lParam))) return TRUE;
            break;
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wParam);
            HWND child = reinterpret_cast<HWND>(lParam);
            const int id = child ? GetDlgCtrlID(child) : 0;
            SetBkMode(dc, OPAQUE);
            SetBkColor(dc, palette.title);
            SetDCBrushColor(dc, palette.title);
            SetTextColor(dc, id == kSubtitleId ? palette.muted : (id == kBetaId ? palette.accent : palette.text));
            return reinterpret_cast<LRESULT>(GetStockObject(DC_BRUSH));
        }
        case WM_CTLCOLOREDIT: {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetBkColor(dc, palette.control);
            SetTextColor(dc, palette.text);
            return reinterpret_cast<LRESULT>(controlBrush_);
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
            Paint();
            return 0;
        case WM_CLOSE:
            HandleClose();
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd_, kMemoryTimer);
            KillTimer(hwnd_, kJunkTimer);
            tray_.Destroy();
            if (runtimeWorker_.joinable()) runtimeWorker_.request_stop();
            PostQuitMessage(0);
            return 0;
        default:
            break;
        }
        return DefWindowProcW(hwnd_, message, wParam, lParam);
    }

    HINSTANCE instance_{};
    HWND hwnd_{};
    HWND product_{};
    HWND subtitle_{};
    HWND beta_{};
    HWND gear_{};
    HWND pageHost_{};
    std::array<HWND, 10> tabButtons_{};

    HFONT productFont_{};
    HFONT subtitleFont_{};
    HFONT tabFont_{};
    HBRUSH backgroundBrush_{};
    HBRUSH titleBrush_{};
    HBRUSH controlBrush_{};

    Page activePage_{Page::Overview};
    SessionLog sessionLog_;
    StatusBar statusBar_;
    OverviewPage overviewPage_;
    CleaningPage cleaningPage_;
    MemoryPage memoryPage_;
    GuardPage guardPage_;
    DiskPage diskPage_;
    ApplicationsPage applicationsPage_;
    WindowsPage windowsPage_;
    DuplicatesPage duplicatesPage_;
    ToolsPage toolsPage_;
    ZapretPage zapretPage_;
    SettingsPage settingsPage_;

    dpop::settings::AppSettings settings_{};
    TrayIcon tray_;
    std::jthread runtimeWorker_;
    std::atomic_bool runtimeBusy_{false};
    bool exiting_{};
};

} // namespace

int Run(HINSTANCE instance, int showCommand) {
    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&controls);
    ShellApp app{instance};
    return app.Run(showCommand);
}

} // namespace dpop::ui::shell
