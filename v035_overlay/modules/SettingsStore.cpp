#include "modules/SettingsStore.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>
#include <system_error>

namespace dpop::settings {
namespace {
namespace fs = std::filesystem;

std::wstring EnvWide(const wchar_t* name) {
    const DWORD required = GetEnvironmentVariableW(name, nullptr, 0);
    if (required == 0) return {};
    std::wstring value(required, L'\0');
    const DWORD written = GetEnvironmentVariableW(name, value.data(), required);
    if (written == 0 || written >= required) return {};
    value.resize(written);
    return value;
}

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (required <= 0) return {};
    std::string out(static_cast<std::size_t>(required), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), out.data(), required, nullptr, nullptr) <= 0) return {};
    return out;
}

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) return {};
    const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (required <= 0) return {};
    std::wstring out(static_cast<std::size_t>(required), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), out.data(), required) <= 0) return {};
    return out;
}

std::optional<std::string> ReadFileUtf8(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::size_t SkipSpace(const std::string& text, std::size_t pos) {
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])) != 0) ++pos;
    return pos;
}

std::optional<std::size_t> ValueStart(const std::string& text, std::string_view key) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto keyPos = text.find(needle);
    if (keyPos == std::string::npos) return std::nullopt;
    const auto colon = text.find(':', keyPos + needle.size());
    if (colon == std::string::npos) return text.size();
    return SkipSpace(text, colon + 1);
}

bool ReadBool(const std::string& text, std::string_view key, bool fallback, bool& invalid) {
    const auto start = ValueStart(text, key);
    if (!start) return fallback;
    if (*start >= text.size()) { invalid = true; return fallback; }
    if (text.compare(*start, 4, "true") == 0) return true;
    if (text.compare(*start, 5, "false") == 0) return false;
    invalid = true;
    return fallback;
}

unsigned ReadUInt(const std::string& text, std::string_view key, unsigned fallback, bool& invalid, bool* present = nullptr) {
    const auto start = ValueStart(text, key);
    if (!start) {
        if (present) *present = false;
        return fallback;
    }
    if (present) *present = true;
    if (*start >= text.size() || !std::isdigit(static_cast<unsigned char>(text[*start]))) {
        invalid = true;
        return fallback;
    }
    std::size_t end = *start;
    while (end < text.size() && std::isdigit(static_cast<unsigned char>(text[end]))) ++end;
    try {
        const auto value = std::stoull(text.substr(*start, end - *start));
        if (value > 0xffffffffULL) throw std::out_of_range("uint");
        return static_cast<unsigned>(value);
    } catch (...) {
        invalid = true;
        return fallback;
    }
}

std::vector<std::string> ReadStringArray(const std::string& text, std::string_view key, bool& invalid) {
    std::vector<std::string> out;
    const auto startOpt = ValueStart(text, key);
    if (!startOpt) return out;
    std::size_t pos = *startOpt;
    if (pos >= text.size() || text[pos] != '[') { invalid = true; return {}; }
    ++pos;
    for (;;) {
        pos = SkipSpace(text, pos);
        if (pos >= text.size()) { invalid = true; return {}; }
        if (text[pos] == ']') return out;
        if (text[pos] != '"') { invalid = true; return {}; }
        ++pos;
        std::string value;
        bool closed = false;
        while (pos < text.size()) {
            const char ch = text[pos++];
            if (ch == '"') { closed = true; break; }
            if (ch != '\\') {
                value.push_back(ch);
                continue;
            }
            if (pos >= text.size()) { invalid = true; return {}; }
            const char esc = text[pos++];
            switch (esc) {
            case '"': value.push_back('"'); break;
            case '\\': value.push_back('\\'); break;
            case '/': value.push_back('/'); break;
            case 'b': value.push_back('\b'); break;
            case 'f': value.push_back('\f'); break;
            case 'n': value.push_back('\n'); break;
            case 'r': value.push_back('\r'); break;
            case 't': value.push_back('\t'); break;
            default: invalid = true; return {};
            }
        }
        if (!closed) { invalid = true; return {}; }
        out.push_back(std::move(value));
        pos = SkipSpace(text, pos);
        if (pos >= text.size()) { invalid = true; return {}; }
        if (text[pos] == ']') return out;
        if (text[pos] != ',') { invalid = true; return {}; }
        ++pos;
    }
}

std::string JsonEscape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (unsigned char ch : value) {
        switch (ch) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (ch < 0x20) {
                const char hex[] = "0123456789abcdef";
                out += "\\u00";
                out.push_back(hex[(ch >> 4) & 0x0f]);
                out.push_back(hex[ch & 0x0f]);
            } else {
                out.push_back(static_cast<char>(ch));
            }
        }
    }
    return out;
}

std::vector<std::wstring> NormalizeExclusions(const std::vector<std::wstring>& values) {
    std::set<std::wstring> unique;
    for (const auto& value : values) {
        const auto normalized = NormalizeExclusionPath(value);
        if (!normalized.empty()) unique.insert(normalized);
    }
    return {unique.begin(), unique.end()};
}

std::string Serialize(const AppSettings& source) {
    AppSettings settings = source;
    settings.cleanExclusions = NormalizeExclusions(settings.cleanExclusions);
    std::ostringstream out;
    out << "{\n"
        << "  \"schema_version\": " << settings.schemaVersion << ",\n"
        << "  \"confirm_destructive\": " << (settings.confirmDestructive ? "true" : "false") << ",\n"
        << "  \"large_file_mb\": " << settings.largeFileMB << ",\n"
        << "  \"duplicate_min_mb\": " << settings.duplicateMinMB << ",\n"
        << "  \"run_at_startup\": " << (settings.runAtStartup ? "true" : "false") << ",\n"
        << "  \"always_run_as_admin\": " << (settings.alwaysRunAsAdmin ? "true" : "false") << ",\n"
        << "  \"check_updates_at_startup\": " << (settings.checkUpdatesAtStartup ? "true" : "false") << ",\n"
        << "  \"quick_guard_at_startup\": " << (settings.quickGuardAtStartup ? "true" : "false") << ",\n"
        << "  \"check_update_cache_at_startup\": " << (settings.checkUpdateCacheAtStartup ? "true" : "false") << ",\n"
        << "  \"background_junk_monitor\": " << (settings.backgroundJunkMonitor ? "true" : "false") << ",\n"
        << "  \"tray_enabled\": " << (settings.trayEnabled ? "true" : "false") << ",\n"
        << "  \"close_behavior\": " << static_cast<int>(settings.closeBehavior) << ",\n"
        << "  \"memory_auto_trim_enabled\": " << (settings.memoryAutoTrimEnabled ? "true" : "false") << ",\n"
        << "  \"memory_auto_trim_percent\": " << settings.memoryAutoTrimPercent << ",\n"
        << "  \"memory_auto_trim_interval_minutes\": " << settings.memoryAutoTrimIntervalMinutes << ",\n"
        << "  \"memory_scope\": " << static_cast<int>(settings.memoryScope) << ",\n"
        << "  \"clean_exclusions\": [";
    for (std::size_t i = 0; i < settings.cleanExclusions.size(); ++i) {
        if (i) out << ", ";
        out << '"' << JsonEscape(WideToUtf8(settings.cleanExclusions[i])) << '"';
    }
    out << "]\n}\n";
    return out.str();
}

bool WriteAtomic(const fs::path& finalPath, const std::string& bytes, std::wstring& error) {
    const fs::path tempPath(finalPath.wstring() + L".tmp");
    HANDLE file = CreateFileW(tempPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        error = L"Не удалось открыть временный файл настроек. Код Windows: " + std::to_wstring(GetLastError());
        return false;
    }
    bool ok = true;
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(bytes.size() - offset, 1u << 20));
        DWORD written = 0;
        if (!WriteFile(file, bytes.data() + offset, chunk, &written, nullptr) || written != chunk) {
            ok = false;
            error = L"Не удалось записать временный файл настроек. Код Windows: " + std::to_wstring(GetLastError());
            break;
        }
        offset += written;
    }
    if (ok && !FlushFileBuffers(file)) {
        ok = false;
        error = L"Не удалось сбросить настройки на диск. Код Windows: " + std::to_wstring(GetLastError());
    }
    CloseHandle(file);
    if (!ok) {
        DeleteFileW(tempPath.c_str());
        return false;
    }

    if (!ReplaceFileW(finalPath.c_str(), tempPath.c_str(), nullptr,
                      REPLACEFILE_WRITE_THROUGH, nullptr, nullptr)) {
        const DWORD replaceError = GetLastError();
        if (!MoveFileExW(tempPath.c_str(), finalPath.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            const DWORD moveError = GetLastError();
            error = L"Не удалось атомарно сохранить настройки. Код Windows: " +
                    std::to_wstring(moveError ? moveError : replaceError);
            DeleteFileW(tempPath.c_str());
            return false;
        }
    }
    return true;
}

SettingsLoadResult DefaultsWithWarning(std::wstring warning) {
    SettingsLoadResult result{};
    result.settings = DefaultSettings();
    result.usedDefaults = true;
    result.warning = std::move(warning);
    return result;
}

} // namespace

AppSettings DefaultSettings() noexcept {
    return AppSettings{};
}

fs::path SettingsPath() {
    const auto testRoot = EnvWide(L"DPOP_SETTINGS_ROOT");
    if (!testRoot.empty()) return fs::path(testRoot) / L"settings.json";
    const auto local = EnvWide(L"LOCALAPPDATA");
    return local.empty() ? fs::path{} : fs::path(local) / L"DPopCleaner" / L"settings.json";
}

bool ValidateSettings(const AppSettings& settings, std::wstring& error) noexcept {
    error.clear();
    if (settings.largeFileMB < 50 || settings.largeFileMB > 4096) {
        error = L"Порог крупных файлов должен быть от 50 до 4096 МБ.";
        return false;
    }
    if (settings.duplicateMinMB < 1 || settings.duplicateMinMB > 1024) {
        error = L"Минимальный размер дубликатов должен быть от 1 до 1024 МБ.";
        return false;
    }
    if (settings.memoryAutoTrimPercent < 50 || settings.memoryAutoTrimPercent > 98) {
        error = L"Порог автоочистки памяти должен быть от 50 до 98%.";
        return false;
    }
    if (settings.memoryAutoTrimIntervalMinutes < 1 || settings.memoryAutoTrimIntervalMinutes > 1440) {
        error = L"Интервал автоочистки памяти должен быть от 1 до 1440 минут.";
        return false;
    }
    if (!settings.trayEnabled && settings.closeBehavior == CloseBehavior::MinimizeToTray) {
        error = L"Сворачивание в трей требует включённого значка в трее.";
        return false;
    }
    return true;
}

std::wstring NormalizeExclusionPath(const fs::path& path) {
    if (path.empty()) return {};
    std::error_code ec;
    auto normalizedPath = fs::weakly_canonical(path, ec);
    if (ec) normalizedPath = path.lexically_normal();
    std::wstring value = normalizedPath.wstring();
    std::replace(value.begin(), value.end(), L'/', L'\\');
    while (value.size() > 3 && !value.empty() && value.back() == L'\\') value.pop_back();
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(towlower(c));
    });
    return value;
}

bool IsExcludedPath(const fs::path& path, const AppSettings& settings) {
    const auto candidate = NormalizeExclusionPath(path);
    if (candidate.empty()) return false;
    for (const auto& raw : settings.cleanExclusions) {
        const auto excluded = NormalizeExclusionPath(raw);
        if (excluded.empty()) continue;
        if (candidate == excluded) return true;
        if (candidate.size() > excluded.size() && candidate.rfind(excluded + L"\\", 0) == 0) return true;
    }
    return false;
}

SettingsLoadResult LoadAppSettings() {
    const auto path = SettingsPath();
    if (path.empty()) return DefaultsWithWarning(L"LOCALAPPDATA недоступен; используются безопасные настройки по умолчанию.");

    const auto bytes = ReadFileUtf8(path);
    if (!bytes) {
        SettingsLoadResult result{};
        result.settings = DefaultSettings();
        result.usedDefaults = true;
        return result;
    }

    const std::string& text = *bytes;
    const auto first = SkipSpace(text, 0);
    std::size_t last = text.size();
    while (last > first && std::isspace(static_cast<unsigned char>(text[last - 1])) != 0) --last;
    if (first >= last || text[first] != '{' || text[last - 1] != '}') {
        return DefaultsWithWarning(L"settings.json повреждён; используются безопасные настройки по умолчанию.");
    }

    AppSettings settings = DefaultSettings();
    bool invalid = false;
    bool schemaPresent = false;
    const unsigned schema = ReadUInt(text, "schema_version", 1, invalid, &schemaPresent);
    if (schema == 0 || schema > 2) invalid = true;

    settings.confirmDestructive = ReadBool(text, "confirm_destructive", settings.confirmDestructive, invalid);
    settings.largeFileMB = ReadUInt(text, "large_file_mb", settings.largeFileMB, invalid);
    settings.duplicateMinMB = ReadUInt(text, "duplicate_min_mb", settings.duplicateMinMB, invalid);
    settings.runAtStartup = ReadBool(text, "run_at_startup", settings.runAtStartup, invalid);
    settings.alwaysRunAsAdmin = ReadBool(text, "always_run_as_admin", settings.alwaysRunAsAdmin, invalid);
    settings.checkUpdatesAtStartup = ReadBool(text, "check_updates_at_startup", settings.checkUpdatesAtStartup, invalid);
    settings.quickGuardAtStartup = ReadBool(text, "quick_guard_at_startup", settings.quickGuardAtStartup, invalid);
    settings.checkUpdateCacheAtStartup = ReadBool(text, "check_update_cache_at_startup", settings.checkUpdateCacheAtStartup, invalid);
    settings.backgroundJunkMonitor = ReadBool(text, "background_junk_monitor", settings.backgroundJunkMonitor, invalid);
    settings.trayEnabled = ReadBool(text, "tray_enabled", settings.trayEnabled, invalid);
    settings.memoryAutoTrimEnabled = ReadBool(text, "memory_auto_trim_enabled", settings.memoryAutoTrimEnabled, invalid);
    settings.memoryAutoTrimPercent = ReadUInt(text, "memory_auto_trim_percent", settings.memoryAutoTrimPercent, invalid);
    settings.memoryAutoTrimIntervalMinutes = ReadUInt(text, "memory_auto_trim_interval_minutes", settings.memoryAutoTrimIntervalMinutes, invalid);

    const unsigned close = ReadUInt(text, "close_behavior", static_cast<unsigned>(settings.closeBehavior), invalid);
    if (close > static_cast<unsigned>(CloseBehavior::Ask)) invalid = true;
    else settings.closeBehavior = static_cast<CloseBehavior>(close);

    const unsigned scope = ReadUInt(text, "memory_scope", static_cast<unsigned>(settings.memoryScope), invalid);
    if (scope > static_cast<unsigned>(MemoryScope::Advanced)) invalid = true;
    else settings.memoryScope = static_cast<MemoryScope>(scope);

    const auto exclusionUtf8 = ReadStringArray(text, "clean_exclusions", invalid);
    for (const auto& item : exclusionUtf8) {
        auto wide = Utf8ToWide(item);
        if (wide.empty() && !item.empty()) { invalid = true; break; }
        settings.cleanExclusions.push_back(std::move(wide));
    }
    settings.cleanExclusions = NormalizeExclusions(settings.cleanExclusions);

    const bool migrated = !schemaPresent || schema < 2;
    if (migrated) {
        const bool oldMinimizeToTray = ReadBool(text, "minimize_to_tray", false, invalid);
        if (oldMinimizeToTray) {
            settings.trayEnabled = true;
            settings.closeBehavior = CloseBehavior::MinimizeToTray;
        }
    }
    settings.schemaVersion = 2;

    std::wstring validationError;
    if (invalid || !ValidateSettings(settings, validationError)) {
        std::wstring warning = L"settings.json содержит некорректные значения; используются безопасные настройки по умолчанию.";
        if (!validationError.empty()) warning += L" " + validationError;
        return DefaultsWithWarning(std::move(warning));
    }

    SettingsLoadResult result{};
    result.settings = std::move(settings);
    result.migrated = migrated;
    return result;
}

bool SaveAppSettings(const AppSettings& source, std::wstring& error) {
    error.clear();
    AppSettings settings = source;
    settings.schemaVersion = 2;
    settings.cleanExclusions = NormalizeExclusions(settings.cleanExclusions);
    if (!ValidateSettings(settings, error)) return false;

    const auto path = SettingsPath();
    if (path.empty()) {
        error = L"LOCALAPPDATA недоступен.";
        return false;
    }
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    if (ec) {
        error = L"Не удалось создать папку настроек.";
        return false;
    }
    return WriteAtomic(path, Serialize(settings), error);
}

} // namespace dpop::settings
