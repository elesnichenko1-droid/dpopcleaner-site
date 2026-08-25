#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace dpop::disk {

using DiskNodeId = std::uint64_t;

enum class DiskScanState {
    Pending,
    Scanning,
    Complete,
    Incomplete
};

struct DiskNode {
    DiskNodeId id{};
    DiskNodeId parentId{};
    std::filesystem::path path;
    std::wstring displayName;
    bool directory{};
    std::uint64_t logicalBytes{};
    std::uint64_t allocatedBytes{};
    bool allocatedComplete{true};
    std::uint64_t fileCount{};
    std::uint64_t directoryCount{};
    std::int64_t modifiedUnix100ns{};
    bool incomplete{};
    bool protectedPath{};
    DiskScanState state{DiskScanState::Pending};
    std::vector<DiskNodeId> children;
};

struct DiskScanProgress {
    std::filesystem::path currentPath;
    std::uint64_t filesVisited{};
    std::uint64_t directoriesVisited{};
    std::uint64_t logicalBytes{};
    std::uint64_t errors{};
};

struct DiskScanSnapshot {
    DiskNodeId rootId{};
    std::vector<DiskNode> nodes;
    std::uint64_t errorCount{};
    bool complete{};
    bool cancelled{};

    const DiskNode* Find(DiskNodeId id) const noexcept;
    const DiskNode* FindPath(const std::filesystem::path& path) const noexcept;
};

using DiskProgressCallback = std::function<void(const DiskScanProgress&)>;
using DiskPartialCallback = std::function<void(const DiskScanSnapshot&)>;
using DiskAllocatedSizeProvider = std::function<std::optional<std::uint64_t>(
    const std::filesystem::path&,
    std::uint64_t logicalBytes)>;

struct DiskScanOptions {
    std::size_t progressEveryEntries{128};
    bool includeFilesAsNodes{true};
    bool emitTopLevelSnapshots{true};
    DiskPartialCallback partialSnapshot;
    DiskAllocatedSizeProvider allocatedSizeProvider;
};

DiskScanSnapshot ScanDiskTree(
    const std::filesystem::path& root,
    std::stop_token stop = {},
    DiskProgressCallback progress = {},
    DiskScanOptions options = {}
);

double ParentPercent(const DiskNode& node, const DiskNode* parent) noexcept;

} // namespace dpop::disk
