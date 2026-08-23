#pragma once
#include <array>

namespace dpop::ui {

struct Box {
    int x{};
    int y{};
    int width{};
    int height{};
};

struct ShellLayout {
    Box header;
    Box tabs;
    Box content;
    Box footer;
    Box status;
    Box log;
    Box support;
    Box version;
    std::array<Box, 6> overviewCards{};
    std::array<Box, 5> overviewActions{};
};

ShellLayout ComputeShellLayout(int clientWidth, int clientHeight) noexcept;

}
