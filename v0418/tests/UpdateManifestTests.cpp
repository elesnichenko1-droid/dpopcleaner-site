#include "UpdateManifest.h"

#include <iostream>
#include <string>

namespace {
int Fail(const char* message) {
    std::cerr << message << "\n";
    return 1;
}

std::string ValidJson() {
    return R"json({
  "product": "DPopCleaner",
  "channel": "stable",
  "version": "0.4.19",
  "version_code": 419,
  "revision": 1,
  "available": true,
  "mandatory": false,
  "download_url": "https://example.invalid/DPopCleaner_Setup_0.4.19.exe",
  "sha256": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
  "size": 123456,
  "signed": false,
  "notes_url": "https://example.invalid/notes",
  "install_args": "/SILENT /NORESTART"
})json";
}
}

int main() {
    dpop0418::UpdateManifest manifest{};
    std::wstring error;
    if (!dpop0418::ParseUpdateManifestUtf8(ValidJson(), manifest, error))
        return Fail("valid manifest must parse");
    if (!dpop0418::IsUsableStableManifest(manifest, error))
        return Fail("valid stable manifest must be usable");
    if (manifest.versionCode != 419 || manifest.revision != 1 || manifest.size != 123456)
        return Fail("manifest numeric fields must parse exactly");

    auto invalidHash = manifest;
    invalidHash.sha256 = L"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    if (dpop0418::IsUsableStableManifest(invalidHash, error))
        return Fail("63-character SHA-256 must fail closed");

    auto http = manifest;
    http.downloadUrl = L"http://example.invalid/setup.exe";
    if (dpop0418::IsUsableStableManifest(http, error))
        return Fail("non-HTTPS download URL must fail closed");

    auto unavailable = manifest;
    unavailable.available = false;
    if (dpop0418::IsUsableStableManifest(unavailable, error))
        return Fail("available=false must not be installable");

    auto wrongChannel = manifest;
    wrongChannel.channel = L"beta";
    if (dpop0418::IsUsableStableManifest(wrongChannel, error))
        return Fail("non-stable channel must fail closed");

    std::string missingProduct = ValidJson();
    const std::string productNeedle = "  \"product\": \"DPopCleaner\",\n";
    missingProduct.erase(missingProduct.find(productNeedle), productNeedle.size());
    dpop0418::UpdateManifest parsedMissingProduct{};
    if (dpop0418::ParseUpdateManifestUtf8(missingProduct, parsedMissingProduct, error))
        return Fail("missing product must reject manifest");

    std::string missingChannel = ValidJson();
    const std::string channelNeedle = "  \"channel\": \"stable\",\n";
    missingChannel.erase(missingChannel.find(channelNeedle), channelNeedle.size());
    dpop0418::UpdateManifest parsedMissingChannel{};
    if (dpop0418::ParseUpdateManifestUtf8(missingChannel, parsedMissingChannel, error))
        return Fail("missing channel must reject manifest");

    std::string missingHash = ValidJson();
    const std::string hashNeedle = "  \"sha256\": \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\",\n";
    missingHash.erase(missingHash.find(hashNeedle), hashNeedle.size());
    dpop0418::UpdateManifest parsedMissingHash{};
    if (dpop0418::ParseUpdateManifestUtf8(missingHash, parsedMissingHash, error))
        return Fail("missing SHA-256 must reject manifest");

    return 0;
}
