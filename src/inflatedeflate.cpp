/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "common/common.h"

// Include minz implementation.
#define MZ_MALLOC(x)     DM_ALLOC(dm::stackAlloc, x)
#define MZ_FREE(x)       DM_FREE(dm::stackAlloc, x)
#define MZ_REALLOC(p, x) DM_REALLOC(dm::stackAlloc, p, x)
#define MINIZ_NO_MALLOC
#include "common/miniz.h"

/* vim: set sw=4 ts=4 expandtab: */
