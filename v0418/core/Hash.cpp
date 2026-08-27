#include "Hash.h"

#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <cwctype>
#include <iomanip>
#include <sstream>
#include <vector>

namespace dpop0418 {
namespace {

bool EqualsNoCase(std::wstring left, std::wstring right) {
    std::transform(left.begin(), left.end(), left.begin(), ::towlower);
    std::transform(right.begin(), right.end(), right.begin(), ::towlower);
    return left == right;
}

} // namespace

bool Sha256File(const std::filesystem::path& file, std::wstring& hex, std::wstring& error) {
    error.clear();
    hex.clear();
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    HANDLE input = INVALID_HANDLE_VALUE;
    std::vector<UCHAR> object;
    std::vector<UCHAR> digest;
    bool ok = false;

    do {
        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) {
            error = L"BCryptOpenAlgorithmProvider failed";
            break;
        }
        DWORD objectLength = 0;
        DWORD hashLength = 0;
        DWORD received = 0;
        if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                              reinterpret_cast<PUCHAR>(&objectLength), sizeof(objectLength), &received, 0) < 0) break;
        if (BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH,
                              reinterpret_cast<PUCHAR>(&hashLength), sizeof(hashLength), &received, 0) < 0) break;
        object.resize(objectLength);
        digest.resize(hashLength);
        if (BCryptCreateHash(algorithm, &hash, object.data(), objectLength, nullptr, 0, 0) < 0) break;

        input = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                            FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (input == INVALID_HANDLE_VALUE) {
            error = L"Не удалось открыть скачанный файл";
            break;
        }

        std::vector<UCHAR> buffer(1024 * 1024);
        for (;;) {
            DWORD read = 0;
            if (!ReadFile(input, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr)) {
                error = L"Не удалось прочитать файл для SHA-256";
                break;
            }
            if (read == 0) {
                if (BCryptFinishHash(hash, digest.data(), hashLength, 0) < 0) {
                    error = L"Не удалось завершить SHA-256";
                    break;
                }
                std::wostringstream stream;
                stream << std::hex << std::setfill(L'0');
                for (const auto byte : digest) stream << std::setw(2) << static_cast<unsigned>(byte);
                hex = stream.str();
                ok = true;
                break;
            }
            if (BCryptHashData(hash, buffer.data(), read, 0) < 0) {
                error = L"Не удалось обработать данные SHA-256";
                break;
            }
        }
    } while (false);

    if (input != INVALID_HANDLE_VALUE) CloseHandle(input);
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    if (!ok && error.empty()) error = L"Не удалось вычислить SHA-256";
    return ok;
}

bool VerifyPackageFile(const std::filesystem::path& file, const UpdateManifest& manifest, std::wstring& error) {
    error.clear();
    std::error_code ec;
    const auto size = std::filesystem::file_size(file, ec);
    if (ec) {
        error = L"Не удалось определить размер пакета (код " + std::to_wstring(ec.value()) + L")";
        return false;
    }
    if (size != manifest.size) {
        error = L"Размер скачанного пакета не совпадает с манифестом.";
        return false;
    }

    std::wstring actualHash;
    if (!Sha256File(file, actualHash, error)) return false;
    if (!EqualsNoCase(actualHash, manifest.sha256)) {
        error = L"SHA-256 скачанного файла не совпадает с манифестом.";
        return false;
    }
    return true;
}

} // namespace dpop0418
