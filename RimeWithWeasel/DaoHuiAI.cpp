// DaoHui: AI correction of the composition, Ctrl+R (lua/asus_fix.lua).
//
// The model runs on this computer only: llama.cpp's llama-server (Vulkan
// build, falls back to the CPU) with Qwen3.5-4B, listening on 127.0.0.1.
// Both are downloaded by the settings app (AI 修正) into
// %USERPROFILE%\.daohui\ai: llama\llama-server.exe and the .gguf. The server
// is started at the first request, inside a job object that ends it with
// WeaselServer, and stopped after kIdleSeconds without a request.
//
// The answer cannot be pushed to the application: the TSF client only takes
// a new composition in reply to a key it sent. So the answer is kept for the
// session and F23 is pressed (SendInput); the client sends it to the server,
// ProcessKeyEvent hands the answer to Lua (asus_fix_result) and Lua's
// response shows the correction. The English writing helper's F24 works the
// same way.

#include "stdafx.h"
#include "DaoHuiAI.h"

#include <ShlObj.h>
#include <winhttp.h>

#include <chrono>
#include <filesystem>
#include <map>
#include <mutex>
#include <thread>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")

namespace fs = std::filesystem;

namespace {

constexpr INTERNET_PORT kPort = 8739;
constexpr int kIdleSeconds = 600;
constexpr int kStartSeconds = 180;  // the first load reads 2.7 GB
// The very first answer after installing waits for the Vulkan driver to
// compile llama.cpp's shaders (45 s on an RTX 3070 laptop; cached after).
constexpr DWORD kAnswerTimeoutMs = 150000;

std::mutex g_answers_mutex;  // short: the answers waiting for their F23
std::map<RimeSessionId, std::string> g_answers;

std::mutex g_server_mutex;  // long: held while starting and asking
PROCESS_INFORMATION g_server = {};
HANDLE g_job = nullptr;
std::chrono::steady_clock::time_point g_last_used;
bool g_idle_watch = false;

// %USERPROFILE%\.daohui\ai, not under AppData: Windows redirects AppData
// for packaged (MSIX) apps and the processes they start, and the TSF client
// of such an app (Claude, Store apps) may start this server (start_service.bat)
// - it would then look in the app's private copy of AppData and find nothing.
// The profile folder itself is not redirected (Ollama keeps ~\.ollama too).
fs::path AiDir() {
  PWSTR path = nullptr;
  fs::path dir;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Profile, 0, nullptr, &path)))
    dir = fs::path(path) / L".daohui" / L"ai";
  CoTaskMemFree(path);
  return dir;
}

// One request to the model server: HTTP status (0 = no connection).
int Http(const wchar_t* verb, const wchar_t* path, const std::string& body,
         std::string* out, DWORD timeout_ms) {
  int status = 0;
  HINTERNET session = WinHttpOpen(L"DaoHui", WINHTTP_ACCESS_TYPE_NO_PROXY,
                                  WINHTTP_NO_PROXY_NAME,
                                  WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session)
    return 0;
  WinHttpSetTimeouts(session, timeout_ms, timeout_ms, timeout_ms, timeout_ms);
  HINTERNET connect = WinHttpConnect(session, L"127.0.0.1", kPort, 0);
  HINTERNET request =
      connect ? WinHttpOpenRequest(connect, verb, path, nullptr,
                                   WINHTTP_NO_REFERER,
                                   WINHTTP_DEFAULT_ACCEPT_TYPES, 0)
              : nullptr;
  if (request) {
    BOOL sent =
        body.empty()
            ? WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                 WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
            : WinHttpSendRequest(
                  request, L"Content-Type: application/json\r\n", (DWORD)-1L,
                  (LPVOID)body.data(), (DWORD)body.size(), (DWORD)body.size(),
                  0);
    if (sent && WinHttpReceiveResponse(request, nullptr)) {
      DWORD code = 0, size = sizeof code;
      WinHttpQueryHeaders(request,
                          WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                          WINHTTP_HEADER_NAME_BY_INDEX, &code, &size,
                          WINHTTP_NO_HEADER_INDEX);
      status = (int)code;
      DWORD available = 0;
      while (out && WinHttpQueryDataAvailable(request, &available) &&
             available > 0) {
        std::string chunk(available, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(request, chunk.data(), available, &read) ||
            read == 0)
          break;
        out->append(chunk.data(), read);
      }
    }
    WinHttpCloseHandle(request);
  }
  if (connect)
    WinHttpCloseHandle(connect);
  WinHttpCloseHandle(session);
  return status;
}

void AppendUtf8(std::string* out, unsigned cp) {
  if (cp < 0x80) {
    out->push_back((char)cp);
  } else if (cp < 0x800) {
    out->push_back((char)(0xC0 | (cp >> 6)));
    out->push_back((char)(0x80 | (cp & 0x3F)));
  } else if (cp < 0x10000) {
    out->push_back((char)(0xE0 | (cp >> 12)));
    out->push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
    out->push_back((char)(0x80 | (cp & 0x3F)));
  } else {
    out->push_back((char)(0xF0 | (cp >> 18)));
    out->push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
    out->push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
    out->push_back((char)(0x80 | (cp & 0x3F)));
  }
}

// choices[0].message.content of an OpenAI-style answer (the first "content"
// after "message"), JSON string escapes decoded.
bool ContentOf(const std::string& json, std::string* out) {
  size_t i = json.find("\"message\"");
  if (i == std::string::npos)
    return false;
  i = json.find("\"content\"", i);
  if (i == std::string::npos)
    return false;
  i = json.find(':', i);
  if (i == std::string::npos)
    return false;
  ++i;
  while (i < json.size() && isspace((unsigned char)json[i]))
    ++i;
  if (i >= json.size() || json[i] != '"')
    return false;  // null
  out->clear();
  auto hex4 = [&](size_t at, unsigned* v) {
    if (at + 4 > json.size())
      return false;
    *v = 0;
    for (size_t k = at; k < at + 4; ++k) {
      char c = json[k];
      *v <<= 4;
      if (c >= '0' && c <= '9')
        *v |= c - '0';
      else if (c >= 'a' && c <= 'f')
        *v |= c - 'a' + 10;
      else if (c >= 'A' && c <= 'F')
        *v |= c - 'A' + 10;
      else
        return false;
    }
    return true;
  };
  for (++i; i < json.size(); ++i) {
    char c = json[i];
    if (c == '"')
      return true;
    if (c != '\\') {
      out->push_back(c);
      continue;
    }
    if (++i >= json.size())
      return false;
    switch (json[i]) {
      case 'n': out->push_back('\n'); break;
      case 'r': out->push_back('\r'); break;
      case 't': out->push_back('\t'); break;
      case 'b': out->push_back('\b'); break;
      case 'f': out->push_back('\f'); break;
      case 'u': {
        unsigned cp = 0;
        if (!hex4(i + 1, &cp))
          return false;
        i += 4;
        if (cp >= 0xD800 && cp < 0xDC00 && i + 6 < json.size() &&
            json[i + 1] == '\\' && json[i + 2] == 'u') {
          unsigned low = 0;
          if (hex4(i + 3, &low) && low >= 0xDC00 && low < 0xE000) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
            i += 6;
          }
        }
        AppendUtf8(out, cp);
        break;
      }
      default: out->push_back(json[i]); break;  // \" \\ \/
    }
  }
  return false;
}

bool ServerRunning() {  // g_server_mutex held
  return g_server.hProcess &&
         WaitForSingleObject(g_server.hProcess, 0) == WAIT_TIMEOUT;
}

void StopServerLocked() {
  if (g_server.hProcess) {
    TerminateProcess(g_server.hProcess, 0);
    CloseHandle(g_server.hProcess);
    CloseHandle(g_server.hThread);
  }
  g_server = {};
}

// Stops the server once it has not been asked for kIdleSeconds.
void WatchIdle() {
  for (;;) {
    std::this_thread::sleep_for(std::chrono::seconds(30));
    std::lock_guard<std::mutex> lock(g_server_mutex);
    if (ServerRunning() && std::chrono::steady_clock::now() - g_last_used >
                               std::chrono::seconds(kIdleSeconds))
      StopServerLocked();
  }
}

// Starts llama-server if it is not running; "" or a message for the bubble.
std::string StartServerLocked() {
  if (ServerRunning())
    return "";
  StopServerLocked();
  fs::path dir = AiDir();
  fs::path exe = dir / L"llama" / L"llama-server.exe";
  fs::path model = dir / L"Qwen3.5-4B-Q4_K_M.gguf";
  std::error_code ec;
  for (const fs::path& need : {exe, model}) {
    if (dir.empty() || !fs::exists(need, ec)) {
      // (The path says where it was looked for: a profile moved elsewhere,
      // a server started as another user.)
      std::string where = dir.empty() ? std::string("USERPROFILE?")
                                      : need.u8string();
      if (ec)
        where += " (" + std::to_string(ec.value()) + ")";
      return u8"需要先下載 AI 修正模型：島輝設定 → 模型 → AI 修正（找不到 " +
             where + u8"）";
    }
  }
  if (!g_job) {
    g_job = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (g_job)
      SetInformationJobObject(g_job, JobObjectExtendedLimitInformation, &info,
                              sizeof info);
  }
  std::wstring command = L"\"" + exe.wstring() + L"\" -m \"" +
                         model.wstring() + L"\" --host 127.0.0.1 --port " +
                         std::to_wstring(kPort) +
                         L" -ngl all -c 4096 -np 1 --reasoning off --no-webui"
                         L" --log-file \"" +
                         (dir / L"llama-server.log").wstring() + L"\"";
  STARTUPINFOW si = {sizeof si};
  PROCESS_INFORMATION pi = {};
  if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE,
                      CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr,
                      exe.parent_path().c_str(), &si, &pi))
    return u8"AI 修正模型無法啟動";
  if (g_job)
    AssignProcessToJobObject(g_job, pi.hProcess);
  ResumeThread(pi.hThread);
  g_server = pi;
  if (!g_idle_watch) {
    g_idle_watch = true;
    std::thread(WatchIdle).detach();
  }
  for (int i = 0; i < kStartSeconds * 2 && ServerRunning(); ++i) {
    if (Http(L"GET", L"/health", "", nullptr, 2000) == 200)
      return "";
    Sleep(500);
  }
  StopServerLocked();
  return u8"AI 修正模型無法啟動";
}

void PressF23() {
  INPUT in[2] = {};
  in[0].type = in[1].type = INPUT_KEYBOARD;
  in[0].ki.wVk = in[1].ki.wVk = VK_F23;
  in[1].ki.dwFlags = KEYEVENTF_KEYUP;
  SendInput(2, in, sizeof(INPUT));
}

}  // namespace

namespace daohui {

void AskFix(RimeSessionId session, const std::string& request) {
  size_t tab = request.find('\t');
  if (tab == std::string::npos)
    return;
  std::string id = request.substr(0, tab);
  std::string body = request.substr(tab + 1);
  std::thread([session, id, body] {
    std::string answer;
    {
      std::lock_guard<std::mutex> lock(g_server_mutex);
      std::string error = StartServerLocked();
      if (!error.empty()) {
        answer = "!" + error;
      } else {
        std::string response;
        if (Http(L"POST", L"/v1/chat/completions", body, &response,
                 kAnswerTimeoutMs) != 200 ||
            !ContentOf(response, &answer))
          answer = u8"!AI 沒有回應";
        g_last_used = std::chrono::steady_clock::now();
      }
    }
    {
      std::lock_guard<std::mutex> lock(g_answers_mutex);
      g_answers[session] = id + "\t" + answer;
    }
    PressF23();
  }).detach();
}

bool TakeFixAnswer(RimeSessionId session, std::string* answer) {
  std::lock_guard<std::mutex> lock(g_answers_mutex);
  auto it = g_answers.find(session);
  if (it == g_answers.end())
    return false;
  *answer = std::move(it->second);
  g_answers.erase(it);
  return true;
}

void StopFixServer() {
  // (While a request is being answered the lock is held for up to a minute:
  // then the job object ends the server with WeaselServer.)
  if (g_server_mutex.try_lock()) {
    StopServerLocked();
    g_server_mutex.unlock();
  }
}

}  // namespace daohui
