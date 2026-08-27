#include "UpdatePolicy.h"

namespace dpop0418 {

bool IsRemoteNewer(VersionIdentity local, VersionIdentity remote) {
    if (remote.versionCode != local.versionCode) return remote.versionCode > local.versionCode;
    return remote.revision > local.revision;
}

} // namespace dpop0418
