#include "ui/Layout.h"

#include <algorithm>

namespace dpop::ui {
namespace {

constexpr int kMinClientWidth = 1100;
constexpr int kMinClientHeight = 700;
constexpr int kMargin = 24;
constexpr int kHeaderHeight = 100;
constexpr int kTabsHeight = 54;
constexpr int kFooterHeight = 150;
constexpr int kGap = 14;

Box MakeBox(int x, int y, int width, int height) noexcept {
    return {x, y, std::max(0, width), std::max(0, height)};
}

}

ShellLayout ComputeShellLayout(int clientWidth, int clientHeight) noexcept {
    const int w = std::max(kMinClientWidth, clientWidth);
    const int h = std::max(kMinClientHeight, clientHeight);

    ShellLayout layout{};

    layout.header = MakeBox(0, 0, w, kHeaderHeight);
    layout.tabs = MakeBox(0, kHeaderHeight, w, kTabsHeight);

    const int contentY = kHeaderHeight + kTabsHeight + kGap;
    const int footerY = h - kFooterHeight;
    const int contentHeight = footerY - kGap - contentY;

    layout.content = MakeBox(
        kMargin,
        contentY,
        w - (kMargin * 2),
        contentHeight
    );

    layout.footer = MakeBox(0, footerY, w, kFooterHeight);

    layout.status = MakeBox(
        kMargin,
        footerY + 8,
        w - (kMargin * 2),
        22
    );

    layout.log = MakeBox(
        kMargin,
        footerY + 34,
        w - (kMargin * 2),
        76
    );

    layout.support = MakeBox(
        kMargin,
        footerY + 116,
        110,
        26
    );

    layout.version = MakeBox(
        w - kMargin - 150,
        footerY + 116,
        150,
        26
    );

    const bool singleActionRow = w >= 1180;
    const int actionRows = singleActionRow ? 1 : 2;
    const int actionHeight = 40;
    const int actionAreaHeight =
        actionRows * actionHeight + (actionRows - 1) * kGap;

    const int cardsTop = layout.content.y;
    const int cardsAreaHeight =
        layout.content.height - actionAreaHeight - kGap;
    const int cardWidth =
        (layout.content.width - (2 * kGap)) / 3;
    const int cardHeight =
        (cardsAreaHeight - kGap) / 2;

    for (int row = 0; row < 2; ++row) {
        for (int col = 0; col < 3; ++col) {
            const int index = row * 3 + col;
            layout.overviewCards[index] = MakeBox(
                layout.content.x + col * (cardWidth + kGap),
                cardsTop + row * (cardHeight + kGap),
                cardWidth,
                cardHeight
            );
        }
    }

    const int actionsY =
        layout.content.y + layout.content.height - actionAreaHeight;

    if (singleActionRow) {
        const int actionWidth =
            (layout.content.width - (4 * kGap)) / 5;

        for (int i = 0; i < 5; ++i) {
            layout.overviewActions[i] = MakeBox(
                layout.content.x + i * (actionWidth + kGap),
                actionsY,
                actionWidth,
                actionHeight
            );
        }
    } else {
        const int actionWidth =
            (layout.content.width - (2 * kGap)) / 3;

        for (int i = 0; i < 3; ++i) {
            layout.overviewActions[i] = MakeBox(
                layout.content.x + i * (actionWidth + kGap),
                actionsY,
                actionWidth,
                actionHeight
            );
        }

        for (int i = 3; i < 5; ++i) {
            const int col = i - 3;
            layout.overviewActions[i] = MakeBox(
                layout.content.x + col * (actionWidth + kGap),
                actionsY + actionHeight + kGap,
                actionWidth,
                actionHeight
            );
        }
    }

    return layout;
}

}
