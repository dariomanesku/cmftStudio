/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_ALLOCATOR_CONFIG_H_HEADER_GUARD
#define CMFTSTUDIO_ALLOCATOR_CONFIG_H_HEADER_GUARD

#include <stdio.h>

// Config.
//-----

#define CS_ALLOCATOR_IMPL_LIST  0 // Slower - left for testing purposes.
#define CS_ALLOCATOR_IMPL_ARRAY 1 // Faster - recommended!

#ifndef CS_ALLOCAOTR_IMPL
    #define CS_ALLOCAOTR_IMPL CS_ALLOCATOR_IMPL_ARRAY
#endif //CS_ALLOCAOTR_IMPL

#ifndef CS_NATURAL_ALIGNMENT
    #define CS_NATURAL_ALIGNMENT 16
#endif //CS_NATURAL_ALIGNMENT

#ifndef CS_ALLOC_PRINT_STATS
    #define CS_ALLOC_PRINT_STATS 0
#endif //CS_ALLOC_PRINT_STATS

#ifndef CS_ALLOC_PRINT_USAGE
    #define CS_ALLOC_PRINT_USAGE 0
#endif //CS_ALLOC_PRINT_USAGE

#ifndef CS_ALLOC_PRINT_FILELINE
    #define CS_ALLOC_PRINT_FILELINE 0
#endif //CS_ALLOC_PRINT_FILELINE

#ifndef CS_ALLOC_PRINT_STATIC
    #define CS_ALLOC_PRINT_STATIC 0
#endif //CS_ALLOC_PRINT_STATIC

#ifndef CS_ALLOC_PRINT_SMALL
    #define CS_ALLOC_PRINT_SMALL 0
#endif //CS_ALLOC_PRINT_SMALL

#ifndef CS_ALLOC_PRINT_STACK
    #define CS_ALLOC_PRINT_STACK 0
#endif //CS_ALLOC_PRINT_STACK

#ifndef CS_ALLOC_PRINT_HEAP
    #define CS_ALLOC_PRINT_HEAP 0
#endif //CS_ALLOC_PRINT_HEAP

#ifndef CS_ALLOC_PRINT_EXT
    #define CS_ALLOC_PRINT_EXT 0
#endif //CS_ALLOC_PRINT_EXT

#ifndef CS_ALLOC_PRINT_BGFX
    #define CS_ALLOC_PRINT_BGFX 0
#endif //CS_ALLOC_PRINT_BGFX

#endif // CMFTSTUDIO_ALLOCATOR_CONFIG_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
