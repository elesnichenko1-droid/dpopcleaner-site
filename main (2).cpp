#include "app/MainWindow.h"
#include <windows.h>

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int showCommand){
    return dpop::ui::Run(instance,showCommand);
}
