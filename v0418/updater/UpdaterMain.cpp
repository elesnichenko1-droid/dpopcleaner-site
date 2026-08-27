#include "Hash.h"
#include "Signature.h"

#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace {

std::wstring ArgValue(const std::vector<std::wstring>& args, const std::wstring& key) {
    for (std::size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == key) return args[i + 1];
    }
    return {};
}

bool HasArg(const std::vector<std::wstring>& args, const std::wstring& key) {
    return std::find(args.begin(), args.end(), key) != args.end();
}

bool EqualsNoCase(std::wstring left, std::wstring right) {
    std::transform(left.begin(), left.end(), left.begin(), ::towlower);
    std::transform(right.begin(), right.end(), right.begin(), ::towlower);
    return left == right;
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 2;
    std::vector<std::wstring> args;
    args.reserve(static_cast<std::size_t>(argc));
    for (int i = 0; i < argc; ++i) args.emplace_back(argv[i]);
    LocalFree(argv);

    const std::wstring parentText = ArgValue(args, L"--parent");
    const std::filesystem::path package = ArgValue(args, L"--package");
    const std::wstring expectedHash = ArgValue(args, L"--sha256");
    const std::wstring installArgs = ArgValue(args, L"--args");
    const std::filesystem::path restartPath = ArgValue(args, L"--restart");
    const bool allowUnsigned = HasArg(args, L"--allow-unsigned");
    const bool signedPackage = HasArg(args, L"--signed");

    if (parentText.empty() || package.empty() || expectedHash.empty()) return 2;

    DWORD parentPid = 0;
    try {
        parentPid = static_cast<DWORD>(std::stoul(parentText));
    } catch (...) {
        return 2;
    }

    HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, parentPid);
    if (parent) {
        WaitForSingleObject(parent, 30000);
        CloseHandle(parent);
    }

    std::wstring actualHash;
    std::wstring verifyError;
    if (!dpop0418::Sha256File(package, actualHash, verifyError) ||
        !EqualsNoCase(actualHash, expectedHash)) {
        MessageBoxW(nullptr,
                    (L"Обновление остановлено: SHA-256 пакета не совпадает.\n" + verifyError).c_str(),
                    L"DPopUpdater",
                    MB_OK | MB_ICONERROR);
        return 3;
    }

    if (signedPackage && !allowUnsigned) {
        if (!dpop0418::VerifyAuthenticode(package, verifyError)) {
            MessageBoxW(nullptr,
                        (L"Обновление остановлено: подпись пакета не прошла проверку.\n" + verifyError).c_str(),
                        L"DPopUpdater",
                        MB_OK | MB_ICONERROR);
            return 4;
        }
    } else if (!signedPackage && !allowUnsigned) {
        MessageBoxW(nullptr,
                    L"Обновление остановлено: неподписанный пакет не был явно разрешён пользователем.",
                    L"DPopUpdater",
                    MB_OK | MB_ICONERROR);
        return 4;
    }

    SHELLEXECUTEINFOW execute{};
    execute.cbSize = sizeof(execute);
    execute.fMask = SEE_MASK_NOCLOSEPROCESS;
    execute.lpVerb = L"runas";
    execute.lpFile = package.c_str();
    execute.lpParameters = installArgs.empty() ? nullptr : installArgs.c_str();
    execute.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&execute)) {
        MessageBoxW(nullptr,
                    L"Не удалось запустить установщик обновления.",
                    L"DPopUpdater",
                    MB_OK | MB_ICONERROR);
        return 5;
    }

    DWORD exitCode = 0;
    if (execute.hProcess) {
        WaitForSingleObject(execute.hProcess, INFINITE);
        GetExitCodeProcess(execute.hProcess, &exitCode);
        CloseHandle(execute.hProcess);
        if (exitCode != 0 && exitCode != 3010) return static_cast<int>(exitCode);
    }

    if (!restartPath.empty() && std::filesystem::exists(restartPath)) {
        ShellExecuteW(nullptr, L"open", restartPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
    return 0;
}
