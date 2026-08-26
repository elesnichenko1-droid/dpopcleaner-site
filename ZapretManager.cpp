#include "modules/ZapretManager.h"
#include "core/Paths.h"
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
}

namespace dpop::zapret {
Status QueryStatus() {
    Status s{};
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
                    s.winwsRunning = true;
                    const auto p = ProcessPath(pe.th32ProcessID);
                    if (!p.empty()) s.detectedFolder = p.parent_path();
                    break;
                }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
    }

    if (s.detectedFolder.empty()) {
        const auto exe = dpop::paths::ExecutableDir();
        const fs::path candidates[] = { exe / L"zapret", exe / L"Zapret", exe / L"bin" };
        std::error_code ec;
        for (const auto& c : candidates) {
            if (fs::exists(c / L"winws.exe", ec) || fs::exists(c / L"service.bat", ec)) { s.detectedFolder = c; break; }
            ec.clear();
        }
    }
    return s;
}

bool OpenDetectedFolder(std::wstring& error) {
    const auto s = QueryStatus();
    if (s.detectedFolder.empty()) { error = L"Папка Zapret не обнаружена автоматически."; return false; }
    if (reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", s.detectedFolder.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) <= 32) {
        error = L"Не удалось открыть папку Zapret.";
        return false;
    }
    return true;
}
}
