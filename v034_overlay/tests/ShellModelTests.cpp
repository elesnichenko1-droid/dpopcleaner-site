#include "ui/ShellModel.h"

#include <iostream>
#include <string_view>

namespace {
bool Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "ShellModelTests FAILED: " << message << '\n';
        return false;
    }
    return true;
}
}

int main() {
    using namespace dpop::ui;
    const auto tabs = PrimaryTabs();
    if (!Check(tabs.size() == 13, "0.3.4 must expose thirteen sidebar sections")) return 1;

    const std::wstring_view expectedLabels[] = {
        L"Обзор", L"Очистка", L"ОЗУ", L"DPopGuard", L"Автозагрузка", L"Диск",
        L"Приложения", L"Windows", L"Дубликаты", L"Инструменты", L"Zapret Center",
        L"Обновления", L"Настройки"
    };
    const Page expectedPages[] = {
        Page::Overview, Page::Cleaning, Page::Memory, Page::Guard, Page::Startup, Page::Disk,
        Page::Applications, Page::WindowsUpdate, Page::Duplicates, Page::Tools, Page::Zapret,
        Page::Updates, Page::Settings
    };

    for (std::size_t i = 0; i < tabs.size(); ++i) {
        if (!Check(tabs[i].label == expectedLabels[i], "sidebar label/order mismatch")) return 2;
        if (!Check(tabs[i].page == expectedPages[i], "sidebar page/order mismatch")) return 3;
        if (!Check(tabs[i].commandId == 1000 + static_cast<int>(i), "command IDs must be contiguous")) return 4;
        if (!Check(IsPrimaryTab(tabs[i].page), "every section must be a primary sidebar item")) return 5;
        if (!Check(PageForCommand(tabs[i].commandId) == tabs[i].page, "command must map back to page")) return 6;
    }
    if (!Check(PageForCommand(7777) == Page::Overview, "unknown command must fail safe to Overview")) return 7;
    std::cout << "ShellModelTests PASS\n";
    return 0;
}
