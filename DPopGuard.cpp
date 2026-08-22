#include "modules/DPopGuard.h"
#include "modules/StartupManager.h"
#include <windows.h>
#include <tlhelp32.h>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cwctype>
#include <vector>
#include <iterator>

namespace fs = std::filesystem;

namespace {
std::wstring Lower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c){ return static_cast<wchar_t>(std::towlower(c)); });
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
        PROCESSENTRY32W pe{}; pe.dwSize = sizeof(pe);
        if (Process32FirstW(snap, &pe)) {
            do {
                ++result.processesChecked;
                const std::wstring name = pe.szExeFile;
                const std::wstring path = ProcessPath(pe.th32ProcessID);
                if (ContainsAny(name, minerNames)) {
                    result.findings.push_back({L"Похоже на майнер", name + (path.empty()?L"":L"\n"+path), L"Высокий"});
                } else if (!path.empty()) {
                    const auto lp = Lower(path);
                    if ((lp.find(L"\\temp\\") != std::wstring::npos || lp.find(L"\\appdata\\local\\temp\\") != std::wstring::npos) &&
                        (ContainsAny(name, {L"update", L"system", L"service", L"host"}))) {
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

    result.note = L"DPopGuard 2: проверка процессов + persistence + miner heuristics. Файлы можно дополнительно проверить через AMSI. Результаты являются эвристикой, а не диагнозом.";
    return result;
}

bool ScanFileWithAmsi(const fs::path& file, std::wstring& verdict, std::wstring& error) {
    std::ifstream in(file, std::ios::binary);
    if (!in) { error = L"Не удалось открыть файл."; return false; }
    std::vector<char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (data.size() > 64ull * 1024ull * 1024ull) { error = L"Для быстрой AMSI-проверки выбери файл меньше 64 МБ."; return false; }

    HMODULE amsi = LoadLibraryW(L"amsi.dll");
    if (!amsi) { error = L"AMSI недоступен в этой системе."; return false; }
    auto init = reinterpret_cast<AmsiInitializeFn>(GetProcAddress(amsi, "AmsiInitialize"));
    auto uninit = reinterpret_cast<AmsiUninitializeFn>(GetProcAddress(amsi, "AmsiUninitialize"));
    auto scan = reinterpret_cast<AmsiScanBufferFn>(GetProcAddress(amsi, "AmsiScanBuffer"));
    if (!init || !uninit || !scan) { FreeLibrary(amsi); error = L"Не удалось загрузить функции AMSI."; return false; }

    void* context = nullptr;
    if (FAILED(init(L"DPopCleaner DPopGuard 2", &context)) || !context) { FreeLibrary(amsi); error = L"AmsiInitialize завершился ошибкой."; return false; }
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
}
