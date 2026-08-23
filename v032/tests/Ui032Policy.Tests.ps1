$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path -Parent $PSScriptRoot
$version = Get-Content -Raw (Join-Path $root 'Version.h')
$adapter = Get-Content -Raw (Join-Path $root 'MainWindow.cpp')
$resource = Get-Content -Raw (Join-Path $root 'version.rc.in')
$cmake = Get-Content -Raw (Join-Path $root 'CMakeLists.txt')

foreach ($token in @('0.3.2 BETA R1', '0.3.2', '3021', '1')) {
    if (-not $version.Contains($token)) { throw "Missing version token: $token" }
}
if (-not $adapter.Contains('dpop::ui::shell::Run')) { throw 'MainWindow must delegate to shell::Run.' }
if (-not $resource.Contains('FILEVERSION 0,3,2,1') -or -not $resource.Contains('ProductVersion", "0.3.2 BETA R1')) { throw 'Version resource mismatch.' }
if (-not $cmake.Contains('project(DPopCleaner VERSION 0.3.2')) { throw 'CMake version mismatch.' }
if ($cmake.Contains('r4/MainWindow.cpp')) { throw 'R4 UI must not compile into 0.3.2.' }

$theme = Get-Content -Raw (Join-Path $root 'ui/Theme.cpp')
foreach ($token in @('RGB(0x0B, 0x10, 0x17)', 'RGB(0x39, 0xD0, 0xA0)', 'RGB(0xF6, 0xF7, 0xF9)')) {
    if (-not $theme.Contains($token)) { throw "Midnight token missing: $token" }
}

$controls = Get-Content -Raw (Join-Path $root 'ui/Controls.cpp')
foreach ($token in @('BS_OWNERDRAW', 'DarkMode_Explorer', 'ListView_SetBkColor', 'DrawOwnerButton')) {
    if (-not $controls.Contains($token)) { throw "Control behavior missing: $token" }
}

$shell = Get-Content -Raw (Join-Path $root 'ui/Shell.cpp')
foreach ($token in @('PrimaryTabs()', 'L"⚙"', 'OverviewPage', 'WorkspacePage', 'kWorkspaceLogChangedMessage', 'WM_GETMINMAXINFO')) {
    if (-not $shell.Contains($token)) { throw "FULL shell integration missing: $token" }
}
if ($shell.Contains('Раздел будет подключён')) { throw 'Primary-page placeholder text is forbidden in FULL candidate.' }

$workspace = Get-Content -Raw (Join-Path $root 'ui/pages/WorkspacePage.cpp')
foreach ($token in @(
    'Page::Cleaning', 'Page::Memory', 'Page::Guard', 'Page::Disk', 'Page::Applications',
    'Page::WindowsUpdate', 'Page::Duplicates', 'Page::Tools', 'Page::Zapret', 'Page::Settings',
    'AnalyzeCleaning', 'TrimWorkingSets', 'QuickScan', 'ScanFileWithAmsi', 'ScanLargeFiles',
    'LaunchUninstaller', 'RunMaintenance', 'FindDuplicates', 'LaunchDefaultStrategy', 'SaveSettings',
    'std::jthread', 'request_stop'
)) {
    if (-not $workspace.Contains($token)) { throw "Workspace behavior missing: $token" }
}

$core = Get-Content -Raw (Join-Path $root 'modules/FullCore.cpp')
foreach ($token in @(
    'Windows TEMP', 'Windows Error Reporting', 'DirectX Shader Cache', 'Кэш Discord', 'Кэш Steam',
    'EmptyWorkingSet', 'Sha256File', 'FOF_ALLOWUNDO', 'StartComponentCleanup /ResetBase',
    'ScanFolderWithAmsi', 'Quarantine', 'settings.json', 'CurrentVersion\\Run'
)) {
    if (-not $core.Contains($token)) { throw "FullCore capability missing: $token" }
}

foreach ($file in @(Get-ChildItem (Join-Path $root 'ui') -Filter '*.cpp' -Recurse)) {
    $text = Get-Content -Raw $file.FullName
    if ($text -match 'PaintSunset|SunsetBackground|SIDEBAR') { throw "R4 visual concept leaked: $($file.FullName)" }
}

if (-not $cmake.Contains('src/modules/FullCore.cpp') -or -not $cmake.Contains('src/ui/pages/WorkspacePage.cpp') -or -not $cmake.Contains('psapi')) { throw 'FULL CMake wiring missing.' }
if (-not $cmake.Contains('FullCoreTests')) { throw 'FullCoreTests target missing.' }
$session = Get-Content -Raw (Join-Path $root 'ui/SessionLog.cpp')
if (-not $session.Contains('DPopCleaner.log') -or -not $session.Contains('CP_UTF8')) { throw 'Persistent UTF-8 session log is missing.' }

'Ui032Policy PASS'
