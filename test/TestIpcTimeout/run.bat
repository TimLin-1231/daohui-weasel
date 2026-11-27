@echo off
rem ---------------------------------------------------------------------------
rem run.bat - build and run TestIpcTimeout (x64). It starts nothing but its
rem own fake server, on a pipe of its own: DaoHuiServer is never touched.
rem From Git Bash: cmd //c "D:\src\daohui\test\TestIpcTimeout\run.bat"
rem Exit code 0: all checks passed.
rem ---------------------------------------------------------------------------
setlocal
set NoDefaultCurrentDirectoryInExePath=
set "ROOT=%~dp0..\.."
set "VSDEVCMD=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEVCMD%" (
  echo Error: VS 2022 Build Tools not found: "%VSDEVCMD%"
  exit /b 2
)
call "%VSDEVCMD%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b 2
if not defined BOOST_ROOT set "BOOST_ROOT=D:\src\deps\boost_1_84_0"

set "OUT=%ROOT%\msbuild\test\TestIpcTimeout"
if not exist "%OUT%" mkdir "%OUT%"
cl /nologo /EHsc /std:c++17 /MT /O2 /W3 /DWIN32 /DNDEBUG /DUNICODE /D_UNICODE ^
  /I"%ROOT%\include" /I"%BOOST_ROOT%" ^
  /Fo"%OUT%\\" /Fe"%OUT%\TestIpcTimeout.exe" ^
  "%~dp0TestIpcTimeout.cpp" "%~dp0ServerPart.cpp" ^
  /link /LIBPATH:"%BOOST_ROOT%\stage\lib" advapi32.lib user32.lib
if errorlevel 1 exit /b 2

"%OUT%\TestIpcTimeout.exe"
exit /b %errorlevel%
