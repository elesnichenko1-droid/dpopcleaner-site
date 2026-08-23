#pragma once
#include <windows.h>

namespace dpop::ui {

class SettingsStubPage {
public:
    SettingsStubPage() = default;
    ~SettingsStubPage();

    SettingsStubPage(const SettingsStubPage&) = delete;
    SettingsStubPage& operator=(const SettingsStubPage&) = delete;

    bool Create(HWND parent) noexcept;
    void Destroy() noexcept;
    void Show(bool visible) noexcept;
    void Layout(int width, int height) noexcept;

private:
    HWND heading_{};
    HWND language_{};
    HWND theme_{};
    HWND beta_{};
    HWND note_{};
    HFONT headingFont_{};
    HFONT bodyFont_{};
};

}
