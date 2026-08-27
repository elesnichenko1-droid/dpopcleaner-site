#include "Signature.h"

#include <windows.h>
#include <softpub.h>
#include <wintrust.h>

namespace dpop0418 {

bool VerifyAuthenticode(const std::filesystem::path& file, std::wstring& error) {
    WINTRUST_FILE_INFO fileInfo{};
    fileInfo.cbStruct = sizeof(fileInfo);
    fileInfo.pcwszFilePath = file.c_str();

    WINTRUST_DATA data{};
    data.cbStruct = sizeof(data);
    data.dwUIChoice = WTD_UI_NONE;
    data.fdwRevocationChecks = WTD_REVOKE_WHOLECHAIN;
    data.dwUnionChoice = WTD_CHOICE_FILE;
    data.pFile = &fileInfo;
    data.dwStateAction = WTD_STATEACTION_VERIFY;
    data.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;

    GUID policy = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    const LONG status = WinVerifyTrust(nullptr, &policy, &data);
    data.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(nullptr, &policy, &data);

    if (status == ERROR_SUCCESS) return true;
    error = L"Authenticode-подпись отсутствует или не прошла проверку. Код: " + std::to_wstring(status);
    return false;
}

} // namespace dpop0418
