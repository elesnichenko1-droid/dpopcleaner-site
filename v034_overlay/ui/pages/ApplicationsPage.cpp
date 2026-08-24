#include "ui/pages/ApplicationsPage.h"

#include "modules/FullCore.h"
#include "ui/Controls.h"
#include "ui/PageLayout.h"
#include "ui/Theme.h"

#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <cwctype>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace dpop::ui {
namespace {
constexpr int kSearch = 2400;
constexpr int kCount = 2401;
constexpr int kList = 2402;
constexpr int kDetailBase = 2410;
constexpr int kButtonBase = 2430;

std::wstring Lower(std::wstring text) {
    std::transform(text.begin(), text.end(), text.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return text;
}

std::wstring Trim(std::wstring value) {
    const auto first = value.find_first_not_of(L" \t\r\n\"");
    if (first == std::wstring::npos) return {};
    const auto last = value.find_last_not_of(L" \t\r\n\"");
    return value.substr(first, last - first + 1);
}

fs::path DisplayIconPath(const dpop::apps::InstalledApp& app) {
    std::wstring raw = Trim(app.displayIcon);
    if (!raw.empty()) {
        const auto comma = raw.rfind(L',');
        if (comma != std::wstring::npos) raw.resize(comma);
        raw = Trim(raw);
        DWORD needed = ExpandEnvironmentStringsW(raw.c_str(), nullptr, 0);
        if (needed > 1) {
            std::wstring expanded(needed, L'\0');
            ExpandEnvironmentStringsW(raw.c_str(), expanded.data(), needed);
            while (!expanded.empty() && expanded.back() == L'\0') expanded.pop_back();
            raw = std::move(expanded);
        }
        if (!raw.empty()) return fs::path(raw);
    }
    if (!app.installLocation.empty()) return fs::path(app.installLocation);
    return {};
}

std::wstring BytesToWide(const std::string& bytes) {
    if (bytes.empty()) return {};
    UINT cp = CP_UTF8;
    int needed = MultiByteToWideChar(cp, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
    if (needed <= 0) {
        cp = CP_ACP;
        needed = MultiByteToWideChar(cp, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
    }
    if (needed <= 0) return L"WinGet вернул вывод, который не удалось декодировать.";
    std::wstring out(static_cast<std::size_t>(needed), L'\0');
    MultiByteToWideChar(cp, cp == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0, bytes.data(), static_cast<int>(bytes.size()), out.data(), needed);
    return out;
}

struct WingetResult {
    bool available{false};
    bool started{false};
    DWORD exitCode{0};
    std::wstring output;
};

WingetResult CheckWithWinget(const std::wstring& displayName) {
    WingetResult result{};
    wchar_t exe[MAX_PATH * 4]{};
    const DWORD length = SearchPathW(nullptr, L"winget.exe", nullptr, static_cast<DWORD>(std::size(exe)), exe, nullptr);
    if (!length || length >= std::size(exe)) {
        result.output = L"WinGet не найден. Установите/обновите App Installer из Microsoft Store, либо проверяйте приложение у издателя.";
        return result;
    }
    result.available = true;

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE readPipe = nullptr, writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) {
        result.output = L"Не удалось создать канал для WinGet.";
        return result;
    }
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    std::wstring command = L"\"" + std::wstring(exe) + L"\" upgrade --name \"" + displayName +
        L"\" --exact --accept-source-agreements --disable-interactivity --include-unknown";
    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = writePipe;
    si.hStdError = writePipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi{};
    const BOOL created = CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(writePipe);
    if (!created) {
        CloseHandle(readPipe);
        result.output = L"WinGet найден, но не запустился. Код Windows: " + std::to_wstring(GetLastError());
        return result;
    }
    result.started = true;

    std::string bytes;
    std::array<char, 4096> buffer{};
    DWORD read = 0;
    while (ReadFile(readPipe, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr) && read) {
        if (bytes.size() < 128 * 1024) bytes.append(buffer.data(), read);
    }
    CloseHandle(readPipe);
    WaitForSingleObject(pi.hProcess, 120000);
    GetExitCodeProcess(pi.hProcess, &result.exitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    result.output = BytesToWide(bytes);
    if (result.output.empty()) result.output = L"WinGet завершил проверку без текстового результата. Код: " + std::to_wstring(result.exitCode);
    return result;
}
}

bool ApplicationsPage::OnCreate() {
    if (!fonts_.Create()) return false;
    search_ = CreateDarkEdit(Hwnd(), kSearch, L"");
    count_ = CreateTextLabel(Hwnd(), kCount, L"0 программ", SS_RIGHT | SS_NOPREFIX);
    list_ = CreateDarkListView(Hwnd(), kList);
    if (!search_ || !count_ || !list_) return false;
    ApplyControlFont(search_, fonts_.body);
    ApplyControlFont(count_, fonts_.smallFont);
    ApplyControlFont(list_, fonts_.smallFont);
    SendMessageW(search_, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Поиск по названию или издателю…"));

    ResetList(list_);
    AddListColumn(list_, 0, L"Приложение", 285);
    AddListColumn(list_, 1, L"Версия", 110);
    AddListColumn(list_, 2, L"Издатель", 210);
    AddListColumn(list_, 3, L"Путь", 340);

    SHFILEINFOW sfi{};
    systemImages_ = reinterpret_cast<HIMAGELIST>(SHGetFileInfoW(
        L"C:\\", FILE_ATTRIBUTE_DIRECTORY, &sfi, sizeof(sfi),
        SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES));
    if (systemImages_) ListView_SetImageList(list_, systemImages_, LVSIL_SMALL);

    for (std::size_t i = 0; i < detailLabels_.size(); ++i) {
        detailLabels_[i] = CreateTextLabel(Hwnd(), kDetailBase + static_cast<int>(i), L"—", SS_LEFT | SS_NOPREFIX);
        if (!detailLabels_[i]) return false;
        ApplyControlFont(detailLabels_[i], i == 0 ? fonts_.section : fonts_.smallFont);
    }

    const std::array<std::wstring_view, 7> labels = {
        L"Обновить список", L"Проверить обновление", L"Удалить", L"Открыть папку", L"Найти хвосты",
        L"Хвосты в Корзину", L"К приложениям"
    };
    for (std::size_t i = 0; i < labels.size(); ++i) {
        const ButtonVisual visual = i == 0 || i == 1 ? ButtonVisual::Accent : i == 2 || i == 5 ? ButtonVisual::Danger : ButtonVisual::Normal;
        buttons_[i] = CreatePushButton(Hwnd(), kButtonBase + static_cast<int>(i), labels[i], visual);
        if (!buttons_[i]) return false;
        ApplyControlFont(buttons_[i], fonts_.smallFont);
    }
    ShowWindow(buttons_[5], SW_HIDE);
    ShowWindow(buttons_[6], SW_HIDE);
    return true;
}

void ApplicationsPage::OnVisibilityChanged(bool visible) noexcept {
    if (!visible) { Cancel(); return; }
    if (!loaded_ && !IsBusy()) RefreshApps();
}

int ApplicationsPage::IconIndexForPath(const fs::path& path) {
    if (path.empty()) return 0;
    SHFILEINFOW sfi{};
    std::error_code ec;
    const bool directory = fs::is_directory(path, ec);
    ec.clear();
    const bool exists = fs::exists(path, ec);
    UINT flags = SHGFI_SYSICONINDEX | SHGFI_SMALLICON;
    DWORD attrs = directory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
    if (!exists) flags |= SHGFI_USEFILEATTRIBUTES;
    if (!SHGetFileInfoW(path.c_str(), attrs, &sfi, sizeof(sfi), flags)) return 0;
    return sfi.iIcon;
}

int ApplicationsPage::IconIndexFor(const dpop::apps::InstalledApp& app) {
    return IconIndexForPath(DisplayIconPath(app));
}

void ApplicationsPage::OnLayout(int width, int height) noexcept {
    const UINT dpiRaw = GetDpiForWindow(Hwnd());
    const int top = ComputePageContentTop(dpiRaw ? static_cast<int>(dpiRaw) : 96);
    const int margin = 18;
    const int searchPanelH = 72;
    const int gap = 12;
    const int detailW = std::min(350, std::max(300, width / 3));
    const int listRight = width - margin - detailW - gap;

    MoveWindow(search_, margin + 12, top + 34, std::max(220, listRight - margin - 150), 30, TRUE);
    MoveWindow(count_, listRight - 130, top + 36, 120, 26, TRUE);
    MoveWindow(list_, margin + 10, top + searchPanelH + 2, std::max(260, listRight - margin - 10), std::max(160, height - margin - (top + searchPanelH + 2)), TRUE);

    const int x = listRight + gap + 14;
    const int w = detailW - 28;
    MoveWindow(detailLabels_[0], x, top + 36, w, 28, TRUE);
    MoveWindow(detailLabels_[1], x, top + 66, w, 24, TRUE);
    MoveWindow(detailLabels_[2], x, top + 92, w, 32, TRUE);
    MoveWindow(detailLabels_[3], x, top + 124, w, 48, TRUE);
    MoveWindow(detailLabels_[4], x, top + 174, w, 42, TRUE);

    int y = top + 228;
    for (HWND button : buttons_) {
        if (!button || !IsWindowVisible(button)) continue;
        MoveWindow(button, x, y, w, 30, TRUE);
        y += 36;
    }
}

void ApplicationsPage::OnPaint(HDC dc, const RECT& client) noexcept {
    PageBase::OnPaint(dc, client);
    DrawPageHeading(dc, 18, 4, L"Приложения", L"Иконки, штатное удаление, проверка обновлений через WinGet и безопасный поиск хвостов после деинсталляции.", fonts_.title, fonts_.body);
    const UINT dpiRaw = GetDpiForWindow(Hwnd());
    const int top = ComputePageContentTop(dpiRaw ? static_cast<int>(dpiRaw) : 96);
    const int width = static_cast<int>(client.right);
    const int height = static_cast<int>(client.bottom);
    const int margin = 18;
    const int detailW = std::min(350, std::max(300, width / 3));
    const int gap = 12;
    const int searchPanelH = 72;
    const int listRight = width - margin - detailW - gap;
    RECT search{margin, top, listRight, top + searchPanelH};
    RECT list{margin, top + searchPanelH + 2, listRight, height - margin};
    RECT detail{listRight + gap, top, width - margin, height - margin};
    DrawPanel(dc, search, true);
    DrawPanel(dc, list, false);
    DrawPanel(dc, detail, false);
    DrawPanelTitle(dc, search, leftoversMode_ ? L"Результаты поиска хвостов" : L"Поиск и фильтр", fonts_.section);
    DrawPanelTitle(dc, list, leftoversMode_ ? L"Хвосты выбранного приложения" : L"Установленные приложения", fonts_.section);
    DrawPanelTitle(dc, detail, leftoversMode_ ? L"Безопасное удаление хвостов" : L"Карточка приложения", fonts_.section);
}

void ApplicationsPage::RefreshApps() {
    StartAsync(L"Читаем установленные приложения…", [this](std::stop_token) {
        auto apps = dpop::apps::EnumerateInstalledApps();
        QueueApply([this, apps = std::move(apps)]() mutable {
            apps_ = std::move(apps);
            leftovers_.clear();
            leftoversMode_ = false;
            loaded_ = true;
            ShowWindow(buttons_[5], SW_HIDE);
            ShowWindow(buttons_[6], SW_HIDE);
            for (int i = 0; i < 5; ++i) ShowWindow(buttons_[static_cast<std::size_t>(i)], SW_SHOW);
            EnableWindow(search_, TRUE);
            ResetList(list_);
            AddListColumn(list_, 0, L"Приложение", 285);
            AddListColumn(list_, 1, L"Версия", 110);
            AddListColumn(list_, 2, L"Издатель", 210);
            AddListColumn(list_, 3, L"Путь", 340);
            if (systemImages_) ListView_SetImageList(list_, systemImages_, LVSIL_SMALL);
            ApplyFilter();
            SetStatus(L"Установлено приложений: " + std::to_wstring(apps_.size()) + L".");
            Log(EventLevel::Info, L"Список приложений обновлён.");
            RECT rc{}; GetClientRect(Hwnd(), &rc); OnLayout(rc.right, rc.bottom);
            InvalidateRect(Hwnd(), nullptr, TRUE);
        });
    });
}

void ApplicationsPage::ApplyFilter() {
    if (leftoversMode_) return;
    const std::wstring query = Lower(GetControlText(search_));
    ListView_DeleteAllItems(list_);
    visibleAppIndices_.clear();
    for (std::size_t i = 0; i < apps_.size(); ++i) {
        const auto& app = apps_[i];
        const std::wstring haystack = Lower(app.displayName + L" " + app.publisher + L" " + app.displayVersion);
        if (!query.empty() && haystack.find(query) == std::wstring::npos) continue;
        visibleAppIndices_.push_back(i);
        const int row = AddListRow(list_, {app.displayName, app.displayVersion, app.publisher, app.installLocation});
        if (row >= 0 && systemImages_) {
            LVITEMW item{}; item.mask = LVIF_IMAGE; item.iItem = row; item.iImage = IconIndexFor(app); ListView_SetItem(list_, &item);
        }
    }
    SetControlText(count_, std::to_wstring(visibleAppIndices_.size()) + L" / " + std::to_wstring(apps_.size()));
    UpdateDetails();
}

void ApplicationsPage::UpdateDetails() {
    const int row = SelectedListIndex(list_);
    if (leftoversMode_) {
        if (row < 0 || row >= static_cast<int>(leftovers_.size())) {
            SetControlText(detailLabels_[0], L"Хвосты приложения");
            SetControlText(detailLabels_[1], L"Отмечайте только элементы HIGH.");
            SetControlText(detailLabels_[2], L"Review-элементы не удаляются автоматически.");
            SetControlText(detailLabels_[3], L"Удаление — только через Корзину Windows.");
            SetControlText(detailLabels_[4], L"Выберите строку для просмотра пути.");
            return;
        }
        const auto& item = leftovers_[static_cast<std::size_t>(row)];
        SetControlText(detailLabels_[0], item.path.filename().wstring());
        SetControlText(detailLabels_[1], item.highConfidence ? L"Уверенность: HIGH" : L"Уверенность: review");
        SetControlText(detailLabels_[2], item.reason);
        SetControlText(detailLabels_[3], item.path.wstring());
        SetControlText(detailLabels_[4], item.highConfidence ? L"Можно отметить для Корзины после проверки." : L"Требует ручной проверки; DPopCleaner не удаляет автоматически.");
        return;
    }
    if (row < 0 || row >= static_cast<int>(visibleAppIndices_.size())) {
        for (HWND label : detailLabels_) SetControlText(label, L"—");
        SetControlText(detailLabels_[0], L"Выберите приложение");
        return;
    }
    const auto& app = apps_[visibleAppIndices_[static_cast<std::size_t>(row)]];
    SetControlText(detailLabels_[0], app.displayName);
    SetControlText(detailLabels_[1], L"Версия: " + (app.displayVersion.empty() ? std::wstring(L"не указана") : app.displayVersion));
    SetControlText(detailLabels_[2], L"Издатель: " + (app.publisher.empty() ? std::wstring(L"не указан") : app.publisher));
    SetControlText(detailLabels_[3], app.installLocation.empty() ? L"Путь установки не указан" : app.installLocation);
    SetControlText(detailLabels_[4], app.windowsInstaller ? L"Деинсталлятор: Windows Installer (MSI)" : L"Деинсталлятор: vendor/registry command");
}

void ApplicationsPage::CheckUpdate() {
    const int row = SelectedListIndex(list_);
    if (leftoversMode_ || row < 0 || row >= static_cast<int>(visibleAppIndices_.size())) {
        SetStatus(L"Выберите установленное приложение.");
        return;
    }
    const auto app = apps_[visibleAppIndices_[static_cast<std::size_t>(row)]];
    StartAsync(L"WinGet проверяет обновление для выбранного приложения…", [this, app](std::stop_token) {
        auto result = CheckWithWinget(app.displayName);
        QueueApply([this, app, result = std::move(result)]() mutable {
            std::wstring text = result.output;
            if (text.size() > 12000) text.resize(12000);
            MessageBoxW(Hwnd(), text.c_str(), (L"WinGet — " + app.displayName).c_str(), MB_OK | (result.available ? MB_ICONINFORMATION : MB_ICONWARNING));
            SetStatus(result.available ? L"WinGet завершил проверку обновления. Код: " + std::to_wstring(result.exitCode) : L"WinGet недоступен.");
            Log(result.available ? EventLevel::Info : EventLevel::Warning, L"Проверка обновления приложения через WinGet завершена.");
        });
    });
}

void ApplicationsPage::UninstallSelected() {
    const int row = SelectedListIndex(list_);
    if (leftoversMode_ || row < 0 || row >= static_cast<int>(visibleAppIndices_.size())) { SetStatus(L"Выберите приложение."); return; }
    const auto app = apps_[visibleAppIndices_[static_cast<std::size_t>(row)]];
    if (!ConfirmAction(Hwnd(), L"Запустить штатный деинсталлятор выбранного приложения? DPopCleaner не удаляет файлы программы напрямую.", true)) return;
    std::wstring error;
    if (dpop::full::LaunchUninstaller(app, error)) {
        SetStatus(L"Штатный деинсталлятор завершён/запущен. Обновите список и при необходимости проверьте хвосты.");
        Log(EventLevel::Info, L"Штатный деинсталлятор приложения запущен.");
    } else {
        SetStatus(error);
        Log(EventLevel::Warning, error);
    }
}

void ApplicationsPage::OpenSelectedFolder() {
    const int row = SelectedListIndex(list_);
    if (leftoversMode_) {
        if (row >= 0 && row < static_cast<int>(leftovers_.size())) OpenPathInExplorer(Hwnd(), leftovers_[static_cast<std::size_t>(row)].path, true);
        return;
    }
    if (row < 0 || row >= static_cast<int>(visibleAppIndices_.size())) { SetStatus(L"Выберите приложение."); return; }
    const auto& app = apps_[visibleAppIndices_[static_cast<std::size_t>(row)]];
    const fs::path icon = DisplayIconPath(app);
    std::error_code ec;
    if (!app.installLocation.empty() && fs::exists(app.installLocation, ec)) OpenPathInExplorer(Hwnd(), app.installLocation);
    else if (!icon.empty() && fs::exists(icon, ec)) OpenPathInExplorer(Hwnd(), icon, true);
    else SetStatus(L"Путь установки и файл иконки не найдены.");
}

void ApplicationsPage::FindLeftovers() {
    const int row = SelectedListIndex(list_);
    if (leftoversMode_ || row < 0 || row >= static_cast<int>(visibleAppIndices_.size())) { SetStatus(L"Сначала выберите приложение."); return; }
    const auto app = apps_[visibleAppIndices_[static_cast<std::size_t>(row)]];
    StartAsync(L"Ищем хвосты выбранного приложения…", [this, app](std::stop_token) {
        auto leftovers = dpop::apps::FindLeftovers(app);
        QueueApply([this, leftovers = std::move(leftovers)]() mutable {
            leftovers_ = std::move(leftovers);
            leftoversMode_ = true;
            SetControlText(search_, L"");
            EnableWindow(search_, FALSE);
            ResetList(list_, true);
            AddListColumn(list_, 0, L"Элемент", 220);
            AddListColumn(list_, 1, L"Уверенность", 105);
            AddListColumn(list_, 2, L"Причина", 260);
            AddListColumn(list_, 3, L"Путь", 380);
            if (systemImages_) ListView_SetImageList(list_, systemImages_, LVSIL_SMALL);
            for (std::size_t i = 0; i < leftovers_.size(); ++i) {
                const auto& item = leftovers_[i];
                const int listRow = AddListRow(list_, {item.path.filename().wstring(), item.highConfidence ? L"HIGH" : L"review", item.reason, item.path.wstring()});
                if (listRow >= 0 && systemImages_) { LVITEMW lv{}; lv.mask = LVIF_IMAGE; lv.iItem = listRow; lv.iImage = IconIndexForPath(item.path); ListView_SetItem(list_, &lv); }
                ListView_SetCheckState(list_, static_cast<int>(i), FALSE);
            }
            for (int i = 0; i < 5; ++i) ShowWindow(buttons_[static_cast<std::size_t>(i)], SW_HIDE);
            ShowWindow(buttons_[5], SW_SHOW);
            ShowWindow(buttons_[6], SW_SHOW);
            SetControlText(count_, std::to_wstring(leftovers_.size()) + L" хвостов");
            UpdateDetails();
            SetStatus(L"Найдено хвостов: " + std::to_wstring(leftovers_.size()) + L". DPopCleaner не отмечает их автоматически.");
            RECT rc{}; GetClientRect(Hwnd(), &rc); OnLayout(rc.right, rc.bottom);
            InvalidateRect(Hwnd(), nullptr, TRUE);
        });
    });
}

void ApplicationsPage::RecycleLeftovers() {
    if (!leftoversMode_) return;
    std::vector<dpop::apps::LeftoverItem> selected;
    for (int row : CheckedListIndices(list_)) {
        if (row >= 0 && row < static_cast<int>(leftovers_.size()) && leftovers_[static_cast<std::size_t>(row)].highConfidence) selected.push_back(leftovers_[static_cast<std::size_t>(row)]);
    }
    if (selected.empty()) { SetStatus(L"Отметьте high-confidence хвосты после ручной проверки."); return; }
    if (!ConfirmAction(Hwnd(), L"Переместить отмеченные high-confidence хвосты в Корзину?", true)) return;
    StartAsync(L"Перемещаем хвосты в Корзину…", [this, selected = std::move(selected)](std::stop_token) {
        std::size_t removed = 0;
        std::wstring error;
        const bool ok = dpop::apps::MoveLeftoversToRecycleBin(selected, removed, error);
        QueueApply([this, ok, removed, error = std::move(error)] {
            SetStatus(ok ? L"Хвостов перемещено: " + std::to_wstring(removed) : error);
            Log(ok ? EventLevel::Info : EventLevel::Warning, ok ? L"Хвосты приложения перемещены в Корзину." : error);
            ShowAppsMode();
            RefreshApps();
        });
    });
}

void ApplicationsPage::ShowAppsMode() {
    leftoversMode_ = false;
    EnableWindow(search_, TRUE);
    ResetList(list_);
    AddListColumn(list_, 0, L"Приложение", 285);
    AddListColumn(list_, 1, L"Версия", 110);
    AddListColumn(list_, 2, L"Издатель", 210);
    AddListColumn(list_, 3, L"Путь", 340);
    if (systemImages_) ListView_SetImageList(list_, systemImages_, LVSIL_SMALL);
    for (int i = 0; i < 5; ++i) ShowWindow(buttons_[static_cast<std::size_t>(i)], SW_SHOW);
    ShowWindow(buttons_[5], SW_HIDE);
    ShowWindow(buttons_[6], SW_HIDE);
    ApplyFilter();
    RECT rc{}; GetClientRect(Hwnd(), &rc); OnLayout(rc.right, rc.bottom);
    InvalidateRect(Hwnd(), nullptr, TRUE);
}

LRESULT ApplicationsPage::OnMessage(UINT message, WPARAM wParam, LPARAM lParam, bool& handled) {
    if (message == WM_COMMAND) {
        const int id = LOWORD(wParam);
        if (id == kSearch && HIWORD(wParam) == EN_CHANGE) { ApplyFilter(); handled = true; return 0; }
        if (id == kButtonBase + 0) { RefreshApps(); handled = true; return 0; }
        if (id == kButtonBase + 1) { CheckUpdate(); handled = true; return 0; }
        if (id == kButtonBase + 2) { UninstallSelected(); handled = true; return 0; }
        if (id == kButtonBase + 3) { OpenSelectedFolder(); handled = true; return 0; }
        if (id == kButtonBase + 4) { FindLeftovers(); handled = true; return 0; }
        if (id == kButtonBase + 5) { RecycleLeftovers(); handled = true; return 0; }
        if (id == kButtonBase + 6) { ShowAppsMode(); handled = true; return 0; }
    }
    if (message == WM_NOTIFY) {
        const auto* hdr = reinterpret_cast<const NMHDR*>(lParam);
        if (hdr && hdr->hwndFrom == list_) {
            if (hdr->code == LVN_ITEMCHANGED || hdr->code == NM_CLICK) UpdateDetails();
            if (hdr->code == NM_DBLCLK) OpenSelectedFolder();
            handled = true;
            return 0;
        }
    }
    if (message == WM_DRAWITEM) {
        const auto* draw = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (draw && draw->CtlType == ODT_BUTTON && draw->CtlID >= kButtonBase && draw->CtlID < kButtonBase + 7) {
            const ButtonVisual visual = (draw->CtlID == kButtonBase || draw->CtlID == kButtonBase + 1) ? ButtonVisual::Accent :
                (draw->CtlID == kButtonBase + 2 || draw->CtlID == kButtonBase + 5) ? ButtonVisual::Danger : ButtonVisual::Normal;
            handled = DrawOwnerButton(*draw, GetControlText(draw->hwndItem), visual);
            return handled ? TRUE : 0;
        }
    }
    handled = false;
    return 0;
}

}
