#pragma once

#include "ui/PageBase.h"
#include "ui/RecoveryControls.h"
#include "modules/Applications.h"

#include <array>
#include <vector>

namespace dpop::ui {

class ApplicationsPage final : public PageBase {
public:
    ~ApplicationsPage() override { Destroy(); }
    bool Create(HWND parent, SessionLog& log) {
        return PageBase::Create(parent, log, L"Приложения");
    }

protected:
    bool OnCreate() override;
    void OnLayout(int width, int height) noexcept override;
    LRESULT OnMessage(UINT message, WPARAM wParam, LPARAM lParam, bool& handled) override;
    void OnPaint(HDC dc, const RECT& client) noexcept override;
    void OnVisibilityChanged(bool visible) noexcept override;

private:
    void RefreshApps();
    void ApplyFilter();
    void UpdateDetails();
    void CheckUpdate();
    void UninstallSelected();
    void OpenSelectedFolder();
    void FindLeftovers();
    void RecycleLeftovers();
    void ShowAppsMode();
    int IconIndexFor(const dpop::apps::InstalledApp& app);
    int IconIndexForPath(const std::filesystem::path& path);

    RecoveryFonts fonts_;
    HWND search_{};
    HWND count_{};
    HWND list_{};
    std::array<HWND, 5> detailLabels_{};
    std::array<HWND, 7> buttons_{};
    HIMAGELIST systemImages_{};

    std::vector<dpop::apps::InstalledApp> apps_;
    std::vector<std::size_t> visibleAppIndices_;
    std::vector<dpop::apps::LeftoverItem> leftovers_;
    bool leftoversMode_{false};
    bool loaded_{false};
};

}
