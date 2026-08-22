#pragma once
#include <windows.h>
#include <filesystem>
#include <string>
#include <vector>

namespace dpop::apps {

struct InstalledApp {
    std::wstring displayName;
    std::wstring displayVersion;
    std::wstring publisher;
    std::wstring installLocation;
    std::wstring uninstallString;
    std::wstring quietUninstallString;
    std::wstring displayIcon;
    std::wstring registryPath;
    HKEY registryRoot{};
    REGSAM registryView{};
    bool windowsInstaller{};
};

struct LeftoverItem {
    std::filesystem::path path;
    std::wstring reason;
    bool highConfidence{};
};

std::vector<InstalledApp> EnumerateInstalledApps();
bool RunUninstaller(const InstalledApp& app, DWORD& exitCode, std::wstring& error);
std::vector<LeftoverItem> FindLeftovers(const InstalledApp& app);
bool MoveLeftoversToRecycleBin(const std::vector<LeftoverItem>& items, std::size_t& removed, std::wstring& error);

}
