/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_STB_IMAGE_H_HEADER_GUARD
#define CMFTSTUDIO_STB_IMAGE_H_HEADER_GUARD

#ifndef CS_OVERRIDE_STBI_ALLOCATOR
#   define CS_OVERRIDE_STBI_ALLOCATOR 0
#endif //CS_OVERRIDE_STBI_ALLOCATOR

#if CS_OVERRIDE_STBI_ALLOCATOR
#   undef STBI_MALLOC
#   undef STBI_REALLOC
#   undef STBI_FREE
#   define STBI_MALLOC(sz)    BX_ALLOC(dm::mainAlloc,sz)
#   define STBI_REALLOC(p,sz) BX_REALLOC(dm::mainAlloc,p,sz)
#   define STBI_FREE(p)       BX_FREE(dm::mainAlloc,p)
#endif //CS_OVERRIDE_STBI_ALLOCATOR

namespace stb
{
    #include <stb/stb_image.h>
}

#endif // CMFTSTUDIO_STB_IMAGE_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
