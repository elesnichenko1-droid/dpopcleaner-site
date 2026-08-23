#include "modules/FullCore.h"

#include "modules/DPopGuard.h"
#include "update/Hash.h"

#include <windows.h>
#include <psapi.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cwctype>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <system_error>
#include <unordered_map>

namespace fs = std::filesystem;

namespace {

fs::path Env(const wchar_t* name) {
    wchar_t* value = nullptr;
    size_t len = 0;
    if (_wdupenv_s(&value, &len, name) == 0 && value) {
        fs::path out(value);
        free(value);
        return out;
    }
    if (value) free(value);
    return {};
}

fs::path UserTemp() {
    std::wstring buffer(32768, L'\0');
    const DWORD n = GetTempPathW(static_cast<DWORD>(buffer.size()), buffer.data());
    if (!n || n >= buffer.size()) return {};
    buffer.resize(n);
    return fs::path(buffer);
}

fs::path Join(const fs::path& base, const fs::path& child) {
    return base.empty() ? fs::path{} : base / child;
}

std::uint64_t EstimateTree(const fs::path& root, std::stop_token stop) {
    std::uint64_t total = 0;
    std::error_code ec;
    if (root.empty() || !fs::exists(root, ec)) return 0;
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
         it != end && !stop.stop_requested(); it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (it->is_regular_file(ec)) {
            const auto size = it->file_size(ec);
            if (!ec) total += size;
        }
        ec.clear();
    }
    return total;
}

void CleanTree(const fs::path& root, dpop::full::CleanSummary& out, std::stop_token stop) {
    std::error_code ec;
    if (root.empty() || !fs::exists(root, ec)) return;
    std::vector<fs::path> dirs;
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
         it != end && !stop.stop_requested(); it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (it->is_directory(ec)) { dirs.push_back(it->path()); ec.clear(); continue; }
        if (!it->is_regular_file(ec)) { ec.clear(); continue; }
        const auto size = it->file_size(ec);
        ec.clear();
        if (fs::remove(it->path(), ec)) {
            out.removedBytes += size;
            ++out.removedFiles;
        } else {
            ++out.failedFiles;
            ec.clear();
        }
    }
    for (auto it = dirs.rbegin(); it != dirs.rend() && !stop.stop_requested(); ++it) {
        fs::remove(*it, ec);
        ec.clear();
    }
}

bool StartsWithInsensitive(std::wstring value, std::wstring prefix) {
    auto lower = [](std::wstring& text) {
        std::transform(text.begin(), text.end(), text.begin(), [](wchar_t c) {
            return static_cast<wchar_t>(towlower(c));
        });
    };
    lower(value);
    lower(prefix);
    return value.rfind(prefix, 0) == 0;
}

std::uint64_t EstimateDirectFiles(const fs::path& root, const std::vector<std::wstring>& prefixes) {
    std::uint64_t total = 0;
    std::error_code ec;
    if (root.empty() || !fs::exists(root, ec)) return 0;
    for (fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (!it->is_regular_file(ec)) { ec.clear(); continue; }
        const auto name = it->path().filename().wstring();
        bool match = false;
        for (const auto& p : prefixes) if (StartsWithInsensitive(name, p)) { match = true; break; }
        if (!match) continue;
        const auto size = it->file_size(ec);
        if (!ec) total += size;
        ec.clear();
    }
    return total;
}

void CleanDirectFiles(const fs::path& root, const std::vector<std::wstring>& prefixes, dpop::full::CleanSummary& out) {
    std::error_code ec;
    if (root.empty() || !fs::exists(root, ec)) return;
    for (fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (!it->is_regular_file(ec)) { ec.clear(); continue; }
        const auto name = it->path().filename().wstring();
        bool match = false;
        for (const auto& p : prefixes) if (StartsWithInsensitive(name, p)) { match = true; break; }
        if (!match) continue;
        const auto size = it->file_size(ec);
        ec.clear();
        if (fs::remove(it->path(), ec)) { out.removedBytes += size; ++out.removedFiles; }
        else { ++out.failedFiles; ec.clear(); }
    }
}

std::vector<fs::path> BrowserRoots() {
    const auto local = Env(L"LOCALAPPDATA");
    const auto roaming = Env(L"APPDATA");
    std::vector<fs::path> out;
    if (!local.empty()) {
        out.push_back(local / L"Microsoft/Edge/User Data/Default/Cache");
        out.push_back(local / L"Microsoft/Edge/User Data/Default/Code Cache");
        out.push_back(local / L"Google/Chrome/User Data/Default/Cache");
        out.push_back(local / L"Google/Chrome/User Data/Default/Code Cache");
        out.push_back(local / L"Yandex/YandexBrowser/User Data/Default/Cache");
    }
    if (!roaming.empty()) {
        const auto profiles = roaming / L"Mozilla/Firefox/Profiles";
        std::error_code ec;
        if (fs::exists(profiles, ec)) {
            for (fs::directory_iterator it(profiles, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
                if (ec) { ec.clear(); continue; }
                if (!it->is_directory(ec)) continue;
                out.push_back(it->path() / L"cache2");
                out.push_back(it->path() / L"startupCache");
            }
        }
    }
    return out;
}

std::vector<fs::path> EpicRoots() {
    std::vector<fs::path> out;
    const auto local = Env(L"LOCALAPPDATA");
    const auto saved = local / L"EpicGamesLauncher/Saved";
    std::error_code ec;
    if (local.empty() || !fs::exists(saved, ec)) return out;
    for (fs::directory_iterator it(saved, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (it->is_directory(ec) && StartsWithInsensitive(it->path().filename().wstring(), L"webcache")) out.push_back(it->path());
    }
    return out;
}

std::vector<fs::path> RootsFor(dpop::full::CleanKind kind) {
    const auto local = Env(L"LOCALAPPDATA");
    const auto roaming = Env(L"APPDATA");
    const auto programData = Env(L"PROGRAMDATA");
    const auto windows = Env(L"WINDIR");
    const auto pf86 = Env(L"PROGRAMFILES(X86)");
    switch (kind) {
    case dpop::full::CleanKind::UserTemp: return {UserTemp()};
    case dpop::full::CleanKind::WindowsTemp: return {Join(windows, L"Temp")};
    case dpop::full::CleanKind::CrashDumps: return {Join(local, L"CrashDumps")};
    case dpop::full::CleanKind::Wer: return {
        Join(local, L"Microsoft/Windows/WER"),
        Join(programData, L"Microsoft/Windows/WER/ReportArchive"),
        Join(programData, L"Microsoft/Windows/WER/ReportQueue")
    };
    case dpop::full::CleanKind::DirectX: return {Join(local, L"D3DSCache")};
    case dpop::full::CleanKind::Browsers: return BrowserRoots();
    case dpop::full::CleanKind::Discord: return {
        Join(roaming, L"discord/Cache"), Join(roaming, L"discord/Code Cache"), Join(roaming, L"discord/GPUCache")
    };
    case dpop::full::CleanKind::Steam: return {
        Join(local, L"Steam/htmlcache"), Join(pf86, L"Steam/appcache/httpcache")
    };
    case dpop::full::CleanKind::Epic: return EpicRoots();
    case dpop::full::CleanKind::Nvidia: return {
        Join(local, L"NVIDIA/DXCache"), Join(local, L"NVIDIA/GLCache"), Join(programData, L"NVIDIA Corporation/NV_Cache")
    };
    case dpop::full::CleanKind::Amd: return {Join(local, L"AMD/DxCache"), Join(local, L"AMD/GLCache")};
    default: return {};
    }
}

std::wstring Trim(std::wstring value) {
    const auto first = value.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) return {};
    const auto last = value.find_last_not_of(L" \t\r\n");
    return value.substr(first, last - first + 1);
}

bool SplitExecutableCommand(const std::wstring& command, std::wstring& exe, std::wstring& params) {
    const auto cmd = Trim(command);
    if (cmd.empty()) return false;
    if (cmd.front() == L'"') {
        const auto end = cmd.find(L'"', 1);
        if (end == std::wstring::npos) return false;
        exe = cmd.substr(1, end - 1);
        params = Trim(cmd.substr(end + 1));
        return true;
    }
    std::wstring lower = cmd;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t c){ return static_cast<wchar_t>(towlower(c)); });
    const auto end = lower.find(L".exe");
    if (end == std::wstring::npos) return false;
    exe = Trim(cmd.substr(0, end + 4));
    params = Trim(cmd.substr(end + 4));
    return !exe.empty();
}

bool LaunchProcess(const wchar_t* file, const std::wstring& args, bool elevated, unsigned long& exitCode, std::wstring& error, std::stop_token stop) {
    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = elevated ? L"runas" : L"open";
    sei.lpFile = file;
    sei.lpParameters = args.empty() ? nullptr : args.c_str();
    sei.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&sei)) {
        const DWORD code = GetLastError();
        error = code == ERROR_CANCELLED ? L"Операция отменена пользователем." : L"Не удалось запустить системную команду. Код: " + std::to_wstring(code);
        return false;
    }
    if (!sei.hProcess) { exitCode = 0; return true; }
    for (;;) {
        const DWORD wait = WaitForSingleObject(sei.hProcess, 250);
        if (wait == WAIT_OBJECT_0) break;
        if (wait == WAIT_FAILED) { CloseHandle(sei.hProcess); error = L"Не удалось дождаться системной команды."; return false; }
        if (stop.stop_requested()) {
            CloseHandle(sei.hProcess);
            exitCode = STILL_ACTIVE;
            error = L"Ожидание отменено; запущенная системная команда может продолжать работу отдельно.";
            return false;
        }
    }
    DWORD code = 0;
    GetExitCodeProcess(sei.hProcess, &code);
    CloseHandle(sei.hProcess);
    exitCode = code;
    if (code != 0) {
        error = L"Системная команда завершилась с кодом " + std::to_wstring(code) + L".";
        return false;
    }
    return true;
}

std::wstring ReadText(const fs::path& path) {
    std::wifstream in(path);
    if (!in) return {};
    std::wstringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

bool ExtractBool(const std::wstring& text, std::wstring_view key, bool fallback) {
    const auto pos = text.find(std::wstring(L"\"") + std::wstring(key) + L"\"");
    if (pos == std::wstring::npos) return fallback;
    const auto colon = text.find(L':', pos);
    if (colon == std::wstring::npos) return fallback;
    const auto value = text.substr(colon + 1, 16);
    if (value.find(L"true") != std::wstring::npos) return true;
    if (value.find(L"false") != std::wstring::npos) return false;
    return fallback;
}

unsigned ExtractUInt(const std::wstring& text, std::wstring_view key, unsigned fallback) {
    const auto pos = text.find(std::wstring(L"\"") + std::wstring(key) + L"\"");
    if (pos == std::wstring::npos) return fallback;
    const auto colon = text.find(L':', pos);
    if (colon == std::wstring::npos) return fallback;
    const auto begin = text.find_first_of(L"0123456789", colon + 1);
    if (begin == std::wstring::npos) return fallback;
    const auto end = text.find_first_not_of(L"0123456789", begin);
    try { return static_cast<unsigned>(std::stoul(text.substr(begin, end - begin))); }
    catch (...) { return fallback; }
}

} // namespace

namespace dpop::full {

std::wstring_view CleanKindLabel(CleanKind kind) noexcept {
    switch (kind) {
    case CleanKind::UserTemp: return L"Временные файлы пользователя";
    case CleanKind::WindowsTemp: return L"Windows TEMP";
    case CleanKind::CrashDumps: return L"Crash dumps";
    case CleanKind::Wer: return L"Windows Error Reporting";
    case CleanKind::Thumbnails: return L"Кэш миниатюр и иконок";
    case CleanKind::DirectX: return L"DirectX Shader Cache";
    case CleanKind::Browsers: return L"Кэш браузеров";
    case CleanKind::Discord: return L"Кэш Discord";
    case CleanKind::Steam: return L"Кэш Steam";
    case CleanKind::Epic: return L"Кэш Epic Games Launcher";
    case CleanKind::Nvidia: return L"Кэш NVIDIA";
    case CleanKind::Amd: return L"Кэш AMD";
    case CleanKind::RecycleBin: return L"Корзина";
    }
    return L"Очистка";
}

std::vector<CleanItem> AnalyzeCleaning(std::stop_token stop) {
    const std::array<CleanKind, 13> kinds = {
        CleanKind::UserTemp, CleanKind::WindowsTemp, CleanKind::CrashDumps, CleanKind::Wer,
        CleanKind::Thumbnails, CleanKind::DirectX, CleanKind::Browsers, CleanKind::Discord,
        CleanKind::Steam, CleanKind::Epic, CleanKind::Nvidia, CleanKind::Amd, CleanKind::RecycleBin
    };
    std::vector<CleanItem> out;
    out.reserve(kinds.size());
    for (const auto kind : kinds) {
        if (stop.stop_requested()) break;
        std::uint64_t bytes = 0;
        if (kind == CleanKind::RecycleBin) {
            SHQUERYRBINFO info{}; info.cbSize = sizeof(info);
            if (SUCCEEDED(SHQueryRecycleBinW(nullptr, &info))) bytes = static_cast<std::uint64_t>(info.i64Size);
        } else if (kind == CleanKind::Thumbnails) {
            bytes = EstimateDirectFiles(Join(Env(L"LOCALAPPDATA"), L"Microsoft/Windows/Explorer"), {L"thumbcache_", L"iconcache_"});
        } else {
            for (const auto& root : RootsFor(kind)) bytes += EstimateTree(root, stop);
        }
        const bool admin = kind == CleanKind::WindowsTemp || kind == CleanKind::Wer || kind == CleanKind::Nvidia;
        const bool recommended = kind != CleanKind::WindowsTemp && kind != CleanKind::RecycleBin;
        out.push_back({kind, std::wstring(CleanKindLabel(kind)), bytes, recommended, admin});
    }
    return out;
}

CleanSummary CleanSelected(const std::vector<CleanKind>& kinds, std::stop_token stop) {
    CleanSummary out{};
    for (const auto kind : kinds) {
        if (stop.stop_requested()) break;
        if (kind == CleanKind::RecycleBin) {
            std::uint64_t bytes = 0;
            SHQUERYRBINFO info{};
            info.cbSize = sizeof(info);
            if (SUCCEEDED(SHQueryRecycleBinW(nullptr, &info))) bytes = static_cast<std::uint64_t>(info.i64Size);
            const HRESULT hr = SHEmptyRecycleBinW(nullptr, nullptr, SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
            if (SUCCEEDED(hr)) out.removedBytes += bytes;
            else { ++out.failedFiles; out.error = L"Не удалось очистить Корзину."; }
            continue;
        }
        if (kind == CleanKind::Thumbnails) {
            CleanDirectFiles(Join(Env(L"LOCALAPPDATA"), L"Microsoft/Windows/Explorer"), {L"thumbcache_", L"iconcache_"}, out);
            continue;
        }
        for (const auto& root : RootsFor(kind)) CleanTree(root, out, stop);
    }
    return out;
}

unsigned Percent(std::uint64_t used, std::uint64_t total) noexcept {
    if (!total) return 0;
    return static_cast<unsigned>((used * 100 + total / 2) / total);
}

MemoryStats QueryMemoryStats() {
    MEMORYSTATUSEX m{}; m.dwLength = sizeof(m);
    MemoryStats out{};
    if (GlobalMemoryStatusEx(&m)) {
        out.totalPhysical = m.ullTotalPhys;
        out.availablePhysical = m.ullAvailPhys;
        out.usedPhysical = out.totalPhysical - std::min(out.totalPhysical, out.availablePhysical);
        out.usedPercent = Percent(out.usedPhysical, out.totalPhysical);
    }
    DWORD processes[8192]{}; DWORD bytes = 0;
    if (EnumProcesses(processes, sizeof(processes), &bytes)) out.processCount = bytes / sizeof(DWORD);
    return out;
}

MemoryTrimResult TrimWorkingSets(bool aggressive, std::stop_token stop) {
    MemoryTrimResult out{};
    DWORD processes[8192]{}; DWORD bytes = 0;
    if (!EnumProcesses(processes, sizeof(processes), &bytes)) return out;
    const DWORD self = GetCurrentProcessId();
    const auto count = bytes / sizeof(DWORD);
    for (DWORD i = 0; i < count && !stop.stop_requested(); ++i) {
        const DWORD pid = processes[i];
        if (!pid || pid == self) continue;
        HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_SET_QUOTA, FALSE, pid);
        if (!process) continue;
        PROCESS_MEMORY_COUNTERS_EX pmc{};
        bool shouldTrim = aggressive;
        if (GetProcessMemoryInfo(process, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
            const SIZE_T threshold = aggressive ? 16ull * 1024ull * 1024ull : 128ull * 1024ull * 1024ull;
            shouldTrim = pmc.WorkingSetSize >= threshold;
        }
        if (shouldTrim) {
            ++out.attempted;
            if (EmptyWorkingSet(process)) ++out.trimmed;
            else ++out.failed;
        }
        CloseHandle(process);
    }
    return out;
}

std::vector<FileItem> ScanLargeFiles(const fs::path& root, std::uint64_t minBytes, std::stop_token stop, std::size_t maxResults) {
    std::vector<FileItem> out;
    std::error_code ec;
    if (root.empty() || !fs::exists(root, ec)) return out;
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
         it != end && !stop.stop_requested(); it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (!it->is_regular_file(ec)) { ec.clear(); continue; }
        const auto size = it->file_size(ec);
        if (!ec && size >= minBytes) out.push_back({it->path(), size});
        ec.clear();
    }
    std::sort(out.begin(), out.end(), [](const FileItem& a, const FileItem& b) { return a.size > b.size; });
    if (out.size() > maxResults) out.resize(maxResults);
    return out;
}

std::vector<DuplicateFile> FindDuplicates(const fs::path& root, std::uint64_t minBytes, std::stop_token stop) {
    std::map<std::uint64_t, std::vector<fs::path>> bySize;
    std::error_code ec;
    if (root.empty() || !fs::exists(root, ec)) return {};
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
         it != end && !stop.stop_requested(); it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (!it->is_regular_file(ec)) { ec.clear(); continue; }
        const auto size = it->file_size(ec);
        if (!ec && size >= minBytes) bySize[size].push_back(it->path());
        ec.clear();
    }
    std::vector<DuplicateFile> out;
    unsigned group = 0;
    for (auto& [size, files] : bySize) {
        if (stop.stop_requested()) break;
        if (files.size() < 2) continue;
        std::map<std::wstring, std::vector<fs::path>> byHash;
        for (const auto& file : files) {
            if (stop.stop_requested()) break;
            std::wstring hash, error;
            if (dpop::update::Sha256File(file, hash, error)) byHash[hash].push_back(file);
        }
        for (auto& [hash, same] : byHash) {
            if (same.size() < 2) continue;
            ++group;
            for (const auto& file : same) out.push_back({group, size, hash, file});
        }
    }
    return out;
}

RecycleResult MoveToRecycleBin(const std::vector<fs::path>& paths) {
    RecycleResult out{};
    for (const auto& path : paths) {
        std::wstring from = path.wstring();
        from.push_back(L'\0'); from.push_back(L'\0');
        SHFILEOPSTRUCTW op{};
        op.wFunc = FO_DELETE;
        op.pFrom = from.c_str();
        op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
        const int rc = SHFileOperationW(&op);
        if (rc == 0 && !op.fAnyOperationsAborted) ++out.moved;
        else { ++out.failed; if (out.error.empty()) out.error = L"Не удалось переместить один или несколько файлов в Корзину."; }
    }
    return out;
}

bool LaunchUninstaller(const dpop::apps::InstalledApp& app, std::wstring& error) {
    const std::wstring command = app.uninstallString.empty() ? app.quietUninstallString : app.uninstallString;
    std::wstring exe, params;
    if (!SplitExecutableCommand(command, exe, params)) {
        error = L"Некорректная команда штатного деинсталлятора.";
        return false;
    }
    std::wstring filename = fs::path(exe).filename().wstring();
    std::transform(filename.begin(), filename.end(), filename.begin(), [](wchar_t c){ return static_cast<wchar_t>(towlower(c)); });
    if (filename == L"msiexec.exe" || filename == L"msiexec") {
        for (std::size_t i = 0; i + 1 < params.size(); ++i) {
            if (params[i] == L'/' && (params[i + 1] == L'i' || params[i + 1] == L'I')) {
                params[i + 1] = L'X';
                break;
            }
        }
    }
    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = L"runas";
    sei.lpFile = exe.c_str();
    sei.lpParameters = params.empty() ? nullptr : params.c_str();
    sei.nShow = SW_SHOWNORMAL;
    if (ShellExecuteExW(&sei)) return true;
    const DWORD code = GetLastError();
    error = code == ERROR_CANCELLED ? L"Удаление отменено пользователем." : L"Не удалось запустить штатный деинсталлятор. Код: " + std::to_wstring(code);
    return false;
}

std::wstring_view MaintenanceLabel(MaintenanceAction action) noexcept {
    switch (action) {
    case MaintenanceAction::ClearUpdateCache: return L"Очистить кэш Windows Update";
    case MaintenanceAction::ComponentCleanup: return L"DISM StartComponentCleanup";
    case MaintenanceAction::ResetBase: return L"DISM StartComponentCleanup /ResetBase";
    case MaintenanceAction::SfcScan: return L"SFC /scannow";
    case MaintenanceAction::DismCheckHealth: return L"DISM CheckHealth";
    case MaintenanceAction::DismScanHealth: return L"DISM ScanHealth";
    case MaintenanceAction::DismRestoreHealth: return L"DISM RestoreHealth";
    case MaintenanceAction::ChkdskScan: return L"CHKDSK C: /scan";
    }
    return L"Windows";
}

bool RunMaintenance(MaintenanceAction action, unsigned long& exitCode, std::wstring& error, std::stop_token stop) {
    switch (action) {
    case MaintenanceAction::ClearUpdateCache:
        return LaunchProcess(L"powershell.exe", L"-NoProfile -Command \"Stop-Service wuauserv,bits -Force -ErrorAction SilentlyContinue; Remove-Item \\\"$env:windir\\SoftwareDistribution\\Download\\*\\\" -Recurse -Force -ErrorAction SilentlyContinue; Start-Service bits,wuauserv -ErrorAction SilentlyContinue\"", true, exitCode, error, stop);
    case MaintenanceAction::ComponentCleanup:
        return LaunchProcess(L"dism.exe", L"/Online /Cleanup-Image /StartComponentCleanup", true, exitCode, error, stop);
    case MaintenanceAction::ResetBase:
        return LaunchProcess(L"dism.exe", L"/Online /Cleanup-Image /StartComponentCleanup /ResetBase", true, exitCode, error, stop);
    case MaintenanceAction::SfcScan:
        return LaunchProcess(L"sfc.exe", L"/scannow", true, exitCode, error, stop);
    case MaintenanceAction::DismCheckHealth:
        return LaunchProcess(L"dism.exe", L"/Online /Cleanup-Image /CheckHealth", true, exitCode, error, stop);
    case MaintenanceAction::DismScanHealth:
        return LaunchProcess(L"dism.exe", L"/Online /Cleanup-Image /ScanHealth", true, exitCode, error, stop);
    case MaintenanceAction::DismRestoreHealth:
        return LaunchProcess(L"dism.exe", L"/Online /Cleanup-Image /RestoreHealth", true, exitCode, error, stop);
    case MaintenanceAction::ChkdskScan:
        return LaunchProcess(L"chkdsk.exe", L"C: /scan", true, exitCode, error, stop);
    }
    error = L"Неизвестная операция обслуживания.";
    return false;
}

std::wstring_view ToolLabel(ToolAction action) noexcept {
    switch (action) {
    case ToolAction::TaskManager: return L"Диспетчер задач";
    case ToolAction::EventViewer: return L"Просмотр событий";
    case ToolAction::Startup: return L"Автозагрузка";
    case ToolAction::SystemRestore: return L"Восстановление системы";
    case ToolAction::WindowsSecurity: return L"Безопасность Windows";
    case ToolAction::Performance: return L"Монитор производительности";
    case ToolAction::Logs: return L"Логи DPopCleaner";
    }
    return L"Инструмент";
}

bool OpenTool(ToolAction action, std::wstring& error) {
    std::wstring file;
    std::wstring params;
    switch (action) {
    case ToolAction::TaskManager: file = L"taskmgr.exe"; break;
    case ToolAction::EventViewer: file = L"eventvwr.msc"; break;
    case ToolAction::Startup: file = L"ms-settings:startupapps"; break;
    case ToolAction::SystemRestore: file = L"rstrui.exe"; break;
    case ToolAction::WindowsSecurity: file = L"windowsdefender:"; break;
    case ToolAction::Performance: file = L"perfmon.exe"; break;
    case ToolAction::Logs: {
        const auto logs = Env(L"LOCALAPPDATA") / L"DPopCleaner/Logs";
        std::error_code ec; fs::create_directories(logs, ec);
        file = L"explorer.exe"; params = L"\"" + logs.wstring() + L"\""; break;
    }
    }
    const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", file.c_str(), params.empty() ? nullptr : params.c_str(), nullptr, SW_SHOWNORMAL));
    if (result > 32) return true;
    error = L"Не удалось открыть системный инструмент. Код: " + std::to_wstring(result);
    return false;
}

bool IsGuardCandidate(const fs::path& path) noexcept {
    static const std::set<std::wstring> extensions = {
        L".exe", L".dll", L".com", L".scr", L".msi", L".js", L".jse", L".vbs", L".vbe", L".ps1", L".bat", L".cmd"
    };
    std::wstring ext = path.extension().wstring();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](wchar_t c){ return static_cast<wchar_t>(towlower(c)); });
    return extensions.contains(ext);
}

GuardFolderResult ScanFolderWithAmsi(const fs::path& root, std::stop_token stop, unsigned maxFiles) {
    GuardFolderResult out{};
    std::error_code ec;
    if (root.empty() || !fs::exists(root, ec)) return out;
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
         it != end && !stop.stop_requested() && out.checked < maxFiles; it.increment(ec)) {
        if (ec) { ++out.errors; ec.clear(); continue; }
        if (!it->is_regular_file(ec)) { ec.clear(); continue; }
        const auto size = it->file_size(ec);
        ec.clear();
        if (!IsGuardCandidate(it->path()) || size > 64ull * 1024ull * 1024ull) { ++out.skipped; continue; }
        ++out.checked;
        std::wstring verdict, error;
        if (!dpop::guard::ScanFileWithAmsi(it->path(), verdict, error)) { ++out.errors; continue; }
        if (verdict.find(L"обнаружений нет") == std::wstring::npos) out.hits.push_back({it->path(), verdict});
    }
    return out;
}

bool QuarantineFile(const fs::path& file, fs::path& destination, std::wstring& error) {
    std::error_code ec;
    if (file.empty() || !fs::is_regular_file(file, ec)) { error = L"Файл для карантина не найден."; return false; }
    const auto local = Env(L"LOCALAPPDATA");
    if (local.empty()) { error = L"LOCALAPPDATA недоступен."; return false; }
    const auto dir = local / L"DPopCleaner/Quarantine";
    fs::create_directories(dir, ec);
    if (ec) { error = L"Не удалось создать папку карантина."; return false; }
    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    destination = dir / (std::to_wstring(stamp) + L"_" + file.filename().wstring() + L".quarantine");
    if (!MoveFileExW(file.c_str(), destination.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH)) {
        error = L"Не удалось переместить файл в карантин. Код: " + std::to_wstring(GetLastError());
        return false;
    }
    const fs::path metaPath(destination.wstring() + L".txt");
    std::wofstream meta(metaPath);
    if (meta) meta << L"original=" << file.wstring() << L"\nquarantined=" << destination.wstring() << L"\n";
    return true;
}

fs::path SettingsPath() {
    const auto local = Env(L"LOCALAPPDATA");
    return local.empty() ? fs::path{} : local / L"DPopCleaner/settings.json";
}

Settings LoadSettings() {
    Settings out{};
    const auto text = ReadText(SettingsPath());
    if (text.empty()) return out;
    out.confirmDestructive = ExtractBool(text, L"confirm_destructive", true);
    out.largeFileMB = std::clamp(ExtractUInt(text, L"large_file_mb", 500), 50u, 4096u);
    out.duplicateMinMB = std::clamp(ExtractUInt(text, L"duplicate_min_mb", 10), 1u, 1024u);
    out.runAtStartup = ExtractBool(text, L"run_at_startup", false);
    return out;
}

bool SaveSettings(const Settings& settings, std::wstring& error) {
    const auto path = SettingsPath();
    if (path.empty()) { error = L"LOCALAPPDATA недоступен."; return false; }
    std::error_code ec; fs::create_directories(path.parent_path(), ec);
    if (ec) { error = L"Не удалось создать папку настроек."; return false; }
    std::wofstream out(path, std::ios::trunc);
    if (!out) { error = L"Не удалось сохранить settings.json."; return false; }
    out << L"{\n"
        << L"  \"confirm_destructive\": " << (settings.confirmDestructive ? L"true" : L"false") << L",\n"
        << L"  \"large_file_mb\": " << settings.largeFileMB << L",\n"
        << L"  \"duplicate_min_mb\": " << settings.duplicateMinMB << L",\n"
        << L"  \"run_at_startup\": " << (settings.runAtStartup ? L"true" : L"false") << L"\n"
        << L"}\n";
    return true;
}

bool SetRunAtStartup(bool enabled, std::wstring& error) {
    HKEY key{};
    const LONG open = RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);
    if (open != ERROR_SUCCESS) { error = L"Не удалось открыть HKCU Run."; return false; }
    LONG rc = ERROR_SUCCESS;
    if (enabled) {
        std::wstring exe(32768, L'\0');
        DWORD n = GetModuleFileNameW(nullptr, exe.data(), static_cast<DWORD>(exe.size()));
        if (!n || n >= exe.size()) { RegCloseKey(key); error = L"Не удалось определить путь DPopCleaner.exe."; return false; }
        exe.resize(n);
        const std::wstring value = L"\"" + exe + L"\"";
        rc = RegSetValueExW(key, L"DPopCleaner", 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()), static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    } else {
        rc = RegDeleteValueW(key, L"DPopCleaner");
        if (rc == ERROR_FILE_NOT_FOUND) rc = ERROR_SUCCESS;
    }
    RegCloseKey(key);
    if (rc == ERROR_SUCCESS) return true;
    error = L"Не удалось изменить автозапуск. Код: " + std::to_wstring(rc);
    return false;
}

std::wstring FormatBytes(std::uint64_t bytes) {
    constexpr double KiB = 1024.0;
    constexpr double MiB = KiB * 1024.0;
    constexpr double GiB = MiB * 1024.0;
    std::wostringstream out;
    out << std::fixed << std::setprecision(1);
    if (bytes >= static_cast<std::uint64_t>(GiB)) out << static_cast<double>(bytes) / GiB << L" ГБ";
    else if (bytes >= static_cast<std::uint64_t>(MiB)) out << static_cast<double>(bytes) / MiB << L" МБ";
    else if (bytes >= static_cast<std::uint64_t>(KiB)) out << static_cast<double>(bytes) / KiB << L" КБ";
    else { out.unsetf(std::ios::floatfield); out << bytes << L" Б"; }
    return out.str();
}

} // namespace dpop::full
