#include "ui/SessionLog.h"

#include <ctime>
#include <iomanip>
#include <sstream>

namespace dpop::ui {
namespace {

std::wstring NormalizeLine(std::wstring_view text) {
    std::wstring result{text};
    for (auto& ch : result) {
        if (ch == L'\r' || ch == L'\n' || ch == L'\t') {
            ch = L' ';
        }
    }
    return result;
}

std::wstring_view LevelName(EventLevel level) noexcept {
    switch (level) {
    case EventLevel::Warning:
        return L"WARNING";
    case EventLevel::Error:
        return L"ERROR";
    case EventLevel::Info:
    default:
        return L"INFO";
    }
}

}

void SessionLog::Append(
    std::wstring_view category,
    EventLevel level,
    std::wstring_view message
) {
    events_.push_back(SessionEvent{
        std::chrono::system_clock::now(),
        NormalizeLine(category),
        level,
        NormalizeLine(message)
    });
}

std::span<const SessionEvent> SessionLog::Events() const noexcept {
    return std::span<const SessionEvent>{events_.data(), events_.size()};
}

std::wstring SessionLog::RenderText() const {
    std::wostringstream out;

    for (std::size_t i = 0; i < events_.size(); ++i) {
        const auto& event = events_[i];

        const std::time_t raw =
            std::chrono::system_clock::to_time_t(event.time);

        std::tm local{};
#ifdef _WIN32
        localtime_s(&local, &raw);
#else
        localtime_r(&raw, &local);
#endif

        out << std::put_time(&local, L"%H:%M:%S")
            << L" [" << event.category << L"]"
            << L" [" << LevelName(event.level) << L"] "
            << event.message;

        if (i + 1 < events_.size()) {
            out << L"\r\n";
        }
    }

    return out.str();
}

void SessionLog::Clear() noexcept {
    events_.clear();
}

}
