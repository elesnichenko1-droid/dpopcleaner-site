#include "MainWindow.h"
#include "../resources/resource.h"

#include <windows.h>

#include <chrono>
#include <string>
#include <thread>

namespace {

struct WindowSearchContext {
    DWORD processId{};
    HWND window{};
};

BOOL CALLBACK FindDPopWindow(HWND hwnd, LPARAM parameter) {
    auto* context = reinterpret_cast<WindowSearchContext*>(parameter);
    if (!context) return FALSE;

    DWORD ownerProcessId = 0;
    GetWindowThreadProcessId(hwnd, &ownerProcessId);
    if (ownerProcessId != context->processId) return TRUE;

    wchar_t className[128]{};
    if (GetClassNameW(hwnd, className, static_cast<int>(std::size(className))) <= 0) return TRUE;
    if (lstrcmpW(className, L"DPopCleaner0418MainWindow") != 0) return TRUE;

    context->window = hwnd;
    return FALSE;
}

BOOL CALLBACK UpdateRevisionLabel(HWND hwnd, LPARAM) {
    wchar_t text[512]{};
    const int length = GetWindowTextW(hwnd, text, static_cast<int>(std::size(text)));
    if (length <= 0) return TRUE;

    std::wstring value(text, static_cast<size_t>(length));
    const std::wstring oldRevision = L"rev.1";
    const auto position = value.find(oldRevision);
    if (position != std::wstring::npos) {
        value.replace(position, oldRevision.size(), L"rev.2");
        SetWindowTextW(hwnd, value.c_str());
    }
    return TRUE;
}

void ApplyApplicationIdentityWhenWindowAppears(HINSTANCE instance) {
    HICON bigIcon = static_cast<HICON>(LoadImageW(
        instance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
        GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_SHARED));
    HICON smallIcon = static_cast<HICON>(LoadImageW(
        instance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED));
    if (!bigIcon || !smallIcon) return;

    WindowSearchContext context{GetCurrentProcessId(), nullptr};
    for (int attempt = 0; attempt < 150; ++attempt) {
        context.window = nullptr;
        EnumWindows(FindDPopWindow, reinterpret_cast<LPARAM>(&context));
        if (context.window) {
            SendMessageW(context.window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(bigIcon));
            SendMessageW(context.window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));
            SetClassLongPtrW(context.window, GCLP_HICON, reinterpret_cast<LONG_PTR>(bigIcon));
            SetClassLongPtrW(context.window, GCLP_HICONSM, reinterpret_cast<LONG_PTR>(smallIcon));
            EnumChildWindows(context.window, UpdateRevisionLabel, 0);
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int showCommand) {
    std::thread(ApplyApplicationIdentityWhenWindowAppears, instance).detach();
    return dpop0418::RunMainWindow(instance, showCommand);
}
