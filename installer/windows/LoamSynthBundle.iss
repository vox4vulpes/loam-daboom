; Windows installer, built once for the whole bundle and once per synth.
;
; Built by installer/windows/build-installer.sh, which passes:
;   AppVersion  e.g. 1.2.0 (from the release tag)
;   AppName     "Loam Synth Bundle" or a synth's name, e.g. "Loam Eight Sub"
;   AppKey      fixed per product: LoamSynthBundle, LoamEightSub, ...
;   OutputName  installer file name without .exe
;   StageDir    folder with VST3\<synth>.vst3, Standalone\<synth>.exe, Licenses\
;   IconsFile   generated [Icons] lines, one Start-menu entry per standalone app
;
; Every installer writes into the same shared folders (Common Files\VST3\Loam
; and Program Files\Loam) with Inno's shared-file reference counting, so the
; bundle and a single-synth installer can both be installed without the DAW
; seeing a plugin twice, and uninstalling one leaves the other's files alone.
;
; Updating: AppId stays fixed per product, so a newer installer upgrades in
; place. InitializeSetup refuses to downgrade and asks before reinstalling the
; same version.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef AppName
  #define AppName "Loam Synth Bundle"
#endif
#ifndef AppKey
  #define AppKey "LoamSynthBundle"
#endif
#ifndef OutputName
  #define OutputName "1Bundle-Loam-Synth-Setup-" + AppVersion
#endif
#define Vst3Dir "{commoncf64}\VST3\Loam"

[Setup]
AppId={#AppKey}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=Loam
AppPublisherURL=https://github.com/vox4vulpes/loam-synth-vst
AppSupportURL=https://github.com/vox4vulpes/loam-synth-vst/issues
AppUpdatesURL=https://github.com/vox4vulpes/loam-synth-vst/releases
DefaultDirName={autopf}\Loam
UsePreviousAppDir=no
DisableDirPage=yes
DefaultGroupName=Loam
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputBaseFilename={#OutputName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
UninstallDisplayName={#AppName}

[InstallDelete]
; v1.1-v1.2 bundle installs used per-bundle folders; move them to the shared ones.
Type: filesandordirs; Name: "{commoncf64}\VST3\Loam Synth Bundle"
Type: filesandordirs; Name: "{autopf}\Loam Synth Bundle"
Type: filesandordirs; Name: "{autoprograms}\Loam Synth Bundle"

[Files]
Source: "{#StageDir}\VST3\*"; DestDir: "{#Vst3Dir}"; Excludes: "*.lib,*.exp"; Flags: ignoreversion recursesubdirs createallsubdirs sharedfile uninsnosharedfileprompt
Source: "{#StageDir}\Standalone\*"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist sharedfile uninsnosharedfileprompt
Source: "{#StageDir}\Licenses\*"; DestDir: "{app}\Licenses"; Flags: ignoreversion skipifsourcedoesntexist sharedfile uninsnosharedfileprompt

[Icons]
#ifdef IconsFile
  #include IconsFile
#endif
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"

[Messages]
FinishedLabel=Setup has installed [name] on your computer.%n%nThe plugins are in Common Files\VST3\Loam - rescan plugins in your DAW to pick them up.

[Code]
// Pops the next dot-separated number off S ("1.2.3" -> 1, S becomes "2.3").
// Anything non-numeric (e.g. "0-rc1") counts as 0.
function NextVersionPart(var S: String): Integer;
var
  P: Integer;
begin
  P := Pos('.', S);
  if P = 0 then
  begin
    Result := StrToIntDef(S, 0);
    S := '';
  end
  else
  begin
    Result := StrToIntDef(Copy(S, 1, P - 1), 0);
    Delete(S, 1, P);
  end;
end;

// -1 if A < B, 0 if equal, 1 if A > B (compares up to four parts).
function CompareVersions(A, B: String): Integer;
var
  I, X, Y: Integer;
begin
  Result := 0;
  for I := 1 to 4 do
  begin
    X := NextVersionPart(A);
    Y := NextVersionPart(B);
    if X <> Y then
    begin
      if X > Y then Result := 1 else Result := -1;
      Exit;
    end;
  end;
end;

function InitializeSetup(): Boolean;
var
  Installed: String;
  Cmp: Integer;
begin
  Result := True;
  if not RegQueryStringValue(HKLM, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{#AppKey}_is1',
                             'DisplayVersion', Installed) then
    Exit;

  Cmp := CompareVersions('{#AppVersion}', Installed);
  if Cmp < 0 then
  begin
    SuppressibleMsgBox(FmtMessage('A newer version of {#AppName} (%1) is already installed, so this older installer ({#AppVersion}) will not replace it.', [Installed]),
                       mbInformation, MB_OK, IDOK);
    Result := False;
  end
  else if Cmp = 0 then
    Result := SuppressibleMsgBox(FmtMessage('{#AppName} %1 is already installed. Reinstall it?', [Installed]),
                                 mbConfirmation, MB_YESNO, IDYES) = IDYES;
end;
