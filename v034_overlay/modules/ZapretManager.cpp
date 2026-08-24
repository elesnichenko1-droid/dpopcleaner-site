#include "modules/ZapretManager.h"
#include "core/Paths.h"
#include "modules/ZapretPolicy.h"

#include <windows.h>
#include <tlhelp32.h>
#include <winsvc.h>
#include <shellapi.h>
#include <filesystem>
#include <cwchar>
#include <vector>

namespace fs = std::filesystem;

namespace {
fs::path ProcessPath(DWORD pid) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return {};
    std::wstring path(32768, L'\0');
    DWORD size = static_cast<DWORD>(path.size());
    if (!QueryFullProcessImageNameW(process, 0, path.data(), &size)) path.clear();
    else path.resize(size);
    CloseHandle(process);
    return path;
}

fs::path BundleRoot() {
    return dpop::zapret::ResolveBundledRoot(dpop::paths::ExecutableDir());
}

bool RequireValidBundle(fs::path& root, std::wstring& error) {
    root = BundleRoot();
    const auto validation = dpop::zapret::ValidateBundle(root);
    if (!validation.valid) {
        error = L"Комплект Zapret повреждён или неполон. Не найден файл: " + validation.missing.wstring();
        return false;
    }
    return true;
}

bool RunHiddenAndWait(const wchar_t* file, const wchar_t* arguments, DWORD timeoutMs, DWORD& exitCode, std::wstring& error) {
    SHELLEXECUTEINFOW execute{};
    execute.cbSize = sizeof(execute);
    execute.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    execute.lpFile = file;
    execute.lpParameters = arguments;
    execute.nShow = SW_HIDE;
    if (!ShellExecuteExW(&execute) || !execute.hProcess) {
        error = L"Не удалось запустить системную команду. Код Windows: " + std::to_wstring(GetLastError());
        return false;
    }
    const DWORD wait = WaitForSingleObject(execute.hProcess, timeoutMs);
    if (wait != WAIT_OBJECT_0) {
        CloseHandle(execute.hProcess);
        error = wait == WAIT_TIMEOUT ? L"Системная команда не завершилась вовремя." : L"Ошибка ожидания системной команды.";
        return false;
    }
    if (!GetExitCodeProcess(execute.hProcess, &exitCode)) exitCode = static_cast<DWORD>(-1);
    CloseHandle(execute.hProcess);
    if (exitCode != 0) {
        error = L"Системная команда завершилась с кодом " + std::to_wstring(exitCode) + L".";
        return false;
    }
    error.clear();
    return true;
}

bool LaunchBatch(const fs::path& relativeScript, std::wstring& error) {
    fs::path root;
    if (!RequireValidBundle(root, error)) return false;
    const fs::path script = root / relativeScript;
    std::error_code ec;
    if (!fs::is_regular_file(script, ec)) {
        error = L"Скрипт Zapret не найден: " + script.wstring();
        return false;
    }
    const std::wstring arguments = L"/d /c \"\"" + script.wstring() + L"\"\"";
    SHELLEXECUTEINFOW execute{};
    execute.cbSize = sizeof(execute);
    execute.lpFile = L"cmd.exe";
    execute.lpParameters = arguments.c_str();
    execute.lpDirectory = root.c_str();
    execute.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&execute)) {
        error = L"Не удалось запустить " + script.wstring();
        return false;
    }
    error.clear();
    return true;
}
}

namespace dpop::zapret {

Status QueryStatus() {
    Status status{};
    status.bundleFolder = BundleRoot();
    const auto validation = ValidateBundle(status.bundleFolder);
    status.bundleValid = validation.valid;
    status.missingBundleFile = validation.missing;

    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (scm) {
        SC_HANDLE service = OpenServiceW(scm, L"zapret", SERVICE_QUERY_STATUS);
        if (service) {
            status.serviceInstalled = true;
            SERVICE_STATUS serviceStatus{};
            if (QueryServiceStatus(service, &serviceStatus)) {
                status.serviceRunning = serviceStatus.dwCurrentState == SERVICE_RUNNING;
            }
            CloseServiceHandle(service);
        }
        CloseServiceHandle(scm);
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        if (Process32FirstW(snapshot, &entry)) {
            do {
                if (_wcsicmp(entry.szExeFile, L"winws.exe") != 0) continue;
                const auto path = ProcessPath(entry.th32ProcessID);
                if (IsBundledWinwsPath(status.bundleFolder, path)) {
                    status.winwsRunning = true;
                    break;
                }
            } while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
    }
    return status;
}

std::vector<StrategyEntry> EnumerateStrategies() {
    return EnumerateStrategiesAt(BundleRoot());
}

bool LaunchStrategy(const fs::path& relativeScript, std::wstring& error) {
    if (!IsLaunchableStrategyPath(relativeScript)) {
        error = L"Выбран небезопасный или неподдерживаемый путь стратегии Zapret.";
        return false;
    }
    return LaunchBatch(relativeScript, error);
}

bool LaunchDefaultStrategy(std::wstring& error) {
    return LaunchStrategy(L"general.bat", error);
}

bool StopBundledWinws(std::wstring& error) {
    const auto status = QueryStatus();
    if (status.serviceRunning) {
        error = L"Zapret запущен как Windows-служба. Для корректной остановки открой Service Manager.";
        return false;
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        error = L"Не удалось получить список процессов Windows.";
        return false;
    }

    bool failed = false;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, L"winws.exe") != 0) continue;
            const auto path = ProcessPath(entry.th32ProcessID);
            if (!IsBundledWinwsPath(status.bundleFolder, path)) continue;
            HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, entry.th32ProcessID);
            if (!process) {
                failed = true;
                continue;
            }
            if (!TerminateProcess(process, 0)) failed = true;
            else WaitForSingleObject(process, 3000);
            CloseHandle(process);
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);

    if (failed) {
        error = L"Не удалось остановить один или несколько bundled winws.exe. Возможно, нужны права администратора.";
        return false;
    }
    error.clear();
    return true;
}

bool OpenServiceManager(std::wstring& error) {
    return LaunchBatch(L"service.bat", error);
}

bool OpenBundledFolder(std::wstring& error) {
    fs::path root;
    if (!RequireValidBundle(root, error)) return false;
    if (reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", root.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) <= 32) {
        error = L"Не удалось открыть папку Zapret.";
        return false;
    }
    error.clear();
    return true;
}

bool RepairRtc(const fs::path& relativeScript, std::wstring& report) {
    if (!IsLaunchableStrategyPath(relativeScript)) {
        report = L"Выбран небезопасный или неподдерживаемый путь стратегии Zapret.";
        return false;
    }
    const auto before = QueryStatus();
    if (!before.bundleValid) {
        report = L"Сначала восстановите bundle Zapret: отсутствует " + before.missingBundleFile.wstring();
        return false;
    }
    if (before.serviceRunning) {
        report = L"Zapret сейчас работает как Windows-служба. RTC repair не перезапускает службу автоматически: откройте Service Manager, остановите службу и повторите действие.";
        return false;
    }

    std::wstring stopError;
    if (before.winwsRunning && !StopBundledWinws(stopError)) {
        report = L"Не удалось безопасно остановить bundled winws: " + stopError;
        return false;
    }

    DWORD dnsCode = 0;
    std::wstring dnsError;
    if (!RunHiddenAndWait(L"ipconfig.exe", L"/flushdns", 15000, dnsCode, dnsError)) {
        report = L"Bundled winws остановлен, но DNS-кэш не очищен: " + dnsError;
        return false;
    }

    Sleep(700);
    std::wstring launchError;
    if (!LaunchStrategy(relativeScript, launchError)) {
        report = L"DNS-кэш очищен, но стратегия Zapret не перезапустилась: " + launchError;
        return false;
    }
    report = L"RTC repair выполнен: standalone bundled winws остановлен (если был запущен), DNS-кэш очищен и выбранная стратегия запущена заново. Если Discord всё ещё висит на RTC, полностью перезапустите Discord.";
    return true;
}

bool OpenZapretUpdatePage(std::wstring& error) {
    constexpr wchar_t kLatest[] = L"https://github.com/Flowseal/zapret-discord-youtube/releases/latest";
    const auto code = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", kLatest, nullptr, nullptr, SW_SHOWNORMAL));
    if (code <= 32) {
        error = L"Не удалось открыть страницу обновлений Zapret.";
        return false;
    }
    error.clear();
    return true;
}

}
