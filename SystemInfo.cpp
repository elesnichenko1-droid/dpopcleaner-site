#include "modules/SystemInfo.h"
#include <windows.h>
#include <tlhelp32.h>
#include <dxgi.h>

namespace dpop::system_info {
Snapshot Collect() {
    Snapshot s{};
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    s.cpuCount = si.dwNumberOfProcessors;

    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        s.ramTotal = ms.ullTotalPhys;
        s.ramAvailable = ms.ullAvailPhys;
    }

    wchar_t windowsDir[MAX_PATH]{};
    if (GetWindowsDirectoryW(windowsDir, MAX_PATH)) {
        wchar_t root[4] = {windowsDir[0], L':', L'\\', 0};
        ULARGE_INTEGER freeBytes{}, totalBytes{};
        if (GetDiskFreeSpaceExW(root, &freeBytes, &totalBytes, nullptr)) {
            s.systemDriveTotal = totalBytes.QuadPart;
            s.systemDriveFree = freeBytes.QuadPart;
        }
    }

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe{};
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(snap, &pe)) {
            do { ++s.processCount; } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
    }

    IDXGIFactory1* factory = nullptr;
    if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory))) && factory) {
        IDXGIAdapter1* adapter = nullptr;
        for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc{};
            if (SUCCEEDED(adapter->GetDesc1(&desc)) && !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
                s.gpuName = desc.Description;
                adapter->Release();
                break;
            }
            adapter->Release();
            adapter = nullptr;
        }
        factory->Release();
    }
    if (s.gpuName.empty()) s.gpuName = L"Не определён";
    return s;
}
