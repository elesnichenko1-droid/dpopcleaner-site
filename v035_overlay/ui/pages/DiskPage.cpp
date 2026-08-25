#include "ui/pages/DiskPage.h"

#include "ui/Controls.h"
#include "ui/PageLayout.h"
#include "ui/Theme.h"

#include <shellapi.h>

#include <algorithm>
#include <array>
#include <cwctype>
#include <iomanip>
#include <sstream>

namespace dpop::ui {
namespace {
namespace fs = std::filesystem;

constexpr int kToolbarBase = 3300;
constexpr int kPath = 3320;
constexpr int kTree = 3321;

std::wstring Lower(std::wstring value) {
    std::replace(value.begin(), value.end(), L'/', L'\\');
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(std::towlower(c));
    });
    while (value.size() > 3 && !value.empty() && value.back() == L'\\') value.pop_back();
    return value;
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

std::wstring CountText(std::uint64_t value) {
    return std::to_wstring(value);
}

std::wstring AllocatedText(const dpop::disk::DiskNode& node) {
    return node.allocatedComplete ? dpop::full::FormatBytes(node.allocatedBytes) : L"—";
}

bool LessNoCase(const std::wstring& a, const std::wstring& b) {
    return Lower(a) < Lower(b);
}
} // namespace

bool DiskPage::OnCreate() {
    if (!fonts_.Create()) return false;
    settings_ = dpop::full::LoadSettings();

    const std::array<std::wstring_view, 8> labels = {
        L"Назад", L"C:\\", L"Выбрать каталог", L"Сканировать",
        L"Стоп", L"Обновить", L"Крупные файлы", L"Проводник"
    };
    for (std::size_t i = 0; i < labels.size(); ++i) {
        const ButtonVisual visual = (i == 3) ? ButtonVisual::Accent : ButtonVisual::Normal;
        toolbar_[i] = CreatePushButton(Hwnd(), kToolbarBase + static_cast<int>(i), labels[i], visual);
        if (!toolbar_[i]) return false;
        ApplyControlFont(toolbar_[i], fonts_.smallFont);
    }

    pathEdit_ = CreateDarkEdit(Hwnd(), kPath, root_.wstring(), false);
    if (!pathEdit_ || !tree_.Create(Hwnd(), kTree, fonts_.smallFont)) return false;
    ApplyControlFont(pathEdit_, fonts_.body);
    EnableWindow(toolbar_[4], FALSE);
    summary_ = L"Выберите каталог или нажмите «C:\\», затем запустите анализ.";
    return true;
}

std::wstring DiskPage::CacheKey(const fs::path& root) const {
    return Lower(root.lexically_normal().wstring());
}

void DiskPage::StartScan(const fs::path& root, bool addHistory) {
    std::error_code ec;
    if (root.empty() || !fs::exists(root, ec) || ec || !fs::is_directory(root, ec) || ec) {
        SetStatus(L"Каталог не существует или недоступен.");
        return;
    }

    Cancel();
    const std::uint64_t generation = ++scanGeneration_;
    root_ = root.lexically_normal();
    SetControlText(pathEdit_, root_.wstring());
    largeMode_ = false;
    scanning_ = true;
    snapshot_ = {};
    expanded_.clear();
    summary_ = L"Сканирование: " + root_.wstring();
    EnableWindow(toolbar_[3], FALSE);
    EnableWindow(toolbar_[4], TRUE);

    if (addHistory) {
        if (historyIndex_ + 1 < static_cast<int>(history_.size())) {
            history_.erase(history_.begin() + historyIndex_ + 1, history_.end());
        }
        if (history_.empty() || CacheKey(history_.back()) != CacheKey(root_)) history_.push_back(root_);
        historyIndex_ = static_cast<int>(history_.size()) - 1;
    }
    InvalidateRect(Hwnd(), nullptr, TRUE);

    const fs::path scanRoot = root_;
    StartAsync(L"Анализируем занятое место…", [this, scanRoot, generation](std::stop_token token) {
        dpop::disk::DiskScanOptions options{};
        options.progressEveryEntries = 192;
        options.includeFilesAsNodes = true;
        options.emitTopLevelSnapshots = true;
        options.partialSnapshot = [this, generation](const dpop::disk::DiskScanSnapshot& partial) {
            dpop::disk::DiskScanSnapshot copy = partial;
            QueueApply([this, generation, copy = std::move(copy)]() mutable {
                if (generation != scanGeneration_) return;
                ApplySnapshot(std::move(copy), false);
            });
        };

        auto result = dpop::disk::ScanDiskTree(
            scanRoot,
            token,
            [this, generation](const dpop::disk::DiskScanProgress& p) {
                QueueApply([this, generation, p] {
                    if (generation != scanGeneration_) return;
                    summary_ = L"Файлов: " + std::to_wstring(p.filesVisited) +
                               L" • папок: " + std::to_wstring(p.directoriesVisited) +
                               L" • найдено: " + dpop::full::FormatBytes(p.logicalBytes) +
                               L" • ошибок доступа: " + std::to_wstring(p.errors);
                    SetStatus(summary_);
                    InvalidateRect(Hwnd(), nullptr, TRUE);
                });
            },
            std::move(options));

        QueueApply([this, generation, result = std::move(result)]() mutable {
            if (generation != scanGeneration_) return;
            ApplySnapshot(std::move(result), true);
        });
    });
}

void DiskPage::ApplySnapshot(dpop::disk::DiskScanSnapshot snapshot, bool finalSnapshot) {
    snapshot_ = std::move(snapshot);
    if (snapshot_.rootId) expanded_.insert(snapshot_.rootId);
    largeMode_ = false;
    BuildVisibleRows();

    const auto* rootNode = snapshot_.Find(snapshot_.rootId);
    if (rootNode) {
        summary_ = dpop::full::FormatBytes(rootNode->logicalBytes) + L" • занято " +
                   AllocatedText(*rootNode) + L" • файлов " +
                   std::to_wstring(rootNode->fileCount) + L" • папок " +
                   std::to_wstring(rootNode->directoryCount) + L" • ошибок " +
                   std::to_wstring(snapshot_.errorCount);
    }

    if (finalSnapshot) {
        scanning_ = false;
        EnableWindow(toolbar_[3], TRUE);
        EnableWindow(toolbar_[4], FALSE);
        if (snapshot_.complete) cache_[CacheKey(root_)] = snapshot_;
        if (snapshot_.cancelled) summary_ = L"Сканирование остановлено. Частичные результаты сохранены в окне. " + summary_;
        else if (!snapshot_.complete) summary_ = L"Сканирование завершилось частично. " + summary_;
        SetStatus(summary_);
        Log(EventLevel::Info, snapshot_.cancelled ? L"Анализ диска остановлен пользователем." : L"Анализ диска завершён.");
    }
    InvalidateRect(Hwnd(), nullptr, TRUE);
}

std::vector<dpop::disk::DiskNodeId> DiskPage::SortedChildren(const dpop::disk::DiskNode& parent) const {
    std::vector<dpop::disk::DiskNodeId> ids = parent.children;
    auto compare = [&](dpop::disk::DiskNodeId lhsId, dpop::disk::DiskNodeId rhsId) {
        const auto* a = snapshot_.Find(lhsId);
        const auto* b = snapshot_.Find(rhsId);
        if (!a || !b) return lhsId < rhsId;
        if (a->directory != b->directory) return a->directory > b->directory;
        if (sortColumn_ == 2 && a->allocatedComplete != b->allocatedComplete) {
            return a->allocatedComplete;
        }

        int cmp = 0;
        switch (sortColumn_) {
            case 0: cmp = LessNoCase(a->displayName, b->displayName) ? -1 : (LessNoCase(b->displayName, a->displayName) ? 1 : 0); break;
            case 1: cmp = a->logicalBytes < b->logicalBytes ? -1 : (a->logicalBytes > b->logicalBytes ? 1 : 0); break;
            case 2: cmp = a->allocatedBytes < b->allocatedBytes ? -1 : (a->allocatedBytes > b->allocatedBytes ? 1 : 0); break;
            case 3: cmp = a->fileCount < b->fileCount ? -1 : (a->fileCount > b->fileCount ? 1 : 0); break;
            case 4: cmp = a->directoryCount < b->directoryCount ? -1 : (a->directoryCount > b->directoryCount ? 1 : 0); break;
            case 5: {
                const double ap = dpop::disk::ParentPercent(*a, &parent);
                const double bp = dpop::disk::ParentPercent(*b, &parent);
                cmp = ap < bp ? -1 : (ap > bp ? 1 : 0);
                break;
            }
            case 6: cmp = a->modifiedUnix100ns < b->modifiedUnix100ns ? -1 : (a->modifiedUnix100ns > b->modifiedUnix100ns ? 1 : 0); break;
            default: break;
        }
        if (cmp == 0) cmp = LessNoCase(a->displayName, b->displayName) ? -1 : (LessNoCase(b->displayName, a->displayName) ? 1 : 0);
        return sortAscending_ ? cmp < 0 : cmp > 0;
    };
    std::stable_sort(ids.begin(), ids.end(), compare);
    return ids;
}

void DiskPage::AppendVisibleNode(dpop::disk::DiskNodeId id, unsigned depth, std::vector<DiskTreeRow>& rows) const {
    const auto* node = snapshot_.Find(id);
    if (!node) return;
    const auto* parent = node->parentId ? snapshot_.Find(node->parentId) : nullptr;

    DiskTreeRow row{};
    row.nodeId = node->id;
    row.depth = depth;
    row.directory = node->directory;
    row.hasChildren = !node->children.empty();
    row.expanded = expanded_.contains(node->id);
    row.protectedPath = node->protectedPath;
    row.incomplete = node->incomplete;
    row.name = node->displayName;
    row.sizeText = dpop::full::FormatBytes(node->logicalBytes);
    row.allocatedText = AllocatedText(*node);
    row.filesText = node->directory ? CountText(node->fileCount) : L"1";
    row.dirsText = node->directory ? CountText(node->directoryCount) : L"";
    row.parentPercent = dpop::disk::ParentPercent(*node, parent);
    row.modifiedText = ModifiedText(node->path);
    rows.push_back(std::move(row));

    if (!node->directory || !expanded_.contains(node->id)) return;
    for (const auto childId : SortedChildren(*node)) AppendVisibleNode(childId, depth + 1, rows);
}

void DiskPage::BuildVisibleRows() {
    if (largeMode_) {
        ShowLargeFiles();
        return;
    }
    std::vector<DiskTreeRow> rows;
    if (snapshot_.rootId) AppendVisibleNode(snapshot_.rootId, 0, rows);
    tree_.SetRows(std::move(rows));
}

void DiskPage::ToggleExpanded(dpop::disk::DiskNodeId id) {
    const auto* node = snapshot_.Find(id);
    if (!node || !node->directory || node->children.empty()) return;
    if (expanded_.contains(id)) expanded_.erase(id);
    else expanded_.insert(id);
    largeMode_ = false;
    BuildVisibleRows();
}

void DiskPage::ShowLargeFiles() {
    if (!snapshot_.rootId) {
        SetStatus(L"Сначала выполните анализ диска.");
        return;
    }
    largeMode_ = true;
    const std::uint64_t threshold = static_cast<std::uint64_t>(settings_.largeFileMB) * 1024ull * 1024ull;
    const auto* rootNode = snapshot_.Find(snapshot_.rootId);
    std::vector<const dpop::disk::DiskNode*> files;
    for (const auto& node : snapshot_.nodes) {
        if (!node.directory && node.logicalBytes >= threshold) files.push_back(&node);
    }
    std::stable_sort(files.begin(), files.end(), [](const auto* a, const auto* b) {
        if (a->logicalBytes != b->logicalBytes) return a->logicalBytes > b->logicalBytes;
        return Lower(a->path.wstring()) < Lower(b->path.wstring());
    });

    std::vector<DiskTreeRow> rows;
    rows.reserve(files.size());
    for (const auto* node : files) {
        DiskTreeRow row{};
        row.nodeId = node->id;
        row.name = node->path.wstring();
        row.sizeText = dpop::full::FormatBytes(node->logicalBytes);
        row.allocatedText = AllocatedText(*node);
        row.filesText = L"1";
        row.parentPercent = rootNode ? dpop::disk::ParentPercent(*node, rootNode) : 0.0;
        row.modifiedText = ModifiedText(node->path);
        row.protectedPath = node->protectedPath;
        rows.push_back(std::move(row));
    }
    tree_.SetRows(std::move(rows));
    summary_ = L"Крупные файлы ≥ " + std::to_wstring(settings_.largeFileMB) + L" МБ: " + std::to_wstring(files.size());
    SetStatus(summary_);
    InvalidateRect(Hwnd(), nullptr, TRUE);
}

void DiskPage::ShowTree() {
    largeMode_ = false;
    BuildVisibleRows();
    SetStatus(summary_);
    InvalidateRect(Hwnd(), nullptr, TRUE);
}

void DiskPage::ChooseRoot() {
    const auto folder = ChooseFolder(Hwnd(), L"Выберите каталог для анализа занятого места");
    if (!folder.empty()) StartScan(folder, true);
}

void DiskPage::GoBack() {
    if (historyIndex_ <= 0 || history_.empty()) {
        SetStatus(L"Предыдущего каталога нет.");
        return;
    }
    --historyIndex_;
    root_ = history_[static_cast<std::size_t>(historyIndex_)];
    SetControlText(pathEdit_, root_.wstring());
    const auto found = cache_.find(CacheKey(root_));
    if (found != cache_.end()) {
        Cancel();
        ++scanGeneration_;
        snapshot_ = found->second;
        expanded_.clear();
        if (snapshot_.rootId) expanded_.insert(snapshot_.rootId);
        largeMode_ = false;
        scanning_ = false;
        BuildVisibleRows();
        summary_ = L"Кэшированный анализ: " + root_.wstring();
        SetStatus(summary_);
        InvalidateRect(Hwnd(), nullptr, TRUE);
    } else {
        StartScan(root_, false);
    }
}

void DiskPage::OpenExplorer() {
    const auto* selected = tree_.SelectedRow();
    const auto* node = selected ? snapshot_.Find(selected->nodeId) : nullptr;
    if (node) {
        OpenPathInExplorer(Hwnd(), node->path, !node->directory);
        SetStatus(node->directory ? L"Папка открыта в Проводнике." : L"Файл выделен в Проводнике.");
        return;
    }
    OpenPathInExplorer(Hwnd(), root_);
    SetStatus(L"Текущий каталог открыт в Проводнике.");
}

void DiskPage::HandleColumnClick(int column) {
    if (column < 0 || column > 6) return;
    if (sortColumn_ == column) sortAscending_ = !sortAscending_;
    else {
        sortColumn_ = column;
        sortAscending_ = (column == 0 || column == 6);
    }
    BuildVisibleRows();
}

void DiskPage::OnVisibilityChanged(bool visible) noexcept {
    if (!visible) {
        Cancel();
        ++scanGeneration_;
        scanning_ = false;
        EnableWindow(toolbar_[3], TRUE);
        EnableWindow(toolbar_[4], FALSE);
    }
}

void DiskPage::OnLayout(int width, int height) noexcept {
    const UINT dpiRaw = GetDpiForWindow(Hwnd());
    const int top = ComputePageContentTop(dpiRaw ? static_cast<int>(dpiRaw) : 96);
    const int margin = 18;
    const int y = top + 30;
    const int h = 30;
    const int gap = 6;
    const int widths[8] = {64, 48, 110, 92, 58, 78, 110, 88};
    const int fixed = widths[0] + widths[1] + widths[2] + widths[3] + widths[4] + widths[5] + widths[6] + widths[7] + gap * 8;
    const int pathW = std::max(160, width - margin * 2 - 24 - fixed);
    int x = margin + 12;

    for (int i = 0; i < 3; ++i) {
        MoveWindow(toolbar_[static_cast<std::size_t>(i)], x, y, widths[i], h, TRUE);
        x += widths[i] + gap;
    }
    MoveWindow(pathEdit_, x, y, pathW, h, TRUE);
    x += pathW + gap;
    for (int i = 3; i < 8; ++i) {
        MoveWindow(toolbar_[static_cast<std::size_t>(i)], x, y, widths[i], h, TRUE);
        x += widths[i] + gap;
    }

    const int listY = top + 78;
    MoveWindow(tree_.Hwnd(), margin + 10, listY,
               std::max(120, width - margin * 2 - 20),
               std::max(120, height - margin - listY), TRUE);
}

void DiskPage::OnPaint(HDC dc, const RECT& client) noexcept {
    PageBase::OnPaint(dc, client);
    DrawPageHeading(dc, 18, 4, L"Диск",
                    L"TreeSize-подобный анализ: реальный размер папок, занятое место и доля относительно родителя.",
                    fonts_.title, fonts_.body);

    const UINT dpiRaw = GetDpiForWindow(Hwnd());
    const int top = ComputePageContentTop(dpiRaw ? static_cast<int>(dpiRaw) : 96);
    const int margin = 18;
    RECT toolbar{margin, top, client.right - margin, top + 72};
    RECT table{margin, top + 84, client.right - margin, client.bottom - margin};
    DrawPanel(dc, toolbar, true);
    DrawPanel(dc, table, false);
    DrawPanelTitle(dc, toolbar, L"Корень и сканирование", fonts_.section);
    DrawPanelTitle(dc, table, largeMode_ ? L"Крупные файлы" : L"Размеры каталогов", fonts_.section);

    const auto& palette = MidnightPalette();
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, palette.muted);
    HGDIOBJ old = SelectObject(dc, fonts_.smallFont);
    RECT status{table.left + 12, table.top + 8, table.right - 12, table.top + 28};
    std::wstring text = summary_;
    DrawTextW(dc, text.data(), static_cast<int>(text.size()), &status,
              DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(dc, old);
}

LRESULT DiskPage::OnMessage(UINT message, WPARAM wParam, LPARAM lParam, bool& handled) {
    if (message == WM_COMMAND) {
        const int id = LOWORD(wParam);
        if (id == kToolbarBase + 0) { GoBack(); handled = true; return 0; }
        if (id == kToolbarBase + 1) { StartScan(L"C:\\", true); handled = true; return 0; }
        if (id == kToolbarBase + 2) { ChooseRoot(); handled = true; return 0; }
        if (id == kToolbarBase + 3) {
            const std::wstring typed = GetControlText(pathEdit_);
            StartScan(typed.empty() ? root_ : fs::path(typed), true);
            handled = true; return 0;
        }
        if (id == kToolbarBase + 4) {
            if (scanning_) {
                Cancel();
                summary_ = L"Останавливаем сканирование…";
                SetStatus(summary_);
            }
            handled = true; return 0;
        }
        if (id == kToolbarBase + 5) { StartScan(root_, false); handled = true; return 0; }
        if (id == kToolbarBase + 6) {
            if (largeMode_) ShowTree(); else ShowLargeFiles();
            handled = true; return 0;
        }
        if (id == kToolbarBase + 7) { OpenExplorer(); handled = true; return 0; }
    }

    if (message == WM_NOTIFY) {
        auto* hdr = reinterpret_cast<NMHDR*>(lParam);
        if (hdr && hdr->hwndFrom == tree_.Hwnd()) {
            if (hdr->code == NM_CUSTOMDRAW) {
                LRESULT result = 0;
                if (tree_.HandleCustomDraw(*reinterpret_cast<NMLVCUSTOMDRAW*>(lParam), result)) {
                    handled = true;
                    return result;
                }
            }
            if (hdr->code == LVN_COLUMNCLICK) {
                auto* info = reinterpret_cast<NMLISTVIEW*>(lParam);
                HandleColumnClick(info->iSubItem);
                handled = true;
                return 0;
            }
            if (hdr->code == NM_CLICK) {
                auto* click = reinterpret_cast<NMITEMACTIVATE*>(lParam);
                const auto id = tree_.ExpanderNodeFromClick(*click);
                if (id) ToggleExpanded(id);
                handled = true;
                return 0;
            }
            if (hdr->code == NM_DBLCLK) {
                const auto* row = tree_.SelectedRow();
                const auto* node = row ? snapshot_.Find(row->nodeId) : nullptr;
                if (node && node->directory) ToggleExpanded(node->id);
                else if (node) OpenPathInExplorer(Hwnd(), node->path, true);
                handled = true;
                return 0;
            }
        }
    }

    return PageBase::OnMessage(message, wParam, lParam, handled);
}

} // namespace dpop::ui