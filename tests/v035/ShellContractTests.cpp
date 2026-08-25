#include "ui/Shell.h"
#include "ui/ShellModel.h"
#include "ui/StatusBar.h"

#include <iostream>

namespace {

bool Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "ShellContractTests FAILED: " << message << '\n';
        return false;
    }
    return true;
}

}

int main() {
    using namespace dpop::ui;
    using namespace dpop::ui::shell;

    const auto& identity = Identity();

    if (!Check(identity.windowTitle == L"DPopCleaner 0.3.5 BETA R1",
               "window title mismatch")) return 1;
    if (!Check(identity.productName == L"DPopCleaner",
               "product name mismatch")) return 2;
    if (!Check(identity.subtitle ==
               L"Очистка • память • защита • диски • Windows",
               "subtitle mismatch")) return 3;
    if (!Check(identity.betaLabel == L"BETA",
               "BETA label mismatch")) return 4;
    if (!Check(kSettingsCommandId == 1100,
               "Settings gear command must remain 1100")) return 5;
    if (!Check(kSupportCommandId == 1200,
               "Support command must remain 1200")) return 6;

    const auto tabs = PrimaryTabs();
    if (!Check(tabs.size() == 10,
               "recovered shell must expose exactly ten primary tabs")) return 7;
    if (!Check(!IsPrimaryTab(Page::Settings),
               "Settings must remain behind the gear, not a primary tab")) return 8;

    std::cout << "ShellContractTests PASS\n";
    return 0;
}
