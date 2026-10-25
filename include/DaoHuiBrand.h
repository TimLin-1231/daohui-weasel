/*
 * DaoHui identity constants.
 *
 * DaoHui is a renamed fork of Weasel (the RIME team's Windows frontend).
 * Everything that would collide with a stock Weasel installation running on
 * the same machine is defined here, so both can be installed side by side.
 * DAOHUI.md lists every identifier and its stock Weasel value.
 *
 * Keep this file ASCII-only: WeaselServer.rc includes it (rc.exe defines
 * RC_INVOKED, which hides the C++-only part).
 */
#ifndef DAOHUI_BRAND_H_
#define DAOHUI_BRAND_H_

/* Update feed for WinSparkle (FeedURL resource of DaoHuiServer.exe).
   Change the URL here only. The testing channel uses the same feed. */
#define DAOHUI_APPCAST_URL "https://raw.githubusercontent.com/TimLin-1231/rime-asus-zhuyin/main/update/appcast.xml"
#define DAOHUI_TESTING_APPCAST_URL DAOHUI_APPCAST_URL

#ifndef RC_INVOKED

/* Names. The Chinese name (U+5CF6 U+8F1D) is written with universal
   character names to keep this file ASCII. */
#define DAOHUI_CODE_NAME "DaoHui"
#define DAOHUI_NAME_EN_W L"DaoHui"
#define DAOHUI_NAME_ZH_W L"島輝"
#define DAOHUI_HOMEPAGE_URL_W L"https://github.com/TimLin-1231/rime-asus-zhuyin"

/* Binaries: daohui.dll / daohuix64.dll (TSF), daohui.ime / daohuix64.ime. */
#define DAOHUI_TSF_BASENAME "daohui"
#define DAOHUI_TSF_BASENAME_W L"daohui"
#define DAOHUI_SERVER_EXE_W L"DaoHuiServer.exe"
#define DAOHUI_DEPLOYER_EXE_W L"DaoHuiDeployer.exe"

/* Registry: HKLM and HKCU share the same path (stock: Software\Rime\Weasel).
   The 32-bit setup writes HKLM through WOW64 redirection. */
#define DAOHUI_REG_KEY_W L"Software\\DaoHui"
#define DAOHUI_REG_KEY_WOW64_W L"Software\\WOW6432Node\\DaoHui"
#define DAOHUI_UPDATES_REG_KEY "Software\\DaoHui\\Updates"
#define DAOHUI_UPDATES_REG_KEY_W L"Software\\DaoHui\\Updates"
#define DAOHUI_WER_KEY_W                          \
  L"SOFTWARE\\Microsoft\\Windows\\Windows Error " \
  L"Reporting\\LocalDumps\\DaoHuiServer.exe"

/* Default user data directory (stock: %AppData%\Rime) and log directory
   (stock: %TEMP%\rime.weasel, log files rime.weasel.*). */
#define DAOHUI_USER_DIR_DEFAULT_W L"%AppData%\\DaoHui"
#define DAOHUI_LOG_DIR_W L"%TEMP%\\rime.daohui"
#define DAOHUI_LOG_APP_NAME "rime.daohui"

/* IPC and other kernel / window object names. */
#define DAOHUI_IPC_PIPE_NAME_W L"DaoHuiNamedPipe"
#define DAOHUI_IPC_WINDOW_W L"DaoHuiIPCWindow_1.0"
#define DAOHUI_SERVER_MUTEX_PREFIX_W L"(DAOHUI)DaoHuiServer-"
#define DAOHUI_DEPLOYER_MUTEX_W L"DaoHuiDeployerMutex"
#define DAOHUI_DEPLOYER_EXCLUSIVE_MUTEX_W L"DaoHuiDeployerExclusiveMutex"
#define DAOHUI_SERVICE_NAME_W L"DaoHuiInputService"
#define DAOHUI_IME_UI_CLASS_W L"DaoHuiUIClass"
#define DAOHUI_TSF_LOG_ENV_W L"DAOHUI_TSF_LOG"

/* TSF GUIDs (fresh uuid4 values; stock Weasel CLSID is
   {A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}). */
#define DAOHUI_CLSID_TEXTSERVICE_WSTR L"{FBE69A4F-6AE3-456A-A1B9-F13ACE2ACBD8}"
#define DAOHUI_GUID_PROFILE_WSTR L"{9A01A79E-97A6-491A-8E0B-7B54EC00E330}"
/* {FBE69A4F-6AE3-456A-A1B9-F13ACE2ACBD8} */
#define DAOHUI_CLSID_TEXTSERVICE_INIT \
  {0xfbe69a4f, 0x6ae3, 0x456a, {0xa1, 0xb9, 0xf1, 0x3a, 0xce, 0x2a, 0xcb, 0xd8}}
/* {9A01A79E-97A6-491A-8E0B-7B54EC00E330} */
#define DAOHUI_GUID_PROFILE_INIT \
  {0x9a01a79e, 0x97a6, 0x491a, {0x8e, 0x0b, 0x7b, 0x54, 0xec, 0x00, 0xe3, 0x30}}
/* {FB05F849-DC49-42F8-A2C3-6A7C7174C5F8} */
#define DAOHUI_GUID_LANGBAR_ITEM_BUTTON_INIT \
  {0xfb05f849, 0xdc49, 0x42f8, {0xa2, 0xc3, 0x6a, 0x7c, 0x71, 0x74, 0xc5, 0xf8}}
/* {1A8D1CEB-FD31-46A3-A232-9B1DBC93D251} */
#define DAOHUI_GUID_DISPLAY_ATTRIBUTE_INPUT_INIT \
  {0x1a8d1ceb, 0xfd31, 0x46a3, {0xa2, 0x32, 0x9b, 0x1d, 0xbc, 0x93, 0xd2, 0x51}}
/* {6819EB7E-CCEC-4ABC-8FEC-F3397FD15762} */
#define DAOHUI_GUID_IME_MODE_PRESERVED_KEY_INIT \
  {0x6819eb7e, 0xccec, 0x4abc, {0x8f, 0xec, 0xf3, 0x39, 0x7f, 0xd1, 0x57, 0x62}}
/* {B81852D0-AD48-43E7-BF8E-127456760B42} */
#define DAOHUI_GUID_CANDIDATE_LIST_INIT \
  {0xb81852d0, 0xad48, 0x43e7, {0xbf, 0x8e, 0x12, 0x74, 0x56, 0x76, 0x0b, 0x42}}

#endif /* RC_INVOKED */

#endif /* DAOHUI_BRAND_H_ */
