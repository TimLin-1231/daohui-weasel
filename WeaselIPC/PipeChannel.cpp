#include "stdafx.h"

#include <algorithm>
#include <PipeChannel.h>

using namespace weasel;
using namespace std;
using namespace boost;

#define _ThrowLastError throw ::GetLastError()
#define _ThrowCode(__c) throw __c
#define _ThrowIfNot(__c)                 \
  {                                      \
    DWORD err;                           \
    if ((err = ::GetLastError()) != __c) \
      throw err;                         \
  }

PipeChannelBase::PipeChannelBase(std::wstring&& pn_cmd,
                                 size_t bs = 4 * 1024,
                                 SECURITY_ATTRIBUTES* s = NULL)
    : pname(pn_cmd),
      write_stream(nullptr),
      buff_size(bs),
      buffer(std::make_unique<char[]>(bs)),
      hpipe(INVALID_HANDLE_VALUE),
      has_body(false),
      overlapped(false),
      timeout_ms(INFINITE),
      sa(s) {};

PipeChannelBase::PipeChannelBase(PipeChannelBase&& r)
    : write_stream(std::move(r.write_stream)),
      pname(std::move(r.pname)),
      buff_size(r.buff_size),
      buffer(std::move(r.buffer)),
      hpipe(r.hpipe),
      has_body(r.has_body),
      overlapped(r.overlapped),
      timeout_ms(r.timeout_ms),
      sa(r.sa) {};

PipeChannelBase::~PipeChannelBase() {
  _FinalizePipe(hpipe);
}

bool PipeChannelBase::_Ensure() {
  try {
    if (_Invalid(hpipe)) {
      hpipe = _Connect(pname.c_str());
      return !_Invalid(hpipe);
    }
  } catch (...) {
    return false;
  }

  return true;
}

HANDLE PipeChannelBase::_Connect(const wchar_t* name) {
  HANDLE pipe = INVALID_HANDLE_VALUE;
  // DaoHui: a client waits for a free pipe instance at most timeout_ms (stock
  // waited for ever).
  const bool bounded = overlapped && timeout_ms != INFINITE;
  const ULONGLONG deadline = ::GetTickCount64() + (bounded ? timeout_ms : 0);
  while (_Invalid(pipe = _TryConnect())) {
    DWORD wait = 500;
    if (bounded) {
      ULONGLONG now = ::GetTickCount64();
      if (now >= deadline)
        _ThrowCode(kTimeout);
      wait = static_cast<DWORD>((std::min)(deadline - now, 500ULL));
    }
    ::WaitNamedPipe(name, wait);
  }
  DWORD mode = PIPE_READMODE_MESSAGE;
  if (!SetNamedPipeHandleState(pipe, &mode, NULL, NULL)) {
    _ThrowLastError;
  }
  return pipe;
}

void PipeChannelBase::_Reconnect() {
  _FinalizePipe(hpipe);
  _Ensure();
}

HANDLE PipeChannelBase::_TryConnect() {
  auto pipe = ::CreateFile(pname.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL,
                           OPEN_EXISTING, overlapped ? FILE_FLAG_OVERLAPPED : 0,
                           NULL);
  if (!_Invalid(pipe)) {
    // connected to the pipe
    return pipe;
  }
  // being busy is not really an error since we just need to wait.
  _ThrowIfNot(ERROR_PIPE_BUSY);
  // All pipe instances are busy
  return INVALID_HANDLE_VALUE;
}

size_t PipeChannelBase::_WritePipe(HANDLE pipe, size_t s, char* b) {
  if (overlapped) {
    // (No FlushFileBuffers: it waits, unbounded, until the server has read
    // the message. Its answer, read next, says as much.)
    _Io(pipe, true, b, static_cast<DWORD>(s));
    return s;
  }
  DWORD lwritten;
  if (!::WriteFile(pipe, b, s, &lwritten, NULL) || lwritten <= 0) {
    _ThrowLastError;
  }
  ::FlushFileBuffers(pipe);
  return lwritten;
}

void PipeChannelBase::_FinalizePipe(HANDLE& p) {
  if (!_Invalid(p)) {
    DisconnectNamedPipe(p);
    CloseHandle(p);
  }
  p = INVALID_HANDLE_VALUE;
}

void PipeChannelBase::_Receive(HANDLE pipe, LPVOID msg, size_t rec_len) {
  if (overlapped) {
    if (_Io(pipe, false, msg, static_cast<DWORD>(rec_len))) {
      memset(buffer.get(), 0, buff_size);
      _Io(pipe, false, buffer.get(), static_cast<DWORD>(buff_size));
    }
    has_body = false;
    return;
  }
  DWORD lread;
  BOOL success = ::ReadFile(pipe, msg, rec_len, &lread, NULL);
  if (!success) {
    _ThrowIfNot(ERROR_MORE_DATA);

    memset(buffer.get(), 0, buff_size);
    success = ::ReadFile(pipe, buffer.get(), buff_size, &lread, NULL);
    if (!success) {
      _ThrowLastError;
    }
  }
  has_body = false;
}

bool PipeChannelBase::_Io(HANDLE pipe, bool write, LPVOID data, DWORD size) {
  OVERLAPPED ov = {};
  ov.hEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
  if (!ov.hEvent)
    _ThrowLastError;
  BOOL ok = write ? ::WriteFile(pipe, data, size, NULL, &ov)
                  : ::ReadFile(pipe, data, size, NULL, &ov);
  DWORD err = ok ? ERROR_SUCCESS : ::GetLastError();
  if (err == ERROR_IO_PENDING &&
      ::WaitForSingleObject(ov.hEvent, timeout_ms) != WAIT_OBJECT_0) {
    // Given up. Wait for the cancel to finish (data and ov are ours again),
    // then drop the pipe and whatever the buffer held, so that nothing of
    // this exchange is read as the next one's.
    DWORD n = 0;
    ::CancelIoEx(pipe, &ov);
    ::GetOverlappedResult(pipe, &ov, &n, TRUE);
    ::CloseHandle(ov.hEvent);
    memset(buffer.get(), 0, buff_size);
    _FinalizePipe(hpipe);
    _ThrowCode(kTimeout);
  }
  if (err == ERROR_SUCCESS || err == ERROR_IO_PENDING ||
      err == ERROR_MORE_DATA) {
    DWORD n = 0;
    err = ::GetOverlappedResult(pipe, &ov, &n, FALSE) ? ERROR_SUCCESS
                                                      : ::GetLastError();
  }
  ::CloseHandle(ov.hEvent);
  if (err == ERROR_MORE_DATA)
    return true;
  if (err != ERROR_SUCCESS)
    _ThrowCode(err);
  return false;
}

HANDLE PipeChannelBase::_ConnectServerPipe(std::wstring& pn) {
  HANDLE pipe =
      CreateNamedPipe(pn.c_str(), PIPE_ACCESS_DUPLEX,
                      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                      PIPE_UNLIMITED_INSTANCES, buff_size, buff_size, 0, sa);
  if (pipe == INVALID_HANDLE_VALUE || !::ConnectNamedPipe(pipe, NULL)) {
    _ThrowLastError;
  }
  return pipe;
}
