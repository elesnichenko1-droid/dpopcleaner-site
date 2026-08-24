#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace dpop::settings {

enum class CloseBehavior {
    Exit = 0,
    MinimizeToTray = 1,
    Ask = 2
};

enum class MemoryScope {
    Safe = 0,
    Advanced = 1
};

struct AppSettings {
    unsigned schemaVersion{2};
    bool confirmDestructive{true};
    unsigned largeFileMB{500};
    unsigned duplicateMinMB{10};
    bool runAtStartup{false};
    bool alwaysRunAsAdmin{false};
    bool checkUpdatesAtStartup{true};
    bool quickGuardAtStartup{false};
    bool checkUpdateCacheAtStartup{false};
    bool backgroundJunkMonitor{false};
    bool trayEnabled{true};
    CloseBehavior closeBehavior{CloseBehavior::MinimizeToTray};
    bool memoryAutoTrimEnabled{false};
    unsigned memoryAutoTrimPercent{80};
    unsigned memoryAutoTrimIntervalMinutes{15};
    MemoryScope memoryScope{MemoryScope::Safe};
    std::vector<std::wstring> cleanExclusions;

    bool operator==(const AppSettings&) const = default;
};

struct SettingsLoadResult {
    AppSettings settings;
    bool usedDefaults{};
    bool migrated{};
    std::wstring warning;
};

AppSettings DefaultSettings() noexcept;
std::filesystem::path SettingsPath();
SettingsLoadResult LoadAppSettings();
bool SaveAppSettings(const AppSettings& settings, std::wstring& error);
bool ValidateSettings(const AppSettings& settings, std::wstring& error) noexcept;
std::wstring NormalizeExclusionPath(const std::filesystem::path& path);
bool IsExcludedPath(const std::filesystem::path& path, const AppSettings& settings);

} // namespace dpop::settings
