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

    if (!dpop0418::LoadSettings(settingsPath).autoCheckUpdates)
        return Fail("missing settings must default auto-check to enabled");

    WriteText(settingsPath, "[updates]\nauto_check=0\n");
    if (dpop0418::LoadSettings(settingsPath).autoCheckUpdates)
        return Fail("auto_check=0 must disable auto-check");

    WriteText(settingsPath, "[updates]\nauto_check=maybe\n");
    if (!dpop0418::LoadSettings(settingsPath).autoCheckUpdates)
        return Fail("malformed auto_check must fail safe to enabled");

    dpop0418::AppSettings disabled{};
    disabled.autoCheckUpdates = false;
    std::wstring error;
    if (!dpop0418::SaveSettingsAtomic(settingsPath, disabled, error))
        return Fail("atomic save must succeed for writable temp path");
    if (dpop0418::LoadSettings(settingsPath).autoCheckUpdates)
        return Fail("saved disabled setting must reload disabled");
    if (fs::exists(settingsPath.wstring() + L".tmp"))
        return Fail("atomic save must not leave temporary file behind");

    dpop0418::AppSettings enabled{};
    enabled.autoCheckUpdates = true;
    if (!dpop0418::SaveSettingsAtomic(settingsPath, enabled, error))
        return Fail("second atomic save must replace existing settings");
    if (!dpop0418::LoadSettings(settingsPath).autoCheckUpdates)
        return Fail("saved enabled setting must reload enabled");

    fs::remove_all(root, ec);
    return 0;
}
