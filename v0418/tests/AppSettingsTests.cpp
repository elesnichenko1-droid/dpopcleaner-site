#include "AppSettings.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace {
int Fail(const char* message) {
    std::cerr << message << "\n";
    return 1;
}

void WriteText(const fs::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
}
}

int main() {
    const fs::path root = fs::temp_directory_path() / L"dpop0418-appsettings-tests";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    if (ec) return Fail("cannot create temp directory");

    const fs::path settingsPath = root / L"settings.ini";

    const auto defaults = dpop0418::LoadSettings(settingsPath);
    if (!defaults.autoCheckUpdates)
        return Fail("missing settings must default auto-check to enabled");
    if (defaults.zapretStrategy != L"general.bat")
        return Fail("missing settings must default Zapret strategy to general.bat");

    WriteText(settingsPath,
              "[updates]\nauto_check=0\n"
              "[zapret]\nstrategy=general (ALT13).bat\n");
    const auto loaded = dpop0418::LoadSettings(settingsPath);
    if (loaded.autoCheckUpdates)
        return Fail("auto_check=0 must disable auto-check");
    if (loaded.zapretStrategy != L"general (ALT13).bat")
        return Fail("Zapret strategy must load from [zapret] strategy key");

    WriteText(settingsPath, "[updates]\nauto_check=maybe\n[zapret]\nstrategy=\n");
    const auto malformed = dpop0418::LoadSettings(settingsPath);
    if (!malformed.autoCheckUpdates)
        return Fail("malformed auto_check must fail safe to enabled");
    if (malformed.zapretStrategy != L"general.bat")
        return Fail("empty Zapret strategy must fail safe to general.bat");

    dpop0418::AppSettings disabled{};
    disabled.autoCheckUpdates = false;
    disabled.zapretStrategy = L"general (ALT13).bat";
    std::wstring error;
    if (!dpop0418::SaveSettingsAtomic(settingsPath, disabled, error))
        return Fail("atomic save must succeed for writable temp path");
    const auto savedDisabled = dpop0418::LoadSettings(settingsPath);
    if (savedDisabled.autoCheckUpdates)
        return Fail("saved disabled setting must reload disabled");
    if (savedDisabled.zapretStrategy != L"general (ALT13).bat")
        return Fail("saved Zapret strategy must reload exactly");
    if (fs::exists(settingsPath.wstring() + L".tmp"))
        return Fail("atomic save must not leave temporary file behind");

    dpop0418::AppSettings enabled{};
    enabled.autoCheckUpdates = true;
    enabled.zapretStrategy = L"general.bat";
    if (!dpop0418::SaveSettingsAtomic(settingsPath, enabled, error))
        return Fail("second atomic save must replace existing settings");
    const auto savedEnabled = dpop0418::LoadSettings(settingsPath);
    if (!savedEnabled.autoCheckUpdates)
        return Fail("saved enabled setting must reload enabled");
    if (savedEnabled.zapretStrategy != L"general.bat")
        return Fail("second save must replace Zapret strategy");

    fs::remove_all(root, ec);
    return 0;
}
