#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace dpop::guard {
struct Finding {
    std::wstring title;
    std::wstring details;
    std::wstring severity;
};
struct ScanResult {
    std::vector<Finding> findings;
    unsigned processesChecked{};
    unsigned startupChecked{};
    std::wstring note;
};
ScanResult QuickScan();
bool ScanFileWithAmsi(const std::filesystem::path& file, std::wstring& verdict, std::wstring& error);
}
