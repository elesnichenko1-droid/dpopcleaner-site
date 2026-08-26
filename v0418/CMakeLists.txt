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

enable_testing()

function(dpop0418_test target)
    target_include_directories(${target} PRIVATE core)
    target_compile_definitions(${target} PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    endif()
    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_executable(AppSettingsTests tests/AppSettingsTests.cpp core/AppSettings.cpp)
dpop0418_test(AppSettingsTests)

add_executable(UpdatePolicyTests tests/UpdatePolicyTests.cpp core/UpdatePolicy.cpp)
dpop0418_test(UpdatePolicyTests)

add_executable(UpdateManifestTests tests/UpdateManifestTests.cpp core/UpdateManifest.cpp)
dpop0418_test(UpdateManifestTests)

add_executable(PackageVerificationTests tests/PackageVerificationTests.cpp core/Hash.cpp)
target_link_libraries(PackageVerificationTests PRIVATE bcrypt)
dpop0418_test(PackageVerificationTests)

add_executable(UpdateClientContractTests tests/UpdateClientContractTests.cpp)
target_compile_definitions(UpdateClientContractTests PRIVATE DPOP0418_SOURCE_DIR=L"${CMAKE_CURRENT_SOURCE_DIR}")
dpop0418_test(UpdateClientContractTests)
