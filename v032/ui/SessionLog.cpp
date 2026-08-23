#include "ui/SessionLog.h"

#include <windows.h>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace dpop::ui {
namespace {
namespace fs = std::filesystem;

std::wstring NormalizeLine(std::wstring_view text) {
    std::wstring result{text};
    for (auto& ch : result) if (ch == L'\r' || ch == L'\n' || ch == L'\t') ch = L' ';
    return result;
}

std::wstring_view LevelName(EventLevel level) noexcept {
    switch (level) {
    case EventLevel::Warning: return L"WARNING";
    case EventLevel::Error: return L"ERROR";
    case EventLevel::Info:
    default: return L"INFO";
    }
}

std::wstring FormatEvent(const SessionEvent& event) {
    const std::time_t raw = std::chrono::system_clock::to_time_t(event.time);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &raw);
#else
    localtime_r(&raw, &local);
#endif
    std::wostringstream out;
    out << std::put_time(&local, L"%H:%M:%S")
        << L" [" << event.category << L"]"
        << L" [" << LevelName(event.level) << L"] "
        << event.message;
    return out.str();
}

fs::path LogPath() {
    wchar_t* value = nullptr;
    size_t len = 0;
    if (_wdupenv_s(&value, &len, L"LOCALAPPDATA") != 0 || !value) {
        if (value) free(value);
        return {};
    }
    fs::path root(value);
    free(value);
    return root / L"DPopCleaner/Logs/DPopCleaner.log";
}

std::string Utf8(std::wstring_view text) {
    if (text.empty()) return {};
    const int needed = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};
    std::string out(static_cast<std::size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), needed, nullptr, nullptr);
    return out;
}

void AppendDiskLog(const SessionEvent& event) noexcept {
    try {
        const auto path = LogPath();
        if (path.empty()) return;
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if (ec) return;
        std::ofstream out(path, std::ios::binary | std::ios::app);
        if (!out) return;
        const auto line = Utf8(FormatEvent(event));
        out.write(line.data(), static_cast<std::streamsize>(line.size()));
        out.write("\r\n", 2);
    } catch (...) {
    }
}

}

void SessionLog::Append(std::wstring_view category, EventLevel level, std::wstring_view message) {
    events_.push_back(SessionEvent{
        std::chrono::system_clock::now(),
        NormalizeLine(category),
        level,
        NormalizeLine(message)
    });
    AppendDiskLog(events_.back());
}

std::span<const SessionEvent> SessionLog::Events() const noexcept {
    return std::span<const SessionEvent>{events_.data(), events_.size()};
}

std::wstring SessionLog::RenderText() const {
    std::wostringstream out;
    for (std::size_t i = 0; i < events_.size(); ++i) {
        out << FormatEvent(events_[i]);
        if (i + 1 < events_.size()) out << L"\r\n";
    }
    return out.str();
}

void SessionLog::Clear() noexcept { events_.clear(); }

}
