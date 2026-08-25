#include "modules/DiskAnalyzer.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stop_token>
#include <string>

namespace fs = std::filesystem;
using namespace dpop::disk;

static void WriteBytes(const fs::path& p, std::size_t count) {
    std::ofstream out(p, std::ios::binary);
    std::string data(count, 'x');
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
}

int main() {
    const fs::path root = fs::temp_directory_path() / "dpop035_disk_test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "A" / "B");
    WriteBytes(root / "root.bin", 100);
    WriteBytes(root / "A" / "a.bin", 300);
    WriteBytes(root / "A" / "B" / "b.bin", 600);

    const auto snapshot = ScanDiskTree(root, {}, {}, {});
    assert(snapshot.complete);
    assert(!snapshot.cancelled);
    const auto* rootNode = snapshot.Find(snapshot.rootId);
    assert(rootNode);
    assert(rootNode->logicalBytes == 1000);
    assert(rootNode->fileCount == 3);
    assert(rootNode->directoryCount == 2);
    assert(rootNode->allocatedComplete);

    const auto* a = snapshot.FindPath(root / "A");
    assert(a && a->logicalBytes == 900);
    assert(a->fileCount == 2);
    assert(a->directoryCount == 1);
    assert(a->allocatedComplete);
    assert(ParentPercent(*a, rootNode) == 90.0);

    DiskNode zeroParent{};
    assert(ParentPercent(*a, &zeroParent) == 0.0);
    assert(ParentPercent(*rootNode, nullptr) == 100.0);

    DiskScanOptions unknownAllocation{};
    unknownAllocation.allocatedSizeProvider = [](const fs::path& path, std::uint64_t logical)
        -> std::optional<std::uint64_t> {
        if (path.filename() == L"a.bin") return std::nullopt;
        return logical;
    };
    const auto partialAllocation = ScanDiskTree(root, {}, {}, unknownAllocation);
    assert(partialAllocation.complete);
    const auto* partialRoot = partialAllocation.Find(partialAllocation.rootId);
    const auto* partialA = partialAllocation.FindPath(root / "A");
    const auto* unknownFile = partialAllocation.FindPath(root / "A" / "a.bin");
    const auto* knownFile = partialAllocation.FindPath(root / "A" / "B" / "b.bin");
    assert(partialRoot && !partialRoot->allocatedComplete);
    assert(partialA && !partialA->allocatedComplete);
    assert(unknownFile && !unknownFile->allocatedComplete);
    assert(unknownFile->allocatedBytes == 0);
    assert(knownFile && knownFile->allocatedComplete);
    assert(partialRoot->allocatedBytes == 700);
    assert(partialA->allocatedBytes == 600);

    std::stop_source stop;
    stop.request_stop();
    const auto cancelled = ScanDiskTree(root, stop.get_token(), {}, {});
    assert(cancelled.cancelled);
    assert(!cancelled.complete);

    fs::remove_all(root, ec);
    return 0;
}
