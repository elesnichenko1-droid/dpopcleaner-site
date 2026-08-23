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

$themePath = Join-Path $root 'ui/Theme.cpp'
$controlsPath = Join-Path $root 'ui/Controls.cpp'

if (Test-Path -LiteralPath $themePath) {
    $theme = Get-Content -Raw $themePath
    foreach ($token in @(
        'RGB(0x0B, 0x10, 0x17)',
        'RGB(0x1B, 0x1F, 0x25)',
        'RGB(0x14, 0x1D, 0x28)',
        'RGB(0x39, 0xD0, 0xA0)',
        'RGB(0xF6, 0xF7, 0xF9)'
    )) {
        if (-not $theme.Contains($token)) {
            throw "Midnight palette token missing: $token"
        }
    }
}

if (Test-Path -LiteralPath $controlsPath) {
    $controls = Get-Content -Raw $controlsPath
    foreach ($token in @(
        'BS_OWNERDRAW',
        'DarkMode_Explorer',
        'ListView_SetBkColor',
        'DrawOwnerButton'
    )) {
        if (-not $controls.Contains($token)) {
            throw "Reusable control behavior missing: $token"
        }
    }
}

$uiCpp = @(Get-ChildItem (Join-Path $root 'ui') -Filter '*.cpp' -Recurse -ErrorAction SilentlyContinue)
foreach ($file in $uiCpp) {
    $text = Get-Content -Raw $file.FullName
    if ($text -match 'PaintSunset|SunsetBackground|SIDEBAR') {
        throw "R4 visual concept leaked into 0.3.2 UI: $($file.FullName)"
    }
}

# Task 6 shell recovery policy.
$shellPath = Join-Path $root 'ui/Shell.cpp'
$settingsStubPath = Join-Path $root 'ui/pages/SettingsStubPage.cpp'

if (Test-Path -LiteralPath $shellPath) {
    $shell = Get-Content -Raw $shellPath

    foreach ($token in @(
        'PrimaryTabs()',
        'kSettingsCommandId',
        'L"⚙"',
        'DwmSetWindowAttribute',
        'WM_GETMINMAXINFO',
        'ComputeShellLayout',
        'StatusBar',
        'SettingsStubPage',
        'L"DPopCleaner 0.3.2 запущен."'
    )) {
        if (-not $shell.Contains($token)) {
            throw "0.3.2 shell behavior missing: $token"
        }
    }

    if ($shell -match 'SIDEBAR|PaintSunset|SunsetBackground') {
        throw 'R4 sidebar/sunset concepts are forbidden in the 0.3.2 shell.'
    }
}

if (Test-Path -LiteralPath $settingsStubPath) {
    $settingsStub = Get-Content -Raw $settingsStubPath

    foreach ($token in @(
        'L"Настройки"',
        'L"Язык: Русский"',
        'L"Тема: Midnight"',
        'L"Бесплатная BETA"',
        'shell-candidate не сохраняет параметры'
    )) {
        if (-not $settingsStub.Contains($token)) {
            throw "Settings stub policy missing: $token"
        }
    }
}

'Ui032Policy PASS'

