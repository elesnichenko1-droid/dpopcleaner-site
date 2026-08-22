#pragma once
#include <cstdint>
#include <string>

namespace dpop::cleaner {
struct Result {
    std::uint64_t removedBytes{};
    unsigned removedFiles{};
    unsigned failedFiles{};
};

std::uint64_t EstimateUserTempBytes();
std::uint64_t EstimateCrashDumpBytes();
std::uint64_t EstimateBrowserCacheBytes();
std::uint64_t EstimateRecycleBinBytes();
Result CleanUserTemp();
Result CleanCrashDumps();
Result CleanBrowserCaches();
bool EmptyRecycleBin(std::wstring& error);
}
