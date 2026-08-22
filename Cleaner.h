#pragma once
#include <cstdint>

namespace dpop::cleaner {
std::uint64_t EstimateUserTempBytes();
struct Result { std::uint64_t removedBytes{}; unsigned removedFiles{}; unsigned failedFiles{}; };
Result CleanUserTemp();
}
