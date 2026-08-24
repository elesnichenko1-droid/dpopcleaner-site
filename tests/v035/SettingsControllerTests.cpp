#include "ui/settings/SettingsController.h"

#include <cassert>

using namespace dpop::settings;
using namespace dpop::ui;

int main() {
    AppSettings persisted = DefaultSettings();
    SettingsController controller(persisted);
    assert(!controller.Dirty());
    assert(controller.Edit() == persisted);

    controller.Edit().largeFileMB = 700;
    controller.MarkDirty();
    assert(controller.Dirty());
    assert(controller.Persisted().largeFileMB == 500);

    controller.CancelEdits();
    assert(!controller.Dirty());
    assert(controller.Edit().largeFileMB == 500);

    controller.Edit().largeFileMB = 800;
    controller.MarkDirty();
    controller.CommitInMemory();
    assert(!controller.Dirty());
    assert(controller.Persisted().largeFileMB == 800);
    assert(controller.Edit().largeFileMB == 800);

    controller.LoadDefaults();
    assert(controller.Dirty());
    assert(controller.Edit().largeFileMB == 500);
    assert(controller.Persisted().largeFileMB == 800);

    controller.CancelEdits();
    assert(!controller.Dirty());
    assert(controller.Edit().largeFileMB == 800);

    SettingsController defaults(DefaultSettings());
    defaults.LoadDefaults();
    assert(!defaults.Dirty());
    return 0;
}
