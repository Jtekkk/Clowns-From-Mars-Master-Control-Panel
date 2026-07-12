; ============================================================================
;  Inno Setup script for Clowns From Mars — Master Control Panel (Windows x64)
;  Installs the VST3 plugin into the system-wide Common Files\VST3 folder.
;  Built on Windows by the GitHub Actions workflow (.github/workflows).
; ============================================================================

#define AppName    "Clowns From Mars Master Control Panel"
#define AppVersion "1.0.0"
#define Publisher  "Clowns From Mars"

; Path to the built .vst3 bundle. Overridable from the command line:
;   ISCC.exe /DSourceVst3="C:\path\to\...vst3"
#ifndef SourceVst3
  #define SourceVst3 "..\build\MasterControlPanel_artefacts\Release\VST3\Clowns From Mars Master Control Panel.vst3"
#endif

[Setup]
AppId={{7C2B9E10-1A2B-4C3D-9E4F-5A6B7C8D9E0F}}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#Publisher}
AppPublisherURL=https://clownsfrommars.audio
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableWelcomePage=no
OutputDir=Output
OutputBaseFilename=ClownsFromMars-MasterControlPanel-{#AppVersion}-Windows-x64-Setup
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
UninstallDisplayName={#AppName}
PrivilegesRequired=admin

[Messages]
WelcomeLabel2=This will install the {#AppName} VST3 plugin (64-bit) into your system VST3 folder:%n%n{commoncf64}\VST3%n%nMake sure your DAW is closed before continuing.

[Files]
Source: "{#SourceVst3}\*"; DestDir: "{commoncf64}\VST3\{#AppName}.vst3"; \
    Flags: recursesubdirs createallsubdirs ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\{#AppName}.vst3"
