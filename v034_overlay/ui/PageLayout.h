#pragma once

namespace dpop::ui {

struct LayoutRect {
    int x{};
    int y{};
    int width{};
    int height{};
};

struct PageRegions {
    LayoutRect heading;
    LayoutRect description;
    LayoutRect content;
    LayoutRect actions;
    int gap{};
    int margin{};
    int actionRows{};
    int actionRowHeight{};
};

PageRegions ComputePageRegions(int width, int height, int dpi, int actionCount) noexcept;

}
