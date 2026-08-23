#include "ui/Controls.h"
#include "ui/Theme.h"

#include <iostream>

namespace {

bool Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "ControlsTests FAILED: " << message << '\n';
        return false;
    }
    return true;
}

}

int main() {
    using namespace dpop::ui;

    const auto& p = MidnightPalette();

    const auto normal = ButtonColors(ButtonVisual::Normal, false);
    if (!Check(normal.background == p.control, "normal background mismatch")) return 1;
    if (!Check(normal.border == p.border, "normal border mismatch")) return 2;
    if (!Check(normal.text == p.text, "normal text mismatch")) return 3;

    const auto normalPressed = ButtonColors(ButtonVisual::Normal, true);
    if (!Check(normalPressed.background == p.hover, "pressed normal must use hover")) return 4;

    const auto accent = ButtonColors(ButtonVisual::Accent, false);
    if (!Check(accent.background == p.accent, "accent background mismatch")) return 5;
    if (!Check(accent.border == p.accent, "accent border mismatch")) return 6;
    if (!Check(accent.text == p.background, "accent text must use dark background color")) return 7;

    const auto accentPressed = ButtonColors(ButtonVisual::Accent, true);
    if (!Check(accentPressed.background == p.accentHover, "pressed accent mismatch")) return 8;

    const auto danger = ButtonColors(ButtonVisual::Danger, false);
    if (!Check(danger.background == p.control, "danger background mismatch")) return 9;
    if (!Check(danger.border == p.error, "danger border must use error color")) return 10;
    if (!Check(danger.text == p.text, "danger text mismatch")) return 11;

    std::cout << "ControlsTests PASS\n";
    return 0;
}
