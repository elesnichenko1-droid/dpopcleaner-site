#include "app/MainWindow.h"
#include "core/Version.h"
#include "core/Paths.h"
#include "core/Logger.h"
#include "modules/SystemInfo.h"
#include "modules/Cleaner.h"
#include "modules/StartupManager.h"
#include "modules/DPopGuard.h"
#include "modules/ZapretManager.h"
#include "update/UpdateClient.h"
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <thread>
#include <sstream>
#include <iomanip>
#include <memory>
#include <filesystem>

namespace {
constexpr int ID_STATUS=1001;
constexpr int ID_CHECK_UPDATE=1002;
constexpr int ID_CLEAN_TEMP=1003;
constexpr int ID_GUARD=1004;
constexpr int ID_ZAPRET=1005;
constexpr int ID_STARTUP=1006;
constexpr int ID_OVERVIEW=1007;
constexpr UINT WM_UPDATE_RESULT=WM_APP+1;
constexpr UINT_PTR ID_STARTUP_UPDATE_TIMER=9001;

HWND g_status{};
std::wstring Bytes(std::uint64_t b){
    const double gb=b/1024.0/1024.0/1024.0;
    std::wostringstream s; s<<std::fixed<<std::setprecision(1)<<gb<<L" GB"; return s.str();
}
void SetStatus(const std::wstring& text){ SetWindowTextW(g_status,text.c_str()); }

void ShowOverview(){
    const auto s=dpop::system_info::Collect();
    std::wstring text=L"Готово к анализу\r\n\r\nCPU: "+std::to_wstring(s.cpuCount)+L" логических потоков\r\n"+
      L"RAM: "+Bytes(s.ramTotal-s.ramAvailable)+L" / "+Bytes(s.ramTotal)+L"\r\n"+
      L"Системный диск: свободно "+Bytes(s.systemDriveFree)+L" / "+Bytes(s.systemDriveTotal)+L"\r\n"+
      L"Процессов: "+std::to_wstring(s.processCount)+L"\r\n\r\n"+
      L"Версия: "+std::wstring(dpop::version::kVersion)+L" BETA";
    SetStatus(text);
}

void CheckUpdatesAsync(HWND hwnd){
    SetStatus(L"Проверяем обновления…");
    std::thread([hwnd]{
        auto* result=new dpop::update::CheckResult(dpop::update::CheckForUpdates());
        PostMessageW(hwnd,WM_UPDATE_RESULT,0,reinterpret_cast<LPARAM>(result));
    }).detach();
}

LRESULT CALLBACK WndProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_CREATE:{
        auto font=CreateFontW(-18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        auto titleFont=CreateFontW(-30,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        HWND title=CreateWindowW(L"STATIC",L"DPopCleaner 0.2.15 BETA",WS_CHILD|WS_VISIBLE,30,24,780,44,hwnd,nullptr,nullptr,nullptr);
        SendMessageW(title,WM_SETFONT,reinterpret_cast<WPARAM>(titleFont),TRUE);
        g_status=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE,320,105,620,390,hwnd,reinterpret_cast<HMENU>(ID_STATUS),nullptr,nullptr);
        SendMessageW(g_status,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
        struct Btn{const wchar_t* t;int id;int y;};
        Btn buttons[]={{L"Обзор",ID_OVERVIEW,95},{L"Очистить TEMP",ID_CLEAN_TEMP,145},{L"DPopGuard",ID_GUARD,195},{L"Автозагрузка",ID_STARTUP,245},{L"Zapret статус",ID_ZAPRET,295},{L"Проверить обновления",ID_CHECK_UPDATE,345}};
        for(auto& b:buttons){ HWND h=CreateWindowW(L"BUTTON",b.t,WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,30,b.y,235,42,hwnd,reinterpret_cast<HMENU>(b.id),nullptr,nullptr); SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE); }
        ShowOverview();
        SetTimer(hwnd, ID_STARTUP_UPDATE_TIMER, 2500, nullptr);
        return 0;
    }
    case WM_TIMER:
        if (wp == ID_STARTUP_UPDATE_TIMER) {
            KillTimer(hwnd, ID_STARTUP_UPDATE_TIMER);
            CheckUpdatesAsync(hwnd);
            return 0;
        }
        break;
    case WM_COMMAND:{
        switch(LOWORD(wp)){
        case ID_OVERVIEW: ShowOverview(); break;
        case ID_CLEAN_TEMP:{
            const auto estimate=dpop::cleaner::EstimateUserTempBytes();
            if(MessageBoxW(hwnd,(L"Удалить доступные обычные файлы из TEMP?\nПримерный объём: "+Bytes(estimate)).c_str(),L"DPopCleaner",MB_YESNO|MB_ICONQUESTION)==IDYES){
                const auto r=dpop::cleaner::CleanUserTemp();
                SetStatus(L"Очистка TEMP завершена.\r\nУдалено файлов: "+std::to_wstring(r.removedFiles)+L"\r\nОсвобождено: "+Bytes(r.removedBytes)+L"\r\nПропущено/занято: "+std::to_wstring(r.failedFiles));
            } break;
        }
        case ID_GUARD:{
            const auto r=dpop::guard::QuickScan();
            std::wstring t=L"DPopGuard 2 — безопасная реконструкция\r\n\r\n"+r.note+L"\r\n\r\nНаходок: "+std::to_wstring(r.findings.size());
            for(const auto& f:r.findings) t+=L"\r\n• "+f.title+L" — "+f.details;
            SetStatus(t); break;
        }
        case ID_STARTUP:{
            const auto items=dpop::startup::EnumerateCurrentUserRun();
            std::wstring text=L"Автозагрузка текущего пользователя\r\n\r\nЗаписей: "+std::to_wstring(items.size());
            size_t shown=0;
            for(const auto& item:items){
                if(shown++>=20){ text+=L"\r\n…"; break; }
                text+=L"\r\n• "+item.name+L" — "+item.command;
            }
            SetStatus(text);
            break;
        }
        case ID_ZAPRET:{
            const auto s=dpop::zapret::QueryStatus();
            SetStatus(L"Zapret / WinDivert\r\n\r\nСлужба zapret: "+std::wstring(s.serviceInstalled?(s.serviceRunning?L"RUNNING":L"STOPPED"):L"не установлена")+L"\r\nwinws.exe: "+(s.winwsRunning?L"RUNNING":L"не запущен"));
            break;
        }
        case ID_CHECK_UPDATE: CheckUpdatesAsync(hwnd); break;
        }
        return 0;
    }
    case WM_UPDATE_RESULT:{
        std::unique_ptr<dpop::update::CheckResult> r(reinterpret_cast<dpop::update::CheckResult*>(lp));
        if(!r->success){ SetStatus(L"Ошибка проверки обновлений:\r\n"+r->error); return 0; }
        if(!r->updateAvailable){ SetStatus(L"Установлена актуальная версия "+std::wstring(dpop::version::kVersion)+L" BETA."); return 0; }
        const auto& m=r->manifest;
        if(MessageBoxW(hwnd,(L"Доступно обновление "+m.version+L".\nСкачать и проверить SHA-256?").c_str(),L"DPopCleaner Update",MB_YESNO|MB_ICONINFORMATION)!=IDYES) return 0;
        SetStatus(L"Загружаем обновление "+m.version+L"…");
        std::filesystem::path file; std::wstring error;
        if(!dpop::update::DownloadPackage(m,file,error)){ SetStatus(L"Обновление не загружено:\r\n"+error); return 0; }
        if(!m.signedPackage){
            SetStatus(L"Обновление скачано и SHA-256 совпал.\r\n\r\nАвтозапуск отключён: релиз пока не подписан Authenticode.\r\nФайл:\r\n"+file.wstring());
            ShellExecuteW(hwnd,L"open",file.parent_path().c_str(),nullptr,nullptr,SW_SHOWNORMAL);
            return 0;
        }
        if(!dpop::update::PrepareAndLaunchUpdater(m,file,error)){ SetStatus(L"Проверка/запуск обновления остановлены:\r\n"+error); return 0; }
        DestroyWindow(hwnd);
        return 0;
    }
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
}

namespace dpop::ui {
int Run(HINSTANCE instance,int showCommand){
    dpop::paths::EnsureDirectories();
    dpop::log::Info(L"DPopCleaner 0.2.15 BETA started");
    WNDCLASSEXW wc{}; wc.cbSize=sizeof(wc); wc.hInstance=instance; wc.lpfnWndProc=WndProc; wc.lpszClassName=L"DPopCleanerMain"; wc.hCursor=LoadCursorW(nullptr,IDC_ARROW); wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(101)); wc.hbrBackground=CreateSolidBrush(RGB(13,30,45));
    if(!RegisterClassExW(&wc)) return 1;
    HWND hwnd=CreateWindowExW(0,wc.lpszClassName,L"DPopCleaner 0.2.15 BETA",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1000,600,nullptr,nullptr,instance,nullptr);
    if(!hwnd) return 2;
    ShowWindow(hwnd,showCommand); UpdateWindow(hwnd);
    MSG msg{}; while(GetMessageW(&msg,nullptr,0,0)>0){ TranslateMessage(&msg); DispatchMessageW(&msg); }
    return static_cast<int>(msg.wParam);
}
}
