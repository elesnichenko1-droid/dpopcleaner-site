#pragma once
#include <cstdint>
#include <string>

namespace dpop::update {
struct Manifest {
    std::wstring product;
    std::wstring channel;
    std::wstring version;
    int versionCode{};
    bool mandatory{};
    std::wstring downloadUrl;
    std::wstring sha256;
    std::uint64_t size{};
    bool signedPackage{};
    std::wstring releaseNotesUrl;
    std::wstring installArgs;
};

bool ParseManifestUtf8(const std::string& json, Manifest& out, std::wstring& error);
}
