#include "ui/pages/ZapretPageLayout.h"

#include <algorithm>
#include <cmath>

namespace dpop::ui {
namespace {
int Scale(int value, double scale) {
    return std::max(1, static_cast<int>(std::lround(value * scale)));
}
}

ZapretPageRegions ComputeZapretPageLayout(int width, int height, int dpi) noexcept {
    const double scale = std::clamp(static_cast<double>(dpi > 0 ? dpi : 96) / 96.0, 1.0, 1.25);
    const int margin = Scale(18, scale);
    const int gap = Scale(10, scale);
    const int headingBottom = Scale(64, scale);
    const int cardHeight = Scale(84, scale);
    const int labelHeight = Scale(18, scale);
    const int comboHeight = Scale(30, scale);
    const int actionHeight = Scale(36, scale);
    const int actionBlockHeight = actionHeight * 2 + gap;
    const int innerWidth = std::max(1, width - margin * 2);
    const int cardWidth = std::max(1, (innerWidth - gap * 2) / 3);
    const int actionWidth = std::max(1, (innerWidth - gap * 2) / 3);

    ZapretPageRegions regions{};
    regions.gap = gap;
    regions.headingBottom = headingBottom;
    const int cardsY = headingBottom;
    for (int index = 0; index < 3; ++index) {
        regions.cards[static_cast<std::size_t>(index)] = {
            margin + index * (cardWidth + gap), cardsY, cardWidth, cardHeight
        };
    }

    const int afterCards = cardsY + cardHeight + gap;
    regions.strategyLabel = {margin, afterCards, innerWidth, labelHeight};
    regions.strategyCombo = {
        margin, afterCards + labelHeight + Scale(4, scale), innerWidth, comboHeight
    };

    const int actionsY = std::max(
        regions.strategyCombo.y + regions.strategyCombo.height + gap + Scale(54, scale),
        height - margin - actionBlockHeight);
    for (int index = 0; index < 6; ++index) {
        const int row = index / 3;
        const int column = index % 3;
        regions.actions[static_cast<std::size_t>(index)] = {
            margin + column * (actionWidth + gap),
            actionsY + row * (actionHeight + gap),
            actionWidth,
            actionHeight
        };
    }

    const int diagnosticY = regions.strategyCombo.y + regions.strategyCombo.height + gap;
    const int diagnosticBottom = std::max(diagnosticY + 1, actionsY - gap);
    regions.diagnostic = {
        margin,
        diagnosticY,
        innerWidth,
        std::max(1, diagnosticBottom - diagnosticY)
    };
    return regions;
}

}
