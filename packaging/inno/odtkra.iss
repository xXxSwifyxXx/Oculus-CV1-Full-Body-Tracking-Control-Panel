#define AppName "Oculus CV1 Full Body Tracking Control Panel"
#define AppVersion "2.1.0"
#define AppPublisher "ODTKRA"
#define AppExeName "odtkra_control_panel.exe"
#define AgentExeName "odtkra_agent.exe"

[Setup]
AppId={{2B34A099-3D3C-4B30-B14D-55F1579D471D}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\ODTKRA
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputBaseFilename=ODTKRA-Setup
OutputDir=..\output
Compression=lzma
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
UninstallDisplayIcon={app}\{#AppExeName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:";

[Files]
Source: "..\..\build\Release\{#AppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\build\Release\{#AgentExeName}"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExeName}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "Launch ODTKRA Control Panel"; Flags: nowait postinstall skipifsilent

[Code]
var
  SteamVrDriversPage: TInputDirWizardPage;

function InstallTouchLink(): Boolean;
var
  ResultCode: Integer;
  Params: String;
begin
  Params := '--install-touchlink --steamvr-drivers "' + SteamVrDriversPage.Values[0] + '"';
  Result := Exec(ExpandConstant('{app}\{#AgentExeName}'), Params, '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  if not Result then
  begin
    MsgBox('Unable to run OculusTouchLink install command.', mbError, MB_OK);
    Exit;
  end;

  if ResultCode = 0 then
    MsgBox('OculusTouchLink installed/updated successfully in SteamVR drivers.', mbInformation, MB_OK)
  else
    MsgBox('OculusTouchLink install/update failed. Open Control Panel and retry with "Choose SteamVR Folder".', mbError, MB_OK);

  Result := (ResultCode = 0);
end;

procedure InitializeWizard;
begin
  SteamVrDriversPage := CreateInputDirPage(
    wpSelectTasks,
    'SteamVR Driver Destination',
    'Select SteamVR drivers path for OculusTouchLink',
    'ODTKRA will install OculusTouchLink in this folder.',
    False,
    '');
  SteamVrDriversPage.Add('SteamVR drivers path:');
  SteamVrDriversPage.Values[0] := 'C:\Program Files (x86)\Steam\steamapps\common\SteamVR\drivers';
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    InstallTouchLink();
    MsgBox('Installation finished. Open Control Panel and run diagnostics after Meta/Oculus app and SteamVR are started.'#13#10#13#10'Credits: ODTKRA (DeltaNeverUsed), Oculus_Touch_Steam_Link (mm0zct), Space Calibrator (Steam).', mbInformation, MB_OK);
  end;
end;
