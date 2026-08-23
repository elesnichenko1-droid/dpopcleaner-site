#pragma once
#include <windows.h>

namespace dpop::ui {

struct Palette {
    COLORREF background;
    COLORREF title;
    COLORREF control;
    COLORREF hover;
    COLORREF border;
    COLORREF text;
    COLORREF muted;
    COLORREF accent;
    COLORREF accentHover;
    COLORREF warning;
    COLORREF error;
};

const Palette& MidnightPalette() noexcept;
HFONT CreateUiFont(int points, int weight = FW_NORMAL) noexcept;

}
