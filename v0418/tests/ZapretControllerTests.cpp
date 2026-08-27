#include "ZapretController.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {
int Fail(const char* message) {
    std::cerr << message << "\n";
    return 1;
}

void Touch(const fs::path& path) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << "fixture";
}
}

int main() {
    const fs::path temp = fs::temp_directory_path() / L"dpop0418-zapret-controller-tests";
    std::error_code ec;
    fs::remove_all(temp, ec);
    fs::create_directories(temp, ec);
    if (ec) return Fail("cannot create temp root");

    const fs::path exeDir = temp / L"DPopCleaner";
    const fs::path root = dpop0418::BundledZapretRoot(exeDir);
    if (root != exeDir / L"ThirdParty" / L"Zapret")
        return Fail("bundled root must be <exeDir>/ThirdParty/Zapret");

    std::wstring error;
    if (dpop0418::ValidateBundledPayload(root, error))
        return Fail("empty bundled root must fail validation");

    Touch(root / L"LICENSE.txt");
    Touch(root / L"service.bat");
    Touch(root / L"bin" / L"winws.exe");
    Touch(root / L"bin" / L"WinDivert.dll");
    Touch(root / L"bin" / L"WinDivert64.sys");
    fs::create_directories(root / L"lists", ec);
    if (!dpop0418::ValidateBundledPayload(root, error))
        return Fail("complete required payload fixture must validate");

    Touch(root / L"general.bat");
    Touch(root / L"general (ALT2).bat");
    Touch(root / L"general (ALT13).bat");
    Touch(root / L"service.bat");
    Touch(root / L"nested" / L"general nested.bat");
    Touch(root / L"notes.txt");

    const auto strategies = dpop0418::EnumerateZapretStrategies(root);
    if (strategies.size() != 3)
        return Fail("strategy enumeration must include only top-level non-service .bat files");
    for (const auto& strategy : strategies) {
        if (strategy.batchPath.parent_path() != root)
            return Fail("strategy path must stay directly under bundled root");
        const std::wstring lower = strategy.batchPath.filename().wstring();
        if (lower.rfind(L"service", 0) == 0)
            return Fail("service*.bat must not be exposed as a strategy");
    }

    if (dpop0418::FindStrategyMenuIndex(strategies, L"general.bat") == 0)
        return Fail("known strategy must map to a 1-based upstream menu index");
    if (dpop0418::FindStrategyMenuIndex(strategies, L"missing.bat") != 0)
        return Fail("unknown strategy must not map to an upstream menu index");

    const fs::path bundledWinws = root / L"bin" / L"winws.exe";
    if (!dpop0418::IsBundledWinwsPath(bundledWinws, root))
        return Fail("bundled winws path must be recognized as owned");
    if (!dpop0418::IsBundledWinwsPath(root / L"bin" / L"." / L"winws.exe", root))
        return Fail("normalized bundled winws path must be recognized as owned");
    if (dpop0418::IsBundledWinwsPath(temp / L"external" / L"winws.exe", root))
        return Fail("external winws path must never be treated as bundled");

    fs::remove(root / L"bin" / L"WinDivert64.sys", ec);
    error.clear();
    if (dpop0418::ValidateBundledPayload(root, error))
        return Fail("missing WinDivert driver must fail validation");
    if (error.find(L"WinDivert64.sys") == std::wstring::npos)
        return Fail("payload validation must identify the missing required file");

    fs::remove_all(temp, ec);
    return 0;
}
