/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_MINIZ_H_HEADER_GUARD
#define CMFTSTUDIO_MINIZ_H_HEADER_GUARD

#include <bx/macros.h>

BX_PRAGMA_DIAGNOSTIC_PUSH_GCC()
BX_PRAGMA_DIAGNOSTIC_IGNORED_GCC("-Wstrict-aliasing")
#define MINIZ_NO_TIME
#define MINIZ_NO_ARCHIVE_APIS
#define MINIZ_NO_ARCHIVE_WRITING_APIS
#include <miniz/miniz.c>
BX_PRAGMA_DIAGNOSTIC_POP_GCC()

#endif // CMFTSTUDIO_MINIZ_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */

