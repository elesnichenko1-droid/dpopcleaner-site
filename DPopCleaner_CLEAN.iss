#define MyAppName "DPopCleaner"
#define MyAppVersion "0.2.14 BETA"
#ifndef SourceExe
  #define SourceExe "..\app\DPopCleaner.exe"
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
DefaultDirName={autopf}\DPopCleaner
DefaultGroupName=DPopCleaner
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir=output
OutputBaseFilename=DPopCleaner_Setup_0.2.14_BETA_CLEAN
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
AllowNoIcons=yes
MinVersion=10.0
UninstallDisplayIcon={app}\DPopCleaner.exe
UninstallDisplayName=DPopCleaner 0.2.14 BETA

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; ВАЖНО: только основной DPopCleaner.exe.
; WinDivert.sys / WinDivert.dll / winws.exe / zapret НЕ упаковываются.
Source: "{#SourceExe}"; DestDir: "{app}"; DestName: "DPopCleaner.exe"; Flags: ignoreversion restartreplace

[Tasks]
Name: "desktopicon"; Description: "Создать ярлык на рабочем столе"; Flags: unchecked

[Icons]
Name: "{autoprograms}\DPopCleaner"; Filename: "{app}\DPopCleaner.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\DPopCleaner"; Filename: "{app}\DPopCleaner.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\DPopCleaner.exe"; Description: "Запустить DPopCleaner"; Flags: nowait postinstall skipifsilent
