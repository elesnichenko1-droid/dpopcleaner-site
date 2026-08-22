#include "modules/Applications.h"
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <filesystem>
#include <algorithm>
#include <cwctype>
#include <map>
#include <set>
#include <vector>
#include <optional>
#include <iterator>

namespace fs = std::filesystem;

namespace {

std::wstring QueryString(HKEY key, const wchar_t* name) {
    DWORD type = 0;
    DWORD bytes = 0;
    if (RegQueryValueExW(key, name, nullptr, &type, nullptr, &bytes) != ERROR_SUCCESS ||
        (type != REG_SZ && type != REG_EXPAND_SZ) || bytes < sizeof(wchar_t)) return {};
    std::wstring value(bytes / sizeof(wchar_t), L'\0');
    if (RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<BYTE*>(value.data()), &bytes) != ERROR_SUCCESS) return {};
    while (!value.empty() && value.back() == L'\0') value.pop_back();
    if (type == REG_EXPAND_SZ && !value.empty()) {
        DWORD needed = ExpandEnvironmentStringsW(value.c_str(), nullptr, 0);
        if (needed > 1) {
            std::wstring expanded(needed, L'\0');
            ExpandEnvironmentStringsW(value.c_str(), expanded.data(), needed);
            while (!expanded.empty() && expanded.back() == L'\0') expanded.pop_back();
            value = std::move(expanded);
        }
    }
    return value;
}

DWORD QueryDword(HKEY key, const wchar_t* name, DWORD fallback = 0) {
    DWORD type = 0, value = fallback, bytes = sizeof(value);
    if (RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<BYTE*>(&value), &bytes) != ERROR_SUCCESS || type != REG_DWORD)
        return fallback;
    return value;
}

void EnumerateRoot(HKEY root, REGSAM view, std::vector<dpop::apps::InstalledApp>& out) {
    constexpr wchar_t kBase[] = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
    HKEY base{};
    if (RegOpenKeyExW(root, kBase, 0, KEY_READ | view, &base) != ERROR_SUCCESS) return;

    DWORD index = 0;
    wchar_t name[512]{};
    for (;;) {
        DWORD nameLen = static_cast<DWORD>(std::size(name));
        const LONG rc = RegEnumKeyExW(base, index++, name, &nameLen, nullptr, nullptr, nullptr, nullptr);
        if (rc == ERROR_NO_MORE_ITEMS) break;
        if (rc != ERROR_SUCCESS) continue;
        HKEY appKey{};
        if (RegOpenKeyExW(base, name, 0, KEY_READ, &appKey) != ERROR_SUCCESS) continue;

        dpop::apps::InstalledApp app;
        app.displayName = QueryString(appKey, L"DisplayName");
        if (app.displayName.empty() || QueryDword(appKey, L"SystemComponent") == 1) {
            RegCloseKey(appKey);
            continue;
        }
        const auto releaseType = QueryString(appKey, L"ReleaseType");
        if (releaseType == L"Hotfix" || releaseType == L"Security Update" || releaseType == L"Update Rollup") {
            RegCloseKey(appKey);
            continue;
        }

        app.displayVersion = QueryString(appKey, L"DisplayVersion");
        app.publisher = QueryString(appKey, L"Publisher");
        app.installLocation = QueryString(appKey, L"InstallLocation");
        app.uninstallString = QueryString(appKey, L"UninstallString");
        app.quietUninstallString = QueryString(appKey, L"QuietUninstallString");
        app.displayIcon = QueryString(appKey, L"DisplayIcon");
        app.registryRoot = root;
        app.registryView = view;
        app.registryPath = std::wstring(kBase) + L"\\" + std::wstring(name, nameLen);
        app.windowsInstaller = QueryDword(appKey, L"WindowsInstaller") == 1;
        RegCloseKey(appKey);

        if (app.uninstallString.empty() && app.quietUninstallString.empty()) continue;
        out.push_back(std::move(app));
    }
    RegCloseKey(base);
}

std::wstring Lower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t c){ return static_cast<wchar_t>(std::towlower(c)); });
    return value;
}

std::wstring NormalizeName(const std::wstring& value) {
    std::wstring out;
    out.reserve(value.size());
    for (wchar_t c : Lower(value)) {
        if (std::iswalnum(c)) out.push_back(c);
    }
    const std::wstring suffixes[] = {L"x64", L"x86", L"64bit", L"32bit", L"beta", L"application", L"app"};
    for (const auto& s : suffixes) {
        if (out.size() > s.size() && out.ends_with(s)) out.resize(out.size() - s.size());
    }
    return out;
}

bool LooksLikeSameProduct(const std::wstring& candidate, const std::wstring& product) {
    const auto a = NormalizeName(candidate);
    const auto b = NormalizeName(product);
    if (a.empty() || b.empty()) return false;
    if (a == b) return true;
    if (b.size() >= 5 && a.size() >= b.size() && a.rfind(b, 0) == 0 && (a.size() - b.size()) <= 10) return true;
    return false;
}

std::optional<fs::path> KnownFolder(REFKNOWNFOLDERID id) {
    PWSTR raw = nullptr;
    if (SHGetKnownFolderPath(id, KF_FLAG_DEFAULT, nullptr, &raw) != S_OK || !raw) return std::nullopt;
    fs::path p(raw);
    CoTaskMemFree(raw);
    return p;
}

void AddCandidate(std::vector<dpop::apps::LeftoverItem>& out, std::set<std::wstring>& seen,
                  const fs::path& path, const std::wstring& reason, bool high) {
    std::error_code ec;
    if (path.empty() || !fs::exists(path, ec)) return;
    auto key = Lower(path.lexically_normal().wstring());
    if (!seen.insert(key).second) return;
    out.push_back({path, reason, high});
}

void ScanNamedChildren(std::vector<dpop::apps::LeftoverItem>& out, std::set<std::wstring>& seen,
                       const fs::path& root, const std::wstring& product, const std::wstring& reason) {
    std::error_code ec;
    if (root.empty() || !fs::exists(root, ec) || !fs::is_directory(root, ec)) return;
    for (fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        const auto filename = it->path().filename().wstring();
        if (LooksLikeSameProduct(filename, product)) AddCandidate(out, seen, it->path(), reason, true);
    }
}

std::wstring Trim(std::wstring s) {
    const auto first = s.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) return {};
    const auto last = s.find_last_not_of(L" \t\r\n");
    return s.substr(first, last - first + 1);
}

bool SplitCommand(const std::wstring& command, std::wstring& exe, std::wstring& params) {
    const auto cmd = Trim(command);
    if (cmd.empty()) return false;

    // Prefer an explicitly quoted executable. For broken vendor registry entries where
    // a path with spaces is not quoted, fall back to the first .exe boundary.
    if (cmd.front() == L'\"') {
        const auto end = cmd.find(L'\"', 1);
        if (end == std::wstring::npos) return false;
        exe = cmd.substr(1, end - 1);
        params = Trim(cmd.substr(end + 1));
        return true;
    }
    const auto exeEnd = Lower(cmd).find(L".exe");
    if (exeEnd != std::wstring::npos) {
        exe = Trim(cmd.substr(0, exeEnd + 4));
        params = Trim(cmd.substr(exeEnd + 4));
        return true;
    }

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(cmd.c_str(), &argc);
    if (!argv || argc < 1) { if (argv) LocalFree(argv); return false; }
    exe = argv[0];
    const auto firstSpace = cmd.find(L' ');
    params = firstSpace == std::wstring::npos ? L"" : Trim(cmd.substr(firstSpace + 1));
    LocalFree(argv);
    return true;
}

void NormalizeMsiUninstall(std::wstring& exe, std::wstring& params) {
    const auto filename = Lower(fs::path(exe).filename().wstring());
    if (filename != L"msiexec.exe" && filename != L"msiexec") return;
    for (std::size_t i = 0; i + 1 < params.size(); ++i) {
        if (params[i] == L'/' && (params[i + 1] == L'I' || params[i + 1] == L'i')) {
            params[i + 1] = L'X';
            break;
        }
    }
}

}

namespace dpop::apps {

std::vector<InstalledApp> EnumerateInstalledApps() {
    std::vector<InstalledApp> all;
    EnumerateRoot(HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY, all);
    EnumerateRoot(HKEY_LOCAL_MACHINE, KEY_WOW64_32KEY, all);
    EnumerateRoot(HKEY_CURRENT_USER, KEY_WOW64_64KEY, all);
    EnumerateRoot(HKEY_CURRENT_USER, KEY_WOW64_32KEY, all);

    std::map<std::wstring, InstalledApp> unique;
    for (auto& app : all) {
        const auto key = Lower(app.displayName + L"|" + app.displayVersion + L"|" + app.publisher);
        auto it = unique.find(key);
        if (it == unique.end() || (it->second.installLocation.empty() && !app.installLocation.empty()))
            unique[key] = std::move(app);
    }
    std::vector<InstalledApp> result;
    result.reserve(unique.size());
    for (auto& [_, app] : unique) result.push_back(std::move(app));
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b){ return Lower(a.displayName) < Lower(b.displayName); });
    return result;
}

bool RunUninstaller(const InstalledApp& app, DWORD& exitCode, std::wstring& error) {
    std::wstring command = app.uninstallString.empty() ? app.quietUninstallString : app.uninstallString;
    std::wstring exe, params;
    if (!SplitCommand(command, exe, params)) { error = L"Некорректная команда удаления."; return false; }
    NormalizeMsiUninstall(exe, params);

    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = L"runas";
    sei.lpFile = exe.c_str();
    sei.lpParameters = params.empty() ? nullptr : params.c_str();
    sei.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&sei)) {
        const DWORD code = GetLastError();
        error = code == ERROR_CANCELLED ? L"Удаление отменено пользователем." : L"Не удалось запустить штатный деинсталлятор. Код: " + std::to_wstring(code);
        return false;
    }
    if (sei.hProcess) {
        WaitForSingleObject(sei.hProcess, INFINITE);
        if (!GetExitCodeProcess(sei.hProcess, &exitCode)) exitCode = 0;
        CloseHandle(sei.hProcess);
    } else exitCode = 0;

    if (exitCode == ERROR_INSTALL_USEREXIT) {
        error = L"Удаление отменено пользователем.";
        return false;
    }

    // Some uninstallers start a helper process and terminate early. Give Windows a short
    // window to remove the uninstall registration before the leftover scan begins.
    for (int attempt = 0; attempt < 10; ++attempt) {
        HKEY key{};
        const LONG rc = RegOpenKeyExW(app.registryRoot, app.registryPath.c_str(), 0, KEY_READ | app.registryView, &key);
        if (rc != ERROR_SUCCESS) break;
        RegCloseKey(key);
        Sleep(500);
    }
    return true;
}

std::vector<LeftoverItem> FindLeftovers(const InstalledApp& app) {
    std::vector<LeftoverItem> out;
    std::set<std::wstring> seen;

    if (!app.installLocation.empty()) {
        fs::path install = Trim(app.installLocation);
        AddCandidate(out, seen, install, L"Папка установки, оставшаяся после штатного удаления", true);
    }

    const auto pf = KnownFolder(FOLDERID_ProgramFiles);
    const auto pfx86 = KnownFolder(FOLDERID_ProgramFilesX86);
    const auto programData = KnownFolder(FOLDERID_ProgramData);
    const auto local = KnownFolder(FOLDERID_LocalAppData);
    const auto roaming = KnownFolder(FOLDERID_RoamingAppData);
    const auto startUser = KnownFolder(FOLDERID_Programs);
    const auto startCommon = KnownFolder(FOLDERID_CommonPrograms);

    if (pf) ScanNamedChildren(out, seen, *pf, app.displayName, L"Папка программы в Program Files");
    if (pfx86) ScanNamedChildren(out, seen, *pfx86, app.displayName, L"Папка программы в Program Files (x86)");
    if (programData) ScanNamedChildren(out, seen, *programData, app.displayName, L"Данные программы в ProgramData");
    if (local) ScanNamedChildren(out, seen, *local, app.displayName, L"Локальные данные пользователя");
    if (roaming) ScanNamedChildren(out, seen, *roaming, app.displayName, L"Перемещаемые данные пользователя");

    auto scanShortcuts = [&](const std::optional<fs::path>& root){
        if (!root) return;
        std::error_code ec;
        for (fs::recursive_directory_iterator it(*root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
            if (ec) { ec.clear(); continue; }
            if (it.depth() > 2) { it.disable_recursion_pending(); continue; }
            if (!it->is_regular_file(ec) || Lower(it->path().extension().wstring()) != L".lnk") continue;
            if (LooksLikeSameProduct(it->path().stem().wstring(), app.displayName))
                AddCandidate(out, seen, it->path(), L"Оставшийся ярлык меню Пуск", true);
        }
    };
    scanShortcuts(startUser);
    scanShortcuts(startCommon);

    return out;
}

bool MoveLeftoversToRecycleBin(const std::vector<LeftoverItem>& items, std::size_t& removed, std::wstring& error) {
    removed = 0;
    for (const auto& item : items) {
        std::wstring from = item.path.wstring();
        from.push_back(L'\0');
        from.push_back(L'\0');
        SHFILEOPSTRUCTW op{};
        op.wFunc = FO_DELETE;
        op.pFrom = from.c_str();
        op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
        const int rc = SHFileOperationW(&op);
        if (rc == 0 && !op.fAnyOperationsAborted) ++removed;
        else if (error.empty()) error = L"Не удалось переместить один или несколько хвостов в Корзину. Код: " + std::to_wstring(rc);
    }
    return removed == items.size();
}

}
