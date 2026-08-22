#include "modules/Cleaner.h"
#include <windows.h>
#include <filesystem>

namespace fs = std::filesystem;

namespace {
fs::path UserTemp() {
    std::wstring buf(32768, L'\0');
    DWORD n = GetTempPathW(static_cast<DWORD>(buf.size()), buf.data());
    if (!n || n >= buf.size()) return {};
    buf.resize(n);
    return fs::path(buf);
}
}

namespace dpop::cleaner {
std::uint64_t EstimateUserTempBytes() {
    std::uint64_t total = 0;
    std::error_code ec;
    const auto root = UserTemp();
    if (root.empty()) return 0;
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (it->is_regular_file(ec)) total += it->file_size(ec);
        ec.clear();
    }
    return total;
}

Result CleanUserTemp() {
    Result r{};
    std::error_code ec;
    const auto root = UserTemp();
    if (root.empty()) return r;

    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (!it->is_regular_file(ec)) { ec.clear(); continue; }
        const auto size = it->file_size(ec);
        ec.clear();
        if (fs::remove(it->path(), ec)) {
            r.removedBytes += size;
            ++r.removedFiles;
        } else {
            ++r.failedFiles;
            ec.clear();
        }
    }
    return r;
}
}
