#include "modules/ZapretManager.h"
#include <windows.h>
#include <tlhelp32.h>
#include <winsvc.h>
#include <cwchar>

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
                if (_wcsicmp(pe.szExeFile, L"winws.exe") == 0) { s.winwsRunning = true; break; }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
    }
    return s;
}
}
