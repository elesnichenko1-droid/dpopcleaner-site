#include "modules/SettingsStore.h"
#include "ui/SessionLog.h"
#include "ui/pages/SettingsPage.h"

#include <commctrl.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <windows.h>

using namespace dpop::settings;
namespace fs = std::filesystem;

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct TempSettingsRoot {
    fs::path path;

    TempSettingsRoot() {
        wchar_t buffer[MAX_PATH]{};
        const DWORD n = GetTempPathW(MAX_PATH, buffer);
        Require(n > 0 && n < MAX_PATH, "GetTempPathW failed");
        path = fs::path(buffer) /
               (L"dpop-settings-page-test-" + std::to_wstring(GetCurrentProcessId()) +
                L"-" + std::to_wstring(GetTickCount64()));
        Require(fs::create_directories(path), "could not create temporary settings root");
        Require(SetEnvironmentVariableW(L"DPOP_SETTINGS_ROOT", path.c_str()) != FALSE,
                "could not set DPOP_SETTINGS_ROOT");
    }

    ~TempSettingsRoot() {
        SetEnvironmentVariableW(L"DPOP_SETTINGS_ROOT", nullptr);
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

std::wstring WindowText(HWND hwnd) {
    Require(hwnd != nullptr, "window handle is null");
    const int length = GetWindowTextLengthW(hwnd);
    Require(length >= 0, "GetWindowTextLengthW failed");
    std::wstring text(static_cast<std::size_t>(length + 1), L'\0');
    const int copied = GetWindowTextW(hwnd, text.data(), length + 1);
    Require(copied == length, "GetWindowTextW returned unexpected length");
    text.resize(static_cast<std::size_t>(length));
    return text;
}

void InitPageControls() {
    INITCOMMONCONTROLSEX common{};
    common.dwSize = sizeof(common);
    common.dwICC = ICC_LISTVIEW_CLASSES;
    (void)InitCommonControlsEx(&common);
}

HWND CreateHiddenHost() {
    HWND host = CreateWindowExW(
        0,
        L"STATIC",
        L"DPopCleaner Settings integration host",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1200,
        850,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr);
    Require(host != nullptr, "could not create Settings integration host");
    return host;
}

void SelectCleaning(dpop::ui::SettingsPage& page) {
    HWND button = GetDlgItem(page.Hwnd(), 3301);
    Require(button != nullptr, "Cleaning section button 3301 missing");
    SendMessageW(
        page.Hwnd(),
        WM_COMMAND,
        MAKEWPARAM(3301, BN_CLICKED),
        reinterpret_cast<LPARAM>(button));
}

void SavePage(dpop::ui::SettingsPage& page) {
    HWND save = GetDlgItem(page.Hwnd(), 3431);
    Require(save != nullptr, "Settings Save button 3431 missing");
    SendMessageW(
        page.Hwnd(),
        WM_COMMAND,
        MAKEWPARAM(3431, BN_CLICKED),
        reinterpret_cast<LPARAM>(save));
}

void TestSettingsPageHandlesOwnerDrawButtons() {
    TempSettingsRoot temp;
    InitPageControls();

    HWND host = CreateHiddenHost();
    dpop::ui::SessionLog log;
    dpop::ui::SettingsPage page;
    Require(page.Create(host, log), "SettingsPage::Create failed for owner-draw test");
    page.Layout({0, 0, 1200, 850});

    HWND button = GetDlgItem(page.Hwnd(), 3300);
    Require(button != nullptr, "General section button 3300 missing");

    HDC windowDc = GetDC(page.Hwnd());
    Require(windowDc != nullptr, "GetDC failed for owner-draw test");
    HDC memoryDc = CreateCompatibleDC(windowDc);
    Require(memoryDc != nullptr, "CreateCompatibleDC failed for owner-draw test");
    HBITMAP bitmap = CreateCompatibleBitmap(windowDc, 240, 48);
    Require(bitmap != nullptr, "CreateCompatibleBitmap failed for owner-draw test");
    HGDIOBJ oldBitmap = SelectObject(memoryDc, bitmap);

    DRAWITEMSTRUCT draw{};
    draw.CtlType = ODT_BUTTON;
    draw.CtlID = 3300;
    draw.itemAction = ODA_DRAWENTIRE;
    draw.hwndItem = button;
    draw.hDC = memoryDc;
    draw.rcItem = RECT{0, 0, 240, 48};

    const LRESULT handled = SendMessageW(
        page.Hwnd(),
        WM_DRAWITEM,
        static_cast<WPARAM>(draw.CtlID),
        reinterpret_cast<LPARAM>(&draw));

    SelectObject(memoryDc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memoryDc);
    ReleaseDC(page.Hwnd(), windowDc);

    Require(handled == TRUE,
            "PageBase did not handle WM_DRAWITEM for a page-owned button");
    Require(DestroyWindow(host) != FALSE, "could not destroy owner-draw integration host");
}

void TestSettingsPagePersistsAndRestoresLargeFileThreshold() {
    TempSettingsRoot temp;

    auto seed = DefaultSettings();
    seed.largeFileMB = 500;
    std::wstring error;
    Require(SaveAppSettings(seed, error), "could not seed settings.json");
    Require(error.empty(), "seeding settings.json returned an error");

    InitPageControls();

    HWND host = CreateHiddenHost();
    dpop::ui::SessionLog log;

    {
        dpop::ui::SettingsPage page;
        bool callbackCalled = false;
        unsigned callbackLargeFileMB = 0;
        Require(page.Create(host, log, [&](const AppSettings& applied) {
            callbackCalled = true;
            callbackLargeFileMB = applied.largeFileMB;
        }), "SettingsPage::Create failed");

        page.Layout({0, 0, 1200, 850});
        SelectCleaning(page);

        HWND large = GetDlgItem(page.Hwnd(), 3343);
        Require(large != nullptr, "large-file edit 3343 missing");
        Require(WindowText(large) == L"500", "initial large-file value is not 500");
        Require(SetWindowTextW(large, L"777") != FALSE, "SetWindowTextW(777) failed in same process");
        Require(WindowText(large) == L"777", "large-file edit did not change to 777");

        SavePage(page);

        Require(callbackCalled, "Settings apply callback was not called");
        Require(callbackLargeFileMB == 777, "apply callback did not receive large_file_mb=777");
        Require(WindowText(large) == L"777", "large-file edit changed unexpectedly after Save");

        const auto persisted = LoadAppSettings();
        Require(!persisted.usedDefaults, "LoadAppSettings fell back to defaults after Save");
        Require(persisted.warning.empty(), "LoadAppSettings returned warning after Save");
        Require(persisted.settings.largeFileMB == 777,
                "settings.json did not persist large_file_mb=777");
    }

    {
        dpop::ui::SettingsPage restarted;
        Require(restarted.Create(host, log), "restarted SettingsPage::Create failed");
        restarted.Layout({0, 0, 1200, 850});
        SelectCleaning(restarted);

        HWND large = GetDlgItem(restarted.Hwnd(), 3343);
        Require(large != nullptr, "restarted large-file edit 3343 missing");
        Require(WindowText(large) == L"777",
                "restarted SettingsPage did not restore large_file_mb=777");
    }

    Require(DestroyWindow(host) != FALSE, "could not destroy Settings integration host");
}

} // namespace

int main() {
    try {
        TestSettingsPageHandlesOwnerDrawButtons();
        TestSettingsPagePersistsAndRestoresLargeFileThreshold();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "SettingsPageIntegrationTests: " << ex.what() << '\n';
        return 1;
    }
}
