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
#include <commdlg.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <shellapi.h>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <memory>
#include <thread>
#include <vector>
#include <iterator>
#include <string>
#include <algorithm>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")

namespace {
namespace fs = std::filesystem;

constexpr COLORREF C_BG       = RGB(8, 24, 38);
constexpr COLORREF C_SIDEBAR  = RGB(10, 29, 45);
constexpr COLORREF C_CARD     = RGB(14, 39, 58);
constexpr COLORREF C_CARD2    = RGB(18, 49, 68);
constexpr COLORREF C_BORDER   = RGB(39, 73, 91);
constexpr COLORREF C_TEXT     = RGB(239, 247, 250);
constexpr COLORREF C_MUTED    = RGB(151, 177, 191);
constexpr COLORREF C_ACCENT   = RGB(103, 237, 119);
constexpr COLORREF C_ACCENT2  = RGB(54, 199, 206);
constexpr COLORREF C_DANGER   = RGB(231, 93, 93);

constexpr int ID_NAV_OVERVIEW = 100;
constexpr int ID_NAV_CLEANING = 101;
constexpr int ID_NAV_GUARD    = 102;
constexpr int ID_NAV_STARTUP  = 103;
constexpr int ID_NAV_APPS     = 104;
constexpr int ID_NAV_ZAPRET   = 105;
constexpr int ID_NAV_TOOLS    = 106;
constexpr int ID_NAV_UPDATES  = 107;
constexpr int ID_NAV_SETTINGS = 108;
constexpr int ID_ACTION1      = 300;
constexpr int ID_ACTION2      = 301;
constexpr int ID_ACTION3      = 302;
constexpr int ID_ACTION4      = 303;
constexpr int ID_LIST         = 400;
constexpr int ID_TEXT         = 401;
constexpr UINT WM_UPDATE_RESULT = WM_APP + 15;
constexpr UINT_PTR ID_STARTUP_UPDATE_TIMER = 9901;

enum class Page { Overview, Cleaning, Guard, Startup, Apps, Zapret, Tools, Updates, Settings };

struct UpdateMessage {
    dpop::update::CheckResult result;
    bool interactive{};
};

HWND g_hwnd{};
HWND g_title{};
HWND g_build{};
HWND g_pageTitle{};
HWND g_pageHint{};
HWND g_text{};
HWND g_list{};
HWND g_actions[4]{};
HWND g_nav[9]{};
HFONT g_font{};
HFONT g_fontSmall{};
HFONT g_fontTitle{};
HFONT g_fontPage{};
HBRUSH g_bgBrush{};
HBRUSH g_cardBrush{};
Page g_page = Page::Overview;
std::vector<dpop::apps::InstalledApp> g_apps;
std::vector<dpop::startup::Entry> g_startup;

std::wstring Bytes(std::uint64_t b) {
    const wchar_t* units[] = {L"Б", L"КБ", L"МБ", L"ГБ", L"ТБ"};
    double value = static_cast<double>(b);
    int u = 0;
    while (value >= 1024.0 && u < 4) { value /= 1024.0; ++u; }
    std::wostringstream s;
    s << std::fixed << std::setprecision(u >= 3 ? 1 : 0) << value << L" " << units[u];
    return s.str();
}

void SetText(const std::wstring& text) { SetWindowTextW(g_text, text.c_str()); }

void SetLabel(HWND h, const wchar_t* text) { SetWindowTextW(h, text); }

void SetAction(int index, const wchar_t* text, bool visible = true) {
    SetLabel(g_actions[index], text);
    ShowWindow(g_actions[index], visible ? SW_SHOW : SW_HIDE);
}

void HideAllActions() { for (auto h : g_actions) ShowWindow(h, SW_HIDE); }

void ShowTextMode() {
    ShowWindow(g_text, SW_SHOW);
    ShowWindow(g_list, SW_HIDE);
}

void ShowListMode() {
    ShowWindow(g_text, SW_HIDE);
    ShowWindow(g_list, SW_SHOW);
}

void ClearListColumns() {
    while (ListView_DeleteColumn(g_list, 0)) {}
    ListView_DeleteAllItems(g_list);
}

void AddColumn(int index, const wchar_t* text, int width) {
    LVCOLUMNW c{};
    c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    c.pszText = const_cast<LPWSTR>(text);
    c.cx = width;
    c.iSubItem = index;
    ListView_InsertColumn(g_list, index, &c);
}

void ConfigureListDark() {
    ListView_SetExtendedListViewStyle(g_list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
    ListView_SetBkColor(g_list, C_CARD);
    ListView_SetTextBkColor(g_list, C_CARD);
    ListView_SetTextColor(g_list, C_TEXT);
    SetWindowTheme(g_list, L"DarkMode_Explorer", nullptr);
    HWND header = ListView_GetHeader(g_list);
    if (header) SetWindowTheme(header, L"DarkMode_Explorer", nullptr);
}

void ShowOverview() {
    ShowTextMode(); HideAllActions();
    SetAction(0, L"Обновить обзор");
    SetAction(1, L"Проверить обновления");
    SetLabel(g_pageTitle, L"Обзор системы");
    SetLabel(g_pageHint, L"Сводка состояния Windows и быстрый доступ к основным функциям DPopCleaner.");
    const auto s = dpop::system_info::Collect();
    const auto used = s.ramTotal > s.ramAvailable ? s.ramTotal - s.ramAvailable : 0;
    const auto freePercent = s.systemDriveTotal ? (100.0 * s.systemDriveFree / s.systemDriveTotal) : 0.0;

    std::wostringstream o;
    o << L"СОСТОЯНИЕ СИСТЕМЫ\r\n\r\n"
      << L"CPU       " << s.cpuCount << L" логических потоков\r\n"
      << L"RAM       " << Bytes(used) << L" / " << Bytes(s.ramTotal) << L"\r\n"
      << L"Диск C:   свободно " << Bytes(s.systemDriveFree) << L" / " << Bytes(s.systemDriveTotal)
      << L"  (" << std::fixed << std::setprecision(0) << freePercent << L"%)\r\n"
      << L"GPU       " << s.gpuName << L"\r\n"
      << L"Процессы  " << s.processCount << L"\r\n\r\n"
      << L"DPopCleaner " << dpop::version::kVersion << L" BETA  •  Stage 3 Revision 2\r\n\r\n"
      << L"В этой сборке возвращена исходная концепция DPopCleaner: очистка, DPopGuard 2,\r\n"
      << L"автозагрузка, реальные установленные приложения, Zapret Center, системные\r\n"
      << L"инструменты, журналирование и автоматические обновления.";
    SetText(o.str());
}

void ShowCleaning() {
    ShowTextMode(); HideAllActions();
    SetAction(0, L"Очистить TEMP");
    SetAction(1, L"Кэши браузеров");
    SetAction(2, L"CrashDumps");
    SetAction(3, L"Очистить Корзину");
    SetLabel(g_pageTitle, L"Очистка");
    SetLabel(g_pageHint, L"Безопасные области очистки. Профили браузеров, документы и пользовательские данные не удаляются.");

    const auto temp = dpop::cleaner::EstimateUserTempBytes();
    const auto browsers = dpop::cleaner::EstimateBrowserCacheBytes();
    const auto crash = dpop::cleaner::EstimateCrashDumpBytes();
    const auto recycle = dpop::cleaner::EstimateRecycleBinBytes();
    std::wostringstream o;
    o << L"АНАЛИЗ ОБЛАСТЕЙ ОЧИСТКИ\r\n\r\n"
      << L"Временные файлы пользователя     " << Bytes(temp) << L"\r\n"
      << L"Кэши Edge / Chrome / Yandex / Firefox / DirectX     " << Bytes(browsers) << L"\r\n"
      << L"Application Crash Dumps          " << Bytes(crash) << L"\r\n"
      << L"Корзина                          " << Bytes(recycle) << L"\r\n\r\n"
      << L"Каждая область очищается отдельно. Заблокированные системой файлы пропускаются.\r\n"
      << L"Для Windows Update / Component Store используй раздел «Инструменты», где запускаются\r\n"
      << L"штатные средства Windows с подтверждением UAC.";
    SetText(o.str());
}

void ShowGuard() {
    ShowTextMode(); HideAllActions();
    SetAction(0, L"Быстрый скан");
    SetAction(1, L"Проверить файл (AMSI)");
    SetLabel(g_pageTitle, L"DPopGuard 2");
    SetLabel(g_pageHint, L"Процессы, persistence/автозагрузка, miner heuristics и проверка выбранного файла через Windows AMSI.");
    SetText(L"DPopGuard 2 готов к проверке.\r\n\r\n"
            L"• процессы и известные имена майнеров\r\n"
            L"• подозрительные процессы из TEMP\r\n"
            L"• HKCU/HKLM Run и Startup folders\r\n"
            L"• AMSI-проверка выбранного файла\r\n\r\n"
            L"DPopGuard ничего не удаляет автоматически: сначала показывает находки пользователю.");
}

void RefreshStartup() {
    ShowListMode(); HideAllActions();
    SetAction(0, L"Обновить список");
    SetAction(1, L"Открыть расположение");
    SetLabel(g_pageTitle, L"Автозагрузка");
    SetLabel(g_pageHint, L"Реальные записи HKCU/HKLM Run и папок Startup пользователя и системы.");
    ClearListColumns();
    AddColumn(0, L"Приложение", 240);
    AddColumn(1, L"Источник", 190);
    AddColumn(2, L"Команда / файл", 520);
    g_startup = dpop::startup::EnumerateAll();
    for (std::size_t i = 0; i < g_startup.size(); ++i) {
        auto& e = g_startup[i];
        LVITEMW item{}; item.mask = LVIF_TEXT | LVIF_PARAM; item.iItem = static_cast<int>(i);
        item.lParam = static_cast<LPARAM>(i); item.pszText = e.name.data();
        const int row = ListView_InsertItem(g_list, &item);
        if (row < 0) continue;
        ListView_SetItemText(g_list, row, 1, e.source.data());
        ListView_SetItemText(g_list, row, 2, e.command.data());
    }
}

void RefreshApps() {
    ShowListMode(); HideAllActions();
    SetAction(0, L"Обновить список");
    SetAction(1, L"Удалить выбранное");
    SetAction(2, L"Найти хвосты");
    SetAction(3, L"Открыть папку");
    SetLabel(g_pageTitle, L"Установленные приложения");
    SetLabel(g_pageHint, L"Данные берутся из реальных Uninstall-разделов Windows. После штатного удаления можно проверить остаточные файлы.");
    ClearListColumns();
    AddColumn(0, L"Приложение", 285);
    AddColumn(1, L"Версия", 110);
    AddColumn(2, L"Издатель", 190);
    AddColumn(3, L"Папка установки", 365);
    g_apps = dpop::apps::EnumerateInstalledApps();
    for (std::size_t i = 0; i < g_apps.size(); ++i) {
        auto& app = g_apps[i];
        LVITEMW item{}; item.mask = LVIF_TEXT | LVIF_PARAM; item.iItem = static_cast<int>(i);
        item.lParam = static_cast<LPARAM>(i); item.pszText = app.displayName.data();
        const int row = ListView_InsertItem(g_list, &item);
        if (row < 0) continue;
        ListView_SetItemText(g_list, row, 1, app.displayVersion.data());
        ListView_SetItemText(g_list, row, 2, app.publisher.data());
        ListView_SetItemText(g_list, row, 3, app.installLocation.data());
    }
}

void ShowZapret() {
    ShowTextMode(); HideAllActions();
    SetAction(0, L"Обновить статус");
    SetAction(1, L"Открыть папку Zapret");
    SetLabel(g_pageTitle, L"Zapret Center");
    SetLabel(g_pageHint, L"DPopCleaner показывает состояние интегрированного Zapret/WinDivert, не затрагивая сторонние VPN.");
    const auto z = dpop::zapret::QueryStatus();
    std::wstring t = L"ZAPRET / WINDIVERT\r\n\r\nСлужба zapret: ";
    t += z.serviceInstalled ? (z.serviceRunning ? L"RUNNING" : L"STOPPED") : L"не установлена";
    t += L"\r\nwinws.exe: ";
    t += z.winwsRunning ? L"RUNNING" : L"не запущен";
    t += L"\r\nПапка: ";
    t += z.detectedFolder.empty() ? L"не обнаружена" : z.detectedFolder.wstring();
    t += L"\r\n\r\nDPopCleaner не завершает сторонние VPN-процессы и не меняет стратегию без явного действия пользователя.";
    SetText(t);
}

void ShowTools() {
    ShowTextMode(); HideAllActions();
    SetAction(0, L"Диспетчер задач");
    SetAction(1, L"Просмотр событий");
    SetAction(2, L"Безопасность Windows");
    SetAction(3, L"Windows Apps");
    SetLabel(g_pageTitle, L"Системные инструменты");
    SetLabel(g_pageHint, L"Быстрый доступ к штатной диагностике Windows.");
    SetText(L"ИНСТРУМЕНТЫ WINDOWS\r\n\r\n"
            L"• Диспетчер задач — процессы, производительность, автозагрузка\r\n"
            L"• Просмотр событий — системные ошибки и журналы приложений\r\n"
            L"• Безопасность Windows — Defender и состояние защиты\r\n"
            L"• Установленные приложения Windows — системная страница управления ПО\r\n\r\n"
            L"Дополнительно из оригинальной концепции DPopCleaner остаются SFC, CHKDSK,\r\n"
            L"Windows Update cleanup и диагностика. Они запускаются только через штатные\r\n"
            L"средства Windows и с подтверждением администратора.");
}

void ShowUpdates() {
    ShowTextMode(); HideAllActions();
    SetAction(0, L"Проверить обновления");
    SetAction(1, L"Открыть страницу релиза");
    SetLabel(g_pageTitle, L"Обновления");
    SetLabel(g_pageHint, L"HTTPS → SHA-256 → Authenticode (для подписанных релизов) → DPopUpdater.");
    SetText(L"Текущая версия: 0.3.1 BETA\r\n"
            L"Внутренний build: Stage 3 Revision 2\r\n\r\n"
            L"DPopCleaner проверяет update/beta.json на сайте. При новой версии пакет загружается\r\n"
            L"во временную папку, сверяется SHA-256 и только затем передаётся DPopUpdater.\r\n\r\n"
            L"Для неподписанной BETA автоматический запуск требует отдельного подтверждения.\r\n"
            L"После подключения Code Signing неподписанные обновления можно будет полностью запретить.");
}

void ShowSettings() {
    ShowTextMode(); HideAllActions();
    SetAction(0, L"Открыть логи");
    SetAction(1, L"Папка DPopCleaner");
    SetAction(2, L"Сайт проекта");
    SetLabel(g_pageTitle, L"Настройки и поддержка");
    SetLabel(g_pageHint, L"Служебные каталоги, журнал DPopCleaner и страница проекта.");
    SetText(L"DPopCleaner 0.3.1 BETA\r\n\r\n"
            L"Данные приложения: %LOCALAPPDATA%\\DPopCleaner\r\n"
            L"Логи: %LOCALAPPDATA%\\DPopCleaner\\Logs\r\n"
            L"Обновления: %LOCALAPPDATA%\\DPopCleaner\\Updates\r\n\r\n"
            L"Темы Midnight / Ocean / Violet / Amber и дополнительные фоновые настройки\r\n"
            L"вернутся после стабилизации Stage 3. Сейчас приоритет — реальные функции и безопасное обновление.");
}

void SwitchPage(Page p) {
    g_page = p;
    InvalidateRect(g_hwnd, nullptr, FALSE);
    switch (p) {
        case Page::Overview: ShowOverview(); break;
        case Page::Cleaning: ShowCleaning(); break;
        case Page::Guard: ShowGuard(); break;
        case Page::Startup: RefreshStartup(); break;
        case Page::Apps: RefreshApps(); break;
        case Page::Zapret: ShowZapret(); break;
        case Page::Tools: ShowTools(); break;
        case Page::Updates: ShowUpdates(); break;
        case Page::Settings: ShowSettings(); break;
    }
}

int SelectedRow() { return ListView_GetNextItem(g_list, -1, LVNI_SELECTED); }

int SelectedAppIndex() {
    const int row = SelectedRow();
    if (row < 0) return -1;
    LVITEMW item{}; item.mask = LVIF_PARAM; item.iItem = row;
    if (!ListView_GetItem(g_list, &item)) return -1;
    const auto idx = static_cast<std::size_t>(item.lParam);
    return idx < g_apps.size() ? static_cast<int>(idx) : -1;
}

int SelectedStartupIndex() {
    const int row = SelectedRow();
    if (row < 0) return -1;
    LVITEMW item{}; item.mask = LVIF_PARAM; item.iItem = row;
    if (!ListView_GetItem(g_list, &item)) return -1;
    const auto idx = static_cast<std::size_t>(item.lParam);
    return idx < g_startup.size() ? static_cast<int>(idx) : -1;
}

std::wstring LeftoversText(const std::vector<dpop::apps::LeftoverItem>& leftovers) {
    std::wstring text = L"После штатного удаления найдены связанные остатки:\n\n";
    std::size_t shown = 0;
    for (const auto& item : leftovers) {
        if (shown++ >= 14) { text += L"\n… и ещё " + std::to_wstring(leftovers.size() - 14) + L" объект(ов)"; break; }
        text += L"• " + item.path.wstring() + L"\n  " + item.reason + L"\n";
    }
    text += L"\nПереместить найденные объекты в Корзину?\n\nУдаление выполняется консервативно: DPopCleaner не стирает произвольные файлы только из-за похожего имени.";
    return text;
}

void ScanAndOfferCleanup(HWND hwnd, const dpop::apps::InstalledApp& app) {
    const auto leftovers = dpop::apps::FindLeftovers(app);
    if (leftovers.empty()) {
        MessageBoxW(hwnd, L"Явных связанных хвостов не найдено.", L"DPopCleaner — остатки", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (MessageBoxW(hwnd, LeftoversText(leftovers).c_str(), L"DPopCleaner — остатки приложения", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return;
    std::size_t removed = 0; std::wstring error;
    dpop::apps::MoveLeftoversToRecycleBin(leftovers, removed, error);
    std::wstring result = L"Перемещено в Корзину: " + std::to_wstring(removed) + L" из " + std::to_wstring(leftovers.size()) + L".";
    if (!error.empty()) result += L"\n\n" + error;
    MessageBoxW(hwnd, result.c_str(), L"DPopCleaner", error.empty() ? MB_ICONINFORMATION : MB_ICONWARNING);
}

void UninstallSelected(HWND hwnd) {
    const int index = SelectedAppIndex();
    if (index < 0) { MessageBoxW(hwnd, L"Сначала выбери программу в списке.", L"DPopCleaner", MB_OK | MB_ICONINFORMATION); return; }
    const auto app = g_apps[static_cast<std::size_t>(index)];
    const std::wstring q = L"Запустить штатный деинсталлятор:\n\n" + app.displayName + L" " + app.displayVersion +
        L"\n\nПосле завершения DPopCleaner предложит отдельный поиск связанных хвостов.";
    if (MessageBoxW(hwnd, q.c_str(), L"Удаление приложения", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return;
    DWORD exitCode = 0; std::wstring error;
    if (!dpop::apps::RunUninstaller(app, exitCode, error)) {
        MessageBoxW(hwnd, error.c_str(), L"Не удалось удалить приложение", MB_OK | MB_ICONERROR); return;
    }
    RefreshApps();
    ScanAndOfferCleanup(hwnd, app);
}

void CheckUpdatesAsync(HWND hwnd, bool interactive) {
    if (interactive) {
        g_page = Page::Updates;
        ShowUpdates();
        SetText(L"Проверяем update/beta.json…");
    }
    std::thread([hwnd, interactive] {
        auto* message = new UpdateMessage{dpop::update::CheckForUpdates(), interactive};
        PostMessageW(hwnd, WM_UPDATE_RESULT, 0, reinterpret_cast<LPARAM>(message));
    }).detach();
}

void HandleUpdateResult(HWND hwnd, UpdateMessage* raw) {
    std::unique_ptr<UpdateMessage> message(raw);
    auto& r = message->result;
    if (!r.success) {
        if (message->interactive) SetText(L"Ошибка проверки обновлений:\r\n\r\n" + r.error);
        return;
    }
    if (!r.updateAvailable) {
        if (message->interactive) SetText(L"Установлена актуальная версия DPopCleaner 0.3.1 BETA.\r\n\r\nНовых BETA-релизов сейчас нет.");
        return;
    }

    const auto& m = r.manifest;
    const std::wstring q = L"Доступно обновление DPopCleaner " + m.version + L".\n\nСкачать пакет и проверить SHA-256?";
    if (MessageBoxW(hwnd, q.c_str(), L"DPopCleaner Update", MB_YESNO | MB_ICONINFORMATION) != IDYES) return;

    SetText(L"Загружаем обновление " + m.version + L"…");
    fs::path file; std::wstring error;
    if (!dpop::update::DownloadPackage(m, file, error)) { SetText(L"Обновление не загружено:\r\n\r\n" + error); return; }

    bool allowUnsigned = false;
    if (!m.signedPackage) {
        const std::wstring warning = L"SHA-256 совпал, но BETA-пакет пока не подписан Authenticode.\n\n"
                                     L"Запустить проверенный по SHA-256 установщик вручную?";
        if (MessageBoxW(hwnd, warning.c_str(), L"Неподписанное BETA-обновление", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) {
            SetText(L"Обновление скачано и проверено, но автоматический запуск отменён.\r\n\r\n" + file.wstring());
            ShellExecuteW(hwnd, L"open", file.parent_path().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            return;
        }
        allowUnsigned = true;
    }

    if (!dpop::update::PrepareAndLaunchUpdater(m, file, allowUnsigned, error)) {
        SetText(L"Запуск обновления остановлен:\r\n\r\n" + error); return;
    }
    DestroyWindow(hwnd);
}

void OpenTool(const wchar_t* file, const wchar_t* args = nullptr) {
    ShellExecuteW(g_hwnd, L"open", file, args, nullptr, SW_SHOWNORMAL);
}

void RunGuardScan() {
    const auto r = dpop::guard::QuickScan();
    std::wostringstream o;
    o << L"DPOPGuard 2 — РЕЗУЛЬТАТ\r\n\r\n"
      << L"Проверено процессов: " << r.processesChecked << L"\r\n"
      << L"Проверено записей автозапуска: " << r.startupChecked << L"\r\n"
      << L"Находок: " << r.findings.size() << L"\r\n\r\n";
    if (r.findings.empty()) o << L"Явных подозрительных признаков в быстрой проверке не найдено.\r\n";
    for (const auto& f : r.findings) o << L"[" << f.severity << L"] " << f.title << L"\r\n" << f.details << L"\r\n\r\n";
    o << L"\r\n" << r.note;
    SetText(o.str());
}

void ScanFileAmsi() {
    wchar_t file[MAX_PATH * 4]{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = g_hwnd; ofn.lpstrFile = file;
    ofn.nMaxFile = static_cast<DWORD>(std::size(file));
    ofn.lpstrFilter = L"Все файлы\0*.*\0Исполняемые файлы\0*.exe;*.dll;*.scr;*.com\0\0";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return;
    std::wstring verdict, error;
    if (!dpop::guard::ScanFileWithAmsi(file, verdict, error)) verdict = L"Ошибка AMSI: " + error;
    SetText(L"Проверенный файл:\r\n" + std::wstring(file) + L"\r\n\r\n" + verdict +
            L"\r\n\r\nAMSI использует установленные в Windows средства защиты. Отсутствие срабатывания не является абсолютной гарантией безопасности.");
}

void HandleAction(int index) {
    switch (g_page) {
        case Page::Overview:
            if (index == 0) ShowOverview();
            else if (index == 1) CheckUpdatesAsync(g_hwnd, true);
            break;
        case Page::Cleaning:
            if (index == 0) {
                const auto r = dpop::cleaner::CleanUserTemp();
                MessageBoxW(g_hwnd, (L"TEMP очищен.\nФайлов: " + std::to_wstring(r.removedFiles) + L"\nОсвобождено: " + Bytes(r.removedBytes)).c_str(), L"DPopCleaner", MB_OK | MB_ICONINFORMATION);
                ShowCleaning();
            } else if (index == 1) {
                const auto r = dpop::cleaner::CleanBrowserCaches();
                MessageBoxW(g_hwnd, (L"Кэши очищены.\nФайлов: " + std::to_wstring(r.removedFiles) + L"\nОсвобождено: " + Bytes(r.removedBytes)).c_str(), L"DPopCleaner", MB_OK | MB_ICONINFORMATION);
                ShowCleaning();
            } else if (index == 2) {
                const auto r = dpop::cleaner::CleanCrashDumps();
                MessageBoxW(g_hwnd, (L"CrashDumps очищены.\nОсвобождено: " + Bytes(r.removedBytes)).c_str(), L"DPopCleaner", MB_OK | MB_ICONINFORMATION);
                ShowCleaning();
            } else if (index == 3) {
                std::wstring e;
                if (!dpop::cleaner::EmptyRecycleBin(e)) MessageBoxW(g_hwnd, e.c_str(), L"DPopCleaner", MB_OK | MB_ICONERROR);
                ShowCleaning();
            }
            break;
        case Page::Guard:
            if (index == 0) RunGuardScan();
            else if (index == 1) ScanFileAmsi();
            break;
        case Page::Startup:
            if (index == 0) RefreshStartup();
            else if (index == 1) {
                const int i = SelectedStartupIndex();
                if (i < 0) { MessageBoxW(g_hwnd, L"Выбери запись автозагрузки.", L"DPopCleaner", MB_OK | MB_ICONINFORMATION); break; }
                const auto& e = g_startup[static_cast<std::size_t>(i)];
                if (!e.location.empty()) ShellExecuteW(g_hwnd, L"open", e.location.parent_path().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                else MessageBoxW(g_hwnd, (L"Источник: " + e.source + L"\n\n" + e.command).c_str(), L"Автозагрузка", MB_OK | MB_ICONINFORMATION);
            }
            break;
        case Page::Apps:
            if (index == 0) RefreshApps();
            else if (index == 1) UninstallSelected(g_hwnd);
            else if (index == 2) {
                const int i = SelectedAppIndex();
                if (i < 0) MessageBoxW(g_hwnd, L"Выбери приложение.", L"DPopCleaner", MB_OK | MB_ICONINFORMATION);
                else ScanAndOfferCleanup(g_hwnd, g_apps[static_cast<std::size_t>(i)]);
            } else if (index == 3) {
                const int i = SelectedAppIndex();
                if (i < 0) break;
                const auto& p = g_apps[static_cast<std::size_t>(i)].installLocation;
                if (!p.empty()) ShellExecuteW(g_hwnd, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }
            break;
        case Page::Zapret:
            if (index == 0) ShowZapret();
            else if (index == 1) { std::wstring e; if (!dpop::zapret::OpenDetectedFolder(e)) MessageBoxW(g_hwnd, e.c_str(), L"DPopCleaner", MB_OK | MB_ICONINFORMATION); }
            break;
        case Page::Tools:
            if (index == 0) OpenTool(L"taskmgr.exe");
            else if (index == 1) OpenTool(L"eventvwr.msc");
            else if (index == 2) OpenTool(L"windowsdefender:");
            else if (index == 3) OpenTool(L"ms-settings:appsfeatures");
            break;
        case Page::Updates:
            if (index == 0) CheckUpdatesAsync(g_hwnd, true);
            else if (index == 1) OpenTool(L"https://github.com/elesnichenko1-droid/dpopcleaner-site/releases");
            break;
        case Page::Settings:
            if (index == 0) ShellExecuteW(g_hwnd, L"open", dpop::paths::LogsDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            else if (index == 1) ShellExecuteW(g_hwnd, L"open", dpop::paths::DataDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            else if (index == 2) OpenTool(L"https://elesnichenko1-droid.github.io/dpopcleaner-site/");
            break;
    }
}

void DrawButton(const DRAWITEMSTRUCT* dis) {
    wchar_t text[256]{};
    GetWindowTextW(dis->hwndItem, text, static_cast<int>(std::size(text)));
    const int id = GetDlgCtrlID(dis->hwndItem);
    bool selected = false;
    if (id >= ID_NAV_OVERVIEW && id <= ID_NAV_SETTINGS) {
        const int pageIndex = static_cast<int>(g_page);
        selected = (id - ID_NAV_OVERVIEW) == pageIndex;
    }
    COLORREF fill = selected ? RGB(25, 73, 65) : C_CARD;
    if (id >= ID_ACTION1 && id <= ID_ACTION4) fill = (id == ID_ACTION1) ? RGB(42, 118, 79) : C_CARD2;
    if (dis->itemState & ODS_SELECTED) fill = RGB(35, 95, 76);

    HBRUSH brush = CreateSolidBrush(fill);
    HBRUSH border = CreateSolidBrush(C_BORDER);
    FillRect(dis->hDC, &dis->rcItem, brush);
    FrameRect(dis->hDC, &dis->rcItem, border);
    DeleteObject(brush); DeleteObject(border);

    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, C_TEXT);
    HFONT old = reinterpret_cast<HFONT>(SelectObject(dis->hDC, g_font));
    RECT r = dis->rcItem; r.left += 14; r.right -= 10;
    DrawTextW(dis->hDC, text, -1, &r, DT_SINGLELINE | DT_VCENTER | ((id >= ID_NAV_OVERVIEW && id <= ID_NAV_SETTINGS) ? DT_LEFT : DT_CENTER));
    SelectObject(dis->hDC, old);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            g_hwnd = hwnd;
            BOOL dark = TRUE;
            DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
            INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_LISTVIEW_CLASSES};
            InitCommonControlsEx(&icc);

            g_bgBrush = CreateSolidBrush(C_BG);
            g_cardBrush = CreateSolidBrush(C_CARD);
            g_font = CreateFontW(-18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
            g_fontSmall = CreateFontW(-15,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
            g_fontTitle = CreateFontW(-30,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
            g_fontPage = CreateFontW(-25,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");

            g_title = CreateWindowW(L"STATIC", L"DPopCleaner", WS_CHILD|WS_VISIBLE, 28, 20, 210, 38, hwnd, nullptr, nullptr, nullptr);
            SendMessageW(g_title, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontTitle), TRUE);
            g_build = CreateWindowW(L"STATIC", L"0.3.1 BETA  •  Stage 3", WS_CHILD|WS_VISIBLE, 30, 57, 220, 24, hwnd, nullptr, nullptr, nullptr);
            SendMessageW(g_build, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontSmall), TRUE);

            const wchar_t* navText[] = {L"Обзор", L"Очистка", L"DPopGuard 2", L"Автозагрузка", L"Приложения", L"Zapret Center", L"Инструменты", L"Обновления", L"Настройки"};
            for (int i = 0; i < 9; ++i) {
                g_nav[i] = CreateWindowW(L"BUTTON", navText[i], WS_CHILD|WS_VISIBLE|BS_OWNERDRAW, 22, 105 + i*52, 218, 42, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_NAV_OVERVIEW+i)), nullptr, nullptr);
            }

            g_pageTitle = CreateWindowW(L"STATIC", L"", WS_CHILD|WS_VISIBLE, 285, 26, 830, 34, hwnd, nullptr, nullptr, nullptr);
            SendMessageW(g_pageTitle, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontPage), TRUE);
            g_pageHint = CreateWindowW(L"STATIC", L"", WS_CHILD|WS_VISIBLE, 286, 62, 865, 28, hwnd, nullptr, nullptr, nullptr);
            SendMessageW(g_pageHint, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontSmall), TRUE);

            g_text = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD|WS_VISIBLE|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL,
                                     285, 104, 885, 475, hwnd, reinterpret_cast<HMENU>(ID_TEXT), nullptr, nullptr);
            SendMessageW(g_text, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);

            g_list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD|LVS_REPORT|LVS_SINGLESEL|WS_BORDER,
                                     285, 104, 885, 475, hwnd, reinterpret_cast<HMENU>(ID_LIST), nullptr, nullptr);
            SendMessageW(g_list, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontSmall), TRUE);
            ConfigureListDark();

            for (int i = 0; i < 4; ++i) {
                g_actions[i] = CreateWindowW(L"BUTTON", L"", WS_CHILD|BS_OWNERDRAW, 285 + i*218, 598, 205, 44,
                                             hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_ACTION1+i)), nullptr, nullptr);
            }

            SwitchPage(Page::Overview);
            SetTimer(hwnd, ID_STARTUP_UPDATE_TIMER, 5000, nullptr);
            return 0;
        }
        case WM_TIMER:
            if (wp == ID_STARTUP_UPDATE_TIMER) {
                KillTimer(hwnd, ID_STARTUP_UPDATE_TIMER);
                CheckUpdatesAsync(hwnd, false);
                return 0;
            }
            break;
        case WM_COMMAND: {
            const int id = LOWORD(wp);
            if (id >= ID_NAV_OVERVIEW && id <= ID_NAV_SETTINGS) {
                SwitchPage(static_cast<Page>(id - ID_NAV_OVERVIEW));
                return 0;
            }
            if (id >= ID_ACTION1 && id <= ID_ACTION4) {
                HandleAction(id - ID_ACTION1);
                return 0;
            }
            break;
        }
        case WM_DRAWITEM:
            DrawButton(reinterpret_cast<DRAWITEMSTRUCT*>(lp));
            return TRUE;
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wp);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, reinterpret_cast<HWND>(lp) == g_build || reinterpret_cast<HWND>(lp) == g_pageHint ? C_MUTED : C_TEXT);
            return reinterpret_cast<LRESULT>(g_bgBrush);
        }
        case WM_CTLCOLOREDIT: {
            HDC dc = reinterpret_cast<HDC>(wp);
            SetBkColor(dc, C_CARD);
            SetTextColor(dc, C_TEXT);
            return reinterpret_cast<LRESULT>(g_cardBrush);
        }
        case WM_ERASEBKGND: {
            RECT rc{}; GetClientRect(hwnd, &rc);
            HDC dc = reinterpret_cast<HDC>(wp);
            FillRect(dc, &rc, g_bgBrush);
            RECT side{0,0,260,rc.bottom};
            HBRUSH sideBrush = CreateSolidBrush(C_SIDEBAR);
            FillRect(dc, &side, sideBrush);
            DeleteObject(sideBrush);
            return 1;
        }
        case WM_UPDATE_RESULT:
            HandleUpdateResult(hwnd, reinterpret_cast<UpdateMessage*>(lp));
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, ID_STARTUP_UPDATE_TIMER);
            if (g_font) DeleteObject(g_font);
            if (g_fontSmall) DeleteObject(g_fontSmall);
            if (g_fontTitle) DeleteObject(g_fontTitle);
            if (g_fontPage) DeleteObject(g_fontPage);
            if (g_bgBrush) DeleteObject(g_bgBrush);
            if (g_cardBrush) DeleteObject(g_cardBrush);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
}

namespace dpop::ui {
int Run(HINSTANCE instance, int showCommand) {
    dpop::paths::EnsureDirectories();
    dpop::log::Info(L"DPopCleaner 0.3.1 Stage 3 Revision 2 started");

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpfnWndProc = WndProc;
    wc.lpszClassName = L"DPopCleanerMainV3";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
    wc.hbrBackground = nullptr;
    if (!RegisterClassExW(&wc)) return 1;

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"DPopCleaner 0.3.1 BETA — Stage 3",
                                WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
                                CW_USEDEFAULT, CW_USEDEFAULT, 1215, 715,
                                nullptr, nullptr, instance, nullptr);
    if (!hwnd) return 2;
    ShowWindow(hwnd, showCommand);
    UpdateWindow(hwnd);
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return static_cast<int>(msg.wParam);
}
}
