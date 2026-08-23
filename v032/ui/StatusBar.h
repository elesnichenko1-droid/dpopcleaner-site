#pragma once
#include <windows.h>

#include <string_view>

#include "ui/Layout.h"
#include "ui/SessionLog.h"

namespace dpop::ui {

inline constexpr int kSupportCommandId = 1200;

class StatusBar {
public:
    StatusBar() = default;
    ~StatusBar();

    StatusBar(const StatusBar&) = delete;
    StatusBar& operator=(const StatusBar&) = delete;

    bool Create(HWND parent, SessionLog& sessionLog) noexcept;
    void Destroy() noexcept;

    void Layout(const ShellLayout& layout) noexcept;
    void SetStatus(std::wstring_view text) noexcept;
    void Refresh() noexcept;

    void AppendLog(
        std::wstring_view category,
        EventLevel level,
        std::wstring_view message
    );

    HWND SupportButton() const noexcept;
    HWND LogControl() const noexcept;

private:
    void RefreshLogControl() noexcept;

    SessionLog* sessionLog_{};
    HWND status_{};
    HWND log_{};
    HWND support_{};
    HWND version_{};
    HFONT font_{};
};

}
