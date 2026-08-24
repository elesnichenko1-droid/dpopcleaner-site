#include "ui/pages/DiskPage.h"

#include "ui/Controls.h"
#include "ui/PageLayout.h"
#include "ui/Theme.h"

#include <shellapi.h>
#include <algorithm>
#include <array>
#include <cwctype>
#include <string>

namespace fs = std::filesystem;

namespace dpop::ui {
namespace {
constexpr int kToolbarBase = 2300;
constexpr int kPath = 2320;
constexpr int kList = 2321;

std::wstring Lower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    std::replace(s.begin(), s.end(), L'/', L'\\');
    return s;
}

std::wstring Env(const wchar_t* name) {
    const DWORD needed = GetEnvironmentVariableW(name, nullptr, 0);
    if (!needed) return {};
    std::wstring value(needed, L'\0');
    GetEnvironmentVariableW(name, value.data(), needed);
    while (!value.empty() && value.back() == L'\0') value.pop_back();
    return value;
}

bool Under(const fs::path& path, const fs::path& root) {
    if (root.empty()) return false;
    const auto p = Lower(path.lexically_normal().wstring());
    auto r = Lower(root.lexically_normal().wstring());
    while (r.size() > 3 && !r.empty() && r.back() == L'\\') r.pop_back();
    if (p == r) return true;
    return p.size() > r.size() && p.rfind(r + L"\\", 0) == 0;
}

bool IsProtectedPath(const fs::path& path) {
    wchar_t windows[MAX_PATH]{};
    GetWindowsDirectoryW(windows, static_cast<UINT>(std::size(windows)));
    const std::array<fs::path, 4> roots = {
        fs::path(windows), fs::path(Env(L"ProgramFiles")), fs::path(Env(L"ProgramFiles(x86)")), fs::path(Env(L"ProgramData"))
    };
    for (const auto& root : roots) if (!root.empty() && Under(path, root)) return true;
    return false;
}

std::wstring FileType(const fs::path& path, bool directory) {
    if (directory) return L"Папка";
    auto ext = path.extension().wstring();
    if (ext.empty()) return L"Файл";
    for (auto& c : ext) c = static_cast<wchar_t>(std::towupper(c));
    return ext + L" файл";
}

std::wstring ModifiedText(const fs::path& path) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) return L"—";
    FILETIME local{};
    SYSTEMTIME st{};
    if (!FileTimeToLocalFileTime(&data.ftLastWriteTime, &local) || !FileTimeToSystemTime(&local, &st)) return L"—";
    wchar_t buffer[64]{};
    swprintf_s(buffer, L"%02u.%02u.%04u %02u:%02u", st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute);
    return buffer;
}
}

bool DiskPage::OnCreate() {
    if (!fonts_.Create()) return false;
    settings_ = dpop::full::LoadSettings();
    const std::array<std::wstring_view, 7> labels = {
        L"Назад", L"Вверх", L"C:\\", L"Выбрать", L"Обновить", L"Крупные файлы", L"Проводник"
    };
    for (std::size_t i = 0; i < labels.size(); ++i) {
        toolbar_[i] = CreatePushButton(Hwnd(), kToolbarBase + static_cast<int>(i), labels[i], i == 4 ? ButtonVisual::Accent : ButtonVisual::Normal);
        if (!toolbar_[i]) return false;
        ApplyControlFont(toolbar_[i], fonts_.smallFont);
    }
    pathEdit_ = CreateDarkEdit(Hwnd(), kPath, root_.wstring(), false);
    list_ = CreateDarkListView(Hwnd(), kList);
    if (!pathEdit_ || !list_) return false;
    ApplyControlFont(pathEdit_, fonts_.body);
    ApplyControlFont(list_, fonts_.smallFont);

    ResetList(list_);
    AddListColumn(list_, 0, L"Имя", 310);
    AddListColumn(list_, 1, L"Размер", 120);
    AddListColumn(list_, 2, L"Тип", 120);
    AddListColumn(list_, 3, L"Изменён", 150);
    AddListColumn(list_, 4, L"Безопасность", 260);
    AddListColumn(list_, 5, L"Полный путь", 420);

    SHFILEINFOW sfi{};
    systemImages_ = reinterpret_cast<HIMAGELIST>(SHGetFileInfoW(
        L"C:\\", FILE_ATTRIBUTE_DIRECTORY, &sfi, sizeof(sfi),
        SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES));
    if (systemImages_) ListView_SetImageList(list_, systemImages_, LVSIL_SMALL);

    Navigate(root_, true);
    return true;
}

void DiskPage::OnVisibilityChanged(bool visible) noexcept {
    if (!visible) Cancel();
}

void DiskPage::Navigate(const fs::path& root, bool addHistory) {
    std::error_code ec;
    if (root.empty() || !fs::exists(root, ec) || !fs::is_directory(root, ec)) {
        SetStatus(L"Папка не существует или недоступна.");
        return;
    }
    root_ = root.lexically_normal();
    SetControlText(pathEdit_, root_.wstring());
    largeMode_ = false;
    if (addHistory) {
        if (historyIndex_ + 1 < static_cast<int>(history_.size())) history_.erase(history_.begin() + historyIndex_ + 1, history_.end());
        history_.push_back(root_);
        historyIndex_ = static_cast<int>(history_.size()) - 1;
    }
    Browse();
}

void DiskPage::Browse() {
    const fs::path root = root_;
    StartAsync(L"Читаем каталог…", [this, root](std::stop_token token) {
        std::vector<BrowserEntry> entries;
        std::error_code ec;
        for (fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
             it != end && !token.stop_requested(); it.increment(ec)) {
            if (ec) { ec.clear(); continue; }
            BrowserEntry entry{};
            entry.path = it->path();
            entry.directory = it->is_directory(ec);
            ec.clear();
            if (!entry.directory && it->is_regular_file(ec)) entry.size = it->file_size(ec);
            ec.clear();
            entry.protectedPath = IsProtectedPath(entry.path);
            entries.push_back(std::move(entry));
            if (entries.size() >= 5000) break;
        }
        std::stable_sort(entries.begin(), entries.end(), [](const BrowserEntry& a, const BrowserEntry& b) {
            if (a.directory != b.directory) return a.directory > b.directory;
            return Lower(a.path.filename().wstring()) < Lower(b.path.filename().wstring());
        });
        QueueApply([this, entries = std::move(entries)]() mutable { PopulateBrowser(std::move(entries)); });
    });
}

int DiskPage::IconIndexFor(const fs::path& path, bool directory) {
    SHFILEINFOW sfi{};
    std::error_code ec;
    const bool exists = fs::exists(path, ec);
    UINT flags = SHGFI_SYSICONINDEX | SHGFI_SMALLICON;
    DWORD attrs = directory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
    if (!exists) flags |= SHGFI_USEFILEATTRIBUTES;
    if (!SHGetFileInfoW(path.c_str(), attrs, &sfi, sizeof(sfi), flags)) return 0;
    return sfi.iIcon;
}

void DiskPage::PopulateBrowser(std::vector<BrowserEntry> entries) {
    entries_ = std::move(entries);
    ListView_DeleteAllItems(list_);
    for (const auto& entry : entries_) {
        const std::wstring size = entry.directory ? L"—" : dpop::full::FormatBytes(entry.size);
        const std::wstring safety = entry.protectedPath
            ? L"Системный путь — не изменяйте без необходимости"
            : entry.directory ? L"Папка пользователя / приложения" : L"Обычный файл";
        const int row = AddListRow(list_, {
            entry.path.filename().wstring().empty() ? entry.path.wstring() : entry.path.filename().wstring(),
            size,
            FileType(entry.path, entry.directory),
            ModifiedText(entry.path),
            safety,
            entry.path.wstring()
        });
        if (row >= 0 && systemImages_) {
            LVITEMW item{};
            item.mask = LVIF_IMAGE;
            item.iItem = row;
            item.iImage = IconIndexFor(entry.path, entry.directory);
            ListView_SetItem(list_, &item);
        }
    }
    summary_ = L"Папка: " + root_.wstring() + L" • элементов: " + std::to_wstring(entries_.size());
    if (IsProtectedPath(root_)) summary_ += L" • системный путь: только просмотр/навигация";
    SetStatus(summary_);
    Log(EventLevel::Info, L"Каталог открыт во встроенном проводнике.");
    InvalidateRect(Hwnd(), nullptr, TRUE);
}

void DiskPage::ScanLarge() {
    const fs::path root = root_;
    const std::uint64_t minBytes = static_cast<std::uint64_t>(settings_.largeFileMB) * 1024ull * 1024ull;
    StartAsync(L"Ищем крупные файлы рекурсивно…", [this, root, minBytes](std::stop_token token) {
        auto files = dpop::full::ScanLargeFiles(root, minBytes, token, 1200);
        QueueApply([this, files = std::move(files)]() mutable { PopulateLarge(std::move(files)); });
    });
}

void DiskPage::PopulateLarge(std::vector<dpop::full::FileItem> files) {
    entries_.clear();
    entries_.reserve(files.size());
    for (auto& file : files) entries_.push_back({std::move(file.path), file.size, false, IsProtectedPath(file.path)});
    largeMode_ = true;
    ListView_DeleteAllItems(list_);
    for (const auto& entry : entries_) {
        const int row = AddListRow(list_, {
            entry.path.filename().wstring(), dpop::full::FormatBytes(entry.size), FileType(entry.path, false),
            ModifiedText(entry.path), entry.protectedPath ? L"Системный путь — осторожно" : L"Обычный файл", entry.path.wstring()
        });
        if (row >= 0 && systemImages_) {
            LVITEMW item{}; item.mask = LVIF_IMAGE; item.iItem = row; item.iImage = IconIndexFor(entry.path, false); ListView_SetItem(list_, &item);
        }
    }
    summary_ = L"Крупные файлы ≥ " + std::to_wstring(settings_.largeFileMB) + L" МБ: " + std::to_wstring(entries_.size());
    SetStatus(summary_);
    Log(EventLevel::Info, L"Поиск крупных файлов завершён.");
    InvalidateRect(Hwnd(), nullptr, TRUE);
}

void DiskPage::OpenSelected(bool activate) {
    const int index = SelectedListIndex(list_);
    if (index < 0 || index >= static_cast<int>(entries_.size())) {
        if (activate) OpenPathInExplorer(Hwnd(), root_);
        else SetStatus(L"Выберите файл или папку.");
        return;
    }
    const auto entry = entries_[static_cast<std::size_t>(index)];
    if (activate && entry.directory && !largeMode_) {
        Navigate(entry.path, true);
        return;
    }
    OpenPathInExplorer(Hwnd(), entry.path, !entry.directory);
    SetStatus(entry.directory ? L"Папка открыта в Проводнике." : L"Файл выделен в Проводнике.");
}

void DiskPage::OpenExplorer() {
    OpenPathInExplorer(Hwnd(), root_);
    SetStatus(L"Текущая папка открыта в Проводнике Windows.");
}

void DiskPage::OnLayout(int width, int height) noexcept {
    const UINT dpiRaw = GetDpiForWindow(Hwnd());
    const int top = ComputePageContentTop(dpiRaw ? static_cast<int>(dpiRaw) : 96);
    const int margin = 18;
    const int y = top + 28;
    const int h = 30;
    const int gap = 6;
    const int widths[7] = {62, 62, 48, 72, 78, 108, 82};
    const int fixed = widths[0] + widths[1] + widths[2] + widths[3] + widths[4] + widths[5] + widths[6] + gap * 7;
    const int pathW = std::max(180, width - margin * 2 - 24 - fixed);
    int x = margin + 12;
    for (int i = 0; i < 4; ++i) { MoveWindow(toolbar_[static_cast<std::size_t>(i)], x, y, widths[i], h, TRUE); x += widths[i] + gap; }
    MoveWindow(pathEdit_, x, y, pathW, h, TRUE); x += pathW + gap;
    for (int i = 4; i < 7; ++i) { MoveWindow(toolbar_[static_cast<std::size_t>(i)], x, y, widths[i], h, TRUE); x += widths[i] + gap; }
    const int listY = top + 70;
    MoveWindow(list_, margin + 10, listY, std::max(120, width - margin * 2 - 20), std::max(120, height - margin - listY), TRUE);
}

void DiskPage::OnPaint(HDC dc, const RECT& client) noexcept {
    PageBase::OnPaint(dc, client);
    DrawPageHeading(dc, 18, 4, L"Диск", L"Встроенный проводник: папки открываются двойным кликом; крупные файлы сканируются отдельно, системные пути помечаются явно.", fonts_.title, fonts_.body);
    const UINT dpiRaw = GetDpiForWindow(Hwnd());
    const int top = ComputePageContentTop(dpiRaw ? static_cast<int>(dpiRaw) : 96);
    const int margin = 18;
    RECT toolbar{margin, top, client.right - margin, top + 74};
    RECT table{margin, top + 86, client.right - margin, client.bottom - margin};
    DrawPanel(dc, toolbar, true);
    DrawPanel(dc, table, false);
    DrawPanelTitle(dc, toolbar, L"Навигация и анализ", fonts_.section);
    DrawPanelTitle(dc, table, largeMode_ ? L"Крупные файлы" : L"Содержимое папки", fonts_.section);

    const auto& p = MidnightPalette();
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, p.muted);
    HGDIOBJ old = SelectObject(dc, fonts_.smallFont);
    RECT status{table.left + 12, table.top + 8, table.right - 12, table.top + 30};
    std::wstring text = summary_;
    DrawTextW(dc, text.data(), static_cast<int>(text.size()), &status, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(dc, old);
}

LRESULT DiskPage::OnMessage(UINT message, WPARAM wParam, LPARAM lParam, bool& handled) {
    if (message == WM_COMMAND) {
        const int id = LOWORD(wParam);
        if (id == kToolbarBase + 0) {
            if (historyIndex_ > 0) { --historyIndex_; Navigate(history_[static_cast<std::size_t>(historyIndex_)], false); }
            else SetStatus(L"Предыдущей папки в истории нет.");
            handled = true; return 0;
        }
        if (id == kToolbarBase + 1) {
            if (root_.has_parent_path() && root_.parent_path() != root_) Navigate(root_.parent_path(), true);
            handled = true; return 0;
        }
        if (id == kToolbarBase + 2) { Navigate(fs::path(L"C:\\"), true); handled = true; return 0; }
        if (id == kToolbarBase + 3) {
            const auto folder = ChooseFolder(Hwnd(), L"Выберите папку или диск");
            if (!folder.empty()) Navigate(folder, true);
            handled = true; return 0;
        }
        if (id == kToolbarBase + 4) { Navigate(GetControlText(pathEdit_), false); handled = true; return 0; }
        if (id == kToolbarBase + 5) { ScanLarge(); handled = true; return 0; }
        if (id == kToolbarBase + 6) { OpenExplorer(); handled = true; return 0; }
        if (id == kPath && HIWORD(wParam) == EN_KILLFOCUS) { Navigate(GetControlText(pathEdit_), true); handled = true; return 0; }
    }
    if (message == WM_NOTIFY) {
        const auto* hdr = reinterpret_cast<const NMHDR*>(lParam);
        if (hdr && hdr->hwndFrom == list_ && hdr->code == NM_DBLCLK) { OpenSelected(true); handled = true; return 0; }
    }
    if (message == WM_DRAWITEM) {
        const auto* draw = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (draw && draw->CtlType == ODT_BUTTON && draw->CtlID >= kToolbarBase && draw->CtlID < kToolbarBase + 7) {
            const ButtonVisual visual = draw->CtlID == kToolbarBase + 4 ? ButtonVisual::Accent : ButtonVisual::Normal;
            handled = DrawOwnerButton(*draw, GetControlText(draw->hwndItem), visual);
            return handled ? TRUE : 0;
        }
    }
    handled = false;
    return 0;
}

}
