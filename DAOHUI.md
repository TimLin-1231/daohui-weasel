# 島輝 DaoHui: fork notes

DaoHui (島輝) is a renamed, independent fork of [Weasel 小狼毫](https://github.com/rime/weasel)
0.17.4 (tag `0.17.4`, commit `9cc96e2`) by the RIME team, licensed under GPLv3 like
the original. Branch `daohui` of this repository holds:

1. `keep-composition TSF patch`: the TSF keeps the inline text when focus leaves or the
   application ends the composition (identical to `keep-composition.patch` v3 of the
   input-method project).
2. Build inputs (afxres.h shim, librime API headers).
3. The rename, so DaoHui can be installed **side by side** with a stock Weasel.

Every identity constant that could collide with stock Weasel lives in one header,
[`include/DaoHuiBrand.h`](include/DaoHuiBrand.h). Change values there, not in the
sources.

## Renamed identifiers

### Text service (TSF) GUIDs

All values are fresh `uuid4` GUIDs.

| What | Stock Weasel | DaoHui | Where |
|---|---|---|---|
| Text service CLSID | `{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}` | `{FBE69A4F-6AE3-456A-A1B9-F13ACE2ACBD8}` | `DAOHUI_CLSID_TEXTSERVICE_*`; WeaselTSF/Globals.cpp, WeaselSetup/imesetup.cpp |
| Language profile GUID (zh-Hans, zh-Hant, HK, MO, SG) | `{3D02CAB6-2B8E-4781-BA20-1C9267529467}` | `{9A01A79E-97A6-491A-8E0B-7B54EC00E330}` | `DAOHUI_GUID_PROFILE_*`; same files |
| `InstallLayoutOrTip` strings | `0804:{A3F4CDED…}{3D02CAB6…}`, `0404:…` | `0804:{FBE69A4F…}{9A01A79E…}`, `0404:…` | imesetup.cpp `PSZTITLE_HANS/HANT` |
| Language bar button GUID | `{341F9E3A-B8AD-499D-936C-48701E329FB2}` | `{FB05F849-DC49-42F8-A2C3-6A7C7174C5F8}` | Globals.cpp |
| Display attribute (input) GUID | `{2AC87E79-3260-4B32-9DEA-F8390976C20B}` | `{1A8D1CEB-FD31-46A3-A232-9B1DBC93D251}` | Globals.cpp |
| Preserved key (Shift) GUID | `{0BD899FC-A8F7-4B42-A96D-CEC7C50E0EAE}` | `{6819EB7E-CCEC-4ABC-8FEC-F3397FD15762}` | Globals.cpp `GUID_IME_MODE_PRESERVED_KEY` |
| Candidate list UI element GUID | `{36C3C795-7159-45AA-AB12-30229A51DBD3}` | `{B81852D0-AD48-43E7-BF8E-127456760B42}` | WeaselTSF/CandidateList.cpp |

`GUID_LBI_INPUTMODE` and the `GUID_TFCAT_*` values are Microsoft-defined and unchanged.
The Visual Studio project GUIDs in `weasel.sln` are build-time only and unchanged.

### Binaries

| Stock Weasel | DaoHui | Project |
|---|---|---|
| `weaselx64.dll` (x64 TSF) | `daohuix64.dll` | WeaselTSF, Release\|x64 |
| `weasel.dll` (Win32 TSF) | `daohui.dll` | WeaselTSF, Release\|Win32 |
| `weaselARM.dll` / `weaselARM64.dll` / `weaselARM64X.dll` | `daohuiARM.dll` / `daohuiARM64.dll` / `daohuiARM64X.dll` (names only; ARM not built) | WeaselTSF |
| `weaselx64.ime` / `weasel.ime` (legacy IMM32 IME) | `daohuix64.ime` / `daohui.ime` | WeaselIME |
| `WeaselServer.exe` | `DaoHuiServer.exe` | WeaselServer |
| `WeaselDeployer.exe` | `DaoHuiDeployer.exe` | WeaselDeployer |
| `WeaselSetup.exe` | `DaoHuiSetup.exe` | WeaselSetup |
| `System32\weasel.dll`, `SysWOW64\weasel.dll` (copied by setup) | `System32\daohui.dll`, `SysWOW64\daohui.dll` | imesetup.cpp |
| `Ime File` = `weasel.ime` (keyboard layout) | `daohui.ime` | imesetup.cpp, Register.cpp |
| HKCR `CLSID\{…}` default value `Weasel` | `DaoHui` | `WEASEL_CODE_NAME` |

Project names (`WeaselTSF`, `WeaselServer`, …) and source file names are unchanged, so
upstream changes still merge; only `TargetName`/`OutputFile` differ.

### Run-time names (kernel objects, windows, services)

| What | Stock Weasel | DaoHui |
|---|---|---|
| IPC named pipe | `\\.\pipe\<user>\WeaselNamedPipe` | `\\.\pipe\<user>\DaoHuiNamedPipe` |
| IPC window class/title | `WeaselIPCWindow_1.0` | `DaoHuiIPCWindow_1.0` |
| Server single-instance mutex | `(WEASEL)Furandōru-Sukāretto-<user>` | `(DAOHUI)DaoHuiServer-<user>` |
| Deployer mutex | `WeaselDeployerMutex` | `DaoHuiDeployerMutex` |
| Deployer exclusive mutex | `WeaselDeployerExclusiveMutex` | `DaoHuiDeployerExclusiveMutex` |
| Service name (`WeaselService`) | `WeaselInputService` | `DaoHuiInputService` |
| Legacy IME UI window class (`CS_IME`, global) | `WeaselUIClass` | `DaoHuiUIClass` |
| TSF diagnostics env variable | `WEASEL_TSF_LOG` | `DAOHUI_TSF_LOG` |
| Tray popup menu name | `WeaselTray` | `DaoHuiTray` |
| TSF display attribute description | `Weasel Display Attribute Input` | `DaoHui Display Attribute Input` |
| Language bar button description | `WeaselTSF Button` | `DaoHui Button` |

There is no shared memory in 0.17.4 (IPC is the named pipe only). The tray window
class `TrayIconClass` is process-local and unchanged.

### Registry, folders, logs

| What | Stock Weasel | DaoHui |
|---|---|---|
| HKLM root (`WeaselRoot`, `ServerExecutable`, `InstallDir`) | `Software\Rime\Weasel` (32-bit view: `WOW6432Node\Rime\Weasel`) | `Software\DaoHui` (32-bit view: `WOW6432Node\DaoHui`) |
| HKCU root (`RimeUserDir`, `Language`, `Hant`, `ToggleImeOnOpenClose`, `UpdateChannel`) | `Software\Rime\Weasel` | `Software\DaoHui` |
| WinSparkle registry path | `Software\Rime\Weasel\Updates` | `Software\DaoHui\Updates` |
| Uninstall removes | `HKLM\Software\Rime\Weasel` **and** `HKLM\Software\Rime` | `HKLM\Software\DaoHui` only |
| WER crash dump key | `…\Windows Error Reporting\LocalDumps\WeaselServer.exe` | `…\LocalDumps\DaoHuiServer.exe` |
| Default user data dir | `%AppData%\Rime` | `%AppData%\DaoHui` |
| Log dir / glog app name | `%TEMP%\rime.weasel`, `rime.weasel.*` | `%TEMP%\rime.daohui`, `rime.daohui.*` |
| librime `distribution_code_name` | `Weasel` | `DaoHui` |
| librime `distribution_name` | `小狼毫` / `Weasel` | `島輝` / `DaoHui` |
| Deployer UI style generator id | `Weasel::UIStyleSettings` | `DaoHui::UIStyleSettings` |
| Install dir (NSIS) | `Program Files\Rime\weasel-<ver>` | `Program Files\DaoHui\daohui-<ver>` |
| Uninstall key (NSIS) | `…\CurrentVersion\Uninstall\Weasel` | `…\CurrentVersion\Uninstall\DaoHui` |
| Run value (NSIS, `install.bat`) | `WeaselServer` | `DaoHuiServer` |
| Start-menu folder (NSIS) | `小狼毫輸入法` / `Weasel` | `島輝輸入法` / `DaoHui` |
| Backup dir during upgrade (NSIS) | `%TEMP%\weasel-backup` | `%TEMP%\daohui-backup` |
| `rime-install-config.bat` user dir | `HKCU\SOFTWARE\Rime\Weasel` `RimeUserDir`, default `%APPDATA%\Rime` | `HKCU\SOFTWARE\DaoHui`, default `%APPDATA%\DaoHui` |

### Updates

| What | Stock Weasel | DaoHui |
|---|---|---|
| Appcast (`FeedURL`, `ManualUpdateFeedURL`) | `http://rime.github.io/release/weasel/appcast.xml` | `https://raw.githubusercontent.com/TimLin-1231/rime-asus-zhuyin/main/update/appcast.xml` |
| Testing appcast | `http://rime.github.io/testing/weasel/appcast.xml` | same URL as above |
| Tray "homepage" link | `https://rime.im/` | `https://github.com/TimLin-1231/rime-asus-zhuyin` |

The URL is the single constant `DAOHUI_APPCAST_URL` in `include/DaoHuiBrand.h`;
`WeaselServer/WeaselServer.rc` builds the `APPCAST` resources from it.
`update/appcast.xml` is only a template of the published feed.

### User-visible names

| Where | Stock Weasel | DaoHui |
|---|---|---|
| IME name (TSF profile description, IMM32 layout text, tray tooltip, message boxes) | `小狼毫` (Chinese UI) / `Weasel` | `島輝` / `DaoHui` (`get_weasel_ime_name()`) |
| `.rc` string tables, dialogs, menus (zh-Hans, zh-Hant, en) | `【小狼毫】…`, `Weasel …` | `【島輝】…`, `DaoHui …` |
| Version info `ProductName` / `FileDescription` / `InternalName` / `OriginalFilename` | `小狼毫`, `小狼毫算法服務`, `WeaselServer`, … | `島輝`, `島輝算法服務`, `DaoHuiServer`, `DaoHuiServer.exe`, … |
| Version info `CompanyName` | `式恕堂` / `Shishutang` | `島輝` / `DaoHui` |
| Version info `Comments` (credit) | `Powered by RIME \| 中州韻輸入法引擎` | `基於 RIME 團隊的小狼毫 (Weasel) \| Powered by RIME` (zh-Hans: 基于…团队…; en: `Based on Weasel (小狼毫) by the RIME team \| Powered by RIME`) |
| Version info `LegalCopyright` | `式恕堂 版權所無` / `Copyleft RIME Developers` | `GPLv3 \| Copyleft RIME Developers 與島輝貢獻者` / `… and DaoHui contributors` |
| NSIS installer, `README.md` (= installer `README.txt`) | 小狼毫 / Weasel | 島輝 / DaoHui, with the Weasel credits kept |

The brand name stays 島輝 in the zh-Hans resources too.

### Icons

Every icon is referenced from `resource/` by file name only:
`weasel.ico` (application / setup / installer), `zh.ico`, `en.ico`, `full.ico`, `half.ico`,
`reload.ico`. `WeaselSetup/WeaselSetup.ico` was removed; the setup now uses
`..\resource\weasel.ico`. New artwork can be dropped in under the same names.

## Kept on purpose (still named Weasel)

- Project, source and C++ identifiers (`WeaselTSF`, `weasel::Client`, `WEASEL_*` macros,
  `ID_WEASELTRAY_*` resource ids): internal, no run-time effect.
- Rime config id **`weasel`** (`weasel.yaml`, `weasel.custom.yaml`): the frontend
  settings file keeps its name so existing Rime user folders and schemas work unchanged.
- Registry value names `WeaselRoot` and `ServerExecutable` (under `Software\DaoHui`).
- Command-line switch `DaoHuiServer.exe /weaseldir` (opens the program folder).
- `WeaselIME/weasel.def`, `arm64x_wrapper/*` (ARM64X not built), `xmake.lua` files
  (unused; msbuild is the supported build), `.github/workflows` (upstream CI).
- Help/forum links (`https://rime.im/docs/`, `https://rime.im/discuss/`): DaoHui uses
  the same engine and settings as Weasel.

## AI correction (Ctrl+R)

Since 0.17.4.9. While composing, Ctrl+R asks a local language model to correct the
sentence; `lua/asus_fix.lua` (input-method data) builds the request and checks the
answer, this fork only carries it:

- `AddSession` sets the option `_asus_fix` (Lua asks only when a front end does).
- `ProcessKeyEvent`, after each key, takes a request Lua left in the session property
  `asus_fix_request` (`<id>\t<chat completions JSON>`) and hands it to
  `RimeWithWeasel/DaoHuiAI.cpp`, which asks in a thread of its own; before the key
  **F23** (`0xFFD4`) it puts a waiting answer into `asus_fix_result` for Lua.
- The answer cannot be pushed: the TSF client only takes a composition in reply to a
  key it sent. So `DaoHuiAI.cpp` keeps the answer for the session and presses F23 with
  `SendInput` (the English writing helper's F24 works the same way).
- `DaoHuiAI.cpp` runs llama.cpp's `llama-server` (b11559 Vulkan build, falls back to the
  CPU; `127.0.0.1:8739`, `-ngl all --reasoning off`, log in `ai\llama-server.log`) on
  Qwen3.5-4B Q4_K_M from `%LOCALAPPDATA%\DaoHui\ai` (`llama\llama-server.exe`, the
  `.gguf`), which the settings app downloads (模型 → AI 修正). The server is started at the
  first request inside a job object (it dies with DaoHuiServer) and stopped after 10
  minutes unused. Measured here: ready in 3.6 s (7.6 s the first time), answers in
  0.3–0.9 s; the very first answer after installing took 45 s while the driver compiled
  the Vulkan shaders (cached after), hence the 150 s answer timeout.
- Messages for the bubble come back as `!<message>` (model not downloaded, server did
  not start, no answer).

## Building

Requirements (already on the development machine):

- Visual Studio 2022 Build Tools: MSVC 14.44 (v143), ATL, Windows SDK 10.0.26100. MFC is
  not needed (`include/afxres.h` shim).
- Boost 1.84 headers at `D:\src\deps\boost_1_84_0` (override with `BOOST_ROOT`) and in
  its `stage\lib`: `libboost_{serialization,wserialization,thread,chrono}-vc143-mt-s-{x32,x64}-1_84.lib`.
  b2 does not recognise MSVC 14.44, so these were compiled directly with `cl`
  (`D:\src\deps\build_ser.bat`, `build_thread.bat`, `build_lib.bat`).
- librime: headers in `include/` (committed, from librime 1.17.0; the `RimeApi` and
  `RimeLeversApi` function tables are append-only and identical up to `change_page`,
  the last member used here) and the x64 `rime.dll` 1.13.1 in `output\` (not committed;
  copied from a Weasel 0.17.4 installation). `lib64\rime.lib` is generated from it.
- Data in `output\data` (not committed except `weasel.yaml` and `preview/`): the stock
  Weasel 0.17.4 shared data (`*.yaml`, `essay.txt`, `opencc/`).
- `output\rime-install.bat` (plum's installer script, not committed).

Build (only builds; never installs, registers or starts anything):

```
cmd /c "D:\src\daohui\build_daohui.bat"            :: incremental
cmd /c "D:\src\daohui\build_daohui.bat" rebuild    :: clean rebuild
cmd /c "D:\src\daohui\build_daohui.bat" installer  :: also run makensis on output\install.nsi
```

Run it by full path (the shell sets `NoDefaultCurrentDirectoryInExePath`; the script
clears it). It renders `weasel.props` (version `0.17.4.<commits since 0.17.4>`, override
with `DAOHUI_BUILD`), builds `Release|x64` (WeaselTSF, WeaselIME, WeaselServer,
WeaselDeployer) and `Release|Win32` (WeaselTSF, WeaselIME, WeaselSetup), and copies
`LICENSE.txt` and `README.md` (as `README.txt`) to `output\`. Logs:
`msbuild\daohui_x64.log`, `msbuild\daohui_x86.log`.

Outputs in `output\`: `daohuix64.dll`, `daohui.dll`, `daohuix64.ime`, `daohui.ime`,
`DaoHuiServer.exe`, `DaoHuiDeployer.exe`, `DaoHuiSetup.exe`.

Not built: Win32 `DaoHuiServer.exe`/`DaoHuiDeployer.exe` (no 32-bit librime here; the
installer is x64-only), ARM/ARM64 binaries, the test projects.

Build fix: the WeaselIME project now puts its import library/`.exp` in its intermediate
directory, because the TSF and IME targets share a base name per platform and a
parallel build failed with `LNK1104 … daohuix64.exp`.

## Installer (NSIS)

`output\install.nsi` builds `output\archives\daohui-<PRODUCT_VERSION>-installer.exe`
(`build_daohui.bat installer`, or `makensis /DDAOHUI_VERSION=0.17.4 /DDAOHUI_BUILD=<n>
/DPRODUCT_VERSION=0.17.4.<n> output\install.nsi`). It is x64-only (quits on 32-bit
Windows), installs to `Program Files\DaoHui\daohui-<version>`, runs `DaoHuiSetup.exe`
(copies `daohui.dll` into System32/SysWOW64 and registers the new CLSID/profile),
deploys with `DaoHuiDeployer.exe /install`, adds the `DaoHuiServer` Run value and starts
the server. Uninstall only removes DaoHui's own registry keys and files.

Before shipping: replace the icons in `resource/`, publish `update/appcast.xml` (with an
`<item>` per release) to the URL above, and decide whether to sign the binaries.
