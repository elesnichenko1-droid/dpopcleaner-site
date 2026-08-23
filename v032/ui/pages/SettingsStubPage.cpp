#include "ui/pages/SettingsStubPage.h"

#include "ui/Controls.h"
#include "ui/Theme.h"

namespace dpop::ui {
namespace {

constexpr int kHeadingId = 1410;
constexpr int kLanguageId = 1411;
constexpr int kThemeId = 1412;
constexpr int kBetaId = 1413;
constexpr int kNoteId = 1414;

void Move(HWND hwnd, int x, int y, int width, int height) noexcept {
    if (hwnd) {
        MoveWindow(hwnd, x, y, width, height, TRUE);
    }
}

}

SettingsStubPage::~SettingsStubPage() {
    Destroy();
}

bool SettingsStubPage::Create(HWND parent) noexcept {
    Destroy();

    headingFont_ = CreateUiFont(22, FW_SEMIBOLD);
    bodyFont_ = CreateUiFont(11, FW_NORMAL);

    heading_ = CreateTextLabel(parent, kHeadingId, L"Настройки");
    language_ = CreateTextLabel(parent, kLanguageId, L"Язык: Русский");
    theme_ = CreateTextLabel(parent, kThemeId, L"Тема: Midnight");
    beta_ = CreateTextLabel(parent, kBetaId, L"Бесплатная BETA");
    note_ = CreateTextLabel(
        parent,
        kNoteId,
        L"Функциональные настройки будут подключены отдельным этапом 0.3.2; "
        L"этот shell-candidate не сохраняет параметры."
    );

    if (!heading_ || !language_ || !theme_ || !beta_ || !note_) {
        Destroy();
        return false;
    }

    ApplyControlFont(heading_, headingFont_);
    ApplyControlFont(language_, bodyFont_);
    ApplyControlFont(theme_, bodyFont_);
    ApplyControlFont(beta_, bodyFont_);
    ApplyControlFont(note_, bodyFont_);

    Show(false);
    return true;
}

void SettingsStubPage::Destroy() noexcept {
    for (HWND* handle : {&heading_, &language_, &theme_, &beta_, &note_}) {
        if (*handle && IsWindow(*handle)) {
            DestroyWindow(*handle);
        }
        *handle = nullptr;
    }

    if (headingFont_) {
        DeleteObject(headingFont_);
        headingFont_ = nullptr;
    }
    if (bodyFont_) {
        DeleteObject(bodyFont_);
        bodyFont_ = nullptr;
    }
}

void SettingsStubPage::Show(bool visible) noexcept {
    const int command = visible ? SW_SHOW : SW_HIDE;
    for (HWND handle : {heading_, language_, theme_, beta_, note_}) {
        if (handle) {
            ShowWindow(handle, command);
        }
    }
}

void SettingsStubPage::Layout(int width, int height) noexcept {
    const int contentWidth = (width > 48) ? width - 48 : 0;

    Move(heading_, 24, 20, contentWidth, 40);
    Move(language_, 24, 78, contentWidth, 28);
    Move(theme_, 24, 114, contentWidth, 28);
    Move(beta_, 24, 150, contentWidth, 28);
    Move(note_, 24, 204, contentWidth, (height > 260) ? 72 : 48);
}

}
