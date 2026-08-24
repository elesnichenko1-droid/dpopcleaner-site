#include "ui/Shell.h"

#include "ui/Controls.h"
#include "ui/Layout.h"
#include "ui/PageBase.h"
#include "ui/SessionLog.h"
#include "ui/ShellModel.h"
#include "ui/StatusBar.h"
#include "ui/Theme.h"
#include "modules/FullCore.h"
#include "ui/pages/ApplicationsPage.h"
#include "ui/pages/CleaningPage.h"
#include "ui/pages/DiskPage.h"
#include "ui/pages/DuplicatesPage.h"
#include "ui/pages/GuardPage.h"
#include "ui/pages/MemoryPage.h"
#include "ui/pages/OverviewPage.h"
#include "ui/pages/SettingsPage.h"
#include "ui/pages/StartupPage.h"
#include "ui/pages/ToolsPage.h"
#include "ui/pages/UpdatesPage.h"
#include "ui/pages/WindowsPage.h"
#include "ui/pages/ZapretPage.h"

#include <array>
#include <string>

#include <dwmapi.h>
#include <shellapi.h>
#include <uxtheme.h>

namespace dpop::ui::shell {
namespace {
constexpr wchar_t kWindowClassName[] = L"DPopCleaner034ShellWindow";
constexpr wchar_t kPageHostClassName[] = L"DPopCleaner034PageHost";
constexpr int kProductId = 1300;
constexpr int kSubtitleId = 1301;
constexpr int kBetaId = 1302;
constexpr int kPageHostId = 1303;
constexpr int kMinClientWidth = 1100;
constexpr int kMinClientHeight = 700;
constexpr UINT kRunStartupActionsMessage = WM_APP + 0x72;

std::wstring WindowText(HWND hwnd) {
    const int length = GetWindowTextLengthW(hwnd);
    if (length <= 0) return {};
    std::wstring text(static_cast<std::size_t>(length + 1), L'\0');
    GetWindowTextW(hwnd, text.data(), length + 1);
    text.resize(static_cast<std::size_t>(length));
    return text;
}

std::wstring_view LabelForPage(Page page) noexcept {
    for (const auto& tab : PrimaryTabs()) if (tab.page == page) return tab.label;
    return L"Обзор";
}

LRESULT CALLBACK PageHostProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    const auto& palette = MidnightPalette();
    switch (message) {
    case WM_ERASEBKGND: {
        RECT rc{}; GetClientRect(hwnd, &rc);
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetDCBrushColor(dc, palette.background);
        FillRect(dc, &rc, reinterpret_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
        return 1;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps{}; HDC dc = BeginPaint(hwnd, &ps); RECT rc{}; GetClientRect(hwnd, &rc);
        SetDCBrushColor(dc, palette.background);
        FillRect(dc, &rc, reinterpret_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
        EndPaint(hwnd, &ps); return 0;
    }
    default: return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

class ShellApp {
public:
    explicit ShellApp(HINSTANCE instance) noexcept : instance_(instance) {}
    ~ShellApp() {
        if (productFont_) DeleteObject(productFont_);
        if (subtitleFont_) DeleteObject(subtitleFont_);
        if (tabFont_) DeleteObject(tabFont_);
        if (backgroundBrush_) DeleteObject(backgroundBrush_);
        if (sidebarBrush_) DeleteObject(sidebarBrush_);
        if (controlBrush_) DeleteObject(controlBrush_);
    }

    int Run(int showCommand) {
        if (!RegisterClasses()) return 10;
        if (!CreateMainWindow()) return 11;
        ShowWindow(hwnd_, showCommand); UpdateWindow(hwnd_);
        MSG message{}; int result = 0;
        while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
            TranslateMessage(&message); DispatchMessageW(&message);
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
        return self ? self->HandleMessage(message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
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
            WS_EX_APPWINDOW, kWindowClassName, Identity().windowTitle.data(),
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            CW_USEDEFAULT, CW_USEDEFAULT,
            desired.right - desired.left, desired.bottom - desired.top,
            nullptr, nullptr, instance_, this);
        if (!hwnd_) return false;
        constexpr DWORD dark = 1; constexpr DWORD kImmersiveDarkMode = 20;
        DwmSetWindowAttribute(hwnd_, kImmersiveDarkMode, &dark, sizeof(dark));
        SetWindowTheme(hwnd_, L"DarkMode_Explorer", nullptr);
        return true;
    }

    bool CreateChildren() {
        const auto& palette = MidnightPalette();
        backgroundBrush_ = CreateSolidBrush(palette.background);
        sidebarBrush_ = CreateSolidBrush(palette.title);
        controlBrush_ = CreateSolidBrush(palette.control);
        productFont_ = CreateUiFont(22, FW_BOLD);
        subtitleFont_ = CreateUiFont(9, FW_NORMAL);
        tabFont_ = CreateUiFont(9, FW_SEMIBOLD);
        if (!backgroundBrush_ || !sidebarBrush_ || !controlBrush_ || !productFont_ || !subtitleFont_ || !tabFont_) return false;

        product_ = CreateTextLabel(hwnd_, kProductId, Identity().productName);
        subtitle_ = CreateTextLabel(hwnd_, kSubtitleId, Identity().subtitle);
        beta_ = CreateTextLabel(hwnd_, kBetaId, Identity().betaLabel);
        pageHost_ = CreateWindowExW(
            0, kPageHostClassName, L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
            0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPageHostId)), instance_, nullptr);
        if (!product_ || !subtitle_ || !beta_ || !pageHost_) return false;
        ApplyControlFont(product_, productFont_);
        ApplyControlFont(subtitle_, subtitleFont_);
        ApplyControlFont(beta_, tabFont_);

        std::size_t index = 0;
        for (const auto& tab : PrimaryTabs()) {
            navButtons_[index] = CreatePushButton(hwnd_, tab.commandId, tab.label, ButtonVisual::Normal);
            if (!navButtons_[index]) return false;
            ApplyControlFont(navButtons_[index], tabFont_);
            ++index;
        }

        if (!statusBar_.Create(hwnd_, sessionLog_)) return false;
        statusBar_.AppendLog(L"Shell", EventLevel::Info, L"DPopCleaner 0.3.4 запущен.");

        if (!overviewPage_.Create(pageHost_, sessionLog_, [this](Page page) { ShowPage(page); })) return false;
        if (!cleaningPage_.Create(pageHost_, sessionLog_)) return false;
        if (!memoryPage_.Create(pageHost_, sessionLog_)) return false;
        if (!guardPage_.Create(pageHost_, sessionLog_)) return false;
        if (!startupPage_.Create(pageHost_, sessionLog_)) return false;
        if (!diskPage_.Create(pageHost_, sessionLog_)) return false;
        if (!applicationsPage_.Create(pageHost_, sessionLog_)) return false;
        if (!windowsPage_.Create(pageHost_, sessionLog_)) return false;
        if (!duplicatesPage_.Create(pageHost_, sessionLog_)) return false;
        if (!toolsPage_.Create(pageHost_, sessionLog_)) return false;
        if (!zapretPage_.Create(pageHost_, sessionLog_)) return false;
        if (!updatesPage_.Create(pageHost_, sessionLog_)) return false;
        if (!settingsPage_.Create(pageHost_, sessionLog_)) return false;

        ShowPage(Page::Overview, false);
        LayoutChildren();
        PostMessageW(hwnd_, kRunStartupActionsMessage, 0, 0);
        return true;
    }

    void LayoutChildren() noexcept {
        if (!hwnd_) return;
        RECT client{}; GetClientRect(hwnd_, &client);
        const int width = client.right - client.left;
        const int height = client.bottom - client.top;
        const ShellLayout layout = ComputeShellLayout(width, height);

        if (product_) MoveWindow(product_, 18, 16, 176, 34, TRUE);
        if (beta_) MoveWindow(beta_, 18, 52, 58, 22, TRUE);
        if (subtitle_) MoveWindow(subtitle_, 18, 76, 176, 22, TRUE);
        for (std::size_t i = 0; i < navButtons_.size(); ++i) {
            const auto& b = layout.navButtons[i];
            if (navButtons_[i]) MoveWindow(navButtons_[i], b.x, b.y, b.width, b.height, TRUE);
        }

        if (pageHost_) MoveWindow(pageHost_, layout.content.x, layout.content.y, layout.content.width, layout.content.height, TRUE);
        const Box pageBox{0, 0, layout.content.width, layout.content.height};
        overviewPage_.Layout(pageBox); cleaningPage_.Layout(pageBox); memoryPage_.Layout(pageBox);
        guardPage_.Layout(pageBox); startupPage_.Layout(pageBox); diskPage_.Layout(pageBox);
        applicationsPage_.Layout(pageBox); windowsPage_.Layout(pageBox); duplicatesPage_.Layout(pageBox);
        toolsPage_.Layout(pageBox); zapretPage_.Layout(pageBox); updatesPage_.Layout(pageBox); settingsPage_.Layout(pageBox);
        statusBar_.Layout(layout);
    }

    void SetPageVisibility(Page page) noexcept {
        overviewPage_.Show(page == Page::Overview);
        cleaningPage_.Show(page == Page::Cleaning);
        memoryPage_.Show(page == Page::Memory);
        guardPage_.Show(page == Page::Guard);
        startupPage_.Show(page == Page::Startup);
        diskPage_.Show(page == Page::Disk);
        applicationsPage_.Show(page == Page::Applications);
        windowsPage_.Show(page == Page::WindowsUpdate);
        duplicatesPage_.Show(page == Page::Duplicates);
        toolsPage_.Show(page == Page::Tools);
        zapretPage_.Show(page == Page::Zapret);
        updatesPage_.Show(page == Page::Updates);
        settingsPage_.Show(page == Page::Settings);
    }

    void ShowPage(Page page, bool writeLog = true) {
        activePage_ = page;
        SetPageVisibility(page);
        statusBar_.SetStatus(L"Готово.");
        if (writeLog) {
            std::wstring message = L"Открыт раздел: "; message.append(LabelForPage(page));
            statusBar_.AppendLog(L"Shell", EventLevel::Info, message);
        }
        for (HWND button : navButtons_) if (button) InvalidateRect(button, nullptr, TRUE);
        if (pageHost_) InvalidateRect(pageHost_, nullptr, TRUE);
    }

    void CancelAllPages() noexcept {
        cleaningPage_.Cancel(); memoryPage_.Cancel(); guardPage_.Cancel(); startupPage_.Cancel();
        diskPage_.Cancel(); applicationsPage_.Cancel(); windowsPage_.Cancel(); duplicatesPage_.Cancel();
        toolsPage_.Cancel(); zapretPage_.Cancel(); updatesPage_.Cancel(); settingsPage_.Cancel();
    }

    bool DrawButton(const DRAWITEMSTRUCT* draw) {
        if (!draw || draw->CtlType != ODT_BUTTON || !draw->hwndItem) return false;
        ButtonVisual visual = ButtonVisual::Normal;
        const int id = static_cast<int>(draw->CtlID);
        if (id >= 1000 && id <= 1012 && PageForCommand(id) == activePage_) visual = ButtonVisual::Accent;
        return DrawOwnerButton(*draw, WindowText(draw->hwndItem), visual);
    }

    void OpenSupport() {
        const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(
            hwnd_, L"open", L"https://elesnichenko1-droid.github.io/dpopcleaner-site/",
            nullptr, nullptr, SW_SHOWNORMAL));
        statusBar_.AppendLog(L"Shell", result <= 32 ? EventLevel::Warning : EventLevel::Info,
            result <= 32 ? L"Не удалось открыть страницу поддержки." : L"Открыта страница поддержки.");
    }

    void Paint() {
        PAINTSTRUCT ps{}; HDC dc = BeginPaint(hwnd_, &ps);
        RECT client{}; GetClientRect(hwnd_, &client);
        FillRect(dc, &client, backgroundBrush_);
        const ShellLayout layout = ComputeShellLayout(client.right, client.bottom);
        RECT sidebar{layout.sidebar.x, layout.sidebar.y, layout.sidebar.x + layout.sidebar.width, layout.sidebar.y + layout.sidebar.height};
        RECT footer{layout.footer.x, layout.footer.y, layout.footer.x + layout.footer.width, layout.footer.y + layout.footer.height};
        FillRect(dc, &sidebar, sidebarBrush_);
        FillRect(dc, &footer, sidebarBrush_);
        const auto& palette = MidnightPalette();
        HPEN pen = CreatePen(PS_SOLID, 1, palette.border);
        if (pen) {
            HGDIOBJ old = SelectObject(dc, pen);
            MoveToEx(dc, layout.sidebar.width, 0, nullptr); LineTo(dc, layout.sidebar.width, client.bottom);
            SelectObject(dc, old); DeleteObject(pen);
        }
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
        case WM_CREATE: return CreateChildren() ? 0 : -1;
        case WM_GETMINMAXINFO: ApplyMinimumTrackSize(reinterpret_cast<MINMAXINFO*>(lParam)); return 0;
        case WM_SIZE: LayoutChildren(); InvalidateRect(hwnd_, nullptr, TRUE); return 0;
        case kRunStartupActionsMessage: {
            const auto settings = dpop::full::LoadSettings();
            if (settings.quickGuardAtStartup) guardPage_.RunQuickScanAtStartup();
            if (settings.checkUpdateCacheAtStartup) windowsPage_.CheckUpdateCacheAtStartup();
            if (settings.checkUpdatesAtStartup) updatesPage_.CheckAtStartup();
            return 0;
        }
        case kOverviewLogChangedMessage:
        case kPageLogChangedMessage: statusBar_.Refresh(); return 0;
        case kPageStatusChangedMessage: {
            const auto* text = reinterpret_cast<const wchar_t*>(lParam); statusBar_.SetStatus(text ? text : L""); return 0;
        }
        case WM_COMMAND: {
            const int id = LOWORD(wParam);
            if (id >= 1000 && id <= 1012) { ShowPage(PageForCommand(id)); return 0; }
            if (id == kSupportCommandId) { OpenSupport(); return 0; }
            break;
        }
        case WM_DRAWITEM: if (DrawButton(reinterpret_cast<const DRAWITEMSTRUCT*>(lParam))) return TRUE; break;
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wParam); HWND child = reinterpret_cast<HWND>(lParam);
            const int id = child ? GetDlgCtrlID(child) : 0;
            SetBkMode(dc, OPAQUE); SetBkColor(dc, palette.title); SetDCBrushColor(dc, palette.title);
            SetTextColor(dc, id == kSubtitleId ? palette.muted : (id == kBetaId ? palette.accent : palette.text));
            return reinterpret_cast<LRESULT>(GetStockObject(DC_BRUSH));
        }
        case WM_CTLCOLOREDIT: {
            HDC dc = reinterpret_cast<HDC>(wParam); SetBkColor(dc, palette.control); SetTextColor(dc, palette.text);
            return reinterpret_cast<LRESULT>(controlBrush_);
        }
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: Paint(); return 0;
        case WM_CLOSE: CancelAllPages(); DestroyWindow(hwnd_); return 0;
        case WM_DESTROY: PostQuitMessage(0); return 0;
        default: break;
        }
        return DefWindowProcW(hwnd_, message, wParam, lParam);
    }

    HINSTANCE instance_{};
    HWND hwnd_{};
    HWND product_{};
    HWND subtitle_{};
    HWND beta_{};
    HWND pageHost_{};
    std::array<HWND, 13> navButtons_{};
    HFONT productFont_{};
    HFONT subtitleFont_{};
    HFONT tabFont_{};
    HBRUSH backgroundBrush_{};
    HBRUSH sidebarBrush_{};
    HBRUSH controlBrush_{};
    Page activePage_{Page::Overview};
    SessionLog sessionLog_;
    StatusBar statusBar_;
    OverviewPage overviewPage_;
    CleaningPage cleaningPage_;
    MemoryPage memoryPage_;
    GuardPage guardPage_;
    StartupPage startupPage_;
    DiskPage diskPage_;
    ApplicationsPage applicationsPage_;
    WindowsPage windowsPage_;
    DuplicatesPage duplicatesPage_;
    ToolsPage toolsPage_;
    ZapretPage zapretPage_;
    UpdatesPage updatesPage_;
    SettingsPage settingsPage_;
};
}

int Run(HINSTANCE instance, int showCommand) {
    INITCOMMONCONTROLSEX controls{}; controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&controls);
    ShellApp app{instance}; return app.Run(showCommand);
}

}
