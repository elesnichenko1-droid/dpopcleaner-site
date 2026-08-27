cmake_minimum_required(VERSION 3.24)
project(DPopCleaner0418 VERSION 0.4.18 LANGUAGES CXX RC)

if(NOT WIN32)
    message(FATAL_ERROR "DPopCleaner 0.4.18 targets Windows only")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../dpopcleaner.ico" "${CMAKE_BINARY_DIR}/generated/dpopcleaner.ico" COPYONLY)
configure_file(resources/resource.h "${CMAKE_BINARY_DIR}/generated/resource.h" COPYONLY)
configure_file(resources/version.rc.in "${CMAKE_BINARY_DIR}/generated/version.rc" @ONLY)
set(DPOP0418_VERSION_RESOURCE "${CMAKE_BINARY_DIR}/generated/version.rc")

enable_testing()

function(dpop0418_target target)
    target_include_directories(${target} PRIVATE core "${CMAKE_BINARY_DIR}/generated")
    target_compile_definitions(${target} PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    endif()
endfunction()

function(dpop0418_test target)
    dpop0418_target(${target})
    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_executable(AppSettingsTests tests/AppSettingsTests.cpp core/AppSettings.cpp)
dpop0418_test(AppSettingsTests)

add_executable(ZapretControllerTests tests/ZapretControllerTests.cpp core/ZapretController.cpp)
target_link_libraries(ZapretControllerTests PRIVATE advapi32 shell32)
dpop0418_test(ZapretControllerTests)

add_executable(UpdatePolicyTests tests/UpdatePolicyTests.cpp core/UpdatePolicy.cpp)
dpop0418_test(UpdatePolicyTests)

add_executable(UpdateManifestTests tests/UpdateManifestTests.cpp core/UpdateManifest.cpp)
dpop0418_test(UpdateManifestTests)

add_executable(PackageVerificationTests tests/PackageVerificationTests.cpp core/Hash.cpp)
target_link_libraries(PackageVerificationTests PRIVATE bcrypt)
dpop0418_test(PackageVerificationTests)

add_executable(UpdateClientContractTests
    tests/UpdateClientContractTests.cpp
    core/UpdateClient.cpp
    core/UpdateManifest.cpp
    core/UpdatePolicy.cpp
    core/Hash.cpp
    core/Signature.cpp
)
target_compile_definitions(UpdateClientContractTests PRIVATE DPOP0418_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(UpdateClientContractTests PRIVATE winhttp bcrypt wintrust crypt32 shell32)
dpop0418_test(UpdateClientContractTests)

add_executable(DPopUpdater WIN32
    updater/UpdaterMain.cpp
    core/Hash.cpp
    core/Signature.cpp
    "${DPOP0418_VERSION_RESOURCE}"
)
target_link_libraries(DPopUpdater PRIVATE user32 shell32 bcrypt wintrust crypt32)
dpop0418_target(DPopUpdater)

add_executable(DPopCleaner WIN32
    core/main.cpp
    core/MainWindow.cpp
    core/AppSettings.cpp
    core/ZapretController.cpp
    core/UpdateClient.cpp
    core/UpdateManifest.cpp
    core/UpdatePolicy.cpp
    core/Hash.cpp
    core/Signature.cpp
    "${DPOP0418_VERSION_RESOURCE}"
)
target_link_libraries(DPopCleaner PRIVATE
    user32 gdi32 shell32 comctl32 dwmapi advapi32
    winhttp bcrypt wintrust crypt32
)
dpop0418_target(DPopCleaner)

# Both DPopCleaner and DPopUpdater receive the same icon + version resource.
set_target_properties(DPopCleaner DPopUpdater PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${cfg}" cfg_upper)
    set_target_properties(DPopCleaner DPopUpdater PROPERTIES "RUNTIME_OUTPUT_DIRECTORY_${cfg_upper}" "${CMAKE_BINARY_DIR}/bin/${cfg}")
endforeach()
