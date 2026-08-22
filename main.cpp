#include "update/Hash.h"
#include "update/Signature.h"
#include <windows.h>
#include <shellapi.h>
#include <filesystem>
#include <string>
#include <vector>

namespace {
std::wstring ArgValue(const std::vector<std::wstring>& args,const std::wstring& key){
    for(size_t i=0;i+1<args.size();++i) if(args[i]==key) return args[i+1];
    return {};
}
}

int WINAPI wWinMain(HINSTANCE,HINSTANCE,LPWSTR,int){
    int argc=0; LPWSTR* argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    std::vector<std::wstring> args; for(int i=0;i<argc;++i) args.emplace_back(argv[i]); LocalFree(argv);
    const auto parentText=ArgValue(args,L"--parent");
    const auto package=ArgValue(args,L"--package");
    const auto installArgs=ArgValue(args,L"--args");
    if(parentText.empty()||package.empty()) return 2;

    const DWORD parent=static_cast<DWORD>(std::stoul(parentText));
    HANDLE process=OpenProcess(SYNCHRONIZE,FALSE,parent);
    if(process){ WaitForSingleObject(process,30000); CloseHandle(process); }

    std::wstring sigError;
    if(!dpop::update::VerifyAuthenticode(package,sigError)){
        MessageBoxW(nullptr,(L"Обновление остановлено.\n"+sigError).c_str(),L"DPopUpdater",MB_ICONERROR);
        return 3;
    }

    SHELLEXECUTEINFOW sei{}; sei.cbSize=sizeof(sei); sei.fMask=SEE_MASK_NOCLOSEPROCESS; sei.lpVerb=L"runas"; sei.lpFile=package.c_str(); sei.lpParameters=installArgs.empty()?nullptr:installArgs.c_str(); sei.nShow=SW_SHOWNORMAL;
    if(!ShellExecuteExW(&sei)){
        MessageBoxW(nullptr,L"Не удалось запустить подписанный установщик.",L"DPopUpdater",MB_ICONERROR);
        return 4;
    }
    if(sei.hProcess) CloseHandle(sei.hProcess);
    return 0;
}
