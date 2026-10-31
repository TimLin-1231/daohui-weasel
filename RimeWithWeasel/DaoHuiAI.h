#pragma once
// DaoHui: AI correction of the composition, Ctrl+R (lua/asus_fix.lua of the
// input-method data). Lua leaves a chat-completions request in the session
// property asus_fix_request; RimeWithWeaselHandler::ProcessKeyEvent hands it
// here, and the answer goes back through asus_fix_result when the key F23
// arrives. See DaoHuiAI.cpp.

#include <rime_api.h>
#include <string>

namespace daohui {

// Takes the request Lua left ("<id>\t<JSON body>"), asks the local model in
// the background and, when the answer is in, keeps "<id>\t<answer>" (or
// "<id>\t!<message>" for the bubble) for session and presses F23.
void AskFix(RimeSessionId session, const std::string& request);

// The answer kept for session, if any; it is taken.
bool TakeFixAnswer(RimeSessionId session, std::string* answer);

// Stops the model server (WeaselServer is quitting).
void StopFixServer();

}  // namespace daohui
