; Values are supplied by scripts/package.ps1. No build output is downloaded here.
#ifndef PackageSource
  #error PackageSource must identify the staged application directory.
#endif
#ifndef PackageOutput
  #error PackageOutput must identify the installer output directory.
#endif
#ifndef PackageVersion
  #error PackageVersion is required.
#endif
#ifndef PackageNumericVersion
  #error PackageNumericVersion is required.
#endif

[Setup]
AppId={{AC80E0A9-F20A-4989-9D9E-7EC225540F98}
AppName=CompIntel3
AppVersion={#PackageVersion}
AppPublisher=competitive-intel
AppPublisherURL=https://github.com/competitive-intel/CompIntel3
DefaultDirName={localappdata}\Programs\CompIntel3
DefaultGroupName=CompIntel3
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.19041
AppMutex=Local\CompIntel3.Desktop
CloseApplications=yes
RestartApplications=no
UninstallDisplayIcon={app}\CompIntel3.exe
OutputDir={#PackageOutput}
OutputBaseFilename=CompIntel3-{#PackageVersion}-Setup-x64
VersionInfoVersion={#PackageNumericVersion}
VersionInfoProductVersion={#PackageNumericVersion}
VersionInfoProductTextVersion={#PackageVersion}
Compression=lzma2/normal
SolidCompression=yes
WizardStyle=modern
SetupLogging=yes

[Files]
Source: "{#PackageSource}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\CompIntel3"; Filename: "{app}\CompIntel3.exe"; WorkingDir: "{app}"

[Run]
Filename: "{app}\CompIntel3.exe"; Description: "Launch CompIntel3"; Flags: nowait postinstall skipifsilent
