#pragma once

#include <windows.h>
#include <shellapi.h>

namespace dpop::ui {

class TrayIcon {
public:
    enum class Command {
        None,
        Restore,
        Exit,
    };

    TrayIcon() = default;
    ~TrayIcon() { Destroy(); }
    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

    bool Create(HWND owner, UINT callbackMessage);
    void Destroy() noexcept;
    bool SetVisible(bool visible);
    bool Visible() const noexcept { return visible_; }
    Command HandleCallback(LPARAM eventMessage);

private:
    Command ShowContextMenu();
    bool Add();
    void Remove() noexcept;

    HWND owner_{};
    UINT callbackMessage_{};
    NOTIFYICONDATAW data_{};
    bool visible_{};
};

} // namespace dpop::ui
