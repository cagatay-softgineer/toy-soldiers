; v1.0 #163: Inno Setup script for the Windows installer.
; Build: run scripts/package.ps1 first (stages dist/), then compile this with Inno Setup 6+:
;   iscc scripts\installer.iss
; The portable zip remains the primary distribution; this is the one-click option.

#define AppName "Oyuncak Asker Masa Savasi"
#define AppVersion "1.2.0"
#define AppPublisher "preunec / cagatay-softgineer"
#define AppURL "https://github.com/cagatay-softgineer/toy-soldiers"
#define StageDir "..\dist\toy-soldiers-v" + AppVersion

[Setup]
AppId={{8E4A2B7C-5D31-4F60-9B1E-TOYSOLDIERS10}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}/issues
DefaultDirName={autopf}\ToySoldiers
DefaultGroupName=Toy Soldiers
DisableProgramGroupPage=yes
LicenseFile={#StageDir}\LICENSE
OutputDir=..\dist
OutputBaseFilename=toy-soldiers-v{#AppVersion}-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "turkish"; MessagesFile: "compiler:Languages\Turkish.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#StageDir}\toy_soldiers.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\data\cards.json"; DestDir: "{app}\data"; Flags: ignoreversion
Source: "{#StageDir}\version.json"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\README.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\THIRD_PARTY_LICENSES.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\CHANGELOG.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\toy_soldiers.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\toy_soldiers.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\toy_soldiers.exe"; Description: "{cm:LaunchProgram,{#AppName}}"; WorkingDir: "{app}"; Flags: nowait postinstall skipifsilent
