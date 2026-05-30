; Inno Setup script for SuperWin.
; Produces a single wizard setup.exe that installs to Program Files, offers a
; "launch at sign-in" option, silently installs the Windows App SDK runtime if
; needed, and registers a clean uninstaller.
;
; Build:  "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\SuperWin.iss
; (expects the Release build output and the WindowsAppRuntime redist staged --
;  see the [Files] section paths below.)

#define AppName "SuperWin"
#define AppVersion "0.1.0"
#define AppPublisher "SuperWin"
#define AppExe "SuperWin.exe"

[Setup]
AppId={{B7E6B3A2-1C4D-4E2A-9F3B-5A2C7D81E4F0}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
UninstallDisplayIcon={app}\{#AppExe}
OutputDir=Output
OutputBaseFilename=SuperWin-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked
Name: "autostart"; Description: "Launch {#AppName} when I sign in"; GroupDescription: "Startup:"

[Files]
; Main application (point Source at your Release build output directory).
Source: "..\build\Release\{#AppExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\Release\*.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "..\assets\*"; DestDir: "{app}\assets"; Flags: ignoreversion recursesubdirs createallsubdirs
; Windows App SDK runtime redistributable (stage it here before building the installer).
Source: "redist\WindowsAppRuntimeInstall-x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall skipifsourcedoesntexist

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Registry]
; Per-user autostart entry (only when the task is selected). The app also exposes
; this toggle in Settings; both write the same value.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; \
    ValueType: string; ValueName: "SuperWin"; \
    ValueData: """{app}\{#AppExe}"" --autostart"; \
    Flags: uninsdeletevalue; Tasks: autostart

[Run]
; Install the Windows App SDK runtime quietly (no-op/repair if already present).
Filename: "{tmp}\WindowsAppRuntimeInstall-x64.exe"; Parameters: "--quiet"; \
    StatusMsg: "Installing Windows App Runtime..."; \
    Flags: waituntilterminated skipifdoesntexist
; Offer to launch after install.
Filename: "{app}\{#AppExe}"; Description: "Launch {#AppName}"; \
    Flags: nowait postinstall skipifsilent

[UninstallRun]
; Stop a running instance before files are removed.
Filename: "{cmd}"; Parameters: "/C taskkill /IM {#AppExe} /F"; \
    Flags: runhidden; RunOnceId: "StopSuperWin"

[UninstallDelete]
; Remove per-user data on uninstall (the app stores settings/history here).
Type: filesandordirs; Name: "{userappdata}\SuperWin"
