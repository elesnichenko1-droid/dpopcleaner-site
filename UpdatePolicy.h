#pragma once

#include <cstdint>
#include <string>

namespace dpop::update {

enum class CheckMode {
    Background,
    Interactive
};

enum class ResultAction {
    Ignore,
    ShowError,
    ShowCurrent,
    RecordAvailable,
    OfferInstall
};

bool IsUpdateOfferAllowed(bool manifestAvailable, int currentVersionCode, int remoteVersionCode);
ResultAction DecideUpdateResult(CheckMode mode, bool success, bool updateAvailable);
std::wstring BuildUpdaterArguments(std::uint32_t parentPid,
                                   const std::wstring& packagePath,
                                   const std::wstring& sha256,
                                   bool allowUnsigned,
                                   const std::wstring& installArgs);

}
