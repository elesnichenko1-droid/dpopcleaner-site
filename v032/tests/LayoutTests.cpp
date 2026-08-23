#include "ui/Layout.h"

#include <iostream>
#include <utility>

namespace {

bool Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "LayoutTests FAILED: " << message << '\n';
        return false;
    }
    return true;
}

bool Inside(const dpop::ui::Box& child, const dpop::ui::Box& parent) {
    return child.x >= parent.x &&
           child.y >= parent.y &&
           child.x + child.width <= parent.x + parent.width &&
           child.y + child.height <= parent.y + parent.height;
}

bool DisjointVertically(const dpop::ui::Box& a, const dpop::ui::Box& b) {
    return a.y + a.height <= b.y ||
           b.y + b.height <= a.y;
}

}

int main() {
    using namespace dpop::ui;

    const std::pair<int, int> sizes[] = {
        {1100, 700},
        {1200, 850},
        {1920, 1080},
    };

    for (const auto [w, h] : sizes) {
        const auto l = ComputeShellLayout(w, h);

        if (!Check(l.header.x == 0 && l.header.y == 0,
                   "header must start at origin")) return 1;
        if (!Check(l.header.width == w,
                   "header must span client width")) return 2;
        if (!Check(l.tabs.width == w &&
                   l.tabs.y == l.header.height,
                   "tabs must sit below header")) return 3;
        if (!Check(l.content.width > 900,
                   "content must remain wide enough")) return 4;
        if (!Check(l.content.height > 350,
                   "content must remain tall enough")) return 5;
        if (!Check(l.footer.y + l.footer.height == h,
                   "footer must be bottom anchored")) return 6;
        if (!Check(l.content.y + l.content.height < l.footer.y,
                   "content must not overlap footer")) return 7;

        if (!Check(Inside(l.status, l.footer),
                   "status must stay inside footer")) return 8;
        if (!Check(Inside(l.log, l.footer),
                   "log must stay inside footer")) return 9;
        if (!Check(Inside(l.support, l.footer),
                   "support must stay inside footer")) return 10;
        if (!Check(Inside(l.version, l.footer),
                   "version must stay inside footer")) return 11;

        for (const auto& card : l.overviewCards) {
            if (!Check(Inside(card, l.content),
                       "overview card must stay inside content")) return 12;
            if (!Check(card.width >= 250,
                       "overview card must be at least 250px wide")) return 13;
            if (!Check(card.height >= 90,
                       "overview card must be at least 90px tall")) return 14;
        }

        for (const auto& action : l.overviewActions) {
            if (!Check(Inside(action, l.content),
                       "overview action must stay inside content")) return 15;
            if (!Check(action.height == 40,
                       "overview action height must be stable")) return 16;
        }

        for (const auto& card : l.overviewCards) {
            for (const auto& action : l.overviewActions) {
                if (!Check(DisjointVertically(card, action),
                           "cards must not overlap quick actions")) return 17;
            }
        }
    }

    const auto minimum = ComputeShellLayout(1100, 700);
    if (!Check(minimum.overviewActions[3].y >
               minimum.overviewActions[0].y,
               "1100px layout must wrap quick actions")) return 18;
    if (!Check(minimum.overviewActions[4].y ==
               minimum.overviewActions[3].y,
               "wrapped action row must align")) return 19;

    const auto normal = ComputeShellLayout(1200, 850);
    if (!Check(normal.overviewActions[0].y ==
               normal.overviewActions[4].y,
               "1200px actions must use one row")) return 20;
    if (!Check(normal.overviewCards[0].x <
               normal.overviewCards[1].x &&
               normal.overviewCards[1].x <
               normal.overviewCards[2].x,
               "first card row must be 3 columns")) return 21;
    if (!Check(normal.overviewCards[3].y >
               normal.overviewCards[0].y,
               "second card row must be below first")) return 22;

    const auto clamped = ComputeShellLayout(800, 500);
    if (!Check(clamped.header.width == 1100,
               "width below minimum must clamp")) return 23;
    if (!Check(clamped.footer.y + clamped.footer.height == 700,
               "height below minimum must clamp")) return 24;

    std::cout << "LayoutTests PASS\n";
    return 0;
}
