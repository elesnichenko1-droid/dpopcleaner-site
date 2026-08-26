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

add_executable(AppSettingsTests
    tests/AppSettingsTests.cpp
    core/AppSettings.cpp
)
target_include_directories(AppSettingsTests PRIVATE core)
target_compile_definitions(AppSettingsTests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
if(MSVC)
    target_compile_options(AppSettingsTests PRIVATE /W4 /permissive- /utf-8)
endif()
add_test(NAME AppSettingsTests COMMAND AppSettingsTests)

add_executable(UpdatePolicyTests
    tests/UpdatePolicyTests.cpp
    core/UpdatePolicy.cpp
)
target_include_directories(UpdatePolicyTests PRIVATE core)
target_compile_definitions(UpdatePolicyTests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
if(MSVC)
    target_compile_options(UpdatePolicyTests PRIVATE /W4 /permissive- /utf-8)
endif()
add_test(NAME UpdatePolicyTests COMMAND UpdatePolicyTests)
