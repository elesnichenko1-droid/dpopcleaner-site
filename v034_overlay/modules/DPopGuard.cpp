#include "modules/DPopGuard.h"
#include "modules/StartupManager.h"

#include <windows.h>
#include <winsvc.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::wstring Lower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(std::towlower(c));
    });
    return s;
}

bool ContainsAny(const std::wstring& text, const std::vector<std::wstring>& needles) {
    const auto lower = Lower(text);
    for (const auto& n : needles) if (lower.find(n) != std::wstring::npos) return true;
    return false;
}

std::wstring ProcessPath(DWORD pid) {
    HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!p) return {};
    std::wstring path(32768, L'\0');
    DWORD size = static_cast<DWORD>(path.size());
    if (!QueryFullProcessImageNameW(p, 0, path.data(), &size)) path.clear();
    else path.resize(size);
    CloseHandle(p);
    return path;
}

std::wstring Environment(std::wstring_view name) {
    const DWORD needed = GetEnvironmentVariableW(std::wstring(name).c_str(), nullptr, 0);
    if (!needed) return {};
    std::wstring value(needed, L'\0');
    GetEnvironmentVariableW(std::wstring(name).c_str(), value.data(), needed);
    while (!value.empty() && value.back() == L'\0') value.pop_back();
    return value;
}

fs::path FindDefenderCli() {
    std::error_code ec;
    const auto programData = Environment(L"ProgramData");
    if (!programData.empty()) {
        const fs::path platform = fs::path(programData) / L"Microsoft/Windows Defender/Platform";
        fs::path newest;
        fs::file_time_type newestTime{};
        bool haveTime = false;
        if (fs::is_directory(platform, ec)) {
            for (fs::directory_iterator it(platform, fs::directory_options::skip_permission_denied, ec), end;
                 it != end; it.increment(ec)) {
                if (ec) { ec.clear(); continue; }
                const fs::path candidate = it->path() / L"MpCmdRun.exe";
                if (!fs::is_regular_file(candidate, ec)) { ec.clear(); continue; }
                const auto time = fs::last_write_time(candidate, ec);
                if (ec) { ec.clear(); continue; }
                if (!haveTime || time > newestTime) {
                    newest = candidate;
                    newestTime = time;
                    haveTime = true;
                }
            }
        }
        if (!newest.empty()) return newest;
    }

    const auto programFiles = Environment(L"ProgramFiles");
    if (!programFiles.empty()) {
        const fs::path legacy = fs::path(programFiles) / L"Windows Defender/MpCmdRun.exe";
        if (fs::is_regular_file(legacy, ec)) return legacy;
    }
    return {};
}

bool IsWinDefendRunning() {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return false;
    SC_HANDLE service = OpenServiceW(scm, L"WinDefend", SERVICE_QUERY_STATUS);
    if (!service) { CloseServiceHandle(scm); return false; }
    SERVICE_STATUS_PROCESS status{};
    DWORD bytes = 0;
    const bool ok = QueryServiceStatusEx(
        service, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&status), sizeof(status), &bytes) != FALSE;
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return ok && status.dwCurrentState == SERVICE_RUNNING;
}

std::wstring Quote(std::wstring_view value) {
    std::wstring result = L"\"";
    result.append(value);
    result += L"\"";
    return result;
}

dpop::guard::DefenderScanResult RunDefender(const std::wstring& arguments) {
    dpop::guard::DefenderScanResult out{};
    const fs::path cli = FindDefenderCli();
    if (cli.empty()) {
        out.message = L"Microsoft Defender CLI (MpCmdRun.exe) не найден.";
        return out;
    }

    std::wstring command = Quote(cli.wstring()) + L" " + arguments;
    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    const BOOL created = CreateProcessW(
        cli.c_str(), mutableCommand.data(), nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, cli.parent_path().c_str(), &si, &pi);
    if (!created) {
        out.message = L"Не удалось запустить Microsoft Defender. Код Windows: " + std::to_wstring(GetLastError());
        return out;
    }

    out.started = true;
    const DWORD wait = WaitForSingleObject(pi.hProcess, 30u * 60u * 1000u);
    if (wait == WAIT_OBJECT_0) {
        DWORD code = 0;
        if (GetExitCodeProcess(pi.hProcess, &code)) {
            out.completed = true;
            out.exitCode = static_cast<unsigned long>(code);
            out.message = L"Microsoft Defender завершил проверку. Код провайдера: " + std::to_wstring(code) +
                L". Подробности обнаружений смотрите в Windows Security → Protection history.";
        } else {
            out.message = L"Microsoft Defender завершился, но DPopCleaner не смог получить код результата.";
        }
    } else if (wait == WAIT_TIMEOUT) {
        out.message = L"Microsoft Defender продолжает длительную проверку в фоне. DPopCleaner не прерывал системный антивирус.";
    } else {
        out.message = L"Ошибка ожидания Microsoft Defender. Код Windows: " + std::to_wstring(GetLastError());
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return out;
}

using AmsiInitializeFn = HRESULT (WINAPI*)(LPCWSTR, void**);
using AmsiUninitializeFn = void (WINAPI*)(void*);
using AmsiScanBufferFn = HRESULT (WINAPI*)(void*, PVOID, ULONG, LPCWSTR, void*, int*);

}

namespace dpop::guard {

ScanResult QuickScan() {
    ScanResult result{};
    const std::vector<std::wstring> minerNames = {
        L"xmrig", L"t-rex", L"trex", L"phoenixminer", L"gminer", L"teamredminer", L"nbminer", L"lolminer"
    };

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe{};
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(snap, &pe)) {
            do {
                ++result.processesChecked;
                const std::wstring name = pe.szExeFile;
                const std::wstring path = ProcessPath(pe.th32ProcessID);
                if (ContainsAny(name, minerNames)) {
                    result.findings.push_back({L"Похоже на майнер", name + (path.empty() ? L"" : L"\n" + path), L"Высокий"});
                } else if (!path.empty()) {
                    const auto lp = Lower(path);
                    if ((lp.find(L"\\temp\\") != std::wstring::npos || lp.find(L"\\appdata\\local\\temp\\") != std::wstring::npos) &&
                        ContainsAny(name, {L"update", L"system", L"service", L"host"})) {
                        result.findings.push_back({L"Подозрительный процесс из TEMP", name + L"\n" + path, L"Средний"});
                    }
                }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
    }

    const auto startup = dpop::startup::EnumerateAll();
    result.startupChecked = static_cast<unsigned>(startup.size());
    for (const auto& e : startup) {
        const auto lc = Lower(e.command);
        if (lc.find(L"\\temp\\") != std::wstring::npos || lc.find(L"\\appdata\\local\\temp\\") != std::wstring::npos) {
            result.findings.push_back({L"Автозапуск из временной папки", e.name + L"\n" + e.command, L"Средний"});
        }
        if (ContainsAny(e.command, minerNames)) {
            result.findings.push_back({L"Майнер в автозагрузке", e.name + L"\n" + e.command, L"Высокий"});
        }
    }

    const auto defender = QueryDefenderStatus();
    result.note = defender.cliAvailable
        ? L"DPopGuard: процессы + persistence + miner heuristics. AMSI и Microsoft Defender доступны для файлов/системного сканирования."
        : L"DPopGuard: процессы + persistence + miner heuristics. Файлы проверяются через AMSI; Microsoft Defender CLI не найден.";
    return result;
}

bool ScanFileWithAmsi(const fs::path& file, std::wstring& verdict, std::wstring& error) {
    std::ifstream in(file, std::ios::binary);
    if (!in) { error = L"Не удалось открыть файл."; return false; }
    std::vector<char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (data.size() > 64ull * 1024ull * 1024ull) {
        error = L"Для AMSI-проверки выберите файл меньше 64 МБ; крупный файл можно передать Microsoft Defender.";
        return false;
    }

    HMODULE amsi = LoadLibraryW(L"amsi.dll");
    if (!amsi) { error = L"AMSI недоступен в этой системе."; return false; }
    auto init = reinterpret_cast<AmsiInitializeFn>(GetProcAddress(amsi, "AmsiInitialize"));
    auto uninit = reinterpret_cast<AmsiUninitializeFn>(GetProcAddress(amsi, "AmsiUninitialize"));
    auto scan = reinterpret_cast<AmsiScanBufferFn>(GetProcAddress(amsi, "AmsiScanBuffer"));
    if (!init || !uninit || !scan) {
        FreeLibrary(amsi);
        error = L"Не удалось загрузить функции AMSI.";
        return false;
    }

    void* context = nullptr;
    if (FAILED(init(L"DPopCleaner DPopGuard R2", &context)) || !context) {
        FreeLibrary(amsi);
        error = L"AmsiInitialize завершился ошибкой.";
        return false;
    }
    int result = 0;
    const HRESULT hr = scan(context, data.data(), static_cast<ULONG>(data.size()), file.filename().c_str(), nullptr, &result);
    uninit(context);
    FreeLibrary(amsi);
    if (FAILED(hr)) { error = L"AMSI не смог проверить файл."; return false; }

    if (result >= 32768) verdict = L"AMSI: обнаружена угроза или потенциально нежелательный объект.";
    else if (result >= 1) verdict = L"AMSI: файл помечен как подозрительный.";
    else verdict = L"AMSI: обнаружений нет.";
    return true;
}

DefenderStatus QueryDefenderStatus() {
    DefenderStatus out{};
    out.cliPath = FindDefenderCli();
    out.cliAvailable = !out.cliPath.empty();
    out.serviceRunning = IsWinDefendRunning();
    return out;
}

DefenderScanResult RunDefenderQuickScan() {
    return RunDefender(L"-Scan -ScanType 1");
}

DefenderScanResult RunDefenderCustomScan(const fs::path& path) {
    std::error_code ec;
    if (path.empty() || !fs::exists(path, ec)) {
        DefenderScanResult out{};
        out.message = L"Файл или папка для Defender-проверки не найдены.";
        return out;
    }
    return RunDefender(L"-Scan -ScanType 3 -File " + Quote(path.wstring()) + L" -DisableRemediation");
}

}
