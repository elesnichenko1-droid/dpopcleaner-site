#include "modules/ZapretManager.h"
#include "core/Paths.h"
#include "modules/ZapretPolicy.h"
#include <windows.h>
#include <tlhelp32.h>
#include <winsvc.h>
#include <shellapi.h>
#include <filesystem>
#include <cwchar>

namespace fs = std::filesystem;

namespace {
fs::path ProcessPath(DWORD pid) {
    HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!p) return {};
    std::wstring path(32768, L'\0');
    DWORD size = static_cast<DWORD>(path.size());
    if (!QueryFullProcessImageNameW(p, 0, path.data(), &size)) path.clear();
    else path.resize(size);
    CloseHandle(p);
    return path;
}

fs::path BundleRoot() {
    return dpop::zapret::ResolveBundledRoot(dpop::paths::ExecutableDir());
}

bool RequireValidBundle(fs::path& root, std::wstring& error) {
    root = BundleRoot();
    const auto validation = dpop::zapret::ValidateBundle(root);
    if (!validation.valid) {
        error = L"Комплект Zapret повреждён или неполон. Не найден файл: " + validation.missing.wstring();
        return false;
    }
    return true;
}

bool LaunchBatch(const wchar_t* relativeScript, std::wstring& error) {
    fs::path root;
    if (!RequireValidBundle(root, error)) return false;
    const fs::path script = root / relativeScript;
    const std::wstring arguments = L"/d /c \"\"" + script.wstring() + L"\"\"";
    SHELLEXECUTEINFOW execute{};
    execute.cbSize = sizeof(execute);
    execute.lpFile = L"cmd.exe";
    execute.lpParameters = arguments.c_str();
    execute.lpDirectory = root.c_str();
    execute.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&execute)) {
        error = L"Не удалось запустить " + script.wstring();
        return false;
    }
    return true;
}
}

namespace dpop::zapret {
Status QueryStatus() {
    Status s{};
    s.bundleFolder = BundleRoot();
    const auto validation = ValidateBundle(s.bundleFolder);
    s.bundleValid = validation.valid;
    s.missingBundleFile = validation.missing;
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (scm) {
        SC_HANDLE svc = OpenServiceW(scm, L"zapret", SERVICE_QUERY_STATUS);
        if (svc) {
            s.serviceInstalled = true;
            SERVICE_STATUS status{};
            if (QueryServiceStatus(svc, &status)) s.serviceRunning = status.dwCurrentState == SERVICE_RUNNING;
            CloseServiceHandle(svc);
        }
        CloseServiceHandle(scm);
    }

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe{}; pe.dwSize = sizeof(pe);
        if (Process32FirstW(snap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, L"winws.exe") == 0) {
                    const auto p = ProcessPath(pe.th32ProcessID);
                    if (IsBundledWinwsPath(s.bundleFolder, p)) {
                        s.winwsRunning = true;
                        break;
                    }
                }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
    }

    return s;
}

bool LaunchDefaultStrategy(std::wstring& error) {
    return LaunchBatch(L"general.bat", error);
}

bool OpenServiceManager(std::wstring& error) {
    return LaunchBatch(L"service.bat", error);
}

bool OpenBundledFolder(std::wstring& error) {
    fs::path root;
    if (!RequireValidBundle(root, error)) return false;
    if (reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", root.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) <= 32) {
        error = L"Не удалось открыть папку Zapret.";
        return false;
    }
    return true;
}
}
