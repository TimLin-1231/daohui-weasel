// DaoHui: a client goes on after a request of its timed out.
//
// A request that gets no answer in time closes the pipe (PipeChannel.h), so
// that a late answer is never read as the next one's. Up to 0.17.4.20 the
// client never asked for a new one: Echo() saw no pipe and returned false
// without sending anything, TimedOut() still said what the timed-out
// request had left (true), and WeaselTSF::_EnsureServerConnected took that
// for "the server is late": no reconnect, the key went to the application.
// Every key did, until the input method was switched (OPEN.md A7: Claude.exe,
// 2026-11-27, after the language bar's "redeploy" waited 203 ms).
//
// A fake server, on a pipe of its own (never DaoHuiServer's), answers some
// requests late on purpose; the client is compiled from its own sources.
// Then the real server's dispatch (ServerPart.cpp), on another pipe of the
// test's, with the language bar's commands.
// Build and run: test\TestIpcTimeout\run.bat. Exit code 0: all checks passed.

#include <WeaselIPC.h>

// The client under test, on the test's pipe. WeaselIPC.h is included once
// (#pragma once), so the macro renames only the call in ClientImpl's
// constructor.
namespace weasel {
std::wstring TestPipeName();
}
#define GetPipeName TestPipeName
#include "../../WeaselIPC/WeaselClientImpl.cpp"
#undef GetPipeName
#include "../../WeaselIPC/PipeChannel.cpp"

#include <resource.h>  // (ID_WEASELTRAY_DEPLOY)
#include <atomic>
#include <cstdio>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

// ServerPart.cpp
namespace server_part {
extern std::atomic<int> commands_run;
bool Start();
void Busy(bool busy);
}  // namespace server_part

namespace {

const DWORD kBufferSize = 64 * 1024;  // PipeChannel's default
std::wstring g_pipe_name;

// --- the fake server --------------------------------------------------------

struct FakeServer {
  // The next request of a kind is answered this many ms late (then 0 again).
  std::atomic<DWORD> late[32] = {};
  std::atomic<int> sessions_started{0};
  std::atomic<int> sessions_ended{0};
  std::atomic<int> connections{0};
  std::mutex mutex;
  std::set<DWORD> sessions;
  DWORD next_session = 100;

  void LateOnce(WEASEL_IPC_COMMAND cmd, DWORD ms) {
    late[cmd - WEASEL_IPC_ECHO] = ms;
  }
  void NoneLate() {
    for (auto& ms : late)
      ms = 0;
  }
  void Forget() {  // as a server that was restarted
    std::lock_guard<std::mutex> lock(mutex);
    sessions.clear();
  }

  DWORD Answer(const PipeMessage& msg) {
    std::lock_guard<std::mutex> lock(mutex);
    switch (msg.Msg) {
      case WEASEL_IPC_ECHO:
        return sessions.count(msg.lParam) ? msg.lParam : 0;
      case WEASEL_IPC_START_SESSION: {
        const DWORD id = next_session++;
        sessions.insert(id);
        ++sessions_started;
        return id;
      }
      case WEASEL_IPC_END_SESSION:
        sessions.erase(msg.lParam);
        ++sessions_ended;
        return 0;
      case WEASEL_IPC_PROCESS_KEY_EVENT:
        return sessions.count(msg.lParam) ? 1 : 0;  // taken
      case WEASEL_IPC_TRAY_COMMAND:
        return 1;  // (handled)
      default:
        return 0;
    }
  }

  void Serve(HANDLE pipe) {
    std::vector<char> buf(kBufferSize);
    for (;;) {
      DWORD n = 0;
      // (A request with a body is kBufferSize bytes: it fits.)
      BOOL ok = ::ReadFile(pipe, buf.data(), kBufferSize, &n, NULL);
      if (!ok || n < sizeof(PipeMessage))
        break;  // the client closed its end
      const PipeMessage msg = *reinterpret_cast<PipeMessage*>(buf.data());
      const int i = msg.Msg - WEASEL_IPC_ECHO;
      if (i >= 0 && i < 32) {
        const DWORD ms = late[i].exchange(0);
        if (ms)
          ::Sleep(ms);
      }
      const DWORD reply = Answer(msg);
      DWORD written = 0;
      if (!::WriteFile(pipe, &reply, sizeof(reply), &written, NULL))
        break;  // the client gave up waiting and closed its end
    }
    ::DisconnectNamedPipe(pipe);
    ::CloseHandle(pipe);
  }

  std::wstring name;

  HANDLE NewInstance() {
    return ::CreateNamedPipeW(
        name.c_str(), PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES, kBufferSize, kBufferSize, 0, NULL);
  }

  // The first instance exists before this returns: a client can connect.
  bool Start(const std::wstring& pipe_name) {
    name = pipe_name;
    HANDLE first = NewInstance();
    if (first == INVALID_HANDLE_VALUE)
      return false;
    std::thread([this, first] {
      HANDLE pipe = first;
      while (pipe != INVALID_HANDLE_VALUE) {
        if (::ConnectNamedPipe(pipe, NULL) ||
            ::GetLastError() == ERROR_PIPE_CONNECTED) {
          ++connections;
          std::thread([this, pipe] { Serve(pipe); }).detach();
        } else {
          ::CloseHandle(pipe);
        }
        pipe = NewInstance();
      }
    }).detach();
    return true;
  }
};

FakeServer g_server;

// --- what WeaselTSF does with the client ------------------------------------

// WeaselTSF::_Reconnect (WeaselTSF.cpp).
void Reconnect(weasel::ClientImpl& c) {
  c.Disconnect();
  c.Connect(weasel::ServerLauncher());
  c.StartSession();
}

// WeaselTSF::_EnsureServerConnected (WeaselTSF.cpp), without starting the
// server after six failures: true when a key would go to Rime, false when it
// goes to the application.
bool EnsureServerConnected(weasel::ClientImpl& c) {
  if (c.Echo(false))
    return true;
  if (c.TimedOut())
    return false;  // the server is late: the key goes to the application
  Reconnect(c);
  if (c.TimedOut())
    return false;
  return c.Echo(false);
}

// WeaselTSF::_ProcessKeyEvent: true when Rime took the key.
bool TypeKey(weasel::ClientImpl& c) {
  if (!EnsureServerConnected(c))
    return false;
  return c.ProcessKeyEvent(weasel::KeyEvent('a', 0));
}

// --- checks -------------------------------------------------------------------

int g_failures = 0;

void Check(bool ok, const char* what) {
  std::printf("  %s  %s\n", ok ? "ok  " : "FAIL", what);
  std::fflush(stdout);
  if (!ok)
    ++g_failures;
}

ULONGLONG Now() {
  return ::GetTickCount64();
}

// The real trace: a light request (the language bar's "redeploy", a
// TrayCommand: 200 ms at most) answered late, then keys.
void LightRequestLate() {
  std::printf("A light request answered late (TrayCommand, as in Claude.exe)\n");
  g_server.NoneLate();
  weasel::ClientImpl c;
  Reconnect(c);
  const int started = g_server.sessions_started;
  Check(TypeKey(c), "a key before: taken");

  g_server.LateOnce(WEASEL_IPC_TRAY_COMMAND, 700);
  c.TrayCommand(40002);
  Check(c.TimedOut(), "the TrayCommand timed out");

  // A focus change (WeaselTSF::OnSetThreadFocus: Echo(true)) within the
  // pause after a light request: skipped, no reconnect.
  Check(!c.Echo(true) && c.TimedOut(),
        "a light echo within the light pause: skipped as late");

  // Keys are not light: a light request that timed out does not pause them.
  bool all = true;
  for (int i = 0; i < 5; ++i)
    all = TypeKey(c) && all;
  Check(all, "keys right after it: taken (the pipe is opened again)");
  Check(g_server.sessions_started == started,
        "the session was kept (no new session started)");
  Check(g_server.sessions_ended == 0, "no session ended");
}

// A key answered late: every request is paused for a while (kPause, 3 s).
void KeyLate() {
  std::printf("A key answered late (all requests paused)\n");
  g_server.NoneLate();
  weasel::ClientImpl c;
  Reconnect(c);
  const int started = g_server.sessions_started;
  Check(TypeKey(c), "a key before: taken");

  g_server.LateOnce(WEASEL_IPC_PROCESS_KEY_EVENT, 1600);
  const ULONGLONG t0 = Now();
  Check(!TypeKey(c) && c.TimedOut(), "the late key: timed out");

  const int connections = g_server.connections;
  ::Sleep(300);
  Check(!TypeKey(c) && c.TimedOut(),
        "a key within the pause: to the application, as late");
  Check(g_server.connections == connections,
        "no reconnect within the pause");

  // Past the pause (3 s after the timeout) keys are taken again.
  while (Now() - t0 < 1000 + 3000 + 200)
    ::Sleep(50);
  Check(TypeKey(c), "a key after the pause: taken");
  Check(TypeKey(c), "the next key: taken");
  Check(g_server.sessions_started == started,
        "the session was kept (no new session started)");
}

// The server still late when the pause is over: the reconnect's echo times
// out too. The session is kept for the next try.
void StillLate() {
  std::printf("The server still late after the pause\n");
  g_server.NoneLate();
  weasel::ClientImpl c;
  Reconnect(c);
  const int started = g_server.sessions_started;

  g_server.LateOnce(WEASEL_IPC_PROCESS_KEY_EVENT, 1600);
  ULONGLONG t0 = Now();
  Check(!TypeKey(c) && c.TimedOut(), "the late key: timed out");
  while (Now() - t0 < 1000 + 3000 + 200)
    ::Sleep(50);

  // The first echo after the pause is Echo() itself: no pipe, not sent.
  // The reconnect's echo (StartSession) is the one answered late.
  g_server.LateOnce(WEASEL_IPC_ECHO, 1600);
  t0 = Now();
  Check(!TypeKey(c) && c.TimedOut(),
        "a key while the reconnect's echo is late: to the application");
  while (Now() - t0 < 1000 + 3000 + 200)
    ::Sleep(50);
  Check(TypeKey(c), "a key after the second pause: taken");
  Check(g_server.sessions_started == started,
        "the session was kept through both");
}

// A server that lost the session (restarted) answers the echo with 0, in
// time: a new session, as before.
void SessionLost() {
  std::printf("The server lost the session (restarted)\n");
  g_server.NoneLate();
  weasel::ClientImpl c;
  Reconnect(c);
  const int started = g_server.sessions_started;
  Check(TypeKey(c), "a key before: taken");
  g_server.Forget();
  Check(TypeKey(c), "a key after: taken");
  Check(g_server.sessions_started == started + 1, "in a new session");
}

bool WaitFor(const std::atomic<int>& n, int value, DWORD ms) {
  const ULONGLONG t0 = Now();
  while (n < value && Now() - t0 < ms)
    ::Sleep(20);
  return n >= value;
}

// The real server's dispatch (ServerPart.cpp): the language bar's commands
// are answered before they run, and run even when the client gave up.
void RealServerTrayCommand() {
  std::printf("The real server's dispatch: a slow TrayCommand (500 ms)\n");
  weasel::ClientImpl c;
  c.Connect(weasel::ServerLauncher());
  ULONGLONG t0 = Now();
  c.TrayCommand(ID_WEASELTRAY_DEPLOY);
  const ULONGLONG waited = Now() - t0;
  std::printf("        (answered in %llu ms)\n", waited);
  Check(!c.TimedOut() && waited < 150, "answered before it ran");
  Check(WaitFor(server_part::commands_run, 1, 2000), "and it ran");

  // Busy with another application's request: this client gives up.
  weasel::ClientImpl c2;
  c2.Connect(weasel::ServerLauncher());
  server_part::Busy(true);
  c2.TrayCommand(ID_WEASELTRAY_DEPLOY);
  Check(c2.TimedOut(), "while the server is busy: the client gave up");
  server_part::Busy(false);
  Check(WaitFor(server_part::commands_run, 2, 2000),
        "the command ran all the same");
}

}  // namespace

std::wstring weasel::TestPipeName() {
  return g_pipe_name;
}

int wmain() {
  // What the client and the server write about what went wrong goes to
  // %USERPROFILE%\.daohui\logs (LogEvent) and %TEMP%\rime.daohui
  // (DaoHuiServer.slow.log): not the real ones.
  wchar_t temp[MAX_PATH] = {0};
  ::GetTempPathW(MAX_PATH, temp);
  std::wstring home = std::wstring(temp) + L"TestIpcTimeout";
  ::CreateDirectoryW(home.c_str(), NULL);
  ::SetEnvironmentVariableW(L"USERPROFILE", home.c_str());
  ::SetEnvironmentVariableW(L"TEMP", home.c_str());
  ::SetEnvironmentVariableW(L"TMP", home.c_str());

  const std::wstring pid = std::to_wstring(::GetCurrentProcessId());
  g_pipe_name = L"\\\\.\\pipe\\DaoHuiIpcTimeoutTest-" + pid;
  if (!g_server.Start(g_pipe_name)) {
    std::printf("FAIL  the fake server could not create its pipe\n");
    return 2;
  }

  LightRequestLate();
  KeyLate();
  StillLate();
  SessionLost();

  // (Clients made from here on, and the real server, use this pipe.)
  g_pipe_name = L"\\\\.\\pipe\\DaoHuiIpcTimeoutTest-server-" + pid;
  if (!server_part::Start()) {
    std::printf("FAIL  the real server's dispatch did not start\n");
    return 2;
  }
  RealServerTrayCommand();

  std::printf("%s: %d failed\n", g_failures ? "FAILED" : "PASSED", g_failures);
  std::fflush(stdout);
  return g_failures ? 1 : 0;
}
