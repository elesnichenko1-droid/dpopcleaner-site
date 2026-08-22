#include "app/MainWindow.h"
#include "core/SingleInstance.h"
#include <windows.h>
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int showCommand) {
    dpop::SingleInstance singleInstance(L"Local\\DPopCleaner.0.3.1.R3");
    if (!singleInstance.IsPrimary()) {
        dpop::SingleInstance::ActivateExistingWindow(L"DPopCleanerMainV3");
        return 0;
    }
    return dpop::ui::Run(instance, showCommand);
}
