#include "modules/ZapretCenterModel.h"

#include <algorithm>
#include <cwctype>

namespace fs = std::filesystem;

namespace {
std::wstring Fold(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}
}

namespace dpop::zapret {

bool IsLaunchableStrategyPath(const fs::path& relative) noexcept {
    if (relative.empty() || relative.is_absolute() || !relative.parent_path().empty()) return false;
    const std::wstring filename = Fold(relative.filename().wstring());
    if (filename == L"." || filename == L"..") return false;
    if (Fold(relative.extension().wstring()) != L".bat") return false;
    return filename.rfind(L"general", 0) == 0;
}

std::vector<StrategyEntry> EnumerateStrategiesAt(const fs::path& bundleRoot) {
    std::vector<StrategyEntry> out;
    std::error_code ec;
    for (fs::directory_iterator it(bundleRoot, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) { ec.clear(); continue; }
        const fs::path relative = it->path().filename();
        if (!IsLaunchableStrategyPath(relative)) continue;
        const bool isDefault = Fold(relative.filename().wstring()) == L"general.bat";
        out.push_back({relative, relative.filename().wstring(), isDefault});
    }
    std::sort(out.begin(), out.end(), [](const StrategyEntry& a, const StrategyEntry& b) {
        if (a.isDefault != b.isDefault) return a.isDefault > b.isDefault;
        return Fold(a.displayName) < Fold(b.displayName);
    });
    return out;
}

}
