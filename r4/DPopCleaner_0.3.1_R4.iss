#define MyAppName "DPopCleaner"
#define MyAppVersion "0.3.1 BETA R4"
#ifndef SourceDir
  #define SourceDir "."
#endif
#ifndef ZapretDir
  #define ZapretDir ".\zapret"
#endif
#ifndef IconFile
  #define IconFile "..\dpopcleaner.ico"
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
VersionInfoVersion=0.3.1.4
VersionInfoProductVersion=0.3.1.4
VersionInfoCompany=DPopCleaner Project
VersionInfoDescription=DPopCleaner 0.3.1 BETA R4 Setup
VersionInfoProductName=DPopCleaner
DefaultDirName={autopf}\DPopCleaner
DefaultGroupName=DPopCleaner
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir=output
OutputBaseFilename=DPopCleaner_Setup_0.3.1_BETA_R4
SetupIconFile={#IconFile}
UninstallDisplayIcon={app}\DPopCleaner.exe
UninstallDisplayName=DPopCleaner 0.3.1 BETA R4
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupLogging=yes
CloseApplications=yes
RestartApplications=no
AllowNoIcons=yes
MinVersion=10.0

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#SourceDir}\DPopCleaner.exe"; DestDir: "{app}"; Flags: ignoreversion restartreplace
Source: "{#SourceDir}\DPopUpdater.exe"; DestDir: "{app}"; Flags: ignoreversion restartreplace
Source: "{#ZapretDir}\*"; DestDir: "{app}\zapret"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\DPopCleaner"; Filename: "{app}\DPopCleaner.exe"; WorkingDir: "{app}"; IconFilename: "{app}\DPopCleaner.exe"
Name: "{autodesktop}\DPopCleaner"; Filename: "{app}\DPopCleaner.exe"; WorkingDir: "{app}"; IconFilename: "{app}\DPopCleaner.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Создать ярлык на рабочем столе"; GroupDescription: "Дополнительные ярлыки:"; Flags: unchecked

[UninstallDelete]
Type: filesandordirs; Name: "{localappdata}\DPopCleaner\Updates"
Type: filesandordirs; Name: "{localappdata}\DPopCleaner\Logs"
Type: dirifempty; Name: "{localappdata}\DPopCleaner"

[Code]
function InitializeUninstall(): Boolean;
begin
  MsgBox('Если служба Zapret установлена, сначала удалите её через Zapret Center → service.bat. Деинсталлятор не удаляет стороннюю службу автоматически.', mbInformation, MB_OK);
  Result := True;
end;
