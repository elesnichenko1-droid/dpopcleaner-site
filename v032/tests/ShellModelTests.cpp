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
    if (!Check(tabs.size() == 10, "PrimaryTabs must contain exactly 10 tabs")) return 1;

    const std::wstring_view expectedLabels[] = {
        L"Обзор", L"Очистка", L"ОЗУ", L"DPopGuard", L"Диск",
        L"Приложения", L"Windows", L"Дубликаты", L"Инструменты", L"Zapret"
    };
    const Page expectedPages[] = {
        Page::Overview, Page::Cleaning, Page::Memory, Page::Guard, Page::Disk,
        Page::Applications, Page::WindowsUpdate, Page::Duplicates, Page::Tools, Page::Zapret
    };

    for (std::size_t i = 0; i < tabs.size(); ++i) {
        if (!Check(tabs[i].label == expectedLabels[i], "tab label/order mismatch")) return 2;
        if (!Check(tabs[i].page == expectedPages[i], "tab page/order mismatch")) return 3;
        if (!Check(tabs[i].commandId == 1000 + static_cast<int>(i), "command IDs must be 1000..1009")) return 4;
        if (!Check(IsPrimaryTab(tabs[i].page), "primary tab must be recognized")) return 5;
        if (!Check(PageForCommand(tabs[i].commandId) == tabs[i].page, "command must map back to page")) return 6;
    }

    if (!Check(!IsPrimaryTab(Page::Settings), "Settings must not be a primary tab")) return 7;
    if (!Check(PageForCommand(1100) == Page::Overview, "unknown command must safely map to Overview")) return 8;

    std::cout << "ShellModelTests PASS\n";
    return 0;
}
