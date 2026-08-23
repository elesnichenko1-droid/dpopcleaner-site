#include "ui/ShellModel.h"

#include <array>

namespace dpop::ui {
namespace {

constexpr std::array<TabDescriptor, 10> kPrimaryTabs{{
    {Page::Overview,      1000, L"Обзор"},
    {Page::Cleaning,      1001, L"Очистка"},
    {Page::Memory,        1002, L"ОЗУ"},
    {Page::Guard,         1003, L"DPopGuard"},
    {Page::Disk,          1004, L"Диск"},
    {Page::Applications,  1005, L"Приложения"},
    {Page::WindowsUpdate, 1006, L"Windows"},
    {Page::Duplicates,    1007, L"Дубликаты"},
    {Page::Tools,         1008, L"Инструменты"},
    {Page::Zapret,        1009, L"Zapret"},
}};

}

std::span<const TabDescriptor> PrimaryTabs() noexcept {
    return kPrimaryTabs;
}

bool IsPrimaryTab(Page page) noexcept {
    for (const auto& tab : kPrimaryTabs) {
        if (tab.page == page) {
            return true;
        }
    }
    return false;
}

Page PageForCommand(int commandId) noexcept {
    for (const auto& tab : kPrimaryTabs) {
        if (tab.commandId == commandId) {
            return tab.page;
        }
    }
    return Page::Overview;
}

}
