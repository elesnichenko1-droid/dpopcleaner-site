#pragma once
#include <windows.h>
#include <string_view>

namespace dpop::ui::shell {

// Kept for binary/source compatibility with the recovered 0.3.3 shell.
// In 0.3.4 Settings is a normal sidebar section (command 1012).
inline constexpr int kSettingsCommandId = 1100;

struct ShellIdentity {
    std::wstring_view windowTitle;
    std::wstring_view productName;
    std::wstring_view subtitle;
    std::wstring_view betaLabel;
};

inline constexpr ShellIdentity kShellIdentity{
    L"DPopCleaner 0.3.4 BETA R1",
    L"DPopCleaner",
    L"Windows под контролем",
    L"BETA"
};

constexpr const ShellIdentity& Identity() noexcept { return kShellIdentity; }
int Run(HINSTANCE instance, int showCommand);

}
