#include "ui/Theme.h"

#include <algorithm>

namespace dpop::ui {
namespace {

const Palette kMidnight{
    RGB(0x0B, 0x10, 0x17), // background #0B1017
    RGB(0x1B, 0x1F, 0x25), // title      #1B1F25
    RGB(0x14, 0x1D, 0x28), // control    #141D28
    RGB(0x1B, 0x27, 0x35), // hover      #1B2735
    RGB(0x2A, 0x39, 0x49), // border     #2A3949
    RGB(0xF6, 0xF7, 0xF9), // text       #F6F7F9
    RGB(0xB6, 0xC0, 0xCC), // muted      #B6C0CC
    RGB(0x39, 0xD0, 0xA0), // accent     #39D0A0
    RGB(0x47, 0xDF, 0xB0), // accentHover#47DFB0
    RGB(0xE4, 0xB6, 0x5D), // warning    #E4B65D
    RGB(0xE4, 0x6F, 0x6F), // error      #E46F6F
};

}

const Palette& MidnightPalette() noexcept {
    return kMidnight;
}

HFONT CreateUiFont(int points, int weight) noexcept {
    HDC dc = GetDC(nullptr);
    const int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSY) : 96;
    if (dc) {
        ReleaseDC(nullptr, dc);
    }

    const int safePoints = std::max(1, points);
    const int height = -MulDiv(safePoints, dpi, 72);

    return CreateFontW(
        height,
        0,
        0,
        0,
        weight,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );
}

}
