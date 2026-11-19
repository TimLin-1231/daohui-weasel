#include "stdafx.h"
#include "WeaselClientImpl.h"
#include <StringAlgorithm.hpp>
#include <cstdarg>
#include <cstdio>
#include <string>

using namespace weasel;

// DaoHui: how long a request waits for the server. The client runs in the UI
// thread of every application (Explorer's taskbar, browsers); stock waited
// for ever, and a server slow to answer (its candidate window drawn on a
// graphics card waking up) froze them. A request that gets no answer in time
// counts as failed: a key goes to the application.
namespace {
const DWORD kLight = 200;      // focus, caret position, the status probe
const DWORD kKey = 1000;       // keys and what they lead to; Echo
const DWORD kSession = 5000;   // starting a session; shutting the server down
// After a request timed out, requests are skipped (as timed out) for a while
// instead of each waiting again: light ones after a light one, all after
// any other.
const ULONGLONG kLightPause = 2000;
const ULONGLONG kPause = 3000;

// The event log (LogEvent) is renamed to frontend.old.log past this size.
const DWORD kLogMaxBytes = 256 * 1024;

struct LogPaths {
  std::wstring file, old_file, app;
};

LogPaths MakeLogPaths() {
  LogPaths p;
  wchar_t home[MAX_PATH] = {0};
  DWORD n = GetEnvironmentVariableW(L"USERPROFILE", home, MAX_PATH);
  if (n > 0 && n < MAX_PATH) {
    std::wstring dir = std::wstring(home) + L"\\.daohui";
    CreateDirectoryW(dir.c_str(), NULL);
    dir += L"\\logs";
    CreateDirectoryW(dir.c_str(), NULL);
    p.file = dir + L"\\frontend.log";
    p.old_file = dir + L"\\frontend.old.log";
  }
  wchar_t exe[MAX_PATH] = {0};
  GetModuleFileNameW(NULL, exe, MAX_PATH);
  const wchar_t* base = wcsrchr(exe, L'\\');
  p.app = base ? base + 1 : exe;
  return p;
}
}  // namespace

// DaoHui: one line per request that failed and per key that went to the
// application because of it, so that "the keys came out as letters" can be
// told apart afterwards (the server's log only sees what reached it). Written
// only when something went wrong. %USERPROFILE%\.daohui\logs, not %TEMP% or
// %AppData%: packaged (MSIX) applications write those to a private copy.
void weasel::LogEvent(const wchar_t* fmt, ...) {
  const DWORD saved_error = GetLastError();
  static const LogPaths paths = MakeLogPaths();  // (thread-safe init)
  if (paths.file.empty())
    return;
  wchar_t msg[400];
  va_list ap;
  va_start(ap, fmt);
  _vsnwprintf_s(msg, _TRUNCATE, fmt, ap);
  va_end(ap);
  SYSTEMTIME t;
  GetLocalTime(&t);
  wchar_t line[600];
  swprintf_s(line, L"%04u-%02u-%02u %02u:%02u:%02u.%03u %lu %ls: %ls\n",
             t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond,
             t.wMilliseconds, GetCurrentProcessId(), paths.app.c_str(), msg);
  char utf8[1800];
  int len = WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8, sizeof(utf8),
                                NULL, NULL);
  if (len > 1) {
    WIN32_FILE_ATTRIBUTE_DATA info;
    if (GetFileAttributesExW(paths.file.c_str(), GetFileExInfoStandard,
                             &info) &&
        (info.nFileSizeHigh || info.nFileSizeLow > kLogMaxBytes))
      MoveFileExW(paths.file.c_str(), paths.old_file.c_str(),
                  MOVEFILE_REPLACE_EXISTING);
    HANDLE h = CreateFileW(paths.file.c_str(), FILE_APPEND_DATA,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
      DWORD written = 0;
      WriteFile(h, utf8, static_cast<DWORD>(len - 1), &written, NULL);
      CloseHandle(h);
    }
  }
  SetLastError(saved_error);
}

ClientImpl::ClientImpl()
    : session_id(0),
      channel(GetPipeName()),
      is_ime(false),
      timed_out(false),
      paused_until(0),
      light_paused_until(0) {
  channel.SetTimeout(kKey);
  _InitializeClientInfo();
}

ClientImpl::~ClientImpl() {
  if (channel.Connected())
    Disconnect();
}

// http://stackoverflow.com/questions/557081/how-do-i-get-the-hmodule-for-the-currently-executing-code
HMODULE GetCurrentModule() {  // NB: XP+ solution!
  HMODULE hModule = NULL;
  GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                    (LPCTSTR)GetCurrentModule, &hModule);

  return hModule;
}

void ClientImpl::_InitializeClientInfo() {
  // get app name
  WCHAR exe_path[MAX_PATH] = {0};
  GetModuleFileName(NULL, exe_path, MAX_PATH);
  std::wstring path = exe_path;
  size_t separator_pos = path.find_last_of(L"\\/");
  if (separator_pos < path.size())
    app_name = path.substr(separator_pos + 1);
  else
    app_name = path;
  to_lower(app_name);
  // determine client type
  GetModuleFileName(GetCurrentModule(), exe_path, MAX_PATH);
  path = exe_path;
  to_lower(path);
  is_ime = ends_with(path, L".ime");
}

bool ClientImpl::Connect(ServerLauncher const& launcher) {
  return channel.Connect();
}

void ClientImpl::Disconnect() {
  if (_Active())
    EndSession();
  channel.Disconnect();
}

void ClientImpl::ShutdownServer() {
  _SendMessage(WEASEL_IPC_SHUTDOWN_SERVER, 0, 0);
}

bool ClientImpl::ProcessKeyEvent(KeyEvent const& keyEvent) {
  if (!_Active())
    return false;

  // (Key 0 is the status probe of WeaselTSF::OnSetThreadFocus: light.)
  LRESULT ret = _SendMessage(WEASEL_IPC_PROCESS_KEY_EVENT, keyEvent, session_id,
                             keyEvent.keycode == 0);
  return ret != 0;
}

bool ClientImpl::CommitComposition() {
  if (!_Active())
    return false;

  LRESULT ret = _SendMessage(WEASEL_IPC_COMMIT_COMPOSITION, 0, session_id);
  return ret != 0;
}

bool ClientImpl::ClearComposition() {
  if (!_Active())
    return false;

  LRESULT ret = _SendMessage(WEASEL_IPC_CLEAR_COMPOSITION, 0, session_id);
  return ret != 0;
}

bool ClientImpl::SelectCandidateOnCurrentPage(size_t index) {
  if (!_Active())
    return false;
  LRESULT ret = _SendMessage(WEASEL_IPC_SELECT_CANDIDATE_ON_CURRENT_PAGE, index,
                             session_id);
  return ret != 0;
}

bool ClientImpl::HighlightCandidateOnCurrentPage(size_t index) {
  if (!_Active())
    return false;
  LRESULT ret = _SendMessage(WEASEL_IPC_HIGHLIGHT_CANDIDATE_ON_CURRENT_PAGE,
                             index, session_id);
  return ret != 0;
}

bool ClientImpl::ChangePage(bool backward) {
  if (!_Active())
    return false;
  LRESULT ret = _SendMessage(WEASEL_IPC_CHANGE_PAGE, backward, session_id);
  return ret != 0;
}

void ClientImpl::UpdateInputPosition(RECT const& rc) {
  if (!_Active())
    return;
  /*
  移位标志 = 1bit == 0
  height:0~127 = 7bit
  top:-2048~2047 = 12bit（有符号）
  left:-2048~2047 = 12bit（有符号）

  高解析度下：
  移位标志 = 1bit == 1
  height:0~254 = 7bit（舍弃低1位）
  top:-4096~4094 = 12bit（有符号，舍弃低1位）
  left:-4096~4094 = 12bit（有符号，舍弃低1位）
  */
  int hi_res =
      static_cast<int>(rc.bottom - rc.top >= 128 || rc.left < -2048 ||
                       rc.left >= 2048 || rc.top < -2048 || rc.top >= 2048);
  int left = max(-2048, min(2047, rc.left >> hi_res));
  int top = max(-2048, min(2047, rc.top >> hi_res));
  int height = max(0, min(127, (rc.bottom - rc.top) >> hi_res));
  DWORD compressed_rect = ((hi_res & 0x01) << 31) | ((height & 0x7f) << 24) |
                          ((top & 0xfff) << 12) | (left & 0xfff);
  _SendMessage(WEASEL_IPC_UPDATE_INPUT_POS, compressed_rect, session_id);
}

void ClientImpl::FocusIn() {
  DWORD client_caps = 0; /* TODO */
  _SendMessage(WEASEL_IPC_FOCUS_IN, client_caps, session_id);
}

void ClientImpl::FocusOut() {
  _SendMessage(WEASEL_IPC_FOCUS_OUT, 0, session_id);
}

void ClientImpl::TrayCommand(UINT menuId) {
  _SendMessage(WEASEL_IPC_TRAY_COMMAND, menuId, session_id);
}

void ClientImpl::StartSession() {
  if (_Active() && Echo(false))
    return;

  _WriteClientInfo();
  UINT ret = _SendMessage(WEASEL_IPC_START_SESSION, 0, 0);
  session_id = ret;
}

void ClientImpl::EndSession() {
  _SendMessage(WEASEL_IPC_END_SESSION, 0, session_id);
  session_id = 0;
}

void ClientImpl::StartMaintenance() {
  _SendMessage(WEASEL_IPC_START_MAINTENANCE, 0, 0);
  session_id = 0;
}

void ClientImpl::EndMaintenance() {
  _SendMessage(WEASEL_IPC_END_MAINTENANCE, 0, 0);
  session_id = 0;
}

bool ClientImpl::Echo(bool light) {
  if (!_Active())
    return false;

  UINT serverEcho = _SendMessage(WEASEL_IPC_ECHO, 0, session_id, light);
  return (serverEcho == session_id);
}

bool ClientImpl::GetResponseData(ResponseHandler const& handler) {
  // DaoHui: after a timeout the buffer holds no answer.
  if (!handler || timed_out) {
    return false;
  }

  return channel.HandleResponseData(handler);
}

bool ClientImpl::_WriteClientInfo() {
  channel << L"action=session\n";
  channel << L"session.client_app=" << app_name.c_str() << L"\n";
  channel << L"session.client_type=" << (is_ime ? L"ime" : L"tsf") << L"\n";
  channel << L".\n";
  return true;
}

LRESULT ClientImpl::_SendMessage(WEASEL_IPC_COMMAND Msg,
                                 DWORD wParam,
                                 DWORD lParam,
                                 bool light) {
  DWORD timeout = kKey;
  switch (Msg) {
    case WEASEL_IPC_FOCUS_IN:
    case WEASEL_IPC_FOCUS_OUT:
    case WEASEL_IPC_UPDATE_INPUT_POS:
    case WEASEL_IPC_END_SESSION:
    case WEASEL_IPC_TRAY_COMMAND:
      light = true;
      break;
    case WEASEL_IPC_START_SESSION:
    case WEASEL_IPC_SHUTDOWN_SERVER:
      timeout = kSession;
      break;
    case WEASEL_IPC_START_MAINTENANCE:  // (the deployer, not an application)
    case WEASEL_IPC_END_MAINTENANCE:
      timeout = INFINITE;
      break;
    default:
      break;
  }
  if (light)
    timeout = kLight;
  timed_out = false;
  ULONGLONG now = ::GetTickCount64();
  if (timeout != INFINITE &&
      (now < paused_until || (light && now < light_paused_until))) {
    timed_out = true;
    return 0;
  }
  try {
    channel.SetTimeout(timeout);
    PipeMessage req{Msg, wParam, lParam};
    return channel.Transact(req);
  } catch (DWORD ex) {
    const ULONGLONG waited = ::GetTickCount64() - now;
    if (ex == PipeChannelBase::kTimeout) {
      timed_out = true;
      if (light)
        light_paused_until = ::GetTickCount64() + kLightPause;
      else
        paused_until = ::GetTickCount64() + kPause;
      LogEvent(L"request %d (0x%lx) got no answer in %llu ms; %ls requests "
               L"skipped for %llu ms",
               (int)Msg, wParam, waited, light ? L"light" : L"all",
               light ? kLightPause : kPause);
    } else {
      LogEvent(L"request %d (0x%lx) failed after %llu ms: error %lu", (int)Msg,
               wParam, waited, ex);
    }
    return 0;
  }
}

Client::Client() : m_pImpl(new ClientImpl()) {}

Client::~Client() {
  if (m_pImpl)
    delete m_pImpl;
}

bool Client::Connect(ServerLauncher launcher) {
  return m_pImpl->Connect(launcher);
}

void Client::Disconnect() {
  m_pImpl->Disconnect();
}

void Client::ShutdownServer() {
  m_pImpl->ShutdownServer();
}

bool Client::ProcessKeyEvent(KeyEvent const& keyEvent) {
  return m_pImpl->ProcessKeyEvent(keyEvent);
}

bool Client::CommitComposition() {
  return m_pImpl->CommitComposition();
}

bool Client::ClearComposition() {
  return m_pImpl->ClearComposition();
}

bool Client::SelectCandidateOnCurrentPage(size_t index) {
  return m_pImpl->SelectCandidateOnCurrentPage(index);
}

bool Client::HighlightCandidateOnCurrentPage(size_t index) {
  return m_pImpl->HighlightCandidateOnCurrentPage(index);
}

bool Client::ChangePage(bool backward) {
  return m_pImpl->ChangePage(backward);
}

void Client::UpdateInputPosition(RECT const& rc) {
  m_pImpl->UpdateInputPosition(rc);
}

void Client::FocusIn() {
  m_pImpl->FocusIn();
}

void Client::FocusOut() {
  m_pImpl->FocusOut();
}

void Client::StartSession() {
  m_pImpl->StartSession();
}

void Client::EndSession() {
  m_pImpl->EndSession();
}

void Client::StartMaintenance() {
  m_pImpl->StartMaintenance();
}

void Client::EndMaintenance() {
  m_pImpl->EndMaintenance();
}

void Client::TrayCommand(UINT menuId) {
  m_pImpl->TrayCommand(menuId);
}

bool Client::Echo(bool light) {
  return m_pImpl->Echo(light);
}

bool Client::TimedOut() const {
  return m_pImpl->TimedOut();
}

bool Client::GetResponseData(ResponseHandler handler) {
  return m_pImpl->GetResponseData(handler);
}
