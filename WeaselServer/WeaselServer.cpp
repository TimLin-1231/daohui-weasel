// WeaselServer.cpp : main source file for WeaselServer.exe
//
//	WTL MessageLoop 封装了消息循环. 实现了 getmessage/dispatchmessage....

#include "stdafx.h"
#include "resource.h"
#include "WeaselService.h"
#include <WeaselIPC.h>
#include <WeaselUI.h>
#include <RimeWithWeasel.h>
#include <WeaselUtility.h>
#include <winsparkle.h>
#include <functional>
#include <ShellScalingApi.h>
#include <WinUser.h>
#include <memory>
#include <atlstr.h>
#pragma comment(lib, "Shcore.lib")
CAppModule _Module;

// DaoHui: the server answers every key of every application, so it runs at
// normal priority whoever started it. A process inherits its parent's CPU
// priority class and, from a Task Scheduler task of priority 5-7, a low I/O
// and memory priority as well: the learning-data backup task restarts the
// server, and at background priority a busy machine kept it from answering
// within the front end's time limit (the keys then went to the application as
// letters). Lowering is never wanted here; raising to normal needs no
// privilege.
static void UseNormalPriority() {
  HANDLE self = GetCurrentProcess();
  SetPriorityClass(self, NORMAL_PRIORITY_CLASS);
  MEMORY_PRIORITY_INFORMATION memory = {MEMORY_PRIORITY_NORMAL};
  SetProcessInformation(self, ProcessMemoryPriority, &memory, sizeof(memory));
  // I/O priority has no documented setter for a process.
  typedef LONG(NTAPI * SetInformationProcess)(HANDLE, ULONG, PVOID, ULONG);
  HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  auto set = ntdll ? reinterpret_cast<SetInformationProcess>(
                         GetProcAddress(ntdll, "NtSetInformationProcess"))
                   : nullptr;
  if (set) {
    const ULONG kProcessIoPriority = 33;
    ULONG io_normal = 2;
    set(self, kProcessIoPriority, &io_normal, sizeof(io_normal));
  }
}

int WINAPI _tWinMain(HINSTANCE hInstance,
                     HINSTANCE /*hPrevInstance*/,
                     LPTSTR lpstrCmdLine,
                     int nCmdShow) {
  LANGID langId = get_language_id();
  SetThreadUILanguage(langId);
  SetThreadLocale(langId);

  if (!IsWindowsBlueOrLaterEx()) {
    CString info, cap;
    info.LoadStringW(IDS_STR_SYSTEM_VERSION_WARNING);
    cap.LoadStringW(IDS_STR_SYSTEM_VERSION_WARNING_CAPTION);
    MessageBoxExW(NULL, info, cap, MB_ICONERROR, langId);
    return 0;
  }
  SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

  // 防止服务进程开启输入法
  ImmDisableIME(-1);

  WCHAR user_name[20] = {0};
  DWORD size = _countof(user_name);
  GetUserName(user_name, &size);
  if (!_wcsicmp(user_name, L"SYSTEM")) {
    return 1;
  }

  HRESULT hRes = ::CoInitialize(NULL);
  // If you are running on NT 4.0 or higher you can use the following call
  // instead to make the EXE free threaded. This means that calls come in on a
  // random RPC thread.
  // HRESULT hRes = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
  ATLASSERT(SUCCEEDED(hRes));

  // this resolves ATL window thunking problem when Microsoft Layer for Unicode
  // (MSLU) is used
  ::DefWindowProc(NULL, 0, 0, 0L);

  AtlInitCommonControls(
      ICC_BAR_CLASSES);  // add flags to support other controls

  hRes = _Module.Init(NULL, hInstance);
  ATLASSERT(SUCCEEDED(hRes));

  if (!wcscmp(L"/userdir", lpstrCmdLine)) {
    CreateDirectory(WeaselUserDataPath().c_str(), NULL);
    WeaselServerApp::explore(WeaselUserDataPath());
    return 0;
  }
  if (!wcscmp(L"/weaseldir", lpstrCmdLine)) {
    WeaselServerApp::explore(WeaselServerApp::install_dir());
    return 0;
  }
  if (!wcscmp(L"/ascii", lpstrCmdLine) || !wcscmp(L"/nascii", lpstrCmdLine)) {
    weasel::Client client;
    bool ascii = !wcscmp(L"/ascii", lpstrCmdLine);
    if (client.Connect())  // try to connect to running server
    {
      if (ascii)
        client.TrayCommand(ID_WEASELTRAY_ENABLE_ASCII);
      else
        client.TrayCommand(ID_WEASELTRAY_DISABLE_ASCII);
    }
    return 0;
  }

  // command line option /q stops the running server
  bool quit = !wcscmp(L"/q", lpstrCmdLine) || !wcscmp(L"/quit", lpstrCmdLine);
  // restart if already running
  {
    weasel::Client client;
    if (client.Connect())  // try to connect to running server
    {
      client.ShutdownServer();
      if (quit)
        return 0;
      int retry = 0;
      while (client.Connect() && retry < 10) {
        client.ShutdownServer();
        retry++;
        Sleep(50);
      }
      if (retry >= 10)
        return 0;
    } else if (quit)
      return 0;
  }

  bool check_updates = !wcscmp(L"/update", lpstrCmdLine);
  if (check_updates) {
    WeaselServerApp::check_update();
  }

  CreateDirectory(WeaselUserDataPath().c_str(), NULL);
  UseNormalPriority();

  int nRet = 0;
  try {
    WeaselServerApp app;
    RegisterApplicationRestart(NULL, 0);
    nRet = app.Run();
  } catch (...) {
    // bad luck...
    nRet = -1;
  }

  _Module.Term();
  ::CoUninitialize();

  return nRet;
}
