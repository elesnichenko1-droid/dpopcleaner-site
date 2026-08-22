#include "modules/ZapretPolicy.h"

#include <algorithm>
#include <cwctype>
#include <string>

namespace fs = std::filesystem;

namespace {
fs::path CanonicalPath(const fs::path& path) {
    std::error_code ec;
    fs::path absolute = fs::absolute(path, ec);
    if (ec) {
        absolute = path.lexically_normal();
        ec.clear();
    }
    const fs::path canonical = fs::weakly_canonical(absolute, ec);
    return ec ? absolute.lexically_normal() : canonical;
}

std::wstring FoldCase(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}
}

namespace dpop::zapret {

fs::path ResolveBundledRoot(const fs::path& executableDirectory) {
    return (executableDirectory / L"zapret").lexically_normal();
}

const std::vector<fs::path>& RequiredBundleFiles() {
    static const std::vector<fs::path> files{
        L"general.bat",
        L"service.bat",
        fs::path(L"bin") / L"winws.exe",
        fs::path(L"bin") / L"WinDivert.dll",
        fs::path(L"bin") / L"WinDivert64.sys",
        fs::path(L"bin") / L"cygwin1.dll",
        fs::path(L"utils") / L"check_updates.enabled",
        L"LICENSE.txt"
    };
    return files;
}

BundleValidation ValidateBundle(const fs::path& bundleRoot) {
    std::error_code ec;
    for (const auto& relative : RequiredBundleFiles()) {
        if (!fs::is_regular_file(bundleRoot / relative, ec)) {
            return {false, relative};
        }
        ec.clear();
    }
    return {true, {}};
}

bool IsBundledWinwsPath(const fs::path& bundleRoot, const fs::path& processImagePath) {
    if (processImagePath.empty()) {
        return false;
    }
    const auto expected = CanonicalPath(bundleRoot / L"bin" / L"winws.exe");
    const auto actual = CanonicalPath(processImagePath);
    return FoldCase(expected.native()) == FoldCase(actual.native());
}

}
