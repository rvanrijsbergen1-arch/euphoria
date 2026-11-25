; Inno Setup script for Euphoria (Windows x64)
; Built automatically by .github/workflows/build.yml:  ISCC /DAppVersion=1.2.3 installer\Euphoria.iss

#define AppName "Euphoria"
#define AppExe "Euphoria.exe"
#define AppPublisher "Ephoric"
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\build\Euphoria_artefacts\Release"
#endif

[Setup]
AppId={{B6E1F4A2-3C8D-4F7E-9A51-2D6C8E0B7F43}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppName}
UsePreviousAppDir=no
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=Euphoria-{#AppVersion}-Windows-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#AppName}
UninstallDisplayIcon={app}\{#AppExe}

[Types]
Name: "full"; Description: "VST3 plugin + standalone app"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plugin (C:\Program Files\Common Files\VST3)"; Types: full custom; Flags: fixed
Name: "standalone"; Description: "Standalone app"; Types: full

[Files]
Source: "{#BuildDir}\VST3\{#AppName}.vst3\*"; DestDir: "{commoncf64}\VST3\{#AppName}.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildDir}\Standalone\{#AppExe}"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "QuickStart.txt"; DestDir: "{app}"; Flags: ignoreversion isreadme

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\{#AppName}.vst3"

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"; Components: standalone
