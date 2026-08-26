#pragma once
#include <filesystem>
#include <string>

namespace dpop::zapret {
struct Status {
    bool serviceInstalled{};
    bool serviceRunning{};
    bool winwsRunning{};
    std::filesystem::path detectedFolder;
};
Status QueryStatus();
bool OpenDetectedFolder(std::wstring& error);
}
