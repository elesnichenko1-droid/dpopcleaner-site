#include "UpdatePolicy.h"

#include <iostream>

namespace {
int Fail(const char* message) {
    std::cerr << message << "\n";
    return 1;
}
}

int main() {
    using dpop0418::IsRemoteNewer;
    using dpop0418::VersionIdentity;

    if (!IsRemoteNewer(VersionIdentity{418, 1}, VersionIdentity{419, 1}))
        return Fail("greater remote version code must be newer");
    if (!IsRemoteNewer(VersionIdentity{418, 1}, VersionIdentity{418, 2}))
        return Fail("equal version code with greater revision must be newer");
    if (IsRemoteNewer(VersionIdentity{418, 1}, VersionIdentity{417, 99}))
        return Fail("lower version code must never be newer even with greater revision");
    if (IsRemoteNewer(VersionIdentity{418, 1}, VersionIdentity{418, 1}))
        return Fail("equal identity must not be newer");
    if (IsRemoteNewer(VersionIdentity{418, 2}, VersionIdentity{418, 1}))
        return Fail("older revision must not be newer");

    return 0;
}
