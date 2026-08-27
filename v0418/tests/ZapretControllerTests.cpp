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

void Write(const fs::path& path, const std::string& text = "fixture") {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
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

    Write(root / L"LICENSE.txt");
    Write(root / L"service.bat");
    Write(root / L"general.bat");
    Write(root / L".service" / L"version.txt", "1.10.2\n");
    Write(root / L"bin" / L"winws.exe");
    Write(root / L"bin" / L"WinDivert.dll");
    Write(root / L"bin" / L"WinDivert64.sys");
    fs::create_directories(root / L"lists", ec);
    if (!dpop0418::ValidateBundledPayload(root, error))
        return Fail("complete required payload fixture must validate");

    Write(root / L"general (ALT2).bat");
    Write(root / L"general (ALT13).bat");
    Write(root / L"nested" / L"general nested.bat");
    Write(root / L"notes.txt");

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

    dpop0418::ZapretStrategy resolved{};
    if (!dpop0418::ResolveBundledStrategy(root, L"general (ALT13).bat", resolved, error))
        return Fail("known top-level strategy must resolve");
    if (resolved.batchPath.filename() != L"general (ALT13).bat")
        return Fail("resolved strategy must retain exact bundled filename");
    if (dpop0418::ResolveBundledStrategy(root, L"..\\general.bat", resolved, error))
        return Fail("strategy path traversal must be refused");

    const fs::path bundledWinws = root / L"bin" / L"winws.exe";
    if (!dpop0418::IsBundledWinwsPath(bundledWinws, root))
        return Fail("bundled winws path must be recognized as owned");
    if (!dpop0418::IsBundledWinwsPath(root / L"bin" / L"." / L"winws.exe", root))
        return Fail("normalized bundled winws path must be recognized as owned");
    if (dpop0418::IsBundledWinwsPath(temp / L"external" / L"winws.exe", root))
        return Fail("external winws path must never be treated as bundled");

    const std::wstring ownedCommand = L"\"" + bundledWinws.wstring() + L"\" --wf-tcp=80,443 --filter-tcp=443";
    if (!dpop0418::IsOwnedZapretServiceCommand(ownedCommand, root))
        return Fail("service command pointing at bundled winws must be owned");
    const std::wstring externalCommand = L"\"" + (temp / L"external" / L"winws.exe").wstring() + L"\" --wf-tcp=443";
    if (dpop0418::IsOwnedZapretServiceCommand(externalCommand, root))
        return Fail("external service command must never be treated as owned");

    const std::wstring installInput = dpop0418::BuildPinnedServiceInstallInput(strategies, L"general (ALT13).bat");
    const size_t menuIndex = dpop0418::FindStrategyMenuIndex(strategies, L"general (ALT13).bat");
    if (menuIndex == 0 || installInput.find(L"1\r\n" + std::to_wstring(menuIndex) + L"\r\n") != 0)
        return Fail("pinned service input must select Install Service and exact strategy index");
    if (dpop0418::BuildPinnedServiceInstallInput(strategies, L"missing.bat").size() != 0)
        return Fail("service input must not be generated for an unknown strategy");

    Write(root / L".service" / L"version.txt", "9.9.9\n");
    error.clear();
    if (dpop0418::ValidateBundledPayload(root, error))
        return Fail("wrong pinned Flowseal version must fail validation");
    if (error.find(L"1.10.2") == std::wstring::npos)
        return Fail("version validation error must name the required pinned version");

    Write(root / L".service" / L"version.txt", "1.10.2\n");
    fs::remove(root / L"bin" / L"WinDivert64.sys", ec);
    error.clear();
    if (dpop0418::ValidateBundledPayload(root, error))
        return Fail("missing WinDivert driver must fail validation");
    if (error.find(L"WinDivert64.sys") == std::wstring::npos)
        return Fail("payload validation must identify the missing required file");

    fs::remove_all(temp, ec);
    return 0;
}
