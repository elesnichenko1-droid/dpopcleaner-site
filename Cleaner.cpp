#include "modules/Cleaner.h"
#include <windows.h>
#include <shellapi.h>
#include <filesystem>
#include <vector>
#include <cstdlib>

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
    std::wstring buf(32768, L'\0');
    DWORD n = GetTempPathW(static_cast<DWORD>(buf.size()), buf.data());
    if (!n || n >= buf.size()) return {};
    buf.resize(n);
    return fs::path(buf);
}

std::uint64_t EstimateTree(const fs::path& root) {
    std::uint64_t total = 0;
    std::error_code ec;
    if (root.empty() || !fs::exists(root, ec)) return 0;
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (it->is_regular_file(ec)) total += it->file_size(ec);
        ec.clear();
    }
    return total;
}

void CleanTree(const fs::path& root, dpop::cleaner::Result& r) {
    std::error_code ec;
    if (root.empty() || !fs::exists(root, ec)) return;
    std::vector<fs::path> dirs;
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (it->is_directory(ec)) { dirs.push_back(it->path()); ec.clear(); continue; }
        if (!it->is_regular_file(ec)) { ec.clear(); continue; }
        const auto size = it->file_size(ec);
        ec.clear();
        if (fs::remove(it->path(), ec)) { r.removedBytes += size; ++r.removedFiles; }
        else { ++r.failedFiles; ec.clear(); }
    }
    for (auto it = dirs.rbegin(); it != dirs.rend(); ++it) { fs::remove(*it, ec); ec.clear(); }
}

std::vector<fs::path> BrowserCaches() {
    const auto local = Env(L"LOCALAPPDATA");
    const auto roaming = Env(L"APPDATA");
    std::vector<fs::path> p;
    if (!local.empty()) {
        p.push_back(local / L"Microsoft/Edge/User Data/Default/Cache");
        p.push_back(local / L"Google/Chrome/User Data/Default/Cache");
        p.push_back(local / L"Yandex/YandexBrowser/User Data/Default/Cache");
        p.push_back(local / L"D3DSCache");
    }
    if (!roaming.empty()) {
        p.push_back(roaming / L"Mozilla/Firefox/Profiles");
    }
    return p;
}

bool IsFirefoxCachePath(const fs::path& p) {
    return p.wstring().find(L"Mozilla\\Firefox\\Profiles") != std::wstring::npos ||
           p.wstring().find(L"Mozilla/Firefox/Profiles") != std::wstring::npos;
}
}

namespace dpop::cleaner {
std::uint64_t EstimateUserTempBytes() { return EstimateTree(UserTemp()); }

std::uint64_t EstimateCrashDumpBytes() {
    const auto local = Env(L"LOCALAPPDATA");
    return local.empty() ? 0 : EstimateTree(local / L"CrashDumps");
}

std::uint64_t EstimateBrowserCacheBytes() {
    std::uint64_t total = 0;
    for (const auto& root : BrowserCaches()) {
        if (!IsFirefoxCachePath(root)) { total += EstimateTree(root); continue; }
        std::error_code ec;
        if (!fs::exists(root, ec)) continue;
        for (fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
            if (ec) { ec.clear(); continue; }
            if (!it->is_directory(ec)) continue;
            total += EstimateTree(it->path() / L"cache2");
            total += EstimateTree(it->path() / L"startupCache");
        }
    }
    return total;
}

std::uint64_t EstimateRecycleBinBytes() {
    SHQUERYRBINFO info{};
    info.cbSize = sizeof(info);
    if (SUCCEEDED(SHQueryRecycleBinW(nullptr, &info))) return static_cast<std::uint64_t>(info.i64Size);
    return 0;
}

Result CleanUserTemp() { Result r{}; CleanTree(UserTemp(), r); return r; }

Result CleanCrashDumps() {
    Result r{};
    const auto local = Env(L"LOCALAPPDATA");
    if (!local.empty()) CleanTree(local / L"CrashDumps", r);
    return r;
}

Result CleanBrowserCaches() {
    Result r{};
    for (const auto& root : BrowserCaches()) {
        if (!IsFirefoxCachePath(root)) { CleanTree(root, r); continue; }
        std::error_code ec;
        if (!fs::exists(root, ec)) continue;
        for (fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
            if (ec) { ec.clear(); continue; }
            if (!it->is_directory(ec)) continue;
            CleanTree(it->path() / L"cache2", r);
            CleanTree(it->path() / L"startupCache", r);
        }
    }
    return r;
}

bool EmptyRecycleBin(std::wstring& error) {
    const HRESULT hr = SHEmptyRecycleBinW(nullptr, nullptr, SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
    if (SUCCEEDED(hr)) return true;
    error = L"Не удалось очистить Корзину. HRESULT: " + std::to_wstring(static_cast<unsigned long>(hr));
    return false;
}
}
