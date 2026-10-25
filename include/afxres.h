#pragma once
// Local build shim: MFC is not installed; non-MFC resources only need winres.h.
#include <winres.h>
#ifndef IDC_STATIC
#define IDC_STATIC (-1)
#endif
