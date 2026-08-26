#pragma once

#include <filesystem>
#include <string>

namespace dpop0418 {
bool VerifyAuthenticode(const std::filesystem::path& file, std::wstring& error);
}
