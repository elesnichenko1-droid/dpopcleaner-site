#define MyAppName "DPopCleaner"
#define MyAppVersion "0.2.14 BETA R1"
#ifndef SourceExe
  #define SourceExe "..\downloads\DPopCleaner_0.2.14_BETA.exe"
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
VersionInfoVersion=0.2.14.1
VersionInfoProductVersion=0.2.14.1
VersionInfoCompany=DPopCleaner Project
VersionInfoDescription=DPopCleaner 0.2.14 BETA R1 Clean Setup
VersionInfoProductName=DPopCleaner
DefaultDirName={autopf}\DPopCleaner
DefaultGroupName=DPopCleaner
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir=output
OutputBaseFilename=DPopCleaner_Setup_0.2.14_BETA_CLEAN_R1
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupLogging=yes
CloseApplications=yes
RestartApplications=no
AllowNoIcons=yes
MinVersion=10.0
UninstallDisplayIcon={app}\DPopCleaner.exe
UninstallDisplayName=DPopCleaner 0.2.14 BETA R1

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#SourceExe}"; DestDir: "{app}"; DestName: "DPopCleaner.exe"; Flags: ignoreversion restartreplace

[Tasks]
Name: "desktopicon"; Description: "Создать ярлык на рабочем столе"; GroupDescription: "Дополнительные ярлыки:"; Flags: unchecked

[Icons]
Name: "{autoprograms}\DPopCleaner"; Filename: "{app}\DPopCleaner.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\DPopCleaner"; Filename: "{app}\DPopCleaner.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\DPopCleaner.exe"; Description: "Запустить DPopCleaner"; Flags: nowait postinstall skipifsilent
