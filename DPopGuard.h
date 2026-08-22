#pragma once
#include <string>
#include <vector>

namespace dpop::guard {
struct Finding { std::wstring title; std::wstring details; };
struct ScanResult { std::vector<Finding> findings; std::wstring note; };
ScanResult QuickScan();
}
