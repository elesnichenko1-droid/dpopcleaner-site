#include "MainWindow.h"

#include "AppSettings.h"
#include "UpdateClient.h"
#include "UpdateManifest.h"
#include "Version.h"
#include "ZapretController.h"

#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>

#include <atomic>
#include <filesystem>
#include <iterator>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")

namespace dpop0418 {
namespace {
namespace fs = std::filesystem;

constexpr COLORREF C_BG      = RGB(8, 24, 38);
constexpr COLORREF C_SIDE    = RGB(10, 29, 45);
constexpr COLORREF C_CARD    = RGB(14, 39, 58);
constexpr COLORREF C_CARD2   = RGB(18, 49, 68);
constexpr COLORREF C_BORDER  = RGB(39, 73, 91);
constexpr COLORREF C_TEXT    = RGB(239, 247, 250);
constexpr COLORREF C_MUTED   = RGB(151, 177, 191);

constexpr int ID_NAV_BASE = 1000;
constexpr int ID_ACTION_BASE = 2000;
constexpr int ID_CONTENT = 3000;
constexpr UINT WM_UPDATE_EVENT = WM_APP + 41;
constexpr UINT_PTR ID_STARTUP_UPDATE_TIMER = 4101;

constexpr int NAV_COUNT = 8;
constexpr int ACTION_COUNT = 4;

enum class Page {
    Overview = 0,
    Cleaning,
    DiskAnalyzer,
    RestoreCenter,
    Zapret,
    ZapretFix,
    Updates,
    Settings
};

enum class PendingKind {
    Check,
    Download,
    Zapret
};

enum class ZapretOperation {
    Start,
    Stop,
    InstallService,
    RemoveService
};

struct PendingEvent {
    PendingKind kind{PendingKind::Check};
    bool interactive{};
    UpdateCheckResult check{};
    UpdateManifest manifest{};
    bool success{};
    fs::path package;
    std::wstring message;
    std::wstring error;
};

HWND gMainWindow{};
HWND gTitle{};
HWND gVersion{};
HWND gPageTitle{};
HWND gPageHint{};
HWND gContent{};
HWND gNav[NAV_COUNT]{};
HWND gActions[ACTION_COUNT]{};
HFONT gFont{};
HFONT gFontSmall{};
HFONT gFontTitle{};
HFONT gFontPage{};
HBRUSH gBackgroundBrush{};
HBRUSH gCardBrush{};
Page gPage = Page::Overview;
AppSettings gSettings{};
std::atomic_bool gShuttingDown{false};
std::atomic_bool gUpdateBusy{false};
std::atomic_bool gZapretBusy{false};
std::atomic_bool gUpdaterDriven{false};
std::mutex gPendingMutex;
std::optional<PendingEvent> gPendingEvent;

fs::path ExecutableDirectory() {
    wchar_t buffer[32768]{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    if (length == 0 || length >= std::size(buffer)) return fs::current_path();
    return fs::path(std::wstring(buffer, length)).parent_path();
}

fs::path SettingsPath() {
    wchar_t overridePath[32768]{};
    const DWORD length = GetEnvironmentVariableW(
        L"DPOP0418_SETTINGS_PATH", overridePath, static_cast<DWORD>(std::size(overridePath)));
    if (length > 0 && length < std::size(overridePath)) return fs::path(overridePath);
    return AppDataDirectory() / L"settings.ini";
}

fs::path LogsDirectory() {
    return AppDataDirectory() / L"Logs";
}

fs::path ZapretRoot() {
    return BundledZapretRoot(ExecutableDirectory());
}

void EnsureUserDirectories() {
    std::error_code ec;
    fs::create_directories(AppDataDirectory(), ec);
    ec.clear();
    fs::create_directories(UpdatesDirectory(), ec);
    ec.clear();
    fs::create_directories(LogsDirectory(), ec);
}

void SetContent(const std::wstring& text) {
    if (gContent) SetWindowTextW(gContent, text.c_str());
}

void SetLabel(HWND control, const wchar_t* text) {
    if (control) SetWindowTextW(control, text);
}

void SetAction(int index, const wchar_t* text, bool visible = true) {
    if (index < 0 || index >= ACTION_COUNT) return;
    SetLabel(gActions[index], text);
    ShowWindow(gActions[index], visible ? SW_SHOW : SW_HIDE);
}

void SetActionEnabled(int index, bool enabled) {
    if (index < 0 || index >= ACTION_COUNT || !gActions[index]) return;
    EnableWindow(gActions[index], enabled ? TRUE : FALSE);
}

void HideActions() {
    for (HWND button : gActions) {
        EnableWindow(button, TRUE);
        ShowWindow(button, SW_HIDE);
    }
}

void OpenTarget(const fs::path& target) {
    ShellExecuteW(gMainWindow, L"open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void OpenUrl(const wchar_t* url) {
    ShellExecuteW(gMainWindow, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
}

void OpenModule(const wchar_t* filename) {
    const fs::path module = ExecutableDirectory() / L"Modules" / filename;
    if (!fs::exists(module)) {
        const std::wstring message = L"Модуль не найден:\n" + module.wstring();
        MessageBoxW(gMainWindow, message.c_str(), L"DPopCleaner", MB_OK | MB_ICONWARNING);
        return;
    }
    OpenTarget(module);
}

std::wstring SelectedZapretStrategyLabel() {
    const fs::path selected(gSettings.zapretStrategy);
    const std::wstring stem = selected.stem().wstring();
    return stem.empty() ? std::wstring(L"не выбрана") : stem;
}

void ShowOverview() {
    HideActions();
    SetAction(0, L"Проверить обновления");
    SetAction(1, L"Настройки");
    SetLabel(gPageTitle, L"Обзор");
    SetLabel(gPageHint, L"DPopCleaner 0.4.18 — основное окно, безопасные инструменты и проверяемые обновления.");
    SetContent(
        L"DPopCleaner 0.4.18\r\n\r\n"
        L"В этой версии обновлено само основное приложение.\r\n\r\n"
        L"• исправлено закрытие окна — приложение не ждёт сетевые проверки;\r\n"
        L"• добавлена настройка автоматической проверки обновлений;\r\n"
        L"• обновления идут только по stable-манифесту через HTTPS;\r\n"
        L"• установщик принимается только после проверки размера и SHA-256;\r\n"
        L"• Flowseal Zapret 1.10.2 встроен как управляемый bundled-компонент;\r\n"
        L"• Disk Analyzer, Restore Center и Zapret Screen Fix остаются отдельными модулями.\r\n\r\n"
        L"Автоматическая проверка обновлений: " +
        std::wstring(gSettings.autoCheckUpdates ? L"ВКЛ" : L"ВЫКЛ") + L".");
}

void ShowCleaning() {
    HideActions();
    SetAction(0, L"Очистка диска Windows");
    SetAction(1, L"Контроль памяти");
    SetAction(2, L"TEMP пользователя");
    SetLabel(gPageTitle, L"Очистка");
    SetLabel(gPageHint, L"Безопасные точки входа в штатную очистку Windows и временные файлы пользователя.");
    SetContent(
        L"ОЧИСТКА\r\n\r\n"
        L"DPopCleaner не удаляет пользовательские документы и профили приложений без отдельного действия.\r\n\r\n"
        L"• «Очистка диска Windows» запускает cleanmgr.exe.\r\n"
        L"• «Контроль памяти» открывает системные параметры Storage Sense.\r\n"
        L"• «TEMP пользователя» открывает текущую папку временных файлов для ручного просмотра.\r\n\r\n"
        L"Более глубокую очистку будем расширять отдельно, не смешивая её с updater lifecycle.");
}

void ShowDiskAnalyzer() {
    HideActions();
    SetAction(0, L"Открыть Анализатор диска");
    SetLabel(gPageTitle, L"Анализатор диска");
    SetLabel(gPageHint, L"Иерархический анализ занятого места с процентами родителя и физическим размером, когда он известен.");
    SetContent(
        L"АНАЛИЗАТОР ДИСКА\r\n\r\n"
        L"Запускается отдельный проверенный модуль DiskAnalyzer.exe.\r\n"
        L"Он не удаляет файлы автоматически и не следует reparse points по умолчанию.");
}

void ShowRestoreCenter() {
    HideActions();
    SetAction(0, L"Открыть Центр восстановления");
    SetLabel(gPageTitle, L"Центр восстановления");
    SetLabel(gPageHint, L"История действий и откат только там, где DPopCleaner сохранил достаточно данных для восстановления.");
    SetContent(
        L"ЦЕНТР ВОССТАНОВЛЕНИЯ\r\n\r\n"
        L"RestoreCenter.exe показывает историю и резервные данные DPopCleaner.\r\n"
        L"Обновление приложения не удаляет пользовательскую историю и backups.");
}

void ShowZapret() {
    HideActions();
    const fs::path root = ZapretRoot();
    const ZapretStatus status = QueryZapretStatus(root);
    const auto strategies = EnumerateZapretStrategies(root);
    const bool busy = gZapretBusy.load(std::memory_order_acquire);

    SetAction(0, L"Запустить Zapret");
    SetAction(1, L"Остановить Zapret");
    const std::wstring strategyAction = L"Стратегия Zapret: " + SelectedZapretStrategyLabel();
    SetAction(2, strategyAction.c_str());

    if (status.serviceInstalled && status.bundledServiceOwned) {
        SetAction(3, L"Удалить службу");
    } else if (status.serviceInstalled && !status.bundledServiceOwned) {
        SetAction(3, L"Внешняя служба zapret");
        SetActionEnabled(3, false);
    } else {
        SetAction(3, L"Установить службу");
    }

    if (busy) {
        for (int i = 0; i < ACTION_COUNT; ++i) SetActionEnabled(i, false);
    } else {
        SetActionEnabled(0, status.payloadIntegrityOk && !status.serviceRunning && !status.externalWinwsRunning);
        SetActionEnabled(1, status.bundledWinwsRunning || (status.serviceRunning && status.bundledServiceOwned));
        SetActionEnabled(2, status.payloadIntegrityOk && strategies.size() > 1 && !status.serviceRunning);
        if (!status.serviceInstalled) SetActionEnabled(3, status.payloadIntegrityOk && !status.externalWinwsRunning);
    }

    SetLabel(gPageTitle, L"Zapret");
    SetLabel(gPageHint, L"Встроенный Flowseal Zapret 1.10.2: запуск, остановка, стратегия и служба без управления внешними экземплярами.");

    std::wstring payloadText = status.payloadIntegrityOk ? L"готов и проверен" :
                               (status.payloadAvailable ? L"найден, но проверка не пройдена" : L"не установлен");
    std::wstring runtimeText = status.bundledWinwsRunning ? L"запущен" : L"остановлен";
    std::wstring serviceText;
    if (!status.serviceInstalled) serviceText = L"не установлена";
    else if (!status.bundledServiceOwned) serviceText = L"внешняя — DPopCleaner её не изменяет";
    else serviceText = status.serviceRunning ? L"установлена и запущена" : L"установлена, остановлена";

    std::wstring content =
        L"ZAPRET 1.10.2\r\n\r\n"
        L"Bundled payload: " + payloadText + L"\r\n"
        L"Standalone winws: " + runtimeText + L"\r\n"
        L"Служба zapret: " + serviceText + L"\r\n"
        L"Выбранная стратегия: " + gSettings.zapretStrategy + L"\r\n";

    if (!status.serviceStrategy.empty())
        content += L"Стратегия службы: " + status.serviceStrategy + L"\r\n";
    if (status.externalWinwsRunning)
        content += L"\r\nВНИМАНИЕ: обнаружен внешний winws.exe. DPopCleaner не завершает и не перезаписывает его.\r\n";
    if (!status.error.empty())
        content += L"\r\nПроверка bundled payload: " + status.error + L"\r\n";
    if (busy)
        content += L"\r\nОперация Zapret выполняется… окно DPopCleaner при этом остаётся неблокирующим.\r\n";

    content +=
        L"\r\nКнопка «Стратегия Zapret» переключает только стратегии из bundled-каталога и сразу сохраняет выбор.\r\n"
        L"Установка/удаление службы требует UAC. Служба с тем же именем, но другим путём, считается внешней и не меняется.\r\n"
        L"Discord Screen Fix остаётся отдельной страницей и не подменяет управление Zapret.";
    SetContent(content);
}

void ShowZapretFix() {
    HideActions();
    SetAction(0, L"Открыть Zapret Screen Fix");
    SetLabel(gPageTitle, L"Zapret Screen Fix");
    SetLabel(gPageHint, L"Фикс для Discord screen sharing с резервной копией и возможностью отката.");
    SetContent(
        L"ZAPRET SCREEN FIX\r\n\r\n"
        L"Модуль исправляет нужный discord.media TCP-фильтр, не переписывая посторонние стратегии.\r\n"
        L"Перед изменением создаётся backup; повторное применение идемпотентно.\r\n\r\n"
        L"Запуск, остановка и установка Flowseal Zapret теперь находятся на отдельной встроенной странице «Zapret»." );
}

void ShowUpdates() {
    HideActions();
    SetAction(0, L"Проверить обновления сейчас");
    SetAction(1, L"Открыть страницу релизов");
    SetLabel(gPageTitle, L"Обновления");
    SetLabel(gPageHint, L"Stable: HTTPS → exact size → SHA-256 → DPopUpdater → повторная SHA-проверка → установщик.");
    SetContent(
        L"Текущая версия: 0.4.18 rev.1\r\n"
        L"Канал: stable\r\n\r\n"
        L"Манифест:\r\n"
        L"https://elesnichenko1-droid.github.io/dpopcleaner-site/update/stable.json\r\n\r\n"
        L"Автоматическая проверка: " +
        std::wstring(gSettings.autoCheckUpdates ? L"ВКЛ" : L"ВЫКЛ") +
        L"\r\n\r\n"
        L"Даже при включённой автоматической проверке optional-обновление не скачивается и не устанавливается без подтверждения пользователя.");
}

void ShowSettings() {
    HideActions();
    SetAction(0, gSettings.autoCheckUpdates ? L"Автообновление: ВКЛ" : L"Автообновление: ВЫКЛ");
    SetAction(1, L"Проверить обновления сейчас");
    SetAction(2, L"Открыть логи");
    SetAction(3, L"Сайт проекта");
    SetLabel(gPageTitle, L"Настройки");
    SetLabel(gPageHint, L"Настройки DPopCleaner сохраняются для текущего пользователя.");
    SetContent(
        L"НАСТРОЙКИ\r\n\r\n"
        L"Автоматически проверять обновления: " +
        std::wstring(gSettings.autoCheckUpdates ? L"Включено" : L"Выключено") +
        L"\r\n"
        L"Стратегия Zapret: " + gSettings.zapretStrategy +
        L"\r\n\r\n"
        L"Файл настроек:\r\n" + SettingsPath().wstring() +
        L"\r\n\r\n"
        L"Выключение автоматической проверки не отключает кнопку «Проверить обновления сейчас».\r\n"
        L"Изменения настроек сохраняются сразу.");
}

void SwitchPage(Page page) {
    gPage = page;
    switch (page) {
        case Page::Overview: ShowOverview(); break;
        case Page::Cleaning: ShowCleaning(); break;
        case Page::DiskAnalyzer: ShowDiskAnalyzer(); break;
        case Page::RestoreCenter: ShowRestoreCenter(); break;
        case Page::Zapret: ShowZapret(); break;
        case Page::ZapretFix: ShowZapretFix(); break;
        case Page::Updates: ShowUpdates(); break;
        case Page::Settings: ShowSettings(); break;
    }
    if (gMainWindow) InvalidateRect(gMainWindow, nullptr, FALSE);
}

void PublishPending(HWND hwnd, PendingEvent event) {
    if (gShuttingDown.load(std::memory_order_acquire)) return;
    {
        std::lock_guard<std::mutex> guard(gPendingMutex);
        if (gShuttingDown.load(std::memory_order_acquire)) return;
        gPendingEvent = std::move(event);
    }
    if (!gShuttingDown.load(std::memory_order_acquire) && IsWindow(hwnd)) {
        PostMessageW(hwnd, WM_UPDATE_EVENT, 0, 0);
    }
}

void StartUpdateCheck(HWND hwnd, bool interactive) {
    if (gShuttingDown.load(std::memory_order_acquire)) return;
    bool expected = false;
    if (!gUpdateBusy.compare_exchange_strong(expected, true)) {
        if (interactive) {
            MessageBoxW(hwnd, L"Проверка или загрузка обновления уже выполняется.",
                        L"DPopCleaner", MB_OK | MB_ICONINFORMATION);
        }
        return;
    }

    if (interactive) {
        gPage = Page::Updates;
        ShowUpdates();
        SetContent(L"Проверяем stable-канал обновлений…");
    }

    std::thread([hwnd, interactive] {
        PendingEvent event{};
        event.kind = PendingKind::Check;
        event.interactive = interactive;
        event.check = CheckStableUpdates(&gShuttingDown);
        gUpdateBusy.store(false, std::memory_order_release);
        PublishPending(hwnd, std::move(event));
    }).detach();
}

void StartPackageDownload(HWND hwnd, const UpdateManifest& manifest) {
    if (gShuttingDown.load(std::memory_order_acquire)) return;
    bool expected = false;
    if (!gUpdateBusy.compare_exchange_strong(expected, true)) return;

    gPage = Page::Updates;
    ShowUpdates();
    SetContent(L"Загружаем DPopCleaner " + manifest.version + L"…\r\n\r\nПосле загрузки обязательно проверим точный размер и SHA-256.");

    std::thread([hwnd, manifest] {
        PendingEvent event{};
        event.kind = PendingKind::Download;
        event.interactive = true;
        event.manifest = manifest;
        event.success = DownloadVerifiedPackage(manifest, event.package, event.error, &gShuttingDown);
        gUpdateBusy.store(false, std::memory_order_release);
        PublishPending(hwnd, std::move(event));
    }).detach();
}

void StartZapretOperation(HWND hwnd, ZapretOperation operation) {
    if (gShuttingDown.load(std::memory_order_acquire)) return;
    bool expected = false;
    if (!gZapretBusy.compare_exchange_strong(expected, true)) {
        MessageBoxW(hwnd, L"Другая операция Zapret уже выполняется.", L"DPopCleaner Zapret",
                    MB_OK | MB_ICONINFORMATION);
        return;
    }

    const fs::path root = ZapretRoot();
    const std::wstring selectedStrategy = gSettings.zapretStrategy;
    if (gPage == Page::Zapret) ShowZapret();

    std::thread([hwnd, operation, root, selectedStrategy] {
        PendingEvent event{};
        event.kind = PendingKind::Zapret;
        event.interactive = true;
        switch (operation) {
            case ZapretOperation::Start:
                event.message = L"Bundled Zapret запущен.";
                event.success = StartBundledZapret(selectedStrategy, root, event.error);
                break;
            case ZapretOperation::Stop:
                event.message = L"Bundled Zapret остановлен.";
                event.success = StopBundledZapret(root, event.error);
                break;
            case ZapretOperation::InstallService:
                event.message = L"Служба bundled Zapret установлена для выбранной стратегии.";
                event.success = InstallBundledZapretService(selectedStrategy, root, event.error);
                break;
            case ZapretOperation::RemoveService:
                event.message = L"Служба bundled Zapret удалена.";
                event.success = RemoveBundledZapretService(root, event.error);
                break;
        }
        gZapretBusy.store(false, std::memory_order_release);
        PublishPending(hwnd, std::move(event));
    }).detach();
}

void HandleCheckFinished(HWND hwnd, const PendingEvent& event) {
    const UpdateCheckResult& result = event.check;
    if (!result.success) {
        if (event.interactive && !gShuttingDown.load(std::memory_order_acquire)) {
            SetContent(L"Не удалось проверить обновления:\r\n\r\n" + result.error);
        }
        return;
    }

    if (!result.updateAvailable) {
        if (event.interactive) {
            SetContent(L"DPopCleaner 0.4.18 rev.1 — актуальная версия.\r\n\r\nНовых stable-обновлений сейчас нет.");
        }
        return;
    }

    if (gShuttingDown.load(std::memory_order_acquire)) return;
    const std::wstring prompt =
        L"Доступно обновление DPopCleaner " + result.manifest.version +
        L" rev." + std::to_wstring(result.manifest.revision) +
        L".\n\nСкачать установщик и проверить его размер + SHA-256?";
    if (MessageBoxW(hwnd, prompt.c_str(), L"DPopCleaner Update",
                    MB_YESNO | MB_ICONINFORMATION | MB_DEFBUTTON1) != IDYES) {
        if (event.interactive) SetContent(L"Обновление доступно, но загрузка отменена пользователем.");
        return;
    }
    StartPackageDownload(hwnd, result.manifest);
}

void HandleDownloadFinished(HWND hwnd, const PendingEvent& event) {
    if (!event.success) {
        if (!gShuttingDown.load(std::memory_order_acquire)) {
            SetContent(L"Обновление не принято:\r\n\r\n" + event.error +
                       L"\r\n\r\nТекущая установка DPopCleaner не изменена.");
        }
        return;
    }
    if (gShuttingDown.load(std::memory_order_acquire)) return;

    bool allowUnsigned = false;
    if (!event.manifest.signedPackage) {
        const std::wstring warning =
            L"Размер и SHA-256 установщика совпали с stable-манифестом.\n\n"
            L"Но пакет не подписан Authenticode. Разрешить запуск именно этого проверенного пакета?";
        if (MessageBoxW(hwnd, warning.c_str(), L"Неподписанное обновление",
                        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) {
            SetContent(L"Установщик скачан и прошёл size/SHA-256 проверку, но его запуск отменён пользователем.\r\n\r\n" +
                       event.package.wstring());
            return;
        }
        allowUnsigned = true;
    }

    const fs::path exeDir = ExecutableDirectory();
    const fs::path updater = exeDir / L"DPopUpdater.exe";
    const fs::path restart = exeDir / L"DPopCleaner.exe";
    std::wstring error;
    if (!LaunchUpdater(event.manifest, event.package, allowUnsigned, updater, restart, error)) {
        SetContent(L"Запуск обновления остановлен:\r\n\r\n" + error);
        return;
    }

    gUpdaterDriven.store(true, std::memory_order_release);
    PostMessageW(hwnd, WM_CLOSE, 0, 0);
}

void HandleZapretFinished(HWND hwnd, const PendingEvent& event) {
    if (gShuttingDown.load(std::memory_order_acquire)) return;
    if (gPage == Page::Zapret) ShowZapret();
    if (!event.success) {
        MessageBoxW(hwnd, event.error.c_str(), L"Операция Zapret не выполнена", MB_OK | MB_ICONWARNING);
    } else if (!event.message.empty()) {
        MessageBoxW(hwnd, event.message.c_str(), L"DPopCleaner Zapret", MB_OK | MB_ICONINFORMATION);
    }
}

void HandlePendingEvent(HWND hwnd) {
    if (gShuttingDown.load(std::memory_order_acquire)) {
        std::lock_guard<std::mutex> guard(gPendingMutex);
        gPendingEvent.reset();
        return;
    }

    std::optional<PendingEvent> event;
    {
        std::lock_guard<std::mutex> guard(gPendingMutex);
        event.swap(gPendingEvent);
    }
    if (!event || gShuttingDown.load(std::memory_order_acquire)) return;

    if (event->kind == PendingKind::Check) HandleCheckFinished(hwnd, *event);
    else if (event->kind == PendingKind::Download) HandleDownloadFinished(hwnd, *event);
    else HandleZapretFinished(hwnd, *event);
}

void ToggleAutoUpdate(HWND hwnd) {
    const bool oldValue = gSettings.autoCheckUpdates;
    gSettings.autoCheckUpdates = !oldValue;
    std::wstring error;
    if (!SaveSettingsAtomic(SettingsPath(), gSettings, error)) {
        gSettings.autoCheckUpdates = oldValue;
        MessageBoxW(hwnd, error.c_str(), L"Не удалось сохранить настройки", MB_OK | MB_ICONERROR);
    }
    ShowSettings();
}

void CycleZapretStrategy(HWND hwnd) {
    const auto strategies = EnumerateZapretStrategies(ZapretRoot());
    if (strategies.empty()) {
        MessageBoxW(hwnd, L"Bundled Zapret не содержит доступных стратегий.", L"DPopCleaner Zapret",
                    MB_OK | MB_ICONWARNING);
        return;
    }

    const size_t current = FindStrategyMenuIndex(strategies, gSettings.zapretStrategy);
    const size_t nextIndex = current == 0 ? 0 : (current % strategies.size());
    const std::wstring oldValue = gSettings.zapretStrategy;
    gSettings.zapretStrategy = strategies[nextIndex].batchPath.filename().wstring();

    std::wstring error;
    if (!SaveSettingsAtomic(SettingsPath(), gSettings, error)) {
        gSettings.zapretStrategy = oldValue;
        MessageBoxW(hwnd, error.c_str(), L"Не удалось сохранить стратегию Zapret", MB_OK | MB_ICONERROR);
    }
    ShowZapret();
}

void HandleZapretServiceAction(HWND hwnd) {
    const ZapretStatus status = QueryZapretStatus(ZapretRoot());
    if (status.serviceInstalled && !status.bundledServiceOwned) {
        MessageBoxW(hwnd,
                    L"Служба zapret существует, но её executable не находится в bundled-каталоге DPopCleaner.\n\n"
                    L"Она считается внешней и не будет изменена.",
                    L"Внешняя служба zapret", MB_OK | MB_ICONWARNING);
        return;
    }
    StartZapretOperation(hwnd, status.serviceInstalled ? ZapretOperation::RemoveService
                                                       : ZapretOperation::InstallService);
}

void HandleAction(int index) {
    switch (gPage) {
        case Page::Overview:
            if (index == 0) StartUpdateCheck(gMainWindow, true);
            else if (index == 1) SwitchPage(Page::Settings);
            break;
        case Page::Cleaning:
            if (index == 0) ShellExecuteW(gMainWindow, L"open", L"cleanmgr.exe", nullptr, nullptr, SW_SHOWNORMAL);
            else if (index == 1) OpenUrl(L"ms-settings:storagesense");
            else if (index == 2) {
                wchar_t temp[32768]{};
                const DWORD length = GetTempPathW(static_cast<DWORD>(std::size(temp)), temp);
                if (length > 0 && length < std::size(temp)) OpenTarget(fs::path(temp));
            }
            break;
        case Page::DiskAnalyzer:
            if (index == 0) OpenModule(L"DiskAnalyzer.exe");
            break;
        case Page::RestoreCenter:
            if (index == 0) OpenModule(L"RestoreCenter.exe");
            break;
        case Page::Zapret:
            if (index == 0) StartZapretOperation(gMainWindow, ZapretOperation::Start);
            else if (index == 1) StartZapretOperation(gMainWindow, ZapretOperation::Stop);
            else if (index == 2) CycleZapretStrategy(gMainWindow);
            else if (index == 3) HandleZapretServiceAction(gMainWindow);
            break;
        case Page::ZapretFix:
            if (index == 0) OpenModule(L"ZapretScreenFix.exe");
            break;
        case Page::Updates:
            if (index == 0) StartUpdateCheck(gMainWindow, true);
            else if (index == 1) OpenUrl(L"https://github.com/elesnichenko1-droid/dpopcleaner-site/releases");
            break;
        case Page::Settings:
            if (index == 0) ToggleAutoUpdate(gMainWindow);
            else if (index == 1) StartUpdateCheck(gMainWindow, true);
            else if (index == 2) {
                EnsureUserDirectories();
                OpenTarget(LogsDirectory());
            } else if (index == 3) {
                OpenUrl(L"https://elesnichenko1-droid.github.io/dpopcleaner-site/");
            }
            break;
    }
}

void DrawOwnerButton(const DRAWITEMSTRUCT* item) {
    wchar_t text[256]{};
    GetWindowTextW(item->hwndItem, text, static_cast<int>(std::size(text)));
    const int id = GetDlgCtrlID(item->hwndItem);
    bool selected = false;
    if (id >= ID_NAV_BASE && id < ID_NAV_BASE + NAV_COUNT) {
        selected = (id - ID_NAV_BASE) == static_cast<int>(gPage);
    }

    COLORREF fill = selected ? RGB(25, 73, 65) : C_CARD;
    if (id >= ID_ACTION_BASE && id < ID_ACTION_BASE + ACTION_COUNT) {
        fill = id == ID_ACTION_BASE ? RGB(42, 118, 79) : C_CARD2;
    }
    if (item->itemState & ODS_DISABLED) fill = RGB(20, 42, 55);
    if (item->itemState & ODS_SELECTED) fill = RGB(35, 95, 76);

    HBRUSH fillBrush = CreateSolidBrush(fill);
    HBRUSH borderBrush = CreateSolidBrush(C_BORDER);
    FillRect(item->hDC, &item->rcItem, fillBrush);
    FrameRect(item->hDC, &item->rcItem, borderBrush);
    DeleteObject(fillBrush);
    DeleteObject(borderBrush);

    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, (item->itemState & ODS_DISABLED) ? C_MUTED : C_TEXT);
    HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(item->hDC, gFont));
    RECT rect = item->rcItem;
    rect.left += 12;
    rect.right -= 12;
    const UINT alignment = (id >= ID_NAV_BASE && id < ID_NAV_BASE + NAV_COUNT) ? DT_LEFT : DT_CENTER;
    DrawTextW(item->hDC, text, -1, &rect, DT_SINGLELINE | DT_VCENTER | alignment | DT_END_ELLIPSIS);
    SelectObject(item->hDC, oldFont);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            gMainWindow = hwnd;
            BOOL dark = TRUE;
            DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));

            gBackgroundBrush = CreateSolidBrush(C_BG);
            gCardBrush = CreateSolidBrush(C_CARD);
            gFont = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            gFontSmall = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            gFontTitle = CreateFontW(-29, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            gFontPage = CreateFontW(-24, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                    CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

            gTitle = CreateWindowW(L"STATIC", L"DPopCleaner", WS_CHILD | WS_VISIBLE,
                                   25, 20, 210, 36, hwnd, nullptr, nullptr, nullptr);
            SendMessageW(gTitle, WM_SETFONT, reinterpret_cast<WPARAM>(gFontTitle), TRUE);
            gVersion = CreateWindowW(L"STATIC", L"0.4.18  •  stable  •  rev.1", WS_CHILD | WS_VISIBLE,
                                     27, 58, 220, 24, hwnd, nullptr, nullptr, nullptr);
            SendMessageW(gVersion, WM_SETFONT, reinterpret_cast<WPARAM>(gFontSmall), TRUE);

            const wchar_t* navText[NAV_COUNT] = {
                L"Обзор", L"Очистка", L"Анализатор диска", L"Восстановление",
                L"Zapret", L"Zapret Fix", L"Обновления", L"Настройки"
            };
            for (int i = 0; i < NAV_COUNT; ++i) {
                gNav[i] = CreateWindowW(L"BUTTON", navText[i],
                                        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                        20, 105 + i * 53, 220, 43, hwnd,
                                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_NAV_BASE + i)),
                                        nullptr, nullptr);
            }

            gPageTitle = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                                       285, 25, 830, 34, hwnd, nullptr, nullptr, nullptr);
            SendMessageW(gPageTitle, WM_SETFONT, reinterpret_cast<WPARAM>(gFontPage), TRUE);
            gPageHint = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                                      286, 63, 865, 28, hwnd, nullptr, nullptr, nullptr);
            SendMessageW(gPageHint, WM_SETFONT, reinterpret_cast<WPARAM>(gFontSmall), TRUE);

            gContent = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                       WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY |
                                       ES_AUTOVSCROLL | WS_VSCROLL,
                                       285, 104, 885, 470, hwnd,
                                       reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_CONTENT)),
                                       nullptr, nullptr);
            SendMessageW(gContent, WM_SETFONT, reinterpret_cast<WPARAM>(gFont), TRUE);

            for (int i = 0; i < ACTION_COUNT; ++i) {
                gActions[i] = CreateWindowW(L"BUTTON", L"", WS_CHILD | BS_OWNERDRAW,
                                             285 + i * 218, 592, 205, 44, hwnd,
                                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_ACTION_BASE + i)),
                                             nullptr, nullptr);
            }

            SwitchPage(Page::Overview);
            if (gSettings.autoCheckUpdates && !gShuttingDown.load(std::memory_order_acquire)) {
                SetTimer(hwnd, ID_STARTUP_UPDATE_TIMER, 300, nullptr);
            }
            return 0;
        }
        case WM_TIMER:
            if (wParam == ID_STARTUP_UPDATE_TIMER) {
                KillTimer(hwnd, ID_STARTUP_UPDATE_TIMER);
                if (!gShuttingDown.load(std::memory_order_acquire) && gSettings.autoCheckUpdates) {
                    StartUpdateCheck(hwnd, false);
                }
                return 0;
            }
            break;
        case WM_COMMAND: {
            const int id = LOWORD(wParam);
            if (id >= ID_NAV_BASE && id < ID_NAV_BASE + NAV_COUNT) {
                SwitchPage(static_cast<Page>(id - ID_NAV_BASE));
                return 0;
            }
            if (id >= ID_ACTION_BASE && id < ID_ACTION_BASE + ACTION_COUNT) {
                HandleAction(id - ID_ACTION_BASE);
                return 0;
            }
            break;
        }
        case WM_UPDATE_EVENT:
            HandlePendingEvent(hwnd);
            return 0;
        case WM_DRAWITEM:
            DrawOwnerButton(reinterpret_cast<DRAWITEMSTRUCT*>(lParam));
            return TRUE;
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetBkMode(dc, TRANSPARENT);
            const HWND control = reinterpret_cast<HWND>(lParam);
            SetTextColor(dc, (control == gVersion || control == gPageHint) ? C_MUTED : C_TEXT);
            return reinterpret_cast<LRESULT>(gBackgroundBrush);
        }
        case WM_CTLCOLOREDIT: {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetBkColor(dc, C_CARD);
            SetTextColor(dc, C_TEXT);
            return reinterpret_cast<LRESULT>(gCardBrush);
        }
        case WM_ERASEBKGND: {
            HDC dc = reinterpret_cast<HDC>(wParam);
            RECT rect{};
            GetClientRect(hwnd, &rect);
            FillRect(dc, &rect, gBackgroundBrush);
            RECT sidebar{0, 0, 260, rect.bottom};
            HBRUSH sideBrush = CreateSolidBrush(C_SIDE);
            FillRect(dc, &sidebar, sideBrush);
            DeleteObject(sideBrush);
            return 1;
        }
        case WM_CLOSE:
            gShuttingDown.store(true, std::memory_order_release);
            KillTimer(hwnd, ID_STARTUP_UPDATE_TIMER);
            ShowWindow(hwnd, SW_HIDE);
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            if (gFont) { DeleteObject(gFont); gFont = nullptr; }
            if (gFontSmall) { DeleteObject(gFontSmall); gFontSmall = nullptr; }
            if (gFontTitle) { DeleteObject(gFontTitle); gFontTitle = nullptr; }
            if (gFontPage) { DeleteObject(gFontPage); gFontPage = nullptr; }
            if (gBackgroundBrush) { DeleteObject(gBackgroundBrush); gBackgroundBrush = nullptr; }
            if (gCardBrush) { DeleteObject(gCardBrush); gCardBrush = nullptr; }
            gMainWindow = nullptr;
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

} // namespace

int RunMainWindow(HINSTANCE instance, int showCommand) {
    EnsureUserDirectories();
    gShuttingDown.store(false, std::memory_order_release);
    gUpdateBusy.store(false, std::memory_order_release);
    gZapretBusy.store(false, std::memory_order_release);
    gUpdaterDriven.store(false, std::memory_order_release);
    {
        std::lock_guard<std::mutex> guard(gPendingMutex);
        gPendingEvent.reset();
    }
    gSettings = LoadSettings(SettingsPath());

    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.hInstance = instance;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.lpszClassName = L"DPopCleaner0418MainWindow";
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hIconSm = windowClass.hIcon;
    if (!RegisterClassExW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return 1;

    HWND hwnd = CreateWindowExW(
        0,
        windowClass.lpszClassName,
        L"DPopCleaner 0.4.18",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 1215, 695,
        nullptr, nullptr, instance, nullptr);
    if (!hwnd) return 2;

    ShowWindow(hwnd, showCommand == 0 ? SW_SHOWNORMAL : showCommand);
    UpdateWindow(hwnd);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

} // namespace dpop0418
