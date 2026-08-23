#pragma once
#include <chrono>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dpop::ui {

enum class EventLevel {
    Info,
    Warning,
    Error
};

struct SessionEvent {
    std::chrono::system_clock::time_point time;
    std::wstring category;
    EventLevel level;
    std::wstring message;
};

class SessionLog {
public:
    void Append(
        std::wstring_view category,
        EventLevel level,
        std::wstring_view message
    );

    std::span<const SessionEvent> Events() const noexcept;
    std::wstring RenderText() const;
    void Clear() noexcept;

private:
    std::vector<SessionEvent> events_;
};

}
