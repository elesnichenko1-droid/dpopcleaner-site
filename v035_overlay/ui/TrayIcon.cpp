#include "ui/TrayIcon.h"

#include <cwchar>

namespace dpop::ui {
namespace {
constexpr UINT kTrayId = 0xD035;
constexpr UINT kOpenCommand = 1;
constexpr UINT kExitCommand = 2;
}

bool TrayIcon::Create(HWND owner, UINT callbackMessage) {
    Destroy();
    if (!owner || callbackMessage < WM_APP) return false;
    owner_ = owner;
    callbackMessage_ = callbackMessage;

    data_ = {};
    data_.cbSize = sizeof(data_);
    data_.hWnd = owner_;
    data_.uID = kTrayId;
    data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    data_.uCallbackMessage = callbackMessage_;
    data_.hIcon = reinterpret_cast<HICON>(GetClassLongPtrW(owner_, GCLP_HICONSM));
    if (!data_.hIcon) data_.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(data_.szTip, L"DPopCleaner 0.3.5 BETA R1");
    return true;
}

void TrayIcon::Destroy() noexcept {
    Remove();
    owner_ = nullptr;
    callbackMessage_ = 0;
    data_ = {};
}

bool TrayIcon::Add() {
    if (!owner_) return false;
    if (visible_) return true;
    if (!Shell_NotifyIconW(NIM_ADD, &data_)) return false;
    data_.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &data_);
    visible_ = true;
    return true;
}

void TrayIcon::Remove() noexcept {
    if (!visible_) return;
    Shell_NotifyIconW(NIM_DELETE, &data_);
    visible_ = false;
}

bool TrayIcon::SetVisible(bool visible) {
    if (visible) return Add();
    Remove();
    return true;
}

TrayIcon::Command TrayIcon::ShowContextMenu() {
    if (!owner_) return Command::None;
    HMENU menu = CreatePopupMenu();
    if (!menu) return Command::None;
    AppendMenuW(menu, MF_STRING | MF_DEFAULT, kOpenCommand, L"Открыть DPopCleaner");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kExitCommand, L"Выход");

    POINT point{};
    GetCursorPos(&point);
    SetForegroundWindow(owner_);
    const UINT command = TrackPopupMenu(
        menu,
        TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
        point.x,
        point.y,
        0,
        owner_,
        nullptr);
    DestroyMenu(menu);
    PostMessageW(owner_, WM_NULL, 0, 0);

    if (command == kOpenCommand) return Command::Restore;
    if (command == kExitCommand) return Command::Exit;
    return Command::None;
}

TrayIcon::Command TrayIcon::HandleCallback(LPARAM eventMessage) {
    const UINT event = LOWORD(static_cast<DWORD_PTR>(eventMessage));
    switch (event) {
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
        return Command::Restore;
    case WM_CONTEXTMENU:
    case WM_RBUTTONUP:
        return ShowContextMenu();
    default:
        return Command::None;
    }
}

} // namespace dpop::ui
