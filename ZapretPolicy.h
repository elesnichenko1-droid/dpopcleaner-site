#pragma once

#include <filesystem>
#include <vector>

namespace dpop::zapret {

struct BundleValidation {
    bool valid{};
    std::filesystem::path missing;
};

std::filesystem::path ResolveBundledRoot(const std::filesystem::path& executableDirectory);
const std::vector<std::filesystem::path>& RequiredBundleFiles();
BundleValidation ValidateBundle(const std::filesystem::path& bundleRoot);
bool IsBundledWinwsPath(const std::filesystem::path& bundleRoot,
                        const std::filesystem::path& processImagePath);

}
