#include "Hash.h"
#include "UpdateManifest.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace {
int Fail(const char* message) {
    std::cerr << message << "\n";
    return 1;
}
}

int main() {
    const fs::path root = fs::temp_directory_path() / L"dpop0418-package-tests";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    if (ec) return Fail("cannot create temp directory");

    const fs::path file = root / L"package.exe";
    {
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        out << "DPopCleaner 0.4.18 package verification fixture";
    }

    dpop0418::UpdateManifest manifest{};
    manifest.product = L"DPopCleaner";
    manifest.channel = L"stable";
    manifest.version = L"0.4.19";
    manifest.versionCode = 419;
    manifest.revision = 1;
    manifest.available = true;
    manifest.downloadUrl = L"https://example.invalid/setup.exe";
    manifest.size = fs::file_size(file);

    std::wstring error;
    if (!dpop0418::Sha256File(file, manifest.sha256, error))
        return Fail("fixture SHA-256 must be computable");
    if (!dpop0418::VerifyPackageFile(file, manifest, error))
        return Fail("correct size and SHA-256 must verify");

    auto wrongSize = manifest;
    wrongSize.size += 1;
    if (dpop0418::VerifyPackageFile(file, wrongSize, error))
        return Fail("wrong byte size must reject package");

    auto wrongHash = manifest;
    wrongHash.sha256.assign(64, L'0');
    if (dpop0418::VerifyPackageFile(file, wrongHash, error))
        return Fail("wrong SHA-256 must reject package");

    fs::remove_all(root, ec);
    return 0;
}
