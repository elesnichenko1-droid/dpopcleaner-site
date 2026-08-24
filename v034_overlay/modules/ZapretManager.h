#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "modules/ZapretCenterModel.h"

namespace dpop::zapret {

struct Status {
    bool serviceInstalled{};
    bool serviceRunning{};
    bool winwsRunning{};
    bool bundleValid{};
    std::filesystem::path bundleFolder;
    std::filesystem::path missingBundleFile;
};

struct UpdateInfo {
    bool success{};
    bool updateAvailable{};
    std::wstring currentVersion;
    std::wstring latestVersion;
    std::wstring downloadUrl;
    std::wstring sha256;
    std::uint64_t size{};
    std::wstring error;
};

Status QueryStatus();
std::vector<StrategyEntry> EnumerateStrategies();
bool LaunchStrategy(const std::filesystem::path& relativeScript, std::wstring& error);
bool LaunchDefaultStrategy(std::wstring& error);
bool StopBundledWinws(std::wstring& error);
bool OpenServiceManager(std::wstring& error);
bool OpenBundledFolder(std::wstring& error);
bool RepairRtc(const std::filesystem::path& relativeScript, std::wstring& report);
UpdateInfo CheckForZapretUpdate();
bool UpdateBundledZapret(std::wstring& report);
bool OpenZapretUpdatePage(std::wstring& error);

}
