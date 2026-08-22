#include "update/UpdatePolicy.h"

namespace {
std::wstring QuoteArgument(const std::wstring& value) {
    std::wstring quoted(1, L'"');
    std::size_t backslashes = 0;
    for (const wchar_t ch : value) {
        if (ch == L'\\') {
            ++backslashes;
            continue;
        }
        if (ch == L'"') {
            quoted.append(backslashes * 2 + 1, L'\\');
            quoted.push_back(L'"');
            backslashes = 0;
            continue;
        }
        quoted.append(backslashes, L'\\');
        backslashes = 0;
        quoted.push_back(ch);
    }
    quoted.append(backslashes * 2, L'\\');
    quoted.push_back(L'"');
    return quoted;
}
}

namespace dpop::update {

bool IsUpdateOfferAllowed(bool manifestAvailable, int currentVersionCode, int remoteVersionCode) {
    return manifestAvailable && remoteVersionCode > currentVersionCode;
}

ResultAction DecideUpdateResult(CheckMode mode, bool success, bool updateAvailable) {
    if (!success) {
        return mode == CheckMode::Interactive ? ResultAction::ShowError : ResultAction::Ignore;
    }
    if (!updateAvailable) {
        return mode == CheckMode::Interactive ? ResultAction::ShowCurrent : ResultAction::Ignore;
    }
    return mode == CheckMode::Interactive ? ResultAction::OfferInstall : ResultAction::RecordAvailable;
}

std::wstring BuildUpdaterArguments(std::uint32_t parentPid,
                                   const std::wstring& packagePath,
                                   const std::wstring& sha256,
                                   bool allowUnsigned,
                                   const std::wstring& installArgs) {
    std::wstring arguments = L"--parent " + std::to_wstring(parentPid) +
        L" --package " + QuoteArgument(packagePath) +
        L" --sha256 " + QuoteArgument(sha256);
    if (allowUnsigned) {
        arguments += L" --allow-unsigned";
    }
    if (!installArgs.empty()) {
        arguments += L" --args " + QuoteArgument(installArgs);
    }
    return arguments;
}

}
