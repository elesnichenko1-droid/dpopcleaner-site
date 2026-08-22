#include "update/Hash.h"
#include "update/Signature.h"
#include <windows.h>
#include <shellapi.h>
#include <filesystem>
#include <string>
#include <vector>
#include <algorithm>
#include <cwctype>

namespace {
std::wstring ArgValue(const std::vector<std::wstring>& args,const std::wstring& key){
    for(size_t i=0;i+1<args.size();++i) if(args[i]==key) return args[i+1];
    return {};
}
bool HasArg(const std::vector<std::wstring>& args,const std::wstring& key){
    return std::find(args.begin(),args.end(),key)!=args.end();
}
bool EqualsNoCase(std::wstring a,std::wstring b){
    std::transform(a.begin(),a.end(),a.begin(),::towlower);
    std::transform(b.begin(),b.end(),b.begin(),::towlower);
    return a==b;
}
}

int WINAPI wWinMain(HINSTANCE,HINSTANCE,LPWSTR,int){
    int argc=0; LPWSTR* argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    std::vector<std::wstring> args; for(int i=0;i<argc;++i) args.emplace_back(argv[i]); LocalFree(argv);
    const auto parentText=ArgValue(args,L"--parent");
    const auto package=ArgValue(args,L"--package");
    const auto expectedHash=ArgValue(args,L"--sha256");
    const auto installArgs=ArgValue(args,L"--args");
    const auto restartPath=ArgValue(args,L"--restart");
    const bool allowUnsigned=HasArg(args,L"--allow-unsigned");
    if(parentText.empty()||package.empty()||expectedHash.empty()) return 2;

    const DWORD parent=static_cast<DWORD>(std::stoul(parentText));
    HANDLE process=OpenProcess(SYNCHRONIZE,FALSE,parent);
    if(process){ WaitForSingleObject(process,30000); CloseHandle(process); }

    std::wstring actualHash,hashError;
    if(!dpop::update::Sha256File(package,actualHash,hashError) || !EqualsNoCase(actualHash,expectedHash)){
        MessageBoxW(nullptr,(L"Обновление остановлено: SHA-256 пакета не совпадает.\n"+hashError).c_str(),L"DPopUpdater",MB_ICONERROR);
        return 3;
    }

    std::wstring sigError;
    if(!allowUnsigned && !dpop::update::VerifyAuthenticode(package,sigError)){
        MessageBoxW(nullptr,(L"Обновление остановлено.\n"+sigError).c_str(),L"DPopUpdater",MB_ICONERROR);
        return 4;
    }

    SHELLEXECUTEINFOW sei{}; sei.cbSize=sizeof(sei); sei.fMask=SEE_MASK_NOCLOSEPROCESS; sei.lpVerb=L"runas"; sei.lpFile=package.c_str(); sei.lpParameters=installArgs.empty()?nullptr:installArgs.c_str(); sei.nShow=SW_SHOWNORMAL;
    if(!ShellExecuteExW(&sei)){
        MessageBoxW(nullptr,L"Не удалось запустить установщик обновления.",L"DPopUpdater",MB_ICONERROR);
        return 5;
    }
    if(sei.hProcess){
        WaitForSingleObject(sei.hProcess, INFINITE);
        DWORD exitCode=0; GetExitCodeProcess(sei.hProcess,&exitCode);
        CloseHandle(sei.hProcess);
        if(exitCode!=0 && exitCode!=3010) return static_cast<int>(exitCode);
    }
    if(!restartPath.empty() && std::filesystem::exists(restartPath)){
        ShellExecuteW(nullptr,L"open",restartPath.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
    }
    return 0;
}
