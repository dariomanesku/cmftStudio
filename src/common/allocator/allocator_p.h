/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_ALLOCATOR_PRIVATE_H_HEADER_GUARD
#define CMFTSTUDIO_ALLOCATOR_PRIVATE_H_HEADER_GUARD

#include <stdio.h>

#if CS_ALLOC_PRINT_FILELINE
    #define CS_ALLOC_FILE_LINE BX_FILE_LINE_LITERAL
#else
    #define CS_ALLOC_FILE_LINE
#endif //CS_ALLOC_PRINT_FILELINE

#define CS_ALLOC_PRINT(_format, ...) do { fprintf(stderr, "CS MEM " CS_ALLOC_FILE_LINE "" _format "\n", ##__VA_ARGS__); } while(0)

#if CS_ALLOC_PRINT_STATS
    #define CS_PRINT_MEM_STATS(_format, ...) CS_ALLOC_PRINT(_format, __VA_ARGS__)
#else
    #define CS_PRINT_MEM_STATS(...)
#endif //CS_ALLOC_PRINT_STATS

#if CS_ALLOC_PRINT_STATIC
    #define CS_PRINT_STATIC(_format, ...) CS_ALLOC_PRINT(_format, __VA_ARGS__)
#else
    #define CS_PRINT_STATIC(...)
#endif //CS_ALLOC_PRINT_STATIC

#if CS_ALLOC_PRINT_SMALL
    #define CS_PRINT_SMALL(_format, ...) CS_ALLOC_PRINT(_format, __VA_ARGS__)
#else
    #define CS_PRINT_SMALL(...)
#endif //CS_ALLOC_PRINT_SMALL

#if CS_ALLOC_PRINT_STACK
    #define CS_PRINT_STACK(_format, ...) CS_ALLOC_PRINT(_format, __VA_ARGS__)
#else
    #define CS_PRINT_STACK(...)
#endif //CS_ALLOC_PRINT_STACK

#if CS_ALLOC_PRINT_HEAP
    #define CS_PRINT_HEAP(_format, ...) CS_ALLOC_PRINT(_format, __VA_ARGS__)
#else
    #define CS_PRINT_HEAP(...)
#endif //CS_ALLOC_PRINT_HEAP

#if CS_ALLOC_PRINT_EXT
    #define CS_PRINT_EXT(_format, ...) CS_ALLOC_PRINT(_format, __VA_ARGS__)
#else
    #define CS_PRINT_EXT(...)
#endif //CS_ALLOC_PRINT_EXT

#if CS_ALLOC_PRINT_BGFX
    #define CS_PRINT_BGFX(_format, ...) CS_ALLOC_PRINT(_format, __VA_ARGS__)
#else
    #define CS_PRINT_BGFX(...)
#endif //CS_ALLOC_PRINT_BGFX

#endif // CMFTSTUDIO_ALLOCATOR_PRIVATE_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
