#include "ui/StatusBar.h"

#include "ui/Controls.h"
#include "ui/Theme.h"

#include <string>

namespace dpop::ui {
namespace {

constexpr int kStatusControlId = 1201;
constexpr int kLogControlId = 1202;
constexpr int kVersionControlId = 1203;

void MoveToBox(HWND hwnd, const Box& box) noexcept {
    if (!hwnd) {
        return;
    }

    MoveWindow(
        hwnd,
        box.x,
        box.y,
        box.width,
        box.height,
        TRUE
    );
}

void SetText(HWND hwnd, std::wstring_view text) noexcept {
    if (!hwnd) {
        return;
    }

    const std::wstring owned{text};
    SetWindowTextW(hwnd, owned.c_str());
}

}

StatusBar::~StatusBar() {
    Destroy();
}

bool StatusBar::Create(
    HWND parent,
    SessionLog& sessionLog
) noexcept {
    Destroy();

    sessionLog_ = &sessionLog;
    font_ = CreateUiFont(10, FW_NORMAL);

    status_ = CreateTextLabel(
        parent,
        kStatusControlId,
        L"Готово.",
        SS_LEFT | SS_NOPREFIX
    );

    log_ = CreateReadOnlyLogEdit(
        parent,
        kLogControlId
    );

    support_ = CreatePushButton(
        parent,
        kSupportCommandId,
        L"Поддержка",
        ButtonVisual::Normal
    );

    version_ = CreateTextLabel(
        parent,
        kVersionControlId,
        L"v0.3.5 BETA R1",
        SS_RIGHT | SS_NOPREFIX
    );

    if (!status_ || !log_ || !support_ || !version_) {
        Destroy();
        return false;
    }

    ApplyControlFont(status_, font_);
    ApplyControlFont(log_, font_);
    ApplyControlFont(support_, font_);
    ApplyControlFont(version_, font_);

    RefreshLogControl();
    return true;
}

void StatusBar::Destroy() noexcept {
    for (HWND* handle : {&status_, &log_, &support_, &version_}) {
        if (*handle && IsWindow(*handle)) {
            DestroyWindow(*handle);
        }
        *handle = nullptr;
    }

    if (font_) {
        DeleteObject(font_);
        font_ = nullptr;
    }

    sessionLog_ = nullptr;
}

void StatusBar::Layout(const ShellLayout& layout) noexcept {
    MoveToBox(status_, layout.status);
    MoveToBox(log_, layout.log);
    MoveToBox(support_, layout.support);
    MoveToBox(version_, layout.version);
}

void StatusBar::SetStatus(std::wstring_view text) noexcept {
    SetText(status_, text);
}

void StatusBar::Refresh() noexcept {
    RefreshLogControl();
}

void StatusBar::AppendLog(
    std::wstring_view category,
    EventLevel level,
    std::wstring_view message
) {
    if (!sessionLog_) {
        return;
    }

    sessionLog_->Append(category, level, message);
    RefreshLogControl();
}

HWND StatusBar::SupportButton() const noexcept {
    return support_;
}

HWND StatusBar::LogControl() const noexcept {
    return log_;
}

void StatusBar::RefreshLogControl() noexcept {
    if (!log_ || !sessionLog_) {
        return;
    }

    const std::wstring text = sessionLog_->RenderText();
    SetWindowTextW(log_, text.c_str());

    SendMessageW(log_, EM_SETSEL, static_cast<WPARAM>(-1), static_cast<LPARAM>(-1));
    SendMessageW(log_, EM_SCROLLCARET, 0, 0);
}

}
