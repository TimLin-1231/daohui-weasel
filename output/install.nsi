; DaoHui (島輝) installation script
; Based on the Weasel (小狼毫) installer by the RIME team (GPLv3).
; Installs side by side with stock Weasel: own install dir, registry root
; (HKLM/HKCU Software\DaoHui), uninstall key, Run value and Start-menu folder.
!include FileFunc.nsh
!include LogicLib.nsh
!include MUI2.nsh
!include x64.nsh
!include winVer.nsh

Unicode true

;--------------------------------
; General

!ifndef DAOHUI_VERSION
!define DAOHUI_VERSION 0.1.0
!endif

!ifndef DAOHUI_BUILD
!define DAOHUI_BUILD 0
!endif

!ifndef PRODUCT_VERSION
!define PRODUCT_VERSION ${DAOHUI_VERSION}.${DAOHUI_BUILD}
!endif

!define DAOHUI_ROOT $INSTDIR\daohui-${DAOHUI_VERSION}
; The installer is a 32-bit process: HKLM\${DAOHUI_REG_KEY} below is
; HKLM\SOFTWARE\WOW6432Node\DaoHui, the same view DaoHuiSetup.exe (32-bit)
; writes WeaselRoot/ServerExecutable to and the text service reads.
!define DAOHUI_REG_KEY "Software\DaoHui"
!define REG_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\DaoHui"
!define DAOHUI_HOMEPAGE "https://github.com/TimLin-1231/rime-asus-zhuyin"

; The name of the installer
Name "島輝 ${DAOHUI_VERSION}"

; The file to write
OutFile "archives\daohui-${PRODUCT_VERSION}-installer.exe"

VIProductVersion "${DAOHUI_VERSION}.${DAOHUI_BUILD}"
VIAddVersionKey /LANG=1028 "ProductName" "島輝"
VIAddVersionKey /LANG=1028 "Comments" "基於 RIME 團隊的小狼毫 (Weasel) | Powered by RIME"
VIAddVersionKey /LANG=1028 "CompanyName" "島輝"
VIAddVersionKey /LANG=1028 "LegalCopyright" "GPLv3 | Copyleft RIME Developers 與島輝貢獻者"
VIAddVersionKey /LANG=1028 "FileDescription" "島輝輸入法"
VIAddVersionKey /LANG=1028 "FileVersion" "${DAOHUI_VERSION}"

!define MUI_ICON ..\resource\installer.ico
!define MUI_UNICON ..\resource\installer.ico
SetCompressor /SOLID lzma


; Request application privileges for Windows Vista
RequestExecutionLevel admin

;--------------------------------

; Pages

!insertmacro MUI_PAGE_LICENSE "LICENSE.txt"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

;--------------------------------

; Languages

!insertmacro MUI_LANGUAGE "TradChinese"
LangString DISPLAYNAME ${LANG_TRADCHINESE} "島輝輸入法"
LangString LNKFORMANUAL ${LANG_TRADCHINESE} "【島輝】說明書"
LangString LNKFORSETTING ${LANG_TRADCHINESE} "【島輝】輸入法設定"
LangString LNKFORDICT ${LANG_TRADCHINESE} "【島輝】用戶詞典管理"
LangString LNKFORSYNC ${LANG_TRADCHINESE} "【島輝】用戶資料同步"
LangString LNKFORDEPLOY ${LANG_TRADCHINESE} "【島輝】重新部署"
LangString LNKFORSERVER ${LANG_TRADCHINESE} "島輝算法服務"
LangString LNKFORUSERFOLDER ${LANG_TRADCHINESE} "【島輝】用戶文件夾"
LangString LNKFORAPPFOLDER ${LANG_TRADCHINESE} "【島輝】程序文件夾"
LangString LNKFORUPDATER ${LANG_TRADCHINESE} "【島輝】檢查新版本"
LangString LNKFORSETUP ${LANG_TRADCHINESE} "【島輝】安裝選項"
LangString LNKFORUNINSTALL ${LANG_TRADCHINESE} "卸載島輝"
LangString CONFIRMATION ${LANG_TRADCHINESE} "安裝前，請先卸載舊版本的島輝。$\n$\n按下「確定」移除舊版本，按下「取消」放棄本次安裝。"
LangString SYSTEMVERSIONNOTOK ${LANG_TRADCHINESE} "您的系統不被支持，最低系統要求：64 位元 Windows 8.1！"
LangString AUTOCHKUPDATE ${LANG_TRADCHINESE} "自動檢查版本更新？"

!insertmacro MUI_LANGUAGE "SimpChinese"
LangString DISPLAYNAME ${LANG_SIMPCHINESE} "島輝输入法"
LangString LNKFORMANUAL ${LANG_SIMPCHINESE} "【島輝】说明书"
LangString LNKFORSETTING ${LANG_SIMPCHINESE} "【島輝】输入法设定"
LangString LNKFORDICT ${LANG_SIMPCHINESE} "【島輝】用户词典管理"
LangString LNKFORSYNC ${LANG_SIMPCHINESE} "【島輝】用户资料同步"
LangString LNKFORDEPLOY ${LANG_SIMPCHINESE} "【島輝】重新部署"
LangString LNKFORSERVER ${LANG_SIMPCHINESE} "島輝算法服务"
LangString LNKFORUSERFOLDER ${LANG_SIMPCHINESE} "【島輝】用户文件夹"
LangString LNKFORAPPFOLDER ${LANG_SIMPCHINESE} "【島輝】程序文件夹"
LangString LNKFORUPDATER ${LANG_SIMPCHINESE} "【島輝】检查新版本"
LangString LNKFORSETUP ${LANG_SIMPCHINESE} "【島輝】安装选项"
LangString LNKFORUNINSTALL ${LANG_SIMPCHINESE} "卸载島輝"
LangString CONFIRMATION ${LANG_SIMPCHINESE} '安装前，请先卸载旧版本的島輝。$\n$\n点击 "确定" 移除旧版本，或点击 "取消" 放弃本次安装。'
LangString SYSTEMVERSIONNOTOK ${LANG_SIMPCHINESE} "您的系统不被支持，最低系统要求：64 位 Windows 8.1！"
LangString AUTOCHKUPDATE ${LANG_SIMPCHINESE} "自动检查版本更新？"

!insertmacro MUI_LANGUAGE "English"
LangString DISPLAYNAME ${LANG_ENGLISH} "DaoHui"
LangString LNKFORMANUAL ${LANG_ENGLISH} "DaoHui Manual"
LangString LNKFORSETTING ${LANG_ENGLISH} "DaoHui Settings"
LangString LNKFORDICT ${LANG_ENGLISH} "DaoHui Dictionary Manager"
LangString LNKFORSYNC ${LANG_ENGLISH} "DaoHui Sync User Profile"
LangString LNKFORDEPLOY ${LANG_ENGLISH} "DaoHui Deploy"
LangString LNKFORSERVER ${LANG_ENGLISH} "DaoHui Server"
LangString LNKFORUSERFOLDER ${LANG_ENGLISH} "DaoHui User Folder"
LangString LNKFORAPPFOLDER ${LANG_ENGLISH} "DaoHui App Folder"
LangString LNKFORUPDATER ${LANG_ENGLISH} "DaoHui Check for Updates"
LangString LNKFORSETUP ${LANG_ENGLISH} "DaoHui Installation Preference"
LangString LNKFORUNINSTALL ${LANG_ENGLISH} "Uninstall DaoHui"
LangString CONFIRMATION ${LANG_ENGLISH} "Before installation, please uninstall the old version of DaoHui.$\n$\nPress 'OK' to remove the old version, or 'Cancel' to abort installation."
LangString SYSTEMVERSIONNOTOK ${LANG_ENGLISH} "Your system is not supported. DaoHui requires 64-bit Windows 8.1 or later."
LangString AUTOCHKUPDATE ${LANG_ENGLISH} "Automatically check for updates?"

;--------------------------------

Function .onInit
  ; DaoHui ships x64 binaries only (no 32-bit librime). Quit on older or
  ; 32-bit Windows (ARM64 Windows 11 runs the x64 build under emulation).
  ${IfNot} ${AtLeastWin8.1}
  ${OrIfNot} ${RunningX64}
    IfSilent toquit
    MessageBox MB_OK '$(SYSTEMVERSIONNOTOK)'
toquit:
    Quit
  ${EndIf}

  ReadRegStr $R0 HKLM "${DAOHUI_REG_KEY}" "InstallDir"
  StrCmp $R0 "" 0 skip
  ; The default installation directory
  StrCpy $INSTDIR "$PROGRAMFILES64\DaoHui"
skip:
  ReadRegStr $R0 HKLM "${REG_UNINST_KEY}" "UninstallString"
  StrCmp $R0 "" done

  StrCpy $0 "Upgrade"
  IfSilent uninst 0
  MessageBox MB_OKCANCEL|MB_ICONINFORMATION "$(CONFIRMATION)" IDOK uninst
  Abort

uninst:
  ; Backup data directory from previous installation, user files may exist
  ReadRegStr $R1 HKLM "${DAOHUI_REG_KEY}" "WeaselRoot"
  StrCmp $R1 "" call_uninstaller
  IfFileExists $R1\data\*.* 0 call_uninstaller
  CreateDirectory $TEMP\daohui-backup
  CopyFiles $R1\data\*.* $TEMP\daohui-backup

call_uninstaller:
  ExecWait '"$R1\DaoHuiServer.exe" /quit'
  ExecWait '"$R1\DaoHuiSetup.exe" /u'
  ; Remove registry keys (DaoHui's own only; never Software\Rime)
  DeleteRegKey HKLM "${DAOHUI_REG_KEY}"
  DeleteRegKey HKLM "${REG_UNINST_KEY}"
  ; don't redirect on 64 bit system for auto run setting
  SetRegView 64
  DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "DaoHuiServer"
  ; recover back to 32bit view
  SetRegView 32
  ; Remove files and uninstaller
  Delete  "$R1\data\opencc\*.*"
  Delete  "$R1\data\preview\*.*"
  Delete  "$R1\data\*.*"
  Delete  "$R1\*.*"
  RMDir   "$R1\data\opencc"
  RMDir   "$R1\data\preview"
  RMDir   "$R1\data"
  RMDir   "$R1"
  SetShellVarContext all
  Delete  "$SMPROGRAMS\$(DISPLAYNAME)\*.*"
  RMDir  "$SMPROGRAMS\$(DISPLAYNAME)"
  ; Prompt reboot
  SetRebootFlag true
  Sleep 800

done:
FunctionEnd

; Registry key to check for directory (so if you install again, it will
; overwrite the old one automatically)
InstallDirRegKey HKLM "${DAOHUI_REG_KEY}" "InstallDir"

; The stuff to install
Section "DaoHui"

  SectionIn RO

  ; Write the new installation path into the registry
  WriteRegStr HKLM "${DAOHUI_REG_KEY}" "InstallDir" "$INSTDIR"

  ; Reset INSTDIR for the new version
  StrCpy $INSTDIR "${DAOHUI_ROOT}"

  IfFileExists "$INSTDIR\DaoHuiServer.exe" 0 +2
  ExecWait '"$INSTDIR\DaoHuiServer.exe" /quit'

  SetOverwrite try
  ; Set output path to the installation directory.
  SetOutPath $INSTDIR

  IfFileExists $TEMP\daohui-backup\*.* 0 program_files
  CreateDirectory $INSTDIR\data
  CopyFiles $TEMP\daohui-backup\*.* $INSTDIR\data
  RMDir /r $TEMP\daohui-backup

program_files:
  File "LICENSE.txt"
  File "README.txt"
  File "7-zip-license.txt"
  File "7z.dll"
  File "7z.exe"
  File "COPYING-curl.txt"
  File "curl.exe"
  File "curl-ca-bundle.crt"
  File "rime-install.bat"
  File "rime-install-config.bat"
  File "start_service.bat"
  File "stop_service.bat"
  ; TSF text service, 32-bit and 64-bit. DaoHuiSetup copies them to
  ; SysWOW64\daohui.dll and System32\daohui.dll and registers them.
  File "daohui.dll"
  File "daohuix64.dll"
  ; legacy IMM32 IME (registered only when "Legacy IME" is chosen)
  File "daohui.ime"
  File "daohuix64.ime"
  ; x64 server, deployer and engine
  File "DaoHuiDeployer.exe"
  File "DaoHuiServer.exe"
  File "rime.dll"
  File "WinSparkle.dll"

  File "DaoHuiSetup.exe"
  ; settings / updates / model download app (Tauri, D:\Document\輸入法\app\daohui-settings)
  File "DaoHuiSettings.exe"
  ; shared data files
  SetOutPath $INSTDIR\data
  File "data\*.yaml"
  File /nonfatal "data\*.txt"
  File /nonfatal "data\*.gram"
  ; opencc data files
  SetOutPath $INSTDIR\data\opencc
  File "data\opencc\*.json"
  File "data\opencc\*.ocd*"
  ; images
  SetOutPath $INSTDIR\data\preview
  File "data\preview\*.png"
  ; DaoHui's own data on top of the stock data (built by
  ; D:\Document\輸入法\tools\package_daohui_data.py; list in data_daohui\FILES.txt):
  ; schema, dictionaries, Lua, OpenCC tables, default.yaml / weasel.yaml
  SetOutPath $INSTDIR\data
  File /x FILES.txt "data_daohui\*.*"
  SetOutPath $INSTDIR\data\lua
  File "data_daohui\lua\*.lua"
  SetOutPath $INSTDIR\data\opencc
  File "data_daohui\opencc\*.json"
  File "data_daohui\opencc\*.ocd2"

  SetOutPath $INSTDIR

  ; test /T flag for zh_TW locale
  StrCpy $R2 "/i"
  ${GetParameters} $R0
  ClearErrors
  ${GetOptions} $R0 "/S" $R1
  IfErrors +2 0
  StrCpy $R2 "/s"
  ${GetOptions} $R0 "/T" $R1
  IfErrors +2 0
  StrCpy $R2 "/t"

  ExecWait '"$INSTDIR\DaoHuiSetup.exe" $R2'

  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "${REG_UNINST_KEY}" "DisplayName" "$(DISPLAYNAME)"
  WriteRegStr HKLM "${REG_UNINST_KEY}" "DisplayIcon" '"$INSTDIR\DaoHuiServer.exe"'
  WriteRegStr HKLM "${REG_UNINST_KEY}" "DisplayVersion" "${DAOHUI_VERSION}.${DAOHUI_BUILD}"
  WriteRegStr HKLM "${REG_UNINST_KEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKLM "${REG_UNINST_KEY}" "Publisher" "島輝"
  WriteRegStr HKLM "${REG_UNINST_KEY}" "URLInfoAbout" "${DAOHUI_HOMEPAGE}"
  WriteRegStr HKLM "${REG_UNINST_KEY}" "HelpLink" "https://rime.im/docs/"
  WriteRegDWORD HKLM "${REG_UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${REG_UNINST_KEY}" "NoRepair" 1
  WriteUninstaller "$INSTDIR\uninstall.exe"

  ; run as user...
  IfSilent deploy_silently
  ExecWait "$INSTDIR\DaoHuiDeployer.exe /install"
  GoTo deploy_done

  deploy_silently:
  ExecWait "$INSTDIR\DaoHuiDeployer.exe /deploy"
  deploy_done:

  ; don't redirect on 64 bit system for auto run setting
  SetRegView 64
  ; Write autorun key
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "DaoHuiServer" "$INSTDIR\DaoHuiServer.exe"
  SetRegView 32
  ; Start DaoHuiServer
  Exec "$INSTDIR\DaoHuiServer.exe"

  ; option CheckForUpdates (WinSparkle registry path, see DaoHuiBrand.h)
  IfSilent DisableAutoCheckUpdate
  MessageBox MB_YESNO|MB_ICONINFORMATION "$(AUTOCHKUPDATE)" IDYES EnableAutoCheckUpdate
  DisableAutoCheckUpdate:
  WriteRegStr HKCU "Software\DaoHui\Updates" "CheckForUpdates" "0"
  GoTo end
  EnableAutoCheckUpdate:
  WriteRegStr HKCU "Software\DaoHui\Updates" "CheckForUpdates" "1"
  end:

  ; Prompt reboot
  StrCmp $0 "Upgrade" 0 +2
  SetRebootFlag true

SectionEnd

; Optional section (can be disabled by the user)
Section "Start Menu Shortcuts"
  SetShellVarContext all
  CreateDirectory "$SMPROGRAMS\$(DISPLAYNAME)"
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\島輝設定.lnk" "$INSTDIR\DaoHuiSettings.exe"
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORMANUAL).lnk" "$INSTDIR\README.txt"
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORSETTING).lnk" "$INSTDIR\DaoHuiDeployer.exe" "" "$SYSDIR\shell32.dll" 21
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORDICT).lnk" "$INSTDIR\DaoHuiDeployer.exe" "/dict" "$SYSDIR\shell32.dll" 6
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORSYNC).lnk" "$INSTDIR\DaoHuiDeployer.exe" "/sync" "$SYSDIR\shell32.dll" 26
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORDEPLOY).lnk" "$INSTDIR\DaoHuiDeployer.exe" "/deploy" "$SYSDIR\shell32.dll" 144
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORSERVER).lnk" "$INSTDIR\DaoHuiServer.exe" "" "$INSTDIR\DaoHuiServer.exe" 0
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORUSERFOLDER).lnk" "$INSTDIR\DaoHuiServer.exe" "/userdir" "$SYSDIR\shell32.dll" 126
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORAPPFOLDER).lnk" "$INSTDIR\DaoHuiServer.exe" "/weaseldir" "$SYSDIR\shell32.dll" 19
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORUPDATER).lnk" "$INSTDIR\DaoHuiServer.exe" "/update" "$SYSDIR\shell32.dll" 13
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORSETUP).lnk" "$INSTDIR\DaoHuiSetup.exe" "" "$SYSDIR\shell32.dll" 162
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORUNINSTALL).lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0

SectionEnd

;--------------------------------

; Uninstaller

Section "Uninstall"

  ExecWait '"$INSTDIR\DaoHuiServer.exe" /quit'

  ExecWait '"$INSTDIR\DaoHuiSetup.exe" /u'

  ; Remove registry keys (DaoHui's own only; never Software\Rime)
  DeleteRegKey HKLM "${DAOHUI_REG_KEY}"
  DeleteRegKey HKLM "${REG_UNINST_KEY}"
  ; don't redirect on 64 bit system for auto run setting
  SetRegView 64
  DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "DaoHuiServer"
  SetRegView 32

  ; Remove files and uninstaller
  SetOutPath $TEMP
  Delete  "$INSTDIR\data\opencc\*.*"
  Delete  "$INSTDIR\data\lua\*.*"
  Delete  "$INSTDIR\data\preview\*.*"
  Delete  "$INSTDIR\data\*.*"
  Delete  "$INSTDIR\*.*"
  RMDir  "$INSTDIR\data\opencc"
  RMDir  "$INSTDIR\data\lua"
  RMDir  "$INSTDIR\data\preview"
  RMDir  "$INSTDIR\data"
  RMDir  "$INSTDIR"
  SetShellVarContext all
  Delete  "$SMPROGRAMS\$(DISPLAYNAME)\*.*"
  RMDir  "$SMPROGRAMS\$(DISPLAYNAME)"

  ; Prompt reboot
  SetRebootFlag true

SectionEnd
