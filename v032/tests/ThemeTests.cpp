#include "ui/Theme.h"

#include <iostream>

namespace {

bool Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "ThemeTests FAILED: " << message << '\n';
        return false;
    }
    return true;
}

}

int main() {
    using namespace dpop::ui;
    const auto& p = MidnightPalette();

    if (!Check(p.background == RGB(0x0B, 0x10, 0x17), "background must be #0B1017")) return 1;
    if (!Check(p.title == RGB(0x1B, 0x1F, 0x25), "title must be #1B1F25")) return 2;
    if (!Check(p.control == RGB(0x14, 0x1D, 0x28), "control must be #141D28")) return 3;
    if (!Check(p.hover == RGB(0x1B, 0x27, 0x35), "hover must be #1B2735")) return 4;
    if (!Check(p.border == RGB(0x2A, 0x39, 0x49), "border must be #2A3949")) return 5;
    if (!Check(p.text == RGB(0xF6, 0xF7, 0xF9), "text must be #F6F7F9")) return 6;
    if (!Check(p.muted == RGB(0xB6, 0xC0, 0xCC), "muted must be #B6C0CC")) return 7;
    if (!Check(p.accent == RGB(0x39, 0xD0, 0xA0), "accent must be #39D0A0")) return 8;
    if (!Check(p.accentHover == RGB(0x47, 0xDF, 0xB0), "accent hover must be #47DFB0")) return 9;
    if (!Check(p.warning == RGB(0xE4, 0xB6, 0x5D), "warning must be #E4B65D")) return 10;
    if (!Check(p.error == RGB(0xE4, 0x6F, 0x6F), "error must be #E46F6F")) return 11;

    std::cout << "ThemeTests PASS\n";
    return 0;
}
