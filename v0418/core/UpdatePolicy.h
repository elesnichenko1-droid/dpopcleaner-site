#pragma once

namespace dpop0418 {

struct VersionIdentity {
    int versionCode{};
    int revision{};
};

bool IsRemoteNewer(VersionIdentity local, VersionIdentity remote);

} // namespace dpop0418
