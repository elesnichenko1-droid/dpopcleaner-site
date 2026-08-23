$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# This script lives in v032/tests, so the overlay root is one level up: v032.
$root = Split-Path -Parent $PSScriptRoot
$version = Get-Content -Raw (Join-Path $root 'Version.h')
$adapter = Get-Content -Raw (Join-Path $root 'MainWindow.cpp')
$resource = Get-Content -Raw (Join-Path $root 'version.rc.in')
$cmake = Get-Content -Raw (Join-Path $root 'CMakeLists.txt')

foreach ($token in @('0.3.2 BETA R1', '0.3.2', '3021', '1')) {
    if (-not $version.Contains($token)) { throw "Missing 0.3.2 version token: $token" }
}
if (-not $adapter.Contains('dpop::ui::shell::Run')) {
    throw 'MainWindow adapter must delegate to dpop::ui::shell::Run.'
}
if (-not $resource.Contains('FILEVERSION 0,3,2,1') -or
    -not $resource.Contains('ProductVersion", "0.3.2 BETA R1')) {
    throw '0.3.2 Windows version resource is incorrect.'
}
if (-not $cmake.Contains('project(DPopCleaner VERSION 0.3.2')) {
    throw '0.3.2 CMake project version is incorrect.'
}
if ($cmake.Contains('r4/MainWindow.cpp')) {
    throw '0.3.2 CMake must not compile the R4 UI source.'
}
'Ui032Policy PASS'
