#include "modules/DiskAnalyzer.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <filesystem>
#include <fstream>
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

    const auto* a = snapshot.FindPath(root / "A");
    assert(a && a->logicalBytes == 900);
    assert(a->fileCount == 2);
    assert(a->directoryCount == 1);
    assert(ParentPercent(*a, rootNode) == 90.0);

    DiskNode zeroParent{};
    assert(ParentPercent(*a, &zeroParent) == 0.0);
    assert(ParentPercent(*rootNode, nullptr) == 100.0);

    std::stop_source stop;
    stop.request_stop();
    const auto cancelled = ScanDiskTree(root, stop.get_token(), {}, {});
    assert(cancelled.cancelled);
    assert(!cancelled.complete);

    fs::remove_all(root, ec);
    return 0;
}
