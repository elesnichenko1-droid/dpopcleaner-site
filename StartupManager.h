#pragma once
#include <string>
#include <vector>
#include <filesystem>

namespace dpop::startup {
struct Entry {
    std::wstring name;
    std::wstring command;
    std::wstring source;
    std::filesystem::path location;
};
std::vector<Entry> EnumerateAll();
std::vector<Entry> EnumerateCurrentUserRun();
}
