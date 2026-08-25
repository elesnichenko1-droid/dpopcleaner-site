#include "ui/settings/SettingsController.h"

#include <utility>

namespace dpop::ui {

SettingsController::SettingsController(dpop::settings::AppSettings persisted)
    : persisted_(std::move(persisted)), edit_(persisted_) {}

const dpop::settings::AppSettings& SettingsController::Persisted() const noexcept {
    return persisted_;
}

dpop::settings::AppSettings& SettingsController::Edit() noexcept {
    return edit_;
}

const dpop::settings::AppSettings& SettingsController::Edit() const noexcept {
    return edit_;
}

bool SettingsController::Dirty() const noexcept {
    return dirty_;
}

void SettingsController::MarkDirty() noexcept {
    dirty_ = true;
}

void SettingsController::CancelEdits() noexcept {
    edit_ = persisted_;
    dirty_ = false;
}

void SettingsController::LoadDefaults() {
    edit_ = dpop::settings::DefaultSettings();
    dirty_ = edit_ != persisted_;
}

void SettingsController::CommitInMemory() {
    persisted_ = edit_;
    dirty_ = false;
}

} // namespace dpop::ui
