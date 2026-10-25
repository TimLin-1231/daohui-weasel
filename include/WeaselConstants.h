#pragma once

#include "DaoHuiBrand.h"

// DaoHui: names come from DaoHuiBrand.h (stock: "Weasel",
// L"Software\\Rime\\Weasel"; RIME_REG_KEY removed so uninstalling DaoHui
// never touches HKLM\Software\Rime).
#define WEASEL_CODE_NAME DAOHUI_CODE_NAME
#define WEASEL_REG_KEY DAOHUI_REG_KEY_W

#define STRINGIZE(x) #x
#define VERSION_STR(x) STRINGIZE(x)
#define WEASEL_VERSION VERSION_STR(VERSION_MAJOR.VERSION_MINOR.VERSION_PATCH)
