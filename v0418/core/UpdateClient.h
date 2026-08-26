#pragma once

#include "UpdateManifest.h"

#include <atomic>
#include <filesystem>
#include <string>

namespace dpop0418 {

inline constexpr wchar_t kStableManifestUrl[] =
    L"https://elesnichenko1-droid.github.io/dpopcleaner-site/update/stable.json";

struct UpdateCheckResult {
    bool success{};
    bool updateAvailable{};
    UpdateManifest manifest{};
    std::wstring error;
};

UpdateCheckResult CheckStableUpdates(const std::atomic_bool* shutdown = nullptr);
bool DownloadVerifiedPackage(const UpdateManifest& manifest,
                             std::filesystem::path& package,
                             std::wstring& error,
                             const std::atomic_bool* shutdown = nullptr);
std::wstring BuildUpdaterArguments(const UpdateManifest& manifest,
                                   const std::filesystem::path& package,
                                   bool allowUnsigned,
                                   const std::filesystem::path& restartExe,
                                   unsigned long parentPid);
bool LaunchUpdater(const UpdateManifest& manifest,
                   const std::filesystem::path& package,
                   bool allowUnsigned,
                   const std::filesystem::path& updaterExe,
                   const std::filesystem::path& restartExe,
                   std::wstring& error);
std::filesystem::path AppDataDirectory();
std::filesystem::path UpdatesDirectory();

} // namespace dpop0418
