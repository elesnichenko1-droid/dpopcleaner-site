#include "ui/PageBase.h"

#include "ui/Theme.h"

#include <exception>
#include <string>

namespace dpop::ui {
namespace {
constexpr wchar_t kRecoveryPageClass[] = L"DPopCleaner032RecoveryPage";
}

PageBase::~PageBase() {
    Destroy();
}

bool PageBase::RegisterClass() noexcept {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &PageBase::WindowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kRecoveryPageClass;

    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }
    return true;
}

bool PageBase::Create(HWND parent, SessionLog& sessionLog, std::wstring_view category) {
    Destroy();
    if (!RegisterClass()) return false;

    parent_ = parent;
    sessionLog_ = &sessionLog;
    category_.assign(category);

    hwnd_ = CreateWindowExW(
        0,
        kRecoveryPageClass,
        L"",
        WS_CHILD | WS_CLIPCHILDREN,
        0, 0, 0, 0,
        parent_,
        nullptr,
        GetModuleHandleW(nullptr),
        this
    );
    if (!hwnd_) {
        parent_ = nullptr;
        sessionLog_ = nullptr;
        return false;
    }

    if (!OnCreate()) {
        Destroy();
        return false;
    }

    ShowWindow(hwnd_, SW_HIDE);
    return true;
}

void PageBase::Destroy() noexcept {
    Cancel();
    if (worker_.joinable()) worker_.join();

    {
        std::lock_guard lock(pendingMutex_);
        pendingApply_.clear();
    }

    if (hwnd_ && IsWindow(hwnd_)) DestroyWindow(hwnd_);
    hwnd_ = nullptr;
    parent_ = nullptr;
    sessionLog_ = nullptr;
    category_.clear();
    busy_.store(false);
}

void PageBase::Show(bool visible) noexcept {
    if (!hwnd_) return;
    ShowWindow(hwnd_, visible ? SW_SHOW : SW_HIDE);
    OnVisibilityChanged(visible);
}

void PageBase::Layout(const Box& box) noexcept {
    if (!hwnd_) return;
    MoveWindow(hwnd_, box.x, box.y, box.width, box.height, TRUE);
    OnLayout(box.width, box.height);
}

void PageBase::Cancel() noexcept {
    if (worker_.joinable()) worker_.request_stop();
}

void PageBase::SetStatus(std::wstring_view text) {
    if (!parent_) return;
    HWND shell = GetParent(parent_);
    if (!shell) return;
    const std::wstring owned{text};
    SendMessageW(
        shell,
        kPageStatusChangedMessage,
        0,
        reinterpret_cast<LPARAM>(owned.c_str())
    );
}

void PageBase::Log(EventLevel level, std::wstring_view message) {
    if (!sessionLog_) return;
    sessionLog_->Append(category_, level, message);
    if (parent_) {
        HWND shell = GetParent(parent_);
        if (shell) PostMessageW(shell, kPageLogChangedMessage, 0, 0);
    }
}

void PageBase::StartAsync(
    std::wstring_view status,
    std::function<void(std::stop_token)> work
) {
    if (busy_.exchange(true)) {
        SetStatus(L"Операция уже выполняется. Сначала останови текущую.");
        return;
    }

    if (worker_.joinable()) worker_.join();
    SetStatus(status);
    const HWND notifyWindow = hwnd_;

    worker_ = std::jthread([this, notifyWindow, work = std::move(work)](std::stop_token token) mutable {
        try {
            work(token);
        } catch (const std::exception&) {
            QueueApply([this] {
                SetStatus(L"Операция завершилась ошибкой.");
                Log(EventLevel::Error, L"Фоновая операция завершилась исключением.");
            });
        } catch (...) {
            QueueApply([this] {
                SetStatus(L"Операция завершилась неизвестной ошибкой.");
                Log(EventLevel::Error, L"Фоновая операция завершилась неизвестным исключением.");
            });
        }

        if (notifyWindow && IsWindow(notifyWindow)) {
            PostMessageW(notifyWindow, kPageAsyncFinishedMessage, 0, 0);
        } else {
            busy_.store(false);
        }
    });
}

void PageBase::QueueApply(std::function<void()> apply) {
    if (!apply) return;
    {
        std::lock_guard lock(pendingMutex_);
        pendingApply_.push_back(std::move(apply));
    }
    if (hwnd_ && IsWindow(hwnd_)) PostMessageW(hwnd_, kPageAsyncApplyMessage, 0, 0);
}

void PageBase::DrainPendingApplies() {
    std::deque<std::function<void()>> pending;
    {
        std::lock_guard lock(pendingMutex_);
        pending.swap(pendingApply_);
    }

    while (!pending.empty()) {
        auto apply = std::move(pending.front());
        pending.pop_front();
        if (apply) apply();
    }
}

void PageBase::CompleteAsync() {
    if (worker_.joinable()) worker_.join();
    busy_.store(false);
    DrainPendingApplies();
}

LRESULT PageBase::OnMessage(UINT, WPARAM, LPARAM, bool& handled) {
    handled = false;
    return 0;
}

void PageBase::OnPaint(HDC dc, const RECT& client) noexcept {
    const auto& p = MidnightPalette();
    SetDCBrushColor(dc, p.background);
    FillRect(dc, &client, reinterpret_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
}

void PageBase::OnVisibilityChanged(bool) noexcept {
}

LRESULT CALLBACK PageBase::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    PageBase* self = reinterpret_cast<PageBase*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        self = static_cast<PageBase*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    }

    return self ? self->HandleMessage(message, wParam, lParam)
                : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT PageBase::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    const auto& p = MidnightPalette();

    switch (message) {
    case WM_SIZE:
        OnLayout(LOWORD(lParam), HIWORD(lParam));
        InvalidateRect(hwnd_, nullptr, TRUE);
        return 0;

    case kPageAsyncApplyMessage:
        DrainPendingApplies();
        return 0;

    case kPageAsyncFinishedMessage:
        CompleteAsync();
        return 0;

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkMode(dc, OPAQUE);
        SetBkColor(dc, p.control);
        SetTextColor(dc, p.text);
        SetDCBrushColor(dc, p.control);
        return reinterpret_cast<LRESULT>(GetStockObject(DC_BRUSH));
    }

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkMode(dc, OPAQUE);
        SetBkColor(dc, p.control);
        SetTextColor(dc, p.text);
        SetDCBrushColor(dc, p.control);
        return reinterpret_cast<LRESULT>(GetStockObject(DC_BRUSH));
    }

    case WM_ERASEBKGND: {
        RECT rc{};
        GetClientRect(hwnd_, &rc);
        OnPaint(reinterpret_cast<HDC>(wParam), rc);
        return 1;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd_, &ps);
        RECT rc{};
        GetClientRect(hwnd_, &rc);
        OnPaint(dc, rc);
        EndPaint(hwnd_, &ps);
        return 0;
    }

    default:
        break;
    }

    bool handled = false;
    const LRESULT result = OnMessage(message, wParam, lParam, handled);
    if (handled) return result;

    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

}
