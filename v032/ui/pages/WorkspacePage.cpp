#include "ui/pages/WorkspacePage.h"

#include "modules/DPopGuard.h"
#include "modules/ZapretManager.h"
#include "ui/Controls.h"
#include "ui/Theme.h"

#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <string>

namespace fs = std::filesystem;

namespace dpop::ui {
namespace {

constexpr wchar_t kClassName[] = L"DPopCleaner032WorkspacePage";
constexpr int kHeadingId = 1600;
constexpr int kStatusId = 1601;
constexpr int kListId = 1602;
constexpr int kButtonBase = 1610;
constexpr UINT kAsyncDone = WM_APP + 41;

std::wstring PageTitle(Page page) {
    for (const auto& tab : PrimaryTabs()) if (tab.page == page) return std::wstring(tab.label);
    return page == Page::Settings ? L"Настройки" : L"DPopCleaner";
}

std::wstring WindowText(HWND hwnd) {
    const int n = GetWindowTextLengthW(hwnd);
    if (n <= 0) return {};
    std::wstring text(static_cast<std::size_t>(n + 1), L'\0');
    GetWindowTextW(hwnd, text.data(), n + 1);
    text.resize(static_cast<std::size_t>(n));
    return text;
}

bool Confirm(HWND parent, std::wstring_view text, bool strong = false) {
    const std::wstring owned{text};
    return MessageBoxW(parent, owned.c_str(), L"DPopCleaner", MB_OKCANCEL | MB_ICONWARNING | (strong ? MB_DEFBUTTON2 : 0)) == IDOK;
}

std::wstring LastDetailPath(const std::wstring& details) {
    const auto pos = details.find_last_of(L"\r\n");
    return pos == std::wstring::npos ? details : details.substr(pos + 1);
}

} // namespace

WorkspacePage::~WorkspacePage() { Destroy(); }

bool WorkspacePage::RegisterClass() noexcept {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &WorkspacePage::WindowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kClassName;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    return true;
}

bool WorkspacePage::Create(HWND parent, SessionLog& sessionLog) {
    Destroy();
    parent_ = parent;
    sessionLog_ = &sessionLog;
    settings_ = dpop::full::LoadSettings();
    if (!RegisterClass()) return false;

    hwnd_ = CreateWindowExW(0, kClassName, L"", WS_CHILD | WS_CLIPCHILDREN,
        0, 0, 0, 0, parent_, nullptr, GetModuleHandleW(nullptr), this);
    if (!hwnd_) return false;

    headingFont_ = CreateUiFont(22, FW_SEMIBOLD);
    bodyFont_ = CreateUiFont(10, FW_NORMAL);
    listFont_ = CreateUiFont(10, FW_NORMAL);

    heading_ = CreateTextLabel(hwnd_, kHeadingId, L"");
    status_ = CreateTextLabel(hwnd_, kStatusId, L"");
    list_ = CreateDarkListView(hwnd_, kListId);
    if (!heading_ || !status_ || !list_ || !headingFont_ || !bodyFont_ || !listFont_) return false;

    LONG_PTR style = GetWindowLongPtrW(list_, GWL_STYLE);
    SetWindowLongPtrW(list_, GWL_STYLE, style & ~static_cast<LONG_PTR>(LVS_SINGLESEL));
    ListView_SetExtendedListViewStyleEx(list_, 0,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);

    ApplyControlFont(heading_, headingFont_);
    ApplyControlFont(status_, bodyFont_);
    ApplyControlFont(list_, listFont_);

    for (int i = 0; i < static_cast<int>(buttons_.size()); ++i) {
        buttons_[static_cast<std::size_t>(i)] = CreatePushButton(hwnd_, kButtonBase + i, L"");
        if (!buttons_[static_cast<std::size_t>(i)]) return false;
        ApplyControlFont(buttons_[static_cast<std::size_t>(i)], bodyFont_);
        ShowWindow(buttons_[static_cast<std::size_t>(i)], SW_HIDE);
    }

    ConfigurePage();
    return true;
}

void WorkspacePage::Destroy() noexcept {
    CancelWorker();
    if (worker_.joinable()) worker_.join();
    for (auto& button : buttons_) {
        if (button && IsWindow(button)) DestroyWindow(button);
        button = nullptr;
    }
    for (HWND* handle : {&heading_, &status_, &list_}) {
        if (*handle && IsWindow(*handle)) DestroyWindow(*handle);
        *handle = nullptr;
    }
    if (hwnd_ && IsWindow(hwnd_)) DestroyWindow(hwnd_);
    hwnd_ = nullptr;
    for (HFONT* font : {&headingFont_, &bodyFont_, &listFont_}) {
        if (*font) DeleteObject(*font);
        *font = nullptr;
    }
    parent_ = nullptr;
    sessionLog_ = nullptr;
}

void WorkspacePage::Show(bool visible) noexcept { if (hwnd_) ShowWindow(hwnd_, visible ? SW_SHOW : SW_HIDE); }

void WorkspacePage::Layout(const Box& box) noexcept {
    if (!hwnd_) return;
    MoveWindow(hwnd_, box.x, box.y, box.width, box.height, TRUE);
    LayoutChildren();
}

void WorkspacePage::LayoutChildren() noexcept {
    if (!hwnd_) return;
    RECT rc{}; GetClientRect(hwnd_, &rc);
    const int width = rc.right;
    const int height = rc.bottom;
    const int margin = 18;
    const int gap = 10;
    const int buttonHeight = 38;
    const int visibleButtons = static_cast<int>(std::count_if(buttons_.begin(), buttons_.end(), [](HWND h){ return h && IsWindowVisible(h); }));
    const int buttonWidth = visibleButtons > 0 ? std::max(110, (width - margin * 2 - gap * (visibleButtons - 1)) / visibleButtons) : 0;

    MoveWindow(heading_, margin, 12, std::max(0, width - margin * 2), 38, TRUE);
    MoveWindow(status_, margin, 52, std::max(0, width - margin * 2), 30, TRUE);
    const int listBottom = height - margin - (visibleButtons ? buttonHeight + gap : 0);
    MoveWindow(list_, margin, 86, std::max(0, width - margin * 2), std::max(80, listBottom - 86), TRUE);

    int x = margin;
    for (HWND button : buttons_) {
        if (!button || !IsWindowVisible(button)) continue;
        MoveWindow(button, x, height - margin - buttonHeight, buttonWidth, buttonHeight, TRUE);
        x += buttonWidth + gap;
    }
}

void WorkspacePage::SetPage(Page page) {
    if (page == Page::Overview) return;
    CancelWorker();
    if (worker_.joinable()) worker_.join();
    busy_ = false;
    page_ = page;
    ConfigurePage();
}

void WorkspacePage::SetStatus(std::wstring_view text) {
    if (!status_) return;
    const std::wstring owned{text};
    SetWindowTextW(status_, owned.c_str());
}

void WorkspacePage::Log(EventLevel level, std::wstring_view message) {
    if (!sessionLog_) return;
    sessionLog_->Append(PageTitle(page_), level, message);
    HWND shell = parent_ ? GetParent(parent_) : nullptr;
    if (shell) PostMessageW(shell, kWorkspaceLogChangedMessage, 0, 0);
}

void WorkspacePage::ClearList(bool checkboxes) {
    ListView_DeleteAllItems(list_);
    HWND header = ListView_GetHeader(list_);
    int count = header ? Header_GetItemCount(header) : 0;
    while (count-- > 0) ListView_DeleteColumn(list_, 0);
    DWORD ex = LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES;
    if (checkboxes) ex |= LVS_EX_CHECKBOXES;
    ListView_SetExtendedListViewStyle(list_, ex);
}

void WorkspacePage::AddColumn(int column, std::wstring_view title, int width) {
    std::wstring owned{title};
    LVCOLUMNW col{};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    col.pszText = owned.data();
    col.cx = width;
    col.iSubItem = column;
    ListView_InsertColumn(list_, column, &col);
}

int WorkspacePage::AddRow(const std::vector<std::wstring>& columns) {
    if (columns.empty()) return -1;
    LVITEMW item{};
    item.mask = LVIF_TEXT;
    item.iItem = ListView_GetItemCount(list_);
    item.pszText = const_cast<wchar_t*>(columns[0].c_str());
    const int index = ListView_InsertItem(list_, &item);
    for (int c = 1; c < static_cast<int>(columns.size()); ++c) {
        ListView_SetItemText(list_, index, c, const_cast<wchar_t*>(columns[static_cast<std::size_t>(c)].c_str()));
    }
    return index;
}

int WorkspacePage::SelectedIndex() const noexcept { return ListView_GetNextItem(list_, -1, LVNI_SELECTED); }

std::vector<int> WorkspacePage::SelectedIndices() const {
    std::vector<int> out;
    int index = -1;
    while ((index = ListView_GetNextItem(list_, index, LVNI_SELECTED)) != -1) out.push_back(index);
    return out;
}

void WorkspacePage::SetButtons(const std::vector<std::wstring>& labels, int accent, int danger) {
    for (std::size_t i = 0; i < buttons_.size(); ++i) {
        if (i < labels.size() && !labels[i].empty()) {
            SetWindowTextW(buttons_[i], labels[i].c_str());
            visuals_[i] = static_cast<int>(i) == danger ? ButtonVisual::Danger : (static_cast<int>(i) == accent ? ButtonVisual::Accent : ButtonVisual::Normal);
            ShowWindow(buttons_[i], SW_SHOW);
        } else {
            visuals_[i] = ButtonVisual::Normal;
            ShowWindow(buttons_[i], SW_HIDE);
        }
    }
    LayoutChildren();
}

fs::path WorkspacePage::ChooseFolder(std::wstring_view title) {
    std::wstring owned{title};
    BROWSEINFOW bi{};
    bi.hwndOwner = hwnd_;
    bi.lpszTitle = owned.c_str();
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return {};
    wchar_t path[MAX_PATH]{};
    fs::path result;
    if (SHGetPathFromIDListW(pidl, path)) result = path;
    CoTaskMemFree(pidl);
    return result;
}

fs::path WorkspacePage::ChooseFile(std::wstring_view title) {
    wchar_t file[32768]{};
    std::wstring owned{title};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_;
    ofn.lpstrTitle = owned.c_str();
    ofn.lpstrFile = file;
    ofn.nMaxFile = static_cast<DWORD>(std::size(file));
    ofn.lpstrFilter = L"Все файлы\0*.*\0\0";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    return GetOpenFileNameW(&ofn) ? fs::path(file) : fs::path{};
}

void WorkspacePage::OpenPath(const fs::path& path, bool selectFile) {
    if (path.empty()) return;
    std::wstring params;
    if (selectFile) params = L"/select,\"" + path.wstring() + L"\"";
    else params = L"\"" + path.wstring() + L"\"";
    ShellExecuteW(hwnd_, L"open", L"explorer.exe", params.c_str(), nullptr, SW_SHOWNORMAL);
}

void WorkspacePage::CancelWorker() noexcept { if (worker_.joinable()) worker_.request_stop(); }

void WorkspacePage::StartAsync(std::wstring_view status, std::function<void(std::stop_token)> work) {
    if (busy_) { SetStatus(L"Операция уже выполняется. Нажми «Отмена»."); return; }
    if (worker_.joinable()) worker_.join();
    busy_ = true;
    SetStatus(status);
    worker_ = std::jthread([this, work = std::move(work)](std::stop_token token) mutable {
        try { work(token); }
        catch (...) { QueueApply([this] { SetStatus(L"Операция завершилась ошибкой."); Log(EventLevel::Error, L"Фоновая операция завершилась исключением."); }); }
    });
}

void WorkspacePage::QueueApply(std::function<void()> apply) {
    {
        std::lock_guard lock(pendingMutex_);
        pendingApply_ = std::move(apply);
    }
    if (hwnd_ && IsWindow(hwnd_)) PostMessageW(hwnd_, kAsyncDone, 0, 0);
}

void WorkspacePage::CompleteAsync() {
    if (worker_.joinable()) worker_.join();
    busy_ = false;
    std::function<void()> apply;
    {
        std::lock_guard lock(pendingMutex_);
        apply = std::move(pendingApply_);
    }
    if (apply) apply();
}

void WorkspacePage::ConfigurePage() {
    if (!hwnd_) return;
    SetWindowTextW(heading_, PageTitle(page_).c_str());
    showingLeftovers_ = false;

    switch (page_) {
    case Page::Cleaning:
        ClearList(true); AddColumn(0, L"Категория", 360); AddColumn(1, L"Размер", 150); AddColumn(2, L"Доступ", 150);
        SetButtons({L"Анализ", L"Очистить выбранное", L"Рекомендуемое", L"Выбрать всё", L"Снять всё", L"Отмена"}, 0, 1);
        RefreshCleaning(); break;
    case Page::Memory:
        ClearList(); AddColumn(0, L"Показатель", 280); AddColumn(1, L"Значение", 300);
        SetButtons({L"Обновить", L"Безопасно освободить", L"Агрессивно", L"Отмена"}, 0, 2);
        RefreshMemory(); break;
    case Page::Guard:
        ClearList(); AddColumn(0, L"Результат", 260); AddColumn(1, L"Уровень / вердикт", 250); AddColumn(2, L"Путь / детали", 520);
        SetButtons({L"QuickScan", L"Проверить файл", L"Проверить папку", L"В карантин", L"Отмена"}, 0, 3);
        SetStatus(L"DPopGuard: эвристика процессов/автозапуска + AMSI. Это не полноценный антивирус."); break;
    case Page::Disk:
        ClearList(); AddColumn(0, L"Размер", 150); AddColumn(1, L"Файл", 850);
        SetButtons({L"Выбрать папку", L"Сканировать", L"Открыть выбранное", L"Отмена"}, 1, -1);
        SetStatus((L"Корень: " + diskRoot_.wstring() + L" • порог: " + std::to_wstring(settings_.largeFileMB) + L" МБ").c_str()); break;
    case Page::Applications:
        ClearList(); AddColumn(0, L"Приложение", 360); AddColumn(1, L"Версия", 150); AddColumn(2, L"Издатель / путь", 500);
        SetButtons({L"Обновить", L"Удалить", L"Открыть папку", L"Найти хвосты", L"Хвосты в Корзину", L"Отмена"}, 0, 1);
        RefreshApps(); break;
    case Page::WindowsUpdate:
        ClearList(); AddColumn(0, L"Операция Windows", 560); AddColumn(1, L"Примечание", 430);
        SetButtons({L"Запустить выбранное", L"Отмена"}, 0, -1); RefreshWindows(); break;
    case Page::Duplicates:
        ClearList(); AddColumn(0, L"Группа", 90); AddColumn(1, L"Размер", 140); AddColumn(2, L"SHA-256", 200); AddColumn(3, L"Файл", 600);
        SetButtons({L"Выбрать папку", L"Найти дубликаты", L"Выбранные в Корзину", L"Отмена"}, 1, 2);
        SetStatus(duplicateRoot_.empty() ? L"Выбери папку для поиска." : (L"Папка: " + duplicateRoot_.wstring()).c_str()); break;
    case Page::Tools:
        ClearList(); AddColumn(0, L"Инструмент", 480); AddColumn(1, L"Назначение", 500);
        SetButtons({L"Открыть выбранное"}, 0, -1); RefreshTools(); break;
    case Page::Zapret:
        ClearList(); AddColumn(0, L"Параметр", 300); AddColumn(1, L"Состояние", 650);
        SetButtons({L"Обновить", L"Запустить default strategy", L"Службы Windows", L"Открыть bundle"}, 0, -1); RefreshZapret(); break;
    case Page::Settings:
        ClearList(); AddColumn(0, L"Параметр", 400); AddColumn(1, L"Значение", 520);
        SetButtons({L"Подтверждения", L"Большие файлы", L"Мин. дубликат", L"Автозапуск", L"Перечитать"}, -1, -1); RefreshSettings(); break;
    default: break;
    }
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void WorkspacePage::RefreshCleaning() {
    StartAsync(L"Анализируем безопасные категории очистки…", [this](std::stop_token token) {
        auto items = dpop::full::AnalyzeCleaning(token);
        QueueApply([this, items = std::move(items)]() mutable {
            cleanItems_ = std::move(items);
            ListView_DeleteAllItems(list_);
            std::uint64_t total = 0;
            for (std::size_t i = 0; i < cleanItems_.size(); ++i) {
                const auto& item = cleanItems_[i]; total += item.bytes;
                AddRow({item.label, dpop::full::FormatBytes(item.bytes), item.requiresAdmin ? L"может потребовать права администратора" : L"пользователь"});
                ListView_SetCheckState(list_, static_cast<int>(i), item.recommended ? TRUE : FALSE);
            }
            SetStatus(L"Найдено: " + dpop::full::FormatBytes(total) + L". Отметь категории и запусти очистку.");
            Log(EventLevel::Info, L"Анализ очистки завершён.");
        });
    });
}

void WorkspacePage::RefreshMemory() {
    const auto m = dpop::full::QueryMemoryStats();
    ListView_DeleteAllItems(list_);
    AddRow({L"Используется", dpop::full::FormatBytes(m.usedPhysical) + L" / " + dpop::full::FormatBytes(m.totalPhysical) + L" • " + std::to_wstring(m.usedPercent) + L"%"});
    AddRow({L"Доступно", dpop::full::FormatBytes(m.availablePhysical)});
    AddRow({L"Процессов", std::to_wstring(m.processCount)});
    SetStatus(L"Безопасный режим тримит крупные working sets; агрессивный — больше доступных процессов.");
}

void WorkspacePage::RefreshApps() {
    StartAsync(L"Читаем установленные приложения…", [this](std::stop_token) {
        auto apps = dpop::apps::EnumerateInstalledApps();
        QueueApply([this, apps = std::move(apps)]() mutable {
            apps_ = std::move(apps); showingLeftovers_ = false; leftovers_.clear();
            ListView_DeleteAllItems(list_);
            for (const auto& app : apps_) AddRow({app.displayName, app.displayVersion, app.publisher + (app.installLocation.empty() ? L"" : L" • " + app.installLocation)});
            SetStatus(L"Установлено приложений: " + std::to_wstring(apps_.size()) + L".");
        });
    });
}

void WorkspacePage::RefreshWindows() {
    ListView_DeleteAllItems(list_);
    const std::array<dpop::full::MaintenanceAction, 8> actions = {
        dpop::full::MaintenanceAction::ClearUpdateCache, dpop::full::MaintenanceAction::ComponentCleanup,
        dpop::full::MaintenanceAction::ResetBase, dpop::full::MaintenanceAction::SfcScan,
        dpop::full::MaintenanceAction::DismCheckHealth, dpop::full::MaintenanceAction::DismScanHealth,
        dpop::full::MaintenanceAction::DismRestoreHealth, dpop::full::MaintenanceAction::ChkdskScan
    };
    for (const auto a : actions) {
        const bool dangerous = a == dpop::full::MaintenanceAction::ResetBase;
        AddRow({std::wstring(dpop::full::MaintenanceLabel(a)), dangerous ? L"Необратимо фиксирует текущую базу компонентов" : L"Штатный инструмент Windows; может запросить UAC"});
    }
    SetStatus(L"Выбери одну системную операцию. DPopCleaner запускает штатные Windows-команды с UAC.");
}

void WorkspacePage::RefreshTools() {
    ListView_DeleteAllItems(list_);
    const std::array<std::wstring, 7> notes = {L"Процессы и производительность", L"Системные журналы", L"Вкладка автозагрузки", L"Точки восстановления", L"Microsoft Defender / Windows Security", L"PerfMon", L"Папка журналов DPopCleaner"};
    for (int i = 0; i < 7; ++i) AddRow({std::wstring(dpop::full::ToolLabel(static_cast<dpop::full::ToolAction>(i))), notes[static_cast<std::size_t>(i)]});
    SetStatus(L"Открываются только штатные инструменты Windows и папка логов DPopCleaner.");
}

void WorkspacePage::RefreshZapret() {
    const auto s = dpop::zapret::QueryStatus();
    ListView_DeleteAllItems(list_);
    AddRow({L"Bundle", s.bundleValid ? L"Готов" : L"Неполный"});
    AddRow({L"Сервис", s.serviceInstalled ? (s.serviceRunning ? L"Установлен • работает" : L"Установлен • остановлен") : L"Не установлен"});
    AddRow({L"winws", s.winwsRunning ? L"Работает" : L"Не запущен"});
    AddRow({L"Папка", s.bundleFolder.wstring()});
    if (!s.missingBundleFile.empty()) AddRow({L"Не хватает", s.missingBundleFile.wstring()});
    SetStatus(L"Показывается фактический bundled Zapret. Автообновление не имитируется.");
}

void WorkspacePage::RefreshSettings() {
    settings_ = dpop::full::LoadSettings();
    ListView_DeleteAllItems(list_);
    AddRow({L"Язык", L"Русский"});
    AddRow({L"Тема", L"Midnight"});
    AddRow({L"Лицензия", L"Бесплатная BETA"});
    AddRow({L"Подтверждать опасные действия", settings_.confirmDestructive ? L"Да" : L"Нет"});
    AddRow({L"Порог больших файлов", std::to_wstring(settings_.largeFileMB) + L" МБ"});
    AddRow({L"Минимальный размер дубликата", std::to_wstring(settings_.duplicateMinMB) + L" МБ"});
    AddRow({L"Автозапуск", settings_.runAtStartup ? L"Включён" : L"Выключен"});
    AddRow({L"Файл настроек", dpop::full::SettingsPath().wstring()});
    SetStatus(L"Изменения сохраняются сразу в settings.json.");
}

void WorkspacePage::HandleAction(int buttonIndex) {
    if (buttonIndex < 0 || buttonIndex >= 6) return;
    if (buttonIndex == 5 || (page_ == Page::Memory && buttonIndex == 3) || (page_ == Page::Guard && buttonIndex == 4) ||
        (page_ == Page::Disk && buttonIndex == 3) || (page_ == Page::WindowsUpdate && buttonIndex == 1) || (page_ == Page::Duplicates && buttonIndex == 3)) {
        CancelWorker(); SetStatus(L"Запрошена отмена текущей операции."); return;
    }

    switch (page_) {
    case Page::Cleaning: {
        if (buttonIndex == 0) { RefreshCleaning(); return; }
        if (buttonIndex == 2 || buttonIndex == 3 || buttonIndex == 4) {
            for (int i = 0; i < ListView_GetItemCount(list_); ++i) {
                bool value = buttonIndex == 3;
                if (buttonIndex == 2 && i < static_cast<int>(cleanItems_.size())) value = cleanItems_[static_cast<std::size_t>(i)].recommended;
                if (buttonIndex == 4) value = false;
                ListView_SetCheckState(list_, i, value ? TRUE : FALSE);
            }
            return;
        }
        if (buttonIndex == 1) {
            std::vector<dpop::full::CleanKind> selected;
            for (int i = 0; i < ListView_GetItemCount(list_) && i < static_cast<int>(cleanItems_.size()); ++i)
                if (ListView_GetCheckState(list_, i)) selected.push_back(cleanItems_[static_cast<std::size_t>(i)].kind);
            if (selected.empty()) { SetStatus(L"Ни одна категория не выбрана."); return; }
            if (settings_.confirmDestructive && !Confirm(hwnd_, L"Удалить выбранные кэши и временные файлы? Закрой приложения, чьи кэши очищаются.")) return;
            StartAsync(L"Очищаем выбранные категории…", [this, selected = std::move(selected)](std::stop_token token) {
                auto result = dpop::full::CleanSelected(selected, token);
                QueueApply([this, result = std::move(result)] {
                    SetStatus(L"Удалено: " + dpop::full::FormatBytes(result.removedBytes) + L" • файлов: " + std::to_wstring(result.removedFiles) + L" • ошибок: " + std::to_wstring(result.failedFiles));
                    Log(result.failedFiles ? EventLevel::Warning : EventLevel::Info, L"Очистка завершена.");
                    RefreshCleaning();
                });
            });
        }
        break;
    }
    case Page::Memory: {
        if (buttonIndex == 0) { RefreshMemory(); return; }
        if (buttonIndex == 1 || buttonIndex == 2) {
            const bool aggressive = buttonIndex == 2;
            if (aggressive && settings_.confirmDestructive && !Confirm(hwnd_, L"Агрессивный trim может временно увеличить обращения приложений к диску. Продолжить?")) return;
            StartAsync(aggressive ? L"Агрессивно освобождаем working sets…" : L"Освобождаем крупные working sets…", [this, aggressive](std::stop_token token) {
                const auto r = dpop::full::TrimWorkingSets(aggressive, token);
                QueueApply([this, r] {
                    SetStatus(L"Обработано: " + std::to_wstring(r.attempted) + L" • успешно: " + std::to_wstring(r.trimmed) + L" • отказов: " + std::to_wstring(r.failed));
                    Log(EventLevel::Info, L"Очистка working sets завершена."); RefreshMemory();
                });
            });
        }
        break;
    }
    case Page::Guard: {
        if (buttonIndex == 0) {
            StartAsync(L"QuickScan: процессы и автозагрузка…", [this](std::stop_token) {
                auto result = dpop::guard::QuickScan();
                QueueApply([this, result = std::move(result)]() mutable {
                    guardHits_.clear(); ListView_DeleteAllItems(list_);
                    for (const auto& f : result.findings) {
                        AddRow({f.title, f.severity, f.details});
                        fs::path p = LastDetailPath(f.details); std::error_code ec;
                        guardHits_.push_back(fs::is_regular_file(p, ec) ? dpop::full::GuardHit{p, f.title} : dpop::full::GuardHit{});
                    }
                    SetStatus(L"Проверено процессов: " + std::to_wstring(result.processesChecked) + L" • автозапусков: " + std::to_wstring(result.startupChecked) + L" • находок: " + std::to_wstring(result.findings.size()));
                    Log(EventLevel::Info, L"DPopGuard QuickScan завершён.");
                });
            }); return;
        }
        if (buttonIndex == 1) {
            const auto file = ChooseFile(L"Выберите файл для AMSI-проверки"); if (file.empty()) return;
            StartAsync(L"AMSI проверяет файл…", [this, file](std::stop_token) {
                std::wstring verdict, error; const bool ok = dpop::guard::ScanFileWithAmsi(file, verdict, error);
                QueueApply([this, file, ok, verdict = std::move(verdict), error = std::move(error)] {
                    ListView_DeleteAllItems(list_); guardHits_.clear();
                    AddRow({file.filename().wstring(), ok ? verdict : L"Ошибка", file.wstring()});
                    guardHits_.push_back(ok && verdict.find(L"обнаружений нет") == std::wstring::npos ? dpop::full::GuardHit{file, verdict} : dpop::full::GuardHit{});
                    SetStatus(ok ? verdict : error); Log(ok ? EventLevel::Info : EventLevel::Error, ok ? L"AMSI-проверка файла завершена." : error);
                });
            }); return;
        }
        if (buttonIndex == 2) {
            const auto folder = ChooseFolder(L"Выберите папку для AMSI-проверки"); if (folder.empty()) return;
            StartAsync(L"AMSI сканирует исполняемые/скриптовые файлы папки…", [this, folder](std::stop_token token) {
                auto r = dpop::full::ScanFolderWithAmsi(folder, token);
                QueueApply([this, r = std::move(r)]() mutable {
                    guardHits_ = r.hits; ListView_DeleteAllItems(list_);
                    for (const auto& hit : guardHits_) AddRow({hit.path.filename().wstring(), hit.verdict, hit.path.wstring()});
                    SetStatus(L"Проверено: " + std::to_wstring(r.checked) + L" • пропущено: " + std::to_wstring(r.skipped) + L" • ошибок: " + std::to_wstring(r.errors) + L" • находок: " + std::to_wstring(r.hits.size()));
                    Log(EventLevel::Info, L"AMSI-сканирование папки завершено.");
                });
            }); return;
        }
        if (buttonIndex == 3) {
            const int index = SelectedIndex(); if (index < 0 || index >= static_cast<int>(guardHits_.size()) || guardHits_[static_cast<std::size_t>(index)].path.empty()) { SetStatus(L"Выбери найденный файл с известным путём."); return; }
            const auto file = guardHits_[static_cast<std::size_t>(index)].path;
            if (!Confirm(hwnd_, L"Переместить выбранный файл в локальный карантин DPopCleaner?", true)) return;
            fs::path dst; std::wstring error;
            if (dpop::full::QuarantineFile(file, dst, error)) { SetStatus(L"Карантин: " + dst.wstring()); Log(EventLevel::Warning, L"Файл перемещён в карантин."); }
            else { SetStatus(error); Log(EventLevel::Error, error); }
        }
        break;
    }
    case Page::Disk: {
        if (buttonIndex == 0) { const auto folder = ChooseFolder(L"Выберите папку или диск"); if (!folder.empty()) { diskRoot_ = folder; SetStatus(L"Корень: " + diskRoot_.wstring()); } return; }
        if (buttonIndex == 1) {
            const auto root = diskRoot_; const auto threshold = static_cast<std::uint64_t>(settings_.largeFileMB) * 1024ull * 1024ull;
            StartAsync(L"Сканируем большие файлы…", [this, root, threshold](std::stop_token token) {
                auto files = dpop::full::ScanLargeFiles(root, threshold, token);
                QueueApply([this, files = std::move(files)]() mutable {
                    largeFiles_ = std::move(files); ListView_DeleteAllItems(list_);
                    for (const auto& f : largeFiles_) AddRow({dpop::full::FormatBytes(f.size), f.path.wstring()});
                    SetStatus(L"Найдено больших файлов: " + std::to_wstring(largeFiles_.size()) + L"."); Log(EventLevel::Info, L"Сканирование диска завершено.");
                });
            }); return;
        }
        if (buttonIndex == 2) { const int i = SelectedIndex(); if (i >= 0 && i < static_cast<int>(largeFiles_.size())) OpenPath(largeFiles_[static_cast<std::size_t>(i)].path, true); }
        break;
    }
    case Page::Applications: {
        if (buttonIndex == 0) { RefreshApps(); return; }
        const int i = SelectedIndex();
        if (buttonIndex == 4 && showingLeftovers_) {
            std::vector<dpop::apps::LeftoverItem> high; for (const auto& x : leftovers_) if (x.highConfidence) high.push_back(x);
            if (high.empty()) { SetStatus(L"Нет high-confidence хвостов для удаления."); return; }
            if (!Confirm(hwnd_, L"Переместить только high-confidence хвосты выбранного приложения в Корзину?", true)) return;
            StartAsync(L"Перемещаем хвосты в Корзину…", [this, high = std::move(high)](std::stop_token) {
                std::size_t removed = 0; std::wstring error; const bool ok = dpop::apps::MoveLeftoversToRecycleBin(high, removed, error);
                QueueApply([this, ok, removed, error = std::move(error)] { SetStatus(ok ? L"Хвостов перемещено: " + std::to_wstring(removed) : error); Log(ok ? EventLevel::Info : EventLevel::Warning, ok ? L"Хвосты приложения перемещены в Корзину." : error); });
            }); return;
        }
        if (i < 0) { SetStatus(L"Выбери приложение."); return; }
        if (showingLeftovers_) { if (buttonIndex == 2 && i < static_cast<int>(leftovers_.size())) OpenPath(leftovers_[static_cast<std::size_t>(i)].path); return; }
        if (i >= static_cast<int>(apps_.size())) return;
        const auto app = apps_[static_cast<std::size_t>(i)];
        if (buttonIndex == 1) {
            if (!Confirm(hwnd_, L"Запустить штатный деинсталлятор выбранного приложения?", true)) return;
            std::wstring error;
            if (dpop::full::LaunchUninstaller(app, error)) {
                SetStatus(L"Штатный деинсталлятор запущен. После завершения нажми «Обновить» или «Найти хвосты».");
                Log(EventLevel::Info, L"Штатный деинсталлятор приложения запущен.");
            } else {
                SetStatus(error); Log(EventLevel::Warning, error);
            }
            return;
        }
        if (buttonIndex == 2) { if (!app.installLocation.empty()) OpenPath(app.installLocation); else SetStatus(L"Путь установки не указан приложением."); return; }
        if (buttonIndex == 3) {
            StartAsync(L"Ищем хвосты выбранного приложения…", [this, app](std::stop_token) {
                auto leftovers = dpop::apps::FindLeftovers(app);
                QueueApply([this, leftovers = std::move(leftovers)]() mutable {
                    leftovers_ = std::move(leftovers); showingLeftovers_ = true; ListView_DeleteAllItems(list_);
                    for (const auto& x : leftovers_) AddRow({x.path.filename().wstring(), x.highConfidence ? L"HIGH" : L"review", x.path.wstring() + L" • " + x.reason});
                    SetStatus(L"Найдено хвостов: " + std::to_wstring(leftovers_.size()) + L". В Корзину отправляются только HIGH.");
                });
            }); return;
        }
        break;
    }
    case Page::WindowsUpdate: {
        if (buttonIndex != 0) return;
        const int i = SelectedIndex(); if (i < 0 || i >= 8) { SetStatus(L"Выбери системную операцию."); return; }
        const auto action = static_cast<dpop::full::MaintenanceAction>(i);
        if (action == dpop::full::MaintenanceAction::ResetBase && !Confirm(hwnd_, L"/ResetBase необратимо удаляет возможность отката заменённых компонентов Windows. Продолжить?", true)) return;
        if (action == dpop::full::MaintenanceAction::ClearUpdateCache && settings_.confirmDestructive && !Confirm(hwnd_, L"Остановить службы Windows Update/BITS, очистить Download cache и снова запустить службы?")) return;
        StartAsync(L"Windows выполняет выбранную операцию…", [this, action](std::stop_token token) {
            unsigned long code = 0; std::wstring error; const bool ok = dpop::full::RunMaintenance(action, code, error, token);
            QueueApply([this, ok, code, error = std::move(error)] { SetStatus(ok ? L"Операция завершена успешно." : error); Log(ok ? EventLevel::Info : EventLevel::Error, ok ? L"Операция Windows завершена." : error); });
        });
        break;
    }
    case Page::Duplicates: {
        if (buttonIndex == 0) { const auto folder = ChooseFolder(L"Выберите папку для поиска дубликатов"); if (!folder.empty()) { duplicateRoot_ = folder; SetStatus(L"Папка: " + folder.wstring()); } return; }
        if (buttonIndex == 1) {
            if (duplicateRoot_.empty()) { SetStatus(L"Сначала выбери папку."); return; }
            const auto root = duplicateRoot_; const auto minBytes = static_cast<std::uint64_t>(settings_.duplicateMinMB) * 1024ull * 1024ull;
            StartAsync(L"Ищем совпадения по размеру и SHA-256…", [this, root, minBytes](std::stop_token token) {
                auto files = dpop::full::FindDuplicates(root, minBytes, token);
                QueueApply([this, files = std::move(files)]() mutable {
                    duplicates_ = std::move(files); ListView_DeleteAllItems(list_);
                    for (const auto& f : duplicates_) AddRow({std::to_wstring(f.group), dpop::full::FormatBytes(f.size), f.sha256.substr(0, 16) + L"…", f.path.wstring()});
                    SetStatus(L"Файлов в группах дубликатов: " + std::to_wstring(duplicates_.size()) + L"."); Log(EventLevel::Info, L"Поиск дубликатов завершён.");
                });
            }); return;
        }
        if (buttonIndex == 2) {
            const auto indices = SelectedIndices(); if (indices.empty()) { SetStatus(L"Выбери файлы для перемещения в Корзину (Ctrl/Shift для нескольких). "); return; }
            std::vector<fs::path> paths; for (int i : indices) if (i >= 0 && i < static_cast<int>(duplicates_.size())) paths.push_back(duplicates_[static_cast<std::size_t>(i)].path);
            if (!Confirm(hwnd_, L"Переместить выбранные дубликаты в Корзину? Убедись, что в каждой группе остаётся нужная копия.", true)) return;
            StartAsync(L"Перемещаем выбранные файлы в Корзину…", [this, paths = std::move(paths)](std::stop_token) {
                auto r = dpop::full::MoveToRecycleBin(paths);
                QueueApply([this, r = std::move(r)] { SetStatus(L"В Корзине: " + std::to_wstring(r.moved) + L" • ошибок: " + std::to_wstring(r.failed)); Log(r.failed ? EventLevel::Warning : EventLevel::Info, L"Удаление выбранных дубликатов завершено."); });
            });
        }
        break;
    }
    case Page::Tools: {
        if (buttonIndex != 0) return;
        const int i = SelectedIndex(); if (i < 0 || i >= 7) { SetStatus(L"Выбери инструмент."); return; }
        std::wstring error; if (!dpop::full::OpenTool(static_cast<dpop::full::ToolAction>(i), error)) { SetStatus(error); Log(EventLevel::Error, error); }
        else SetStatus(L"Инструмент открыт.");
        break;
    }
    case Page::Zapret: {
        if (buttonIndex == 0) { RefreshZapret(); return; }
        std::wstring error; bool ok = false;
        if (buttonIndex == 1) ok = dpop::zapret::LaunchDefaultStrategy(error);
        else if (buttonIndex == 2) ok = dpop::zapret::OpenServiceManager(error);
        else if (buttonIndex == 3) ok = dpop::zapret::OpenBundledFolder(error);
        SetStatus(ok ? L"Команда Zapret выполнена/открыта." : error); Log(ok ? EventLevel::Info : EventLevel::Warning, ok ? L"Действие Zapret выполнено." : error); if (buttonIndex == 1) RefreshZapret();
        break;
    }
    case Page::Settings: {
        if (buttonIndex == 4) { RefreshSettings(); return; }
        if (buttonIndex == 0) settings_.confirmDestructive = !settings_.confirmDestructive;
        if (buttonIndex == 1) { const std::array<unsigned, 5> values = {100, 250, 500, 1024, 2048}; auto it = std::find(values.begin(), values.end(), settings_.largeFileMB); settings_.largeFileMB = it == values.end() || ++it == values.end() ? values[0] : *it; }
        if (buttonIndex == 2) { const std::array<unsigned, 5> values = {1, 5, 10, 50, 100}; auto it = std::find(values.begin(), values.end(), settings_.duplicateMinMB); settings_.duplicateMinMB = it == values.end() || ++it == values.end() ? values[0] : *it; }
        if (buttonIndex == 3) {
            const bool next = !settings_.runAtStartup; std::wstring error;
            if (!dpop::full::SetRunAtStartup(next, error)) { SetStatus(error); Log(EventLevel::Error, error); return; }
            settings_.runAtStartup = next;
        }
        std::wstring error;
        if (!dpop::full::SaveSettings(settings_, error)) { SetStatus(error); Log(EventLevel::Error, error); return; }
        RefreshSettings(); Log(EventLevel::Info, L"Настройки сохранены.");
        break;
    }
    default: break;
    }
}

LRESULT CALLBACK WorkspacePage::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    WorkspacePage* self = reinterpret_cast<WorkspacePage*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        self = static_cast<WorkspacePage*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    }
    return self ? self->HandleMessage(message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT WorkspacePage::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    const auto& p = MidnightPalette();
    switch (message) {
    case WM_SIZE: LayoutChildren(); return 0;
    case kAsyncDone: CompleteAsync(); return 0;
    case WM_COMMAND: {
        const int id = LOWORD(wParam);
        if (id >= kButtonBase && id < kButtonBase + 6) { HandleAction(id - kButtonBase); return 0; }
        break;
    }
    case WM_DRAWITEM: {
        const auto* draw = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (draw && draw->CtlType == ODT_BUTTON && draw->CtlID >= kButtonBase && draw->CtlID < kButtonBase + 6) {
            const int index = static_cast<int>(draw->CtlID) - kButtonBase;
            if (DrawOwnerButton(*draw, WindowText(draw->hwndItem), visuals_[static_cast<std::size_t>(index)])) return TRUE;
        }
        break;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam); SetBkMode(dc, OPAQUE); SetBkColor(dc, p.background); SetDCBrushColor(dc, p.background);
        SetTextColor(dc, reinterpret_cast<HWND>(lParam) == status_ ? p.muted : p.text);
        return reinterpret_cast<LRESULT>(GetStockObject(DC_BRUSH));
    }
    case WM_ERASEBKGND: {
        RECT rc{}; GetClientRect(hwnd_, &rc); HDC dc = reinterpret_cast<HDC>(wParam); SetDCBrushColor(dc, p.background);
        FillRect(dc, &rc, reinterpret_cast<HBRUSH>(GetStockObject(DC_BRUSH))); return 1;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps{}; HDC dc = BeginPaint(hwnd_, &ps); RECT rc{}; GetClientRect(hwnd_, &rc); SetDCBrushColor(dc, p.background);
        FillRect(dc, &rc, reinterpret_cast<HBRUSH>(GetStockObject(DC_BRUSH))); EndPaint(hwnd_, &ps); return 0;
    }
    default: break;
    }
    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

} // namespace dpop::ui
