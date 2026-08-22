#define MyAppName "DPopCleaner"
#define MyAppVersion "0.2.15 BETA"
#ifndef SourceDir
  #define SourceDir "."
#endif
#ifndef IconFile
  #define IconFile "..\\resources\\dpopcleaner.ico"
#endif

[Setup]
AppId={{B892E3D2-00CC-4D16-BB22-8B3943D42D15}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher=DPopCleaner Project
AppPublisherURL=https://elesnichenko1-droid.github.io/dpopcleaner-site/
AppSupportURL=https://elesnichenko1-droid.github.io/dpopcleaner-site/
DefaultDirName={autopf}\DPopCleaner
DefaultGroupName=DPopCleaner
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir=output
OutputBaseFilename=DPopCleaner_Setup_0.2.15_BETA
SetupIconFile={#IconFile}
UninstallDisplayIcon={app}\DPopCleaner.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupLogging=yes

[Files]
Source: "{#SourceDir}\DPopCleaner.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\DPopUpdater.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\DPopCleaner"; Filename: "{app}\DPopCleaner.exe"
Name: "{autodesktop}\DPopCleaner"; Filename: "{app}\DPopCleaner.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Создать ярлык на рабочем столе"; GroupDescription: "Дополнительные ярлыки:"; Flags: unchecked

[Run]
Filename: "{app}\DPopCleaner.exe"; Description: "Запустить DPopCleaner"; Flags: nowait postinstall skipifsilent
