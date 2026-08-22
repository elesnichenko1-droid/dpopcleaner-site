#include "core/Paths.h"
#include <windows.h>
#include <cstdlib>

namespace fs = std::filesystem;

namespace dpop::paths {
fs::path LocalAppData() {
    wchar_t* value = nullptr;
    size_t len = 0;
    if (_wdupenv_s(&value, &len, L"LOCALAPPDATA") == 0 && value) {
        fs::path out(value);
        free(value);
        return out;
    }
    if (value) free(value);
    wchar_t tmp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tmp);
    return fs::path(tmp);
}

fs::path DataDir() { return LocalAppData() / L"DPopCleaner"; }
fs::path LogsDir() { return DataDir() / L"Logs"; }
fs::path UpdatesDir() { return DataDir() / L"Updates"; }

fs::path ExecutableDir() {
    std::wstring buffer(32768, L'\0');
    DWORD n = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (!n) return fs::current_path();
    buffer.resize(n);
    return fs::path(buffer).parent_path();
}

void EnsureDirectories() {
    std::error_code ec;
    fs::create_directories(LogsDir(), ec);
    fs::create_directories(UpdatesDir(), ec);
}
}
