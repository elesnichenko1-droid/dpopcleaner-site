#include "ui/pages/ZapretPageLayout.h"

#include <cassert>

namespace {
int Bottom(const dpop::ui::ZapretRect& rect) { return rect.y + rect.height; }
int Right(const dpop::ui::ZapretRect& rect) { return rect.x + rect.width; }

void Verify(int width, int height, int dpi) {
    const auto regions = dpop::ui::ComputeZapretPageLayout(width, height, dpi);
    assert(regions.headingBottom > 0);
    for (const auto& card : regions.cards) {
        assert(card.y >= regions.headingBottom);
        assert(card.width > 0 && card.height > 0);
    }
    assert(regions.strategyLabel.y >= Bottom(regions.cards[0]));
    assert(regions.strategyCombo.y >= regions.strategyLabel.y);
    assert(regions.diagnostic.y >= Bottom(regions.strategyCombo));
    assert(Bottom(regions.diagnostic) <= regions.actions[0].y - regions.gap);
    for (const auto& action : regions.actions) {
        assert(action.x >= 0 && action.y >= 0);
        assert(Right(action) <= width);
        assert(Bottom(action) <= height);
        assert(action.width >= 120);
        assert(action.height >= 34);
    }
}
}

int main() {
    Verify(1064, 480, 96);
    Verify(1164, 625, 96);
    Verify(1064, 480, 120);
    Verify(1064, 480, 144);
    return 0;
}
