#include "ui/pages/DuplicatesPage.h"

#include "ui/Controls.h"
#include "ui/PageLayout.h"
#include "ui/Theme.h"

#include <shellapi.h>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cwctype>
#include <map>
#include <set>
#include <string>

namespace fs = std::filesystem;

namespace dpop::ui {
namespace {
constexpr int kPath = 2600;
constexpr int kThreshold = 2601;
constexpr int kButtonBase = 2610;
constexpr int kList = 2630;

unsigned ParseUnsigned(HWND edit, unsigned fallback) {
    const std::wstring value = GetControlText(edit);
    if (value.empty()) return fallback;
    wchar_t* end = nullptr;
    const unsigned long parsed = wcstoul(value.c_str(), &end, 10);
    if (!end || *end != L'\0' || parsed == 0 || parsed > 102400) return fallback;
    return static_cast<unsigned>(parsed);
}

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
    return p == r || (p.size() > r.size() && p.rfind(r + L"\\", 0) == 0);
}

std::wstring ReferenceSortKey(const fs::path& path) {
    const auto normalized = Lower(path.lexically_normal().wstring());
    wchar_t profileRaw[32768]{};
    DWORD size = static_cast<DWORD>(std::size(profileRaw));
    std::wstring profile;
    if (GetEnvironmentVariableW(L"USERPROFILE", profileRaw, size)) profile = Lower(profileRaw);
    const bool userPath = !profile.empty() && normalized.rfind(profile, 0) == 0;
    // Prefer a user-owned, shorter path as the reference copy; lexicographic order makes it deterministic.
    return std::wstring(userPath ? L"0|" : L"1|") + std::to_wstring(normalized.size()) + L"|" + normalized;
}
}

bool DuplicatesPage::OnCreate() {
    if (!fonts_.Create()) return false;
    settings_ = dpop::full::LoadSettings();
    path_ = CreateDarkEdit(Hwnd(), kPath, L"");
    threshold_ = CreateDarkEdit(Hwnd(), kThreshold, std::to_wstring(settings_.duplicateMinMB));
    if (!path_ || !threshold_) return false;
    ApplyControlFont(path_, fonts_.body);
    ApplyControlFont(threshold_, fonts_.body);
    SendMessageW(path_, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Папка для поиска дубликатов…"));

    const std::array<std::wstring_view, 6> labels = {
        L"Выбрать папку", L"Сканировать", L"Стоп", L"Открыть", L"Безопасные копии", L"В Корзину"
    };
    for (std::size_t i = 0; i < labels.size(); ++i) {
        const ButtonVisual visual = i == 1 || i == 4 ? ButtonVisual::Accent : i == 5 ? ButtonVisual::Danger : ButtonVisual::Normal;
        buttons_[i] = CreatePushButton(Hwnd(), kButtonBase + static_cast<int>(i), labels[i], visual);
        if (!buttons_[i]) return false;
        ApplyControlFont(buttons_[i], fonts_.smallFont);
    }

    list_ = CreateDarkListView(Hwnd(), kList);
    if (!list_) return false;
    ApplyControlFont(list_, fonts_.smallFont);
    ResetList(list_, true);
    AddListColumn(list_, 0, L"Группа", 75);
    AddListColumn(list_, 1, L"Роль", 190);
    AddListColumn(list_, 2, L"Размер", 105);
    AddListColumn(list_, 3, L"SHA-256", 235);
    AddListColumn(list_, 4, L"Безопасность", 205);
    AddListColumn(list_, 5, L"Путь", 520);

    SHFILEINFOW sfi{};
    systemImages_ = reinterpret_cast<HIMAGELIST>(SHGetFileInfoW(
        L"C:\\", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
        SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES));
    if (systemImages_) ListView_SetImageList(list_, systemImages_, LVSIL_SMALL);
    return true;
}

void DuplicatesPage::OnVisibilityChanged(bool visible) noexcept {
    if (!visible) Cancel();
}

bool DuplicatesPage::IsProtectedPath(const fs::path& path) const {
    wchar_t windows[MAX_PATH]{};
    GetWindowsDirectoryW(windows, static_cast<UINT>(std::size(windows)));
    const std::array<fs::path, 4> roots = {
        fs::path(windows), fs::path(Env(L"ProgramFiles")), fs::path(Env(L"ProgramFiles(x86)")), fs::path(Env(L"ProgramData"))
    };
    for (const auto& root : roots) if (!root.empty() && Under(path, root)) return true;
    return dpop::full::IsPathExcluded(path, settings_);
}

int DuplicatesPage::IconIndexFor(const fs::path& path) {
    SHFILEINFOW sfi{};
    std::error_code ec;
    UINT flags = SHGFI_SYSICONINDEX | SHGFI_SMALLICON;
    if (!fs::exists(path, ec)) flags |= SHGFI_USEFILEATTRIBUTES;
    if (!SHGetFileInfoW(path.c_str(), FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), flags)) return 0;
    return sfi.iIcon;
}

void DuplicatesPage::OnLayout(int width, int height) noexcept {
    const UINT dpiRaw = GetDpiForWindow(Hwnd());
    const int top = ComputePageContentTop(dpiRaw ? static_cast<int>(dpiRaw) : 96);
    const int margin = 18;
    const int y = top + 28;
    const int h = 30;
    const int gap = 6;
    const int widths[6] = {88, 82, 58, 72, 115, 92};
    const int fixed = widths[0] + widths[1] + widths[2] + widths[3] + widths[4] + widths[5] + 64 + gap * 7;
    const int pathW = std::max(210, width - margin * 2 - 24 - fixed);
    int x = margin + 12;
    MoveWindow(buttons_[0], x, y, widths[0], h, TRUE); x += widths[0] + gap;
    MoveWindow(path_, x, y, pathW, h, TRUE); x += pathW + gap;
    MoveWindow(threshold_, x, y, 64, h, TRUE); x += 64 + gap;
    for (int i = 1; i < 6; ++i) {
        MoveWindow(buttons_[static_cast<std::size_t>(i)], x, y, widths[i], h, TRUE);
        x += widths[i] + gap;
    }
    const int listY = top + 70;
    MoveWindow(list_, margin + 10, listY, std::max(120, width - margin * 2 - 20), std::max(130, height - margin - listY), TRUE);
}

void DuplicatesPage::OnPaint(HDC dc, const RECT& client) noexcept {
    PageBase::OnPaint(dc, client);
    DrawPageHeading(dc, 18, 4, L"Дубликаты", L"Группа показывает одну «эталонную копию (оставить)» и связанные дубликаты. Это опорная копия, а не утверждение о том, какой файл был создан первым.", fonts_.title, fonts_.body);
    const UINT dpiRaw = GetDpiForWindow(Hwnd());
    const int top = ComputePageContentTop(dpiRaw ? static_cast<int>(dpiRaw) : 96);
    const int margin = 18;
    RECT toolbar{margin, top, client.right - margin, top + 74};
    RECT results{margin, top + 86, client.right - margin, client.bottom - margin};
    DrawPanel(dc, toolbar, true);
    DrawPanel(dc, results, false);
    DrawPanelTitle(dc, toolbar, L"Папка • минимальный размер (МБ) • безопасные действия", fonts_.section);
    DrawPanelTitle(dc, results, L"Эталон группы → дубликаты", fonts_.section);
    const auto& p = MidnightPalette();
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, p.muted);
    HGDIOBJ old = SelectObject(dc, fonts_.smallFont);
    RECT status{results.left + 12, results.top + 8, results.right - 12, results.top + 30};
    std::wstring text = summary_;
    DrawTextW(dc, text.data(), static_cast<int>(text.size()), &status, DT_RIGHT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(dc, old);
}

void DuplicatesPage::Scan() {
    fs::path root = GetControlText(path_);
    if (root.empty()) root = root_;
    std::error_code ec;
    if (root.empty() || !fs::exists(root, ec)) { SetStatus(L"Сначала выберите существующую папку."); return; }
    root_ = root;
    settings_ = dpop::full::LoadSettings();
    SetControlText(path_, root_.wstring());
    const unsigned mb = ParseUnsigned(threshold_, settings_.duplicateMinMB);
    SetControlText(threshold_, std::to_wstring(mb));
    const std::uint64_t minBytes = static_cast<std::uint64_t>(mb) * 1024ull * 1024ull;
    StartAsync(L"Ищем дубликаты: размер → SHA-256…", [this, root, minBytes](std::stop_token token) {
        auto files = dpop::full::FindDuplicates(root, minBytes, token);
        QueueApply([this, files = std::move(files)]() mutable {
            files_ = std::move(files);
            Populate();
            Log(EventLevel::Info, L"Поиск дубликатов завершён.");
        });
    });
}

void DuplicatesPage::Populate() {
    ListView_DeleteAllItems(list_);
    rows_.clear();
    std::map<unsigned, std::vector<std::size_t>> groups;
    for (std::size_t i = 0; i < files_.size(); ++i) groups[files_[i].group].push_back(i);

    unsigned protectedCount = 0;
    for (auto& [group, indices] : groups) {
        std::stable_sort(indices.begin(), indices.end(), [this](std::size_t a, std::size_t b) {
            return ReferenceSortKey(files_[a].path) < ReferenceSortKey(files_[b].path);
        });
        for (std::size_t pos = 0; pos < indices.size(); ++pos) {
            const std::size_t fileIndex = indices[pos];
            const auto& file = files_[fileIndex];
            const bool reference = pos == 0;
            const bool protectedPath = IsProtectedPath(file.path);
            if (protectedPath) ++protectedCount;
            RowMeta meta{fileIndex, reference, protectedPath, reference ? 0u : static_cast<unsigned>(pos)};
            rows_.push_back(meta);
            const std::wstring role = reference ? L"Эталон группы (оставить)" : L"Дубликат " + std::to_wstring(pos);
            const std::wstring safety = reference ? L"Защищён: опорная копия" : protectedPath ? L"Защищён: системный/исключение" : L"Можно выбрать после проверки";
            const int row = AddListRow(list_, {
                std::to_wstring(group), role, dpop::full::FormatBytes(file.size), file.sha256, safety, file.path.wstring()
            });
            if (row >= 0) {
                ListView_SetCheckState(list_, row, FALSE);
                if (systemImages_) { LVITEMW item{}; item.mask = LVIF_IMAGE; item.iItem = row; item.iImage = IconIndexFor(file.path); ListView_SetItem(list_, &item); }
            }
        }
    }
    summary_ = L"Групп: " + std::to_wstring(groups.size()) + L" • файлов: " + std::to_wstring(files_.size()) +
        L" • защищено: " + std::to_wstring(protectedCount) + L" • эталоны никогда не отмечаются автоматически";
    SetStatus(summary_);
    InvalidateRect(Hwnd(), nullptr, TRUE);
}

void DuplicatesPage::SelectSafeDuplicates() {
    unsigned selected = 0;
    for (std::size_t row = 0; row < rows_.size(); ++row) {
        const auto& meta = rows_[row];
        const bool safe = !meta.referenceCopy && !meta.protectedPath;
        ListView_SetCheckState(list_, static_cast<int>(row), safe ? TRUE : FALSE);
        if (safe) ++selected;
    }
    SetStatus(L"Отмечено безопасных копий: " + std::to_wstring(selected) + L". Эталоны и системные/исключённые пути не отмечены.");
}

bool DuplicatesPage::LeavesOneCopyPerGroup(const std::vector<int>& checked) const {
    std::map<unsigned, unsigned> total;
    std::map<unsigned, unsigned> removed;
    for (const auto& file : files_) ++total[file.group];
    for (int row : checked) {
        if (row < 0 || row >= static_cast<int>(rows_.size())) continue;
        const auto& meta = rows_[static_cast<std::size_t>(row)];
        ++removed[files_[meta.fileIndex].group];
    }
    for (const auto& [group, count] : total) if (removed[group] >= count) return false;
    return true;
}

void DuplicatesPage::RecycleChecked() {
    const auto checked = CheckedListIndices(list_);
    if (checked.empty()) { SetStatus(L"Отметьте дубликаты для перемещения в Корзину."); return; }
    if (!LeavesOneCopyPerGroup(checked)) { SetStatus(L"В каждой группе должна остаться минимум одна копия."); return; }
    std::vector<fs::path> paths;
    for (int row : checked) {
        if (row < 0 || row >= static_cast<int>(rows_.size())) continue;
        const auto& meta = rows_[static_cast<std::size_t>(row)];
        if (meta.referenceCopy) {
            ListView_SetCheckState(list_, row, FALSE);
            SetStatus(L"Эталон группы нельзя отправить в Корзину через DPopCleaner.");
            continue;
        }
        if (meta.protectedPath) {
            ListView_SetCheckState(list_, row, FALSE);
            SetStatus(L"Системный/исключённый путь снят с отметки и не будет удалён.");
            continue;
        }
        paths.push_back(files_[meta.fileIndex].path);
    }
    if (paths.empty()) { SetStatus(L"После проверки безопасности не осталось файлов для удаления."); return; }
    if (!ConfirmAction(Hwnd(), L"Переместить отмеченные безопасные дубликаты в Корзину? Эталон каждой группы останется на месте.", true)) return;
    StartAsync(L"Перемещаем отмеченные копии в Корзину…", [this, paths = std::move(paths)](std::stop_token) {
        auto result = dpop::full::MoveToRecycleBin(paths);
        QueueApply([this, result = std::move(result)] {
            summary_ = L"В Корзину: " + std::to_wstring(result.moved) + L" • ошибок: " + std::to_wstring(result.failed);
            SetStatus(summary_);
            Log(result.failed ? EventLevel::Warning : EventLevel::Info, L"Безопасное удаление выбранных дубликатов завершено.");
            Scan();
        });
    });
}

void DuplicatesPage::OpenSelected() {
    const int row = SelectedListIndex(list_);
    if (row < 0 || row >= static_cast<int>(rows_.size())) { SetStatus(L"Выберите файл в таблице."); return; }
    OpenPathInExplorer(Hwnd(), files_[rows_[static_cast<std::size_t>(row)].fileIndex].path, true);
}

LRESULT DuplicatesPage::OnMessage(UINT message, WPARAM wParam, LPARAM lParam, bool& handled) {
    if (message == WM_COMMAND) {
        const int id = LOWORD(wParam);
        if (id == kButtonBase + 0) { const auto folder = ChooseFolder(Hwnd(), L"Выберите папку для поиска дубликатов"); if (!folder.empty()) { root_ = folder; SetControlText(path_, folder.wstring()); } handled = true; return 0; }
        if (id == kButtonBase + 1) { Scan(); handled = true; return 0; }
        if (id == kButtonBase + 2) { Cancel(); SetStatus(L"Запрошена остановка поиска дубликатов."); handled = true; return 0; }
        if (id == kButtonBase + 3) { OpenSelected(); handled = true; return 0; }
        if (id == kButtonBase + 4) { SelectSafeDuplicates(); handled = true; return 0; }
        if (id == kButtonBase + 5) { RecycleChecked(); handled = true; return 0; }
    }
    if (message == WM_NOTIFY) {
        const auto* hdr = reinterpret_cast<const NMHDR*>(lParam);
        if (hdr && hdr->hwndFrom == list_ && hdr->code == NM_DBLCLK) { OpenSelected(); handled = true; return 0; }
    }
    if (message == WM_DRAWITEM) {
        const auto* draw = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (draw && draw->CtlType == ODT_BUTTON && draw->CtlID >= kButtonBase && draw->CtlID < kButtonBase + 6) {
            const ButtonVisual visual = (draw->CtlID == kButtonBase + 1 || draw->CtlID == kButtonBase + 4) ? ButtonVisual::Accent :
                draw->CtlID == kButtonBase + 5 ? ButtonVisual::Danger : ButtonVisual::Normal;
            handled = DrawOwnerButton(*draw, GetControlText(draw->hwndItem), visual);
            return handled ? TRUE : 0;
        }
    }
    handled = false;
    return 0;
}

}
