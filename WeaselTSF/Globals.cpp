#include "stdafx.h"
#include "Globals.h"

HINSTANCE g_hInst;

LONG g_cRefDll = -1;

CRITICAL_SECTION g_cs;

// DaoHui (DaoHuiBrand.h)
static const GUID c_clsidTextService = DAOHUI_CLSID_TEXTSERVICE_INIT;

// DaoHui (DaoHuiBrand.h)
static const GUID c_guidProfile = DAOHUI_GUID_PROFILE_INIT;

// DaoHui (DaoHuiBrand.h)
static const GUID c_guidLangBarItemButton = DAOHUI_GUID_LANGBAR_ITEM_BUTTON_INIT;

// DaoHui (DaoHuiBrand.h)
static const GUID c_guidDisplayAttributeInput = DAOHUI_GUID_DISPLAY_ATTRIBUTE_INPUT_INIT;

#ifdef WEASEL_USING_OLDER_TSF_SDK

/* For Windows 8 */
const GUID GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT = {
    0x13A016DF,
    0x560B,
    0x46CD,
    {0x94, 0x7A, 0x4C, 0x3A, 0xF1, 0xE0, 0xE3, 0x5D}};

const GUID GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT = {
    0x25504FB4,
    0x7BAB,
    0x4BC1,
    {0x9C, 0x69, 0xCF, 0x81, 0x89, 0x0F, 0x0E, 0xF5}};

#endif

const GUID GUID_LBI_INPUTMODE = {
    0x2C77A81E,
    0x41CC,
    0x4178,
    {0xA3, 0xA7, 0x5F, 0x8A, 0x98, 0x75, 0x68, 0xE6}};

// DaoHui (DaoHuiBrand.h)
const GUID GUID_IME_MODE_PRESERVED_KEY = DAOHUI_GUID_IME_MODE_PRESERVED_KEY_INIT;
