#include "app/MainWindow.h"
#include "core/Version.h"
#include "core/Paths.h"
#include "core/Logger.h"
#include "modules/SystemInfo.h"
#include "modules/Cleaner.h"
#include "modules/StartupManager.h"
#include "modules/DPopGuard.h"
#include "modules/ZapretManager.h"
#include "modules/Applications.h"
#include "update/UpdateClient.h"
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <string>
#include <thread>
#include <sstream>
#include <iomanip>
#include <memory>
#include <filesystem>
#include <vector>

#pragma comment(lib, "comctl32.lib")

namespace {
constexpr int ID_STATUS=1001;
constexpr int ID_CHECK_UPDATE=1002;
constexpr int ID_CLEAN_TEMP=1003;
constexpr int ID_GUARD=1004;
constexpr int ID_ZAPRET=1005;
constexpr int ID_STARTUP=1006;
constexpr int ID_OVERVIEW=1007;
constexpr int ID_APPS=1008;
constexpr int ID_APP_LIST=1100;
constexpr int ID_APP_REFRESH=1101;
constexpr int ID_APP_UNINSTALL=1102;
constexpr int ID_APP_SCAN=1103;
constexpr UINT WM_UPDATE_RESULT=WM_APP+1;
constexpr UINT_PTR ID_STARTUP_UPDATE_TIMER=9001;

HWND g_status{};
HWND g_appList{};
HWND g_appRefresh{};
HWND g_appUninstall{};
HWND g_appScan{};
HFONT g_font{};
std::vector<dpop::apps::InstalledApp> g_apps;

std::wstring Bytes(std::uint64_t b){
    const double gb=b/1024.0/1024.0/1024.0;
    std::wostringstream s; s<<std::fixed<<std::setprecision(1)<<gb<<L" GB"; return s.str();
}
void SetStatus(const std::wstring& text){ SetWindowTextW(g_status,text.c_str()); }

void ShowMainStatus(){
    ShowWindow(g_status, SW_SHOW);
    ShowWindow(g_appList, SW_HIDE);
    ShowWindow(g_appRefresh, SW_HIDE);
    ShowWindow(g_appUninstall, SW_HIDE);
    ShowWindow(g_appScan, SW_HIDE);
}

void ShowApplicationsControls(){
    ShowWindow(g_status, SW_HIDE);
    ShowWindow(g_appList, SW_SHOW);
    ShowWindow(g_appRefresh, SW_SHOW);
    ShowWindow(g_appUninstall, SW_SHOW);
    ShowWindow(g_appScan, SW_SHOW);
}

void ShowOverview(){
    ShowMainStatus();
    const auto s=dpop::system_info::Collect();
    std::wstring text=L"Готово к анализу\r\n\r\nCPU: "+std::to_wstring(s.cpuCount)+L" логических потоков\r\n"+
      L"RAM: "+Bytes(s.ramTotal-s.ramAvailable)+L" / "+Bytes(s.ramTotal)+L"\r\n"+
      L"Системный диск: свободно "+Bytes(s.systemDriveFree)+L" / "+Bytes(s.systemDriveTotal)+L"\r\n"+
      L"Процессов: "+std::to_wstring(s.processCount)+L"\r\n\r\n"+
      L"Версия: "+std::wstring(dpop::version::kVersion)+L" BETA";
    SetStatus(text);
}

void CheckUpdatesAsync(HWND hwnd){
    ShowMainStatus();
    SetStatus(L"Проверяем обновления…");
    std::thread([hwnd]{
        auto* result=new dpop::update::CheckResult(dpop::update::CheckForUpdates());
        PostMessageW(hwnd,WM_UPDATE_RESULT,0,reinterpret_cast<LPARAM>(result));
    }).detach();
}

void ConfigureAppListColumns(){
    ListView_SetExtendedListViewStyle(g_appList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
    struct Col { const wchar_t* title; int width; } cols[] = {
        {L"Приложение", 255}, {L"Версия", 100}, {L"Издатель", 175}, {L"Папка установки", 300}
    };
    for(int i=0;i<4;++i){
        LVCOLUMNW col{}; col.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM; col.pszText=const_cast<LPWSTR>(cols[i].title); col.cx=cols[i].width; col.iSubItem=i;
        ListView_InsertColumn(g_appList,i,&col);
    }
}

void RefreshApplications(){
    ShowApplicationsControls();
    ListView_DeleteAllItems(g_appList);
    g_apps=dpop::apps::EnumerateInstalledApps();
    for(std::size_t i=0;i<g_apps.size();++i){
        auto& app=g_apps[i];
        LVITEMW item{}; item.mask=LVIF_TEXT|LVIF_PARAM; item.iItem=static_cast<int>(i); item.lParam=static_cast<LPARAM>(i); item.pszText=app.displayName.data();
        const int row=ListView_InsertItem(g_appList,&item);
        if(row<0) continue;
        ListView_SetItemText(g_appList,row,1,app.displayVersion.data());
        ListView_SetItemText(g_appList,row,2,app.publisher.data());
        ListView_SetItemText(g_appList,row,3,app.installLocation.data());
    }
}

int SelectedAppIndex(){
    const int row=ListView_GetNextItem(g_appList,-1,LVNI_SELECTED);
    if(row<0) return -1;
    LVITEMW item{}; item.mask=LVIF_PARAM; item.iItem=row;
    if(!ListView_GetItem(g_appList,&item)) return -1;
    const auto idx=static_cast<std::size_t>(item.lParam);
    return idx<g_apps.size()?static_cast<int>(idx):-1;
}

std::wstring LeftoversText(const std::vector<dpop::apps::LeftoverItem>& leftovers){
    std::wstring text=L"Найдены возможные остатки после удаления:\n\n";
    std::size_t shown=0;
    for(const auto& item:leftovers){
        if(shown++>=12){ text+=L"\n… и ещё "+std::to_wstring(leftovers.size()-12)+L" объект(ов)"; break; }
        text+=L"• "+item.path.wstring()+L"\n  "+item.reason+L"\n";
    }
    text+=L"\nПереместить эти объекты в Корзину?\n\nDPopCleaner использует консервативное совпадение имён и не удаляет произвольные ключи реестра.";
    return text;
}

void ScanAndOfferCleanup(HWND hwnd,const dpop::apps::InstalledApp& app){
    const auto leftovers=dpop::apps::FindLeftovers(app);
    if(leftovers.empty()){
        MessageBoxW(hwnd,L"Явных хвостов не найдено.",L"DPopCleaner — остатки приложения",MB_OK|MB_ICONINFORMATION);
        return;
    }
    if(MessageBoxW(hwnd,LeftoversText(leftovers).c_str(),L"DPopCleaner — очистка остатков",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES) return;
    std::size_t removed=0; std::wstring error;
    dpop::apps::MoveLeftoversToRecycleBin(leftovers,removed,error);
    std::wstring result=L"Перемещено в Корзину: "+std::to_wstring(removed)+L" из "+std::to_wstring(leftovers.size())+L".";
    if(!error.empty()) result+=L"\n\n"+error;
    MessageBoxW(hwnd,result.c_str(),L"DPopCleaner",error.empty()?MB_ICONINFORMATION:MB_ICONWARNING);
}

void UninstallSelected(HWND hwnd){
    const int index=SelectedAppIndex();
    if(index<0){ MessageBoxW(hwnd,L"Сначала выбери программу в списке.",L"DPopCleaner",MB_OK|MB_ICONINFORMATION); return; }
    const auto app=g_apps[static_cast<std::size_t>(index)];
    const std::wstring question=L"Запустить штатное удаление приложения:\n\n"+app.displayName+L" "+app.displayVersion+L"\n\nПосле завершения DPopCleaner проверит оставшиеся папки и ярлыки. Их удаление будет предложено отдельно.";
    if(MessageBoxW(hwnd,question.c_str(),L"Удаление приложения",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES) return;

    DWORD exitCode=0; std::wstring error;
    if(!dpop::apps::RunUninstaller(app,exitCode,error)){
        MessageBoxW(hwnd,error.c_str(),L"Не удалось удалить приложение",MB_OK|MB_ICONERROR); return;
    }
    RefreshApplications();
    ScanAndOfferCleanup(hwnd,app);
}

LRESULT CALLBACK WndProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_CREATE:{
        INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_LISTVIEW_CLASSES};
        InitCommonControlsEx(&icc);
        g_font=CreateFontW(-18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        auto titleFont=CreateFontW(-30,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        HWND title=CreateWindowW(L"STATIC",L"DPopCleaner 0.3.1 BETA",WS_CHILD|WS_VISIBLE,30,24,900,44,hwnd,nullptr,nullptr,nullptr);
        SendMessageW(title,WM_SETFONT,reinterpret_cast<WPARAM>(titleFont),TRUE);
        g_status=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE,320,105,720,470,hwnd,reinterpret_cast<HMENU>(ID_STATUS),nullptr,nullptr);
        SendMessageW(g_status,WM_SETFONT,reinterpret_cast<WPARAM>(g_font),TRUE);
        struct Btn{const wchar_t* t;int id;int y;};
        Btn buttons[]={{L"Обзор",ID_OVERVIEW,95},{L"Очистить TEMP",ID_CLEAN_TEMP,145},{L"DPopGuard",ID_GUARD,195},{L"Автозагрузка",ID_STARTUP,245},{L"Приложения",ID_APPS,295},{L"Zapret статус",ID_ZAPRET,345},{L"Обновления",ID_CHECK_UPDATE,395}};
        for(auto& b:buttons){ HWND h=CreateWindowW(L"BUTTON",b.t,WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,30,b.y,235,42,hwnd,reinterpret_cast<HMENU>(b.id),nullptr,nullptr); SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(g_font),TRUE); }

        g_appList=CreateWindowW(WC_LISTVIEWW,L"",WS_CHILD|LVS_REPORT|LVS_SINGLESEL|WS_BORDER,300,95,770,430,hwnd,reinterpret_cast<HMENU>(ID_APP_LIST),nullptr,nullptr);
        SendMessageW(g_appList,WM_SETFONT,reinterpret_cast<WPARAM>(g_font),TRUE);
        ConfigureAppListColumns();
        g_appRefresh=CreateWindowW(L"BUTTON",L"Обновить список",WS_CHILD|BS_PUSHBUTTON,300,540,170,40,hwnd,reinterpret_cast<HMENU>(ID_APP_REFRESH),nullptr,nullptr);
        g_appScan=CreateWindowW(L"BUTTON",L"Найти хвосты",WS_CHILD|BS_PUSHBUTTON,485,540,170,40,hwnd,reinterpret_cast<HMENU>(ID_APP_SCAN),nullptr,nullptr);
        g_appUninstall=CreateWindowW(L"BUTTON",L"Удалить выбранное",WS_CHILD|BS_PUSHBUTTON,670,540,210,40,hwnd,reinterpret_cast<HMENU>(ID_APP_UNINSTALL),nullptr,nullptr);
        SendMessageW(g_appRefresh,WM_SETFONT,reinterpret_cast<WPARAM>(g_font),TRUE);
        SendMessageW(g_appScan,WM_SETFONT,reinterpret_cast<WPARAM>(g_font),TRUE);
        SendMessageW(g_appUninstall,WM_SETFONT,reinterpret_cast<WPARAM>(g_font),TRUE);

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
            ShowMainStatus();
            const auto estimate=dpop::cleaner::EstimateUserTempBytes();
            if(MessageBoxW(hwnd,(L"Удалить доступные обычные файлы из TEMP?\nПримерный объём: "+Bytes(estimate)).c_str(),L"DPopCleaner",MB_YESNO|MB_ICONQUESTION)==IDYES){
                const auto r=dpop::cleaner::CleanUserTemp();
                SetStatus(L"Очистка TEMP завершена.\r\nУдалено файлов: "+std::to_wstring(r.removedFiles)+L"\r\nОсвобождено: "+Bytes(r.removedBytes)+L"\r\nПропущено/занято: "+std::to_wstring(r.failedFiles));
            } break;
        }
        case ID_GUARD:{
            ShowMainStatus();
            const auto r=dpop::guard::QuickScan();
            std::wstring t=L"DPopGuard 2 — безопасная реконструкция\r\n\r\n"+r.note+L"\r\n\r\nНаходок: "+std::to_wstring(r.findings.size());
            for(const auto& f:r.findings) t+=L"\r\n• "+f.title+L" — "+f.details;
            SetStatus(t); break;
        }
        case ID_STARTUP:{
            ShowMainStatus();
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
        case ID_APPS: RefreshApplications(); break;
        case ID_APP_REFRESH: RefreshApplications(); break;
        case ID_APP_UNINSTALL: UninstallSelected(hwnd); break;
        case ID_APP_SCAN:{
            const int index=SelectedAppIndex();
            if(index<0) MessageBoxW(hwnd,L"Сначала выбери программу в списке.",L"DPopCleaner",MB_OK|MB_ICONINFORMATION);
            else ScanAndOfferCleanup(hwnd,g_apps[static_cast<std::size_t>(index)]);
            break;
        }
        case ID_ZAPRET:{
            ShowMainStatus();
            const auto s=dpop::zapret::QueryStatus();
            SetStatus(L"Zapret / WinDivert\r\n\r\nСлужба zapret: "+std::wstring(s.serviceInstalled?(s.serviceRunning?L"RUNNING":L"STOPPED"):L"не установлена")+L"\r\nwinws.exe: "+(s.winwsRunning?L"RUNNING":L"не запущен"));
            break;
        }
        case ID_CHECK_UPDATE: CheckUpdatesAsync(hwnd); break;
        }
        return 0;
    }
    case WM_NOTIFY:{
        if(reinterpret_cast<NMHDR*>(lp)->idFrom==ID_APP_LIST && reinterpret_cast<NMHDR*>(lp)->code==NM_DBLCLK){
            UninstallSelected(hwnd); return 0;
        }
        break;
    }
    case WM_UPDATE_RESULT:{
        std::unique_ptr<dpop::update::CheckResult> r(reinterpret_cast<dpop::update::CheckResult*>(lp));
        if(!r->success){ SetStatus(L"Ошибка проверки обновлений:\r\n"+r->error); return 0; }
        if(!r->updateAvailable){ SetStatus(L"Установлена актуальная версия "+std::wstring(dpop::version::kVersion)+L" BETA."); return 0; }
        const auto& m=r->manifest;
        if(MessageBoxW(hwnd,(L"Доступно обновление "+m.version+L".\nСкачать обновление и проверить SHA-256?").c_str(),L"DPopCleaner Update",MB_YESNO|MB_ICONINFORMATION)!=IDYES) return 0;
        SetStatus(L"Загружаем обновление "+m.version+L"…");
        std::filesystem::path file; std::wstring error;
        if(!dpop::update::DownloadPackage(m,file,error)){ SetStatus(L"Обновление не загружено:\r\n"+error); return 0; }

        bool allowUnsigned=false;
        if(!m.signedPackage){
            const std::wstring warning=L"SHA-256 обновления совпал, но пакет пока не подписан Authenticode.\n\nДля BETA можно продолжить вручную. Windows SmartScreen/Defender всё равно может показать предупреждение.\n\nЗапустить установщик обновления?";
            if(MessageBoxW(hwnd,warning.c_str(),L"Неподписанное BETA-обновление",MB_YESNO|MB_ICONWARNING|MB_DEFBUTTON2)!=IDYES){
                SetStatus(L"Обновление скачано и проверено. Автоматическая установка отменена пользователем.\r\nФайл:\r\n"+file.wstring());
                ShellExecuteW(hwnd,L"open",file.parent_path().c_str(),nullptr,nullptr,SW_SHOWNORMAL);
                return 0;
            }
            allowUnsigned=true;
        }
        if(!dpop::update::PrepareAndLaunchUpdater(m,file,allowUnsigned,error)){ SetStatus(L"Проверка/запуск обновления остановлены:\r\n"+error); return 0; }
        DestroyWindow(hwnd);
        return 0;
    }
    case WM_DESTROY:
        if(g_font) DeleteObject(g_font);
        PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
}

namespace dpop::ui {
int Run(HINSTANCE instance,int showCommand){
    dpop::paths::EnsureDirectories();
    dpop::log::Info(L"DPopCleaner 0.3.1 BETA started");
    WNDCLASSEXW wc{}; wc.cbSize=sizeof(wc); wc.hInstance=instance; wc.lpfnWndProc=WndProc; wc.lpszClassName=L"DPopCleanerMain"; wc.hCursor=LoadCursorW(nullptr,IDC_ARROW); wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(101)); wc.hbrBackground=CreateSolidBrush(RGB(13,30,45));
    if(!RegisterClassExW(&wc)) return 1;
    HWND hwnd=CreateWindowExW(0,wc.lpszClassName,L"DPopCleaner 0.3.1 BETA",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1130,680,nullptr,nullptr,instance,nullptr);
    if(!hwnd) return 2;
    ShowWindow(hwnd,showCommand); UpdateWindow(hwnd);
    MSG msg{}; while(GetMessageW(&msg,nullptr,0,0)>0){ TranslateMessage(&msg); DispatchMessageW(&msg); }
    return static_cast<int>(msg.wParam);
}
}
