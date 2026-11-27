#include "stdafx.h"

#include <WeaselIPCData.h>
#include <thread>
#include <shellapi.h>
#include "WeaselTSF.h"
#include "CandidateList.h"
#include "LanguageBar.h"
#include "Compartment.h"
#include "ResponseParser.h"

static void error_message(const WCHAR* msg) {
  static DWORD next_tick = 0;
  DWORD now = GetTickCount();
  if (now > next_tick) {
    next_tick = now + 10000;  // (ms)
    MessageBox(NULL, msg, get_weasel_ime_name().c_str(), MB_ICONERROR | MB_OK);
  }
}

WeaselTSF::WeaselTSF() {
  _cRef = 1;

  _dwThreadMgrEventSinkCookie = TF_INVALID_COOKIE;

  _dwTextEditSinkCookie = TF_INVALID_COOKIE;
  _dwTextLayoutSinkCookie = TF_INVALID_COOKIE;
  _dwThreadFocusSinkCookie = TF_INVALID_COOKIE;
  _fTestKeyDownPending = FALSE;
  _fTestKeyUpPending = FALSE;

  _fCUASWorkaroundTested = _fCUASWorkaroundEnabled = FALSE;

  _cand = new CCandidateList(this);

  DllAddRef();
}

WeaselTSF::~WeaselTSF() {
  DllRelease();
}

STDAPI WeaselTSF::QueryInterface(REFIID riid, void** ppvObject) {
  if (ppvObject == NULL)
    return E_INVALIDARG;

  *ppvObject = NULL;

  if (IsEqualIID(riid, IID_IUnknown) ||
      IsEqualIID(riid, IID_ITfTextInputProcessor))
    *ppvObject = (ITfTextInputProcessor*)this;
  else if (IsEqualIID(riid, IID_ITfTextInputProcessorEx))
    *ppvObject = (ITfTextInputProcessorEx*)this;
  else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink))
    *ppvObject = (ITfThreadMgrEventSink*)this;
  else if (IsEqualIID(riid, IID_ITfTextEditSink))
    *ppvObject = (ITfTextEditSink*)this;
  else if (IsEqualIID(riid, IID_ITfTextLayoutSink))
    *ppvObject = (ITfTextLayoutSink*)this;
  else if (IsEqualIID(riid, IID_ITfKeyEventSink))
    *ppvObject = (ITfKeyEventSink*)this;
  else if (IsEqualIID(riid, IID_ITfCompositionSink))
    *ppvObject = (ITfCompositionSink*)this;
  else if (IsEqualIID(riid, IID_ITfEditSession))
    *ppvObject = (ITfEditSession*)this;
  else if (IsEqualIID(riid, IID_ITfThreadFocusSink))
    *ppvObject = (ITfThreadFocusSink*)this;
  else if (IsEqualIID(riid, IID_ITfDisplayAttributeProvider))
    *ppvObject = (ITfDisplayAttributeProvider*)this;

  if (*ppvObject) {
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

STDAPI_(ULONG) WeaselTSF::AddRef() {
  return ++_cRef;
}

STDAPI_(ULONG) WeaselTSF::Release() {
  LONG cr = --_cRef;

  assert(_cRef >= 0);

  if (_cRef == 0)
    delete this;

  return cr;
}

STDAPI WeaselTSF::Activate(ITfThreadMgr* pThreadMgr, TfClientId tfClientId) {
  return ActivateEx(pThreadMgr, tfClientId, 0U);
}

STDAPI WeaselTSF::Deactivate() {
  // DaoHui: switching to another input method ends this one in that thread
  // (as does the thread ending). The keys held down tell a shortcut such as
  // Ctrl+Space or Win+Space from a click on the input menu.
  auto held = [](int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; };
  weasel::LogEvent(L"text service deactivated (keys held:%ls%ls%ls%ls%ls)",
                   held(VK_CONTROL) ? L" Ctrl" : L"",
                   held(VK_SHIFT) ? L" Shift" : L"",
                   held(VK_MENU) ? L" Alt" : L"",
                   held(VK_LWIN) || held(VK_RWIN) ? L" Win" : L"",
                   held(VK_SPACE) ? L" Space" : L"");
  _CancelPasteKey(L"deactivated");
  m_client.EndSession();

  _InitTextEditSink(com_ptr<ITfDocumentMgr>());

  _UninitThreadMgrEventSink();

  _UninitKeyEventSink();
  _UninitPreservedKey();

  _UninitLanguageBar();

  _UninitCompartment();

  _UninitThreadMgrEventSink();

  // DaoHui: stock never took this sink back, so a deactivated instance kept
  // getting the thread's focus changes (and sending Echo for them).
  _UninitThreadFocusSink();

  _pThreadMgr = NULL;

  _tfClientId = TF_CLIENTID_NULL;

  _cand->DestroyAll();

  return S_OK;
}

STDAPI WeaselTSF::ActivateEx(ITfThreadMgr* pThreadMgr,
                             TfClientId tfClientId,
                             DWORD dwFlags) {
  com_ptr<ITfDocumentMgr> pDocMgrFocus;
  // DaoHui: the step that failed, for the log (without a key sink every key
  // goes to the application as typed, and nothing else would show why)
  const wchar_t* failed = L"";
  _activateFlags = dwFlags;

  _pThreadMgr = pThreadMgr;
  _tfClientId = tfClientId;

  if (!_InitThreadMgrEventSink()) {
    failed = L"thread manager event sink";
    goto ExitError;
  }

  if ((_pThreadMgr->GetFocus(&pDocMgrFocus) == S_OK) &&
      (pDocMgrFocus != NULL)) {
    _InitTextEditSink(pDocMgrFocus);
  }

  if (!_InitKeyEventSink()) {
    failed = L"key event sink";
    goto ExitError;
  }

  // if (!_InitDisplayAttributeGuidAtom())
  //	goto ExitError;
  //	some app might init failed because it not provide DisplayAttributeInfo,
  // like some opengl stuff
  _InitDisplayAttributeGuidAtom();

  if (!_InitPreservedKey()) {
    failed = L"preserved key";
    goto ExitError;
  }

  if (!_InitLanguageBar()) {
    failed = L"language bar";
    goto ExitError;
  }

  if (!_IsKeyboardOpen())
    _SetKeyboardOpen(TRUE);

  if (!_InitCompartment()) {
    failed = L"compartment sinks";
    goto ExitError;
  }
  if (!_InitThreadFocusSink()) {
    failed = L"thread focus sink";
    goto ExitError;
  }

  _EnsureServerConnected();

  return S_OK;

ExitError:
  weasel::LogEvent(L"text service could not start: %ls failed", failed);
  Deactivate();
  return E_FAIL;
}

STDMETHODIMP WeaselTSF::OnSetThreadFocus() {
  std::wstring _ToggleImeOnOpenClose{};
  RegGetStringValue(HKEY_CURRENT_USER, DAOHUI_REG_KEY_W,
                    L"ToggleImeOnOpenClose", _ToggleImeOnOpenClose);
  _isToOpenClose = (_ToggleImeOnOpenClose == L"yes");
  // DaoHui: light requests (200 ms at most each): this runs in the
  // application's UI thread at every focus change, Explorer's taskbar too.
  if (m_client.Echo(true)) {
    m_client.ProcessKeyEvent(0);
    weasel::ResponseParser parser(NULL, NULL, &_status, NULL, &_cand->style());
    bool ok = m_client.GetResponseData(std::ref(parser));
    if (ok)
      _UpdateLanguageBar(_status);
  }
  return S_OK;
}
STDMETHODIMP WeaselTSF::OnKillThreadFocus() {
  TsfDebugLog(L"OnKillThreadFocus");
  _CancelPasteKey(L"thread focus left");
  _KeepComposition(_pEditSessionContext);
  return S_OK;
}
BOOL WeaselTSF::_InitThreadFocusSink() {
  com_ptr<ITfSource> pSource;
  if (FAILED(_pThreadMgr->QueryInterface(&pSource)))
    return FALSE;
  if (FAILED(pSource->AdviseSink(IID_ITfThreadFocusSink,
                                 (ITfThreadFocusSink*)this,
                                 &_dwThreadFocusSinkCookie)))
    return FALSE;
  return TRUE;
}
void WeaselTSF::_UninitThreadFocusSink() {
  if (_dwThreadFocusSinkCookie == TF_INVALID_COOKIE || _pThreadMgr == NULL)
    return;
  com_ptr<ITfSource> pSource;
  if (FAILED(_pThreadMgr->QueryInterface(&pSource)))
    return;
  pSource->UnadviseSink(_dwThreadFocusSinkCookie);
  _dwThreadFocusSinkCookie = TF_INVALID_COOKIE;
}

STDMETHODIMP WeaselTSF::OnActivated(REFCLSID clsid,
                                    REFGUID guidProfile,
                                    BOOL isActivated) {
  if (!IsEqualCLSID(clsid, c_clsidTextService)) {
    return S_OK;
  }

  if (isActivated) {
    _ShowLanguageBar(TRUE);
    _UpdateLanguageBar(_status);
  } else {
    _DeleteCandidateList();
    _ShowLanguageBar(FALSE);
  }
  return S_OK;
}

void WeaselTSF::_Reconnect() {
  m_client.Disconnect();
  m_client.Connect(NULL);
  m_client.StartSession();
  weasel::ResponseParser parser(NULL, NULL, &_status, NULL, &_cand->style());
  bool ok = m_client.GetResponseData(std::ref(parser));
  if (ok) {
    _UpdateLanguageBar(_status);
  }
}

static unsigned int retry = 0;

bool WeaselTSF::_EnsureServerConnected() {
  if (!m_client.Echo()) {
    // DaoHui: a server that is there but answers late is neither reconnected
    // to nor started again while requests are paused after a timeout (Echo
    // counts as late then); the key goes to the application. Once the pause
    // is over, Echo fails as not sent, and the pipe the timeout closed is
    // opened again here (the session kept when the server still has it).
    if (m_client.TimedOut())
      return false;
    weasel::LogEvent(L"echo failed: reconnecting, try %u", retry + 1);
    _Reconnect();
    // DaoHui: a reconnect the server answered late is no sign that it has
    // gone: not counted towards starting it again. The first key after the
    // pause tries again.
    if (m_client.TimedOut())
      return false;
    retry++;
    if (retry >= 6) {
      HANDLE hMutex = CreateMutex(NULL, TRUE, DAOHUI_DEPLOYER_EXCLUSIVE_MUTEX_W);
      if (!m_client.Echo() && GetLastError() != ERROR_ALREADY_EXISTS) {
        std::wstring dir = _GetRootDir();
        std::thread th([dir, this]() {
          ShellExecuteW(NULL, L"open", (dir + L"\\start_service.bat").c_str(),
                        NULL, dir.c_str(), SW_HIDE);
          // wait 500ms, then reconnect
          std::this_thread::sleep_for(std::chrono::milliseconds(500));
          _Reconnect();
        });
        th.detach();
      }
      if (hMutex) {
        CloseHandle(hMutex);
      }
      retry = 0;
    }
    const bool ok = m_client.Echo() != 0;
    // DaoHui: retry counts failures in a row. Stock counted every reconnect
    // of the process's life, so after five that had worked, the next failed
    // Echo already ran start_service.bat.
    if (ok)
      retry = 0;
    return ok;
  } else {
    return true;
  }
}
