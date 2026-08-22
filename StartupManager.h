#pragma once
#include <string>
#include <vector>

namespace dpop::startup {
struct Entry { std::wstring name; std::wstring command; };
std::vector<Entry> EnumerateCurrentUserRun();
}
