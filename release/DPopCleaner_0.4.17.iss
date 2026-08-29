#define MyAppName "DPopCleaner"
#define MyAppVersion "0.4.17"
#ifndef StageRoot
  #define StageRoot "..\_release\0.4.17\stage"
#endif
#ifndef InstallerOutputDir
  #define InstallerOutputDir "..\_release\0.4.17\installer"
#endif

[Setup]
AppId={{B892E3D2-00CC-4D16-BB22-8B3943D42D15}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher=DPopCleaner Project
AppPublisherURL=https://elesnichenko1-droid.github.io/dpopcleaner-site/
AppSupportURL=https://elesnichenko1-droid.github.io/dpopcleaner-site/
AppUpdatesURL=https://elesnichenko1-droid.github.io/dpopcleaner-site/
VersionInfoVersion=0.4.17.0
VersionInfoProductVersion=0.4.17.0
VersionInfoCompany=DPopCleaner Project
VersionInfoDescription=DPopCleaner 0.4.17 Setup
VersionInfoProductName=DPopCleaner
DefaultDirName={autopf}\DPopCleaner
DefaultGroupName=DPopCleaner
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir={#InstallerOutputDir}
OutputBaseFilename=DPopCleaner_Setup_0.4.17
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupLogging=yes
CloseApplications=yes
RestartApplications=no
AllowNoIcons=yes
MinVersion=10.0
UninstallDisplayIcon={app}\DPopCleaner.exe
UninstallDisplayName=DPopCleaner 0.4.17

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Dirs]
Name: "{app}\Zapret"
Name: "{app}\Languages"
Name: "{app}\Shell"
Name: "{app}\Documentation"; Permissions: users-modify
Name: "{app}\Modules"
Name: "{app}\Resources"

[Files]
; Keep the frozen 0.2.14 bytes intact, but reserve the historical DPopCleaner.exe path for the UI bridge.
Source: "{#StageRoot}\DPopCleaner.exe"; DestDir: "{app}"; DestName: "DPopCleaner.Core.exe"; Flags: ignoreversion restartreplace
Source: "{#StageRoot}\SimpleUpdate.exe"; DestDir: "{app}"; DestName: "DPopCleaner.exe"; Flags: ignoreversion restartreplace
Source: "{#StageRoot}\SimpleUpdate.exe"; DestDir: "{app}"; DestName: "SimpleUpdate.exe"; Flags: ignoreversion restartreplace
; The immutable 0.2.14 Zapret page expects this root-level compatibility updater.
Source: "{#StageRoot}\SimpleUpdate.exe"; DestDir: "{app}"; DestName: "DPopUpdate.exe"; Flags: ignoreversion restartreplace
; The frozen 0.2.14 core resolves its Zapret root as {app}\Zapret.
Source: "{#StageRoot}\Zapret\*"; DestDir: "{app}\Zapret"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageRoot}\Modules\*"; DestDir: "{app}\Modules"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageRoot}\Languages\*"; DestDir: "{app}\Languages"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageRoot}\Shell\*"; DestDir: "{app}\Shell"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageRoot}\Documentation\*"; DestDir: "{app}\Documentation"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageRoot}\Resources\*"; DestDir: "{app}\Resources"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

[Tasks]
Name: "desktopicon"; Description: "Создать ярлык на рабочем столе"; GroupDescription: "Дополнительные ярлыки:"; Flags: unchecked

[Icons]
Name: "{autoprograms}\DPopCleaner"; Filename: "{app}\DPopCleaner.exe"; WorkingDir: "{app}"
Name: "{autoprograms}\DPopCleaner\Анализатор диска"; Filename: "{app}\Modules\DiskAnalyzer.exe"; WorkingDir: "{app}"
Name: "{autoprograms}\DPopCleaner\Центр восстановления"; Filename: "{app}\Modules\RestoreCenter.exe"; WorkingDir: "{app}"
Name: "{autoprograms}\DPopCleaner\Исправление демонстрации экрана Zapret"; Filename: "{app}\Modules\ZapretScreenFix.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\DPopCleaner"; Filename: "{app}\DPopCleaner.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\DPopCleaner.exe"; Description: "Запустить DPopCleaner"; Flags: nowait postinstall skipifsilent
