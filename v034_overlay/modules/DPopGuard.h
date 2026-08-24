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

struct DefenderStatus {
    bool cliAvailable{false};
    bool serviceRunning{false};
    std::filesystem::path cliPath;
};

struct DefenderScanResult {
    bool started{false};
    bool completed{false};
    unsigned long exitCode{0};
    std::wstring message;
};

ScanResult QuickScan();
bool ScanFileWithAmsi(const std::filesystem::path& file, std::wstring& verdict, std::wstring& error);
DefenderStatus QueryDefenderStatus();
DefenderScanResult RunDefenderQuickScan();
DefenderScanResult RunDefenderCustomScan(const std::filesystem::path& path);

}
