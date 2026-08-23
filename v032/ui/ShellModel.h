#pragma once
#include <span>
#include <string_view>

namespace dpop::ui {

enum class Page {
    Overview,
    Cleaning,
    Memory,
    Guard,
    Disk,
    Applications,
    WindowsUpdate,
    Duplicates,
    Tools,
    Zapret,
    Settings
};

struct TabDescriptor {
    Page page;
    int commandId;
    std::wstring_view label;
};

std::span<const TabDescriptor> PrimaryTabs() noexcept;
bool IsPrimaryTab(Page page) noexcept;
Page PageForCommand(int commandId) noexcept;

}
