#include "ui/ShellModel.h"

#include <array>

namespace dpop::ui {
namespace {

constexpr std::array<TabDescriptor, 13> kPrimaryTabs{{
    {Page::Overview,      1000, L"Обзор"},
    {Page::Cleaning,      1001, L"Очистка"},
    {Page::Memory,        1002, L"ОЗУ"},
    {Page::Guard,         1003, L"DPopGuard"},
    {Page::Startup,       1004, L"Автозагрузка"},
    {Page::Disk,          1005, L"Диск"},
    {Page::Applications,  1006, L"Приложения"},
    {Page::WindowsUpdate, 1007, L"Windows"},
    {Page::Duplicates,    1008, L"Дубликаты"},
    {Page::Tools,         1009, L"Инструменты"},
    {Page::Zapret,        1010, L"Zapret Center"},
    {Page::Updates,       1011, L"Обновления"},
    {Page::Settings,      1012, L"Настройки"},
}};

}

std::span<const TabDescriptor> PrimaryTabs() noexcept {
    return kPrimaryTabs;
}

bool IsPrimaryTab(Page page) noexcept {
    for (const auto& tab : kPrimaryTabs) {
        if (tab.page == page) return true;
    }
    return false;
}

Page PageForCommand(int commandId) noexcept {
    for (const auto& tab : kPrimaryTabs) {
        if (tab.commandId == commandId) return tab.page;
    }
    return Page::Overview;
}

}
