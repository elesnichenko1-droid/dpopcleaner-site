#include "AppSettings.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>

namespace dpop0418 {
namespace {

std::string Trim(std::string value) {
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::wstring Win32Error(const wchar_t* prefix) {
    return std::wstring(prefix) + L" (код " + std::to_wstring(GetLastError()) + L")";
}

std::wstring FromUtf8(const std::string& value) {
    if (value.empty()) return {};
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                          static_cast<int>(value.size()), nullptr, 0);
    if (count <= 0) return {};
    std::wstring result(static_cast<size_t>(count), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                            static_cast<int>(value.size()), result.data(), count) != count) return {};
    return result;
}

std::string ToUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                                          static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (count <= 0) return {};
    std::string result(static_cast<size_t>(count), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                            static_cast<int>(value.size()), result.data(), count,
                            nullptr, nullptr) != count) return {};
    return result;
}

std::wstring SafeStrategyName(const std::wstring& value) {
    if (value.empty() || value.find(L'\r') != std::wstring::npos || value.find(L'\n') != std::wstring::npos)
        return L"general.bat";
    return value;
}

} // namespace

AppSettings LoadSettings(const std::filesystem::path& path) {
    AppSettings settings{};
    std::ifstream input(path, std::ios::binary);
    if (!input) return settings;

    enum class Section { None, Updates, Zapret };
    Section section = Section::None;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed.front() == ';' || trimmed.front() == '#') continue;
        if (trimmed.front() == '[' && trimmed.back() == ']') {
            const std::string name = Trim(trimmed.substr(1, trimmed.size() - 2));
            if (name == "updates") section = Section::Updates;
            else if (name == "zapret") section = Section::Zapret;
            else section = Section::None;
            continue;
        }

        const auto eq = trimmed.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = Trim(trimmed.substr(0, eq));
        const std::string value = Trim(trimmed.substr(eq + 1));

        if (section == Section::Updates && key == "auto_check") {
            if (value == "0") settings.autoCheckUpdates = false;
            else if (value == "1") settings.autoCheckUpdates = true;
            else settings.autoCheckUpdates = true;
        } else if (section == Section::Zapret && key == "strategy") {
            const std::wstring decoded = FromUtf8(value);
            settings.zapretStrategy = SafeStrategyName(decoded);
        }
    }
    return settings;
}

bool SaveSettingsAtomic(const std::filesystem::path& path, const AppSettings& settings, std::wstring& error) {
    error.clear();
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            error = L"Не удалось создать папку настроек (код " + std::to_wstring(ec.value()) + L")";
            return false;
        }
    }

    const std::filesystem::path temp = path.wstring() + L".tmp";
    const std::wstring strategy = SafeStrategyName(settings.zapretStrategy);
    const std::string strategyUtf8 = ToUtf8(strategy);
    if (strategyUtf8.empty()) {
        error = L"Не удалось кодировать выбранную стратегию Zapret";
        return false;
    }
    const std::string bytes = std::string("[updates]\r\nauto_check=") +
                              (settings.autoCheckUpdates ? "1" : "0") +
                              "\r\n\r\n[zapret]\r\nstrategy=" + strategyUtf8 + "\r\n";

    HANDLE file = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        error = Win32Error(L"Не удалось создать временный файл настроек");
        return false;
    }

    DWORD written = 0;
    const BOOL wrote = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
    const BOOL flushed = wrote && written == bytes.size() && FlushFileBuffers(file);
    CloseHandle(file);

    if (!flushed) {
        DeleteFileW(temp.c_str());
        error = L"Не удалось безопасно записать настройки";
        return false;
    }

    if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = Win32Error(L"Не удалось заменить файл настроек");
        DeleteFileW(temp.c_str());
        return false;
    }
    return true;
}

} // namespace dpop0418
