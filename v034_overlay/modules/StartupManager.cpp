#include "modules/StartupManager.h"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <cwctype>
#include <set>
#include <system_error>

namespace fs = std::filesystem;

namespace {
constexpr wchar_t kRunPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kDisabledPath[] = L"Software\\DPopCleaner\\DisabledStartup";

std::wstring Lower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(std::towlower(c));
    });
    return s;
}

std::wstring Expand(const std::wstring& text) {
    const DWORD needed = ExpandEnvironmentStringsW(text.c_str(), nullptr, 0);
    if (needed <= 1) return text;
    std::wstring out(needed, L'\0');
    ExpandEnvironmentStringsW(text.c_str(), out.data(), needed);
    while (!out.empty() && out.back() == L'\0') out.pop_back();
    return out;
}

fs::path ExtractExecutable(std::wstring command) {
    command = Expand(command);
    while (!command.empty() && iswspace(command.front())) command.erase(command.begin());
    if (command.empty()) return {};
    std::wstring token;
    if (command.front() == L'"') {
        const auto end = command.find(L'"', 1);
        token = command.substr(1, end == std::wstring::npos ? std::wstring::npos : end - 1);
    } else {
        const auto end = command.find_first_of(L" \t");
        token = command.substr(0, end);
    }
    if (token.empty()) return {};
    return fs::path(token);
}

bool StartsWithPath(const fs::path& path, const wchar_t* root) {
    const auto p = Lower(path.wstring());
    auto r = Lower(std::wstring(root));
    std::replace(r.begin(), r.end(), L'/', L'\\');
    return p.rfind(r, 0) == 0;
}

void Classify(dpop::startup::Entry& entry) {
    const std::wstring name = Lower(entry.name);
    const std::wstring command = Lower(entry.command);
    const std::wstring exe = Lower(entry.executable.wstring());
    const bool windowsPath = exe.find(L"\\windows\\") != std::wstring::npos || StartsWithPath(entry.executable, L"C:\\Windows");
    const bool driverish = name.find(L"driver") != std::wstring::npos ||
        name.find(L"realtek") != std::wstring::npos ||
        name.find(L"securityhealth") != std::wstring::npos ||
        command.find(L"driverstore") != std::wstring::npos ||
        command.find(L"system32") != std::wstring::npos;
    const bool microsoftish = name.find(L"securityhealth") != std::wstring::npos ||
        name.find(L"windows") != std::wstring::npos ||
        exe.find(L"\\microsoft\\") != std::wstring::npos;

    if (!entry.enabled) {
        entry.category = L"Отключено";
        entry.recommendation = L"Можно восстановить";
        entry.protectedEntry = false;
        return;
    }
    if (windowsPath || microsoftish) {
        entry.category = L"Системный";
        entry.recommendation = L"Лучше не отключать";
        entry.protectedEntry = true;
        return;
    }
    if (driverish || entry.source.rfind(L"HKLM", 0) == 0) {
        entry.category = L"Драйвер / vendor";
        entry.recommendation = L"Проверьте перед отключением";
        entry.protectedEntry = true;
        return;
    }
    entry.category = L"Пользовательский";
    entry.recommendation = L"Можно отключить, если не нужен при входе";
    entry.protectedEntry = false;
}

std::wstring BackupValueName(bool view32, std::wstring_view name) {
    return std::wstring(view32 ? L"32|" : L"64|") + std::wstring(name);
}

void EnumerateRun(HKEY root, REGSAM view, const wchar_t* source, bool manageable, std::vector<dpop::startup::Entry>& out) {
    HKEY key{};
    if (RegOpenKeyExW(root, kRunPath, 0, KEY_READ | view, &key) != ERROR_SUCCESS) return;
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
        dpop::startup::Entry e{};
        e.name.assign(name, nameLen);
        e.command = command;
        e.source = source;
        e.executable = ExtractExecutable(command);
        e.manageable = manageable;
        e.enabled = true;
        e.from32BitView = (view & KEY_WOW64_32KEY) != 0;
        Classify(e);
        out.push_back(std::move(e));
    }
    RegCloseKey(key);
}

void EnumerateDisabled(std::vector<dpop::startup::Entry>& out) {
    HKEY key{};
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kDisabledPath, 0, KEY_READ, &key) != ERROR_SUCCESS) return;
    for (DWORD index = 0;; ++index) {
        wchar_t rawName[768]{};
        BYTE data[8192]{};
        DWORD nameLen = static_cast<DWORD>(std::size(rawName));
        DWORD dataLen = sizeof(data), type = 0;
        const LONG rc = RegEnumValueW(key, index, rawName, &nameLen, nullptr, &type, data, &dataLen);
        if (rc == ERROR_NO_MORE_ITEMS) break;
        if (rc != ERROR_SUCCESS || type != REG_SZ) continue;
        std::wstring stored(rawName, nameLen);
        const bool view32 = stored.rfind(L"32|", 0) == 0;
        const bool view64 = stored.rfind(L"64|", 0) == 0;
        if (!view32 && !view64) continue;
        dpop::startup::Entry e{};
        e.name = stored.substr(3);
        e.command = reinterpret_cast<const wchar_t*>(data);
        e.source = view32 ? L"DPopCleaner: отключено (32-bit)" : L"DPopCleaner: отключено";
        e.executable = ExtractExecutable(e.command);
        e.manageable = true;
        e.enabled = false;
        e.from32BitView = view32;
        Classify(e);
        out.push_back(std::move(e));
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
        dpop::startup::Entry e{};
        e.name = it->path().stem().wstring();
        e.command = it->path().wstring();
        e.source = source;
        e.location = it->path();
        e.executable = it->path();
        e.manageable = false;
        e.enabled = true;
        Classify(e);
        out.push_back(std::move(e));
    }
}

bool ReadRunValue(REGSAM view, const std::wstring& name, std::wstring& command, std::wstring& error) {
    HKEY key{};
    const LONG open = RegOpenKeyExW(HKEY_CURRENT_USER, kRunPath, 0, KEY_QUERY_VALUE | KEY_SET_VALUE | view, &key);
    if (open != ERROR_SUCCESS) { error = L"Не удалось открыть HKCU Run. Код: " + std::to_wstring(open); return false; }
    DWORD type = 0, bytes = 0;
    LONG rc = RegQueryValueExW(key, name.c_str(), nullptr, &type, nullptr, &bytes);
    if (rc != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) || bytes < sizeof(wchar_t)) {
        RegCloseKey(key); error = L"Запись автозагрузки уже изменилась или исчезла."; return false;
    }
    std::vector<BYTE> data(bytes + sizeof(wchar_t));
    rc = RegQueryValueExW(key, name.c_str(), nullptr, &type, data.data(), &bytes);
    RegCloseKey(key);
    if (rc != ERROR_SUCCESS) { error = L"Не удалось прочитать запись автозагрузки."; return false; }
    command = reinterpret_cast<const wchar_t*>(data.data());
    return true;
}
}

namespace dpop::startup {
std::vector<Entry> EnumerateAll() {
    std::vector<Entry> raw;
    EnumerateRun(HKEY_CURRENT_USER, KEY_WOW64_64KEY, L"HKCU Run", true, raw);
    EnumerateRun(HKEY_CURRENT_USER, KEY_WOW64_32KEY, L"HKCU Run (32-bit)", true, raw);
    EnumerateRun(HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY, L"HKLM Run", false, raw);
    EnumerateRun(HKEY_LOCAL_MACHINE, KEY_WOW64_32KEY, L"HKLM Run (32-bit)", false, raw);
    EnumerateStartupFolder(FOLDERID_Startup, L"Автозагрузка пользователя", raw);
    EnumerateStartupFolder(FOLDERID_CommonStartup, L"Общая автозагрузка", raw);
    EnumerateDisabled(raw);

    std::vector<Entry> out;
    std::set<std::wstring> seen;
    for (auto& e : raw) {
        const std::wstring key = Lower(e.name) + L"|" + Lower(e.command) + (e.enabled ? L"|1" : L"|0");
        if (!seen.insert(key).second) continue;
        out.push_back(std::move(e));
    }
    std::stable_sort(out.begin(), out.end(), [](const Entry& a, const Entry& b) {
        if (a.protectedEntry != b.protectedEntry) return !a.protectedEntry;
        return Lower(a.name) < Lower(b.name);
    });
    return out;
}

std::vector<Entry> EnumerateCurrentUserRun() {
    std::vector<Entry> out;
    EnumerateRun(HKEY_CURRENT_USER, KEY_WOW64_64KEY, L"HKCU Run", true, out);
    return out;
}

bool SetEnabled(const Entry& entry, bool enabled, std::wstring& error) {
    if (!entry.manageable) {
        error = entry.protectedEntry
            ? L"Эта системная/vendor-запись защищена DPopCleaner. Используйте штатное управление Windows после проверки."
            : L"Прямое изменение этой записи пока отключено из соображений безопасности.";
        return false;
    }
    if (entry.enabled == enabled) return true;

    const REGSAM view = entry.from32BitView ? KEY_WOW64_32KEY : KEY_WOW64_64KEY;
    const std::wstring backupName = BackupValueName(entry.from32BitView, entry.name);
    HKEY backup{};
    LONG rc = RegCreateKeyExW(HKEY_CURRENT_USER, kDisabledPath, 0, nullptr, 0, KEY_QUERY_VALUE | KEY_SET_VALUE, nullptr, &backup, nullptr);
    if (rc != ERROR_SUCCESS) { error = L"Не удалось открыть резерв DPopCleaner для автозагрузки."; return false; }

    if (!enabled) {
        std::wstring command;
        if (!ReadRunValue(view, entry.name, command, error)) { RegCloseKey(backup); return false; }
        rc = RegSetValueExW(backup, backupName.c_str(), 0, REG_SZ,
            reinterpret_cast<const BYTE*>(command.c_str()), static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
        if (rc == ERROR_SUCCESS) {
            HKEY run{};
            rc = RegOpenKeyExW(HKEY_CURRENT_USER, kRunPath, 0, KEY_SET_VALUE | view, &run);
            if (rc == ERROR_SUCCESS) { rc = RegDeleteValueW(run, entry.name.c_str()); RegCloseKey(run); }
        }
    } else {
        DWORD type = 0, bytes = 0;
        rc = RegQueryValueExW(backup, backupName.c_str(), nullptr, &type, nullptr, &bytes);
        if (rc == ERROR_SUCCESS && type == REG_SZ) {
            std::vector<BYTE> data(bytes + sizeof(wchar_t));
            rc = RegQueryValueExW(backup, backupName.c_str(), nullptr, &type, data.data(), &bytes);
            if (rc == ERROR_SUCCESS) {
                HKEY run{};
                rc = RegCreateKeyExW(HKEY_CURRENT_USER, kRunPath, 0, nullptr, 0, KEY_SET_VALUE | view, nullptr, &run, nullptr);
                if (rc == ERROR_SUCCESS) {
                    rc = RegSetValueExW(run, entry.name.c_str(), 0, REG_SZ, data.data(), bytes);
                    RegCloseKey(run);
                }
            }
        }
        if (rc == ERROR_SUCCESS) RegDeleteValueW(backup, backupName.c_str());
    }
    RegCloseKey(backup);
    if (rc == ERROR_SUCCESS) return true;
    error = L"Не удалось изменить автозагрузку. Код Windows: " + std::to_wstring(rc);
    return false;
}
}
