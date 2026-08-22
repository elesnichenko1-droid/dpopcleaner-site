#pragma once
#include <filesystem>
#include <string>

namespace dpop::update {
bool VerifyAuthenticode(const std::filesystem::path& file, std::wstring& error);
}
