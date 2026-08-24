#include "ui/Layout.h"

#include <algorithm>

namespace dpop::ui {
namespace {
constexpr int kMinClientWidth = 1100;
constexpr int kMinClientHeight = 700;
constexpr int kSidebarWidth = 210;
constexpr int kOuterMargin = 18;
constexpr int kFooterHeight = 110;
constexpr int kGap = 14;
constexpr int kNavTop = 104;
constexpr int kNavButtonHeight = 32;
constexpr int kNavGap = 5;

Box MakeBox(int x, int y, int width, int height) noexcept {
    return {x, y, std::max(0, width), std::max(0, height)};
}
}

ShellLayout ComputeShellLayout(int clientWidth, int clientHeight) noexcept {
    const int w = std::max(kMinClientWidth, clientWidth);
    const int h = std::max(kMinClientHeight, clientHeight);

    ShellLayout layout{};
    layout.sidebar = MakeBox(0, 0, kSidebarWidth, h);

    const int navX = 14;
    const int navWidth = kSidebarWidth - 28;
    const int navHeight = static_cast<int>(layout.navButtons.size()) * kNavButtonHeight +
                          (static_cast<int>(layout.navButtons.size()) - 1) * kNavGap;
    layout.navigation = MakeBox(navX, kNavTop, navWidth, navHeight);
    for (std::size_t i = 0; i < layout.navButtons.size(); ++i) {
        layout.navButtons[i] = MakeBox(
            navX,
            kNavTop + static_cast<int>(i) * (kNavButtonHeight + kNavGap),
            navWidth,
            kNavButtonHeight);
    }

    const int rightX = kSidebarWidth;
    const int footerY = h - kFooterHeight;
    layout.footer = MakeBox(rightX, footerY, w - rightX, kFooterHeight);

    layout.content = MakeBox(
        rightX + kOuterMargin,
        kOuterMargin,
        w - rightX - kOuterMargin * 2,
        footerY - kOuterMargin * 2);

    layout.status = MakeBox(
        rightX + kOuterMargin,
        footerY + 6,
        w - rightX - kOuterMargin * 2,
        20);
    layout.log = MakeBox(
        rightX + kOuterMargin,
        footerY + 28,
        w - rightX - kOuterMargin * 2,
        48);
    layout.support = MakeBox(
        rightX + kOuterMargin,
        footerY + 80,
        110,
        24);
    layout.version = MakeBox(
        w - kOuterMargin - 150,
        footerY + 80,
        150,
        24);

    const bool singleActionRow = layout.content.width >= 1132;
    const int actionRows = singleActionRow ? 1 : 2;
    const int actionHeight = 40;
    const int actionAreaHeight = actionRows * actionHeight + (actionRows - 1) * kGap;
    const int cardsAreaHeight = layout.content.height - actionAreaHeight - kGap;
    const int cardWidth = (layout.content.width - 2 * kGap) / 3;
    const int cardHeight = (cardsAreaHeight - kGap) / 2;

    for (int row = 0; row < 2; ++row) {
        for (int col = 0; col < 3; ++col) {
            const int index = row * 3 + col;
            layout.overviewCards[static_cast<std::size_t>(index)] = MakeBox(
                layout.content.x + col * (cardWidth + kGap),
                layout.content.y + row * (cardHeight + kGap),
                cardWidth,
                cardHeight);
        }
    }

    const int actionsY = layout.content.y + layout.content.height - actionAreaHeight;
    if (singleActionRow) {
        const int actionWidth = (layout.content.width - 4 * kGap) / 5;
        for (int i = 0; i < 5; ++i) {
            layout.overviewActions[static_cast<std::size_t>(i)] = MakeBox(
                layout.content.x + i * (actionWidth + kGap), actionsY,
                actionWidth, actionHeight);
        }
    } else {
        const int actionWidth = (layout.content.width - 2 * kGap) / 3;
        for (int i = 0; i < 3; ++i) {
            layout.overviewActions[static_cast<std::size_t>(i)] = MakeBox(
                layout.content.x + i * (actionWidth + kGap), actionsY,
                actionWidth, actionHeight);
        }
        for (int i = 3; i < 5; ++i) {
            const int col = i - 3;
            layout.overviewActions[static_cast<std::size_t>(i)] = MakeBox(
                layout.content.x + col * (actionWidth + kGap),
                actionsY + actionHeight + kGap,
                actionWidth, actionHeight);
        }
    }

    return layout;
}

}
