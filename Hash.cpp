#include "update/Hash.h"
#include <windows.h>
#include <bcrypt.h>
#include <vector>
#include <sstream>
#include <iomanip>

namespace dpop::update {
bool Sha256File(const std::filesystem::path& file, std::wstring& hex, std::wstring& error) {
    BCRYPT_ALG_HANDLE alg{};
    BCRYPT_HASH_HANDLE hash{};
    HANDLE h = INVALID_HANDLE_VALUE;
    std::vector<UCHAR> object;
    std::vector<UCHAR> digest;
    bool ok = false;

    do {
        if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) { error=L"BCryptOpenAlgorithmProvider failed"; break; }
        DWORD objLen=0, cb=0, hashLen=0;
        if (BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objLen), sizeof(objLen), &cb, 0) < 0) break;
        if (BCryptGetProperty(alg, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashLen), sizeof(hashLen), &cb, 0) < 0) break;
        object.resize(objLen); digest.resize(hashLen);
        if (BCryptCreateHash(alg, &hash, object.data(), objLen, nullptr, 0, 0) < 0) break;

        h = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (h == INVALID_HANDLE_VALUE) { error=L"Не удалось открыть скачанный файл"; break; }
        std::vector<UCHAR> buf(1024*1024);
        bool readOk = true;
        for (;;) {
            DWORD read=0;
            if (!ReadFile(h, buf.data(), static_cast<DWORD>(buf.size()), &read, nullptr)) { readOk=false; break; }
            if (!read) break;
            if (BCryptHashData(hash, buf.data(), read, 0) < 0) { readOk=false; break; }
        }
        if (!readOk) { error=L"Не удалось прочитать файл для SHA-256"; break; }
        if (BCryptFinishHash(hash, digest.data(), hashLen, 0) < 0) break;

        std::wostringstream ss;
        ss << std::hex << std::setfill(L'0');
        for (auto b : digest) ss << std::setw(2) << static_cast<unsigned>(b);
        hex = ss.str();
        ok = true;
    } while(false);

    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    if (hash) BCryptDestroyHash(hash);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    if (!ok && error.empty()) error = L"Не удалось вычислить SHA-256";
    return ok;
}
}
