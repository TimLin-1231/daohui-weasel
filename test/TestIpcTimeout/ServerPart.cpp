// DaoHui: the real server's dispatch (WeaselServerImpl.cpp) on the test's
// pipe, for the language bar's commands (TrayCommand). Run() only: no
// Start(), so no window and no single-instance mutex, and DaoHuiServer is
// never touched. See TestIpcTimeout.cpp.

#include "../../WeaselIPCServer/stdafx.h"
#include <WeaselIPC.h>

namespace weasel {
std::wstring TestPipeName();
}
#define GetPipeName TestPipeName
#include "../../WeaselIPCServer/WeaselServerImpl.cpp"
#undef GetPipeName
#include "../../WeaselIPCServer/SecurityAttribute.cpp"

#include <atomic>
#include <thread>

CAppModule _Module;

namespace server_part {

std::atomic<int> commands_run{0};

// A command as slow as "redeploy" at its slowest (it starts the deployer).
bool SlowCommand() {
  ::Sleep(500);
  ++commands_run;
  return true;
}

// The server listens on TestPipeName() when this returns true.
bool Start() {
  _Module.Init(NULL, ::GetModuleHandle(NULL));
  static weasel::Server server;  // (its pipe: TestPipeName(), now)
  server.AddMenuHandler(ID_WEASELTRAY_DEPLOY, SlowCommand);
  std::thread([] { server.Run(); }).detach();
  const std::wstring name = weasel::TestPipeName();
  for (int i = 0; i < 100; ++i) {
    if (::WaitNamedPipeW(name.c_str(), 50))
      return true;
    ::Sleep(50);
  }
  return false;
}

// Holds the lock every request is handled under: a server busy with
// another application's request.
void Busy(bool busy) {
  if (busy)
    g_api_mutex.lock();
  else
    g_api_mutex.unlock();
}

}  // namespace server_part
