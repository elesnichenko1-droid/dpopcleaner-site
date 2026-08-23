#include "ui/Shell.h"

#include "ui/Controls.h"
#include "ui/Layout.h"
#include "ui/SessionLog.h"
#include "ui/ShellModel.h"
#include "ui/StatusBar.h"
#include "ui/Theme.h"
#include "ui/pages/SettingsStubPage.h"

#include <array>
#include <string>

#include <dwmapi.h>
#include <shellapi.h>
#include <uxtheme.h>

namespace dpop::ui::shell {
namespace {

constexpr wchar_t kWindowClassName[] = L"DPopCleaner032ShellWindow";
constexpr wchar_t kPageHostClassName[] = L"DPopCleaner032PageHost";

constexpr int kProductId = 1300;
constexpr int kSubtitleId = 1301;
constexpr int kBetaId = 1302;
constexpr int kPageHostId = 1303;
constexpr int kPageTitleId = 1304;
constexpr int kPageMessageId = 1305;

constexpr int kMinClientWidth = 1100;
constexpr int kMinClientHeight = 700;

std::wstring WindowText(HWND hwnd) {
    const int length = GetWindowTextLengthW(hwnd);
    if (length <= 0) {
        return {};
    }

    std::wstring text(static_cast<std::size_t>(length + 1), L'\0');
    GetWindowTextW(hwnd, text.data(), length + 1);
    text.resize(static_cast<std::size_t>(length));
    return text;
}

std::wstring_view LabelForPage(Page page) noexcept {
    for (const auto& tab : PrimaryTabs()) {
        if (tab.page == page) {
            return tab.label;
        }
    }

    if (page == Page::Settings) {
        return L"Настройки";
    }

    return L"Обзор";
}

LRESULT CALLBACK PageHostProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
) {
    const auto& palette = MidnightPalette();

    switch (message) {
    case WM_ERASEBKGND: {
        RECT rc{};
        GetClientRect(hwnd, &rc);
        HBRUSH brush = CreateSolidBrush(palette.background);
        if (brush) {
            FillRect(reinterpret_cast<HDC>(wParam), &rc, brush);
            DeleteObject(brush);
        }
        return 1;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc{};
        GetClientRect(hwnd, &rc);
        HBRUSH brush = CreateSolidBrush(palette.background);
        if (brush) {
            FillRect(dc, &rc, brush);
            DeleteObject(brush);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkMode(dc, TRANSPARENT);

        const HWND child = reinterpret_cast<HWND>(lParam);
        const int id = child ? GetDlgCtrlID(child) : 0;
        SetTextColor(
            dc,
            id == kPageMessageId ? palette.muted : palette.text
        );
        return reinterpret_cast<LRESULT>(GetStockObject(NULL_BRUSH));
    }

    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

class ShellApp {
public:
    explicit ShellApp(HINSTANCE instance) noexcept
        : instance_(instance) {
    }

    ~ShellApp() {
        if (productFont_) DeleteObject(productFont_);
        if (subtitleFont_) DeleteObject(subtitleFont_);
        if (tabFont_) DeleteObject(tabFont_);
        if (pageTitleFont_) DeleteObject(pageTitleFont_);
        if (pageBodyFont_) DeleteObject(pageBodyFont_);
        if (backgroundBrush_) DeleteObject(backgroundBrush_);
        if (titleBrush_) DeleteObject(titleBrush_);
        if (controlBrush_) DeleteObject(controlBrush_);
    }

    int Run(int showCommand) {
        if (!RegisterClasses()) {
            return 10;
        }

        if (!CreateMainWindow()) {
            return 11;
        }

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
    static LRESULT CALLBACK WindowProc(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam
    ) {
        ShellApp* self = reinterpret_cast<ShellApp*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA)
        );

        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            self = static_cast<ShellApp*>(create->lpCreateParams);
            SetWindowLongPtrW(
                hwnd,
                GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(self)
            );
            self->hwnd_ = hwnd;
        }

        if (!self) {
            return DefWindowProcW(hwnd, message, wParam, lParam);
        }

        return self->HandleMessage(message, wParam, lParam);
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

        if (!RegisterClassExW(&shellClass) &&
            GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }

        WNDCLASSEXW pageClass{};
        pageClass.cbSize = sizeof(pageClass);
        pageClass.lpfnWndProc = &PageHostProc;
        pageClass.hInstance = instance_;
        pageClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        pageClass.lpszClassName = kPageHostClassName;

        if (!RegisterClassExW(&pageClass) &&
            GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }

        return true;
    }

    bool CreateMainWindow() {
        RECT desired{0, 0, 1200, 850};
        AdjustWindowRectEx(
            &desired,
            WS_OVERLAPPEDWINDOW,
            FALSE,
            WS_EX_APPWINDOW
        );

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

        if (!hwnd_) {
            return false;
        }

        constexpr DWORD dark = 1;
        constexpr DWORD kImmersiveDarkMode = 20;
        DwmSetWindowAttribute(
            hwnd_,
            kImmersiveDarkMode,
            &dark,
            sizeof(dark)
        );
        SetWindowTheme(hwnd_, L"DarkMode_Explorer", nullptr);

        return true;
    }

    bool CreateChildren() {
        const auto& palette = MidnightPalette();

        backgroundBrush_ = CreateSolidBrush(palette.background);
        titleBrush_ = CreateSolidBrush(palette.title);
        controlBrush_ = CreateSolidBrush(palette.control);

        productFont_ = CreateUiFont(24, FW_BOLD);
        subtitleFont_ = CreateUiFont(10, FW_NORMAL);
        tabFont_ = CreateUiFont(10, FW_SEMIBOLD);
        pageTitleFont_ = CreateUiFont(22, FW_SEMIBOLD);
        pageBodyFont_ = CreateUiFont(11, FW_NORMAL);

        if (!backgroundBrush_ || !titleBrush_ || !controlBrush_ ||
            !productFont_ || !subtitleFont_ || !tabFont_ ||
            !pageTitleFont_ || !pageBodyFont_) {
            return false;
        }

        product_ = CreateTextLabel(
            hwnd_,
            kProductId,
            Identity().productName
        );
        subtitle_ = CreateTextLabel(
            hwnd_,
            kSubtitleId,
            Identity().subtitle
        );
        beta_ = CreateTextLabel(
            hwnd_,
            kBetaId,
            Identity().betaLabel
        );
        gear_ = CreatePushButton(
            hwnd_,
            kSettingsCommandId,
            L"⚙",
            ButtonVisual::Normal
        );

        pageHost_ = CreateWindowExW(
            0,
            kPageHostClassName,
            L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
            0,
            0,
            0,
            0,
            hwnd_,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(kPageHostId)
            ),
            instance_,
            nullptr
        );

        if (!product_ || !subtitle_ || !beta_ || !gear_ || !pageHost_) {
            return false;
        }

        ApplyControlFont(product_, productFont_);
        ApplyControlFont(subtitle_, subtitleFont_);
        ApplyControlFont(beta_, tabFont_);
        ApplyControlFont(gear_, tabFont_);

        std::size_t index = 0;
        for (const auto& tab : PrimaryTabs()) {
            tabButtons_[index] = CreatePushButton(
                hwnd_,
                tab.commandId,
                tab.label,
                ButtonVisual::Normal
            );
            if (!tabButtons_[index]) {
                return false;
            }
            ApplyControlFont(tabButtons_[index], tabFont_);
            ++index;
        }

        pageTitle_ = CreateTextLabel(
            pageHost_,
            kPageTitleId,
            L"Обзор"
        );
        pageMessage_ = CreateTextLabel(
            pageHost_,
            kPageMessageId,
            L"Раздел будет подключён в следующем функциональном этапе 0.3.2."
        );

        if (!pageTitle_ || !pageMessage_) {
            return false;
        }

        ApplyControlFont(pageTitle_, pageTitleFont_);
        ApplyControlFont(pageMessage_, pageBodyFont_);

        if (!settingsPage_.Create(pageHost_)) {
            return false;
        }

        if (!statusBar_.Create(hwnd_, sessionLog_)) {
            return false;
        }

        statusBar_.AppendLog(
            L"Shell",
            EventLevel::Info,
            L"DPopCleaner 0.3.2 запущен."
        );

        ShowPage(Page::Overview, false);
        LayoutChildren();

        return true;
    }

    void LayoutChildren() noexcept {
        if (!hwnd_) {
            return;
        }

        RECT client{};
        GetClientRect(hwnd_, &client);
        const int width = client.right - client.left;
        const int height = client.bottom - client.top;

        const ShellLayout layout =
            ComputeShellLayout(width, height);

        if (product_) MoveWindow(product_, 24, 16, 250, 38, TRUE);
        if (beta_) MoveWindow(beta_, 286, 22, 70, 28, TRUE);
        if (subtitle_) {
            MoveWindow(
                subtitle_,
                24,
                58,
                (width > 130) ? width - 130 : 0,
                24,
                TRUE
            );
        }
        if (gear_) {
            MoveWindow(
                gear_,
                (width > 72) ? width - 68 : 0,
                22,
                44,
                44,
                TRUE
            );
        }

        const int tabGap = 6;
        const int tabMargin = 24;
        const int totalGap = tabGap * 9;
        const int usable =
            (width > (tabMargin * 2 + totalGap))
                ? width - (tabMargin * 2) - totalGap
                : 0;
        const int tabWidth = usable / 10;
        const int remainder = usable - tabWidth * 10;
        const int tabY = layout.tabs.y + 8;
        const int tabHeight = 38;

        for (int i = 0; i < 10; ++i) {
            const int x = tabMargin + i * (tabWidth + tabGap);
            const int thisWidth =
                tabWidth + ((i == 9) ? remainder : 0);

            if (tabButtons_[static_cast<std::size_t>(i)]) {
                MoveWindow(
                    tabButtons_[static_cast<std::size_t>(i)],
                    x,
                    tabY,
                    thisWidth,
                    tabHeight,
                    TRUE
                );
            }
        }

        if (pageHost_) {
            MoveWindow(
                pageHost_,
                layout.content.x,
                layout.content.y,
                layout.content.width,
                layout.content.height,
                TRUE
            );
        }

        const int hostWidth = layout.content.width;
        const int hostHeight = layout.content.height;

        if (pageTitle_) {
            MoveWindow(
                pageTitle_,
                24,
                18,
                (hostWidth > 48) ? hostWidth - 48 : 0,
                40,
                TRUE
            );
        }
        if (pageMessage_) {
            MoveWindow(
                pageMessage_,
                24,
                72,
                (hostWidth > 48) ? hostWidth - 48 : 0,
                56,
                TRUE
            );
        }

        settingsPage_.Layout(hostWidth, hostHeight);
        statusBar_.Layout(layout);
    }

    void ShowPage(Page page, bool writeLog = true) {
        activePage_ = page;

        const bool settings = page == Page::Settings;
        settingsPage_.Show(settings);

        if (pageTitle_) {
            ShowWindow(pageTitle_, settings ? SW_HIDE : SW_SHOW);
        }
        if (pageMessage_) {
            ShowWindow(pageMessage_, settings ? SW_HIDE : SW_SHOW);
        }

        const std::wstring_view label = LabelForPage(page);

        if (!settings) {
            const std::wstring ownedLabel{label};
            SetWindowTextW(pageTitle_, ownedLabel.c_str());
            SetWindowTextW(
                pageMessage_,
                L"Раздел будет подключён в следующем функциональном этапе 0.3.2."
            );
        }

        statusBar_.SetStatus(L"Готово.");

        if (writeLog) {
            std::wstring message = L"Открыт раздел: ";
            message.append(label);
            statusBar_.AppendLog(
                L"Shell",
                EventLevel::Info,
                message
            );
        }

        for (HWND button : tabButtons_) {
            if (button) {
                InvalidateRect(button, nullptr, TRUE);
            }
        }
        if (gear_) {
            InvalidateRect(gear_, nullptr, TRUE);
        }
        if (pageHost_) {
            InvalidateRect(pageHost_, nullptr, TRUE);
        }
    }

    bool DrawButton(const DRAWITEMSTRUCT* draw) {
        if (!draw || draw->CtlType != ODT_BUTTON || !draw->hwndItem) {
            return false;
        }

        ButtonVisual visual = ButtonVisual::Normal;
        const int id = static_cast<int>(draw->CtlID);

        if (id >= 1000 && id <= 1009) {
            const Page page = PageForCommand(id);
            if (page == activePage_) {
                visual = ButtonVisual::Accent;
            }
        } else if (
            id == kSettingsCommandId &&
            activePage_ == Page::Settings
        ) {
            visual = ButtonVisual::Accent;
        }

        return DrawOwnerButton(
            *draw,
            WindowText(draw->hwndItem),
            visual
        );
    }

    void OpenSupport() {
        const auto result = reinterpret_cast<INT_PTR>(
            ShellExecuteW(
                hwnd_,
                L"open",
                L"https://elesnichenko1-droid.github.io/dpopcleaner-site/",
                nullptr,
                nullptr,
                SW_SHOWNORMAL
            )
        );

        if (result <= 32) {
            statusBar_.AppendLog(
                L"Shell",
                EventLevel::Warning,
                L"Не удалось открыть страницу поддержки."
            );
            return;
        }

        statusBar_.AppendLog(
            L"Shell",
            EventLevel::Info,
            L"Открыта страница поддержки."
        );
    }

    void Paint() {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd_, &ps);

        RECT client{};
        GetClientRect(hwnd_, &client);
        FillRect(dc, &client, backgroundBrush_);

        const ShellLayout layout =
            ComputeShellLayout(
                client.right - client.left,
                client.bottom - client.top
            );

        RECT header{
            layout.header.x,
            layout.header.y,
            layout.header.x + layout.header.width,
            layout.header.y + layout.header.height
        };
        FillRect(dc, &header, titleBrush_);

        RECT tabs{
            layout.tabs.x,
            layout.tabs.y,
            layout.tabs.x + layout.tabs.width,
            layout.tabs.y + layout.tabs.height
        };
        FillRect(dc, &tabs, titleBrush_);

        RECT footer{
            layout.footer.x,
            layout.footer.y,
            layout.footer.x + layout.footer.width,
            layout.footer.y + layout.footer.height
        };
        FillRect(dc, &footer, titleBrush_);

        EndPaint(hwnd_, &ps);
    }

    void ApplyMinimumTrackSize(MINMAXINFO* info) noexcept {
        if (!info || !hwnd_) {
            return;
        }

        RECT rc{0, 0, kMinClientWidth, kMinClientHeight};

        const DWORD style = static_cast<DWORD>(
            GetWindowLongPtrW(hwnd_, GWL_STYLE)
        );
        const DWORD exStyle = static_cast<DWORD>(
            GetWindowLongPtrW(hwnd_, GWL_EXSTYLE)
        );

        AdjustWindowRectEx(&rc, style, FALSE, exStyle);

        info->ptMinTrackSize.x = rc.right - rc.left;
        info->ptMinTrackSize.y = rc.bottom - rc.top;
    }

    LRESULT HandleMessage(
        UINT message,
        WPARAM wParam,
        LPARAM lParam
    ) {
        const auto& palette = MidnightPalette();

        switch (message) {
        case WM_CREATE:
            return CreateChildren() ? 0 : -1;

        case WM_GETMINMAXINFO:
            ApplyMinimumTrackSize(
                reinterpret_cast<MINMAXINFO*>(lParam)
            );
            return 0;

        case WM_SIZE:
            LayoutChildren();
            InvalidateRect(hwnd_, nullptr, TRUE);
            return 0;

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
            if (DrawButton(
                    reinterpret_cast<const DRAWITEMSTRUCT*>(lParam))) {
                return TRUE;
            }
            break;

        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wParam);
            HWND child = reinterpret_cast<HWND>(lParam);

            if (child == statusBar_.LogControl()) {
                SetBkMode(dc, OPAQUE);
                SetBkColor(dc, palette.control);
                SetTextColor(dc, palette.text);
                return reinterpret_cast<LRESULT>(controlBrush_);
            }

            const int id = child ? GetDlgCtrlID(child) : 0;
            SetBkMode(dc, TRANSPARENT);

            if (id == kSubtitleId) {
                SetTextColor(dc, palette.muted);
            } else if (id == kBetaId) {
                SetTextColor(dc, palette.accent);
            } else {
                SetTextColor(dc, palette.text);
            }

            return reinterpret_cast<LRESULT>(
                GetStockObject(NULL_BRUSH)
            );
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
            DestroyWindow(hwnd_);
            return 0;

        case WM_DESTROY:
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
    HWND pageTitle_{};
    HWND pageMessage_{};

    std::array<HWND, 10> tabButtons_{};

    HFONT productFont_{};
    HFONT subtitleFont_{};
    HFONT tabFont_{};
    HFONT pageTitleFont_{};
    HFONT pageBodyFont_{};

    HBRUSH backgroundBrush_{};
    HBRUSH titleBrush_{};
    HBRUSH controlBrush_{};

    Page activePage_{Page::Overview};
    SessionLog sessionLog_;
    StatusBar statusBar_;
    SettingsStubPage settingsPage_;
};

}

int Run(HINSTANCE instance, int showCommand) {
    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&controls);

    ShellApp app{instance};
    return app.Run(showCommand);
}

}
