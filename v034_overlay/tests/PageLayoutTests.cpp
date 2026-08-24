#include "ui/PageLayout.h"

#include <array>
#include <cassert>
#include <iostream>

using dpop::ui::LayoutRect;
using dpop::ui::PageRegions;
using dpop::ui::ComputePageRegions;

namespace {

bool Inside(const LayoutRect& r, int width, int height) {
    return r.x >= 0 && r.y >= 0 && r.width >= 0 && r.height >= 0 &&
        r.x + r.width <= width && r.y + r.height <= height;
}

bool Overlap(const LayoutRect& a, const LayoutRect& b) {
    return a.x < b.x + b.width && a.x + a.width > b.x &&
        a.y < b.y + b.height && a.y + a.height > b.y;
}

void Verify(int width, int height, int dpi, int actions) {
    const PageRegions r = ComputePageRegions(width, height, dpi, actions);
    assert(r.gap >= 8);
    assert(r.heading.height > 0);
    assert(r.description.height > 0);
    assert(r.content.height > 0);
    assert(r.actions.height > 0);
    assert(r.actionRows >= 1 && r.actionRows <= 2);
    assert(Inside(r.heading, width, height));
    assert(Inside(r.description, width, height));
    assert(Inside(r.content, width, height));
    assert(Inside(r.actions, width, height));
    assert(!Overlap(r.heading, r.description));
    assert(!Overlap(r.heading, r.content));
    assert(!Overlap(r.description, r.content));
    assert(!Overlap(r.content, r.actions));
    assert(r.description.y >= r.heading.y + r.heading.height + r.gap);
    assert(r.content.y >= r.description.y + r.description.height + r.gap);
    assert(r.actions.y >= r.content.y + r.content.height + r.gap);
}

}

int main() {
    for (int dpi : std::array<int, 3>{96, 120, 144}) {
        Verify(1100, 700, dpi, 6);
        Verify(1200, 850, dpi, 6);
        Verify(1200, 850, dpi, 8);
        Verify(1920, 1000, dpi, 8);
    }
    std::cout << "PageLayoutTests passed\n";
    return 0;
}
