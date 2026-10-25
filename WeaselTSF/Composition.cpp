#include "stdafx.h"
#include "WeaselTSF.h"
#include "EditSession.h"
#include "ResponseParser.h"
#include "CandidateList.h"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <vector>

// Diagnostics for tools/tsf_test.py of the input method project: start the
// application with WEASEL_TSF_LOG set to a file path and the composition
// events of that process are appended to it. Off (one check) otherwise.
void TsfDebugLog(const wchar_t* fmt, ...) {
  static int enabled = -1;
  static wchar_t path[MAX_PATH];
  if (enabled < 0)
    enabled = GetEnvironmentVariableW(L"WEASEL_TSF_LOG", path, MAX_PATH) > 0;
  if (!enabled)
    return;
  wchar_t buf[512];
  va_list ap;
  va_start(ap, fmt);
  _vsnwprintf_s(buf, _TRUNCATE, fmt, ap);
  va_end(ap);
  FILE* f = nullptr;
  if (_wfopen_s(&f, path, L"a") == 0 && f) {
    fwprintf(f, L"%lu %ls\n", GetTickCount(), buf);
    fclose(f);
  }
}

/* Start Composition */
class CStartCompositionEditSession : public CEditSession {
 public:
  CStartCompositionEditSession(com_ptr<WeaselTSF> pTextService,
                               com_ptr<ITfContext> pContext,
                               BOOL fCUASWorkaroundEnabled,
                               BOOL inlinePreeditEnabled)
      : CEditSession(pTextService, pContext),
        _inlinePreeditEnabled(inlinePreeditEnabled) {
    _fCUASWorkaroundEnabled = fCUASWorkaroundEnabled;
  }

  /* ITfEditSession */
  STDMETHODIMP DoEditSession(TfEditCookie ec);

 private:
  BOOL _fCUASWorkaroundEnabled;
  BOOL _inlinePreeditEnabled;
};

STDAPI CStartCompositionEditSession::DoEditSession(TfEditCookie ec) {
  HRESULT hr = E_FAIL;
  com_ptr<ITfInsertAtSelection> pInsertAtSelection;
  com_ptr<ITfRange> pRangeComposition;
  if (_pContext->QueryInterface(IID_ITfInsertAtSelection,
                                (LPVOID*)&pInsertAtSelection) != S_OK)
    return hr;
  if (pInsertAtSelection->InsertTextAtSelection(ec, TF_IAS_QUERYONLY, NULL, 0,
                                                &pRangeComposition) != S_OK)
    return hr;

  com_ptr<ITfContextComposition> pContextComposition;
  com_ptr<ITfComposition> pComposition;
  if (_pContext->QueryInterface(IID_ITfContextComposition,
                                (LPVOID*)&pContextComposition) != S_OK)
    return hr;
  HRESULT hrStart = pContextComposition->StartComposition(
      ec, pRangeComposition, _pTextService, &pComposition);
  if (hrStart != S_OK || pComposition == NULL)
    TsfDebugLog(L"start: hr=%08x composition=%d", hrStart, pComposition != NULL);
  if ((hrStart == S_OK) && (pComposition != NULL)) {
    _pTextService->_SetComposition(pComposition);

    /* WORKAROUND:
     *   CUAS does not provide a correct GetTextExt() position unless the
     * composition is filled with characters. So we insert a zero width space
     * here. The workaround is only needed when inline preedit is not enabled.
     *   See https://github.com/rime/weasel/pull/883#issuecomment-1567625762
     */
    if (!_inlinePreeditEnabled) {
      pRangeComposition->SetText(ec, TF_ST_CORRECTION, L" ", 1);
    }
    _pTextService->_SetShownText(_inlinePreeditEnabled ? L"" : L" ");

    /* set selection */
    TF_SELECTION tfSelection;
    if (_inlinePreeditEnabled)
      pRangeComposition->Collapse(ec, TF_ANCHOR_END);
    else
      pRangeComposition->Collapse(ec, TF_ANCHOR_START);
    tfSelection.range = pRangeComposition;
    tfSelection.style.ase = TF_AE_NONE;
    tfSelection.style.fInterimChar = FALSE;
    _pContext->SetSelection(ec, 1, &tfSelection);
  }

  return hr;
}

void WeaselTSF::_StartComposition(com_ptr<ITfContext> pContext,
                                  BOOL fCUASWorkaroundEnabled) {
  com_ptr<CStartCompositionEditSession> pStartCompositionEditSession;
  pStartCompositionEditSession.Attach(new CStartCompositionEditSession(
      this, pContext, fCUASWorkaroundEnabled, _cand->style().inline_preedit));
  _cand->StartUI();
  if (pStartCompositionEditSession != nullptr) {
    HRESULT hr;
    pContext->RequestEditSession(_tfClientId, pStartCompositionEditSession,
                                 TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
  }
}

/* End Composition */
class CEndCompositionEditSession : public CEditSession {
 public:
  CEndCompositionEditSession(com_ptr<WeaselTSF> pTextService,
                             com_ptr<ITfContext> pContext,
                             com_ptr<ITfComposition> pComposition,
                             BOOL clear = TRUE)
      : CEditSession(pTextService, pContext), _clear(clear) {
    _pComposition = pComposition;
  }

  /* ITfEditSession */
  STDMETHODIMP DoEditSession(TfEditCookie ec);

 private:
  com_ptr<ITfComposition> _pComposition;
  BOOL _clear;
};

STDAPI CEndCompositionEditSession::DoEditSession(TfEditCookie ec) {
  /* Clear the dummy text we set before, if any. */
  if (_pComposition == nullptr)
    return S_OK;

  _pTextService->_ClearCompositionDisplayAttributes(ec, _pContext);

  // Only what this service wrote there is removed. In a browser the page
  // may rewrite its text right after a commit, and the composition range
  // can then grow over text that is already committed: clearing it took
  // the whole line with it.
  com_ptr<ITfRange> pCompositionRange;
  if (_clear && _pComposition->GetRange(&pCompositionRange) == S_OK &&
      pCompositionRange != nullptr) {
    if (_pTextService->_IsShownText(ec, pCompositionRange))
      pCompositionRange->SetText(ec, 0, L"", 0);
    else
      TsfDebugLog(L"end session: composition holds other text, kept");
  }

  HRESULT hr = _pComposition->EndComposition(ec);
  TsfDebugLog(L"end session runs: clear=%d EndComposition hr=%08x", _clear, hr);
  _pTextService->_FinalizeComposition(_pComposition);
  return S_OK;
}

void WeaselTSF::_EndComposition(com_ptr<ITfContext> pContext, BOOL clear) {
  CEndCompositionEditSession* pEditSession;
  HRESULT hr;

  // One end per composition. The session is granted when the application
  // gets round to it (TF_ES_ASYNCDONTCARE), and until it has run
  // _IsComposing() is still true: a second request for the same composition
  // (losing the focus is reported twice, by OnSetFocus and OnKillThreadFocus)
  // would run after the first and, if asked to clear, delete what the first
  // one kept.
  if (_pComposition != nullptr && _pComposition.p == _pEndingComposition) {
    TsfDebugLog(L"end requested again for the same composition: ignored (clear=%d)", clear);
    return;
  }
  _pEndingComposition = _pComposition.p;

  _cand->EndUI();
  if ((pEditSession = new CEndCompositionEditSession(
           this, pContext, _pComposition, clear)) != NULL) {
    HRESULT hrRequest =
        pContext->RequestEditSession(_tfClientId, pEditSession,
                                     TF_ES_ASYNCDONTCARE | TF_ES_READWRITE,
                                     &hr);
    TsfDebugLog(L"end requested: clear=%d request=%08x session=%08x", clear,
                hrRequest, hr);
    pEditSession->Release();
  }
}

/* Get Text Extent */
class CGetTextExtentEditSession : public CEditSession {
 public:
  CGetTextExtentEditSession(com_ptr<WeaselTSF> pTextService,
                            com_ptr<ITfContext> pContext,
                            com_ptr<ITfContextView> pContextView,
                            com_ptr<ITfComposition> pComposition,
                            bool enhancedPosition)
      : CEditSession(pTextService, pContext) {
    _pContextView = pContextView;
    _pComposition = pComposition;
    _enhancedPosition = enhancedPosition;
  }

  /* ITfEditSession */
  STDMETHODIMP DoEditSession(TfEditCookie ec);

 private:
  com_ptr<ITfContextView> _pContextView;
  com_ptr<ITfComposition> _pComposition;
  bool _enhancedPosition;
};

STDAPI CGetTextExtentEditSession::DoEditSession(TfEditCookie ec) {
  com_ptr<ITfInsertAtSelection> pInsertAtSelection;
  com_ptr<ITfRange> pRangeComposition;
  ITfRange* pRange;
  RECT rc;
  BOOL fClipped;
  TF_SELECTION selection;
  ULONG nSelection;

  if (FAILED(_pContext->QueryInterface(IID_ITfInsertAtSelection,
                                       (LPVOID*)&pInsertAtSelection)))
    return E_FAIL;
  if (FAILED(_pContext->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &selection,
                                     &nSelection)))
    return E_FAIL;

  if (_pComposition != nullptr && _pComposition->GetRange(&pRange) == S_OK) {
    pRange->Collapse(ec, TF_ANCHOR_START);
  } else {
    // composition end
    // note: selection.range is always an empty range
    pRange = selection.range;
  }

  if ((_pContextView->GetTextExt(ec, pRange, &rc, &fClipped)) == S_OK &&
      (rc.left != 0 || rc.top != 0)) {
    // get the foreground window pos and check if rc from GetTextExt is out of
    // window
    if (_enhancedPosition) {
      HWND hwnd;
      RECT rcForegroundWindow;
      hwnd = GetForegroundWindow();
      ::GetWindowRect(hwnd, &rcForegroundWindow);

      if (rc.left < rcForegroundWindow.left ||
          rc.left > rcForegroundWindow.right ||
          rc.top < rcForegroundWindow.top ||
          rc.top > rcForegroundWindow.bottom) {
        POINT pt;
        bool hasCaret = ::GetCaretPos(&pt);
        int offsetx = rcForegroundWindow.left - rc.left + (hasCaret ? pt.x : 0);
        int offsety = rcForegroundWindow.top - rc.top + (hasCaret ? pt.y : 0);
        rc.left += offsetx;
        rc.right += offsetx;
        rc.top += offsety;
        rc.bottom += offsety;
      }
    }
    _pTextService->_SetCompositionPosition(rc);
  }
  return S_OK;
}

/* Composition Window Handling */
BOOL WeaselTSF::_UpdateCompositionWindow(com_ptr<ITfContext> pContext) {
  com_ptr<ITfContextView> pContextView;
  if (pContext->GetActiveView(&pContextView) != S_OK)
    return FALSE;
  com_ptr<CGetTextExtentEditSession> pEditSession;
  pEditSession.Attach(
      new CGetTextExtentEditSession(this, pContext, pContextView, _pComposition,
                                    _cand->style().enhanced_position));
  if (pEditSession == NULL) {
    return FALSE;
  }
  HRESULT hr;
  pContext->RequestEditSession(_tfClientId, pEditSession,
                               TF_ES_ASYNCDONTCARE | TF_ES_READ, &hr);
  return SUCCEEDED(hr);
}

void WeaselTSF::_SetCompositionPosition(const RECT& rc) {
  /* Test if rect is valid.
   * If it is invalid during CUAS test, we need to apply CUAS workaround */
  if (!_fCUASWorkaroundTested) {
    _fCUASWorkaroundTested = TRUE;
    if (rc.top == rc.bottom) {
      _fCUASWorkaroundEnabled = TRUE;
      return;
    }
  }
  RECT _rc;
  _rc.left = _rc.right = rc.left;
  _rc.top = _rc.bottom = rc.bottom;
  m_client.UpdateInputPosition(rc);
  _cand->UpdateInputPosition(rc);
}

/* Inline Preedit */
class CInlinePreeditEditSession : public CEditSession {
 public:
  CInlinePreeditEditSession(com_ptr<WeaselTSF> pTextService,
                            com_ptr<ITfContext> pContext,
                            com_ptr<ITfComposition> pComposition,
                            const std::shared_ptr<weasel::Context> context)
      : CEditSession(pTextService, pContext),
        _pComposition(pComposition),
        _context(context) {}

  /* ITfEditSession */
  STDMETHODIMP DoEditSession(TfEditCookie ec);

 private:
  com_ptr<ITfComposition> _pComposition;
  const std::shared_ptr<weasel::Context> _context;
};

STDAPI CInlinePreeditEditSession::DoEditSession(TfEditCookie ec) {
  std::wstring preedit = _context->preedit.str;

  com_ptr<ITfRange> pRangeComposition;
  if (_pComposition == nullptr) {
    TsfDebugLog(L"preedit %u: no composition", (unsigned)preedit.length());
    return E_FAIL;
  }
  // (Requested while an end session was pending: that composition is
  // over, and its text is no longer ours to rewrite.)
  if (!_pTextService->_IsCurrentComposition(_pComposition.p)) {
    TsfDebugLog(L"preedit %u: composition already ended", (unsigned)preedit.length());
    return S_OK;
  }
  if ((_pComposition->GetRange(&pRangeComposition)) != S_OK) {
    TsfDebugLog(L"preedit %u: GetRange failed", (unsigned)preedit.length());
    return E_FAIL;
  }

  HRESULT hrSet = pRangeComposition->SetText(
      ec, 0, preedit.c_str(), static_cast<LONG>(preedit.length()));
  if (hrSet != S_OK)
    TsfDebugLog(L"preedit %u: SetText hr=%08x", (unsigned)preedit.length(), hrSet);
  if (hrSet != S_OK) {
    _pTextService->_ForgetShownText();
    return E_FAIL;
  }
  _pTextService->_SetShownText(preedit);

  /* TODO: Check the availability and correctness of these values */
  int sel_cursor = -1;
  for (size_t i = 0; i < _context->preedit.attributes.size(); i++) {
    if (_context->preedit.attributes.at(i).type == weasel::HIGHLIGHTED) {
      sel_cursor = _context->preedit.attributes.at(i).range.cursor;
      break;
    }
  }

  _pTextService->_SetCompositionDisplayAttributes(ec, _pContext,
                                                  pRangeComposition);

  /* Set caret */
  LONG cch;
  TF_SELECTION tfSelection;
  if (sel_cursor < 0) {
    pRangeComposition->Collapse(ec, TF_ANCHOR_END);
  } else {
    pRangeComposition->Collapse(ec, TF_ANCHOR_START);
    pRangeComposition->ShiftStart(ec, sel_cursor, &cch, NULL);
  }
  tfSelection.range = pRangeComposition;
  tfSelection.style.ase = TF_AE_NONE;
  tfSelection.style.fInterimChar = FALSE;
  _pContext->SetSelection(ec, 1, &tfSelection);

  return S_OK;
}

BOOL WeaselTSF::_ShowInlinePreedit(
    com_ptr<ITfContext> pContext,
    const std::shared_ptr<weasel::Context> context) {
  com_ptr<CInlinePreeditEditSession> pEditSession;
  pEditSession.Attach(
      new CInlinePreeditEditSession(this, pContext, _pComposition, context));
  if (pEditSession != NULL) {
    HRESULT hr;
    pContext->RequestEditSession(_tfClientId, pEditSession,
                                 TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
  }
  return TRUE;
}

/* Update Composition */
class CInsertTextEditSession : public CEditSession {
 public:
  CInsertTextEditSession(com_ptr<WeaselTSF> pTextService,
                         com_ptr<ITfContext> pContext,
                         com_ptr<ITfComposition> pComposition,
                         const std::wstring& text)
      : CEditSession(pTextService, pContext),
        _text(text),
        _pComposition(pComposition) {}

  /* ITfEditSession */
  STDMETHODIMP DoEditSession(TfEditCookie ec);

 private:
  std::wstring _text;
  com_ptr<ITfComposition> _pComposition;
};

STDMETHODIMP CInsertTextEditSession::DoEditSession(TfEditCookie ec) {
  com_ptr<ITfRange> pRange;
  TF_SELECTION tfSelection;
  HRESULT hRet = S_OK;

  if (_pComposition == nullptr) {
    TsfDebugLog(L"insert %u: no composition", (unsigned)_text.length());
    return E_FAIL;
  }
  HRESULT hrRange = _pComposition->GetRange(&pRange);
  if (FAILED(hrRange)) {
    TsfDebugLog(L"insert %u: GetRange hr=%08x", (unsigned)_text.length(), hrRange);
    return E_FAIL;
  }

  HRESULT hrSet = pRange->SetText(ec, 0, _text.c_str(),
                                  static_cast<LONG>(_text.length()));
  if (FAILED(hrSet))
    TsfDebugLog(L"insert %u: SetText hr=%08x", (unsigned)_text.length(), hrSet);
  _pTextService->_ForgetShownText();
  if (FAILED(hrSet))
    return E_FAIL;

  /* update the selection to an insertion point just past the inserted text. */
  pRange->Collapse(ec, TF_ANCHOR_END);

  tfSelection.range = pRange;
  tfSelection.style.ase = TF_AE_NONE;
  tfSelection.style.fInterimChar = FALSE;

  _pContext->SetSelection(ec, 1, &tfSelection);

  return hRet;
}

BOOL WeaselTSF::_InsertText(com_ptr<ITfContext> pContext,
                            const std::wstring& text) {
  CInsertTextEditSession* pEditSession;
  HRESULT hr;

  if ((pEditSession = new CInsertTextEditSession(this, pContext, _pComposition,
                                                 text)) != NULL) {
    pContext->RequestEditSession(_tfClientId, pEditSession,
                                 TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
    pEditSession->Release();
  }

  return TRUE;
}

void WeaselTSF::_UpdateComposition(com_ptr<ITfContext> pContext) {
  HRESULT hr;

  _pEditSessionContext = pContext;

  HRESULT hrRequest = _pEditSessionContext->RequestEditSession(
      _tfClientId, this, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
  _async_edit = !!(hr == TF_S_ASYNC);
  if (FAILED(hrRequest))
    TsfDebugLog(L"update: request=%08x", hrRequest);
  _UpdateCompositionWindow(pContext);
}

/* Composition State */
STDAPI WeaselTSF::OnCompositionTerminated(TfEditCookie ecWrite,
                                          ITfComposition* pComposition) {
  // NOTE:
  // This will be called when an edit session ended up with an empty composition
  // string, Even if it is closed normally. Silly M$.

  // The application or TSF has ended the composition (the focus moved, the
  // caret was put elsewhere, ...). The write cookie is ours for this call, so
  // no edit session is requested: one would only run later, on a composition
  // that is gone. With inline preedit the text is the conversion and stays;
  // otherwise it is the placeholder and goes.
  TsfDebugLog(L"OnCompositionTerminated: current=%d inline=%d",
              pComposition == _pComposition.p, _inlinePreedit);
  if (pComposition != nullptr && pComposition == _pComposition.p) {
    if (_pEditSessionContext != nullptr)
      _ClearCompositionDisplayAttributes(ecWrite, _pEditSessionContext);
    if (!_inlinePreedit) {
      com_ptr<ITfRange> pRange;
      if (pComposition->GetRange(&pRange) == S_OK && pRange != nullptr)
        pRange->SetText(ecWrite, 0, L"", 0);
    }
    _FinalizeComposition();
  }
  m_client.ClearComposition();
  _committed = TRUE;
  _cand->Destroy();
  return S_OK;
}

// Ends a composition the user did not finish: focus moved to another window
// or document, the caret left the composition, or the application ended it.
// Stock Weasel deletes the text; here the conversion shown inline stays in
// the document (as with Microsoft Bopomofo and ASUS Smart Input). Rime's
// input is cleared rather than committed: the server only hands committed
// text back with a key event response, so a commit would be inserted again
// on the next key. Without inline preedit the composition only holds
// placeholder text, which is still removed.
void WeaselTSF::_KeepComposition(com_ptr<ITfContext> pContext) {
  TsfDebugLog(L"keep: composing=%d context=%d inline=%d", _IsComposing(),
              pContext != nullptr, _inlinePreedit);
  m_client.ClearComposition();
  // (_inlinePreedit is not reset here: the end session may still be pending,
  // and a later notification for the same composition has to decide the same
  // way. The first version reset it, so whichever call came second cleared
  // the text the first had kept.)
  if (_IsComposing() && pContext)
    _EndComposition(pContext, !_inlinePreedit);
  _committed = TRUE;
  _cand->Destroy();
}

void WeaselTSF::_AbortComposition(bool clear) {
  m_client.ClearComposition();
  if (_IsComposing()) {
    _EndComposition(_pEditSessionContext, clear);
  }
  _committed = TRUE;
  _cand->Destroy();
}

BOOL WeaselTSF::_IsShownText(TfEditCookie ec, ITfRange* pRange) {
  if (!_shownKnown)
    return FALSE;
  // One character more than expected is read, so a longer text differs.
  std::vector<WCHAR> buf(_shownText.size() + 2);
  ULONG cch = 0;
  if (FAILED(pRange->GetText(ec, 0, buf.data(), static_cast<ULONG>(buf.size()),
                             &cch)))
    return FALSE;
  std::wstring got(buf.data(), cch), want = _shownText;
  // (contenteditable turns spaces into no-break spaces)
  std::replace(got.begin(), got.end(), L'\u00a0', L' ');
  std::replace(want.begin(), want.end(), L'\u00a0', L' ');
  return got == want;
}

void WeaselTSF::_FinalizeComposition() {
  _pComposition = nullptr;
  _shownKnown = FALSE;
  _pEndingComposition = nullptr;
}

void WeaselTSF::_FinalizeComposition(ITfComposition* pComposition) {
  if (_pComposition.p == pComposition)
    _FinalizeComposition();
}

void WeaselTSF::_SetComposition(com_ptr<ITfComposition> pComposition) {
  _pComposition = pComposition;
  _pEndingComposition = nullptr;
}

BOOL WeaselTSF::_IsComposing() {
  return _pComposition != NULL;
}
