#include "ZapretController.h"

#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <winsvc.h>

#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <fstream>
#include <string>
#include <vector>

namespace dpop0418 {
namespace {
namespace fs = std::filesystem;

struct ServiceSnapshot {
    bool installed{};
    bool running{};
    bool owned{};
    std::wstring commandLine;
};

struct WinwsProcess {
    DWORD pid{};
    fs::path imagePath;
};

std::wstring Lower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
    return value;
}

std::wstring TrimWide(std::wstring value) {
    const auto notSpace = [](wchar_t ch) { return std::iswspace(ch) == 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
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

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                                         static_cast<int>(value.size()), nullptr, 0,
                                         nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(static_cast<size_t>(size), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                            static_cast<int>(value.size()), result.data(), size,
                            nullptr, nullptr) != size) return {};
    return result;
}

std::wstring Win32Message(const wchar_t* prefix, DWORD code = GetLastError()) {
    return std::wstring(prefix) + L" (код " + std::to_wstring(code) + L")";
}

std::wstring ExtractExecutable(const std::wstring& commandLine) {
    std::wstring value = TrimWide(commandLine);
    if (value.empty()) return {};
    if (value.front() == L'\"') {
        const size_t end = value.find(L'\"', 1);
        if (end == std::wstring::npos) return {};
        return value.substr(1, end - 1);
    }
    const size_t end = value.find_first_of(L" \t");
    return end == std::wstring::npos ? value : value.substr(0, end);
}

bool ReadPinnedVersion(const fs::path& root, std::wstring& value) {
    value.clear();
    std::ifstream input(root / L".service" / L"version.txt", std::ios::binary);
    if (!input) return false;
    std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    while (!bytes.empty() && (bytes.back() == '\r' || bytes.back() == '\n' || bytes.back() == ' ' || bytes.back() == '\t'))
        bytes.pop_back();
    if (bytes.empty()) return false;
    value.assign(bytes.begin(), bytes.end());
    return true;
}

fs::path QueryProcessPath(DWORD pid) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return {};
    std::vector<wchar_t> buffer(32768);
    DWORD size = static_cast<DWORD>(buffer.size());
    fs::path result;
    if (QueryFullProcessImageNameW(process, 0, buffer.data(), &size) && size > 0)
        result = fs::path(std::wstring(buffer.data(), size));
    CloseHandle(process);
    return result;
}

std::vector<WinwsProcess> EnumerateWinwsProcesses() {
    std::vector<WinwsProcess> result;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return result;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, L"winws.exe") == 0)
                result.push_back({entry.th32ProcessID, QueryProcessPath(entry.th32ProcessID)});
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return result;
}

ServiceSnapshot QueryServiceSnapshot(const fs::path& root) {
    ServiceSnapshot result{};
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return result;
    SC_HANDLE service = OpenServiceW(scm, L"zapret", SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);
    if (!service) {
        CloseServiceHandle(scm);
        return result;
    }

    result.installed = true;
    SERVICE_STATUS_PROCESS status{};
    DWORD bytesNeeded = 0;
    if (QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO,
                             reinterpret_cast<LPBYTE>(&status), sizeof(status), &bytesNeeded)) {
        result.running = status.dwCurrentState == SERVICE_RUNNING ||
                         status.dwCurrentState == SERVICE_START_PENDING ||
                         status.dwCurrentState == SERVICE_STOP_PENDING;
    }

    DWORD required = 0;
    QueryServiceConfigW(service, nullptr, 0, &required);
    if (required > 0) {
        std::vector<unsigned char> buffer(required);
        auto* config = reinterpret_cast<QUERY_SERVICE_CONFIGW*>(buffer.data());
        if (QueryServiceConfigW(service, config, required, &required) && config->lpBinaryPathName) {
            result.commandLine = config->lpBinaryPathName;
            result.owned = IsOwnedZapretServiceCommand(result.commandLine, root);
        }
    }

    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return result;
}

std::wstring ReadServiceStrategy() {
    DWORD type = 0;
    DWORD bytes = 0;
    constexpr wchar_t key[] = L"SYSTEM\\CurrentControlSet\\Services\\zapret";
    if (RegGetValueW(HKEY_LOCAL_MACHINE, key, L"zapret-discord-youtube",
                     RRF_RT_REG_SZ, &type, nullptr, &bytes) != ERROR_SUCCESS || bytes < sizeof(wchar_t))
        return {};
    std::vector<wchar_t> buffer(bytes / sizeof(wchar_t) + 1, L'\0');
    if (RegGetValueW(HKEY_LOCAL_MACHINE, key, L"zapret-discord-youtube",
                     RRF_RT_REG_SZ, &type, buffer.data(), &bytes) != ERROR_SUCCESS)
        return {};
    return TrimWide(buffer.data());
}

bool RunElevated(const std::wstring& file,
                 const std::wstring& parameters,
                 const fs::path& workingDirectory,
                 bool wait,
                 DWORD timeoutMs,
                 std::wstring& error) {
    error.clear();
    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS;
    info.hwnd = nullptr;
    info.lpVerb = L"runas";
    info.lpFile = file.c_str();
    info.lpParameters = parameters.empty() ? nullptr : parameters.c_str();
    const std::wstring working = workingDirectory.wstring();
    info.lpDirectory = working.empty() ? nullptr : working.c_str();
    info.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&info)) {
        const DWORD code = GetLastError();
        if (code == ERROR_CANCELLED)
            error = L"Операция отменена пользователем в запросе контроля учётных записей.";
        else
            error = Win32Message(L"Не удалось запустить действие Zapret с правами администратора", code);
        return false;
    }

    if (!info.hProcess) {
        error = L"Windows не вернула дескриптор процесса Zapret.";
        return false;
    }

    if (!wait) {
        CloseHandle(info.hProcess);
        return true;
    }

    const DWORD waitResult = WaitForSingleObject(info.hProcess, timeoutMs);
    if (waitResult == WAIT_TIMEOUT) {
        CloseHandle(info.hProcess);
        error = L"Операция Zapret не завершилась за отведённое время. Процесс оставлен работать и не был принудительно завершён.";
        return false;
    }
    if (waitResult != WAIT_OBJECT_0) {
        const DWORD code = GetLastError();
        CloseHandle(info.hProcess);
        error = Win32Message(L"Не удалось дождаться завершения операции Zapret", code);
        return false;
    }

    DWORD exitCode = 0;
    const BOOL gotExitCode = GetExitCodeProcess(info.hProcess, &exitCode);
    CloseHandle(info.hProcess);
    if (!gotExitCode) {
        error = Win32Message(L"Не удалось получить код завершения операции Zapret");
        return false;
    }
    if (exitCode != 0) {
        error = L"Операция Zapret завершилась с кодом " + std::to_wstring(exitCode) + L".";
        return false;
    }
    return true;
}

fs::path TempWrapperPath(const wchar_t* action) {
    wchar_t tempPath[32768]{};
    const DWORD size = GetTempPathW(static_cast<DWORD>(std::size(tempPath)), tempPath);
    fs::path base = (size > 0 && size < std::size(tempPath)) ? fs::path(tempPath) : fs::temp_directory_path();
    return base / (std::wstring(L"DPopCleaner-Zapret-") + action + L"-" +
                   std::to_wstring(GetCurrentProcessId()) + L"-" +
                   std::to_wstring(GetTickCount64()) + L".cmd");
}

bool WriteUtf8File(const fs::path& path, const std::wstring& text, std::wstring& error) {
    const std::string bytes = WideToUtf8(text);
    if (bytes.empty()) {
        error = L"Не удалось подготовить временный сценарий Zapret.";
        return false;
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        error = L"Не удалось создать временный сценарий Zapret: " + path.wstring();
        return false;
    }
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.flush();
    if (!output) {
        error = L"Не удалось записать временный сценарий Zapret.";
        return false;
    }
    return true;
}

bool StopPidIfStillBundled(DWORD pid, const fs::path& root, std::wstring& error) {
    const fs::path verified = QueryProcessPath(pid);
    if (!IsBundledWinwsPath(verified, root)) return true;

    HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
    if (process) {
        const BOOL terminated = TerminateProcess(process, 0);
        if (terminated) WaitForSingleObject(process, 5000);
        CloseHandle(process);
        if (terminated) return true;
    }

    const fs::path reverified = QueryProcessPath(pid);
    if (!IsBundledWinwsPath(reverified, root)) return true;
    const std::wstring parameters = L"/PID " + std::to_wstring(pid) + L" /F";
    return RunElevated(L"taskkill.exe", parameters, root, true, 30000, error);
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
        {L"general.bat", false},
        {fs::path(L".service") / L"version.txt", false},
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

    std::wstring version;
    if (!ReadPinnedVersion(root, version) || version != kBundledZapretVersion) {
        error = L"Bundled Zapret должен быть строго версии " + std::wstring(kBundledZapretVersion) +
                L". Обнаружено: " + (version.empty() ? std::wstring(L"неизвестно") : version) + L".";
        return false;
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

bool ResolveBundledStrategy(const fs::path& root,
                            const std::wstring& selectedName,
                            ZapretStrategy& resolved,
                            std::wstring& error) {
    error.clear();
    resolved = {};
    if (selectedName.empty() || fs::path(selectedName).filename().wstring() != selectedName) {
        error = L"Недопустимое имя стратегии Zapret.";
        return false;
    }
    const auto strategies = EnumerateZapretStrategies(root);
    const std::wstring wanted = Lower(selectedName);
    for (const auto& strategy : strategies) {
        if (Lower(strategy.batchPath.filename().wstring()) != wanted) continue;
        if (NormalizedForCompare(strategy.batchPath.parent_path()) != NormalizedForCompare(root)) break;
        std::error_code ec;
        if (!fs::is_regular_file(strategy.batchPath, ec) || ec) break;
        resolved = strategy;
        return true;
    }
    error = L"Выбранная стратегия отсутствует в bundled Zapret: " + selectedName;
    return false;
}

bool IsBundledWinwsPath(const fs::path& candidate, const fs::path& root) {
    if (candidate.empty() || root.empty()) return false;
    return NormalizedForCompare(candidate) == NormalizedForCompare(root / L"bin" / L"winws.exe");
}

bool IsOwnedZapretServiceCommand(const std::wstring& commandLine, const fs::path& root) {
    const std::wstring executable = ExtractExecutable(commandLine);
    return !executable.empty() && IsBundledWinwsPath(fs::path(executable), root);
}

size_t FindStrategyMenuIndex(const std::vector<ZapretStrategy>& strategies, const std::wstring& selectedName) {
    const std::wstring wanted = Lower(fs::path(selectedName).filename().wstring());
    if (wanted.empty()) return 0;
    for (size_t i = 0; i < strategies.size(); ++i) {
        if (Lower(strategies[i].batchPath.filename().wstring()) == wanted) return i + 1;
    }
    return 0;
}

std::wstring BuildPinnedServiceInstallInput(const std::vector<ZapretStrategy>& strategies,
                                            const std::wstring& selectedName) {
    const size_t index = FindStrategyMenuIndex(strategies, selectedName);
    if (index == 0) return {};
    return L"1\r\n" + std::to_wstring(index) + L"\r\nx\r\n0\r\n";
}

ZapretStatus QueryZapretStatus(const fs::path& root) {
    ZapretStatus result{};
    std::error_code ec;
    result.payloadAvailable = fs::is_directory(root, ec) && !ec;
    std::wstring validationError;
    result.payloadIntegrityOk = result.payloadAvailable && ValidateBundledPayload(root, validationError);
    if (!result.payloadIntegrityOk) result.error = validationError;

    const ServiceSnapshot service = QueryServiceSnapshot(root);
    result.serviceInstalled = service.installed;
    result.serviceRunning = service.running;
    result.bundledServiceOwned = service.owned;
    if (service.installed && service.owned) result.serviceStrategy = ReadServiceStrategy();

    for (const auto& process : EnumerateWinwsProcesses()) {
        if (!process.imagePath.empty() && IsBundledWinwsPath(process.imagePath, root))
            result.bundledWinwsRunning = true;
        else
            result.externalWinwsRunning = true;
    }
    return result;
}

bool StartBundledZapret(const std::wstring& selectedStrategy,
                        const fs::path& root,
                        std::wstring& error) {
    if (!ValidateBundledPayload(root, error)) return false;
    ZapretStrategy strategy{};
    if (!ResolveBundledStrategy(root, selectedStrategy, strategy, error)) return false;

    const ZapretStatus status = QueryZapretStatus(root);
    if (status.serviceRunning) {
        error = status.bundledServiceOwned
                    ? L"Служба bundled Zapret уже запущена. Сначала остановите её."
                    : L"Обнаружена внешняя служба zapret. DPopCleaner не будет запускать второй экземпляр поверх неё.";
        return false;
    }
    if (status.externalWinwsRunning) {
        error = L"Обнаружен внешний winws.exe. DPopCleaner не запускает bundled Zapret поверх внешнего экземпляра.";
        return false;
    }
    if (status.bundledWinwsRunning) return true;

    const std::wstring parameters = L"/d /s /c \"\"" + strategy.batchPath.wstring() + L"\"\"";
    return RunElevated(L"cmd.exe", parameters, root, false, 0, error);
}

bool StopBundledZapret(const fs::path& root, std::wstring& error) {
    error.clear();
    const ServiceSnapshot service = QueryServiceSnapshot(root);
    if (service.running && service.owned) {
        if (!RunElevated(L"sc.exe", L"stop zapret", root, true, 30000, error)) return false;
    }

    for (const auto& process : EnumerateWinwsProcesses()) {
        if (!process.imagePath.empty() && IsBundledWinwsPath(process.imagePath, root)) {
            if (!StopPidIfStillBundled(process.pid, root, error)) return false;
        }
    }
    return true;
}

bool InstallBundledZapretService(const std::wstring& selectedStrategy,
                                 const fs::path& root,
                                 std::wstring& error) {
    if (!ValidateBundledPayload(root, error)) return false;
    ZapretStrategy strategy{};
    if (!ResolveBundledStrategy(root, selectedStrategy, strategy, error)) return false;

    const ServiceSnapshot existing = QueryServiceSnapshot(root);
    if (existing.installed && !existing.owned) {
        error = L"Служба с именем zapret уже существует, но не принадлежит bundled Zapret DPopCleaner. Она не будет заменена.";
        return false;
    }
    const ZapretStatus status = QueryZapretStatus(root);
    if (status.externalWinwsRunning) {
        error = L"Обнаружен внешний winws.exe. Остановите внешний Zapret перед установкой bundled-службы.";
        return false;
    }
    if (status.bundledWinwsRunning && !status.serviceRunning) {
        error = L"Сначала остановите standalone bundled Zapret, затем устанавливайте службу.";
        return false;
    }

    const auto strategies = EnumerateZapretStrategies(root);
    const size_t index = FindStrategyMenuIndex(strategies, strategy.batchPath.filename().wstring());
    if (index == 0) {
        error = L"Не удалось сопоставить выбранную стратегию с pinned меню Flowseal 1.10.2.";
        return false;
    }

    const fs::path wrapper = TempWrapperPath(L"install-service");
    const std::wstring script =
        L"@echo off\r\n"
        L"chcp 65001 >nul\r\n"
        L"set \"NO_UPDATE_CHECK=1\"\r\n"
        L"cd /d \"" + root.wstring() + L"\"\r\n"
        L"(\r\n"
        L"  echo 1\r\n"
        L"  echo " + std::to_wstring(index) + L"\r\n"
        L"  echo x\r\n"
        L"  echo 0\r\n"
        L") ^| call \"" + (root / L"service.bat").wstring() + L"\" admin\r\n"
        L"exit /b %errorlevel%\r\n";
    if (!WriteUtf8File(wrapper, script, error)) return false;

    const std::wstring parameters = L"/d /s /c \"\"" + wrapper.wstring() + L"\"\"";
    const bool launched = RunElevated(L"cmd.exe", parameters, root, true, 120000, error);
    if (launched) {
        std::error_code ec;
        fs::remove(wrapper, ec);
    }
    if (!launched) return false;

    const ServiceSnapshot installed = QueryServiceSnapshot(root);
    if (!installed.installed || !installed.owned) {
        error = L"Flowseal service.bat завершился, но owned-служба zapret не обнаружена.";
        return false;
    }
    return true;
}

bool RemoveBundledZapretService(const fs::path& root, std::wstring& error) {
    error.clear();
    const ServiceSnapshot existing = QueryServiceSnapshot(root);
    if (!existing.installed) return true;
    if (!existing.owned) {
        error = L"Существующая служба zapret не принадлежит bundled Zapret DPopCleaner и не будет удалена.";
        return false;
    }

    const fs::path wrapper = TempWrapperPath(L"remove-service");
    const std::wstring script =
        L"@echo off\r\n"
        L"sc stop zapret >nul 2>&1\r\n"
        L"for /l %%I in (1,1,20) do (\r\n"
        L"  sc query zapret | findstr /i \"STOPPED\" >nul && goto :delete_service\r\n"
        L"  timeout /t 1 /nobreak >nul\r\n"
        L")\r\n"
        L":delete_service\r\n"
        L"sc delete zapret >nul 2>&1\r\n"
        L"exit /b %errorlevel%\r\n";
    if (!WriteUtf8File(wrapper, script, error)) return false;

    const std::wstring parameters = L"/d /s /c \"\"" + wrapper.wstring() + L"\"\"";
    const bool removed = RunElevated(L"cmd.exe", parameters, root, true, 60000, error);
    if (removed) {
        std::error_code ec;
        fs::remove(wrapper, ec);
    }
    if (!removed) return false;

    return StopBundledZapret(root, error);
}

} // namespace dpop0418
