#define MyAppName "Fallout Unified Co-op Beta"
#define MyAppVersion "0.1-beta"
#define MyAppExeName "fallout2-ce.exe"

[Setup]
AppId={{8D94A6CE-0F8E-4A6B-BA96-6B3B7E8A94C2}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
DefaultDirName={autopf}\Fallout Unified Co-op Beta
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\installer-output
OutputBaseFilename=Fallout-Unified-Coop-Beta-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\{#MyAppExeName}

[Files]
Source: "..\build\RelWithDebInfo\fallout2-ce.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Fallout Unified Co-op Beta"; Filename: "{app}\{#MyAppExeName}"; Parameters: "--unified --fallout1-root=""{code:GetFallout1Root}"" --fallout2-root=""{code:GetFallout2Root}"""; WorkingDir: "{app}"
Name: "{autodesktop}\Fallout Unified Co-op Beta"; Filename: "{app}\{#MyAppExeName}"; Parameters: "--unified --fallout1-root=""{code:GetFallout1Root}"" --fallout2-root=""{code:GetFallout2Root}"""; WorkingDir: "{app}"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"

[Run]
Filename: "{app}\{#MyAppExeName}"; Parameters: "--unified --fallout1-root=""{code:GetFallout1Root}"" --fallout2-root=""{code:GetFallout2Root}"""; Description: "Launch Fallout Unified Co-op Beta"; Flags: nowait postinstall skipifsilent

[Code]
var
  GameRootsPage: TInputDirWizardPage;

function HasGameData(const Root: String; NeedPatch: Boolean): Boolean;
begin
  Result := FileExists(AddBackslash(Root) + 'master.dat') or FileExists(AddBackslash(Root) + 'MASTER.DAT');
  Result := Result and (FileExists(AddBackslash(Root) + 'critter.dat') or FileExists(AddBackslash(Root) + 'CRITTER.DAT'));
  Result := Result and DirExists(AddBackslash(Root) + 'data');
  if NeedPatch then
    Result := Result and (FileExists(AddBackslash(Root) + 'patch000.dat') or FileExists(AddBackslash(Root) + 'PATCH000.DAT'));
end;

function TrySteamCommon(const CommonRoot, GameName: String): String;
var
  Candidate: String;
begin
  Candidate := AddBackslash(CommonRoot) + GameName;
  if DirExists(Candidate) then
    Result := Candidate
  else
    Result := '';
end;

function FindGame(GameName: String): String;
var
  SteamPath: String;
  Candidate: String;
begin
  Result := '';

  Candidate := TrySteamCommon('D:\SteamLibrary\steamapps\common', GameName);
  if Candidate <> '' then begin Result := Candidate; exit; end;

  Candidate := TrySteamCommon('C:\Program Files (x86)\Steam\steamapps\common', GameName);
  if Candidate <> '' then begin Result := Candidate; exit; end;

  if RegQueryStringValue(HKCU, 'Software\Valve\Steam', 'SteamPath', SteamPath) then
  begin
    StringChangeEx(SteamPath, '/', '\', True);
    Candidate := TrySteamCommon(AddBackslash(SteamPath) + 'steamapps\common', GameName);
    if Candidate <> '' then begin Result := Candidate; exit; end;
  end;
end;

procedure InitializeWizard;
var
  F1: String;
  F2: String;
begin
  GameRootsPage := CreateInputDirPage(wpSelectDir,
    'Locate your Fallout games',
    'Select your original Fallout 1 and Fallout 2 installation folders.',
    'The installer does not include copyrighted game data. It only creates a configured launcher that points at your existing installations.',
    False, '');

  GameRootsPage.Add('Fallout 1 folder:');
  GameRootsPage.Add('Fallout 2 folder:');

  F1 := FindGame('Fallout');
  F2 := FindGame('Fallout 2');
  if F1 <> '' then GameRootsPage.Values[0] := F1;
  if F2 <> '' then GameRootsPage.Values[1] := F2;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = GameRootsPage.ID then
  begin
    if not HasGameData(GameRootsPage.Values[0], False) then
    begin
      MsgBox('Fallout 1 data was not found there. Select the folder containing master.dat, critter.dat, and the data folder.', mbError, MB_OK);
      Result := False;
      exit;
    end;

    if not HasGameData(GameRootsPage.Values[1], True) then
    begin
      MsgBox('Fallout 2 data was not found there. Select the folder containing master.dat, critter.dat, patch000.dat, and the data folder.', mbError, MB_OK);
      Result := False;
      exit;
    end;
  end;
end;

function GetFallout1Root(Param: String): String;
begin
  Result := GameRootsPage.Values[0];
end;

function GetFallout2Root(Param: String): String;
begin
  Result := GameRootsPage.Values[1];
end;
