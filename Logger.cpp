#include "core/Logger.h"
#include "core/Paths.h"
#include <fstream>
#include <mutex>
#include <chrono>
#include <iomanip>

namespace {
std::mutex g_mutex;
void Write(const wchar_t* level, const std::wstring& message) {
    std::scoped_lock lock(g_mutex);
    dpop::paths::EnsureDirectories();
    std::wofstream f(dpop::paths::LogsDir() / L"activity.log", std::ios::app);
    if (!f) return;
    const auto now = std::chrono::system_clock::now();
    const auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &t);
    f << std::put_time(&tm, L"%Y-%m-%d %H:%M:%S") << L" [" << level << L"] " << message << L"\n";
}
}

namespace dpop::log {
void Info(const std::wstring& message) { Write(L"INFO", message); }
void Error(const std::wstring& message) { Write(L"ERROR", message); }
}
