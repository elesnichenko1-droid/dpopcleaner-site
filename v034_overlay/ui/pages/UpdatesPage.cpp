#include "ui/pages/UpdatesPage.h"

#include "core/Version.h"
#include "ui/Controls.h"
#include "ui/PageLayout.h"
#include "ui/Theme.h"

#include <shellapi.h>
#include <utility>
#include <filesystem>
#include <algorithm>
#include <string>

namespace dpop::ui {
namespace {
constexpr int kLabelBase = 3200;
constexpr int kButtonBase = 3220;
constexpr wchar_t kReleasesUrl[] = L"https://github.com/elesnichenko1-droid/dpopcleaner-site/releases";
}

bool UpdatesPage::OnCreate() {
    if (!fonts_.Create()) return false;
    for (std::size_t i = 0; i < labels_.size(); ++i) {
        labels_[i] = CreateTextLabel(Hwnd(), kLabelBase + static_cast<int>(i), L"—", SS_LEFT | SS_NOPREFIX);
        if (!labels_[i]) return false;
        ApplyControlFont(labels_[i], i == 0 ? fonts_.section : fonts_.body);
    }
    const std::wstring_view labels[] = {L"Проверить обновления", L"Скачать и установить", L"Открыть релизы"};
    for (int i = 0; i < 3; ++i) {
        buttons_[static_cast<std::size_t>(i)] = CreatePushButton(
            Hwnd(), kButtonBase + i, labels[i], i == 0 ? ButtonVisual::Accent : ButtonVisual::Normal);
        if (!buttons_[static_cast<std::size_t>(i)]) return false;
        ApplyControlFont(buttons_[static_cast<std::size_t>(i)], fonts_.body);
    }
    EnableWindow(buttons_[1], FALSE);
    RefreshText();
    return true;
}

void UpdatesPage::OnVisibilityChanged(bool visible) noexcept {
    if (!visible) { Cancel(); return; }
    RefreshText();
}

void UpdatesPage::CheckAtStartup() {
    if (IsBusy()) return;
    Check(true);
}

void UpdatesPage::RefreshText() {
    SetControlText(labels_[0], L"DPopCleaner " + std::wstring(dpop::version::kDisplayVersion));
    SetControlText(labels_[1], L"Канал: BETA • внутренний код: " + std::to_wstring(dpop::version::kVersionCode));
    if (!checked_) {
        SetControlText(labels_[2], L"Проверка ещё не выполнялась.");
        SetControlText(labels_[3], L"Источник: update/beta.json по HTTPS.");
        SetControlText(labels_[4], L"Перед установкой проверяются размер, SHA-256 и политика подписи.");
        EnableWindow(buttons_[1], FALSE);
        return;
    }
    if (!lastCheck_.success) {
        SetControlText(labels_[2], L"Ошибка проверки: " + lastCheck_.error);
        SetControlText(labels_[3], L"Текущая версия остаётся активной.");
        SetControlText(labels_[4], L"Повторите проверку после восстановления сети.");
        EnableWindow(buttons_[1], FALSE);
        return;
    }
    if (!lastCheck_.updateAvailable) {
        SetControlText(labels_[2], L"Установлена актуальная версия для BETA-канала.");
        SetControlText(labels_[3], L"Манифест: " + lastCheck_.manifest.version);
        SetControlText(labels_[4], L"Новых пакетов с большим version_code нет.");
        EnableWindow(buttons_[1], FALSE);
        return;
    }
    SetControlText(labels_[2], L"Доступно обновление: " + lastCheck_.manifest.version);
    SetControlText(labels_[3], L"Размер: " + std::to_wstring(lastCheck_.manifest.size) + L" байт • revision " + std::to_wstring(lastCheck_.manifest.revision));
    SetControlText(labels_[4], lastCheck_.manifest.signedPackage
        ? L"Пакет требует Authenticode + SHA-256."
        : L"BETA-пакет не подписан: потребуется отдельное подтверждение пользователя.");
    EnableWindow(buttons_[1], TRUE);
}

void UpdatesPage::Check(bool promptWhenAvailable) {
    StartAsync(promptWhenAvailable ? L"Проверяем обновления при запуске…" : L"Проверяем BETA-канал обновлений…", [this, promptWhenAvailable](std::stop_token) {
        auto result = dpop::update::CheckForUpdates();
        QueueApply([this, promptWhenAvailable, result = std::move(result)]() mutable {
            lastCheck_ = std::move(result);
            checked_ = true;
            RefreshText();
            if (!lastCheck_.success) {
                SetStatus(lastCheck_.error);
                Log(EventLevel::Warning, L"Проверка обновлений завершилась ошибкой.");
            } else if (lastCheck_.updateAvailable) {
                SetStatus(L"Доступно обновление: " + lastCheck_.manifest.version);
                Log(EventLevel::Info, L"Найдена новая версия в BETA-канале.");
                if (promptWhenAvailable) {
                    std::wstring prompt = L"Доступно обновление DPopCleaner: " + lastCheck_.manifest.version +
                        L".\n\nСкачать, проверить SHA-256/подпись и установить сейчас?\n\nВы всегда можете выбрать «Нет» и обновиться позже из раздела «Обновления».";
                    const int answer = MessageBoxW(Hwnd(), prompt.c_str(), L"Обновление DPopCleaner", MB_YESNO | MB_ICONINFORMATION | MB_DEFBUTTON2);
                    if (answer == IDYES) Install(true);
                }
            } else {
                SetStatus(L"Установлена актуальная версия.");
                Log(EventLevel::Info, L"Новых обновлений нет.");
            }
            InvalidateRect(Hwnd(), nullptr, TRUE);
        });
    });
}

void UpdatesPage::Install(bool startupApproved) {
    if (!checked_ || !lastCheck_.success || !lastCheck_.updateAvailable) {
        SetStatus(L"Сначала выполните проверку и выберите доступное обновление.");
        return;
    }
    const auto manifest = lastCheck_.manifest;
    bool allowUnsigned = false;
    if (!manifest.signedPackage) {
        if (!ConfirmAction(
                Hwnd(),
                L"Этот BETA-пакет не подписан Authenticode. SHA-256 всё равно будет проверен. Скачать и передать пакет DPopUpdater?",
                true)) {
            return;
        }
        allowUnsigned = true;
    } else if (!startupApproved && !ConfirmAction(Hwnd(), L"Скачать проверенное обновление и запустить DPopUpdater?", true)) {
        return;
    }

    StartAsync(L"Скачиваем и проверяем обновление…", [this, manifest, allowUnsigned](std::stop_token) {
        std::filesystem::path package;
        std::wstring error;
        const bool downloaded = dpop::update::DownloadPackage(manifest, package, error);
        QueueApply([this, manifest, allowUnsigned, package = std::move(package), downloaded, error = std::move(error)]() mutable {
            if (!downloaded) {
                SetStatus(error);
                Log(EventLevel::Error, error);
                return;
            }
            std::wstring launchError;
            if (!dpop::update::PrepareAndLaunchUpdater(manifest, package, allowUnsigned, launchError)) {
                SetStatus(launchError);
                Log(EventLevel::Error, launchError);
                return;
            }
            SetStatus(L"DPopUpdater запущен. DPopCleaner закрывается для установки обновления.");
            Log(EventLevel::Info, L"Пакет обновления проверен и передан DPopUpdater.");
            HWND shell = GetParent(Parent());
            if (shell) PostMessageW(shell, WM_CLOSE, 0, 0);
        });
    });
}

void UpdatesPage::OpenRelease() {
    const wchar_t* target = kReleasesUrl;
    std::wstring release;
    if (checked_ && lastCheck_.success && !lastCheck_.manifest.releaseNotesUrl.empty()) {
        release = lastCheck_.manifest.releaseNotesUrl;
        target = release.c_str();
    }
    const auto code = reinterpret_cast<INT_PTR>(ShellExecuteW(Hwnd(), L"open", target, nullptr, nullptr, SW_SHOWNORMAL));
    SetStatus(code > 32 ? L"Страница релизов открыта." : L"Не удалось открыть страницу релизов.");
}

void UpdatesPage::OnLayout(int width, int height) noexcept {
    const UINT dpiRaw = GetDpiForWindow(Hwnd());
    const int top = ComputePageContentTop(dpiRaw ? static_cast<int>(dpiRaw) : 96);
    const int margin = 18;
    const int panelBottom = std::max(top + 260, height - 76);
    int y = top + 42;
    MoveWindow(labels_[0], margin + 18, y, width - margin * 2 - 36, 30, TRUE); y += 34;
    MoveWindow(labels_[1], margin + 18, y, width - margin * 2 - 36, 26, TRUE); y += 42;
    for (int i = 2; i < 5; ++i) {
        MoveWindow(labels_[static_cast<std::size_t>(i)], margin + 18, y, width - margin * 2 - 36, 34, TRUE);
        y += 38;
    }
    const int gap = 10;
    const int buttonW = std::max(150, (width - margin * 2 - gap * 2) / 3);
    const int buttonY = panelBottom - 52;
    for (int i = 0; i < 3; ++i) {
        MoveWindow(buttons_[static_cast<std::size_t>(i)], margin + i * (buttonW + gap), buttonY, buttonW, 36, TRUE);
    }
}

void UpdatesPage::OnPaint(HDC dc, const RECT& client) noexcept {
    PageBase::OnPaint(dc, client);
    DrawPageHeading(
        dc, 18, 4,
        L"Обновления",
        L"Автопроверка при запуске настраивается в «Настройках». HTTPS-манифест, SHA-256 и Authenticode проверяются до передачи DPopUpdater.",
        fonts_.title, fonts_.body);
    const UINT dpiRaw = GetDpiForWindow(Hwnd());
    const int top = ComputePageContentTop(dpiRaw ? static_cast<int>(dpiRaw) : 96);
    RECT panel{18, top, client.right - 18, client.bottom - 18};
    DrawPanel(dc, panel, true);
    DrawPanelTitle(dc, panel, L"BETA-канал DPopCleaner", fonts_.section);
}

LRESULT UpdatesPage::OnMessage(UINT message, WPARAM wParam, LPARAM lParam, bool& handled) {
    if (message == WM_COMMAND) {
        const int id = LOWORD(wParam);
        if (id == kButtonBase) { Check(false); handled = true; return 0; }
        if (id == kButtonBase + 1) { Install(false); handled = true; return 0; }
        if (id == kButtonBase + 2) { OpenRelease(); handled = true; return 0; }
    }
    if (message == WM_DRAWITEM) {
        const auto* draw = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (draw && draw->CtlType == ODT_BUTTON && draw->CtlID >= kButtonBase && draw->CtlID < kButtonBase + 3) {
            handled = DrawOwnerButton(*draw, GetControlText(draw->hwndItem), draw->CtlID == kButtonBase ? ButtonVisual::Accent : ButtonVisual::Normal);
            return handled ? TRUE : 0;
        }
    }
    handled = false;
    return 0;
}

}
