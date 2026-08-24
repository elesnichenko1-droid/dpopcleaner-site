#include "modules/DiskAnalyzer.h"

#include <algorithm>
#include <chrono>
#include <cwctype>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#endif

namespace dpop::disk {
namespace {
namespace fs = std::filesystem;

std::wstring NormalizePath(fs::path path) {
    std::error_code ec;
    const auto canonical = fs::weakly_canonical(path, ec);
    std::wstring value = (ec ? path.lexically_normal() : canonical).wstring();
    std::replace(value.begin(), value.end(), L'/', L'\\');
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(std::towlower(c));
    });
    while (value.size() > 3 && !value.empty() && value.back() == L'\\') value.pop_back();
    return value;
}

std::int64_t Modified100ns(const fs::path& path) noexcept {
    std::error_code ec;
    const auto stamp = fs::last_write_time(path, ec);
    if (ec) return 0;
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(stamp.time_since_epoch()).count();
    return static_cast<std::int64_t>(ns / 100);
}

bool IsReparsePoint(const fs::path& path) noexcept {
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
    std::error_code ec;
    return fs::is_symlink(fs::symlink_status(path, ec));
#endif
}

bool ProtectedPath(const fs::path& path) noexcept {
#ifdef _WIN32
    wchar_t windows[MAX_PATH]{};
    const UINT count = GetWindowsDirectoryW(windows, static_cast<UINT>(std::size(windows)));
    if (!count || count >= std::size(windows)) return false;
    const auto p = NormalizePath(path);
    const auto w = NormalizePath(fs::path(windows));
    return p == w || (p.size() > w.size() && p.rfind(w + L"\\", 0) == 0);
#else
    (void)path;
    return false;
#endif
}

std::uint64_t AllocatedFileBytes(const fs::path& path, std::uint64_t logical) noexcept {
#ifdef _WIN32
    DWORD high = 0;
    SetLastError(NO_ERROR);
    const DWORD low = GetCompressedFileSizeW(path.c_str(), &high);
    if (low == INVALID_FILE_SIZE && GetLastError() != NO_ERROR) return logical;
    return (static_cast<std::uint64_t>(high) << 32) | low;
#else
    return logical;
#endif
}

struct ScanContext {
    DiskScanSnapshot snapshot;
    std::stop_token stop;
    DiskProgressCallback callback;
    DiskScanOptions options;
    DiskScanProgress progress;
    DiskNodeId nextId{1};
    std::size_t entriesSinceProgress{};

    void Notify(const fs::path& path, bool force = false) {
        if (!callback) return;
        ++entriesSinceProgress;
        const auto every = std::max<std::size_t>(1, options.progressEveryEntries);
        if (!force && entriesSinceProgress < every) return;
        entriesSinceProgress = 0;
        progress.currentPath = path;
        callback(progress);
    }

    void EmitPartial() {
        if (!options.emitTopLevelSnapshots || !options.partialSnapshot) return;
        options.partialSnapshot(snapshot);
    }

    DiskNodeId AddNode(const fs::path& path, DiskNodeId parentId, bool directory) {
        DiskNode node{};
        node.id = nextId++;
        node.parentId = parentId;
        node.path = path.lexically_normal();
        node.displayName = node.path.filename().wstring();
        if (node.displayName.empty()) node.displayName = node.path.wstring();
        node.directory = directory;
        node.modifiedUnix100ns = Modified100ns(node.path);
        node.protectedPath = ProtectedPath(node.path);
        node.state = DiskScanState::Scanning;
        snapshot.nodes.push_back(std::move(node));
        return snapshot.nodes.back().id;
    }

    DiskNode* FindMutable(DiskNodeId id) noexcept {
        auto it = std::find_if(snapshot.nodes.begin(), snapshot.nodes.end(), [id](const DiskNode& n) { return n.id == id; });
        return it == snapshot.nodes.end() ? nullptr : &*it;
    }

    DiskNodeId AddSkippedReparse(const fs::path& path, DiskNodeId parentId, bool directory) {
        const DiskNodeId childId = AddNode(path, parentId, directory);
        if (auto* child = FindMutable(childId)) {
            child->incomplete = true;
            child->state = DiskScanState::Incomplete;
        }
        if (auto* parent = FindMutable(parentId)) {
            parent->children.push_back(childId);
            if (directory) ++parent->directoryCount;
            else ++parent->fileCount;
            parent->incomplete = true;
        }
        ++snapshot.errorCount;
        ++progress.errors;
        Notify(path);
        return childId;
    }

    DiskNodeId ScanDirectory(const fs::path& path, DiskNodeId parentId) {
        const DiskNodeId nodeId = AddNode(path, parentId, true);
        if (parentId == 0) snapshot.rootId = nodeId;
        ++progress.directoriesVisited;
        Notify(path);

        if (stop.stop_requested()) {
            if (auto* node = FindMutable(nodeId)) {
                node->incomplete = true;
                node->state = DiskScanState::Incomplete;
            }
            snapshot.cancelled = true;
            return nodeId;
        }

        std::error_code ec;
        fs::directory_iterator it(path, fs::directory_options::skip_permission_denied, ec);
        const fs::directory_iterator end;
        if (ec) {
            ++snapshot.errorCount;
            ++progress.errors;
            if (auto* node = FindMutable(nodeId)) {
                node->incomplete = true;
                node->state = DiskScanState::Incomplete;
            }
            Notify(path, true);
            return nodeId;
        }

        for (; it != end; it.increment(ec)) {
            if (stop.stop_requested()) {
                snapshot.cancelled = true;
                break;
            }
            if (ec) {
                ++snapshot.errorCount;
                ++progress.errors;
                ec.clear();
                continue;
            }

            const fs::path childPath = it->path();
            std::error_code typeError;
            const bool directory = it->is_directory(typeError);
            if (typeError) {
                ++snapshot.errorCount;
                ++progress.errors;
                continue;
            }

            if (IsReparsePoint(childPath)) {
                AddSkippedReparse(childPath, nodeId, directory);
                if (parentId == 0) EmitPartial();
                continue;
            }

            if (directory) {
                const DiskNodeId childId = ScanDirectory(childPath, nodeId);
                const DiskNode* child = snapshot.Find(childId);
                if (auto* parent = FindMutable(nodeId); parent && child) {
                    parent->children.push_back(childId);
                    parent->logicalBytes += child->logicalBytes;
                    parent->allocatedBytes += child->allocatedBytes;
                    parent->fileCount += child->fileCount;
                    parent->directoryCount += 1 + child->directoryCount;
                    parent->incomplete = parent->incomplete || child->incomplete;
                }
                if (parentId == 0) EmitPartial();
                continue;
            }

            const bool regular = it->is_regular_file(typeError);
            if (typeError || !regular) continue;
            const auto logical = it->file_size(typeError);
            if (typeError) {
                ++snapshot.errorCount;
                ++progress.errors;
                continue;
            }
            const std::uint64_t allocated = AllocatedFileBytes(childPath, logical);
            ++progress.filesVisited;
            progress.logicalBytes += logical;

            if (auto* parent = FindMutable(nodeId)) {
                parent->logicalBytes += logical;
                parent->allocatedBytes += allocated;
                ++parent->fileCount;
            }

            if (options.includeFilesAsNodes) {
                const DiskNodeId childId = AddNode(childPath, nodeId, false);
                if (auto* file = FindMutable(childId)) {
                    file->logicalBytes = logical;
                    file->allocatedBytes = allocated;
                    file->fileCount = 1;
                    file->state = DiskScanState::Complete;
                }
                if (auto* parent = FindMutable(nodeId)) parent->children.push_back(childId);
            }
            Notify(childPath);
        }

        if (auto* node = FindMutable(nodeId)) {
            if (snapshot.cancelled) node->incomplete = true;
            node->state = node->incomplete ? DiskScanState::Incomplete : DiskScanState::Complete;
        }
        Notify(path, true);
        return nodeId;
    }
};

} // namespace

const DiskNode* DiskScanSnapshot::Find(DiskNodeId id) const noexcept {
    const auto it = std::find_if(nodes.begin(), nodes.end(), [id](const DiskNode& node) { return node.id == id; });
    return it == nodes.end() ? nullptr : &*it;
}

const DiskNode* DiskScanSnapshot::FindPath(const std::filesystem::path& path) const noexcept {
    const auto wanted = NormalizePath(path);
    const auto it = std::find_if(nodes.begin(), nodes.end(), [&](const DiskNode& node) {
        return NormalizePath(node.path) == wanted;
    });
    return it == nodes.end() ? nullptr : &*it;
}

DiskScanSnapshot ScanDiskTree(const std::filesystem::path& root,
                              std::stop_token stop,
                              DiskProgressCallback progress,
                              DiskScanOptions options) {
    ScanContext ctx{};
    ctx.stop = stop;
    ctx.callback = std::move(progress);
    ctx.options = std::move(options);

    if (stop.stop_requested()) {
        ctx.snapshot.cancelled = true;
        return ctx.snapshot;
    }

    std::error_code ec;
    if (root.empty() || !std::filesystem::exists(root, ec) || ec || !std::filesystem::is_directory(root, ec) || ec) {
        ctx.snapshot.errorCount = 1;
        return ctx.snapshot;
    }

    ctx.snapshot.rootId = ctx.ScanDirectory(root, 0);
    ctx.snapshot.cancelled = ctx.snapshot.cancelled || stop.stop_requested();
    ctx.snapshot.complete = !ctx.snapshot.cancelled;
    ctx.Notify(root, true);
    ctx.EmitPartial();
    return ctx.snapshot;
}

double ParentPercent(const DiskNode& node, const DiskNode* parent) noexcept {
    if (!parent) return 100.0;
    if (parent->logicalBytes == 0) return 0.0;
    const double percent = 100.0 * static_cast<double>(node.logicalBytes) /
                           static_cast<double>(parent->logicalBytes);
    return std::clamp(percent, 0.0, 100.0);
}

} // namespace dpop::disk
