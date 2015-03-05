/*
 * Copyright 2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_TINYSTL_H_HEADER_GUARD
#define CMFTSTUDIO_TINYSTL_H_HEADER_GUARD

namespace cs
{
    struct TinyStlAllocator
    {
        static void* static_allocate(size_t _bytes);
        static void static_deallocate(void* _ptr, size_t /*_bytes*/);
    };
} //namespace cs

#define TINYSTL_ALLOCATOR cs::TinyStlAllocator
#include <tinystl/allocator.h>
#include <tinystl/vector.h>
#include <tinystl/unordered_map.h>
#include <tinystl/unordered_set.h>
namespace stl = tinystl;

#endif // CMFTSTUDIO_TINYSTL_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
