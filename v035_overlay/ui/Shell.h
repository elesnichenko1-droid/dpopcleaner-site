#pragma once
#include <windows.h>

#include <string_view>

namespace dpop::ui::shell {

inline constexpr int kSettingsCommandId = 1100;

struct ShellIdentity {
    std::wstring_view windowTitle;
    std::wstring_view productName;
    std::wstring_view subtitle;
    std::wstring_view betaLabel;
};

inline constexpr ShellIdentity kShellIdentity{
    L"DPopCleaner 0.3.5 BETA R1",
    L"DPopCleaner",
    L"Очистка • память • защита • диски • Windows",
    L"BETA"
};

constexpr const ShellIdentity& Identity() noexcept {
    return kShellIdentity;
}

int Run(HINSTANCE instance, int showCommand);

}
