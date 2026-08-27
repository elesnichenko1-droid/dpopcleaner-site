#include "UpdateClient.h"

#include "Hash.h"
#include "Signature.h"
#include "UpdatePolicy.h"
#include "Version.h"

#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>

#include <algorithm>
#include <atomic>
#include <cwctype>
#include <filesystem>
#include <string>
#include <vector>

namespace dpop0418 {
namespace {

struct InternetHandle {
    HINTERNET value{};
    ~InternetHandle() { if (value) WinHttpCloseHandle(value); }
    InternetHandle() = default;
    explicit InternetHandle(HINTERNET handle) : value(handle) {}
    InternetHandle(const InternetHandle&) = delete;
    InternetHandle& operator=(const InternetHandle&) = delete;
};

struct FileHandle {
    HANDLE value{INVALID_HANDLE_VALUE};
    ~FileHandle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    FileHandle() = default;
    explicit FileHandle(HANDLE handle) : value(handle) {}
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;
};

bool IsCancelled(const std::atomic_bool* shutdown) {
    return shutdown && shutdown->load(std::memory_order_acquire);
}

int TestSlowDelayMs() {
    wchar_t value[32]{};
    const DWORD length = GetEnvironmentVariableW(L"DPOP0418_TEST_SLOW_UPDATE_MS", value,
                                                   static_cast<DWORD>(std::size(value)));
    if (length == 0 || length >= std::size(value)) return 0;
    wchar_t* end = nullptr;
    const long parsed = wcstol(value, &end, 10);
    if (end == value || parsed <= 0 || parsed > 120000) return 0;
    return static_cast<int>(parsed);
}

bool DelayForTestIfRequested(const std::atomic_bool* shutdown, std::wstring& error) {
    int remaining = TestSlowDelayMs();
    while (remaining > 0) {
        if (IsCancelled(shutdown)) {
            error = L"Проверка обновлений отменена.";
            return false;
        }
        const DWORD slice = static_cast<DWORD>(std::min(remaining, 25));
        Sleep(slice);
        remaining -= static_cast<int>(slice);
    }
    return true;
}

bool ParseUrl(const std::wstring& url,
              std::wstring& host,
              std::wstring& path,
              INTERNET_PORT& port,
              bool& secure,
              std::wstring& error) {
    wchar_t hostBuffer[2048]{};
    wchar_t pathBuffer[8192]{};
    wchar_t extraBuffer[4096]{};
    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.lpszHostName = hostBuffer;
    components.dwHostNameLength = static_cast<DWORD>(std::size(hostBuffer));
    components.lpszUrlPath = pathBuffer;
    components.dwUrlPathLength = static_cast<DWORD>(std::size(pathBuffer));
    components.lpszExtraInfo = extraBuffer;
    components.dwExtraInfoLength = static_cast<DWORD>(std::size(extraBuffer));
    if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &components)) {
        error = L"Некорректный URL обновления.";
        return false;
    }
    host.assign(hostBuffer, components.dwHostNameLength);
    path.assign(pathBuffer, components.dwUrlPathLength);
    if (components.dwExtraInfoLength > 0) path.append(extraBuffer, components.dwExtraInfoLength);
    if (path.empty()) path = L"/";
    port = components.nPort;
    secure = components.nScheme == INTERNET_SCHEME_HTTPS;
    return true;
}

bool OpenGetRequest(const std::wstring& url,
                    InternetHandle& session,
                    InternetHandle& connection,
                    InternetHandle& request,
                    std::wstring& error) {
    std::wstring host;
    std::wstring path;
    INTERNET_PORT port{};
    bool secure = false;
    if (!ParseUrl(url, host, path, port, secure, error)) return false;
    if (!secure) {
        error = L"Обновления разрешены только по HTTPS.";
        return false;
    }

    session.value = WinHttpOpen(L"DPopCleaner/0.4.18",
                                WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                WINHTTP_NO_PROXY_NAME,
                                WINHTTP_NO_PROXY_BYPASS,
                                0);
    if (!session.value) {
        error = L"Не удалось открыть WinHTTP-сессию.";
        return false;
    }
    WinHttpSetTimeouts(session.value, 5000, 5000, 10000, 10000);

    connection.value = WinHttpConnect(session.value, host.c_str(), port, 0);
    if (!connection.value) {
        error = L"Не удалось подключиться к серверу обновлений.";
        return false;
    }

    request.value = WinHttpOpenRequest(connection.value, L"GET", path.c_str(), nullptr,
                                       WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                       WINHTTP_FLAG_SECURE);
    if (!request.value) {
        error = L"Не удалось создать HTTP-запрос обновления.";
        return false;
    }

    if (!WinHttpSendRequest(request.value, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.value, nullptr)) {
        error = L"Не удалось получить ответ сервера обновлений.";
        return false;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    if (!WinHttpQueryHeaders(request.value,
                             WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             nullptr, &status, &statusSize, nullptr) ||
        status < 200 || status >= 300) {
        error = L"Сервер обновлений вернул HTTP " + std::to_wstring(status) + L".";
        return false;
    }
    return true;
}

bool HttpGetUtf8(const std::wstring& url,
                 std::string& body,
                 std::wstring& error,
                 const std::atomic_bool* shutdown) {
    InternetHandle session;
    InternetHandle connection;
    InternetHandle request;
    if (IsCancelled(shutdown)) {
        error = L"Проверка обновлений отменена.";
        return false;
    }
    if (!OpenGetRequest(url, session, connection, request, error)) return false;

    body.clear();
    for (;;) {
        if (IsCancelled(shutdown)) {
            error = L"Проверка обновлений отменена.";
            return false;
        }
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.value, &available)) {
            error = L"Ошибка чтения ответа сервера обновлений.";
            return false;
        }
        if (available == 0) return true;
        const std::size_t oldSize = body.size();
        body.resize(oldSize + available);
        DWORD read = 0;
        if (!WinHttpReadData(request.value, body.data() + oldSize, available, &read)) {
            error = L"Ошибка чтения ответа сервера обновлений.";
            return false;
        }
        body.resize(oldSize + read);
    }
}

std::wstring Quote(const std::wstring& value) {
    std::wstring escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back(L'\"');
    for (const wchar_t ch : value) {
        if (ch == L'\"') escaped += L"\\\"";
        else escaped.push_back(ch);
    }
    escaped.push_back(L'\"');
    return escaped;
}

std::wstring FileNameFromUrl(const std::wstring& url) {
    auto slash = url.find_last_of(L'/');
    std::wstring name = slash == std::wstring::npos ? L"DPopCleaner_Update.exe" : url.substr(slash + 1);
    const auto query = name.find(L'?');
    if (query != std::wstring::npos) name.resize(query);
    if (name.empty()) name = L"DPopCleaner_Update.exe";
    for (auto& ch : name) {
        if (ch == L'<' || ch == L'>' || ch == L':' || ch == L'\"' || ch == L'/' ||
            ch == L'\\' || ch == L'|' || ch == L'?' || ch == L'*') ch = L'_';
    }
    return name;
}

} // namespace

std::filesystem::path AppDataDirectory() {
    wchar_t buffer[32768]{};
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer,
                                                  static_cast<DWORD>(std::size(buffer)));
    if (length > 0 && length < std::size(buffer)) {
        return std::filesystem::path(buffer) / L"DPopCleaner";
    }
    return std::filesystem::temp_directory_path() / L"DPopCleaner";
}

std::filesystem::path UpdatesDirectory() {
    return AppDataDirectory() / L"Updates";
}

UpdateCheckResult CheckStableUpdates(const std::atomic_bool* shutdown) {
    UpdateCheckResult result{};
    if (!DelayForTestIfRequested(shutdown, result.error)) return result;

    std::string json;
    if (!HttpGetUtf8(kStableManifestUrl, json, result.error, shutdown)) return result;
    if (IsCancelled(shutdown)) {
        result.error = L"Проверка обновлений отменена.";
        return result;
    }
    if (!ParseUpdateManifestUtf8(json, result.manifest, result.error)) return result;
    if (!IsUsableStableManifest(result.manifest, result.error)) return result;

    result.success = true;
    result.updateAvailable = IsRemoteNewer(
        VersionIdentity{version::kVersionCode, version::kRevision},
        VersionIdentity{result.manifest.versionCode, result.manifest.revision});
    return result;
}

bool DownloadVerifiedPackage(const UpdateManifest& manifest,
                             std::filesystem::path& package,
                             std::wstring& error,
                             const std::atomic_bool* shutdown) {
    error.clear();
    package.clear();
    if (!IsUsableStableManifest(manifest, error)) return false;
    if (IsCancelled(shutdown)) {
        error = L"Загрузка обновления отменена.";
        return false;
    }

    std::error_code ec;
    const auto directory = UpdatesDirectory();
    std::filesystem::create_directories(directory, ec);
    if (ec) {
        error = L"Не удалось создать папку обновлений (код " + std::to_wstring(ec.value()) + L").";
        return false;
    }

    InternetHandle session;
    InternetHandle connection;
    InternetHandle request;
    if (!OpenGetRequest(manifest.downloadUrl, session, connection, request, error)) return false;

    const auto finalPath = directory / FileNameFromUrl(manifest.downloadUrl);
    const std::filesystem::path partPath = finalPath.wstring() + L".part";
    DeleteFileW(partPath.c_str());

    FileHandle output(CreateFileW(partPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL, nullptr));
    if (output.value == INVALID_HANDLE_VALUE) {
        error = L"Не удалось создать временный файл обновления.";
        return false;
    }

    std::uint64_t total = 0;
    bool downloadOk = true;
    for (;;) {
        if (IsCancelled(shutdown)) {
            error = L"Загрузка обновления отменена.";
            downloadOk = false;
            break;
        }
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.value, &available)) {
            error = L"Ошибка загрузки обновления.";
            downloadOk = false;
            break;
        }
        if (available == 0) break;

        std::vector<char> buffer(available);
        DWORD read = 0;
        if (!WinHttpReadData(request.value, buffer.data(), available, &read)) {
            error = L"Ошибка чтения пакета обновления.";
            downloadOk = false;
            break;
        }
        if (total + read > manifest.size) {
            error = L"Скачанный пакет больше размера, указанного в манифесте.";
            downloadOk = false;
            break;
        }
        DWORD written = 0;
        if (!WriteFile(output.value, buffer.data(), read, &written, nullptr) || written != read) {
            error = L"Не удалось записать пакет обновления на диск.";
            downloadOk = false;
            break;
        }
        total += read;
    }

    if (downloadOk && !FlushFileBuffers(output.value)) {
        error = L"Не удалось завершить запись пакета обновления.";
        downloadOk = false;
    }
    CloseHandle(output.value);
    output.value = INVALID_HANDLE_VALUE;

    if (!downloadOk || total != manifest.size) {
        if (downloadOk) error = L"Размер скачанного пакета не совпадает с манифестом.";
        DeleteFileW(partPath.c_str());
        return false;
    }
    if (!VerifyPackageFile(partPath, manifest, error)) {
        DeleteFileW(partPath.c_str());
        return false;
    }
    if (!MoveFileExW(partPath.c_str(), finalPath.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = L"Не удалось завершить файл обновления (код " + std::to_wstring(GetLastError()) + L").";
        DeleteFileW(partPath.c_str());
        return false;
    }

    package = finalPath;
    return true;
}

std::wstring BuildUpdaterArguments(const UpdateManifest& manifest,
                                   const std::filesystem::path& package,
                                   bool allowUnsigned,
                                   const std::filesystem::path& restartExe,
                                   unsigned long parentPid) {
    std::wstring args = L"--parent " + std::to_wstring(parentPid) +
                        L" --package " + Quote(package.wstring()) +
                        L" --sha256 " + Quote(manifest.sha256) +
                        L" --restart " + Quote(restartExe.wstring());
    if (!manifest.installArgs.empty()) args += L" --args " + Quote(manifest.installArgs);
    if (manifest.signedPackage) args += L" --signed";
    if (allowUnsigned) args += L" --allow-unsigned";
    return args;
}

bool StageUpdaterForHandoff(const std::filesystem::path& installedUpdater,
                            std::filesystem::path& stagedUpdater,
                            std::wstring& error) {
    error.clear();
    stagedUpdater.clear();

    std::error_code ec;
    if (!std::filesystem::is_regular_file(installedUpdater, ec) || ec) {
        error = L"DPopUpdater.exe не найден рядом с DPopCleaner.exe.";
        return false;
    }

    const auto directory = UpdatesDirectory();
    std::filesystem::create_directories(directory, ec);
    if (ec) {
        error = L"Не удалось создать папку временного updater (код " + std::to_wstring(ec.value()) + L").";
        return false;
    }

    const auto fileName = L"DPopUpdater-handoff-" + std::to_wstring(GetCurrentProcessId()) +
                          L"-" + std::to_wstring(GetTickCount64()) + L".exe";
    const auto destination = directory / fileName;
    std::filesystem::copy_file(installedUpdater, destination,
                               std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        error = L"Не удалось подготовить временную копию DPopUpdater.exe (код " +
                std::to_wstring(ec.value()) + L").";
        return false;
    }

    stagedUpdater = destination;
    return true;
}

bool LaunchUpdater(const UpdateManifest& manifest,
                   const std::filesystem::path& package,
                   bool allowUnsigned,
                   const std::filesystem::path& updaterExe,
                   const std::filesystem::path& restartExe,
                   std::wstring& error) {
    error.clear();
    if (!VerifyPackageFile(package, manifest, error)) return false;
    if (manifest.signedPackage) {
        if (!VerifyAuthenticode(package, error)) return false;
    } else if (!allowUnsigned) {
        error = L"Пакет не подписан Authenticode. Требуется явное подтверждение пользователя.";
        return false;
    }
    if (!std::filesystem::exists(updaterExe)) {
        error = L"DPopUpdater.exe не найден рядом с DPopCleaner.exe.";
        return false;
    }

    std::filesystem::path stagedUpdater;
    if (!StageUpdaterForHandoff(updaterExe, stagedUpdater, error)) return false;

    const std::wstring args = BuildUpdaterArguments(
        manifest, package, allowUnsigned, restartExe, GetCurrentProcessId());
    SHELLEXECUTEINFOW execute{};
    execute.cbSize = sizeof(execute);
    execute.lpFile = stagedUpdater.c_str();
    execute.lpParameters = args.c_str();
    execute.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&execute)) {
        const DWORD launchError = GetLastError();
        std::error_code cleanupError;
        std::filesystem::remove(stagedUpdater, cleanupError);
        error = L"Не удалось запустить DPopUpdater.exe (код " + std::to_wstring(launchError) + L").";
        return false;
    }
    return true;
}

} // namespace dpop0418
