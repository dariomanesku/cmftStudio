/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_MINIZ_H_HEADER_GUARD
#define CMFTSTUDIO_MINIZ_H_HEADER_GUARD

#if BX_COMPILER_GCC
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif // BX_COMPILER_GCC

#define MINIZ_NO_TIME
#define MINIZ_NO_ARCHIVE_APIS
#define MINIZ_NO_ARCHIVE_WRITING_APIS
#include <miniz/miniz.c>

#if BX_COMPILER_GCC
#   pragma GCC diagnostic pop
#endif // BX_COMPILER_GCC

#endif // CMFTSTUDIO_MINIZ_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */

