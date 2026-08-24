#pragma once

#include <algorithm>

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

inline int ScalePageLogical(int value, int dpi) noexcept {
    const int safeDpi = std::max(96, dpi);
    return std::max(1, (value * safeDpi + 48) / 96);
}

inline int ComputePageContentTop(int dpi) noexcept {
    const int headingTop = ScalePageLogical(12, dpi);
    const int headingHeight = ScalePageLogical(34, dpi);
    const int gap = ScalePageLogical(8, dpi);
    const int descriptionHeight = ScalePageLogical(44, dpi);
    return headingTop + headingHeight + gap + descriptionHeight + gap;
}

inline PageRegions ComputePageRegions(int width, int height, int dpi, int actionCount) noexcept {
    PageRegions out{};
    out.margin = ScalePageLogical(18, dpi);
    out.gap = ScalePageLogical(8, dpi);
    out.actionRowHeight = ScalePageLogical(38, dpi);
    out.actionRows = actionCount > 6 ? 2 : 1;

    const int headingTop = ScalePageLogical(12, dpi);
    const int headingHeight = ScalePageLogical(34, dpi);
    const int descriptionHeight = ScalePageLogical(44, dpi);
    const int actionHeight = out.actionRows * out.actionRowHeight + (out.actionRows - 1) * out.gap;
    const int innerWidth = std::max(0, width - out.margin * 2);

    out.heading = {out.margin, headingTop, innerWidth, headingHeight};
    out.description = {
        out.margin,
        out.heading.y + out.heading.height + out.gap,
        innerWidth,
        descriptionHeight,
    };
    out.actions = {
        out.margin,
        std::max(0, height - out.margin - actionHeight),
        innerWidth,
        actionHeight,
    };

    const int contentTop = ComputePageContentTop(dpi);
    const int contentBottom = out.actions.y - out.gap;
    out.content = {
        out.margin,
        contentTop,
        innerWidth,
        std::max(0, contentBottom - contentTop),
    };
    return out;
}

} // namespace dpop::ui
