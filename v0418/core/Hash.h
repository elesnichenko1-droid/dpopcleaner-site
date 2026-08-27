#pragma once

#include "UpdateManifest.h"

#include <filesystem>
#include <string>

namespace dpop0418 {

bool Sha256File(const std::filesystem::path& file, std::wstring& hex, std::wstring& error);
bool VerifyPackageFile(const std::filesystem::path& file, const UpdateManifest& manifest, std::wstring& error);

} // namespace dpop0418
