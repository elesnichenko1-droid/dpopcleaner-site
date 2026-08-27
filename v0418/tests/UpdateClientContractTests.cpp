#include "UpdateClient.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {
int Fail(const char* message) {
    std::cerr << message << "\n";
    return 1;
}

std::string ReadAll(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream out;
    out << input.rdbuf();
    return out.str();
}
}

int main() {
    if (std::wstring(dpop0418::kStableManifestUrl) !=
        L"https://elesnichenko1-droid.github.io/dpopcleaner-site/update/stable.json")
        return Fail("stable manifest URL must be exact");

    dpop0418::UpdateManifest manifest{};
    manifest.sha256 = L"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    manifest.installArgs = L"/SILENT /NORESTART";
    manifest.signedPackage = false;

    const std::wstring args = dpop0418::BuildUpdaterArguments(
        manifest,
        std::filesystem::path(L"C:\\Temp\\DPopCleaner Setup.exe"),
        true,
        std::filesystem::path(L"C:\\Program Files\\DPopCleaner\\DPopCleaner.exe"),
        1234);
    if (args.find(L"--parent 1234") == std::wstring::npos)
        return Fail("updater args must contain parent PID");
    if (args.find(L"--package \"C:\\Temp\\DPopCleaner Setup.exe\"") == std::wstring::npos)
        return Fail("package path must be quoted");
    if (args.find(L"--sha256 \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"") == std::wstring::npos)
        return Fail("updater args must carry expected SHA-256");
    if (args.find(L"--allow-unsigned") == std::wstring::npos)
        return Fail("explicit unsigned permission must be forwarded only when approved");
    if (args.find(L"--restart \"C:\\Program Files\\DPopCleaner\\DPopCleaner.exe\"") == std::wstring::npos)
        return Fail("restart path must be quoted");

    const auto fixtureRoot = std::filesystem::temp_directory_path() / L"dpop0418-updater-handoff-test";
    std::error_code ec;
    std::filesystem::remove_all(fixtureRoot, ec);
    std::filesystem::create_directories(fixtureRoot, ec);
    const auto installedUpdater = fixtureRoot / L"DPopUpdater.exe";
    {
        std::ofstream out(installedUpdater, std::ios::binary | std::ios::trunc);
        out << "dpop0418-updater-fixture";
    }
    std::filesystem::path stagedUpdater;
    std::wstring stageError;
    if (!dpop0418::StageUpdaterForHandoff(installedUpdater, stagedUpdater, stageError))
        return Fail("updater handoff copy must be created");
    if (stagedUpdater == installedUpdater)
        return Fail("handoff updater must not execute from the installed path");
    if (!std::filesystem::exists(stagedUpdater))
        return Fail("handoff updater copy must exist");
    if (ReadAll(stagedUpdater) != ReadAll(installedUpdater))
        return Fail("handoff updater copy must preserve exact bytes");
    if (stagedUpdater.filename().wstring().find(L"DPopUpdater-handoff-") != 0)
        return Fail("handoff updater must use an identifiable per-process copy name");
    std::filesystem::remove(stagedUpdater, ec);
    std::filesystem::remove_all(fixtureRoot, ec);

    const std::filesystem::path sourceRoot = std::filesystem::path(DPOP0418_SOURCE_DIR);
    const std::string updaterSource = ReadAll(sourceRoot / "updater" / "UpdaterMain.cpp");
    const auto hashPos = updaterSource.find("Sha256File");
    const auto launchPos = updaterSource.find("ShellExecuteExW");
    if (hashPos == std::string::npos || launchPos == std::string::npos || hashPos > launchPos)
        return Fail("DPopUpdater must independently hash package before installer launch");

    const std::string clientSource = ReadAll(sourceRoot / "core" / "UpdateClient.cpp");
    const auto stagePos = clientSource.find("StageUpdaterForHandoff(updaterExe");
    const auto shellPos = clientSource.find("execute.lpFile = stagedUpdater.c_str()");
    if (stagePos == std::string::npos || shellPos == std::string::npos || stagePos > shellPos)
        return Fail("LaunchUpdater must execute the staged handoff copy, not installed DPopUpdater.exe");

    return 0;
}
