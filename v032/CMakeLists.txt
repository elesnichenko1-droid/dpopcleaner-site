cmake_minimum_required(VERSION 3.24)
project(DPopCleaner VERSION 0.3.2 LANGUAGES CXX RC)
include(CTest)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
set(DPOP_FILE_DESCRIPTION "DPopCleaner Windows utility")
set(DPOP_INTERNAL_NAME "DPopCleaner.exe")
set(DPOP_ORIGINAL_FILENAME "DPopCleaner.exe")
configure_file(resources/version.rc.in "${CMAKE_BINARY_DIR}/generated/dpopcleaner-version.rc" @ONLY)
set(DPOP_FILE_DESCRIPTION "DPopCleaner secure updater")
set(DPOP_INTERNAL_NAME "DPopUpdater.exe")
set(DPOP_ORIGINAL_FILENAME "DPopUpdater.exe")
configure_file(resources/version.rc.in "${CMAKE_BINARY_DIR}/generated/dpopupdater-version.rc" @ONLY)
if(NOT WIN32)
  message(FATAL_ERROR "DPopCleaner targets Windows only.")
endif()

add_executable(DPopCleaner WIN32
  src/main.cpp
  src/core/SingleInstance.cpp
  src/app/MainWindow.cpp
  src/ui/ShellModel.cpp
  src/ui/Layout.cpp
  src/ui/Theme.cpp
  src/ui/Controls.cpp
  src/ui/SessionLog.cpp
  src/ui/StatusBar.cpp
  src/ui/Shell.cpp
  src/ui/pages/OverviewPage.cpp
  src/ui/pages/WorkspacePage.cpp
  src/core/Logger.cpp
  src/core/Paths.cpp
  src/modules/FullCore.cpp
  src/modules/SystemInfo.cpp
  src/modules/Cleaner.cpp
  src/modules/StartupManager.cpp
  src/modules/DPopGuard.cpp
  src/modules/ZapretManager.cpp
  src/modules/ZapretPolicy.cpp
  src/modules/Applications.cpp
  src/update/UpdateManifest.cpp
  src/update/UpdatePolicy.cpp
  src/update/UpdateClient.cpp
  src/update/Hash.cpp
  src/update/Signature.cpp
  resources/app.rc
  "${CMAKE_BINARY_DIR}/generated/dpopcleaner-version.rc"
)
target_include_directories(DPopCleaner PRIVATE src)
target_compile_definitions(DPopCleaner PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(DPopCleaner PRIVATE
  user32 gdi32 shell32 ole32 advapi32 comdlg32 dwmapi uxtheme
  winhttp bcrypt wintrust crypt32 shlwapi comctl32 dxgi psapi
)
if(MSVC)
  target_compile_options(DPopCleaner PRIVATE /W4 /permissive- /utf-8)
  target_link_options(DPopCleaner PRIVATE /MANIFEST:NO)
endif()

add_executable(DPopUpdater WIN32
  src/updater/main.cpp
  src/update/Hash.cpp
  src/update/Signature.cpp
  src/core/Paths.cpp
  resources/updater.rc
  "${CMAKE_BINARY_DIR}/generated/dpopupdater-version.rc"
)
target_include_directories(DPopUpdater PRIVATE src)
target_compile_definitions(DPopUpdater PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(DPopUpdater PRIVATE user32 shell32 bcrypt wintrust crypt32)
if(MSVC)
  target_compile_options(DPopUpdater PRIVATE /W4 /permissive- /utf-8)
  target_link_options(DPopUpdater PRIVATE /MANIFEST:NO)
endif()

set_target_properties(DPopCleaner DPopUpdater PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
  string(TOUPPER "${cfg}" CFG_UPPER)
  set_target_properties(DPopCleaner DPopUpdater PROPERTIES "RUNTIME_OUTPUT_DIRECTORY_${CFG_UPPER}" "${CMAKE_BINARY_DIR}/bin/${cfg}")
endforeach()

if(BUILD_TESTING)
  add_executable(UpdatePolicyTests tests/UpdatePolicyTests.cpp src/update/UpdateManifest.cpp src/update/UpdatePolicy.cpp)
  target_include_directories(UpdatePolicyTests PRIVATE src)
  target_compile_definitions(UpdatePolicyTests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  if(MSVC)
    target_compile_options(UpdatePolicyTests PRIVATE /W4 /permissive- /utf-8)
  endif()
  add_test(NAME UpdatePolicyTests COMMAND UpdatePolicyTests)

  add_executable(SingleInstanceTests tests/SingleInstanceTests.cpp src/core/SingleInstance.cpp)
  target_include_directories(SingleInstanceTests PRIVATE src)
  target_compile_definitions(SingleInstanceTests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(SingleInstanceTests PRIVATE user32)
  if(MSVC)
    target_compile_options(SingleInstanceTests PRIVATE /W4 /permissive- /utf-8)
  endif()
  add_test(NAME SingleInstanceTests COMMAND SingleInstanceTests)

  add_executable(ZapretPolicyTests tests/ZapretPolicyTests.cpp src/modules/ZapretPolicy.cpp)
  target_include_directories(ZapretPolicyTests PRIVATE src)
  target_compile_definitions(ZapretPolicyTests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  if(MSVC)
    target_compile_options(ZapretPolicyTests PRIVATE /W4 /permissive- /utf-8)
  endif()
  add_test(NAME ZapretPolicyTests COMMAND ZapretPolicyTests)

  add_executable(ShellModelTests tests/v032/ShellModelTests.cpp src/ui/ShellModel.cpp)
  target_include_directories(ShellModelTests PRIVATE src)
  target_compile_definitions(ShellModelTests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  if(MSVC)
    target_compile_options(ShellModelTests PRIVATE /W4 /permissive- /utf-8)
  endif()
  add_test(NAME ShellModelTests COMMAND ShellModelTests)

  add_executable(LayoutTests tests/v032/LayoutTests.cpp src/ui/Layout.cpp)
  target_include_directories(LayoutTests PRIVATE src)
  target_compile_definitions(LayoutTests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  if(MSVC)
    target_compile_options(LayoutTests PRIVATE /W4 /permissive- /utf-8)
  endif()
  add_test(NAME LayoutTests COMMAND LayoutTests)

  add_executable(ThemeTests tests/v032/ThemeTests.cpp src/ui/Theme.cpp)
  target_include_directories(ThemeTests PRIVATE src)
  target_compile_definitions(ThemeTests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(ThemeTests PRIVATE user32 gdi32)
  if(MSVC)
    target_compile_options(ThemeTests PRIVATE /W4 /permissive- /utf-8)
  endif()
  add_test(NAME ThemeTests COMMAND ThemeTests)

  add_executable(ControlsTests tests/v032/ControlsTests.cpp src/ui/Theme.cpp src/ui/Controls.cpp)
  target_include_directories(ControlsTests PRIVATE src)
  target_compile_definitions(ControlsTests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(ControlsTests PRIVATE user32 gdi32 uxtheme comctl32)
  if(MSVC)
    target_compile_options(ControlsTests PRIVATE /W4 /permissive- /utf-8)
  endif()
  add_test(NAME ControlsTests COMMAND ControlsTests)

  add_executable(SessionLogTests tests/v032/SessionLogTests.cpp src/ui/SessionLog.cpp)
  target_include_directories(SessionLogTests PRIVATE src)
  target_compile_definitions(SessionLogTests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  if(MSVC)
    target_compile_options(SessionLogTests PRIVATE /W4 /permissive- /utf-8)
  endif()
  add_test(NAME SessionLogTests COMMAND SessionLogTests)

  add_executable(StatusBarCompileTests tests/v032/StatusBarCompileTests.cpp src/ui/StatusBar.cpp src/ui/SessionLog.cpp src/ui/Controls.cpp src/ui/Theme.cpp)
  target_include_directories(StatusBarCompileTests PRIVATE src)
  target_compile_definitions(StatusBarCompileTests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(StatusBarCompileTests PRIVATE user32 gdi32 uxtheme comctl32)
  if(MSVC)
    target_compile_options(StatusBarCompileTests PRIVATE /W4 /permissive- /utf-8)
  endif()
  add_test(NAME StatusBarCompileTests COMMAND StatusBarCompileTests)

  add_executable(ShellContractTests tests/v032/ShellContractTests.cpp src/ui/ShellModel.cpp)
  target_include_directories(ShellContractTests PRIVATE src)
  target_compile_definitions(ShellContractTests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  if(MSVC)
    target_compile_options(ShellContractTests PRIVATE /W4 /permissive- /utf-8)
  endif()
  add_test(NAME ShellContractTests COMMAND ShellContractTests)

  add_executable(OverviewModelTests tests/v032/OverviewModelTests.cpp)
  target_include_directories(OverviewModelTests PRIVATE src)
  target_compile_definitions(OverviewModelTests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  if(MSVC)
    target_compile_options(OverviewModelTests PRIVATE /W4 /permissive- /utf-8)
  endif()
  add_test(NAME OverviewModelTests COMMAND OverviewModelTests)

  add_executable(FullCoreTests
    tests/v032/FullCoreTests.cpp
    src/modules/FullCore.cpp
    src/modules/DPopGuard.cpp
    src/modules/StartupManager.cpp
    src/update/Hash.cpp
  )
  target_include_directories(FullCoreTests PRIVATE src)
  target_compile_definitions(FullCoreTests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(FullCoreTests PRIVATE user32 shell32 ole32 advapi32 psapi bcrypt)
  if(MSVC)
    target_compile_options(FullCoreTests PRIVATE /W4 /permissive- /utf-8)
  endif()
  add_test(NAME FullCoreTests COMMAND FullCoreTests)
endif()
