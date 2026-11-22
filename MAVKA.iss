; MAVKA - Windows installer (Inno Setup 6). Built automatically by GitHub Actions.
; One run: the VST3 goes to C:\Program Files\Common Files\VST3, the standalone app to Program Files.

#define MyAppName "MAVKA"
#define MyAppVersion "0.3.0"
#ifndef SourceDir
  #define SourceDir "..\dist"
#endif

[Setup]
AppId={{8E6B2C4F-3D7A-4B8E-9C1F-5A2D7E4B9C60}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher=MAVKA Audio
DefaultDirName={autopf}\MAVKA
DefaultGroupName=MAVKA
DisableProgramGroupPage=yes
DisableDirPage=yes
OutputBaseFilename=MAVKA-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName=MAVKA - vocal handwriting distortion
CloseApplications=yes

[Tasks]
Name: "desktopicon"; Description: "Desktop shortcut for the standalone MAVKA"; Flags: unchecked

[Files]
Source: "{#SourceDir}\MAVKA.vst3\*"; DestDir: "{commoncf64}\VST3\MAVKA.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\MAVKA.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\README.md"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

[Icons]
Name: "{autoprograms}\MAVKA"; Filename: "{app}\MAVKA.exe"
Name: "{autodesktop}\MAVKA"; Filename: "{app}\MAVKA.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\MAVKA.exe"; Description: "Open the standalone MAVKA now"; Flags: nowait postinstall skipifsilent unchecked

[Messages]
FinishedLabel=MAVKA is installed.%n%nThe VST3 plugin is in C:\Program Files\Common Files\VST3.%nIn FL Studio: Options > Manage plugins > Find installed plugins.
