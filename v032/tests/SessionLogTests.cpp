#include "ui/SessionLog.h"

#include <iostream>

namespace {

bool Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "SessionLogTests FAILED: " << message << '\n';
        return false;
    }
    return true;
}

}

int main() {
    using namespace dpop::ui;

    SessionLog log;
    if (!Check(log.Events().empty(), "new log must be empty")) return 1;

    log.Append(
        L"Shell",
        EventLevel::Info,
        L"DPopCleaner 0.3.2 запущен."
    );
    log.Append(
        L"Overview",
        EventLevel::Warning,
        L"Тест предупреждения."
    );
    log.Append(
        L"Guard",
        EventLevel::Error,
        L"line1\nline2"
    );

    const auto events = log.Events();
    if (!Check(events.size() == 3, "three events expected")) return 2;
    if (!Check(events[0].category == L"Shell", "first category mismatch")) return 3;
    if (!Check(events[0].level == EventLevel::Info, "first level mismatch")) return 4;
    if (!Check(events[1].level == EventLevel::Warning, "warning level mismatch")) return 5;
    if (!Check(events[2].level == EventLevel::Error, "error level mismatch")) return 6;

    const auto rendered = log.RenderText();

    const auto p1 = rendered.find(L"[Shell] [INFO] DPopCleaner 0.3.2 запущен.");
    const auto p2 = rendered.find(L"[Overview] [WARNING] Тест предупреждения.");
    const auto p3 = rendered.find(L"[Guard] [ERROR] line1 line2");

    if (!Check(p1 != std::wstring::npos, "INFO line missing")) return 7;
    if (!Check(p2 != std::wstring::npos, "WARNING line missing")) return 8;
    if (!Check(p3 != std::wstring::npos, "ERROR line or newline sanitization missing")) return 9;
    if (!Check(p1 < p2 && p2 < p3, "render order must preserve append order")) return 10;

    log.Clear();
    if (!Check(log.Events().empty(), "Clear must remove events")) return 11;
    if (!Check(log.RenderText().empty(), "cleared render must be empty")) return 12;

    std::cout << "SessionLogTests PASS\n";
    return 0;
}
