#pragma once

#include "modules/DiskAnalyzer.h"
#include "modules/FullCore.h"
#include "ui/PageBase.h"
#include "ui/RecoveryControls.h"
#include "ui/controls/DiskTreeList.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace dpop::ui {

class DiskPage final : public PageBase {
public:
    ~DiskPage() override { Destroy(); }
    bool Create(HWND parent, SessionLog& log) {
        return PageBase::Create(parent, log, L"Диск");
    }

protected:
    bool OnCreate() override;
    void OnLayout(int width, int height) noexcept override;
    LRESULT OnMessage(UINT message, WPARAM wParam, LPARAM lParam, bool& handled) override;
    void OnPaint(HDC dc, const RECT& client) noexcept override;
    void OnVisibilityChanged(bool visible) noexcept override;

private:
    static std::filesystem::path InitialRoot() {
        // Opt-in release-gate hook. Normal users do not have this variable,
        // so the production default remains C:\. It changes only the initial
        // analyzer root and never enables destructive behavior.
        constexpr DWORD kCapacity = 32768;
        wchar_t buffer[kCapacity]{};
        const DWORD count = GetEnvironmentVariableW(L"DPOP_DISK_TEST_ROOT", buffer, kCapacity);
        if (count > 0 && count < kCapacity) {
            return std::filesystem::path(buffer).lexically_normal();
        }
        return std::filesystem::path(L"C:\\");
    }

    void StartScan(const std::filesystem::path& root, bool addHistory = true);
    void ApplySnapshot(dpop::disk::DiskScanSnapshot snapshot, bool finalSnapshot);
    void BuildVisibleRows();
    void AppendVisibleNode(dpop::disk::DiskNodeId id, unsigned depth, std::vector<DiskTreeRow>& rows) const;
    std::vector<dpop::disk::DiskNodeId> SortedChildren(const dpop::disk::DiskNode& parent) const;
    void ToggleExpanded(dpop::disk::DiskNodeId id);
    void ShowLargeFiles();
    void ShowTree();
    void ChooseRoot();
    void GoBack();
    void OpenExplorer();
    void HandleColumnClick(int column);
    std::wstring CacheKey(const std::filesystem::path& root) const;

    RecoveryFonts fonts_;
    std::array<HWND, 8> toolbar_{};
    HWND pathEdit_{};
    DiskTreeList tree_;

    std::filesystem::path root_{InitialRoot()};
    dpop::disk::DiskScanSnapshot snapshot_;
    std::unordered_set<dpop::disk::DiskNodeId> expanded_;
    std::unordered_map<std::wstring, dpop::disk::DiskScanSnapshot> cache_;
    std::vector<std::filesystem::path> history_;
    int historyIndex_{-1};
    std::uint64_t scanGeneration_{};

    dpop::full::Settings settings_{};
    bool scanning_{false};
    bool largeMode_{false};
    int sortColumn_{1};
    bool sortAscending_{false};
    std::wstring summary_{L"Анализатор диска готов."};
};

} // namespace dpop::ui
