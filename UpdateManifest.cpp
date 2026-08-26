#include "update/UpdateManifest.h"
#include <windows.h>
#include <regex>
#include <optional>

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
    return std::stoll(m[1].str());
}

std::optional<bool> BoolField(const std::string& json, const char* key) {
    const std::regex re(std::string("\\\"") + key + "\\\"\\s*:\\s*(true|false)");
    std::smatch m;
    if (!std::regex_search(json, m, re)) return std::nullopt;
    return m[1].str() == "true";
}
}

namespace dpop::update {
bool ParseManifestUtf8(const std::string& json, Manifest& out, std::wstring& error) {
    const auto product = StringField(json, "product");
    const auto channel = StringField(json, "channel");
    const auto version = StringField(json, "version");
    const auto versionCode = IntField(json, "version_code");
    const auto url = StringField(json, "download_url");
    const auto hash = StringField(json, "sha256");
    if (!version || !versionCode || !url || !hash) {
        error = L"Манифест обновления не содержит обязательных полей.";
        return false;
    }
    out.product = Utf8ToWide(product.value_or("DPopCleaner"));
    out.channel = Utf8ToWide(channel.value_or("beta"));
    out.version = Utf8ToWide(*version);
    out.versionCode = static_cast<int>(*versionCode);
    out.mandatory = BoolField(json, "mandatory").value_or(false);
    out.downloadUrl = Utf8ToWide(*url);
    out.sha256 = Utf8ToWide(*hash);
    out.size = static_cast<std::uint64_t>(IntField(json, "size").value_or(0));
    out.signedPackage = BoolField(json, "signed").value_or(false);
    out.releaseNotesUrl = Utf8ToWide(StringField(json, "notes_url").value_or(""));
    out.installArgs = Utf8ToWide(StringField(json, "install_args").value_or(""));
    return true;
}
}
