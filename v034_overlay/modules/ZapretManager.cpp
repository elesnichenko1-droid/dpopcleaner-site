#include "modules/ZapretManager.h"
#include "core/Paths.h"
#include "modules/ZapretPolicy.h"
#include "update/Hash.h"

#include <windows.h>
#include <tlhelp32.h>
#include <winsvc.h>
#include <shellapi.h>
#include <winhttp.h>

#include <algorithm>
#include <cctype>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace {
constexpr wchar_t kZapretLatestApi[] = L"https://api.github.com/repos/Flowseal/zapret-discord-youtube/releases/latest";
constexpr wchar_t kZapretLatestPage[] = L"https://github.com/Flowseal/zapret-discord-youtube/releases/latest";
constexpr wchar_t kBundleVersionFile[] = L"DPopCleanerBundleVersion.txt";

struct InternetHandle {
    HINTERNET value{};
    ~InternetHandle() { if (value) WinHttpCloseHandle(value); }
};

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

std::wstring Utf8ToWide(std::string_view input) {
    if (input.empty()) return {};
    const int needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(), static_cast<int>(input.size()), nullptr, 0);
    if (needed <= 0) return {};
    std::wstring out(static_cast<std::size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(), static_cast<int>(input.size()), out.data(), needed);
    return out;
}

std::string WideToUtf8(std::wstring_view input) {
    if (input.empty()) return {};
    const int needed = WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};
    std::string out(static_cast<std::size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), out.data(), needed, nullptr, nullptr);
    return out;
}

std::wstring Lower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
    return value;
}

std::wstring NormalizeVersion(std::wstring value) {
    while (!value.empty() && iswspace(value.front())) value.erase(value.begin());
    while (!value.empty() && iswspace(value.back())) value.pop_back();
    if (!value.empty() && (value.front() == L'v' || value.front() == L'V')) value.erase(value.begin());
    return Lower(value);
}

bool ParseHttpsUrl(const std::wstring& url, std::wstring& host, std::wstring& path, INTERNET_PORT& port, std::wstring& error) {
    wchar_t hostBuf[2048]{};
    wchar_t pathBuf[8192]{};
    URL_COMPONENTSW parts{};
    parts.dwStructSize = sizeof(parts);
    parts.lpszHostName = hostBuf;
    parts.dwHostNameLength = static_cast<DWORD>(std::size(hostBuf));
    parts.lpszUrlPath = pathBuf;
    parts.dwUrlPathLength = static_cast<DWORD>(std::size(pathBuf));
    if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &parts)) {
        error = L"Некорректный URL обновления Zapret.";
        return false;
    }
    if (parts.nScheme != INTERNET_SCHEME_HTTPS) {
        error = L"Обновление Zapret разрешено только по HTTPS.";
        return false;
    }
    host.assign(hostBuf, parts.dwHostNameLength);
    path.assign(pathBuf, parts.dwUrlPathLength);
    if (parts.lpszExtraInfo && parts.dwExtraInfoLength) path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    if (path.empty()) path = L"/";
    port = parts.nPort;
    return true;
}

bool HttpGetText(const std::wstring& url, std::string& body, std::wstring& error) {
    std::wstring host, path;
    INTERNET_PORT port{};
    if (!ParseHttpsUrl(url, host, path, port, error)) return false;
    InternetHandle session{WinHttpOpen(L"DPopCleaner/0.3.4 ZapretUpdater", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
    if (!session.value) { error = L"WinHttpOpen failed."; return false; }
    WinHttpSetTimeouts(session.value, 10000, 10000, 20000, 30000);
    InternetHandle connection{WinHttpConnect(session.value, host.c_str(), port, 0)};
    if (!connection.value) { error = L"WinHttpConnect failed."; return false; }
    InternetHandle request{WinHttpOpenRequest(connection.value, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)};
    if (!request.value) { error = L"WinHttpOpenRequest failed."; return false; }
    constexpr wchar_t headers[] = L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
    if (!WinHttpSendRequest(request.value, headers, static_cast<DWORD>(-1L), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.value, nullptr)) {
        error = L"Не удалось получить latest release Zapret.";
        return false;
    }
    DWORD status = 0, bytes = sizeof(status);
    WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &bytes, nullptr);
    if (status < 200 || status >= 300) { error = L"GitHub API вернул HTTP " + std::to_wstring(status); return false; }
    body.clear();
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.value, &available)) { error = L"Ошибка чтения GitHub API."; return false; }
        if (!available) break;
        const auto offset = body.size();
        body.resize(offset + available);
        DWORD read = 0;
        if (!WinHttpReadData(request.value, body.data() + offset, available, &read)) { error = L"Ошибка чтения GitHub API."; return false; }
        body.resize(offset + read);
    }
    return true;
}

std::optional<std::string> JsonString(std::string_view object, std::string_view key) {
    const std::string marker = "\"" + std::string(key) + "\"";
    const auto keyPos = object.find(marker);
    if (keyPos == std::string_view::npos) return std::nullopt;
    const auto colon = object.find(':', keyPos + marker.size());
    if (colon == std::string_view::npos) return std::nullopt;
    const auto quote = object.find('"', colon + 1);
    if (quote == std::string_view::npos) return std::nullopt;
    std::string value;
    bool escaped = false;
    for (std::size_t i = quote + 1; i < object.size(); ++i) {
        const char c = object[i];
        if (escaped) {
            switch (c) {
            case 'n': value.push_back('\n'); break;
            case 'r': value.push_back('\r'); break;
            case 't': value.push_back('\t'); break;
            default: value.push_back(c); break;
            }
            escaped = false;
            continue;
        }
        if (c == '\\') { escaped = true; continue; }
        if (c == '"') return value;
        value.push_back(c);
    }
    return std::nullopt;
}

std::optional<std::uint64_t> JsonUInt(std::string_view object, std::string_view key) {
    const std::string marker = "\"" + std::string(key) + "\"";
    const auto keyPos = object.find(marker);
    if (keyPos == std::string_view::npos) return std::nullopt;
    const auto colon = object.find(':', keyPos + marker.size());
    if (colon == std::string_view::npos) return std::nullopt;
    auto begin = object.find_first_of("0123456789", colon + 1);
    if (begin == std::string_view::npos) return std::nullopt;
    auto end = object.find_first_not_of("0123456789", begin);
    try { return static_cast<std::uint64_t>(std::stoull(std::string(object.substr(begin, end - begin)))); }
    catch (...) { return std::nullopt; }
}

std::vector<std::string_view> AssetObjects(const std::string& json) {
    std::vector<std::string_view> result;
    const auto assetsKey = json.find("\"assets\"");
    if (assetsKey == std::string::npos) return result;
    const auto arrayStart = json.find('[', assetsKey);
    if (arrayStart == std::string::npos) return result;
    bool inString = false, escaped = false;
    int objectDepth = 0;
    std::size_t objectStart = std::string::npos;
    for (std::size_t i = arrayStart + 1; i < json.size(); ++i) {
        const char c = json[i];
        if (inString) {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') inString = false;
            continue;
        }
        if (c == '"') { inString = true; continue; }
        if (c == '{') {
            if (objectDepth == 0) objectStart = i;
            ++objectDepth;
        } else if (c == '}') {
            if (objectDepth > 0) --objectDepth;
            if (objectDepth == 0 && objectStart != std::string::npos) {
                result.emplace_back(json.data() + objectStart, i - objectStart + 1);
                objectStart = std::string::npos;
            }
        } else if (c == ']' && objectDepth == 0) break;
    }
    return result;
}

bool EndsWithZip(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const auto query = value.find('?');
    if (query != std::string::npos) value.resize(query);
    return value.size() >= 4 && value.ends_with(".zip");
}

std::wstring ReadBundleVersion(const fs::path& root) {
    std::ifstream input(root / kBundleVersionFile, std::ios::binary);
    if (!input) return L"неизвестно";
    std::string line;
    std::getline(input, line);
    const auto wide = Utf8ToWide(line);
    return wide.empty() ? L"неизвестно" : wide;
}

bool DownloadVerifiedZip(const dpop::zapret::UpdateInfo& info, fs::path& zipPath, std::wstring& error) {
    std::wstring host, path;
    INTERNET_PORT port{};
    if (!ParseHttpsUrl(info.downloadUrl, host, path, port, error)) return false;
    dpop::paths::EnsureDirectories();
    zipPath = dpop::paths::UpdatesDir() / L"zapret-latest.zip";
    const fs::path part = zipPath.wstring() + L".part";
    std::error_code ec;
    fs::remove(part, ec); ec.clear();
    fs::remove(zipPath, ec); ec.clear();

    InternetHandle session{WinHttpOpen(L"DPopCleaner/0.3.4 ZapretUpdater", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
    if (!session.value) { error = L"WinHttpOpen failed."; return false; }
    InternetHandle connection{WinHttpConnect(session.value, host.c_str(), port, 0)};
    if (!connection.value) { error = L"WinHttpConnect failed."; return false; }
    InternetHandle request{WinHttpOpenRequest(connection.value, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)};
    if (!request.value || !WinHttpSendRequest(request.value, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(request.value, nullptr)) {
        error = L"Не удалось скачать release asset Zapret.";
        return false;
    }
    DWORD status = 0, headerBytes = sizeof(status);
    WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &headerBytes, nullptr);
    if (status < 200 || status >= 300) { error = L"Скачивание Zapret вернуло HTTP " + std::to_wstring(status); return false; }

    HANDLE output = CreateFileW(part.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (output == INVALID_HANDLE_VALUE) { error = L"Не удалось создать staging-файл Zapret."; return false; }
    bool ok = true;
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.value, &available)) { ok = false; break; }
        if (!available) break;
        std::vector<char> buffer(available);
        DWORD read = 0;
        if (!WinHttpReadData(request.value, buffer.data(), available, &read)) { ok = false; break; }
        DWORD written = 0;
        if (!WriteFile(output, buffer.data(), read, &written, nullptr) || written != read) { ok = false; break; }
    }
    FlushFileBuffers(output);
    CloseHandle(output);
    if (!ok) { DeleteFileW(part.c_str()); error = L"Загрузка Zapret прервана."; return false; }

    const auto actualSize = fs::file_size(part, ec);
    if (ec || actualSize != info.size) {
        fs::remove(part, ec);
        error = L"Размер архива Zapret не совпал с GitHub release metadata.";
        return false;
    }
    std::wstring actualHash;
    if (!dpop::update::Sha256File(part, actualHash, error)) { fs::remove(part, ec); return false; }
    if (Lower(actualHash) != Lower(info.sha256)) {
        fs::remove(part, ec);
        error = L"SHA-256 архива Zapret не совпал с GitHub release digest. Архив удалён.";
        return false;
    }
    fs::rename(part, zipPath, ec);
    if (ec) { error = L"Не удалось завершить staging-файл Zapret: " + std::to_wstring(ec.value()); return false; }
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

std::optional<fs::path> FindValidatedReleaseRoot(const fs::path& staging) {
    std::error_code ec;
    for (fs::recursive_directory_iterator it(staging, fs::directory_options::skip_permission_denied, ec), end;
         it != end; it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (it->is_symlink(ec)) return std::nullopt;
        if (!it->is_regular_file(ec)) continue;
        if (_wcsicmp(it->path().filename().c_str(), L"service.bat") != 0) continue;
        const fs::path candidate = it->path().parent_path();
        if (dpop::zapret::ValidateBundle(candidate).valid) return candidate;
    }
    return std::nullopt;
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
            if (QueryServiceStatus(service, &serviceStatus)) status.serviceRunning = serviceStatus.dwCurrentState == SERVICE_RUNNING;
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
                if (IsBundledWinwsPath(status.bundleFolder, path)) { status.winwsRunning = true; break; }
            } while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
    }
    return status;
}

std::vector<StrategyEntry> EnumerateStrategies() { return EnumerateStrategiesAt(BundleRoot()); }

bool LaunchStrategy(const fs::path& relativeScript, std::wstring& error) {
    if (!IsLaunchableStrategyPath(relativeScript)) { error = L"Выбран небезопасный или неподдерживаемый путь стратегии Zapret."; return false; }
    return LaunchBatch(relativeScript, error);
}

bool LaunchDefaultStrategy(std::wstring& error) { return LaunchStrategy(L"general.bat", error); }

bool StopBundledWinws(std::wstring& error) {
    const auto status = QueryStatus();
    if (status.serviceRunning) { error = L"Zapret запущен как Windows-служба. Для корректной остановки открой Service Manager."; return false; }
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) { error = L"Не удалось получить список процессов Windows."; return false; }
    bool failed = false;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, L"winws.exe") != 0) continue;
            const auto path = ProcessPath(entry.th32ProcessID);
            if (!IsBundledWinwsPath(status.bundleFolder, path)) continue;
            HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, entry.th32ProcessID);
            if (!process) { failed = true; continue; }
            if (!TerminateProcess(process, 0)) failed = true; else WaitForSingleObject(process, 3000);
            CloseHandle(process);
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    if (failed) { error = L"Не удалось остановить один или несколько bundled winws.exe. Возможно, нужны права администратора."; return false; }
    error.clear();
    return true;
}

bool OpenServiceManager(std::wstring& error) { return LaunchBatch(L"service.bat", error); }

bool OpenBundledFolder(std::wstring& error) {
    fs::path root;
    if (!RequireValidBundle(root, error)) return false;
    if (reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", root.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) <= 32) {
        error = L"Не удалось открыть папку Zapret."; return false;
    }
    error.clear();
    return true;
}

bool RepairRtc(const fs::path& relativeScript, std::wstring& report) {
    if (!IsLaunchableStrategyPath(relativeScript)) { report = L"Выбран небезопасный или неподдерживаемый путь стратегии Zapret."; return false; }
    const auto before = QueryStatus();
    if (!before.bundleValid) { report = L"Сначала восстановите bundle Zapret: отсутствует " + before.missingBundleFile.wstring(); return false; }
    if (before.serviceRunning) {
        report = L"Zapret сейчас работает как Windows-служба. RTC repair не перезапускает службу автоматически: откройте Service Manager, остановите службу и повторите действие.";
        return false;
    }
    std::wstring stopError;
    if (before.winwsRunning && !StopBundledWinws(stopError)) { report = L"Не удалось безопасно остановить bundled winws: " + stopError; return false; }
    DWORD dnsCode = 0;
    std::wstring dnsError;
    if (!RunHiddenAndWait(L"ipconfig.exe", L"/flushdns", 15000, dnsCode, dnsError)) {
        report = L"Bundled winws остановлен, но DNS-кэш не очищен: " + dnsError; return false;
    }
    Sleep(700);
    std::wstring launchError;
    if (!LaunchStrategy(relativeScript, launchError)) { report = L"DNS-кэш очищен, но стратегия Zapret не перезапустилась: " + launchError; return false; }
    report = L"RTC repair выполнен: standalone bundled winws остановлен (если был запущен), DNS-кэш очищен и выбранная стратегия запущена заново. Если Discord всё ещё висит на RTC, полностью перезапустите Discord.";
    return true;
}

UpdateInfo CheckForZapretUpdate() {
    UpdateInfo info{};
    info.currentVersion = ReadBundleVersion(BundleRoot());
    std::string json;
    if (!HttpGetText(kZapretLatestApi, json, info.error)) return info;
    const auto tag = JsonString(json, "tag_name");
    if (!tag || tag->empty()) { info.error = L"GitHub latest release не содержит tag_name."; return info; }
    info.latestVersion = Utf8ToWide(*tag);

    for (const auto asset : AssetObjects(json)) {
        const auto url = JsonString(asset, "browser_download_url");
        if (!url || !EndsWithZip(*url)) continue;
        const auto size = JsonUInt(asset, "size");
        const auto digest = JsonString(asset, "digest");
        if (!size || !digest || digest->rfind("sha256:", 0) != 0 || digest->size() != 71) continue;
        info.downloadUrl = Utf8ToWide(*url);
        info.size = *size;
        info.sha256 = Utf8ToWide(digest->substr(7));
        break;
    }
    if (info.downloadUrl.empty() || info.sha256.empty() || info.size == 0) {
        info.error = L"GitHub release не предоставил ZIP asset с проверяемым SHA-256 digest. Автообновление остановлено.";
        return info;
    }
    info.success = true;
    info.updateAvailable = info.currentVersion == L"неизвестно" || NormalizeVersion(info.currentVersion) != NormalizeVersion(info.latestVersion);
    return info;
}

bool UpdateBundledZapret(std::wstring& report) {
    const UpdateInfo info = CheckForZapretUpdate();
    if (!info.success) { report = info.error; return false; }
    if (!info.updateAvailable) { report = L"Zapret уже актуален: " + info.currentVersion; return true; }

    const auto status = QueryStatus();
    if (status.serviceRunning) {
        report = L"Перед обновлением остановите Windows-службу Zapret через Service Manager. DPopCleaner не заменяет файлы активной службы.";
        return false;
    }
    std::wstring stopError;
    if (status.winwsRunning && !StopBundledWinws(stopError)) { report = L"Не удалось остановить bundled winws перед обновлением: " + stopError; return false; }

    fs::path archive;
    std::wstring error;
    if (!DownloadVerifiedZip(info, archive, error)) { report = error; return false; }

    dpop::paths::EnsureDirectories();
    const fs::path staging = dpop::paths::UpdatesDir() / (L"zapret-staging-" + std::to_wstring(GetCurrentProcessId()));
    std::error_code ec;
    fs::remove_all(staging, ec); ec.clear();
    fs::create_directories(staging, ec);
    if (ec) { report = L"Не удалось создать staging для Zapret: " + std::to_wstring(ec.value()); return false; }
    const std::wstring tarArgs = L"-xf \"" + archive.wstring() + L"\" -C \"" + staging.wstring() + L"\"";
    DWORD tarCode = 0;
    if (!RunHiddenAndWait(L"tar.exe", tarArgs.c_str(), 60000, tarCode, error)) {
        fs::remove_all(staging, ec);
        report = L"Не удалось распаковать проверенный архив Zapret: " + error;
        return false;
    }
    const auto releaseRoot = FindValidatedReleaseRoot(staging);
    if (!releaseRoot) {
        fs::remove_all(staging, ec);
        report = L"Распакованный release Zapret не прошёл проверку структуры (service.bat/general*.bat/winws).";
        return false;
    }

    fs::path root = BundleRoot();
    if (root.empty()) root = dpop::paths::ExecutableDir() / L"zapret";
    fs::path backup = root;
    backup += L".backup";
    fs::remove_all(backup, ec); ec.clear();
    const bool hadOld = fs::exists(root, ec);
    ec.clear();
    if (hadOld) {
        fs::rename(root, backup, ec);
        if (ec) {
            fs::remove_all(staging, ec);
            report = L"Не удалось создать резервную копию текущего Zapret. Запустите DPopCleaner от администратора. Код: " + std::to_wstring(ec.value());
            return false;
        }
    }

    fs::create_directories(root.parent_path(), ec); ec.clear();
    fs::copy(*releaseRoot, root, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    if (ec || !ValidateBundle(root).valid) {
        const int copyError = ec.value();
        fs::remove_all(root, ec); ec.clear();
        if (hadOld) fs::rename(backup, root, ec);
        fs::remove_all(staging, ec);
        report = L"Новый Zapret не удалось установить или проверить. Старая версия восстановлена. Код: " + std::to_wstring(copyError);
        return false;
    }

    std::ofstream marker(root / kBundleVersionFile, std::ios::binary | std::ios::trunc);
    marker << WideToUtf8(info.latestVersion) << "\n";
    marker.close();
    if (!marker) {
        fs::remove_all(root, ec); ec.clear();
        if (hadOld) fs::rename(backup, root, ec);
        fs::remove_all(staging, ec);
        report = L"Не удалось записать version marker Zapret. Старая версия восстановлена.";
        return false;
    }

    fs::remove_all(backup, ec); ec.clear();
    fs::remove_all(staging, ec); ec.clear();
    fs::remove(archive, ec);
    report = L"Zapret обновлён до " + info.latestVersion + L". Архив был проверен по размеру и SHA-256 digest GitHub, bundle повторно проверен после установки.";
    return true;
}

bool OpenZapretUpdatePage(std::wstring& error) {
    const auto code = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", kZapretLatestPage, nullptr, nullptr, SW_SHOWNORMAL));
    if (code <= 32) { error = L"Не удалось открыть страницу обновлений Zapret."; return false; }
    error.clear();
    return true;
}

}
