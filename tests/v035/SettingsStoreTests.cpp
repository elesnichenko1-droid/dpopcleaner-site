#include "modules/SettingsStore.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <windows.h>

using namespace dpop::settings;
namespace fs = std::filesystem;

namespace {

struct TempSettingsRoot {
    fs::path path;
    TempSettingsRoot() {
        wchar_t buffer[MAX_PATH]{};
        const DWORD n = GetTempPathW(MAX_PATH, buffer);
        assert(n > 0 && n < MAX_PATH);
        path = fs::path(buffer) / (L"dpop-settings-test-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
        fs::create_directories(path);
        SetEnvironmentVariableW(L"DPOP_SETTINGS_ROOT", path.c_str());
    }
    ~TempSettingsRoot() {
        SetEnvironmentVariableW(L"DPOP_SETTINGS_ROOT", nullptr);
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

void WriteUtf8(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    assert(out);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
}

void TestDefaultsAndValidation() {
    auto s = DefaultSettings();
    assert(s.schemaVersion == 2);
    assert(s.confirmDestructive);
    assert(s.checkUpdatesAtStartup);
    assert(!s.quickGuardAtStartup);
    assert(!s.memoryAutoTrimEnabled);
    assert(s.memoryAutoTrimPercent == 80);
    assert(s.memoryAutoTrimIntervalMinutes == 15);
    assert(s.largeFileMB == 500);
    assert(s.duplicateMinMB == 10);
    assert(!s.backgroundJunkMonitor);
    assert(!s.alwaysRunAsAdmin);
    assert(s.trayEnabled);
    assert(s.closeBehavior == CloseBehavior::MinimizeToTray);

    std::wstring error;
    s.memoryAutoTrimPercent = 49;
    assert(!ValidateSettings(s, error));
    s = DefaultSettings();
    s.largeFileMB = 4097;
    assert(!ValidateSettings(s, error));
    s = DefaultSettings();
    s.duplicateMinMB = 0;
    assert(!ValidateSettings(s, error));
    s = DefaultSettings();
    s.memoryAutoTrimIntervalMinutes = 1441;
    assert(!ValidateSettings(s, error));
    s = DefaultSettings();
    s.trayEnabled = false;
    s.closeBehavior = CloseBehavior::MinimizeToTray;
    assert(!ValidateSettings(s, error));
}

void TestNormalizeAndExclusions() {
    const auto a = NormalizeExclusionPath(L"C:/Users/Test/Cache/");
    const auto b = NormalizeExclusionPath(L"c:\\users\\test\\cache");
    assert(a == b);

    auto s = DefaultSettings();
    s.cleanExclusions = {a};
    assert(IsExcludedPath(L"C:\\Users\\Test\\Cache\\child.bin", s));
    assert(!IsExcludedPath(L"C:\\Users\\Test\\Other\\child.bin", s));
}

void TestActiveSettingsAreSessionScoped() {
    auto first = DefaultSettings();
    first.largeFileMB = 640;
    SetActiveSettings(first);
    assert(ActiveSettings().largeFileMB == 640);

    auto second = first;
    second.largeFileMB = 960;
    SetActiveSettings(second);
    assert(ActiveSettings() == second);
}

void TestRoundTrip() {
    TempSettingsRoot temp;
    auto s = DefaultSettings();
    s.confirmDestructive = false;
    s.largeFileMB = 777;
    s.duplicateMinMB = 22;
    s.runAtStartup = true;
    s.alwaysRunAsAdmin = true;
    s.checkUpdatesAtStartup = false;
    s.quickGuardAtStartup = true;
    s.checkUpdateCacheAtStartup = true;
    s.backgroundJunkMonitor = true;
    s.trayEnabled = true;
    s.closeBehavior = CloseBehavior::Ask;
    s.memoryAutoTrimEnabled = true;
    s.memoryAutoTrimPercent = 88;
    s.memoryAutoTrimIntervalMinutes = 9;
    s.memoryScope = MemoryScope::Advanced;
    s.cleanExclusions = {
        NormalizeExclusionPath(L"C:\\Temp\\Keep"),
        NormalizeExclusionPath(L"D:\\Projects\\Safe")
    };

    std::wstring error;
    assert(SaveAppSettings(s, error));
    assert(error.empty());
    assert(fs::exists(SettingsPath()));
    assert(!fs::exists(fs::path(SettingsPath().wstring() + L".tmp")));

    const auto loaded = LoadAppSettings();
    assert(!loaded.usedDefaults);
    assert(!loaded.migrated);
    assert(loaded.warning.empty());
    assert(loaded.settings == s);

    SetActiveSettings(loaded.settings);
    assert(ActiveSettings() == s);
}

void TestOverwriteExistingSettings() {
    TempSettingsRoot temp;
    auto first = DefaultSettings();
    first.trayEnabled = false;
    first.closeBehavior = CloseBehavior::Exit;
    first.checkUpdatesAtStartup = false;
    first.largeFileMB = 500;

    std::wstring error;
    assert(SaveAppSettings(first, error));
    assert(error.empty());
    assert(LoadAppSettings().settings.largeFileMB == 500);

    auto second = first;
    second.largeFileMB = 777;
    assert(SaveAppSettings(second, error));
    assert(error.empty());
    assert(!fs::exists(fs::path(SettingsPath().wstring() + L".tmp")));

    const auto loaded = LoadAppSettings();
    assert(!loaded.usedDefaults);
    assert(loaded.settings.largeFileMB == 777);
    assert(loaded.settings == second);
}

void TestCorruptFallback() {
    TempSettingsRoot temp;
    WriteUtf8(SettingsPath(), "{not-json");
    const auto loaded = LoadAppSettings();
    assert(loaded.usedDefaults);
    assert(!loaded.warning.empty());
    assert(loaded.settings == DefaultSettings());
}

void TestSchemaOneMigrationAndDedup() {
    TempSettingsRoot temp;
    WriteUtf8(SettingsPath(), R"JSON({
  "confirm_destructive": false,
  "large_file_mb": 900,
  "duplicate_min_mb": 15,
  "run_at_startup": true,
  "minimize_to_tray": true,
  "memory_auto_trim_percent": 86,
  "clean_exclusions": ["C:/Users/Test/Cache/", "c:\\users\\test\\cache", "D:/Keep"]
})JSON");

    const auto loaded = LoadAppSettings();
    assert(!loaded.usedDefaults);
    assert(loaded.migrated);
    assert(loaded.settings.schemaVersion == 2);
    assert(!loaded.settings.confirmDestructive);
    assert(loaded.settings.largeFileMB == 900);
    assert(loaded.settings.duplicateMinMB == 15);
    assert(loaded.settings.runAtStartup);
    assert(loaded.settings.trayEnabled);
    assert(loaded.settings.closeBehavior == CloseBehavior::MinimizeToTray);
    assert(loaded.settings.memoryAutoTrimPercent == 86);
    assert(loaded.settings.cleanExclusions.size() == 2);
}

} // namespace

int main() {
    TestDefaultsAndValidation();
    TestNormalizeAndExclusions();
    TestActiveSettingsAreSessionScoped();
    TestRoundTrip();
    TestOverwriteExistingSettings();
    TestCorruptFallback();
    TestSchemaOneMigrationAndDedup();
    return 0;
}
