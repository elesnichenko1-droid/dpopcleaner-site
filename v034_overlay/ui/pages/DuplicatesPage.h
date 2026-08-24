#pragma once

#include "ui/PageBase.h"
#include "ui/RecoveryControls.h"
#include "modules/FullCore.h"

#include <array>
#include <filesystem>
#include <vector>

namespace dpop::ui {

class DuplicatesPage final : public PageBase {
public:
    ~DuplicatesPage() override { Destroy(); }
    bool Create(HWND parent, SessionLog& log) {
        return PageBase::Create(parent, log, L"Дубликаты");
    }

protected:
    bool OnCreate() override;
    void OnLayout(int width, int height) noexcept override;
    LRESULT OnMessage(UINT message, WPARAM wParam, LPARAM lParam, bool& handled) override;
    void OnPaint(HDC dc, const RECT& client) noexcept override;
    void OnVisibilityChanged(bool visible) noexcept override;

private:
    struct RowMeta {
        std::size_t fileIndex{};
        bool referenceCopy{false};
        bool protectedPath{false};
        unsigned duplicateIndex{};
    };

    void Scan();
    void Populate();
    void SelectSafeDuplicates();
    void RecycleChecked();
    void OpenSelected();
    bool LeavesOneCopyPerGroup(const std::vector<int>& checked) const;
    bool IsProtectedPath(const std::filesystem::path& path) const;
    int IconIndexFor(const std::filesystem::path& path);

    RecoveryFonts fonts_;
    HWND path_{};
    HWND threshold_{};
    std::array<HWND, 6> buttons_{};
    HWND list_{};
    HIMAGELIST systemImages_{};
    std::filesystem::path root_;
    std::vector<dpop::full::DuplicateFile> files_;
    std::vector<RowMeta> rows_;
    dpop::full::Settings settings_{};
    std::wstring summary_{L"Выберите папку. Поиск: размер → SHA-256. Эталон группы никогда не выбирается автоматически для удаления."};
};

}
