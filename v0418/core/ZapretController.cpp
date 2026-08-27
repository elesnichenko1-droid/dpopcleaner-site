#include "ZapretController.h"

#include <algorithm>
#include <cwctype>

namespace dpop0418 {
namespace {
namespace fs = std::filesystem;

std::wstring Lower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
    return value;
}

std::wstring NormalizedForCompare(const fs::path& path) {
    std::error_code ec;
    fs::path value = fs::weakly_canonical(path, ec);
    if (ec) {
        ec.clear();
        value = fs::absolute(path, ec);
        if (ec) value = path;
        value = value.lexically_normal();
    }
    return Lower(value.wstring());
}

bool NaturalLessText(const std::wstring& leftRaw, const std::wstring& rightRaw) {
    const std::wstring left = Lower(leftRaw);
    const std::wstring right = Lower(rightRaw);
    size_t i = 0;
    size_t j = 0;
    while (i < left.size() && j < right.size()) {
        if (std::iswdigit(left[i]) && std::iswdigit(right[j])) {
            size_t iEnd = i;
            size_t jEnd = j;
            while (iEnd < left.size() && std::iswdigit(left[iEnd])) ++iEnd;
            while (jEnd < right.size() && std::iswdigit(right[jEnd])) ++jEnd;

            size_t iSig = i;
            size_t jSig = j;
            while (iSig + 1 < iEnd && left[iSig] == L'0') ++iSig;
            while (jSig + 1 < jEnd && right[jSig] == L'0') ++jSig;
            const size_t iDigits = iEnd - iSig;
            const size_t jDigits = jEnd - jSig;
            if (iDigits != jDigits) return iDigits < jDigits;
            const int numericCmp = left.compare(iSig, iDigits, right, jSig, jDigits);
            if (numericCmp != 0) return numericCmp < 0;
            i = iEnd;
            j = jEnd;
            continue;
        }
        if (left[i] != right[j]) return left[i] < right[j];
        ++i;
        ++j;
    }
    return left.size() < right.size();
}

bool HasExtensionBat(const fs::path& path) {
    return Lower(path.extension().wstring()) == L".bat";
}

bool StartsWithService(const fs::path& path) {
    const std::wstring name = Lower(path.filename().wstring());
    return name.rfind(L"service", 0) == 0;
}

} // namespace

fs::path BundledZapretRoot(const fs::path& executableDirectory) {
    return executableDirectory / L"ThirdParty" / L"Zapret";
}

bool ValidateBundledPayload(const fs::path& root, std::wstring& error) {
    error.clear();
    struct Required {
        fs::path relative;
        bool directory;
    };
    const Required required[] = {
        {L"LICENSE.txt", false},
        {L"service.bat", false},
        {fs::path(L"bin") / L"winws.exe", false},
        {fs::path(L"bin") / L"WinDivert.dll", false},
        {fs::path(L"bin") / L"WinDivert64.sys", false},
        {L"lists", true},
    };

    std::error_code ec;
    for (const auto& item : required) {
        const fs::path candidate = root / item.relative;
        const bool ok = item.directory ? fs::is_directory(candidate, ec) : fs::is_regular_file(candidate, ec);
        if (!ok || ec) {
            error = L"Отсутствует обязательный файл/каталог Zapret: " + item.relative.wstring();
            return false;
        }
        ec.clear();
    }
    return true;
}

std::vector<ZapretStrategy> EnumerateZapretStrategies(const fs::path& root) {
    std::vector<ZapretStrategy> result;
    std::error_code ec;
    if (!fs::is_directory(root, ec) || ec) return result;

    for (fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
         !ec && it != end; it.increment(ec)) {
        const fs::directory_entry& entry = *it;
        if (!entry.is_regular_file(ec) || ec) {
            ec.clear();
            continue;
        }
        const fs::path path = entry.path();
        if (!HasExtensionBat(path) || StartsWithService(path)) continue;
        result.push_back({path.stem().wstring(), path});
    }

    std::sort(result.begin(), result.end(), [](const ZapretStrategy& a, const ZapretStrategy& b) {
        return NaturalLessText(a.batchPath.filename().wstring(), b.batchPath.filename().wstring());
    });
    return result;
}

bool IsBundledWinwsPath(const fs::path& candidate, const fs::path& root) {
    if (candidate.empty() || root.empty()) return false;
    return NormalizedForCompare(candidate) == NormalizedForCompare(root / L"bin" / L"winws.exe");
}

size_t FindStrategyMenuIndex(const std::vector<ZapretStrategy>& strategies, const std::wstring& selectedName) {
    const std::wstring wanted = Lower(fs::path(selectedName).filename().wstring());
    if (wanted.empty()) return 0;
    for (size_t i = 0; i < strategies.size(); ++i) {
        if (Lower(strategies[i].batchPath.filename().wstring()) == wanted) return i + 1;
    }
    return 0;
}

} // namespace dpop0418
