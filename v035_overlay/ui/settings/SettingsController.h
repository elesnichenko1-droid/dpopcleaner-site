#pragma once

#include "modules/SettingsStore.h"

namespace dpop::ui {

class SettingsController {
public:
    explicit SettingsController(dpop::settings::AppSettings persisted);

    const dpop::settings::AppSettings& Persisted() const noexcept;
    dpop::settings::AppSettings& Edit() noexcept;
    const dpop::settings::AppSettings& Edit() const noexcept;

    bool Dirty() const noexcept;
    void MarkDirty() noexcept;
    void CancelEdits() noexcept;
    void LoadDefaults();
    void CommitInMemory();

private:
    dpop::settings::AppSettings persisted_;
    dpop::settings::AppSettings edit_;
    bool dirty_{};
};

} // namespace dpop::ui
