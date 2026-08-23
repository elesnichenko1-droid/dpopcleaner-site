#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

#include "modules/Applications.h"

namespace dpop::full {

enum class CleanKind {
    UserTemp, WindowsTemp, CrashDumps, Wer, Thumbnails, DirectX,
    Browsers, Discord, Steam, Epic, Nvidia, Amd, RecycleBin
};

struct CleanItem {
    CleanKind kind{};
    std::wstring label;
    std::uint64_t bytes{};
    bool recommended{};
    bool requiresAdmin{};
};

struct CleanSummary {
    std::uint64_t removedBytes{};
    unsigned removedFiles{};
    unsigned failedFiles{};
    std::wstring error;
};

std::vector<CleanItem> AnalyzeCleaning(std::stop_token stop = {});
CleanSummary CleanSelected(const std::vector<CleanKind>& kinds, std::stop_token stop = {});
std::wstring_view CleanKindLabel(CleanKind kind) noexcept;

struct MemoryStats {
    std::uint64_t totalPhysical{};
    std::uint64_t availablePhysical{};
    std::uint64_t usedPhysical{};
    unsigned usedPercent{};
    unsigned processCount{};
};

struct MemoryTrimResult {
    unsigned attempted{};
    unsigned trimmed{};
    unsigned failed{};
};

unsigned Percent(std::uint64_t used, std::uint64_t total) noexcept;
MemoryStats QueryMemoryStats();
MemoryTrimResult TrimWorkingSets(bool aggressive, std::stop_token stop = {});

struct FileItem {
    std::filesystem::path path;
    std::uint64_t size{};
};

std::vector<FileItem> ScanLargeFiles(
    const std::filesystem::path& root,
    std::uint64_t minBytes,
    std::stop_token stop = {},
    std::size_t maxResults = 500
);

struct DuplicateFile {
    unsigned group{};
    std::uint64_t size{};
    std::wstring sha256;
    std::filesystem::path path;
};

std::vector<DuplicateFile> FindDuplicates(
    const std::filesystem::path& root,
    std::uint64_t minBytes,
    std::stop_token stop = {}
);

struct RecycleResult {
    std::size_t moved{};
    std::size_t failed{};
    std::wstring error;
};

RecycleResult MoveToRecycleBin(const std::vector<std::filesystem::path>& paths);
bool LaunchUninstaller(const dpop::apps::InstalledApp& app, std::wstring& error);

enum class MaintenanceAction {
    ClearUpdateCache,
    ComponentCleanup,
    ResetBase,
    SfcScan,
    DismCheckHealth,
    DismScanHealth,
    DismRestoreHealth,
    ChkdskScan
};

std::wstring_view MaintenanceLabel(MaintenanceAction action) noexcept;
bool RunMaintenance(MaintenanceAction action, unsigned long& exitCode, std::wstring& error, std::stop_token stop = {});

enum class ToolAction {
    TaskManager,
    EventViewer,
    Startup,
    SystemRestore,
    WindowsSecurity,
    Performance,
    Logs
};

std::wstring_view ToolLabel(ToolAction action) noexcept;
bool OpenTool(ToolAction action, std::wstring& error);

struct GuardHit {
    std::filesystem::path path;
    std::wstring verdict;
};

struct GuardFolderResult {
    unsigned checked{};
    unsigned skipped{};
    unsigned errors{};
    std::vector<GuardHit> hits;
};

bool IsGuardCandidate(const std::filesystem::path& path) noexcept;
GuardFolderResult ScanFolderWithAmsi(
    const std::filesystem::path& root,
    std::stop_token stop = {},
    unsigned maxFiles = 5000
);
bool QuarantineFile(const std::filesystem::path& file, std::filesystem::path& destination, std::wstring& error);

struct Settings {
    bool confirmDestructive{true};
    unsigned largeFileMB{500};
    unsigned duplicateMinMB{10};
    bool runAtStartup{false};
};

std::filesystem::path SettingsPath();
Settings LoadSettings();
bool SaveSettings(const Settings& settings, std::wstring& error);
bool SetRunAtStartup(bool enabled, std::wstring& error);

std::wstring FormatBytes(std::uint64_t bytes);

}
