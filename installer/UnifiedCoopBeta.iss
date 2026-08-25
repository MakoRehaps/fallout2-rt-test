#define MyAppName "Fallout Unified Co-op Beta Debug"
#define MyAppVersion "0.1-beta-debug"
#define MyAppExeName "fallout2-ce.exe"

[Setup]
AppId={{8D94A6CE-0F8E-4A6B-BA96-6B3B7E8A94C2}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
DefaultDirName={autopf}\Fallout Unified Co-op Beta
DisableDirPage=no
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\installer-output
OutputBaseFilename=Fallout-Unified-Coop-Beta-Debug-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\{#MyAppExeName}

[Files]
Source: "..\build\Debug\fallout2-ce.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\Debug\fallout2-ce.pdb"; DestDir: "{app}"; Flags: ignoreversion
Source: "cloudflared.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "cloudflared-LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Fallout Unified Co-op Beta Debug"; Filename: "{app}\{#MyAppExeName}"; Parameters: "--unified --fallout1-root=""{app}\GameData\Fallout1"" --fallout2-root=""{app}\GameData\Fallout2"" ""[debug]mode=log"" ""[debug]show_load_info=1"""; WorkingDir: "{app}"
Name: "{autodesktop}\Fallout Unified Co-op Beta Debug"; Filename: "{app}\{#MyAppExeName}"; Parameters: "--unified --fallout1-root=""{app}\GameData\Fallout1"" --fallout2-root=""{app}\GameData\Fallout2"" ""[debug]mode=log"" ""[debug]show_load_info=1"""; WorkingDir: "{app}"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"

[Run]
Filename: "{app}\{#MyAppExeName}"; Parameters: "--unified --fallout1-root=""{app}\GameData\Fallout1"" --fallout2-root=""{app}\GameData\Fallout2"" ""[debug]mode=log"" ""[debug]show_load_info=1"""; Description: "Launch Fallout Unified Co-op Beta Debug"; Flags: nowait postinstall skipifsilent

[Code]
var
  GameRootsPage: TInputDirWizardPage;

function ExistingFileEitherCase(const Root, LowerName, UpperName: String): String;
begin
  if FileExists(AddBackslash(Root) + LowerName) then
    Result := AddBackslash(Root) + LowerName
  else if FileExists(AddBackslash(Root) + UpperName) then
    Result := AddBackslash(Root) + UpperName
  else
    Result := '';
end;

function ExistingDirEitherCase(const Root, LowerName, UpperName: String): String;
begin
  if DirExists(AddBackslash(Root) + LowerName) then
    Result := AddBackslash(Root) + LowerName
  else if DirExists(AddBackslash(Root) + UpperName) then
    Result := AddBackslash(Root) + UpperName
  else
    Result := '';
end;

function HasGameData(const Root: String; NeedPatch: Boolean): Boolean;
begin
  Result := ExistingFileEitherCase(Root, 'master.dat', 'MASTER.DAT') <> '';
  Result := Result and (ExistingFileEitherCase(Root, 'critter.dat', 'CRITTER.DAT') <> '');
  if NeedPatch then
    Result := Result and (ExistingFileEitherCase(Root, 'patch000.dat', 'PATCH000.DAT') <> '');
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

function QuoteCmdArg(const Value: String): String;
begin
  Result := '"' + Value + '"';
end;

function CopyTreeWithRobocopy(const SourceDir, DestDir: String): Boolean;
var
  ResultCode: Integer;
  Params: String;
begin
  if not DirExists(SourceDir) then
  begin
    Result := True;
    exit;
  end;

  ForceDirectories(DestDir);
  Params := QuoteCmdArg(SourceDir) + ' ' + QuoteCmdArg(DestDir)
    + ' /E /COPY:DAT /DCOPY:DA /R:2 /W:1 /NFL /NDL /NJH /NJS /NP';

  Result := Exec(ExpandConstant('{sys}\robocopy.exe'), Params, '', SW_HIDE,
    ewWaitUntilTerminated, ResultCode) and (ResultCode >= 0) and (ResultCode <= 7);
end;

function CopyRequiredFile(const SourceRoot, DestRoot, LowerName, UpperName: String): Boolean;
var
  SourceFile: String;
begin
  SourceFile := ExistingFileEitherCase(SourceRoot, LowerName, UpperName);
  if SourceFile = '' then
  begin
    Result := False;
    exit;
  end;

  ForceDirectories(DestRoot);
  Result := FileCopy(SourceFile, AddBackslash(DestRoot) + LowerName, False);
end;

function CopyGameData(const SourceRoot, DestRoot: String; NeedPatch: Boolean): Boolean;
var
  SoundDir: String;
begin
  Result := CopyRequiredFile(SourceRoot, DestRoot, 'master.dat', 'MASTER.DAT');
  if not Result then exit;

  Result := CopyRequiredFile(SourceRoot, DestRoot, 'critter.dat', 'CRITTER.DAT');
  if not Result then exit;

  if NeedPatch then
  begin
    Result := CopyRequiredFile(SourceRoot, DestRoot, 'patch000.dat', 'PATCH000.DAT');
    if not Result then exit;
  end;

  SoundDir := ExistingDirEitherCase(SourceRoot, 'sound', 'SOUND');
  if SoundDir <> '' then
    Result := CopyTreeWithRobocopy(SoundDir, AddBackslash(DestRoot) + 'sound');
end;

function CopyLooseGameData(const SourceRoot, DestRoot: String): Boolean;
var
  DataDir: String;
begin
  DataDir := ExistingDirEitherCase(SourceRoot, 'data', 'DATA');
  if DataDir = '' then
  begin
    Result := True;
    exit;
  end;

  { Loose files are copied last so an owned installation's mods and official
    loose overrides retain higher priority than patch and base DAT contents. }
  Result := CopyTreeWithRobocopy(DataDir, AddBackslash(DestRoot) + 'data');
end;

function ExtractDat(const DatPath, DestData, F1Root, F2Root: String): Boolean;
var
  ResultCode: Integer;
  Params: String;
begin
  ForceDirectories(DestData);
  Params := '--extract-dat=' + QuoteCmdArg(DatPath)
    + ' --extract-to=' + QuoteCmdArg(DestData)
    + ' --fallout1-root=' + QuoteCmdArg(F1Root)
    + ' --fallout2-root=' + QuoteCmdArg(F2Root);
  Result := Exec(ExpandConstant('{app}\{#MyAppExeName}'), Params,
    ExpandConstant('{app}'), SW_HIDE, ewWaitUntilTerminated, ResultCode)
    and (ResultCode = 0);
end;

function ExtractCopiedGameData(const GameRoot, F1Root, F2Root: String;
  NeedPatch: Boolean): Boolean;
var
  DataRoot: String;
begin
  DataRoot := AddBackslash(GameRoot) + 'data';

  { Base content first, critter assets second, then the official patch layer. }
  Result := ExtractDat(AddBackslash(GameRoot) + 'master.dat', DataRoot, F1Root, F2Root);
  if not Result then exit;
  Result := ExtractDat(AddBackslash(GameRoot) + 'critter.dat', DataRoot, F1Root, F2Root);
  if not Result then exit;
  if NeedPatch then
    Result := ExtractDat(AddBackslash(GameRoot) + 'patch000.dat', DataRoot, F1Root, F2Root);
end;

function VerifyExtractedMaps(const F1Root, F2Root: String): Boolean;
begin
  Result := FileExists(AddBackslash(F1Root) + 'data\maps\V13ENT.MAP')
    and FileExists(AddBackslash(F2Root) + 'data\maps\ARTEMPLE.MAP');
end;

function EnsureFallout1PartyCompatibility(const F1Dest: String): Boolean;
var
  PartyPath: String;
  PartyText: String;
begin
  PartyPath := AddBackslash(F1Dest) + 'data\party.txt';
  ForceDirectories(AddBackslash(F1Dest) + 'data');

  PartyText := '[Party Member 0]' + #13#10
    + 'party_member_pid=16777216' + #13#10
    + 'level_minimum=0' + #13#10
    + 'level_up_every=0' + #13#10
    + 'level_pids=-1' + #13#10;

  Result := SaveStringToFile(PartyPath, PartyText, False);
end;

function EnsureFallout1EndDeathCompatibility(const F1Dest: String): Boolean;
var
  EndDeathPath: String;
  EndDeathText: String;
begin
  EndDeathPath := AddBackslash(F1Dest) + 'data\enddeath.txt';
  ForceDirectories(AddBackslash(F1Dest) + 'data');

  { Fallout 2 CE treats a missing enddeath.txt as fatal during startup. Fallout 1
    does not use Fallout 2's death-ending table. Keep the file parseable but empty
    until the Fallout 1 ending/death system is ported properly. }
  EndDeathText := '# Fallout 1 compatibility stub - no Fallout 2 death ending entries' + #13#10;
  Result := SaveStringToFile(EndDeathPath, EndDeathText, False);
end;

procedure InitializeWizard;
var
  F1: String;
  F2: String;
begin
  GameRootsPage := CreateInputDirPage(wpSelectDir,
    'Locate your Fallout games',
    'Select your original Fallout 1 and Fallout 2 installation folders.',
    'The installer will COPY the required data from your owned games into the existing beta installation. Your Steam installations are not modified.',
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
      MsgBox('Fallout 1 data was not found there. Select the folder containing master.dat and critter.dat.', mbError, MB_OK);
      Result := False;
      exit;
    end;

    if not HasGameData(GameRootsPage.Values[1], True) then
    begin
      MsgBox('Fallout 2 data was not found there. Select the folder containing master.dat, critter.dat, and patch000.dat.', mbError, MB_OK);
      Result := False;
      exit;
    end;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  F1Dest: String;
  F2Dest: String;
begin
  if CurStep <> ssPostInstall then
    exit;

  F1Dest := ExpandConstant('{app}\GameData\Fallout1');
  F2Dest := ExpandConstant('{app}\GameData\Fallout2');

  WizardForm.StatusLabel.Caption := 'Copying Fallout 1 game data...';
  if not CopyGameData(GameRootsPage.Values[0], F1Dest, False) then
    RaiseException('Failed to copy Fallout 1 game data into the beta installation.');

  WizardForm.StatusLabel.Caption := 'Copying Fallout 2 game data...';
  if not CopyGameData(GameRootsPage.Values[1], F2Dest, True) then
    RaiseException('Failed to copy Fallout 2 game data into the beta installation.');

  WizardForm.StatusLabel.Caption := 'Unpacking Fallout 1 DAT archives...';
  if not ExtractCopiedGameData(F1Dest, F1Dest, F2Dest, False) then
    RaiseException('Failed to unpack Fallout 1 DAT data. See GameData\Fallout1\data\dat-extract.log.');

  WizardForm.StatusLabel.Caption := 'Unpacking Fallout 2 DAT archives...';
  if not ExtractCopiedGameData(F2Dest, F1Dest, F2Dest, True) then
    RaiseException('Failed to unpack Fallout 2 DAT data. See GameData\Fallout2\data\dat-extract.log.');

  WizardForm.StatusLabel.Caption := 'Applying loose Fallout data overrides...';
  if not CopyLooseGameData(GameRootsPage.Values[0], F1Dest) then
    RaiseException('Failed to copy loose Fallout 1 data overrides.');
  if not CopyLooseGameData(GameRootsPage.Values[1], F2Dest) then
    RaiseException('Failed to copy loose Fallout 2 data overrides.');

  WizardForm.StatusLabel.Caption := 'Adding Fallout 1 engine compatibility data...';
  if not EnsureFallout1PartyCompatibility(F1Dest) then
    RaiseException('Failed to create Fallout 1 party compatibility data.');
  if not EnsureFallout1EndDeathCompatibility(F1Dest) then
    RaiseException('Failed to create Fallout 1 enddeath compatibility data.');

  WizardForm.StatusLabel.Caption := 'Verifying Fallout 1 and Fallout 2 maps...';
  if not VerifyExtractedMaps(F1Dest, F2Dest) then
    RaiseException('DAT extraction completed but required maps were not found (V13ENT.MAP / ARTEMPLE.MAP).');

  WizardForm.StatusLabel.Caption := 'Full Debug build and unpacked Fallout 1 + Fallout 2 data installed.';
end;
