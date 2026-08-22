#pragma once
#include <filesystem>
#include <string>

namespace dpop::zapret {
struct Status {
    bool serviceInstalled{};
    bool serviceRunning{};
    bool winwsRunning{};
    bool bundleValid{};
    std::filesystem::path bundleFolder;
    std::filesystem::path missingBundleFile;
};
Status QueryStatus();
bool LaunchDefaultStrategy(std::wstring& error);
bool OpenServiceManager(std::wstring& error);
bool OpenBundledFolder(std::wstring& error);
}
