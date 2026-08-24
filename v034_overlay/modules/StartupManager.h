#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace dpop::startup {

struct Entry {
    std::wstring name;
    std::wstring command;
    std::wstring source;
    std::filesystem::path location;
    std::filesystem::path executable;
    std::wstring category;
    std::wstring recommendation;
    bool protectedEntry{false};
    bool manageable{false};
    bool enabled{true};
    bool from32BitView{false};
};

std::vector<Entry> EnumerateAll();
std::vector<Entry> EnumerateCurrentUserRun();
bool SetEnabled(const Entry& entry, bool enabled, std::wstring& error);

}
