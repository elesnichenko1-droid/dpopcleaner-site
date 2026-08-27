#pragma once

#include <filesystem>
#include <string>

namespace dpop0418 {

struct AppSettings {
    bool autoCheckUpdates{true};
    std::wstring zapretStrategy{L"general.bat"};
};

AppSettings LoadSettings(const std::filesystem::path& path);
bool SaveSettingsAtomic(const std::filesystem::path& path, const AppSettings& settings, std::wstring& error);

} // namespace dpop0418
