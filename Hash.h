#pragma once
#include <filesystem>
#include <string>

namespace dpop::update {
bool Sha256File(const std::filesystem::path& file, std::wstring& hex, std::wstring& error);
}
