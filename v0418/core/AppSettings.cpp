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

} // namespace

AppSettings LoadSettings(const std::filesystem::path& path) {
    AppSettings settings{};
    std::ifstream input(path, std::ios::binary);
    if (!input) return settings;

    bool inUpdates = false;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed.front() == ';' || trimmed.front() == '#') continue;
        if (trimmed.front() == '[' && trimmed.back() == ']') {
            inUpdates = Trim(trimmed.substr(1, trimmed.size() - 2)) == "updates";
            continue;
        }
        if (!inUpdates) continue;
        const auto eq = trimmed.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = Trim(trimmed.substr(0, eq));
        const std::string value = Trim(trimmed.substr(eq + 1));
        if (key != "auto_check") continue;
        if (value == "0") settings.autoCheckUpdates = false;
        else if (value == "1") settings.autoCheckUpdates = true;
        else settings.autoCheckUpdates = true;
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
    const std::string bytes = std::string("[updates]\r\nauto_check=") +
                              (settings.autoCheckUpdates ? "1" : "0") + "\r\n";

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
