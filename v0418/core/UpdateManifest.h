#pragma once

#include <cstdint>
#include <string>

namespace dpop0418 {

struct UpdateManifest {
    std::wstring product;
    std::wstring channel;
    std::wstring version;
    int versionCode{};
    int revision{};
    bool available{};
    bool mandatory{};
    std::wstring downloadUrl;
    std::wstring sha256;
    std::uint64_t size{};
    bool signedPackage{};
    std::wstring notesUrl;
    std::wstring installArgs;
};

bool ParseUpdateManifestUtf8(const std::string& json, UpdateManifest& out, std::wstring& error);
bool IsUsableStableManifest(const UpdateManifest& manifest, std::wstring& error);

} // namespace dpop0418
