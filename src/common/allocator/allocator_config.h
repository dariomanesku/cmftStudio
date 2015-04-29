/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

// Small alloc config.
//-----

#if !defined(DM_SMALL_ALLOC_DEF)
    #define DM_SMALL_ALLOC_DEF(_idx, _size, _num)
#endif //!defined(DM_SMALL_ALLOC_DEF)
DM_SMALL_ALLOC_DEF( 0,                16,  16*1024)
DM_SMALL_ALLOC_DEF( 1,                32,  16*1024)
DM_SMALL_ALLOC_DEF( 2,                64, 512*1024) // Std containers use lots of these!
DM_SMALL_ALLOC_DEF( 3,               256,  16*1024)
DM_SMALL_ALLOC_DEF( 4,               512,   8*1024)
DM_SMALL_ALLOC_DEF( 5, DM_KILOBYTES(  1),   8*1024)
DM_SMALL_ALLOC_DEF( 6, DM_KILOBYTES( 16),       64)
DM_SMALL_ALLOC_DEF( 7, DM_KILOBYTES( 64),       64)
DM_SMALL_ALLOC_DEF( 8, DM_KILOBYTES(256),       32)
DM_SMALL_ALLOC_DEF( 9, DM_KILOBYTES(512),       32)
DM_SMALL_ALLOC_DEF(10,   DM_MEGABYTES(1),        8)
DM_SMALL_ALLOC_DEF(11,   DM_MEGABYTES(4),        8)
DM_SMALL_ALLOC_DEF(12,   DM_MEGABYTES(8),        8)
/* -> DM_SMALL_ALLOC_COUNT */
#undef DM_SMALL_ALLOC_DEF

#ifdef DM_SMALL_ALLOC_CONFIG
    #define DM_SMALL_ALLOC_COUNT        13
    #define DM_SMALL_ALLOC_BIGGEST_SIZE DM_MEGABYTES(8)
#endif // DM_SMALL_ALLOC_CONFIG
#undef DM_SMALL_ALLOC_CONFIG

// Alloc config.
//-----

#if !defined(DM_ALLOC_DEF)
    #define DM_ALLOC_DEF(_regionIdx, _num)
#endif //!defined(DM_ALLOC_DEF)
DM_ALLOC_DEF(0, 2048) // for region:    2MB -> DM_ALLOC_SMALLEST_REGION.
DM_ALLOC_DEF(1, 2048) // for region:    4MB
DM_ALLOC_DEF(2, 1024) // for region:    8MB
DM_ALLOC_DEF(3, 1024) // for region:   16MB
DM_ALLOC_DEF(4, 1024) // for region:   32MB
DM_ALLOC_DEF(5,  512) // for region:   64MB
DM_ALLOC_DEF(6,  256) // for region:  128MB
DM_ALLOC_DEF(7,  128) // for region:  256MB
DM_ALLOC_DEF(8,   64) // for region:  512MB
DM_ALLOC_DEF(9,   32) // for region: 1024MB
/* -> DM_ALLOC_NUM_REGIONS */
#undef DM_ALLOC_DEF

#ifdef DM_ALLOC_CONFIG
    #define DM_ALLOC_NUM_REGIONS        10
    #define DM_ALLOC_NUM_SUB_REGIONS    16
    #define DM_ALLOC_SMALLEST_REGION    DM_MEGABYTES(2)
    #define DM_ALLOC_MAX_BIG_FREE_SLOTS 32
#endif // DM_ALLOC_CONFIG
#undef DM_ALLOC_CONFIG

// Allocator config.
//-----

#ifndef DM_ALLOCATOR_CONFIG
#define DM_ALLOCATOR_CONFIG

    // Use #define DM_ALLOCATOR 0 to fallback to default c-runtime allocator.

    #define DM_MEM_MIN_SIZE            DM_MEGABYTES(512)
    #define DM_MEM_DEFAULT_SIZE        DM_MEGABYTES(1536)
    #define DM_MEM_STATIC_STORAGE_SIZE DM_MEGABYTES(32)

    // To override default preallocated memory size:
    //     #define DM_MEM_SIZE_FUNC memSizeFunc
    //     size_t memSizeFunc() { return DM_GIGABYTES(1); }

    #define DM_ALLOCATOR_IMPL_LIST  0 // Slower - left for testing purposes.
    #define DM_ALLOCATOR_IMPL_ARRAY 1 // Faster - recommended!

    #ifndef DM_ALLOCATOR_IMPL
        #define DM_ALLOCATOR_IMPL DM_ALLOCATOR_IMPL_ARRAY
    #endif //DM_ALLOCATOR_IMPL

    #ifndef CS_NATURAL_ALIGNMENT
        #define CS_NATURAL_ALIGNMENT 16
    #endif //CS_NATURAL_ALIGNMENT

    #ifndef DM_ALLOC_PRINT_STATS
        #define DM_ALLOC_PRINT_STATS 0
    #endif //DM_ALLOC_PRINT_STATS

    #ifndef DM_ALLOC_PRINT_USAGE
        #define DM_ALLOC_PRINT_USAGE 0
    #endif //DM_ALLOC_PRINT_USAGE

    #ifndef DM_ALLOC_PRINT_FILELINE
        #define DM_ALLOC_PRINT_FILELINE 0
    #endif //DM_ALLOC_PRINT_FILELINE

    #ifndef DM_ALLOC_PRINT_STATIC
        #define DM_ALLOC_PRINT_STATIC 0
    #endif //DM_ALLOC_PRINT_STATIC

    #ifndef DM_ALLOC_PRINT_SMALL
        #define DM_ALLOC_PRINT_SMALL 0
    #endif //DM_ALLOC_PRINT_SMALL

    #ifndef DM_ALLOC_PRINT_STACK
        #define DM_ALLOC_PRINT_STACK 0
    #endif //DM_ALLOC_PRINT_STACK

    #ifndef DM_ALLOC_PRINT_HEAP
        #define DM_ALLOC_PRINT_HEAP 0
    #endif //DM_ALLOC_PRINT_HEAP

    #ifndef DM_ALLOC_PRINT_EXT
        #define DM_ALLOC_PRINT_EXT 0
    #endif //DM_ALLOC_PRINT_EXT

    #ifndef DM_ALLOC_PRINT_BGFX
        #define DM_ALLOC_PRINT_BGFX 0
    #endif //DM_ALLOC_PRINT_BGFX

#endif // DM_ALLOCATOR_CONFIG

/* vim: set sw=4 ts=4 expandtab: */
