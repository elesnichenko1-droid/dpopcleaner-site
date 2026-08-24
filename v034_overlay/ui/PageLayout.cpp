#include "ui/PageLayout.h"

#include <algorithm>

namespace dpop::ui {
namespace {

int ScaleLogical(int value, int dpi) noexcept {
    const int safeDpi = std::max(96, dpi);
    return std::max(1, (value * safeDpi + 48) / 96);
}

int NonNegative(int value) noexcept {
    return std::max(0, value);
}

}

int ComputePageContentTop(int dpi) noexcept {
    const int headingTop = ScaleLogical(12, dpi);
    const int headingHeight = ScaleLogical(34, dpi);
    const int gap = ScaleLogical(8, dpi);
    const int descriptionHeight = ScaleLogical(44, dpi);
    return headingTop + headingHeight + gap + descriptionHeight + gap;
}

PageRegions ComputePageRegions(int width, int height, int dpi, int actionCount) noexcept {
    PageRegions out{};
    out.margin = ScaleLogical(18, dpi);
    out.gap = ScaleLogical(8, dpi);
    out.actionRowHeight = ScaleLogical(38, dpi);
    out.actionRows = actionCount > 6 ? 2 : 1;

    const int headingTop = ScaleLogical(12, dpi);
    const int headingHeight = ScaleLogical(34, dpi);
    const int descriptionHeight = ScaleLogical(44, dpi);
    const int actionHeight = out.actionRows * out.actionRowHeight + (out.actionRows - 1) * out.gap;
    const int innerWidth = NonNegative(width - out.margin * 2);

    out.heading = {out.margin, headingTop, innerWidth, headingHeight};
    out.description = {
        out.margin,
        out.heading.y + out.heading.height + out.gap,
        innerWidth,
        descriptionHeight,
    };
    out.actions = {
        out.margin,
        NonNegative(height - out.margin - actionHeight),
        innerWidth,
        actionHeight,
    };

    const int contentTop = ComputePageContentTop(dpi);
    const int contentBottom = out.actions.y - out.gap;
    out.content = {
        out.margin,
        contentTop,
        innerWidth,
        NonNegative(contentBottom - contentTop),
    };
    return out;
}

}
