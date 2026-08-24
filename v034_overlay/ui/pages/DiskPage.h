#pragma once

#include "ui/PageBase.h"
#include "ui/RecoveryControls.h"
#include "modules/FullCore.h"

#include <array>
#include <cstdint>
#include <filesystem>
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
    struct BrowserEntry {
        std::filesystem::path path;
        std::uint64_t size{};
        bool directory{false};
        bool protectedPath{false};
    };

    void Navigate(const std::filesystem::path& root, bool addHistory = true);
    void Browse();
    void ScanLarge();
    void OpenSelected(bool activate);
    void OpenExplorer();
    void PopulateBrowser(std::vector<BrowserEntry> entries);
    void PopulateLarge(std::vector<dpop::full::FileItem> files);
    int IconIndexFor(const std::filesystem::path& path, bool directory);

    RecoveryFonts fonts_;
    std::array<HWND, 7> toolbar_{};
    HWND pathEdit_{};
    HWND list_{};
    std::filesystem::path root_{L"C:\\"};
    std::vector<BrowserEntry> entries_;
    std::vector<std::filesystem::path> history_;
    int historyIndex_{-1};
    HIMAGELIST systemImages_{};
    dpop::full::Settings settings_{};
    bool largeMode_{false};
    std::wstring summary_{L"Проводник DPopCleaner готов."};
};

}
