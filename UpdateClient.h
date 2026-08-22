#pragma once
#include "update/UpdateManifest.h"
#include <filesystem>
#include <string>

namespace dpop::update {
struct CheckResult {
    bool success{};
    bool updateAvailable{};
    Manifest manifest{};
    std::wstring error;
};

CheckResult CheckForUpdates();
bool DownloadPackage(const Manifest& manifest, std::filesystem::path& downloadedFile, std::wstring& error);
bool PrepareAndLaunchUpdater(const Manifest& manifest, const std::filesystem::path& package, bool allowUnsigned, std::wstring& error);
}
