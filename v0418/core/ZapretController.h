#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace dpop0418 {

inline constexpr wchar_t kBundledZapretVersion[] = L"1.10.2";

struct ZapretStatus {
    bool payloadAvailable{};
    bool payloadIntegrityOk{};
    bool serviceInstalled{};
    bool serviceRunning{};
    bool bundledWinwsRunning{};
    bool externalWinwsRunning{};
    std::wstring serviceStrategy;
    std::wstring error;
};

struct ZapretStrategy {
    std::wstring displayName;
    std::filesystem::path batchPath;
};

std::filesystem::path BundledZapretRoot(const std::filesystem::path& executableDirectory);
bool ValidateBundledPayload(const std::filesystem::path& root, std::wstring& error);
std::vector<ZapretStrategy> EnumerateZapretStrategies(const std::filesystem::path& root);
bool IsBundledWinwsPath(const std::filesystem::path& candidate, const std::filesystem::path& root);
size_t FindStrategyMenuIndex(const std::vector<ZapretStrategy>& strategies, const std::wstring& selectedName);

} // namespace dpop0418
