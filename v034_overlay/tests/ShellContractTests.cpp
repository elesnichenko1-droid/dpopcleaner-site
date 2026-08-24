#include "ui/Shell.h"
#include "ui/ShellModel.h"
#include "ui/StatusBar.h"
#include <iostream>

namespace {
bool Check(bool c, const char* m) { if (!c) { std::cerr << "ShellContractTests FAILED: " << m << '\n'; return false; } return true; }
}

int main() {
    using namespace dpop::ui;
    using namespace dpop::ui::shell;
    const auto& identity = Identity();
    if (!Check(identity.windowTitle == L"DPopCleaner 0.3.4 BETA R2", "window title mismatch")) return 1;
    if (!Check(identity.productName == L"DPopCleaner", "product name mismatch")) return 2;
    if (!Check(identity.subtitle == L"Windows под контролем", "0.2.14-style subtitle mismatch")) return 3;
    if (!Check(identity.betaLabel == L"BETA", "BETA label mismatch")) return 4;
    if (!Check(kSupportCommandId == 1200, "Support command must remain 1200")) return 5;
    const auto tabs = PrimaryTabs();
    if (!Check(tabs.size() == 13, "shell must expose thirteen sidebar sections")) return 6;
    if (!Check(IsPrimaryTab(Page::Startup), "Startup must be restored as primary section")) return 7;
    if (!Check(IsPrimaryTab(Page::Updates), "Updates must be restored as primary section")) return 8;
    if (!Check(IsPrimaryTab(Page::Settings), "Settings must be a visible sidebar section")) return 9;
    std::cout << "ShellContractTests PASS\n";
    return 0;
}
