#pragma once
#include <windows.h>

#include <atomic>
#include <deque>
#include <functional>
#include <mutex>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>

#include "ui/Layout.h"
#include "ui/SessionLog.h"

namespace dpop::ui {

inline constexpr UINT kPageLogChangedMessage = WM_APP + 70;
inline constexpr UINT kPageStatusChangedMessage = WM_APP + 71;
inline constexpr UINT kPageAsyncApplyMessage = WM_APP + 72;
inline constexpr UINT kPageAsyncFinishedMessage = WM_APP + 73;

class PageBase {
public:
    PageBase() = default;
    virtual ~PageBase();

    PageBase(const PageBase&) = delete;
    PageBase& operator=(const PageBase&) = delete;

    bool Create(HWND parent, SessionLog& sessionLog, std::wstring_view category);
    void Destroy() noexcept;
    void Show(bool visible) noexcept;
    void Layout(const Box& box) noexcept;
    void Cancel() noexcept;

    HWND Hwnd() const noexcept { return hwnd_; }
    bool IsBusy() const noexcept { return busy_.load(); }

protected:
    virtual bool OnCreate() = 0;
    virtual void OnLayout(int width, int height) noexcept = 0;
    virtual LRESULT OnMessage(UINT message, WPARAM wParam, LPARAM lParam, bool& handled);
    virtual void OnPaint(HDC dc, const RECT& client) noexcept;
    virtual void OnVisibilityChanged(bool visible) noexcept;

    void SetStatus(std::wstring_view text);
    void Log(EventLevel level, std::wstring_view message);

    void StartAsync(
        std::wstring_view status,
        std::function<void(std::stop_token)> work
    );
    void QueueApply(std::function<void()> apply);
    void DrainPendingApplies();
    void CompleteAsync();

    HWND Parent() const noexcept { return parent_; }
    SessionLog* LogSink() const noexcept { return sessionLog_; }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    static bool RegisterClass() noexcept;

    HWND parent_{};
    HWND hwnd_{};
    SessionLog* sessionLog_{};
    std::wstring category_;

    std::jthread worker_;
    std::atomic_bool busy_{false};
    std::mutex pendingMutex_;
    std::deque<std::function<void()>> pendingApply_;
};

}
