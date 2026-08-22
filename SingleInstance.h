#pragma once

#include <windows.h>
#include <string>

namespace dpop {

class SingleInstance final {
public:
    explicit SingleInstance(const std::wstring& mutexName);
    ~SingleInstance();

    SingleInstance(const SingleInstance&) = delete;
    SingleInstance& operator=(const SingleInstance&) = delete;

    bool IsPrimary() const noexcept;
    static bool ActivateExistingWindow(const wchar_t* windowClassName);

private:
    HANDLE mutex_{};
    bool primary_{};
};

}
