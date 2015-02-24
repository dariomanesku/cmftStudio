/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "common/common.h"

// Include minz implementation.
#define MZ_MALLOC(x)     BX_ALLOC(cs::g_stackAlloc, x)
#define MZ_FREE(x)       BX_FREE(cs::g_stackAlloc, x)
#define MZ_REALLOC(p, x) BX_REALLOC(cs::g_stackAlloc, p, x)
#define MINIZ_NO_MALLOC
#include "common/miniz.h"

/* vim: set sw=4 ts=4 expandtab: */
