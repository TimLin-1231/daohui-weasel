#include "stdafx.h"
#include "WeaselTSF.h"
#include "CandidateList.h"
#include "ResponseParser.h"

STDAPI WeaselTSF::DoEditSession(TfEditCookie ec) {
  // get commit string from server
  std::wstring commit;
  weasel::Config config;
  auto context = std::make_shared<weasel::Context>();
  weasel::ResponseParser parser(&commit, context.get(), &_status, &config,
                                &_cand->style());

  bool ok = m_client.GetResponseData(std::ref(parser));

  _UpdateLanguageBar(_status);

  if (ok) {
    if (!commit.empty()) {
      // For auto-selecting, commit and preedit can both exist.
      // Commit and close the original composition first.
      if (!_IsComposing()) {
        _StartComposition(_pEditSessionContext,
                          _fCUASWorkaroundEnabled && !config.inline_preedit);
      }
      _InsertText(_pEditSessionContext, commit);
      _EndComposition(_pEditSessionContext, false);
      _committed = TRUE;
    } else {
      _committed = FALSE;
    }
    // Rime can be composing with nothing to show inline: right after a
    // commit, while only the next-word bubble is up. No composition is
    // opened for that: an empty one in a browser could later be cleared
    // over the text just committed, and keys Rime passes on (Enter) would
    // reach the page in the middle of a composition.
    const bool wantComposition =
        _status.composing &&
        !(config.inline_preedit && context->preedit.str.empty());
    if (wantComposition && !_IsComposing()) {
      _StartComposition(_pEditSessionContext,
                        _fCUASWorkaroundEnabled && !config.inline_preedit);
    } else if (!wantComposition && _IsComposing()) {
      _EndComposition(_pEditSessionContext, true);
    }
    // (Not "&& _IsComposing()": for the first key the composition is still
    // being started, and the flag has to be right when the focus leaves
    // after that one key.)
    _inlinePreedit = config.inline_preedit;
    if (wantComposition && _IsComposing() && config.inline_preedit) {
      _ShowInlinePreedit(_pEditSessionContext, context);
    }
    _UpdateCompositionWindow(_pEditSessionContext);
  }

  _UpdateUI(*context, _status);

  return TRUE;
}
