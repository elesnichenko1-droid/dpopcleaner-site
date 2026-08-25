#include "modules/SettingsStore.h"
#include "ui/SessionLog.h"
#include "ui/pages/SettingsPage.h"

#include <cassert>
#include <commctrl.h>
#include <filesystem>
#include <string>
#include <windows.h>

using namespace dpop::settings;
namespace fs = std::filesystem;

namespace {

struct TempSettingsRoot {
    fs::path path;

    TempSettingsRoot() {
        wchar_t buffer[MAX_PATH]{};
        const DWORD n = GetTempPathW(MAX_PATH, buffer);
        assert(n > 0 && n < MAX_PATH);
        path = fs::path(buffer) /
               (L"dpop-settings-page-test-" + std::to_wstring(GetCurrentProcessId()) +
                L"-" + std::to_wstring(GetTickCount64()));
        fs::create_directories(path);
        assert(SetEnvironmentVariableW(L"DPOP_SETTINGS_ROOT", path.c_str()));
    }

    ~TempSettingsRoot() {
        SetEnvironmentVariableW(L"DPOP_SETTINGS_ROOT", nullptr);
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

std::wstring WindowText(HWND hwnd) {
    assert(hwnd);
    const int length = GetWindowTextLengthW(hwnd);
    std::wstring text(static_cast<std::size_t>(length + 1), L'\0');
    const int copied = GetWindowTextW(hwnd, text.data(), length + 1);
    assert(copied == length);
    text.resize(static_cast<std::size_t>(length));
    return text;
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
    assert(host);
    return host;
}

void SelectCleaning(dpop::ui::SettingsPage& page) {
    HWND button = GetDlgItem(page.Hwnd(), 3301);
    assert(button);
    SendMessageW(
        page.Hwnd(),
        WM_COMMAND,
        MAKEWPARAM(3301, BN_CLICKED),
        reinterpret_cast<LPARAM>(button));
}

void SavePage(dpop::ui::SettingsPage& page) {
    HWND save = GetDlgItem(page.Hwnd(), 3431);
    assert(save);
    SendMessageW(
        page.Hwnd(),
        WM_COMMAND,
        MAKEWPARAM(3431, BN_CLICKED),
        reinterpret_cast<LPARAM>(save));
}

void TestSettingsPagePersistsAndRestoresLargeFileThreshold() {
    TempSettingsRoot temp;

    auto seed = DefaultSettings();
    seed.largeFileMB = 500;
    std::wstring error;
    assert(SaveAppSettings(seed, error));
    assert(error.empty());

    INITCOMMONCONTROLSEX common{};
    common.dwSize = sizeof(common);
    common.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
    assert(InitCommonControlsEx(&common));

    HWND host = CreateHiddenHost();
    dpop::ui::SessionLog log;

    {
        dpop::ui::SettingsPage page;
        bool callbackCalled = false;
        unsigned callbackLargeFileMB = 0;
        assert(page.Create(host, log, [&](const AppSettings& applied) {
            callbackCalled = true;
            callbackLargeFileMB = applied.largeFileMB;
        }));

        page.Layout({0, 0, 1200, 850});
        SelectCleaning(page);

        HWND large = GetDlgItem(page.Hwnd(), 3343);
        assert(large);
        assert(WindowText(large) == L"500");
        assert(SetWindowTextW(large, L"777"));
        assert(WindowText(large) == L"777");

        SavePage(page);

        assert(callbackCalled);
        assert(callbackLargeFileMB == 777);
        assert(WindowText(large) == L"777");

        const auto persisted = LoadAppSettings();
        assert(!persisted.usedDefaults);
        assert(persisted.warning.empty());
        assert(persisted.settings.largeFileMB == 777);
    }

    {
        dpop::ui::SettingsPage restarted;
        assert(restarted.Create(host, log));
        restarted.Layout({0, 0, 1200, 850});
        SelectCleaning(restarted);

        HWND large = GetDlgItem(restarted.Hwnd(), 3343);
        assert(large);
        assert(WindowText(large) == L"777");
    }

    DestroyWindow(host);
}

} // namespace

int main() {
    TestSettingsPagePersistsAndRestoresLargeFileThreshold();
    return 0;
}
