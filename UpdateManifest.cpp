#include "update/UpdateManifest.h"
#include <windows.h>
#include <regex>
#include <optional>
#include <algorithm>
#include <exception>

namespace {
std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) return {};
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

std::optional<std::string> StringField(const std::string& json, const char* key) {
    const std::regex re(std::string("\\\"") + key + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
    std::smatch m;
    if (!std::regex_search(json, m, re)) return std::nullopt;
    return m[1].str();
}

std::optional<long long> IntField(const std::string& json, const char* key) {
    const std::regex re(std::string("\\\"") + key + "\\\"\\s*:\\s*([0-9]+)");
    std::smatch m;
    if (!std::regex_search(json, m, re)) return std::nullopt;
    try {
        return std::stoll(m[1].str());
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<bool> BoolField(const std::string& json, const char* key) {
    const std::regex re(std::string("\\\"") + key + "\\\"\\s*:\\s*(true|false)");
    std::smatch m;
    if (!std::regex_search(json, m, re)) return std::nullopt;
    return m[1].str() == "true";
}

bool IsHttpsUrl(const std::string& value) {
    return value.starts_with("https://") && value.size() > 8 && value.find_first_of("\r\n\t ") == std::string::npos;
}

bool IsSha256(const std::string& value) {
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
    });
}
}

namespace dpop::update {
bool ParseManifestUtf8(const std::string& json, Manifest& out, std::wstring& error) {
    const auto product = StringField(json, "product");
    const auto channel = StringField(json, "channel");
    const auto version = StringField(json, "version");
    const auto versionCode = IntField(json, "version_code");
    const auto revision = IntField(json, "revision");
    const auto available = BoolField(json, "available");
    const auto url = StringField(json, "download_url");
    const auto hash = StringField(json, "sha256");
    const auto size = IntField(json, "size");
    if (!version || !versionCode || !available) {
        error = L"Манифест обновления не содержит обязательных полей.";
        return false;
    }
    if (*available && (!url || !hash || !size || *size <= 0 || !IsHttpsUrl(*url) || !IsSha256(*hash))) {
        error = L"Доступный пакет обновления не прошёл проверку HTTPS, SHA-256 и размера.";
        return false;
    }
    out.product = Utf8ToWide(product.value_or("DPopCleaner"));
    out.channel = Utf8ToWide(channel.value_or("beta"));
    out.version = Utf8ToWide(*version);
    out.versionCode = static_cast<int>(*versionCode);
    out.revision = static_cast<int>(revision.value_or(0));
    out.mandatory = BoolField(json, "mandatory").value_or(false);
    out.downloadUrl = Utf8ToWide(url.value_or(""));
    out.sha256 = Utf8ToWide(hash.value_or(""));
    out.size = static_cast<std::uint64_t>(size.value_or(0));
    out.signedPackage = BoolField(json, "signed").value_or(false);
    out.available = *available;
    out.releaseNotesUrl = Utf8ToWide(StringField(json, "notes_url").value_or(""));
    out.installArgs = Utf8ToWide(StringField(json, "install_args").value_or(""));
    return true;
}
}
