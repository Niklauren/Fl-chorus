; Inno Setup script: FL Chorus VST3 installer for Windows (64-bit)
; Build the plugin first (see installer\windows\make-installer.bat), then compile this file with Inno Setup 6.3+.

#define AppName "FL Chorus"
#define AppVersion "1.0.0"
#define Vst3Src "..\..\build-release\FLChorus_artefacts\Release\VST3\FL Chorus.vst3"

[Setup]
AppId={{3269C319-FEF6-45EA-BE66-7AD70F402BEE}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=mikita_laurenau
AppPublisherURL=https://www.instagram.com/mikita_laurenau/
; uninstaller lives here, the plugin itself goes to the common VST3 folder
DefaultDirName={commonpf64}\mikita_laurenau\{#AppName}
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=no
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir=..\..\dist
OutputBaseFilename=FL-Chorus-{#AppVersion}-win-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#AppName}
LicenseFile=..\mac\resources\license.txt

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"

[Files]
Source: "{#Vst3Src}\*"; DestDir: "{commoncf64}\VST3\{#AppName}.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\{#AppName}.vst3"
