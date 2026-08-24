#pragma once
#include <array>

namespace dpop::ui {

struct ZapretRect {
    int x{};
    int y{};
    int width{};
    int height{};
};

struct ZapretPageRegions {
    int gap{};
    int headingBottom{};
    std::array<ZapretRect, 3> cards{};
    ZapretRect strategyLabel{};
    ZapretRect strategyCombo{};
    ZapretRect diagnostic{};
    std::array<ZapretRect, 8> actions{};
};

ZapretPageRegions ComputeZapretPageLayout(int width, int height, int dpi) noexcept;

}
