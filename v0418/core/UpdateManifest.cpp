#include "UpdateManifest.h"

#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <optional>
#include <regex>
#include <string>

namespace dpop0418 {
namespace {

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) return {};
    const int needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                            static_cast<int>(value.size()), nullptr, 0);
    if (needed <= 0) return {};
    std::wstring wide(static_cast<std::size_t>(needed), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                            static_cast<int>(value.size()), wide.data(), needed) <= 0) {
        return {};
    }
    return wide;
}

std::optional<std::string> StringField(const std::string& json, const char* key) {
    const std::regex re(std::string("\\\"") + key + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
    std::smatch match;
    if (!std::regex_search(json, match, re)) return std::nullopt;
    return match[1].str();
}

std::optional<long long> IntField(const std::string& json, const char* key) {
    const std::regex re(std::string("\\\"") + key + "\\\"\\s*:\\s*([0-9]+)");
    std::smatch match;
    if (!std::regex_search(json, match, re)) return std::nullopt;
    try {
        return std::stoll(match[1].str());
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<bool> BoolField(const std::string& json, const char* key) {
    const std::regex re(std::string("\\\"") + key + "\\\"\\s*:\\s*(true|false)");
    std::smatch match;
    if (!std::regex_search(json, match, re)) return std::nullopt;
    return match[1].str() == "true";
}

bool IsHex64(const std::wstring& value) {
    return value.size() == 64 &&
           std::all_of(value.begin(), value.end(), [](wchar_t ch) {
               return (ch >= L'0' && ch <= L'9') || (ch >= L'a' && ch <= L'f') ||
                      (ch >= L'A' && ch <= L'F');
           });
}

bool StartsWithHttps(const std::wstring& value) {
    if (value.size() < 8) return false;
    std::wstring prefix = value.substr(0, 8);
    std::transform(prefix.begin(), prefix.end(), prefix.begin(), ::towlower);
    return prefix == L"https://";
}

} // namespace

bool ParseUpdateManifestUtf8(const std::string& json, UpdateManifest& out, std::wstring& error) {
    error.clear();
    const auto version = StringField(json, "version");
    const auto versionCode = IntField(json, "version_code");
    const auto revision = IntField(json, "revision");
    const auto available = BoolField(json, "available");
    const auto downloadUrl = StringField(json, "download_url");
    const auto sha256 = StringField(json, "sha256");
    const auto size = IntField(json, "size");

    if (!version || !versionCode || !revision || !available || !downloadUrl || !sha256 || !size) {
        error = L"Манифест обновления не содержит обязательных полей.";
        return false;
    }
    if (*versionCode > INT_MAX || *revision > INT_MAX || *size < 0) {
        error = L"Числовые поля манифеста выходят за допустимый диапазон.";
        return false;
    }

    out = {};
    out.product = Utf8ToWide(StringField(json, "product").value_or("DPopCleaner"));
    out.channel = Utf8ToWide(StringField(json, "channel").value_or("stable"));
    out.version = Utf8ToWide(*version);
    out.versionCode = static_cast<int>(*versionCode);
    out.revision = static_cast<int>(*revision);
    out.available = *available;
    out.mandatory = BoolField(json, "mandatory").value_or(false);
    out.downloadUrl = Utf8ToWide(*downloadUrl);
    out.sha256 = Utf8ToWide(*sha256);
    out.size = static_cast<std::uint64_t>(*size);
    out.signedPackage = BoolField(json, "signed").value_or(false);
    out.notesUrl = Utf8ToWide(StringField(json, "notes_url").value_or(""));
    out.installArgs = Utf8ToWide(StringField(json, "install_args").value_or(""));

    if (out.version.empty() || out.downloadUrl.empty() || out.sha256.empty()) {
        error = L"Манифест содержит некорректный UTF-8 или пустые обязательные поля.";
        return false;
    }
    return true;
}

bool IsUsableStableManifest(const UpdateManifest& manifest, std::wstring& error) {
    error.clear();
    if (!manifest.product.empty() && manifest.product != L"DPopCleaner") {
        error = L"Манифест предназначен для другого продукта.";
        return false;
    }
    if (manifest.channel != L"stable") {
        error = L"Поддерживается только stable-канал обновлений.";
        return false;
    }
    if (!manifest.available) {
        error = L"Релиз в манифесте пока недоступен.";
        return false;
    }
    if (manifest.versionCode <= 0 || manifest.revision < 1) {
        error = L"Некорректная версия или revision в манифесте.";
        return false;
    }
    if (!StartsWithHttps(manifest.downloadUrl)) {
        error = L"download_url должен использовать HTTPS.";
        return false;
    }
    if (!IsHex64(manifest.sha256)) {
        error = L"Некорректный SHA-256 в манифесте.";
        return false;
    }
    if (manifest.size == 0) {
        error = L"Размер пакета в манифесте должен быть больше нуля.";
        return false;
    }
    return true;
}

} // namespace dpop0418
