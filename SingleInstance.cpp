#include "core/SingleInstance.h"

namespace dpop {

SingleInstance::SingleInstance(const std::wstring& mutexName) {
    mutex_ = CreateMutexW(nullptr, TRUE, mutexName.c_str());
    primary_ = mutex_ != nullptr && GetLastError() != ERROR_ALREADY_EXISTS;
}

SingleInstance::~SingleInstance() {
    if (mutex_ != nullptr) {
        CloseHandle(mutex_);
    }
}

bool SingleInstance::IsPrimary() const noexcept {
    return primary_;
}

bool SingleInstance::ActivateExistingWindow(const wchar_t* windowClassName) {
    HWND window = nullptr;
    for (int attempt = 0; attempt < 40 && window == nullptr; ++attempt) {
        window = FindWindowW(windowClassName, nullptr);
        if (window == nullptr) {
            Sleep(50);
        }
    }
    if (window == nullptr) return false;
    ShowWindow(window, IsIconic(window) ? SW_RESTORE : SW_SHOW);
    return SetForegroundWindow(window) != FALSE;
}

}
