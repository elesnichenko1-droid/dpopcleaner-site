#include "ui/StatusBar.h"

#include <iostream>

int main() {
    using namespace dpop::ui;

    if (kSupportCommandId != 1200) {
        std::cerr << "StatusBarCompileTests FAILED: support command ID mismatch\n";
        return 1;
    }

    SessionLog log;
    StatusBar bar;

    if (bar.SupportButton() != nullptr) {
        std::cerr << "StatusBarCompileTests FAILED: support handle must start null\n";
        return 2;
    }

    if (bar.LogControl() != nullptr) {
        std::cerr << "StatusBarCompileTests FAILED: log handle must start null\n";
        return 3;
    }

    // These must be safe before Create().
    bar.SetStatus(L"Готово.");
    bar.AppendLog(L"Test", EventLevel::Info, L"ignored before Create");

    if (!log.Events().empty()) {
        std::cerr << "StatusBarCompileTests FAILED: pre-Create AppendLog must not mutate session log\n";
        return 4;
    }

    std::cout << "StatusBarCompileTests PASS\n";
    return 0;
}
