#pragma once
#include <filesystem>

namespace dpop::paths {
std::filesystem::path LocalAppData();
std::filesystem::path DataDir();
std::filesystem::path LogsDir();
std::filesystem::path UpdatesDir();
std::filesystem::path ExecutableDir();
void EnsureDirectories();
}
