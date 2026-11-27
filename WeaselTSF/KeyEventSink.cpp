#include "stdafx.h"
#include "WeaselIPC.h"
#include "WeaselTSF.h"
#include <KeyEvent.h>
#include "CandidateList.h"

static weasel::KeyEvent prevKeyEvent;
static BOOL prevfEaten = FALSE;
static int keyCountToSimulate = 0;
// DaoHui: when the Caps Lock presses counted by keyCountToSimulate were sent.
static DWORD capsSentTick = 0;

/* Caps Lock (DaoHui).
 * When Rime has used Caps Lock (to switch between Chinese and English) and
 * the light came on, the Caps Lock branch of _ProcessKeyEvent presses Caps
 * Lock again with SendInput to turn it off, and keyCountToSimulate counts the
 * injected events, which are not Rime's to see. Stock skipped every key while
 * that count was not 0: when an injected event never came back to this text
 * service (the focus moved to another thread, a keyboard hook took it), every
 * following key of the process went to the application as it was typed,
 * unlogged, with the indicator still on Chinese, until Caps Lock was pressed
 * again. Now only Caps Lock events are skipped: an injected press that never
 * comes back costs one later Caps Lock press at most, which then only
 * toggles the light and leaves Rime's mode as it is, as in stock (if the
 * light comes on, Rime leaves the keys to the application, logged, until
 * Caps Lock is pressed again). The count is not given up after a time: an
 * injected press handled late, behind a busy application, would then reach
 * Rime and switch the mode back. A Caps Lock skipped more than kCapsLateMs
 * after the injection is logged.
 */
namespace {
constexpr DWORD kCapsLateMs = 1500;

/* Keys that go to the application although DaoHui is on, for a reason that
 * leaves no other trace (DaoHui). Printable key presses only (no Ctrl, Alt or
 * Win held): one line per reason at first, then at most one per
 * kQuietPassLogMs and thread, with the number of such keys in between. */
enum QuietPass { kPassClosed, kPassDisabled, kPassNotTaken, kPassUnknown, kPassReasons };
constexpr DWORD kQuietPassLogMs = 60000;
thread_local DWORD tQuietPassLogged[kPassReasons] = {};
thread_local unsigned tQuietPassSince[kPassReasons] = {};

bool IsPrintableKeyPress(WPARAM vk, LPARAM lParam) {
  if ((static_cast<DWORD>(lParam) >> 31) & 1)
    return false;  // a release
  if ((GetKeyState(VK_CONTROL) & 0x8000) || (GetKeyState(VK_MENU) & 0x8000) ||
      (GetKeyState(VK_LWIN) & 0x8000) || (GetKeyState(VK_RWIN) & 0x8000))
    return false;
  // (Space and the numeric keypad go to the application in normal use.)
  return (vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z') ||
         (vk >= VK_OEM_1 && vk <= VK_OEM_3) ||
         (vk >= VK_OEM_4 && vk <= VK_OEM_7);
}

void LogQuietPass(QuietPass reason, WPARAM vk, LPARAM lParam,
                  const wchar_t* why) {
  if (!IsPrintableKeyPress(vk, lParam))
    return;
  const DWORD now = GetTickCount();
  if (tQuietPassLogged[reason] &&
      now - tQuietPassLogged[reason] < kQuietPassLogMs) {
    ++tQuietPassSince[reason];
    return;
  }
  weasel::LogEvent(
      L"key 0x%02x went to the application: %ls (%u more like it since the "
      L"last line)",
      (unsigned)vk, why, tQuietPassSince[reason]);
  tQuietPassLogged[reason] = now | 1;
  tQuietPassSince[reason] = 0;
}
}  // namespace

/* Paste while composing.
 * Ctrl+V (Ctrl+Shift+V, Shift+Insert) while composing: Rime commits what is
 * shown and leaves the key to the application (asus_keys.lua). The committed
 * text reaches the document through edit sessions, which the application may
 * grant only after it has handled the key: it then pasted into the open
 * composition, and the commit wrote over what it had pasted. So the key is
 * held back and sent again with SendInput once the commit is in and the
 * composition has ended (_FinalizeComposition), or as soon as the response
 * is applied when nothing was committed. The key sent again goes through like
 * any other (and is not held back again: no loop when Rime keeps composing).
 */
namespace {
// a held key older than this is dropped rather than pasted late
constexpr DWORD kPasteHoldMs = 1000;
// sent again even if the end of the composition was never reported
constexpr UINT kPasteFallbackMs = 400;
thread_local WeaselTSF* tPasteService = nullptr;
thread_local UINT_PTR tPasteTimer = 0;

void StopPasteTimer() {
  if (tPasteTimer)
    KillTimer(NULL, tPasteTimer);
  tPasteTimer = 0;
  tPasteService = nullptr;
}

VOID CALLBACK PasteTimerProc(HWND, UINT, UINT_PTR id, DWORD) {
  KillTimer(NULL, id);
  if (id != tPasteTimer)
    return;
  WeaselTSF* service = tPasteService;
  tPasteTimer = 0;
  tPasteService = nullptr;
  if (service)
    service->_ReplayPasteKey(L"timer");
}
}  // namespace

// DaoHui: a key press that went to the application while the input method is
// on, and why (weasel::LogEvent). Releases and modifiers are left out.
static void LogPassedKey(WPARAM vk, LPARAM lParam, const wchar_t* why) {
  if ((static_cast<DWORD>(lParam) >> 31) & 1)
    return;
  switch (vk) {
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
    case VK_LWIN:
    case VK_RWIN:
      return;
    default:
      break;
  }
  weasel::LogEvent(L"key 0x%02x went to the application: %ls", (unsigned)vk,
                   why);
}

BOOL WeaselTSF::_IsPasteKey(WPARAM vk) {
  auto down = [this](int k) { return (_lpbKeyState[k] & 0x80) != 0; };
  const bool ctrl = down(VK_CONTROL), shift = down(VK_SHIFT);
  if (down(VK_MENU) || down(VK_LWIN) || down(VK_RWIN))
    return FALSE;
  return (vk == 'V' && ctrl) || (vk == VK_INSERT && shift && !ctrl);
}

void WeaselTSF::_HoldPasteKey(WPARAM vk, LPARAM lParam) {
  StopPasteTimer();
  _pasteHold = PasteHold::AwaitResponse;
  _pasteVk = vk;
  _pasteLParam = lParam;
  _pasteCtrl = (_lpbKeyState[VK_CONTROL] & 0x80) != 0;
  _pasteShift = (_lpbKeyState[VK_SHIFT] & 0x80) != 0;
  _pasteHeldTick = GetTickCount();
  tPasteService = this;
  tPasteTimer = SetTimer(NULL, 0, kPasteFallbackMs, PasteTimerProc);
  TsfDebugLog(L"paste: key %u held back", (unsigned)vk);
}

void WeaselTSF::_CancelPasteKey(const wchar_t* why) {
  if (_pasteHold == PasteHold::None)
    return;
  _pasteHold = PasteHold::None;
  if (tPasteService == this)
    StopPasteTimer();
  TsfDebugLog(L"paste: held key dropped (%ls)", why);
}

void WeaselTSF::_ReplayPasteKey(const wchar_t* why) {
  if (_pasteHold == PasteHold::None)
    return;
  _pasteHold = PasteHold::None;
  if (tPasteService == this)
    StopPasteTimer();
  const DWORD age = GetTickCount() - _pasteHeldTick;
  if (age > kPasteHoldMs) {
    TsfDebugLog(L"paste: held key dropped after %lu ms (%ls)", age, why);
    return;
  }
  // The modifiers the user still holds are not pressed again; those already
  // released are pressed around the key.
  const bool addCtrl = _pasteCtrl && !(GetAsyncKeyState(VK_CONTROL) & 0x8000);
  const bool addShift = _pasteShift && !(GetAsyncKeyState(VK_SHIFT) & 0x8000);
  const bool extended = (_pasteLParam >> 24) & 1;
  INPUT inputs[6] = {};
  int n = 0;
  auto key = [&](WORD vk, bool up, bool ext) {
    INPUT& in = inputs[n++];
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vk;
    in.ki.wScan = static_cast<WORD>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
    in.ki.dwFlags = (up ? KEYEVENTF_KEYUP : 0) | (ext ? KEYEVENTF_EXTENDEDKEY : 0);
  };
  if (addCtrl)
    key(VK_CONTROL, false, false);
  if (addShift)
    key(VK_SHIFT, false, false);
  key(static_cast<WORD>(_pasteVk), false, extended);
  key(static_cast<WORD>(_pasteVk), true, extended);
  if (addShift)
    key(VK_SHIFT, true, false);
  if (addCtrl)
    key(VK_CONTROL, true, false);
  _pasteSentTick = GetTickCount() | 1;
  UINT sent = SendInput(n, inputs, sizeof(INPUT));
  TsfDebugLog(L"paste: key %u sent again after %lu ms (%ls), %u/%d events",
              (unsigned)_pasteVk, age, why, sent, n);
  if (sent == 0)
    _pasteSentTick = 0;
}

void WeaselTSF::_ProcessKeyEvent(WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
  // when _IsKeyboardDisabled don't eat the key,
  // when keyboard closable and keyboard closed, don't eat the key
  if (_isToOpenClose && !_IsKeyboardOpen()) {
    *pfEaten = FALSE;
    LogQuietPass(kPassClosed, wParam, lParam, L"the keyboard is closed");
    return;
  }
  const wchar_t* disabled = L"";
  if (_IsKeyboardDisabled(&disabled)) {
    *pfEaten = FALSE;
    LogQuietPass(kPassDisabled, wParam, lParam, disabled);
    return;
  }

  // if server connection is Not OK, don't eat it.
  if (!_EnsureServerConnected()) {
    *pfEaten = FALSE;
    LogPassedKey(wParam, lParam,
                 m_client.TimedOut() ? L"server late (echo)" : L"no server");
    return;
  }
  weasel::KeyEvent ke;
  GetKeyboardState(_lpbKeyState);
  if (!ConvertKeyEvent(static_cast<UINT>(wParam), lParam, _lpbKeyState, ke)) {
    /* Unknown key event */
    *pfEaten = FALSE;
    LogQuietPass(kPassUnknown, wParam, lParam, L"not a key Rime knows");
  } else {
    // cheet key code when vertical auto reverse happened, swap up and down
    if (_cand->GetIsReposition()) {
      if (ke.keycode == ibus::Up)
        ke.keycode = ibus::Down;
      else if (ke.keycode == ibus::Down)
        ke.keycode = ibus::Up;
    }
    if (keyCountToSimulate && ke.keycode == ibus::Caps_Lock &&
        !(ke.mask & ibus::RELEASE_MASK) &&
        GetTickCount() - capsSentTick > kCapsLateMs)
      weasel::LogEvent(L"caps lock: skipped as the one sent to turn the light "
                       L"off, %lu ms after it was sent",
                       GetTickCount() - capsSentTick);
    if (!keyCountToSimulate || ke.keycode != ibus::Caps_Lock) {
      const bool wasComposing = _status.composing || _IsComposing();
      *pfEaten = (BOOL)m_client.ProcessKeyEvent(ke);
      if (!*pfEaten && m_client.TimedOut())
        LogPassedKey(wParam, lParam, L"server late (key)");
      else if (!*pfEaten && wasComposing)
        LogPassedKey(wParam, lParam, L"server did not take it while composing");
      else if (!*pfEaten && !_status.ascii_mode) {
        const bool caps = (GetKeyState(VK_CAPITAL) & 0x01) != 0;
        const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        LogQuietPass(kPassNotTaken, wParam, lParam,
                     caps && shift ? L"Rime did not take it in Chinese mode "
                                     L"(Caps Lock on, Shift held)"
                     : caps        ? L"Rime did not take it in Chinese mode "
                                     L"(Caps Lock on)"
                     : shift       ? L"Rime did not take it in Chinese mode "
                                     L"(Shift held)"
                                   : L"Rime did not take it in Chinese mode");
      }
      if (!(ke.mask & ibus::RELEASE_MASK) && _IsPasteKey(wParam)) {
        const bool sentAgain =
            _pasteSentTick && GetTickCount() - _pasteSentTick < kPasteHoldMs;
        _pasteSentTick = 0;
        if (!sentAgain && !*pfEaten && wasComposing) {
          _HoldPasteKey(wParam, lParam);
          *pfEaten = TRUE;
        }
      }
    }

    if (ke.keycode == ibus::Caps_Lock) {
      if (prevKeyEvent.keycode == ibus::Caps_Lock && prevfEaten == TRUE &&
          (ke.mask & ibus::RELEASE_MASK) && (!keyCountToSimulate)) {
        if ((GetKeyState(VK_CAPITAL) & 0x01)) {
          if (_committed || (!*pfEaten && _status.composing)) {
            keyCountToSimulate = 2;
            capsSentTick = GetTickCount();
            INPUT inputs[2];
            inputs[0].type = INPUT_KEYBOARD;
            inputs[0].ki = {VK_CAPITAL, 0, 0, 0, 0};
            inputs[1].type = INPUT_KEYBOARD;
            inputs[1].ki = {VK_CAPITAL, 0, KEYEVENTF_KEYUP, 0, 0};
            UINT sent = ::SendInput(sizeof(inputs) / sizeof(INPUT), inputs,
                                    sizeof(INPUT));
            if (sent != 2) {
              weasel::LogEvent(L"caps lock: %u of 2 keys sent to turn the "
                               L"light off (error %lu)",
                               sent, GetLastError());
              // (Nothing on its way: nothing to wait for. One sent: the
              // press still comes back and is skipped as usual.)
              if (sent == 0)
                keyCountToSimulate = 0;
            }
          }
        }
        *pfEaten = TRUE;
      }
      if (keyCountToSimulate)
        keyCountToSimulate--;
    }

    prevfEaten = *pfEaten;
    prevKeyEvent = ke;
  }
}

STDAPI WeaselTSF::OnSetFocus(BOOL fForeground) {
  if (fForeground)
    m_client.FocusIn();
  else {
    m_client.FocusOut();
    TsfDebugLog(L"OnSetFocus(FALSE)");
    _CancelPasteKey(L"focus left");
    _KeepComposition(_pEditSessionContext);
  }

  return S_OK;
}

/* Some apps sends strange OnTestKeyDown/OnKeyDown combinations:
 *  Some sends OnKeyDown() only. (QQ2012)
 *  Some sends multiple OnTestKeyDown() for a single key event. (MS WORD 2010
 * x64)
 *
 * We assume every key event will eventually cause a OnKeyDown() call.
 * We use _fTestKeyDownPending to omit multiple OnTestKeyDown() calls,
 *  and for OnKeyDown() to check if the key has already been sent to the server.
 */

STDAPI WeaselTSF::OnTestKeyDown(ITfContext* pContext,
                                WPARAM wParam,
                                LPARAM lParam,
                                BOOL* pfEaten) {
  _fTestKeyUpPending = FALSE;
  if (_fTestKeyDownPending) {
    *pfEaten = TRUE;
    return S_OK;
  }
  _ProcessKeyEvent(wParam, lParam, pfEaten);
  _UpdateComposition(pContext);
  if (*pfEaten)
    _fTestKeyDownPending = TRUE;
  return S_OK;
}

STDAPI WeaselTSF::OnKeyDown(ITfContext* pContext,
                            WPARAM wParam,
                            LPARAM lParam,
                            BOOL* pfEaten) {
  _fTestKeyUpPending = FALSE;
  if (_fTestKeyDownPending) {
    _fTestKeyDownPending = FALSE;
    *pfEaten = TRUE;
  } else {
    _ProcessKeyEvent(wParam, lParam, pfEaten);
    _UpdateComposition(pContext);
  }
  return S_OK;
}

STDAPI WeaselTSF::OnTestKeyUp(ITfContext* pContext,
                              WPARAM wParam,
                              LPARAM lParam,
                              BOOL* pfEaten) {
  _fTestKeyDownPending = FALSE;
  if (_fTestKeyUpPending) {
    *pfEaten = TRUE;
    return S_OK;
  }
  _ProcessKeyEvent(wParam, lParam, pfEaten);
  _UpdateComposition(pContext);
  if (*pfEaten)
    _fTestKeyUpPending = TRUE;
  return S_OK;
}

STDAPI WeaselTSF::OnKeyUp(ITfContext* pContext,
                          WPARAM wParam,
                          LPARAM lParam,
                          BOOL* pfEaten) {
  _fTestKeyDownPending = FALSE;
  if (_fTestKeyUpPending) {
    _fTestKeyUpPending = FALSE;
    *pfEaten = TRUE;
  } else {
    _ProcessKeyEvent(wParam, lParam, pfEaten);
    if (!_async_edit)
      _UpdateComposition(pContext);
  }
  return S_OK;
}

STDAPI WeaselTSF::OnPreservedKey(ITfContext* pContext,
                                 REFGUID rguid,
                                 BOOL* pfEaten) {
  *pfEaten = FALSE;
  return S_OK;
}

BOOL WeaselTSF::_InitKeyEventSink() {
  com_ptr<ITfKeystrokeMgr> pKeystrokeMgr;
  HRESULT hr;

  if (_pThreadMgr->QueryInterface(&pKeystrokeMgr) != S_OK)
    return FALSE;

  hr = pKeystrokeMgr->AdviseKeyEventSink(_tfClientId, (ITfKeyEventSink*)this,
                                         TRUE);

  return (hr == S_OK);
}

void WeaselTSF::_UninitKeyEventSink() {
  com_ptr<ITfKeystrokeMgr> pKeystrokeMgr;

  if (_pThreadMgr->QueryInterface(&pKeystrokeMgr) != S_OK)
    return;

  pKeystrokeMgr->UnadviseKeyEventSink(_tfClientId);
}

BOOL WeaselTSF::_InitPreservedKey() {
  return TRUE;
#if 0
	com_ptr<ITfKeystrokeMgr> pKeystrokeMgr;
	if (_pThreadMgr->QueryInterface(pKeystrokeMgr.GetAddressOf()) != S_OK)
	{
		return FALSE;
	}
	TF_PRESERVEDKEY preservedKeyImeMode;

	/* Define SHIFT ONLY for now */
	preservedKeyImeMode.uVKey = VK_SHIFT;
	preservedKeyImeMode.uModifiers = TF_MOD_ON_KEYUP;

	auto hr = pKeystrokeMgr->PreserveKey(
		_tfClientId,
		GUID_IME_MODE_PRESERVED_KEY,
		&preservedKeyImeMode, L"", 0);
	
	return SUCCEEDED(hr);
#endif
}

void WeaselTSF::_UninitPreservedKey() {}
