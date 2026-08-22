#include "modules/StartupManager.h"
#include <windows.h>
#include <shlobj.h>
#include <algorithm>
#include <iterator>
#include <cwctype>

namespace fs = std::filesystem;

namespace {
std::wstring Expand(const std::wstring& text) {
    DWORD needed = ExpandEnvironmentStringsW(text.c_str(), nullptr, 0);
    if (needed <= 1) return text;
    std::wstring out(needed, L'\0');
    ExpandEnvironmentStringsW(text.c_str(), out.data(), needed);
    while (!out.empty() && out.back() == L'\0') out.pop_back();
    return out;
}

void EnumerateRun(HKEY root, REGSAM view, const wchar_t* source, std::vector<dpop::startup::Entry>& out) {
    constexpr wchar_t kPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    HKEY key{};
    if (RegOpenKeyExW(root, kPath, 0, KEY_READ | view, &key) != ERROR_SUCCESS) return;
    for (DWORD index = 0;; ++index) {
        wchar_t name[512]{};
        BYTE data[8192]{};
        DWORD nameLen = static_cast<DWORD>(std::size(name));
        DWORD dataLen = sizeof(data), type = 0;
        const LONG rc = RegEnumValueW(key, index, name, &nameLen, nullptr, &type, data, &dataLen);
        if (rc == ERROR_NO_MORE_ITEMS) break;
        if (rc != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) continue;
        std::wstring command(reinterpret_cast<const wchar_t*>(data));
        if (type == REG_EXPAND_SZ) command = Expand(command);
        out.push_back({std::wstring(name, nameLen), command, source, {}});
    }
    RegCloseKey(key);
}

void EnumerateStartupFolder(REFKNOWNFOLDERID id, const wchar_t* source, std::vector<dpop::startup::Entry>& out) {
    PWSTR raw = nullptr;
    if (SHGetKnownFolderPath(id, KF_FLAG_DEFAULT, nullptr, &raw) != S_OK || !raw) return;
    fs::path root(raw);
    CoTaskMemFree(raw);
    std::error_code ec;
    if (!fs::exists(root, ec)) return;
    for (fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (!it->is_regular_file(ec)) continue;
        out.push_back({it->path().stem().wstring(), it->path().wstring(), source, it->path()});
    }
}

std::wstring Lower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c){ return static_cast<wchar_t>(std::towlower(c)); });
    return s;
}
}

namespace dpop::startup {
std::vector<Entry> EnumerateAll() {
    std::vector<Entry> out;
    EnumerateRun(HKEY_CURRENT_USER, KEY_WOW64_64KEY, L"HKCU Run", out);
    EnumerateRun(HKEY_CURRENT_USER, KEY_WOW64_32KEY, L"HKCU Run (32-bit)", out);
    EnumerateRun(HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY, L"HKLM Run", out);
    EnumerateRun(HKEY_LOCAL_MACHINE, KEY_WOW64_32KEY, L"HKLM Run (32-bit)", out);
    EnumerateStartupFolder(FOLDERID_Startup, L"Автозагрузка пользователя", out);
    EnumerateStartupFolder(FOLDERID_CommonStartup, L"Общая автозагрузка", out);

    std::sort(out.begin(), out.end(), [](const Entry& a, const Entry& b){ return Lower(a.name) < Lower(b.name); });
    return out;
}

std::vector<Entry> EnumerateCurrentUserRun() {
    std::vector<Entry> out;
    EnumerateRun(HKEY_CURRENT_USER, KEY_WOW64_64KEY, L"HKCU Run", out);
    return out;
}
}
