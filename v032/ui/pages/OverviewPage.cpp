#include "ui/pages/OverviewPage.h"

#include "modules/Applications.h"
#include "modules/Cleaner.h"
#include "modules/ZapretManager.h"
#include "ui/Controls.h"
#include "ui/Theme.h"

#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>
#include <string>

namespace dpop::ui {
namespace {

constexpr wchar_t kOverviewClassName[] =
    L"DPopCleaner032OverviewPage";

constexpr int kRefreshId = 1500;
constexpr int kCleaningId = 1501;
constexpr int kGuardId = 1502;
constexpr int kDiskId = 1503;
constexpr int kAppsId = 1504;

constexpr int kGap = 14;
constexpr int kActionHeight = 40;

std::wstring FormatBytes(std::uint64_t bytes) {
    constexpr double kGiB = 1024.0 * 1024.0 * 1024.0;
    constexpr double kMiB = 1024.0 * 1024.0;

    std::wostringstream out;
    out << std::fixed << std::setprecision(1);

    if (bytes >= static_cast<std::uint64_t>(kGiB)) {
        out << (static_cast<double>(bytes) / kGiB) << L" ГБ";
    } else {
        out << (static_cast<double>(bytes) / kMiB) << L" МБ";
    }

    return out.str();
}

std::wstring DiskValue(const OverviewModel& model) {
    std::wstring text = FormatBytes(model.driveUsedBytes);
    text += L" / ";
    text += FormatBytes(model.driveTotalBytes);
    text += L" • ";
    text += std::to_wstring(model.driveUsedPercent);
    text += L"%";
    return text;
}

std::wstring DiskDetail(const OverviewModel& model) {
    std::wstring text = L"Свободно: ";
    text += FormatBytes(model.driveFreeBytes);
    return text;
}

std::wstring RamValue(const OverviewModel& model) {
    std::wstring text = FormatBytes(model.ramUsedBytes);
    text += L" / ";
    text += FormatBytes(model.ramTotalBytes);
    text += L" • ";
    text += std::to_wstring(model.ramUsedPercent);
    text += L"%";
    return text;
}

std::wstring RamDetail(const OverviewModel& model) {
    std::wstring text = L"Доступно: ";
    text += FormatBytes(model.ramAvailableBytes);
    text += L" • процессов: ";
    text += std::to_wstring(model.processCount);
    return text;
}

std::wstring AppsValue(const OverviewModel& model) {
    return std::to_wstring(model.appCount) + L" программ";
}

std::wstring RecycleValue(const OverviewModel& model) {
    return model.recycleEmpty
        ? L"Пусто"
        : FormatBytes(model.recycleBytes);
}

struct OverviewGeometry {
    std::array<RECT, 6> cards{};
    std::array<RECT, 5> actions{};
};

OverviewGeometry ComputeGeometry(int width, int height) noexcept {
    OverviewGeometry g{};

    const bool singleActionRow = width >= 1132;
    const int actionRows = singleActionRow ? 1 : 2;
    const int actionAreaHeight =
        actionRows * kActionHeight + (actionRows - 1) * kGap;

    const int cardAreaHeight =
        std::max(180, height - actionAreaHeight - kGap);
    const int cardWidth = std::max(250, (width - 2 * kGap) / 3);
    const int cardHeight =
        std::max(90, (cardAreaHeight - kGap) / 2);

    for (int row = 0; row < 2; ++row) {
        for (int col = 0; col < 3; ++col) {
            const int index = row * 3 + col;
            const int left = col * (cardWidth + kGap);
            const int top = row * (cardHeight + kGap);

            g.cards[index] = RECT{
                left,
                top,
                left + cardWidth,
                top + cardHeight
            };
        }
    }

    const int actionsTop =
        std::max(0, height - actionAreaHeight);

    if (singleActionRow) {
        const int actionWidth =
            std::max(120, (width - 4 * kGap) / 5);

        for (int i = 0; i < 5; ++i) {
            const int left = i * (actionWidth + kGap);
            g.actions[i] = RECT{
                left,
                actionsTop,
                left + actionWidth,
                actionsTop + kActionHeight
            };
        }
    } else {
        const int actionWidth =
            std::max(160, (width - 2 * kGap) / 3);

        for (int i = 0; i < 3; ++i) {
            const int left = i * (actionWidth + kGap);
            g.actions[i] = RECT{
                left,
                actionsTop,
                left + actionWidth,
                actionsTop + kActionHeight
            };
        }

        for (int i = 3; i < 5; ++i) {
            const int col = i - 3;
            const int left = col * (actionWidth + kGap);
            const int top = actionsTop + kActionHeight + kGap;
            g.actions[i] = RECT{
                left,
                top,
                left + actionWidth,
                top + kActionHeight
            };
        }
    }

    return g;
}

void DrawTextLine(
    HDC dc,
    const RECT& card,
    int topOffset,
    int height,
    std::wstring_view text,
    HFONT font,
    COLORREF color
) {
    RECT rc{
        card.left + 16,
        card.top + topOffset,
        card.right - 16,
        card.top + topOffset + height
    };

    HGDIOBJ oldFont = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);

    std::wstring owned{text};
    DrawTextW(
        dc,
        owned.data(),
        static_cast<int>(owned.size()),
        &rc,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE |
            DT_END_ELLIPSIS | DT_NOPREFIX
    );

    SelectObject(dc, oldFont);
}

void DrawCard(
    HDC dc,
    const RECT& card,
    std::wstring_view title,
    std::wstring_view value,
    std::wstring_view detail,
    HFONT titleFont,
    HFONT valueFont,
    HFONT detailFont
) {
    const auto& p = MidnightPalette();

    HBRUSH brush = CreateSolidBrush(p.control);
    HPEN pen = CreatePen(PS_SOLID, 1, p.border);

    if (!brush || !pen) {
        if (brush) DeleteObject(brush);
        if (pen) DeleteObject(pen);
        return;
    }

    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, pen);

    RoundRect(
        dc,
        card.left,
        card.top,
        card.right,
        card.bottom,
        12,
        12
    );

    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);

    DrawTextLine(
        dc, card, 10, 24, title, titleFont, p.muted
    );
    DrawTextLine(
        dc, card, 38, 34, value, valueFont, p.text
    );
    DrawTextLine(
        dc, card, 76, 26, detail, detailFont, p.muted
    );
}

void MoveToRect(HWND hwnd, const RECT& rc) noexcept {
    if (!hwnd) {
        return;
    }

    MoveWindow(
        hwnd,
        rc.left,
        rc.top,
        rc.right - rc.left,
        rc.bottom - rc.top,
        TRUE
    );
}

}

OverviewPage::~OverviewPage() {
    Destroy();
}

bool OverviewPage::RegisterWindowClass() noexcept {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &OverviewPage::WindowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kOverviewClassName;

    if (!RegisterClassExW(&wc) &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    return true;
}

bool OverviewPage::Create(
    HWND parent,
    SessionLog& sessionLog,
    std::function<void(Page)> navigate
) {
    Destroy();

    parent_ = parent;
    sessionLog_ = &sessionLog;
    navigate_ = std::move(navigate);

    if (!RegisterWindowClass()) {
        return false;
    }

    hwnd_ = CreateWindowExW(
        0,
        kOverviewClassName,
        L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0,
        0,
        0,
        0,
        parent_,
        nullptr,
        GetModuleHandleW(nullptr),
        this
    );

    if (!hwnd_) {
        Destroy();
        return false;
    }

    titleFont_ = CreateUiFont(10, FW_SEMIBOLD);
    valueFont_ = CreateUiFont(17, FW_BOLD);
    detailFont_ = CreateUiFont(9, FW_NORMAL);
    actionFont_ = CreateUiFont(10, FW_SEMIBOLD);

    if (!titleFont_ || !valueFont_ || !detailFont_ || !actionFont_) {
        Destroy();
        return false;
    }

    refresh_ = CreatePushButton(
        hwnd_, kRefreshId, L"Обновить", ButtonVisual::Accent
    );
    cleaning_ = CreatePushButton(
        hwnd_, kCleaningId, L"Быстрая очистка"
    );
    guard_ = CreatePushButton(
        hwnd_, kGuardId, L"Быстрый DPopGuard"
    );
    disk_ = CreatePushButton(
        hwnd_, kDiskId, L"Открыть диск"
    );
    apps_ = CreatePushButton(
        hwnd_, kAppsId, L"Открыть приложения"
    );

    for (HWND button : {refresh_, cleaning_, guard_, disk_, apps_}) {
        if (!button) {
            Destroy();
            return false;
        }
        ApplyControlFont(button, actionFont_);
    }

    Refresh();
    return true;
}

void OverviewPage::Destroy() noexcept {
    for (HWND* handle : {
        &refresh_, &cleaning_, &guard_, &disk_, &apps_
    }) {
        if (*handle && IsWindow(*handle)) {
            DestroyWindow(*handle);
        }
        *handle = nullptr;
    }

    if (hwnd_ && IsWindow(hwnd_)) {
        DestroyWindow(hwnd_);
    }
    hwnd_ = nullptr;

    for (HFONT* font : {
        &titleFont_, &valueFont_, &detailFont_, &actionFont_
    }) {
        if (*font) {
            DeleteObject(*font);
        }
        *font = nullptr;
    }

    parent_ = nullptr;
    sessionLog_ = nullptr;
    navigate_ = {};
}

void OverviewPage::Show(bool visible) noexcept {
    if (hwnd_) {
        ShowWindow(hwnd_, visible ? SW_SHOW : SW_HIDE);
    }
}

void OverviewPage::Layout(const Box& box) noexcept {
    if (!hwnd_) {
        return;
    }

    MoveWindow(
        hwnd_,
        box.x,
        box.y,
        box.width,
        box.height,
        TRUE
    );

    LayoutActions();
}

void OverviewPage::LayoutActions() noexcept {
    if (!hwnd_) {
        return;
    }

    RECT client{};
    GetClientRect(hwnd_, &client);
    const auto geometry =
        ComputeGeometry(client.right, client.bottom);

    MoveToRect(refresh_, geometry.actions[0]);
    MoveToRect(cleaning_, geometry.actions[1]);
    MoveToRect(guard_, geometry.actions[2]);
    MoveToRect(disk_, geometry.actions[3]);
    MoveToRect(apps_, geometry.actions[4]);
}

void OverviewPage::NotifyLogChanged() noexcept {
    HWND shell = parent_ ? GetParent(parent_) : nullptr;
    if (shell) {
        PostMessageW(
            shell,
            kOverviewLogChangedMessage,
            0,
            0
        );
    }
}

void OverviewPage::Refresh() {
    try {
        const auto snapshot = dpop::system_info::Collect();
        const auto installed = dpop::apps::EnumerateInstalledApps();
        const auto recycleBytes =
            dpop::cleaner::EstimateRecycleBinBytes();
        const auto zapretStatus = dpop::zapret::QueryStatus();

        model_ = BuildOverviewModel(
            snapshot,
            installed.size(),
            recycleBytes,
            zapretStatus.serviceInstalled,
            zapretStatus.winwsRunning
        );

        if (sessionLog_) {
            sessionLog_->Append(
                L"Overview",
                EventLevel::Info,
                L"Обзор обновлён."
            );
            NotifyLogChanged();
        }
    } catch (...) {
        if (sessionLog_) {
            sessionLog_->Append(
                L"Overview",
                EventLevel::Error,
                L"Не удалось обновить обзор."
            );
            NotifyLogChanged();
        }
    }

    if (hwnd_) {
        InvalidateRect(hwnd_, nullptr, TRUE);
    }
}

void OverviewPage::Paint() noexcept {
    if (!hwnd_) {
        return;
    }

    const auto& p = MidnightPalette();

    PAINTSTRUCT ps{};
    HDC dc = BeginPaint(hwnd_, &ps);

    RECT client{};
    GetClientRect(hwnd_, &client);

    SetDCBrushColor(dc, p.background);
    FillRect(
        dc,
        &client,
        reinterpret_cast<HBRUSH>(GetStockObject(DC_BRUSH))
    );

    const auto geometry =
        ComputeGeometry(client.right, client.bottom);

    const std::wstring diskValue = DiskValue(model_);
    const std::wstring diskDetail = DiskDetail(model_);
    const std::wstring ramValue = RamValue(model_);
    const std::wstring ramDetail = RamDetail(model_);
    const std::wstring appsValue = AppsValue(model_);
    const std::wstring recycleValue = RecycleValue(model_);

    std::wstring gpuDetail =
        model_.gpuName.empty()
            ? L"GPU: не определён"
            : L"GPU: " + model_.gpuName;
    gpuDetail += L" • CPU: ";
    gpuDetail += std::to_wstring(model_.cpuCount);

    DrawCard(
        dc,
        geometry.cards[0],
        L"Диск C:\\",
        diskValue,
        diskDetail,
        titleFont_,
        valueFont_,
        detailFont_
    );

    DrawCard(
        dc,
        geometry.cards[1],
        L"Оперативная память",
        ramValue,
        ramDetail,
        titleFont_,
        valueFont_,
        detailFont_
    );

    DrawCard(
        dc,
        geometry.cards[2],
        L"Установленные приложения",
        appsValue,
        gpuDetail,
        titleFont_,
        valueFont_,
        detailFont_
    );

    DrawCard(
        dc,
        geometry.cards[3],
        L"DPopGuard",
        model_.guardText,
        L"Быстрый скан и проверка файлов через AMSI",
        titleFont_,
        valueFont_,
        detailFont_
    );

    DrawCard(
        dc,
        geometry.cards[4],
        L"Zapret",
        model_.zapretText,
        L"Фактическое состояние сервиса и winws",
        titleFont_,
        valueFont_,
        detailFont_
    );

    DrawCard(
        dc,
        geometry.cards[5],
        L"Заполненность корзины",
        recycleValue,
        model_.recycleEmpty
            ? L"Корзина пуста"
            : L"Можно открыть раздел очистки",
        titleFont_,
        valueFont_,
        detailFont_
    );

    EndPaint(hwnd_, &ps);
}

LRESULT CALLBACK OverviewPage::WindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
) {
    OverviewPage* self = reinterpret_cast<OverviewPage*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA)
    );

    if (message == WM_NCCREATE) {
        const auto* create =
            reinterpret_cast<const CREATESTRUCTW*>(lParam);
        self = static_cast<OverviewPage*>(
            create->lpCreateParams
        );
        SetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(self)
        );
        self->hwnd_ = hwnd;
    }

    if (!self) {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    return self->HandleMessage(message, wParam, lParam);
}

LRESULT OverviewPage::HandleMessage(
    UINT message,
    WPARAM wParam,
    LPARAM lParam
) {
    switch (message) {
    case WM_SIZE:
        LayoutActions();
        InvalidateRect(hwnd_, nullptr, TRUE);
        return 0;

    case WM_COMMAND: {
        const int id = LOWORD(wParam);

        if (id == kRefreshId) {
            Refresh();
            return 0;
        }

        if (!navigate_) {
            return 0;
        }

        if (id == kCleaningId) {
            navigate_(Page::Cleaning);
            return 0;
        }
        if (id == kGuardId) {
            navigate_(Page::Guard);
            return 0;
        }
        if (id == kDiskId) {
            navigate_(Page::Disk);
            return 0;
        }
        if (id == kAppsId) {
            navigate_(Page::Applications);
            return 0;
        }

        break;
    }

    case WM_DRAWITEM: {
        const auto* draw =
            reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (!draw || draw->CtlType != ODT_BUTTON) {
            break;
        }

        const ButtonVisual visual =
            static_cast<int>(draw->CtlID) == kRefreshId
                ? ButtonVisual::Accent
                : ButtonVisual::Normal;

        wchar_t text[128]{};
        GetWindowTextW(
            draw->hwndItem,
            text,
            static_cast<int>(std::size(text))
        );

        if (DrawOwnerButton(*draw, text, visual)) {
            return TRUE;
        }
        break;
    }

    case WM_ERASEBKGND: {
        const auto& p = MidnightPalette();
        RECT rc{};
        GetClientRect(hwnd_, &rc);
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetDCBrushColor(dc, p.background);
        FillRect(
            dc,
            &rc,
            reinterpret_cast<HBRUSH>(
                GetStockObject(DC_BRUSH)
            )
        );
        return 1;
    }

    case WM_PAINT:
        Paint();
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

}
