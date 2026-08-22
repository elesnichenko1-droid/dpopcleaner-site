#include "modules/ZapretPolicy.h"

#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace {
int failures = 0;

void Expect(bool condition, const char* name) {
    if (!condition) {
        std::cerr << "FAIL: " << name << '\n';
        ++failures;
    } else {
        std::cout << "PASS: " << name << '\n';
    }
}

void Touch(const fs::path& path) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << "fixture";
}
}

int main() {
    const fs::path temp = fs::temp_directory_path() /
        (L"dpop-zapret-policy-" + std::to_wstring(GetCurrentProcessId()));
    std::error_code ec;
    fs::remove_all(temp, ec);

    const fs::path executableDirectory = temp / L"DPopCleaner";
    const fs::path bundle = dpop::zapret::ResolveBundledRoot(executableDirectory);
    Expect(bundle == executableDirectory / L"zapret",
           "bundle root is fixed under the executable directory");

    for (const auto& relative : dpop::zapret::RequiredBundleFiles()) {
        Touch(bundle / relative);
    }

    const auto valid = dpop::zapret::ValidateBundle(bundle);
    Expect(valid.valid && valid.missing.empty(),
           "complete pinned bundle passes required-file validation");
    Expect(dpop::zapret::IsBundledWinwsPath(bundle, bundle / L"bin" / L"winws.exe"),
           "exact bundled winws path is accepted");
    Expect(!dpop::zapret::IsBundledWinwsPath(bundle, temp / L"foreign" / L"winws.exe"),
           "foreign winws path is rejected");
    Expect(!dpop::zapret::IsBundledWinwsPath(bundle, bundle / L".." / L"foreign" / L"winws.exe"),
           "path traversal cannot claim a foreign winws process");

    fs::remove(bundle / L"service.bat", ec);
    const auto missing = dpop::zapret::ValidateBundle(bundle);
    Expect(!missing.valid && missing.missing == fs::path(L"service.bat"),
           "missing service manager fails with the exact relative path");

    fs::remove_all(temp, ec);
    if (failures != 0) {
        std::cerr << failures << " Zapret policy test(s) failed.\n";
        return 1;
    }
    std::cout << "All Zapret policy tests passed.\n";
    return 0;
}
