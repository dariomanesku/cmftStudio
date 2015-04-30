/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "appconfig.h"

#include <stdio.h>                   // fprintf
#include "globals.h"                 // g_frameNum
#include "tinystl.h"                 // cs::TinyStlAllocator
#include <dm/datastructures/list.h>  // dm::ListT

// Dm allocator impl.
//-----

#include "config.h" // g_config.m_memorySize
#define DM_MEM_SIZE_FUNC cs::memSize
namespace cs
{
    static inline size_t memSize()
    {
        configFromDefaultPaths(g_config);
        return size_t(g_config.m_memorySize);
    }
}
#define DM_ALLOCATOR_IMPL
#   include <dm/allocator/allocator.h>
#undef DM_ALLOCATOR_IMPL

// DelayedFree and bgfxAlloc impl.
//-----

namespace cs
{
    #if DM_ALLOCATOR
        struct DelayedFreeAllocator : public bx::AllocatorI
        {
            virtual ~DelayedFreeAllocator()
            {
            }

            virtual void* alloc(size_t _size, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
            {
                BX_UNUSED(_align, _file, _line);

                return DM_ALLOC(dm::mainAlloc, _size);
            }

            virtual void free(void* _ptr, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
            {
                BX_UNUSED(_align, _file, _line);

                PostponedFree* free = m_free.addNew();
                free->m_frame = g_frameNum + 2; // Postpone deallocation for two frames.
                free->m_ptr = _ptr;
            }

            void cleanup()
            {
                const uint32_t currFrame = g_frameNum;

                for (uint16_t ii = m_free.count(); ii--; )
                {
                    if (m_free[ii].m_frame <= currFrame)
                    {
                        DM_FREE(dm::mainAlloc, m_free[ii].m_ptr);
                        m_free.removeAt(ii);
                    }
                }
            }

        private:
            struct PostponedFree
            {
                uint32_t m_frame;
                void*    m_ptr;
            };

            dm::ListT<PostponedFree, 512> m_free;
        };
        static DelayedFreeAllocator s_delayedFreeAllocator;

        struct BgfxAllocator : public bx::ReallocatorI
        {
            BgfxAllocator()
            {
                #if DM_ALLOC_PRINT_STATS
                m_alloc   = 0;
                m_realloc = 0;
                m_free    = 0;
                #endif //DM_ALLOC_PRINT_STATS
            }

            virtual ~BgfxAllocator()
            {
            }

            virtual void* alloc(size_t _size, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
            {
                BX_UNUSED(_align, _file, _line);

                #if DM_ALLOC_PRINT_STATS
                m_mutex.lock();
                m_alloc++;
                m_mutex.unlock();
                #endif //DM_ALLOC_PRINT_STATS

                DM_PRINT_BGFX("Bgfx alloc: %zuB", _size);

                return DM_ALLOC(dm::mainAlloc, _size);
            }

            virtual void free(void* _ptr, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
            {
                BX_UNUSED(_align, _file, _line);

                #if DM_ALLOC_PRINT_STATS
                m_mutex.lock();
                m_free++;
                m_mutex.unlock();
                #endif //DM_ALLOC_PRINT_STATS

                DM_PRINT_BGFX("Bgfx free: %llu.%lluKB - (0x%p)", dm::U_UKB(allocSizeOf(_ptr)), _ptr);

                return DM_FREE(dm::mainAlloc, _ptr);
            }

            virtual void* realloc(void* _ptr, size_t _size, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
            {
                BX_UNUSED(_align, _file, _line);

                #if DM_ALLOC_PRINT_STATS
                m_mutex.lock();
                m_realloc++;
                m_mutex.unlock();
                #endif //DM_ALLOC_PRINT_STATS

                DM_PRINT_BGFX("Bgfx realloc: %llu.%lluKB - (0x%p)", dm::U_UKB(allocSizeOf(_ptr)), _ptr);

                return DM_REALLOC(dm::mainAlloc, _ptr, _size);
            }

            #if DM_ALLOC_PRINT_STATS
            void printStats()
            {
                fprintf(stderr
                      , "Bgfx allocator:\n"
                        "\t%d/%d/%d (alloc/realloc/free)\n\n"
                      , m_alloc, m_realloc, m_free
                      );
            }
            #endif //DM_ALLOC_PRINT_STATS

        private:
            #if DM_ALLOC_PRINT_STATS
            uint32_t m_alloc;
            uint32_t m_realloc;
            uint32_t m_free;
            bx::LwMutex m_mutex;
            #endif //DM_ALLOC_PRINT_STATS
        };
        static BgfxAllocator s_bgfxAllocator;

    #else //!DM_ALLOCATOR

        struct CrtDelayedFreeAllocator : public bx::AllocatorI
        {
            virtual ~CrtDelayedFreeAllocator()
            {
            }

            virtual void* alloc(size_t _size, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
            {
                BX_UNUSED(_align, _file, _line);

                return ::malloc(_size);
            }

            virtual void free(void* _ptr, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
            {
                BX_UNUSED(_align, _file, _line);

                PostponedFree* free = m_free.addNew();
                free->m_frame = g_frameNum + 2; // Postpone deallocation for two frames.
                free->m_ptr = _ptr;
            }

            void cleanup()
            {
                const uint32_t currFrame = g_frameNum;

                for (uint16_t ii = m_free.count(); ii--; )
                {
                    if (m_free[ii].m_frame <= currFrame)
                    {
                        ::free(m_free[ii].m_ptr);
                        m_free.removeAt(ii);
                    }
                }
            }

        private:
            struct PostponedFree
            {
                uint32_t m_frame;
                void*    m_ptr;
            };

            dm::ListT<PostponedFree, 512> m_free;
        };
        static CrtDelayedFreeAllocator s_crtDelayedFreeAllocator;
    #endif // DM_ALLOCATOR
} // namespace cs

// Interface impl.
//-----

namespace cs
{
    #if DM_ALLOCATOR
        bx::AllocatorI*   delayedFree = &s_delayedFreeAllocator;
        bx::ReallocatorI* bgfxAlloc   = &s_bgfxAllocator;

        void allocGc()
        {
            s_delayedFreeAllocator.cleanup();
        }
    #else //!DM_ALLOCATOR.
        bx::AllocatorI*   delayedFree = &s_crtDelayedFreeAllocator;
        bx::ReallocatorI* bgfxAlloc   = dm::crtAlloc;

        void allocGc()
        {
            s_crtDelayedFreeAllocator.cleanup();
        }
    #endif //DM_ALLOCATOR

    static bool s_allocatorDestroyed = false;
    void allocDestroy()
    {
        s_allocatorDestroyed = true;
    }
} // namespace cs

// Alloc redirection.
//-----

#if CS_OVERRIDE_NEWDELETE && DM_ALLOCATOR
    void* operator new(size_t _size)
    {
        // Make sure memory is initialized.
        static const bool assertInitialized = dm::allocInit();
        BX_UNUSED(assertInitialized);
        //return DM_ALLOC(dm::mainAlloc, _size);
        return dm::s_memory.alloc(_size); // Apple llvm compiler prefers it this way.
    }

    void* operator new[](size_t _size)
    {
        static const bool assertInitialized = dm::allocInit();
        BX_UNUSED(assertInitialized);
        //return DM_ALLOC(dm::mainAlloc, _size);
        return dm::s_memory.alloc(_size); // Apple llvm compiler prefers it this way.
    }

    void operator delete(void* _ptr)
    {
        static const bool assertInitialized = dm::allocInit();
        BX_UNUSED(assertInitialized);
        if (!cs::s_allocatorDestroyed)
        {
            //DM_FREE(dm::mainAlloc, _ptr);
            dm::s_memory.free(_ptr); // Apple llvm compiler prefers it this way.
        }
        else if (!dm::allocContains(_ptr))
        {
            ::free(_ptr);
        }
    }

    void operator delete[](void* _ptr)
    {
        static const bool assertInitialized = dm::allocInit();
        BX_UNUSED(assertInitialized);
        if (!cs::s_allocatorDestroyed)
        {
            //DM_FREE(dm::mainAlloc, _ptr);
            dm::s_memory.free(_ptr); // Apple llvm compiler prefers it this way.
        }
        else if (!dm::allocContains(_ptr))
        {
            ::free(_ptr);
        }
    }
#endif // CS_OVERRIDE_NEWDELETE && CS_OVERRIDE_TINYSTL_ALLOCATOR

#if !ENTRY_CONFIG_IMPLEMENT_DEFAULT_ALLOCATOR
    namespace entry
    {
        bx::ReallocatorI* getDefaultAllocator()
        {
            #if DM_ALLOCATOR
            static const bool assertInitialized = dm::allocInit();
                BX_UNUSED(assertInitialized);

                return dm::mainAlloc;
            #else
                return &dm::s_crtAllocator;
            #endif // DM_ALLOCATOR
        }
    }
#endif // !ENTRY_CONFIG_IMPLEMENT_DEFAULT_ALLOCATOR

namespace cs
{
    #if CS_OVERRIDE_TINYSTL_ALLOCATOR && DM_ALLOCATOR
        void* TinyStlAllocator::static_allocate(size_t _bytes)
        {
            static const bool assertInitialized = dm::allocInit();
            BX_UNUSED(assertInitialized);
            return DM_ALLOC(dm::mainAlloc, _bytes);
        }

        void TinyStlAllocator::static_deallocate(void* _ptr, size_t /*_bytes*/)
        {
            static const bool assertInitialized = dm::allocInit();
            BX_UNUSED(assertInitialized);

            return DM_FREE(dm::mainAlloc, _ptr);
        }
    #elif CS_OVERRIDE_TINYSTL_ALLOCATOR
        void* TinyStlAllocator::static_allocate(size_t _bytes)
        {
            return ::malloc(_bytes);
        }

        void TinyStlAllocator::static_deallocate(void* _ptr, size_t /*_bytes*/)
        {
            return ::free(_ptr);
        }
    #endif // CS_OVERRIDE_TINYSTL_ALLOCATOR && DM_ALLOCATOR
} // namespace cs

/* vim: set sw=4 ts=4 expandtab: */
