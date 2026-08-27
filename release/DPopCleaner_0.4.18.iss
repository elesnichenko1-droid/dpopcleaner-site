#define MyAppName "DPopCleaner"
#define MyAppVersion "0.4.18"
#ifndef StageRoot
  #define StageRoot "..\_release\0.4.18\stage"
#endif
#ifndef InstallerOutputDir
  #define InstallerOutputDir "..\_release\0.4.18\installer"
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
VersionInfoVersion=0.4.18.1
VersionInfoProductVersion=0.4.18.1
VersionInfoCompany=DPopCleaner Project
VersionInfoDescription=DPopCleaner 0.4.18 Setup Revision 1
VersionInfoProductName=DPopCleaner
DefaultDirName={autopf}\DPopCleaner
DefaultGroupName=DPopCleaner
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir={#InstallerOutputDir}
OutputBaseFilename=DPopCleaner_Setup_0.4.18
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupLogging=yes
CloseApplications=yes
RestartApplications=no
AllowNoIcons=yes
MinVersion=10.0
UninstallDisplayIcon={app}\DPopCleaner.exe
UninstallDisplayName=DPopCleaner 0.4.18

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Dirs]
Name: "{app}\Languages"
Name: "{app}\Shell"
Name: "{app}\Documentation"; Permissions: users-modify
Name: "{app}\Modules"
Name: "{app}\ThirdParty"
Name: "{app}\ThirdParty\Zapret"
Name: "{app}\Resources"

; The bundled Zapret directory is program-owned. User *-user.txt files are
; backed up in CurStepChanged(ssInstall) before this exact-tree replacement and
; restored in ssPostInstall after the new verified tree is installed.
[InstallDelete]
Type: filesandordirs; Name: "{app}\ThirdParty\Zapret"

[Files]
Source: "{#StageRoot}\DPopCleaner.exe"; DestDir: "{app}"; DestName: "DPopCleaner.exe"; Flags: ignoreversion restartreplace
Source: "{#StageRoot}\DPopUpdater.exe"; DestDir: "{app}"; DestName: "DPopUpdater.exe"; Flags: ignoreversion restartreplace
Source: "{#StageRoot}\Modules\*"; DestDir: "{app}\Modules"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageRoot}\Languages\*"; DestDir: "{app}\Languages"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageRoot}\Shell\*"; DestDir: "{app}\Shell"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageRoot}\Documentation\*"; DestDir: "{app}\Documentation"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageRoot}\ThirdParty\Zapret\*"; DestDir: "{app}\ThirdParty\Zapret"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageRoot}\Resources\*"; DestDir: "{app}\Resources"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

[Tasks]
Name: "desktopicon"; Description: "Создать ярлык на рабочем столе"; GroupDescription: "Дополнительные ярлыки:"; Flags: unchecked

[Icons]
Name: "{autoprograms}\DPopCleaner"; Filename: "{app}\DPopCleaner.exe"; WorkingDir: "{app}"
Name: "{autoprograms}\DPopCleaner\Анализатор диска"; Filename: "{app}\Modules\DiskAnalyzer.exe"; WorkingDir: "{app}"
Name: "{autoprograms}\DPopCleaner\Центр восстановления"; Filename: "{app}\Modules\RestoreCenter.exe"; WorkingDir: "{app}"
Name: "{autoprograms}\DPopCleaner\Zapret — фикс демонстрации экрана"; Filename: "{app}\Modules\ZapretScreenFix.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\DPopCleaner"; Filename: "{app}\DPopCleaner.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\DPopCleaner.exe"; Description: "Запустить DPopCleaner"; Flags: nowait postinstall skipifsilent

[Code]
var
  ZapretBackupDir: String;

function NextZapretBackupDir(): String;
var
  Base: String;
  Candidate: String;
  Counter: Integer;
begin
  Base := AddBackslash(ExpandConstant('{localappdata}\DPopCleaner\ZapretBackup')) +
          GetDateTimeString('yyyymmdd-hhnnss', '-', ':');
  Candidate := Base;
  Counter := 1;
  while DirExists(Candidate) do
  begin
    Counter := Counter + 1;
    Candidate := Base + '-' + IntToStr(Counter);
  end;
  Result := Candidate;
end;

procedure BackupZapretUserLists();
var
  ListsDir: String;
  SourcePath: String;
  DestPath: String;
  FindRec: TFindRec;
begin
  ZapretBackupDir := '';
  ListsDir := ExpandConstant('{app}\ThirdParty\Zapret\lists');
  if not DirExists(ListsDir) then
    Exit;

  if FindFirst(AddBackslash(ListsDir) + '*-user.txt', FindRec) then
  begin
    try
      ZapretBackupDir := NextZapretBackupDir();
      if not ForceDirectories(ZapretBackupDir) then
        RaiseException('Cannot create ZapretBackup directory: ' + ZapretBackupDir);

      repeat
        if FindRec.Attributes and FILE_ATTRIBUTE_DIRECTORY = 0 then
        begin
          SourcePath := AddBackslash(ListsDir) + FindRec.Name;
          DestPath := AddBackslash(ZapretBackupDir) + FindRec.Name;
          if not CopyFile(SourcePath, DestPath, True) then
            RaiseException('Cannot back up Zapret user list: ' + SourcePath);
          Log('Backed up Zapret user list: ' + SourcePath + ' -> ' + DestPath);
        end;
      until not FindNext(FindRec);
    finally
      FindClose(FindRec);
    end;
  end;
end;

procedure RestoreZapretUserLists();
var
  ListsDir: String;
  SourcePath: String;
  DestPath: String;
  FindRec: TFindRec;
begin
  if (ZapretBackupDir = '') or (not DirExists(ZapretBackupDir)) then
    Exit;

  ListsDir := ExpandConstant('{app}\ThirdParty\Zapret\lists');
  if not ForceDirectories(ListsDir) then
    RaiseException('Cannot create bundled Zapret lists directory: ' + ListsDir);

  if FindFirst(AddBackslash(ZapretBackupDir) + '*-user.txt', FindRec) then
  begin
    try
      repeat
        if FindRec.Attributes and FILE_ATTRIBUTE_DIRECTORY = 0 then
        begin
          SourcePath := AddBackslash(ZapretBackupDir) + FindRec.Name;
          DestPath := AddBackslash(ListsDir) + FindRec.Name;
          if not CopyFile(SourcePath, DestPath, False) then
            RaiseException('Cannot restore Zapret user list: ' + DestPath);
          Log('Restored Zapret user list: ' + SourcePath + ' -> ' + DestPath);
        end;
      until not FindNext(FindRec);
    finally
      FindClose(FindRec);
    end;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssInstall then
    BackupZapretUserLists()
  else if CurStep = ssPostInstall then
    RestoreZapretUserLists();
end;
