#include "update/UpdateClient.h"
#include "update/UpdateConfig.h"
#include "update/Hash.h"
#include "update/Signature.h"
#include "core/Paths.h"
#include "core/Version.h"
#include "core/Logger.h"
#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <vector>
#include <cwctype>

namespace fs = std::filesystem;

namespace {
struct InternetHandle {
    HINTERNET h{};
    ~InternetHandle(){ if(h) WinHttpCloseHandle(h); }
};

bool ParseUrl(const std::wstring& url, URL_COMPONENTS& uc, std::wstring& host, std::wstring& path, INTERNET_PORT& port, bool& secure) {
    wchar_t hostBuf[2048]{};
    wchar_t pathBuf[8192]{};
    uc = {};
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = hostBuf; uc.dwHostNameLength = _countof(hostBuf);
    uc.lpszUrlPath = pathBuf; uc.dwUrlPathLength = _countof(pathBuf);
    if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &uc)) return false;
    host.assign(hostBuf, uc.dwHostNameLength);
    path.assign(pathBuf, uc.dwUrlPathLength);
    if (uc.lpszExtraInfo && uc.dwExtraInfoLength) path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);
    if (path.empty()) path=L"/";
    port=uc.nPort;
    secure=uc.nScheme==INTERNET_SCHEME_HTTPS;
    return true;
}

bool HttpGet(const std::wstring& url, std::string& body, std::wstring& error) {
    URL_COMPONENTS uc{}; std::wstring host, path; INTERNET_PORT port{}; bool secure{};
    if (!ParseUrl(url, uc, host, path, port, secure)) { error=L"Некорректный URL"; return false; }

    InternetHandle session{WinHttpOpen(dpop::update_config::kUserAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
    if(!session.h){ error=L"WinHttpOpen failed"; return false; }
    WinHttpSetTimeouts(session.h, dpop::update_config::kConnectTimeoutMs, dpop::update_config::kConnectTimeoutMs,
                       dpop::update_config::kReceiveTimeoutMs, dpop::update_config::kReceiveTimeoutMs);
    InternetHandle connect{WinHttpConnect(session.h, host.c_str(), port, 0)};
    if(!connect.h){ error=L"WinHttpConnect failed"; return false; }
    InternetHandle request{WinHttpOpenRequest(connect.h, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        secure ? WINHTTP_FLAG_SECURE : 0)};
    if(!request.h){ error=L"WinHttpOpenRequest failed"; return false; }
    if(!WinHttpSendRequest(request.h, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(request.h, nullptr)) {
        error=L"Не удалось получить ответ сервера обновлений"; return false;
    }
    DWORD status=0, len=sizeof(status);
    WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &len, nullptr);
    if(status<200 || status>=300){ error=L"HTTP " + std::to_wstring(status); return false; }
    body.clear();
    for(;;){
        DWORD available=0;
        if(!WinHttpQueryDataAvailable(request.h,&available)) break;
        if(!available) return true;
        const auto old=body.size(); body.resize(old+available);
        DWORD read=0;
        if(!WinHttpReadData(request.h, body.data()+old, available, &read)){ error=L"Ошибка чтения HTTP-ответа"; return false; }
        body.resize(old+read);
    }
    error=L"Соединение прервано"; return false;
}

std::wstring FileNameFromUrl(const std::wstring& url) {
    auto pos=url.find_last_of(L'/');
    std::wstring name=(pos==std::wstring::npos)?L"DPopCleaner_Update.exe":url.substr(pos+1);
    const auto q=name.find(L'?'); if(q!=std::wstring::npos) name.resize(q);
    if(name.empty()) name=L"DPopCleaner_Update.exe";
    return name;
}

bool EqualsNoCase(std::wstring a, std::wstring b){
    std::transform(a.begin(),a.end(),a.begin(),::towlower);
    std::transform(b.begin(),b.end(),b.begin(),::towlower);
    return a==b;
}
}

namespace dpop::update {
CheckResult CheckForUpdates(){
    CheckResult result{};
    std::string json;
    if(!HttpGet(dpop::update_config::kManifestUrl,json,result.error)) return result;
    std::wstring parseError;
    if(!ParseManifestUtf8(json,result.manifest,parseError)){ result.error=parseError; return result; }
    result.success=true;
    result.updateAvailable=result.manifest.versionCode>dpop::version::kVersionCode;
    return result;
}

bool DownloadPackage(const Manifest& manifest, fs::path& downloadedFile, std::wstring& error){
    dpop::paths::EnsureDirectories();
    URL_COMPONENTS uc{}; std::wstring host,path; INTERNET_PORT port{}; bool secure{};
    if(!ParseUrl(manifest.downloadUrl,uc,host,path,port,secure)){ error=L"Некорректный download_url"; return false; }
    if(!secure){ error=L"Обновления разрешены только по HTTPS"; return false; }

    InternetHandle session{WinHttpOpen(dpop::update_config::kUserAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS,0)};
    if(!session.h){ error=L"WinHttpOpen failed"; return false; }
    InternetHandle connect{WinHttpConnect(session.h,host.c_str(),port,0)};
    if(!connect.h){ error=L"WinHttpConnect failed"; return false; }
    InternetHandle request{WinHttpOpenRequest(connect.h,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)};
    if(!request.h || !WinHttpSendRequest(request.h,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0) || !WinHttpReceiveResponse(request.h,nullptr)){
        error=L"Ошибка загрузки обновления"; return false;
    }
    DWORD status=0, len=sizeof(status);
    WinHttpQueryHeaders(request.h,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&status,&len,nullptr);
    if(status<200 || status>=300){ error=L"Сервер вернул HTTP "+std::to_wstring(status); return false; }

    const auto finalPath=dpop::paths::UpdatesDir()/FileNameFromUrl(manifest.downloadUrl);
    const auto partPath=finalPath.wstring()+L".part";
    HANDLE out=CreateFileW(partPath.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(out==INVALID_HANDLE_VALUE){ error=L"Не удалось создать временный файл обновления"; return false; }
    bool ok=true;
    for(;;){
        DWORD available=0;
        if(!WinHttpQueryDataAvailable(request.h,&available)){ ok=false; break; }
        if(!available) break;
        std::vector<char> buf(available);
        DWORD read=0;
        if(!WinHttpReadData(request.h,buf.data(),available,&read)){ ok=false; break; }
        DWORD written=0;
        if(!WriteFile(out,buf.data(),read,&written,nullptr)||written!=read){ ok=false; break; }
    }
    FlushFileBuffers(out); CloseHandle(out);
    if(!ok){ DeleteFileW(partPath.c_str()); error=L"Загрузка обновления прервана"; return false; }

    std::wstring actualHash;
    if(!Sha256File(partPath,actualHash,error)){ DeleteFileW(partPath.c_str()); return false; }
    if(!EqualsNoCase(actualHash,manifest.sha256)){
        DeleteFileW(partPath.c_str());
        error=L"SHA-256 скачанного файла не совпадает с манифестом. Обновление удалено.";
        return false;
    }
    std::error_code ec;
    fs::remove(finalPath,ec); ec.clear();
    fs::rename(partPath,finalPath,ec);
    if(ec){ error=L"Не удалось завершить файл обновления (код "+std::to_wstring(ec.value())+L")"; return false; }
    downloadedFile=finalPath;
    return true;
}

bool PrepareAndLaunchUpdater(const Manifest& manifest,const fs::path& package,std::wstring& error){
    if(!manifest.signedPackage){
        error=L"Пакет помечен как unsigned. SHA-256 проверен, но автоматический запуск заблокирован до появления цифровой подписи.";
        return false;
    }
    if(!VerifyAuthenticode(package,error)) return false;

    const auto updater=dpop::paths::ExecutableDir()/L"DPopUpdater.exe";
    if(!fs::exists(updater)){ error=L"DPopUpdater.exe не найден рядом с DPopCleaner.exe"; return false; }

    const DWORD pid=GetCurrentProcessId();
    std::wstring args=L"--parent "+std::to_wstring(pid)+L" --package \""+package.wstring()+L"\"";
    if(!manifest.installArgs.empty()) args += L" --args \""+manifest.installArgs+L"\"";

    SHELLEXECUTEINFOW sei{}; sei.cbSize=sizeof(sei); sei.lpFile=updater.c_str(); sei.lpParameters=args.c_str(); sei.nShow=SW_SHOWNORMAL;
    if(!ShellExecuteExW(&sei)){ error=L"Не удалось запустить DPopUpdater.exe"; return false; }
    return true;
}
}
