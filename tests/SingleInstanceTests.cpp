#include "core/SingleInstance.h"

#include <windows.h>
#include <iostream>
#include <string>

int main() {
    const std::wstring name = L"Local\\DPopCleaner.R3.Test." + std::to_wstring(GetCurrentProcessId());
    dpop::SingleInstance first(name);
    if (!first.IsPrimary()) {
        std::cerr << "FAIL: the first named mutex owner must be primary.\n";
        return 1;
    }

    dpop::SingleInstance second(name);
    if (second.IsPrimary()) {
        std::cerr << "FAIL: the second named mutex owner must be rejected.\n";
        return 1;
    }

    std::cout << "PASS: named mutex refuses a second application instance.\n";
    return 0;
}
