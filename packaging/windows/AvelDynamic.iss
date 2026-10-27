; Inno Setup script for Avel Dynamic (Windows, VST3).
; Build:  iscc /DAppVersion=1.0.0 packaging\windows\AvelDynamic.iss
; Expects the plugin at build\AvelDynamic_artefacts\Release\VST3\Avel Dynamic.vst3

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif

#define Root "..\.."

[Setup]
AppId={{D8569EAF-6976-427F-B42C-064CC8232209}
AppName=Avel Dynamic
AppVersion={#AppVersion}
AppVerName=Avel Dynamic {#AppVersion}
AppPublisher=Avel
DefaultDirName={commoncf64}\VST3\Avel Dynamic.vst3
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=no
UninstallFilesDir={autopf}\Avel\Avel Dynamic
UninstallDisplayName=Avel Dynamic {#AppVersion} (VST3)
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#Root}\dist
OutputBaseFilename=AvelDynamic-{#AppVersion}-Windows-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=no

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"

[Messages]
en.WelcomeLabel2=This will install Avel Dynamic {#AppVersion} (VST3) into%n%nC:\Program Files\Common Files\VST3%n%nClose your DAW before continuing.
ru.WelcomeLabel2=Будет установлен Avel Dynamic {#AppVersion} (VST3) в папку%n%nC:\Program Files\Common Files\VST3%n%nПеред продолжением закройте DAW.
en.FinishedLabel=Avel Dynamic is installed. Start your DAW and rescan plugins (REAPER: Preferences > Plug-ins > VST > Re-scan).
ru.FinishedLabel=Avel Dynamic установлен. Запустите DAW и пересканируйте плагины (REAPER: Preferences > Plug-ins > VST > Re-scan).

[InstallDelete]
; remove files of an older version inside the bundle
Type: filesandordirs; Name: "{commoncf64}\VST3\Avel Dynamic.vst3"

[Files]
Source: "{#Root}\build\AvelDynamic_artefacts\Release\VST3\Avel Dynamic.vst3\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Root}\README.md"; DestDir: "{autopf}\Avel\Avel Dynamic"; DestName: "README.txt"; Flags: ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Avel Dynamic.vst3"
