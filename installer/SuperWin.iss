; Inno Setup script for SuperWin.
; Produces a single wizard setup.exe that installs to Program Files, offers a
; "launch at sign-in" option, silently installs the Windows App SDK runtime if
; needed, and registers a clean uninstaller.
;
; Build:  "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\SuperWin.iss
; (expects the Release build output and the WindowsAppRuntime redist staged --
;  see the [Files] section paths below.)

#define AppName "SuperWin"
#define AppVersion "1.3.0"
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
SetupIconFile=..\SuperWin.ico
OutputDir=Output
OutputBaseFilename=SuperWin-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Per-user install: no UAC prompt ("simple and quick"), and the per-user
; autostart (HKCU) + %APPDATA% data line up correctly. Users may still elect a
; machine-wide install via the dialog if they want.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked
Name: "autostart"; Description: "Launch {#AppName} when I sign in"; GroupDescription: "Startup:"

[Files]
; SuperWin is built self-contained, so the entire Release output (exe,
; WinSparkle.dll, the Windows App Runtime DLLs, resources.pri, the
; Microsoft.UI.Xaml theme folder and locale subfolders) must ship. Exclude
; dev-only artifacts and the test binary.
Source: "..\build\Release\*"; DestDir: "{app}"; \
    Excludes: "*.pdb,*.ilk,*.exp,*.lib,SuperWin_tests.exe,pri_dump*,app_only.pri,Catch2*"; \
    Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\assets\*"; DestDir: "{app}\assets"; \
    Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

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
; Self-contained build: no Windows App Runtime redist step needed.
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
