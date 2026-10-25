@echo off
rem ---------------------------------------------------------------------------
rem build_daohui.bat - build DaoHui from source. Build only: it never
rem installs, registers or starts anything.
rem
rem Usage (run by full path, e.g. from Git Bash: cmd //c "D:\src\daohui\build_daohui.bat"):
rem   build_daohui.bat [rebuild] [installer]
rem     rebuild    clean rebuild instead of an incremental build
rem     installer  also compile output\install.nsi with makensis (only creates
rem                output\archives\daohui-*-installer.exe; does not run it)
rem
rem Inputs (see DAOHUI.md):
rem   BOOST_ROOT          default D:\src\deps\boost_1_84_0 with stage\lib holding
rem                       libboost_{serialization,wserialization,thread,chrono}
rem                       -vc143-mt-s-{x32,x64}-1_84.lib
rem   output\rime.dll     x64 librime (1.13.1 from Weasel 0.17.4); lib64\rime.lib
rem                       is generated from it when missing
rem   include\rime_*.h    librime public headers (committed)
rem   DAOHUI_BUILD        optional 4th version number (default: commits since 0.17.4)
rem
rem Outputs in output\: daohuix64.dll daohui.dll daohuix64.ime daohui.ime
rem   DaoHuiServer.exe DaoHuiDeployer.exe (x64) DaoHuiSetup.exe (Win32)
rem ---------------------------------------------------------------------------
setlocal
rem This machine sets NoDefaultCurrentDirectoryInExePath; VsDevCmd and the
rem helper calls below expect the classic search order.
set NoDefaultCurrentDirectoryInExePath=

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

rem Target suffix: empty = incremental build, :Rebuild = clean rebuild.
set T=
set build_installer=0
:parse_args
if "%~1"=="" goto args_done
if /i "%~1"=="rebuild" set T=:Rebuild
if /i "%~1"=="installer" set build_installer=1
shift
goto parse_args
:args_done

set "VSDEVCMD=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEVCMD%" (
  echo Error: VS 2022 Build Tools not found: "%VSDEVCMD%"
  exit /b 1
)
call "%VSDEVCMD%" -no_logo
if errorlevel 1 exit /b 1

if not defined BOOST_ROOT set "BOOST_ROOT=D:\src\deps\boost_1_84_0"
if not exist "%BOOST_ROOT%\boost" (
  echo Error: Boost not found at %BOOST_ROOT%
  exit /b 1
)
set PLATFORM_TOOLSET=v143

cd /d "%ROOT%"

rem --- version ---------------------------------------------------------------
set VERSION_MAJOR=0
set VERSION_MINOR=17
set VERSION_PATCH=4
if not defined DAOHUI_BUILD (
  for /f %%i in ('git rev-list 0.17.4..HEAD --count 2^>nul') do set DAOHUI_BUILD=%%i
)
if not defined DAOHUI_BUILD set DAOHUI_BUILD=0
set PRODUCT_VERSION=%VERSION_MAJOR%.%VERSION_MINOR%.%VERSION_PATCH%.%DAOHUI_BUILD%
set FILE_VERSION=%PRODUCT_VERSION%
echo DaoHui %PRODUCT_VERSION%  (BOOST_ROOT=%BOOST_ROOT%)

cscript //nologo render.js weasel.props BOOST_ROOT PLATFORM_TOOLSET VERSION_MAJOR VERSION_MINOR VERSION_PATCH PRODUCT_VERSION FILE_VERSION
if errorlevel 1 exit /b 1

if not exist msbuild mkdir msbuild

rem --- librime import library (x64) ------------------------------------------
if not exist output\rime.dll (
  echo Error: output\rime.dll missing. Copy the x64 rime.dll of librime 1.13.x
  echo        ^(e.g. from a Weasel 0.17.4 installation^) into output\.
  exit /b 1
)
if not exist lib64\rime.lib (
  echo Generating lib64\rime.lib from output\rime.dll
  powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$names = dumpbin /nologo /exports 'output\rime.dll' | Select-String '^\s+\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]{8}\s+(\S+)' | ForEach-Object { $_.Matches[0].Groups[1].Value };" ^
    "if (-not $names) { exit 1 };" ^
    "@('LIBRARY rime', 'EXPORTS') + $names | Set-Content -Encoding ascii 'msbuild\rime.def'"
  if errorlevel 1 exit /b 1
  lib /nologo /def:msbuild\rime.def /machine:x64 /out:lib64\rime.lib
  if errorlevel 1 exit /b 1
  if exist lib64\rime.exp del lib64\rime.exp
)

rem --- x64: text service, legacy IME, server, deployer ----------------------
msbuild weasel.sln /m /v:minimal /nologo ^
  /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 ^
  /t:WeaselTSF%T%;WeaselIME%T%;WeaselServer%T%;WeaselDeployer%T% ^
  /fl1 /flp1:logfile=msbuild\daohui_x64.log;verbosity=normal
if errorlevel 1 goto error

rem --- Win32: text service, legacy IME for 32-bit apps, setup --------------
rem WeaselServer/WeaselDeployer for Win32 are not built: no x86 librime here,
rem and the installer targets 64-bit Windows only.
msbuild weasel.sln /m /v:minimal /nologo ^
  /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 ^
  /t:WeaselTSF%T%;WeaselIME%T%;WeaselSetup%T% ^
  /fl2 /flp2:logfile=msbuild\daohui_x86.log;verbosity=normal
if errorlevel 1 goto error

rem --- files the installer packs next to the binaries ------------------------
copy /y LICENSE.txt output\LICENSE.txt >nul
copy /y README.md output\README.txt >nul

set "MAKENSIS=%ProgramFiles(x86)%\NSIS\makensis.exe"
if %build_installer% == 1 call :installer
if errorlevel 1 goto error

echo.
echo Build succeeded. Outputs:
for %%f in (daohuix64.dll daohui.dll daohuix64.ime daohui.ime DaoHuiServer.exe DaoHuiDeployer.exe DaoHuiSetup.exe) do (
  if exist output\%%f (echo   output\%%f) else (echo   MISSING output\%%f)
)
exit /b 0

:installer
if not exist "%MAKENSIS%" (
  echo Error: makensis not found: "%MAKENSIS%"
  exit /b 1
)
"%MAKENSIS%" /DDAOHUI_VERSION=%VERSION_MAJOR%.%VERSION_MINOR%.%VERSION_PATCH% /DDAOHUI_BUILD=%DAOHUI_BUILD% /DPRODUCT_VERSION=%PRODUCT_VERSION% output\install.nsi
exit /b %errorlevel%

:error
echo.
echo Build FAILED (logs: msbuild\daohui_x64.log, msbuild\daohui_x86.log)
exit /b 1
