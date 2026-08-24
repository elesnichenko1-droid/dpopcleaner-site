#include "ui/pages/GuardPage.h"

#include "modules/DPopGuard.h"
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
constexpr int kActionBase = 2200;
constexpr int kSummaryBase = 2220;
constexpr int kProgress = 2230;
constexpr int kList = 2240;

fs::path PathFromDetails(const std::wstring& details) {
    std::size_t start = details.find_last_of(L"\r\n");
    std::wstring candidate = start == std::wstring::npos ? details : details.substr(start + 1);
    while (!candidate.empty() && iswspace(candidate.front())) candidate.erase(candidate.begin());
    while (!candidate.empty() && iswspace(candidate.back())) candidate.pop_back();
    std::error_code ec;
    fs::path path(candidate);
    return fs::is_regular_file(path, ec) ? path : fs::path{};
}

bool IsDetection(const std::wstring& verdict) {
    return verdict.find(L"обнаружений нет") == std::wstring::npos &&
           verdict.find(L"Ошибка") == std::wstring::npos;
}

fs::path QuarantineDirectory() {
    const DWORD needed = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
    if (!needed) return {};
    std::wstring value(needed, L'\0');
    GetEnvironmentVariableW(L"LOCALAPPDATA", value.data(), needed);
    while (!value.empty() && value.back() == L'\0') value.pop_back();
    return fs::path(value) / L"DPopCleaner/Quarantine";
}
}

bool GuardPage::OnCreate() {
    if (!fonts_.Create()) return false;

    const std::array<std::wstring_view, 8> labels = {
        L"DPopGuard Quick Scan",
        L"Defender Quick Scan",
        L"Проверить файл",
        L"Проверить папку",
        L"В карантин",
        L"Открыть карантин",
        L"Windows Security",
        L"Отмена"
    };
    for (std::size_t i = 0; i < labels.size(); ++i) {
        ButtonVisual visual = i == 0 || i == 1 ? ButtonVisual::Accent : i == 4 ? ButtonVisual::Danger : ButtonVisual::Normal;
        actionButtons_[i] = CreatePushButton(Hwnd(), kActionBase + static_cast<int>(i), labels[i], visual);
        if (!actionButtons_[i]) return false;
        ApplyControlFont(actionButtons_[i], fonts_.smallFont);
    }

    for (std::size_t i = 0; i < summaryLabels_.size(); ++i) {
        summaryLabels_[i] = CreateTextLabel(Hwnd(), kSummaryBase + static_cast<int>(i), L"—");
        if (!summaryLabels_[i]) return false;
        ApplyControlFont(summaryLabels_[i], fonts_.body);
    }

    progress_ = CreateProgress(Hwnd(), kProgress);
    list_ = CreateDarkListView(Hwnd(), kList);
    if (!progress_ || !list_) return false;
    ApplyControlFont(list_, fonts_.smallFont);
    SendMessageW(progress_, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SetProgress(0);

    ResetList(list_);
    AddListColumn(list_, 0, L"Объект", 220);
    AddListColumn(list_, 1, L"Провайдер / вердикт", 260);
    AddListColumn(list_, 2, L"Уровень", 110);
    AddListColumn(list_, 3, L"Путь / детали", 500);
    AddListColumn(list_, 4, L"Действие", 130);
    SetSummary(0, 0, 0, 0);
    return true;
}

void GuardPage::SetSummary(unsigned processes, unsigned startup, unsigned files, unsigned findings) {
    SetControlText(summaryLabels_[0], L"Процессы: " + std::to_wstring(processes));
    SetControlText(summaryLabels_[1], L"Автозапуск: " + std::to_wstring(startup));
    SetControlText(summaryLabels_[2], L"Файлы: " + std::to_wstring(files));
    SetControlText(summaryLabels_[3], L"Находки: " + std::to_wstring(findings));
}

void GuardPage::SetProgress(unsigned value) noexcept {
    if (progress_) SendMessageW(progress_, PBM_SETPOS, static_cast<WPARAM>(std::min(100u, value)), 0);
}

void GuardPage::ClearResults() {
    ListView_DeleteAllItems(list_);
    rowPaths_.clear();
}

void GuardPage::OnLayout(int width, int height) noexcept {
    const UINT dpiRaw = GetDpiForWindow(Hwnd());
    const int top = ComputePageContentTop(dpiRaw ? static_cast<int>(dpiRaw) : 96);
    const int margin = 18;
    const int commandW = std::min(320, std::max(270, width * 30 / 100));
    const int gap = 12;
    const int summaryH = 70;
    const int contentBottom = height - margin;

    const int innerX = margin + 12;
    const int buttonGap = 7;
    const int buttonW = std::max(105, (commandW - 24 - buttonGap) / 2);
    const int buttonH = 31;
    int y = top + 34;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 2; ++col) {
            const int index = row * 2 + col;
            MoveWindow(actionButtons_[static_cast<std::size_t>(index)],
                innerX + col * (buttonW + buttonGap), y, buttonW, buttonH, TRUE);
        }
        y += buttonH + buttonGap;
    }
    MoveWindow(progress_, innerX, y + 4, commandW - 24, 16, TRUE);

    const int rightX = margin + commandW + gap;
    const int rightW = std::max(160, width - margin - rightX);
    const int cellW = std::max(80, (rightW - 28) / 4);
    for (int i = 0; i < 4; ++i) {
        MoveWindow(summaryLabels_[static_cast<std::size_t>(i)], rightX + 14 + i * cellW, top + 28, cellW - 10, 28, TRUE);
    }
    MoveWindow(list_, rightX + 10, top + summaryH + 12, std::max(120, rightW - 20),
        std::max(120, contentBottom - (top + summaryH + 22)), TRUE);
}

void GuardPage::OnPaint(HDC dc, const RECT& client) noexcept {
    PageBase::OnPaint(dc, client);
    DrawPageHeading(
        dc, 18, 4,
        L"DPopGuard",
        L"Реальные провайдеры: процессы/persistence/miner heuristics, Windows AMSI и Microsoft Defender. Действия и провайдер результата показаны явно.",
        fonts_.title, fonts_.body);

    const UINT dpiRaw = GetDpiForWindow(Hwnd());
    const int top = ComputePageContentTop(dpiRaw ? static_cast<int>(dpiRaw) : 96);
    const int width = static_cast<int>(client.right);
    const int height = static_cast<int>(client.bottom);
    const int margin = 18;
    const int commandW = std::min(320, std::max(270, width * 30 / 100));
    const int gap = 12;
    const int rightX = margin + commandW + gap;
    RECT commands{margin, top, margin + commandW, height - margin};
    RECT summary{rightX, top, width - margin, top + 70};
    RECT results{rightX, top + 82, width - margin, height - margin};
    DrawPanel(dc, commands, true);
    DrawPanel(dc, summary, false);
    DrawPanel(dc, results, false);
    DrawPanelTitle(dc, commands, L"Сканирование и защита", fonts_.section);
    DrawPanelTitle(dc, summary, L"Сводка", fonts_.section);
    DrawPanelTitle(dc, results, L"Результаты", fonts_.section);

    const auto& p = MidnightPalette();
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, p.muted);
    HGDIOBJ old = SelectObject(dc, fonts_.smallFont);
    RECT note{commands.left + 14, commands.top + 205, commands.right - 14, commands.bottom - 14};
    const auto defender = dpop::guard::QueryDefenderStatus();
    std::wstring text = modeStatus_ + L"\n\nMicrosoft Defender: ";
    text += defender.cliAvailable ? L"CLI найден" : L"CLI не найден";
    text += defender.serviceRunning ? L", служба активна." : L", служба не активна/недоступна.";
    text += L"\n\nDPopCleaner не подменяет Windows Security: системные обнаружения и историю угроз открывает штатный интерфейс Windows.";
    DrawTextW(dc, text.data(), static_cast<int>(text.size()), &note, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(dc, old);
}

void GuardPage::OnVisibilityChanged(bool visible) noexcept {
    if (!visible) Cancel();
}

void GuardPage::RunQuickScan() {
    SetProgress(10);
    StartAsync(L"DPopGuard проверяет процессы и persistence…", [this](std::stop_token) {
        auto result = dpop::guard::QuickScan();
        QueueApply([this, result = std::move(result)]() mutable {
            ClearResults();
            for (const auto& finding : result.findings) {
                const fs::path path = PathFromDetails(finding.details);
                AddListRow(list_, {finding.title, L"DPopGuard эвристика", finding.severity, finding.details,
                    path.empty() ? L"наблюдение" : L"можно карантин"});
                rowPaths_.push_back(path);
            }
            const auto defender = dpop::guard::QueryDefenderStatus();
            AddListRow(list_, {L"Microsoft Defender", defender.cliAvailable ? L"Провайдер доступен" : L"Провайдер недоступен",
                defender.serviceRunning ? L"Активен" : L"Проверить", defender.cliPath.wstring(), L"информация"});
            rowPaths_.push_back({});
            SetSummary(result.processesChecked, result.startupChecked, 0, static_cast<unsigned>(result.findings.size()));
            modeStatus_ = L"DPopGuard Quick Scan: процессов " + std::to_wstring(result.processesChecked) +
                L", автозапусков " + std::to_wstring(result.startupChecked) + L", эвристических находок " +
                std::to_wstring(result.findings.size()) + L".";
            SetProgress(100);
            SetStatus(modeStatus_);
            Log(EventLevel::Info, L"DPopGuard Quick Scan завершён.");
            InvalidateRect(Hwnd(), nullptr, TRUE);
        });
    });
}

void GuardPage::RunDefenderQuickScan() {
    if (!ConfirmAction(Hwnd(), L"Запустить штатную быструю проверку Microsoft Defender? Это системная проверка Windows и может занять несколько минут.", false)) return;
    SetProgress(10);
    StartAsync(L"Microsoft Defender выполняет Quick Scan…", [this](std::stop_token) {
        auto result = dpop::guard::RunDefenderQuickScan();
        QueueApply([this, result = std::move(result)]() mutable {
            ClearResults();
            AddListRow(list_, {L"Microsoft Defender Quick Scan", result.started ? L"Microsoft Defender" : L"Ошибка запуска",
                result.completed ? L"Завершено" : result.started ? L"Выполняется/долго" : L"Недоступен",
                result.message, L"Protection history"});
            rowPaths_.push_back({});
            SetSummary(0, 0, 0, 0);
            modeStatus_ = result.message;
            SetProgress(result.completed ? 100 : result.started ? 65 : 0);
            SetStatus(modeStatus_);
            Log(result.started ? EventLevel::Info : EventLevel::Warning, result.message);
            InvalidateRect(Hwnd(), nullptr, TRUE);
        });
    });
}

void GuardPage::ScanFile() {
    const auto file = ChooseFile(Hwnd(), L"Выберите файл для проверки AMSI + Microsoft Defender");
    if (file.empty()) return;
    SetProgress(10);
    StartAsync(L"Проверяем файл через AMSI и Microsoft Defender…", [this, file](std::stop_token) {
        std::wstring verdict, amsiError;
        const bool amsiOk = dpop::guard::ScanFileWithAmsi(file, verdict, amsiError);
        auto defender = dpop::guard::RunDefenderCustomScan(file);
        QueueApply([this, file, amsiOk, verdict = std::move(verdict), amsiError = std::move(amsiError), defender = std::move(defender)]() mutable {
            ClearResults();
            const std::wstring amsiText = amsiOk ? verdict : L"AMSI: " + amsiError;
            AddListRow(list_, {file.filename().wstring(), L"Windows AMSI", amsiOk && IsDetection(verdict) ? L"Проверить" : amsiOk ? L"Чисто" : L"Ошибка",
                amsiText, L"файл"});
            rowPaths_.push_back(file);
            AddListRow(list_, {file.filename().wstring(), L"Microsoft Defender",
                defender.completed ? L"Завершено" : defender.started ? L"Запущено" : L"Недоступен",
                defender.message, L"Protection history"});
            rowPaths_.push_back(file);
            const unsigned findings = amsiOk && IsDetection(verdict) ? 1u : 0u;
            SetSummary(0, 0, 1, findings);
            modeStatus_ = L"Файл проверен двумя доступными Windows-провайдерами. " + defender.message;
            SetProgress(100);
            SetStatus(modeStatus_);
            Log(EventLevel::Info, L"Проверка файла AMSI/Defender завершена.");
            InvalidateRect(Hwnd(), nullptr, TRUE);
        });
    });
}

void GuardPage::ScanFolder() {
    const auto folder = ChooseFolder(Hwnd(), L"Выберите папку для проверки");
    if (folder.empty()) return;
    SetProgress(10);
    StartAsync(L"AMSI и Microsoft Defender проверяют папку…", [this, folder](std::stop_token token) {
        auto result = dpop::full::ScanFolderWithAmsi(folder, token, 20000);
        dpop::guard::DefenderScanResult defender{};
        if (!token.stop_requested()) defender = dpop::guard::RunDefenderCustomScan(folder);
        QueueApply([this, folder, result = std::move(result), defender = std::move(defender)]() mutable {
            ClearResults();
            for (const auto& hit : result.hits) {
                AddListRow(list_, {hit.path.filename().wstring(), L"Windows AMSI", L"Проверить", hit.verdict + L"\n" + hit.path.wstring(), L"можно карантин"});
                rowPaths_.push_back(hit.path);
            }
            AddListRow(list_, {folder.filename().wstring(), L"Microsoft Defender",
                defender.completed ? L"Завершено" : defender.started ? L"Запущено" : L"Недоступен",
                defender.message, L"Protection history"});
            rowPaths_.push_back({});
            SetSummary(0, 0, result.checked, static_cast<unsigned>(result.hits.size()));
            modeStatus_ = L"AMSI: проверено " + std::to_wstring(result.checked) + L", пропущено " +
                std::to_wstring(result.skipped) + L", ошибок " + std::to_wstring(result.errors) + L", находок " +
                std::to_wstring(result.hits.size()) + L". " + defender.message;
            SetProgress(100);
            SetStatus(modeStatus_);
            Log(result.errors ? EventLevel::Warning : EventLevel::Info, L"Проверка папки AMSI/Defender завершена.");
            InvalidateRect(Hwnd(), nullptr, TRUE);
        });
    });
}

void GuardPage::QuarantineSelected() {
    const int index = SelectedListIndex(list_);
    if (index < 0 || index >= static_cast<int>(rowPaths_.size()) || rowPaths_[static_cast<std::size_t>(index)].empty()) {
        SetStatus(L"Выберите строку с реальным файлом.");
        return;
    }
    const fs::path file = rowPaths_[static_cast<std::size_t>(index)];
    if (!ConfirmAction(Hwnd(), L"Переместить выбранный файл в локальный карантин DPopCleaner? Исходный путь будет записан рядом с файлом карантина.", true)) return;
    fs::path destination;
    std::wstring error;
    if (dpop::full::QuarantineFile(file, destination, error)) {
        modeStatus_ = L"Файл перемещён в карантин: " + destination.wstring();
        SetStatus(modeStatus_);
        Log(EventLevel::Warning, L"Файл перемещён в локальный карантин DPopCleaner.");
        ListView_SetItemText(list_, index, 4, const_cast<wchar_t*>(L"в карантине"));
    } else {
        SetStatus(error);
        Log(EventLevel::Error, error);
    }
    InvalidateRect(Hwnd(), nullptr, TRUE);
}

void GuardPage::OpenQuarantine() {
    const fs::path path = QuarantineDirectory();
    if (path.empty()) { SetStatus(L"LOCALAPPDATA недоступен."); return; }
    std::error_code ec;
    fs::create_directories(path, ec);
    OpenPathInExplorer(Hwnd(), path);
    SetStatus(L"Открыт локальный карантин DPopCleaner.");
}

void GuardPage::OpenWindowsSecurity() {
    auto code = reinterpret_cast<INT_PTR>(ShellExecuteW(Hwnd(), L"open", L"windowsdefender://threat/", nullptr, nullptr, SW_SHOWNORMAL));
    if (code <= 32) code = reinterpret_cast<INT_PTR>(ShellExecuteW(Hwnd(), L"open", L"windowsdefender:", nullptr, nullptr, SW_SHOWNORMAL));
    SetStatus(code > 32 ? L"Windows Security открыт." : L"Не удалось открыть Windows Security.");
}

LRESULT GuardPage::OnMessage(UINT message, WPARAM wParam, LPARAM lParam, bool& handled) {
    if (message == WM_COMMAND) {
        const int id = LOWORD(wParam);
        if (id == kActionBase + 0) { RunQuickScan(); handled = true; return 0; }
        if (id == kActionBase + 1) { RunDefenderQuickScan(); handled = true; return 0; }
        if (id == kActionBase + 2) { ScanFile(); handled = true; return 0; }
        if (id == kActionBase + 3) { ScanFolder(); handled = true; return 0; }
        if (id == kActionBase + 4) { QuarantineSelected(); handled = true; return 0; }
        if (id == kActionBase + 5) { OpenQuarantine(); handled = true; return 0; }
        if (id == kActionBase + 6) { OpenWindowsSecurity(); handled = true; return 0; }
        if (id == kActionBase + 7) { Cancel(); SetProgress(0); SetStatus(L"Запрошена отмена DPopGuard-задачи. Уже запущенную системную проверку Defender DPopCleaner принудительно не завершает."); handled = true; return 0; }
    }
    if (message == WM_DRAWITEM) {
        const auto* draw = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (draw && draw->CtlType == ODT_BUTTON && draw->CtlID >= kActionBase && draw->CtlID < kActionBase + 8) {
            const ButtonVisual visual = (draw->CtlID == kActionBase || draw->CtlID == kActionBase + 1) ? ButtonVisual::Accent :
                draw->CtlID == kActionBase + 4 ? ButtonVisual::Danger : ButtonVisual::Normal;
            handled = DrawOwnerButton(*draw, GetControlText(draw->hwndItem), visual);
            return handled ? TRUE : 0;
        }
    }
    handled = false;
    return 0;
}

}
