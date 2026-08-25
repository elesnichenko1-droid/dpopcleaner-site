#include "ui/Controls.h"
#include "ui/Theme.h"

#include <iostream>
#include <stdexcept>
#include <windows.h>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void TestButtonPalettes() {
    const auto normal = dpop::ui::ButtonColors(dpop::ui::ButtonVisual::Normal);
    const auto accent = dpop::ui::ButtonColors(dpop::ui::ButtonVisual::Accent);
    const auto danger = dpop::ui::ButtonColors(dpop::ui::ButtonVisual::Danger);
    Require(normal.background != accent.background, "normal and accent backgrounds must differ");
    Require(danger.border != normal.border, "danger and normal borders must differ");
}

void TestPushButtonPreservesRequestedVisual() {
    HWND host = CreateWindowExW(
        0, L"STATIC", L"controls-test-host", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 240,
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    Require(host != nullptr, "could not create controls test host");

    HWND accent = dpop::ui::CreatePushButton(
        host, 4101, L"Accent", dpop::ui::ButtonVisual::Accent);
    Require(accent != nullptr, "could not create accent button");

    // PageBase must be able to recover the visual requested at creation time
    // when Windows sends WM_DRAWITEM. Current donor code ignores ButtonVisual,
    // so this is intentionally RED until metadata is preserved by Controls.
    HANDLE visual = GetPropW(accent, L"DPopCleaner.ButtonVisual");
    Require(visual != nullptr, "CreatePushButton discarded ButtonVisual metadata");
    Require(reinterpret_cast<INT_PTR>(visual) ==
                static_cast<INT_PTR>(dpop::ui::ButtonVisual::Accent) + 1,
            "stored button visual metadata is not Accent");

    DestroyWindow(accent);
    DestroyWindow(host);
}

} // namespace

int main() {
    try {
        TestButtonPalettes();
        TestPushButtonPreservesRequestedVisual();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ControlsTests: " << ex.what() << '\n';
        return 1;
    }
}
