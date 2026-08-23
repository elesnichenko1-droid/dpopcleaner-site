#include "app/MainWindow.h"
#include "ui/Shell.h"

namespace dpop::ui {
int Run(HINSTANCE instance, int showCommand) {
    return dpop::ui::shell::Run(instance, showCommand);
}
}
