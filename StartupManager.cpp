#include "modules/StartupManager.h"
#include <windows.h>
#include <vector>

namespace dpop::startup {
std::vector<Entry> EnumerateCurrentUserRun() {
    std::vector<Entry> out;
    HKEY key{};
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &key) != ERROR_SUCCESS) return out;

    for (DWORD index = 0;; ++index) {
        wchar_t name[512]{};
        BYTE data[4096]{};
        DWORD nameLen = 512, dataLen = sizeof(data), type = 0;
        const LONG rc = RegEnumValueW(key, index, name, &nameLen, nullptr, &type, data, &dataLen);
        if (rc == ERROR_NO_MORE_ITEMS) break;
        if (rc != ERROR_SUCCESS) continue;
        if (type == REG_SZ || type == REG_EXPAND_SZ) {
            const auto* text = reinterpret_cast<const wchar_t*>(data);
            out.push_back({std::wstring(name, nameLen), std::wstring(text)});
        }
    }
    RegCloseKey(key);
    return out;
}
}
