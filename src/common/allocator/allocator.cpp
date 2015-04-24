/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "../appconfig.h"
#include "allocator_config.h"
#include "allocator.h"

#include <stdio.h>                       // fprintf
#include <emmintrin.h>                   // __m128i

#include "stack.h"                       // DynamicStack, FreeStack

#include "../globals.h"                  // g_frameNum
#include "../config.h"                   // g_config.m_memorySize
#include "../datastructures.h"           // HandleAllocT
#include "../tinystl.h"                  // cs::TinyStlAllocator

#include <dm/misc.h>                     // DM_MEGABYTES
#include <dm/datastructures/array.h>     // dm::Array
#include <dm/datastructures/objarray.h>  // dm::ObjArray

#include <bx/thread.h>                   // bx::Mutex
#include <bx/uint32_t.h>                 // bx::uint32_cntlz

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

namespace cs
{
    enum { StaticStorageSize = DM_MEGABYTES(32) };

    #if !CS_USE_INTERNAL_ALLOCATOR
        struct Memory
        {
            void* alloc(size_t _size)
            {
                return ::malloc(_size);
            }

            void* realloc(void* _ptr, size_t _size)
            {
                return ::realloc(_ptr, _size);
            }

            void free(void* _ptr)
            {
                return ::free(_ptr);
            }

            bool contains(void* /*_ptr*/)
            {
                return true;
            }
        };
        static Memory s_memory;
    #else // CS_USE_INTERNAL_ALLOCATOR.
        struct Memory
        {
            Memory()
            {
                #if CS_ALLOC_PRINT_STATS
                m_externalAlloc = 0;
                m_externalFree  = 0;
                m_externalSize  = 0;
                #endif //CS_ALLOC_PRINT_STATS
            }

            ///
            ///  Memory:
            ///
            ///  Begin                    StackPtr LastHeapAlloc                      End
            ///    .__.___________.__________|___________|_____________________________.
            ///    |S ||||Small|||| Stack -> |           |..|  |..| <- Heap |...| |.|..|
            ///    |__|||||||||||||__________|___________|_____________________________|
            ///
            ///  S     - Static storage.
            ///  Small - All small allocations here.
            ///  Stack - Stack grows forward.
            ///  Heap  - Heap grows backward.
            ///

            bool init()
            {
                // Make sure init() is called only once!
                static bool s_initialized = false;
                if (s_initialized)
                {
                    return false;
                }
                s_initialized = true;

                configFromDefaultPaths(g_config);
                const size_t size = DM_MAX(DM_MEGABYTES(512), size_t(g_config.m_memorySize));

                // Alloc.
                m_orig = ::malloc(size);

                CS_PRINT_MEM_STATS("Init: Allocating %llu.%lluMB - (0x%p)", dm::U_UMB(size), m_orig);

                // Align.
                void*  alignedPtr;
                size_t alignedSize;
                dm::alignPtrAndSize(alignedPtr, alignedSize, m_orig, size, CS_NATURAL_ALIGNMENT);

                // Assign.
                m_memory = alignedPtr;
                m_size   = alignedSize;

                // Touch every piece of memory, effectively forcing OS to add all memory pages to the process's address space.
                memset(m_memory, 0, m_size);

                // Init memory regions.
                void* ptr = m_memory;
                ptr = m_staticStorage.init(ptr, StaticStorageSize);
                ptr = m_segregatedLists.init(ptr, SegregatedLists::DataSize);

                void* end = (void*)((uint8_t*)m_memory + m_size);
                m_stackPtr = (uint8_t*)dm::alignPtrNext(ptr, CS_NATURAL_ALIGNMENT);
                m_heapEnd  = (uint8_t*)dm::alignPtrPrev(end, CS_NATURAL_ALIGNMENT);

                m_stack.init(&m_stackPtr, &m_heapEnd);
                m_heap.init(&m_stackPtr, &m_heapEnd);

                return false; // return value is not important.
            }

            void destroy()
            {
                #if CS_ALLOC_PRINT_STATS
                m_staticStorage.printStats();
                m_stack.printStats();
                m_segregatedLists.printStats();
                m_heap.printStats();
                printf("External: alloc/free %u.%u, total %llu.%lluMB\n\n", m_externalAlloc, m_externalFree, dm::U_UMB(m_externalSize));
                #endif //CS_ALLOC_PRINT_STATS

                // Do not call free, let it stay until the very end of execution. OS will clean it up.
                //::free(m_orig)#
            }

            #if CS_ALLOC_PRINT_USAGE
            void printUsage()
            {
                const size_t stackUsage = m_stack.getUsage();
                const size_t stackTotal = m_stack.total();
                const size_t heapUsage  = m_heap.getUsage();
                const size_t heapTotal  = m_heap.total();
                printf("Usage: Stack %llu.%lluMB / %llu.%lluMB - Heap %llu.%lluMB / %llu.%lluMB\n"
                      , dm::U_UMB(stackUsage), dm::U_UMB(stackTotal)
                      , dm::U_UMB(heapUsage),  dm::U_UMB(heapTotal)
                      );
            }
            #endif //CS_ALLOC_PRINT_USAGE

            // Alloc.
            //-----

            void* externalAlloc(size_t _size)
            {
                void* ptr = ::malloc(_size);

                #if CS_ALLOC_PRINT_STATS
                m_externalAlloc++;
                m_externalSize += _size;
                #endif //CS_ALLOC_PRINT_STATS

                CS_PRINT_EXT("EXTERNAL ALLOC: %llu.%lluMB - (0x%p)", dm::U_UMB(_size), ptr);

                return ptr;
            }

            void* alloc(size_t _size)
            {
                void* ptr;

                // Try small alloc.
                if (_size <= SegregatedLists::BiggestSize)
                {
                    ptr = m_segregatedLists.alloc(_size);
                    if (NULL != ptr)
                    {
                        return ptr;
                    }
                }

                // Try heap alloc.
                ptr = m_heap.alloc(_size);
                if (NULL != ptr)
                {
                    return ptr;
                }

                // External alloc.
                ptr = externalAlloc(_size);

                return ptr;
            }

            void* stackAlloc(size_t _size)
            {
                void* ptr = m_stack.alloc(_size);
                if (NULL != ptr)
                {
                    return ptr;
                }

                return this->alloc(_size);
            }

            void* staticAlloc(size_t _size)
            {
                return m_staticStorage.alloc(_size);
            }

            bool contains(void* _ptr)
            {
                return (m_memory <= _ptr && _ptr <= ((uint8_t*)m_memory + m_size));
            }

            // Realloc.
            //-----

            void* realloc(void* _ptr, size_t _size)
            {
                if (NULL == _ptr)
                {
                    return this->alloc(_size);
                }

                // Handle heap allocation.
                const bool fromHeap = m_heap.contains(_ptr);
                if (fromHeap)
                {
                    void* ptr = m_heap.realloc(_ptr, _size);
                    if (NULL != ptr)
                    {
                        return ptr;
                    }
                }

                // Handle external pointer.
                if (!this->contains(_ptr))
                {
                    void* ptr = ::realloc(_ptr, _size);
                    CS_PRINT_EXT("EXTERNAL REALLOC: %llu.%lluMB - (0x%p - 0x%p)", dm::U_UMB(_size), _ptr, ptr);
                    return ptr;
                }

                // Make a new allocation of requested size.
                void* newPtr = this->alloc(_size);
                if (NULL == newPtr)
                {
                    newPtr = externalAlloc(_size);
                    if (NULL == newPtr)
                    {
                        return NULL;
                    }
                }

                // Get size of current allocation.
                size_t currSize = 0;
                if (fromHeap)
                {
                    currSize = m_heap.getSize(_ptr);
                }
                else if (m_segregatedLists.contains(_ptr))
                {
                    currSize = m_segregatedLists.getSize(_ptr);
                }
                else if (fromStackRegion(_ptr))
                {
                    currSize = DynamicStack::readSize(_ptr);
                }

                CS_CHECK(0 != currSize, "Memory::realloc | Error getting allocation size.");

                // Copy memory.
                const size_t minSize = dm::min(currSize, _size);
                memcpy(newPtr, _ptr, minSize);

                // Free original pointer.
                this->free(_ptr);

                return newPtr;
            }

            void* stackRealloc(void* _ptr, size_t _size)
            {
                // Handle stack pointer.
                void* ptr = m_stack.realloc(_ptr, _size);
                if (NULL != ptr)
                {
                    return ptr;
                }

                // Pointer not from stack.
                return this->realloc(_ptr, _size);
            }

            void* staticRealloc(void* _ptr, size_t _size)
            {
                return m_staticStorage.realloc(_ptr, _size);
            }

            // Free.
            //-----

            void free(void* _ptr)
            {
                if (this->contains(_ptr))
                {
                    if (m_segregatedLists.contains(_ptr))
                    {
                        m_segregatedLists.free(_ptr);
                    }
                    else if (m_heap.contains(_ptr))
                    {
                        m_heap.free(_ptr);
                    }
                }
                else // external pointer
                {
                    CS_PRINT_EXT("~EXTERNAL FREE: (0x%p)", _ptr);

                    #if CS_ALLOC_PRINT_STATS
                    m_externalFree++;
                    #endif //CS_ALLOC_PRINT_STATS

                    ::free(_ptr);
                }
            }

            // Stack.
            //-----

            bool fromStackRegion(void* _ptr)
            {
                return (m_stack.begin() <= _ptr && _ptr < m_heapEnd);
            }

            void stackPush()
            {
                m_stack.push();
            }

            void stackPop()
            {
                m_stack.pop();
            }

            uint8_t* stackAdvance(size_t _size)
            {
                m_stackPtr += _size;
                return m_stackPtr;
            }

            // Other.
            //-----

            size_t getSize(void* _ptr)
            {
                if (m_segregatedLists.contains(_ptr))
                {
                    return m_segregatedLists.getSize(_ptr);
                }
                else if (m_heap.contains(_ptr))
                {
                    return m_heap.getSize(_ptr);
                }
                else if (m_stack.contains(_ptr))
                {
                    return m_stack.getSize(_ptr);
                }
                else // external pointer
                {
                    return 0;
                }
            }

            size_t remainingStaticMemory() const
            {
                return m_staticStorage.available();
            }

            size_t sizeBetweenStackAndHeap()
            {
                return m_heap.getRemainingSpace();
            }

            struct StaticStorage
            {
                void* init(void* _mem, size_t _size)
                {
                    void*  alignedPtr;
                    size_t alignedSize;
                    dm::alignPtrAndSize(alignedPtr, alignedSize, _mem, _size, CS_NATURAL_ALIGNMENT);

                    CS_PRINT_MEM_STATS("Init: Using %llu.%lluMB for static storage.", dm::U_UMB(alignedSize));

                    m_ptr   = (uint8_t*)alignedPtr;
                    m_last  = m_ptr;
                    m_avail = alignedSize;

                    #if CS_ALLOC_PRINT_STATS
                    m_size = alignedSize;
                    #endif //CS_ALLOC_PRINT_STATS

                    return (uint8_t*)alignedPtr + alignedSize;
                }

                void* alloc(size_t _size)
                {
                    const size_t size = dm::alignSizeNext(_size, CS_NATURAL_ALIGNMENT);

                    CS_CHECK(size <= m_avail
                            , "StaticStorage::alloc | No more space left. %llu.%lluKB - %llu.%lluKB (requested/left)"
                            , dm::U_UKB(size)
                            , dm::U_UKB(m_avail)
                            );

                    if (size > m_avail)
                    {
                        return NULL;
                    }

                    m_last = m_ptr;

                    m_ptr   += size;
                    m_avail -= size;

                    CS_PRINT_STATIC("Static alloc: %llu.%lluMB, Remaining: %llu.%lluMB - (0x%p)", dm::U_UMB(_size), dm::U_UMB(m_avail), m_last);

                    return m_last;
                }

                void* realloc(void* _ptr, size_t _size)
                {
                    if (NULL == _ptr)
                    {
                        return alloc(_size);
                    }

                    CS_CHECK(_ptr == m_last, "StaticStorage::realloc | Invalid realloc call! Realloc can be called only for the last alloc.");

                    if (_ptr == m_last)
                    {
                        const size_t currSize = m_ptr - m_last;
                        const size_t newSize = dm::alignSizeNext(_size, CS_NATURAL_ALIGNMENT);
                        const int32_t diff = int32_t(newSize - currSize);

                        CS_CHECK(diff <= int32_t(m_avail)
                               , "StaticStorage::realloc | No more space left. %llu.%lluKB - %llu.%lluKB (requested/left)"
                               , dm::U_UKB(diff)
                               , dm::U_UKB(m_avail)
                               );

                        if (diff > int32_t(m_avail))
                        {
                            return NULL;
                        }

                        m_ptr   += diff;
                        m_avail -= diff;

                        CS_PRINT_STATIC("Static realloc: %llu.%lluMB, Remaining: %llu.%lluMB - (0x%p)", dm::U_UMB(newSize), dm::U_UMB(m_avail), m_last);

                        return m_last;
                    }

                    return NULL;
                }

                size_t available() const
                {
                    return m_avail;
                }

                #if CS_ALLOC_PRINT_STATS
                void printStats()
                {
                    const size_t diff = m_size-m_avail;
                    printf("Static storage:\n");
                    printf("\tTotal: %llu.%03lluMB, Used: %llu.%03lluMB, Remaining: %llu.%03lluMB\n\n"
                          , dm::U_UMB(m_size), dm::U_UMB(diff), dm::U_UMB(m_avail));
                }
                #endif //CS_ALLOC_PRINT_STATS

                uint8_t* m_ptr;
                uint8_t* m_last;
                size_t m_avail;

                #if CS_ALLOC_PRINT_STATS
                size_t m_size;
                #endif //CS_ALLOC_PRINT_STATS
            };

            struct SegregatedLists
            {
                #if (CS_OVERRIDE_TINYSTL_ALLOCATOR &&  CS_OBJTOBIN_USES_TINYSTL) \
                ||  (CS_OVERRIDE_NEWDELETE         && !CS_OBJTOBIN_USES_TINYSTL)
                #   define NUM64 512
                #else
                #   define NUM64 128
                #endif

                enum
                {
                    Size0  =                16, Num0  =    16*1024,
                    Size1  =                32, Num1  =    16*1024,
                    Size2  =                64, Num2  = NUM64*1024, // Notice: objToBin() uses over 100k of these for std containers.
                    Size3  =               256, Num3  =    16*1024,
                    Size4  =               512, Num4  =     8*1024,
                    Size5  = DM_KILOBYTES(  1), Num5  =     8*1024,
                    Size6  = DM_KILOBYTES( 16), Num6  =         64,
                    Size7  = DM_KILOBYTES( 64), Num7  =         64,
                    Size8  = DM_KILOBYTES(256), Num8  =         32,
                    Size9  = DM_KILOBYTES(512), Num9  =         32,
                    Size10 =   DM_MEGABYTES(1), Num10 =          8,
                    Size11 =   DM_MEGABYTES(4), Num11 =          8,
                    Size12 =   DM_MEGABYTES(8), Num12 =          8,

                    Count = 13,
                    BiggestSize = Size12,
                    Steps = dm::Log<2,BiggestSize>::Value + 1,

                    DataSize = Size0*Num0
                             + Size1*Num1
                             + Size2*Num2
                             + Size3*Num3
                             + Size4*Num4
                             + Size5*Num5
                             + Size6*Num6
                             + Size7*Num7
                             + Size8*Num8
                             + Size9*Num9
                             + Size10*Num10
                             + Size11*Num11
                             + Size12*Num12,

                    #define CS_SIZE_FOR(_num) ((_num>>6)+1)*sizeof(uint64_t)
                    ListsSize = CS_SIZE_FOR(Num0)
                              + CS_SIZE_FOR(Num1)
                              + CS_SIZE_FOR(Num2)
                              + CS_SIZE_FOR(Num3)
                              + CS_SIZE_FOR(Num4)
                              + CS_SIZE_FOR(Num5)
                              + CS_SIZE_FOR(Num6)
                              + CS_SIZE_FOR(Num7)
                              + CS_SIZE_FOR(Num8)
                              + CS_SIZE_FOR(Num9)
                              + CS_SIZE_FOR(Num10)
                              + CS_SIZE_FOR(Num11)
                              + CS_SIZE_FOR(Num12),
                    #undef CS_SIZE_FOR
                };

                SegregatedLists()
                {
                    #if CS_ALLOC_PRINT_STATS
                    for (uint8_t ii = Count; ii--; )
                    {
                        m_overflow[ii] = 0;
                    }
                    #endif //CS_ALLOC_PRINT_STATS
                }

                void* init(void* _mem, size_t _size)
                {
                    void*  alignedPtr;
                    size_t alignedSize;
                    dm::alignPtrAndSize(alignedPtr, alignedSize, _mem, _size, CS_NATURAL_ALIGNMENT);

                    m_mem = alignedPtr;
                    m_totalSize = alignedSize;
                    CS_CHECK(m_totalSize == DataSize
                           , "SegregatedLists::init | Not enough data allocated %llu.%lluMB / %llu.%lluMB"
                           , dm::U_UMB(m_totalSize), dm::U_UMB(DataSize)
                           );
                    CS_PRINT_MEM_STATS("Init: Using %llu.%lluMB for segregated lists", dm::U_UMB(DataSize));

                    m_sizes[ 0] = Size0;
                    m_sizes[ 1] = Size1;
                    m_sizes[ 2] = Size2;
                    m_sizes[ 3] = Size3;
                    m_sizes[ 4] = Size4;
                    m_sizes[ 5] = Size5;
                    m_sizes[ 6] = Size6;
                    m_sizes[ 7] = Size7;
                    m_sizes[ 8] = Size8;
                    m_sizes[ 9] = Size9;
                    m_sizes[10] = Size10;
                    m_sizes[11] = Size11;
                    m_sizes[12] = Size12;
                    CS_CHECK(m_sizes[Count-1] == BiggestSize, "Error! 'BiggestSize' is not well defined");

                    for (uint8_t idx = 0, ii = 0; ii < Steps; ++ii)
                    {
                        const uint32_t currSize = 1<<ii;
                        if (currSize > m_sizes[idx])
                        {
                            ++idx;
                            CS_CHECK(idx < Count, "Error! 'Steps' is not well defined.");
                        }

                        m_powToIdx[ii] = idx;
                    }

                    void* ptr = m_allocsData;
                    ptr = m_allocs[ 0].init(Num0,  ptr);
                    ptr = m_allocs[ 1].init(Num1,  ptr);
                    ptr = m_allocs[ 2].init(Num2,  ptr);
                    ptr = m_allocs[ 3].init(Num3,  ptr);
                    ptr = m_allocs[ 4].init(Num4,  ptr);
                    ptr = m_allocs[ 5].init(Num5,  ptr);
                    ptr = m_allocs[ 6].init(Num6,  ptr);
                    ptr = m_allocs[ 7].init(Num7,  ptr);
                    ptr = m_allocs[ 8].init(Num8,  ptr);
                    ptr = m_allocs[ 9].init(Num9,  ptr);
                    ptr = m_allocs[10].init(Num10, ptr);
                    ptr = m_allocs[11].init(Num11, ptr);
                    ptr = m_allocs[12].init(Num12, ptr);

                    m_begin[0] = (uint8_t*)m_mem;
                    for (uint8_t ii = 1; ii < Count; ++ii)
                    {
                        const uint8_t prev = ii-1;
                        m_begin[ii] = (uint8_t*)m_begin[prev] + m_sizes[prev]*m_allocs[prev].max();
                    }

                    return (uint8_t*)alignedPtr + alignedSize;
                }

                void* alloc(size_t _size)
                {
                    CS_CHECK(_size <= BiggestSize, "Requested size is bigger than the largest supported size!");

                    // Get list index.
                    const uint32_t sizePow2 = bx::uint32_nextpow2(uint32_t(_size));
                    const uint32_t pow = bx::uint32_cnttz(sizePow2);
                    CS_CHECK(pow < Steps, "Error! Sizes are probably not well defined.");
                    const uint8_t idx = m_powToIdx[pow];

                    // Allocate if there is an empty slot.
                    m_mutex.lock();
                    const uint32_t slot = m_allocs[idx].setAny();
                    m_mutex.unlock();
                    if (slot != m_allocs[idx].max())
                    {
                        uint8_t* mem = (uint8_t*)m_begin[idx] + slot*m_sizes[idx];

                        CS_CHECK(mem < (uint8_t*)m_mem + m_totalSize, "SegregatedLists::alloc | Allocating outside of bounds!");

                        CS_PRINT_SMALL("Small alloc: %llu.%lluKB -> slot %u (%llu.%lluKB) - %u/%u - (0x%p)"
                                      , dm::U_UKB(_size)
                                      , slot
                                      , dm::U_UKB(m_sizes[idx])
                                      , m_allocs[idx].count(), m_allocs[idx].max()
                                      , mem
                                      );

                        #if CS_ALLOC_PRINT_STATS
                        m_mutex.lock();
                        m_totalUsed[idx]++;
                        m_mutex.unlock();
                        #endif //CS_ALLOC_PRINT_STATS

                        return mem;
                    }
                    else
                    {
                        CS_PRINT_SMALL("Small alloc: All small lists of %uB are full. Requested %zuB.", m_sizes[idx], _size);

                        #if CS_ALLOC_PRINT_STATS
                        m_mutex.lock();
                        m_overflow[idx]++;
                        m_mutex.unlock();
                        #endif //CS_ALLOC_PRINT_STATS

                        return NULL;
                    }
                }

                void free(void* _ptr)
                {
                    for (uint8_t ii = Count; ii--; )
                    {
                        if (_ptr >= m_begin[ii])
                        {
                            const size_t   dist = (uint8_t*)_ptr - (uint8_t*)m_begin[ii];
                            const uint32_t slot = uint32_t(dist/m_sizes[ii]);
                            m_mutex.lock();
                            m_allocs[ii].unset(slot);
                            m_mutex.unlock();

                            CS_PRINT_SMALL("~Small free: slot %u %llu.%lluKB %d/%d - (0x%p)"
                                          , slot
                                          , dm::U_UKB(m_sizes[ii])
                                          , m_allocs[ii].count(), m_allocs[ii].max()
                                          , _ptr
                                          );

                            return;
                        }
                    }
                }

                size_t getSize(void* _ptr) const
                {
                    for (uint8_t ii = Count; ii--; )
                    {
                        if (_ptr >= m_begin[ii])
                        {
                            return m_sizes[ii];
                        }
                    }

                    return 0;
                }

                bool contains(void* _ptr) const
                {
                    return (m_mem <= _ptr && _ptr < ((uint8_t*)m_mem + m_totalSize));
                }

                #if CS_ALLOC_PRINT_STATS
                void printStats()
                {
                    printf("Small allocations:\n");

                    uint32_t totalSize = 0;
                    for (uint8_t ii = 0; ii < Count; ++ii)
                    {
                        const uint32_t used = m_allocs[ii].count();
                        const uint32_t max  = m_allocs[ii].max();
                        totalSize += m_sizes[ii]*used;
                        printf("\t#%2d: Size: %5llu.%03lluKB, Used: %3d / %5d, Overflow: %d, Total: %d\n"
                              , ii, dm::U_UKB(m_sizes[ii]), used, max, m_overflow[ii], m_totalUsed[ii]);
                    }
                    printf("\t-------------------------\n");
                    printf("\tTotal: %llu.%lluMB / %llu.%lluMB\n\n", dm::U_UMB(totalSize), dm::U_UMB(DataSize));
                }
                #endif //CS_ALLOC_PRINT_STATS

            private:
                void*       m_mem;
                size_t      m_totalSize;
                bx::LwMutex m_mutex;
                // Lists:
                uint32_t     m_sizes[Count];
                void*        m_begin[Count];
                uint8_t      m_powToIdx[Steps];
                dm::BitArray m_allocs[Count];
                uint8_t      m_allocsData[ListsSize];

                #if CS_ALLOC_PRINT_STATS
                uint32_t m_totalUsed[Count];
                uint32_t m_overflow[Count];
                #endif //CS_ALLOC_PRINT_STATS
            };

            struct Heap
            {
                enum
                {
                    HeaderSize = sizeof(uint64_t),
                    FooterSize = sizeof(uint64_t),
                    HeaderFooterSize = HeaderSize+FooterSize,

                    MinimalSlotSize = HeaderFooterSize + 64,

                    SmallestRegion    = DM_MEGABYTES(2),
                    SmallestRegionPwr = dm::Log<2,(SmallestRegion>>20ul)>::Value,

                    NumSlots0 = 2048, // for region:    2MB
                    NumSlots1 = 2048, // for region:    4MB
                    NumSlots2 = 1024, // for region:    8MB
                    NumSlots3 = 1024, // for region:   16MB
                    NumSlots4 = 1024, // for region:   32MB
                    NumSlots5 = 512,  // for region:   64MB
                    NumSlots6 = 256,  // for region:  128MB
                    NumSlots7 = 128,  // for region:  256MB
                    NumSlots8 = 64,   // for region:  512MB
                    NumSlots9 = 32,   // for region: 1024MB

                    BiggestRegion    = DM_MEGABYTES(1024),
                    BiggestRegionPwr = dm::Log<2,(BiggestRegion>>20ul)>::Value,

                    MaxBigFreeSlots = 32,

                    NumRegions    = 10, /* == BiggestRegionPwr - SmallestRegionPwr*/
                    NumSubRegions = 16,

                    #if !CS_ALLOCAOTR_IMPL
                        TotalSlotCount = (NumSlots0
                                       +  NumSlots1
                                       +  NumSlots2
                                       +  NumSlots3
                                       +  NumSlots4
                                       +  NumSlots5
                                       +  NumSlots6
                                       +  NumSlots7
                                       +  NumSlots8
                                       +  NumSlots9) * NumSubRegions,
                    #endif //!CS_ALLOCAOTR_IMPL
                };

                void init(uint8_t** _stackPtr, uint8_t** _heap)
                {
                    m_begin    = *_heap;
                    m_end      = _heap;
                    m_stackPtr = _stackPtr;

                    *m_end -= 2*sizeof(uint64_t);
                    uint64_t* terminator = (uint64_t*)*m_end;
                    terminator[0] = UINT64_MAX;
                    terminator[1] = UINT64_MAX;

                    m_bigFreeSlotsCount = 0;
                    memset(m_bigFreeSlotsSize, 0, sizeof(m_bigFreeSlotsSize));
                    memset(m_bigFreeSlotsPtr,  0, sizeof(m_bigFreeSlotsPtr));

                    memset(m_regionInfo, 0xff, sizeof(m_regionInfo));

                    #if CS_ALLOCAOTR_IMPL
                        m_freeSlotsMax[0] = NumSlots0;
                        m_freeSlotsMax[1] = NumSlots1;
                        m_freeSlotsMax[2] = NumSlots2;
                        m_freeSlotsMax[3] = NumSlots3;
                        m_freeSlotsMax[4] = NumSlots4;
                        m_freeSlotsMax[5] = NumSlots5;
                        m_freeSlotsMax[6] = NumSlots6;
                        m_freeSlotsMax[7] = NumSlots7;
                        m_freeSlotsMax[8] = NumSlots8;
                        m_freeSlotsMax[9] = NumSlots9;

                        memset(m_freeSlotsCount, 0, sizeof(m_freeSlotsCount));

                        memset(m_freeSlotsSize,  0, sizeof(m_freeSlotsSize));
                        memset(m_freeSlotsSize0, 0, sizeof(m_freeSlotsSize0));
                        memset(m_freeSlotsSize1, 0, sizeof(m_freeSlotsSize1));
                        memset(m_freeSlotsSize2, 0, sizeof(m_freeSlotsSize2));
                        memset(m_freeSlotsSize3, 0, sizeof(m_freeSlotsSize3));
                        memset(m_freeSlotsSize4, 0, sizeof(m_freeSlotsSize4));
                        memset(m_freeSlotsSize5, 0, sizeof(m_freeSlotsSize5));
                        memset(m_freeSlotsSize6, 0, sizeof(m_freeSlotsSize6));
                        memset(m_freeSlotsSize7, 0, sizeof(m_freeSlotsSize7));
                        memset(m_freeSlotsSize8, 0, sizeof(m_freeSlotsSize8));
                        memset(m_freeSlotsSize9, 0, sizeof(m_freeSlotsSize9));
                        for (uint32_t ii = 0; ii < NumSubRegions; ++ii) { m_freeSlotsSize[0*NumSubRegions+ii] = m_freeSlotsSize0[ii]; }
                        for (uint32_t ii = 0; ii < NumSubRegions; ++ii) { m_freeSlotsSize[1*NumSubRegions+ii] = m_freeSlotsSize1[ii]; }
                        for (uint32_t ii = 0; ii < NumSubRegions; ++ii) { m_freeSlotsSize[2*NumSubRegions+ii] = m_freeSlotsSize2[ii]; }
                        for (uint32_t ii = 0; ii < NumSubRegions; ++ii) { m_freeSlotsSize[3*NumSubRegions+ii] = m_freeSlotsSize3[ii]; }
                        for (uint32_t ii = 0; ii < NumSubRegions; ++ii) { m_freeSlotsSize[4*NumSubRegions+ii] = m_freeSlotsSize4[ii]; }
                        for (uint32_t ii = 0; ii < NumSubRegions; ++ii) { m_freeSlotsSize[5*NumSubRegions+ii] = m_freeSlotsSize5[ii]; }
                        for (uint32_t ii = 0; ii < NumSubRegions; ++ii) { m_freeSlotsSize[6*NumSubRegions+ii] = m_freeSlotsSize6[ii]; }
                        for (uint32_t ii = 0; ii < NumSubRegions; ++ii) { m_freeSlotsSize[7*NumSubRegions+ii] = m_freeSlotsSize7[ii]; }
                        for (uint32_t ii = 0; ii < NumSubRegions; ++ii) { m_freeSlotsSize[8*NumSubRegions+ii] = m_freeSlotsSize8[ii]; }
                        for (uint32_t ii = 0; ii < NumSubRegions; ++ii) { m_freeSlotsSize[9*NumSubRegions+ii] = m_freeSlotsSize9[ii]; }

                        memset(m_freeSlotsPtr,  0, sizeof(m_freeSlotsPtr));
                        memset(m_freeSlotsPtr0, 0, sizeof(m_freeSlotsPtr0));
                        memset(m_freeSlotsPtr1, 0, sizeof(m_freeSlotsPtr1));
                        memset(m_freeSlotsPtr2, 0, sizeof(m_freeSlotsPtr2));
                        memset(m_freeSlotsPtr3, 0, sizeof(m_freeSlotsPtr3));
                        memset(m_freeSlotsPtr4, 0, sizeof(m_freeSlotsPtr4));
                        memset(m_freeSlotsPtr5, 0, sizeof(m_freeSlotsPtr5));
                        memset(m_freeSlotsPtr6, 0, sizeof(m_freeSlotsPtr6));
                        memset(m_freeSlotsPtr7, 0, sizeof(m_freeSlotsPtr7));
                        memset(m_freeSlotsPtr8, 0, sizeof(m_freeSlotsPtr8));
                        memset(m_freeSlotsPtr9, 0, sizeof(m_freeSlotsPtr9));
                        for (uint32_t ii = 0; ii < NumSubRegions; ++ii) { m_freeSlotsPtr[0*NumSubRegions+ii] = m_freeSlotsPtr0[ii]; }
                        for (uint32_t ii = 0; ii < NumSubRegions; ++ii) { m_freeSlotsPtr[1*NumSubRegions+ii] = m_freeSlotsPtr1[ii]; }
                        for (uint32_t ii = 0; ii < NumSubRegions; ++ii) { m_freeSlotsPtr[2*NumSubRegions+ii] = m_freeSlotsPtr2[ii]; }
                        for (uint32_t ii = 0; ii < NumSubRegions; ++ii) { m_freeSlotsPtr[3*NumSubRegions+ii] = m_freeSlotsPtr3[ii]; }
                        for (uint32_t ii = 0; ii < NumSubRegions; ++ii) { m_freeSlotsPtr[4*NumSubRegions+ii] = m_freeSlotsPtr4[ii]; }
                        for (uint32_t ii = 0; ii < NumSubRegions; ++ii) { m_freeSlotsPtr[5*NumSubRegions+ii] = m_freeSlotsPtr5[ii]; }
                        for (uint32_t ii = 0; ii < NumSubRegions; ++ii) { m_freeSlotsPtr[6*NumSubRegions+ii] = m_freeSlotsPtr6[ii]; }
                        for (uint32_t ii = 0; ii < NumSubRegions; ++ii) { m_freeSlotsPtr[7*NumSubRegions+ii] = m_freeSlotsPtr7[ii]; }
                        for (uint32_t ii = 0; ii < NumSubRegions; ++ii) { m_freeSlotsPtr[8*NumSubRegions+ii] = m_freeSlotsPtr8[ii]; }
                        for (uint32_t ii = 0; ii < NumSubRegions; ++ii) { m_freeSlotsPtr[9*NumSubRegions+ii] = m_freeSlotsPtr9[ii]; }
                    #else
                        void* ptr = (void*)m_freeSlotsData;
                        for (uint32_t ii = 0; ii < NumSubRegions; ++ii) { ptr = m_freeSlots[0*NumSubRegions+ii].init(NumSlots0, ptr); }
                        for (uint32_t ii = 0; ii < NumSubRegions; ++ii) { ptr = m_freeSlots[1*NumSubRegions+ii].init(NumSlots1, ptr); }
                        for (uint32_t ii = 0; ii < NumSubRegions; ++ii) { ptr = m_freeSlots[2*NumSubRegions+ii].init(NumSlots2, ptr); }
                        for (uint32_t ii = 0; ii < NumSubRegions; ++ii) { ptr = m_freeSlots[3*NumSubRegions+ii].init(NumSlots3, ptr); }
                        for (uint32_t ii = 0; ii < NumSubRegions; ++ii) { ptr = m_freeSlots[4*NumSubRegions+ii].init(NumSlots4, ptr); }
                        for (uint32_t ii = 0; ii < NumSubRegions; ++ii) { ptr = m_freeSlots[5*NumSubRegions+ii].init(NumSlots5, ptr); }
                        for (uint32_t ii = 0; ii < NumSubRegions; ++ii) { ptr = m_freeSlots[6*NumSubRegions+ii].init(NumSlots6, ptr); }
                        for (uint32_t ii = 0; ii < NumSubRegions; ++ii) { ptr = m_freeSlots[7*NumSubRegions+ii].init(NumSlots7, ptr); }
                        for (uint32_t ii = 0; ii < NumSubRegions; ++ii) { ptr = m_freeSlots[8*NumSubRegions+ii].init(NumSlots8, ptr); }
                        for (uint32_t ii = 0; ii < NumSubRegions; ++ii) { ptr = m_freeSlots[9*NumSubRegions+ii].init(NumSlots9, ptr); }
                    #endif //CS_ALLOCAOTR_IMPL
                }

                uint16_t getRegion(uint16_t _slotGroup)
                {
                    return _slotGroup/NumSubRegions;
                }

                uint16_t nextSlotGroup(uint16_t _slotGroup)
                {
                    const uint16_t region = getRegion(_slotGroup);

                    const uint16_t incr = _slotGroup+1;
                    const uint16_t end  = region*NumSubRegions + NumSubRegions;
                    if (incr < end)
                    {
                        return incr;
                    }
                    else
                    {
                        const uint16_t nextRegion = m_regionInfo[region].m_next;
                        if (UINT16_MAX == nextRegion)
                        {
                            return UINT16_MAX;
                        }
                        else
                        {
                            const uint16_t first = m_regionInfo[nextRegion].m_first;
                            if (UINT16_MAX == first)
                            {
                                return UINT16_MAX;
                            }

                            return nextRegion*NumSubRegions + first;
                        }
                    }
                }

                void registerSlotGroup(uint16_t _slotGroup)
                {
                    const uint16_t region = getRegion(_slotGroup);
                    for (uint16_t ii = region; ii--; )
                    {
                        if (m_regionInfo[ii].m_next > region)
                        {
                            m_regionInfo[ii].m_next = region;
                        }
                        else
                        {
                            break;
                        }
                    }

                    const uint16_t subRegion = _slotGroup - region*NumSubRegions;
                    if (m_regionInfo[region].m_first > subRegion)
                    {
                        m_regionInfo[region].m_first = subRegion;
                    }
                }

                void unregisterSlotGroup(uint16_t _slotGroup)
                {
                    const uint16_t region    = getRegion(_slotGroup);
                    const uint16_t subRegion = _slotGroup - region*NumSubRegions;

                    uint16_t first = m_regionInfo[region].m_first;
                    if (subRegion == first)
                    {
                        // Find and assign first.
                        for (uint16_t ii = subRegion+1; ii < NumSubRegions; ++ii)
                        {
                            if (m_freeSlotsCount[ii] > 0)
                            {
                                first = ii;
                                break;
                            }
                        }

                        if (subRegion == first)
                        {
                            // Region is empty.
                            m_regionInfo[region].m_first = UINT16_MAX;

                            // Reassign pointers to point to the next one.
                            const uint16_t nextRegion = m_regionInfo[region].m_next;
                            for (uint16_t ii = 0; ii < region; ++ii)
                            {
                                if (m_regionInfo[ii].m_next == region)
                                {
                                    m_regionInfo[ii].m_next = nextRegion;
                                }
                                else
                                {
                                    break;
                                }
                            }
                        }
                        else
                        {
                            m_regionInfo[region].m_first = first;
                        }
                    }
                }

                uint16_t getSlotGroup(uint32_t _size)
                {
                    const uint32_t sizePwrTwo   = dm::nextPowTwo(_size);
                    const uint32_t sizePwrTwoMB = sizePwrTwo >> 20;

                    if (0 == sizePwrTwoMB)
                    {
                        return 0;
                    }

                    // Determine region.
                    const uint32_t region = dm::max(0, int32_t(bx::uint32_cnttz(sizePwrTwoMB)) - SmallestRegionPwr);

                    // Determine subRegion.
                    uint32_t subRegion = 0;
                    const uint32_t leftover = sizePwrTwo - _size;
                    if (0 != leftover)
                    {
                        const uint32_t subRegionSize = (sizePwrTwo - (sizePwrTwo>>1))/NumSubRegions;
                        subRegion = leftover/subRegionSize;
                    }

                    // Determine slot group.
                    const uint32_t slotGroup = region*NumSubRegions+subRegion;

                    CS_CHECK(slotGroup < NumRegions*NumSubRegions, "Invalid slot group %d/%d [slotGroup][max].", slotGroup, NumRegions*NumSubRegions);

                    return uint16_t(slotGroup);
                }

                void addFreeSpace(void* _ptr, uint32_t _size)
                {
                    DM_CHECK(_size >= HeaderFooterSize, "Error _size param is invalid.");

                    const uint16_t group = getSlotGroup(_size);

                    #if CS_ALLOCAOTR_IMPL
                        const uint16_t count = m_freeSlotsCount[group];
                        const uint16_t max   = m_freeSlotsMax[group/NumRegions];
                        //TODO: Print warning if count >= max.
                        if (max > count)
                        {
                            registerSlotGroup(group);

                            const uint32_t last = m_freeSlotsCount[group]++;
                            m_freeSlotsSize[group][last] = _size;
                            m_freeSlotsPtr [group][last] = _ptr;
                        }
                        writeHeaderFooter(_ptr, (uint64_t)_size, false);
                    #else
                        //TODO: Print warning if count >= max.
                        if (m_freeSlots[group].count() < m_freeSlots[group].max())
                        {
                            registerSlotGroup(group);

                            FreeSlot* freeSlot = m_freeSlots[group].addNew();
                            freeSlot->m_size = _size;
                            freeSlot->m_ptr  = _ptr;
                            const uint16_t handle = m_freeSlots[group].getHandleOf(freeSlot);
                            writeHeaderFooter(_ptr, (uint64_t)_size, group, handle);
                        }
                        else
                        {
                            const uint16_t handle = UINT16_MAX;
                            writeHeaderFooter(_ptr, (uint64_t)_size, group, handle);
                        }
                    #endif //CS_ALLOCAOTR_IMPL
                }

                void addBigFreeSpace(void* _ptr, uint64_t _size)
                {
                    CS_CHECK(m_bigFreeSlotsCount < 32
                           , "There are not enough slots big allocations. %d/%d", m_bigFreeSlotsCount, 32);

                    const uint32_t last = m_bigFreeSlotsCount++;
                    m_bigFreeSlotsSize[last] = _size;
                    m_bigFreeSlotsPtr [last] = _ptr;
                    writeHeaderFooter(_ptr, _size, false);
                }

                void addSpace(void* _ptr, uint64_t _size)
                {
                    if (_size <= BiggestRegion)
                    {
                        addFreeSpace(_ptr, uint32_t(_size));
                    }
                    else
                    {
                        bx::debugBreak();
                        addBigFreeSpace(_ptr, _size);
                    }
                }

                void removeFreeSlot(uint32_t _group, uint32_t _idx)
                {
                    #if CS_ALLOCAOTR_IMPL
                        const uint32_t last = --m_freeSlotsCount[_group];
                        m_freeSlotsSize[_group][_idx] = m_freeSlotsSize[_group][last];
                        m_freeSlotsPtr [_group][_idx] = m_freeSlotsPtr [_group][last];
                    #else
                        m_freeSlots[_group].remove(_idx);
                    #endif //CS_ALLOCAOTR_IMPL

                    unregisterSlotGroup(uint16_t(_group));
                }

                void removeBigFreeSlot(uint32_t _idx)
                {
                    const uint32_t last = --m_bigFreeSlotsCount;
                    m_bigFreeSlotsSize[_idx] = m_bigFreeSlotsSize[last];
                    m_bigFreeSlotsPtr [_idx] = m_bigFreeSlotsPtr [last];
                }

                #if CS_ALLOCAOTR_IMPL
                    bool removeFreeSpaceRef(void* _ptr, uint32_t _size)
                    {
                        uint16_t group = getSlotGroup(_size);
                        do
                        {
                            for (uint32_t ii = 0, end = m_freeSlotsCount[group]; ii < end; ++ii)
                            {
                                if (m_freeSlotsPtr[group][ii] == _ptr)
                                {
                                    removeFreeSlot(group, ii);

                                    return true;
                                }
                            }

                        } while (UINT16_MAX != (group = nextSlotGroup(group)));

                        return false;
                    }

                    bool removeFreeSpaceSSE(void* _ptr, uint32_t _size)
                    {
                        const __m128i ptrsplat = _mm_set1_epi64x(int64_t(_ptr));

                        uint16_t group = getSlotGroup(_size);
                        do
                        {
                            int32_t ii = 0;
                            const int32_t count = (int32_t)m_freeSlotsCount[group];
                            for (int32_t end = count-1; ii < end; ii+=2)
                            {
                                const __m128i  ptrs = _mm_load_si128((__m128i*)&m_freeSlotsPtr[group][ii]);
                                const __m128i  cmp  = _mm_cmpeq_epi64(ptrsplat, ptrs);
                                const uint32_t mask = _mm_movemask_epi8(cmp);
                                if (mask != 0)
                                {
                                    const uint32_t index = ii + (0xff00 == mask); // 'mask' can be either 0xff (0) or 0xff00 (1).
                                    removeFreeSlot(group, index);

                                    return true;
                                }
                            }
                            for (int32_t end = count; ii < end; ++ii)
                            {
                                if (m_freeSlotsPtr[group][ii] == _ptr)
                                {
                                    removeFreeSlot(group, ii);

                                    return true;
                                }
                            }

                        } while (UINT16_MAX != (group = nextSlotGroup(group)));

                        return false;
                    }

                    bool removeFreeSpace(void* _ptr, uint32_t _size)
                    {
                        return removeFreeSpaceSSE(_ptr, _size);
                    }
                #else
                    bool removeFreeSpace(void* _ptr, uint32_t _size, uint16_t _group, uint16_t _handle)
                    {
                        if (_handle != UINT16_MAX)
                        {
                            m_freeSlots[_group].remove(_handle);
                            unregisterSlotGroup(uint16_t(_group));
                            return true;
                        }

                        return false;
                    }
                #endif //CS_ALLOCAOTR_IMPL


                bool removeBigFreeSpaceRef(void* _ptr)
                {
                    for (uint32_t ii = 0, end = m_bigFreeSlotsCount; ii < end; ++ii)
                    {
                        if (m_bigFreeSlotsPtr[ii] == _ptr)
                        {
                            removeBigFreeSlot(ii);

                            return true;
                        }
                    }

                    return false;
                }

                bool removeBigFreeSpaceSSE(void* _ptr)
                {
                    const __m128i ptrsplat = _mm_set1_epi64x(int64_t(_ptr));

                    uint32_t ii = 0;
                    const uint32_t count = m_bigFreeSlotsCount;
                    for (uint32_t end = count-1; ii <= end; ii+=2)
                    {
                        const __m128i  ptrs = _mm_load_si128((__m128i*)&m_bigFreeSlotsPtr[ii]);
                        const __m128i  cmp  = _mm_cmpeq_epi64(ptrsplat, ptrs);
                        const uint32_t mask = _mm_movemask_epi8(cmp);
                        if (mask != 0)
                        {
                            const uint32_t index = ii + (0xff00 == mask); // 'mask' can be either 0xff (0) or 0xff00 (1).
                            removeBigFreeSlot(index);

                            return true;
                        }
                    }
                    for (uint32_t end = count; ii < end; ++ii)
                    {
                        if (m_bigFreeSlotsPtr[ii] == _ptr)
                        {
                            removeBigFreeSlot(ii);

                            return true;
                        }
                    }

                    return false;
                }

                bool removeBigFreeSpace(void* _ptr)
                {
                    return removeBigFreeSpaceSSE(_ptr);
                }

                #if CS_ALLOCAOTR_IMPL
                #   define DM_UsedMask  0x8000000000000000UL
                #   define DM_UsedShift 63UL
                #   define DM_SizeMask  0x7fffffffffffffffUL
                #   define DM_SizeShift 0UL
                #else
                #   define DM_UsedMask    0x8000000000000000UL
                #   define DM_UsedShift   63UL
                #   define DM_HandleMask  0x7fff000000000000UL
                #   define DM_HandleShift 48UL
                #   define DM_GroupMask   0x0000fff000000000UL
                #   define DM_GroupShift  36UL
                #   define DM_SizeMask    0x0000000fffffffffUL
                #   define DM_SizeShift   0UL
                #endif //CS_ALLOCAOTR_IMPL

                uint64_t packHeader(bool _used, uint64_t _size) const
                {
                    return ((uint64_t(_size)<<DM_SizeShift)&DM_SizeMask)
                         | ((uint64_t(_used)<<DM_UsedShift)&DM_UsedMask)
                         ;
                }

                #if !CS_ALLOCAOTR_IMPL
                    uint64_t packHeader(bool _used, uint16_t _group, uint16_t _handle, uint64_t _size) const
                    {
                        return ((uint64_t(_size)<<DM_SizeShift)&DM_SizeMask)
                             | ((uint64_t(_handle)<<DM_HandleShift)&DM_HandleMask)
                             | ((uint64_t(_group)<<DM_GroupShift)&DM_GroupMask)
                             | ((uint64_t(_used)<<DM_UsedShift)&DM_UsedMask)
                             ;
                    }
                #endif //CS_ALLOCAOTR_IMPL

                bool unpackUsed(uint64_t _header) const
                {
                    return DM_BOOL((_header&DM_UsedMask)>>DM_UsedShift);
                }

                #if !CS_ALLOCAOTR_IMPL
                    uint16_t unpackHandle(uint64_t _header) const
                    {
                        return uint16_t((_header&DM_HandleMask)>>DM_HandleShift);
                    }

                    uint16_t unpackGroup(uint64_t _header) const
                    {
                        return uint16_t((_header&DM_GroupMask)>>DM_GroupShift);
                    }
                #endif //CS_ALLOCAOTR_IMPL

                uint64_t unpackSize(uint64_t _header) const
                {
                    return uint64_t((_header&DM_SizeMask)>>DM_SizeShift);
                }

                void* writeHeaderFooter(const void* _begin, uint64_t _totalSize, bool _used = true)
                {
                    uint8_t* end = (uint8_t*)_begin+_totalSize;

                    uint64_t* header = (uint64_t*)_begin;
                    void*     data   = (uint64_t*)_begin+1;
                    uint64_t* footer = (uint64_t*)end-1;

                    const uint64_t allocSize = _totalSize - HeaderFooterSize;
                    const uint64_t headerData = packHeader(_used, allocSize);
                    *header = headerData;
                    *footer = headerData;

                    return data;
                }

                #if !CS_ALLOCAOTR_IMPL
                    void* writeHeaderFooter(const void* _begin, uint64_t _totalSize, uint16_t _group, uint16_t _handle)
                    {
                        uint8_t* end = (uint8_t*)_begin+_totalSize;

                        uint64_t* header = (uint64_t*)_begin;
                        void*     data   = (uint64_t*)_begin+1;
                        uint64_t* footer = (uint64_t*)end-1;

                        const uint64_t allocSize = _totalSize - HeaderFooterSize;
                        const uint64_t headerData = packHeader(true, _group, _handle, allocSize);
                        *header = headerData;
                        *footer = headerData;

                        return data;
                    }
                #endif //CS_ALLOCAOTR_IMPL

                bool isFree(uint64_t _header)
                {
                    return (UINT64_MAX != _header && !unpackUsed(_header));
                }

                void* ptrToBegin(void* _ptr) const
                {
                    const uint64_t* begin = (uint64_t*)_ptr-1;
                    return (void*)begin;
                }

                uint64_t readUsedSize(const void* _beg) const
                {
                    const uint64_t* usedSize = (const uint64_t*)_beg;
                    return *usedSize;
                }

                uint64_t readLeftUsedSize(const void* _beg) const
                {
                    const uint64_t* usedSize = (const uint64_t*)_beg-1;
                    return *usedSize;
                }

                void* consumeFreeSpace(uint32_t _group, uint32_t _idx, uint32_t _slotSize, uint32_t _consume)
                {
                    #if CS_ALLOCAOTR_IMPL
                        void* beg = m_freeSlotsPtr[_group][_idx];
                    #else
                        void* beg = m_freeSlots[_group][_idx].m_ptr;
                    #endif //CS_ALLOCAOTR_IMPL
                    void* ptr;

                    const uint32_t remainingSize = _slotSize - _consume;
                    if (remainingSize > MinimalSlotSize)
                    {
                        // Consume.
                        ptr = writeHeaderFooter(beg, uint64_t(_consume));
                        removeFreeSlot(_group, _idx);

                        // Leftover.
                        void* next = (uint8_t*)beg + _consume;
                        addFreeSpace(next, remainingSize);
                    }
                    else
                    {
                        // Consume entire slot.
                        ptr = writeHeaderFooter(beg, uint64_t(_slotSize));
                        removeFreeSlot(_group, _idx);
                    }

                    return ptr;
                }

                void* consumeBigFreeSpace(uint32_t _idx, uint64_t _consume)
                {
                    // Consume.
                    void* beg = m_bigFreeSlotsPtr[_idx];
                    void* ptr = writeHeaderFooter(beg, _consume);

                    // Leftover.
                    void* next = (uint8_t*)beg + _consume;
                    const uint64_t remainingSize = m_bigFreeSlotsSize[_idx] - _consume;

                    if (remainingSize <= BiggestRegion)
                    {
                        removeBigFreeSlot(_idx);
                        addBigFreeSpace(next, remainingSize);
                    }
                    else
                    {
                        m_bigFreeSlotsSize[_idx] = remainingSize;
                        m_bigFreeSlotsPtr [_idx] = next;
                    }

                    return ptr;
                }

                void* expandHeap(uint64_t _size)
                {
                    *m_end -= _size;
                    uint64_t* terminator = (uint64_t*)*m_end;
                    *terminator = UINT64_MAX;

                    uint8_t* beg = *m_end+sizeof(uint64_t);
                    void* ptr = writeHeaderFooter(beg, _size);

                    return ptr;
                }

                void* alloc(size_t _size)
                {
                    bx::LwMutexScope lock(m_mutex);

                    const size_t alignedSize = dm::alignSizeNext(_size, CS_NATURAL_ALIGNMENT);
                    const size_t totalSize   = alignedSize + HeaderFooterSize;

                    // Search for free space.
                    if (totalSize <= BiggestRegion)
                    {
                        uint16_t group = getSlotGroup(uint32_t(totalSize));
                        do
                        {
                            #if CS_ALLOCAOTR_IMPL
                                const uint16_t count = m_freeSlotsCount[group];
                                const __m128i totalSizeSplat = _mm_set1_epi32(uint32_t(totalSize));

                                uint16_t ii = 0;
                                for (uint16_t end = ((count>>2)<<2); ii < end; ii+=4)
                                {
                                    const __m128i  sizes = _mm_load_si128((__m128i*)&m_freeSlotsSize[group][ii]);
                                    const __m128i  cmp   = _mm_cmpgt_epi32(sizes, totalSizeSplat);
                                    const uint32_t mask  = _mm_movemask_epi8(cmp);
                                    if (mask != 0)
                                    {
                                        const int32_t idx = ii + (bx::uint32_cnttz(mask)/4);
                                        const uint32_t slotSize = m_freeSlotsSize[group][idx];
                                        void* ptr = consumeFreeSpace(group, idx, uint32_t(slotSize), uint32_t(totalSize));

                                        return ptr;
                                    }
                                }

                                for (uint16_t end = count; ii < end; ++ii)
                                {
                                    const uint32_t slotSize = m_freeSlotsSize[group][ii];
                                    if (slotSize >= uint32_t(totalSize))
                                    {
                                        void* ptr = consumeFreeSpace(group, ii, uint32_t(slotSize), uint32_t(totalSize));

                                        return ptr;
                                    }
                                }
                            #else
                                FreeSlotList& freeSlotList = m_freeSlots[group];
                                for (uint16_t ii = 0, end = freeSlotList.count(); ii < end; ++ii)
                                {
                                    FreeSlot& slot = freeSlotList[ii];
                                    if (slot.m_size > uint32_t(totalSize))
                                    {
                                        void* ptr = consumeFreeSpace(group, ii, slot.m_size, uint32_t(totalSize));
                                        return NULL;
                                    }

                                }
                            #endif //CS_ALLOCAOTR_IMPL

                        } while (UINT16_MAX != (group = nextSlotGroup(group)));
                    }

                    // Search for big space.
                    for (uint8_t ii = m_bigFreeSlotsCount; ii--; )
                    {
                        if (m_bigFreeSlotsSize[ii] >= totalSize)
                        {
                            void* ptr = consumeBigFreeSpace(ii, totalSize);

                            return ptr;
                        }
                    }

                    // Expand heap.
                    if (getRemainingSpace() >= totalSize)
                    {
                        void* ptr = expandHeap(totalSize);

                        return ptr;
                    }

                    return NULL;
                }

                void* realloc(void* _ptr, size_t _size)
                {
                    bx::LwMutexScope lock(m_mutex);

                    void* beg = ptrToBegin(_ptr);

                    // Current size.
                    const uint64_t currUsedSize  = readUsedSize(beg);
                    const uint64_t currSize      = unpackSize(currUsedSize);
                    const uint64_t currTotalSize = currSize + HeaderFooterSize;

                    // Requested size.
                    const size_t reqSize      = dm::alignSizeNext(_size, CS_NATURAL_ALIGNMENT);
                    const size_t reqTotalSize = reqSize + HeaderFooterSize;

                    if (reqTotalSize == currTotalSize)
                    {
                        return _ptr;
                    }

                    if (reqTotalSize <= currTotalSize)
                    {
                        // Shrink.

                        const size_t remainingSize = currTotalSize - reqTotalSize;
                        if (remainingSize > MinimalSlotSize)
                        {
                            // Consume and add leftover.
                            writeHeaderFooter(beg, reqTotalSize);

                            const uint64_t leftoverSize = currTotalSize - reqTotalSize;
                            void*          leftoverBeg  = (uint8_t*)beg + reqTotalSize;
                            addSpace(leftoverBeg, leftoverSize);
                        }
                        else
                        {
                            // Consume entire slot.
                            writeHeaderFooter(beg, currTotalSize);
                        }

                        return _ptr;
                    }
                    else /*(reqTotalSize > currTotalSize).*/
                    {
                        // Try to expand.

                        void*    rightBeg = (uint8_t*)beg + currTotalSize;
                        uint64_t rightUsedSize = readUsedSize(rightBeg);
                        if (isFree(rightUsedSize))
                        {
                            const uint64_t rightSize      = unpackSize(rightUsedSize);
                            const uint64_t rightTotalSize = rightSize + HeaderFooterSize;

                            const uint64_t expandSize = reqTotalSize - currTotalSize;

                            if (rightTotalSize >= expandSize)
                            {
                                if (rightTotalSize <= BiggestRegion)
                                {
                                    #if CS_ALLOCAOTR_IMPL
                                        removeFreeSpace(rightBeg, uint32_t(rightTotalSize));
                                    #else
                                        const uint16_t group  = unpackGroup(rightUsedSize);
                                        const uint16_t handle = unpackHandle(rightUsedSize);
                                        removeFreeSpace(rightBeg, uint32_t(rightTotalSize), group, handle);
                                    #endif //CS_ALLOCAOTR_IMPL
                                }
                                else
                                {
                                    removeBigFreeSpace(rightBeg);
                                }

                                const size_t remainingSize = rightTotalSize - expandSize;
                                if (remainingSize > MinimalSlotSize)
                                {
                                    // Consume and add leftover.
                                    writeHeaderFooter(beg, reqTotalSize);

                                    const uint64_t leftoverSize = rightTotalSize     - expandSize;
                                    void*          leftoverBeg  = (uint8_t*)rightBeg + expandSize;
                                    addSpace(leftoverBeg, leftoverSize);
                                }
                                else
                                {
                                    // Consume entire slot.
                                    writeHeaderFooter(beg, rightTotalSize);
                                }

                                return _ptr;
                            }
                        }
                    }

                    return NULL;
                }

                void free(void* _ptr)
                {
                    bx::LwMutexScope lock(m_mutex);

                    void* beg = ptrToBegin(_ptr);

                    const uint64_t usedSize = readUsedSize(beg);
                    const uint64_t size = unpackSize(usedSize);
                    const uint64_t totalSize = size + HeaderFooterSize;

                    uint64_t freeSize = totalSize;
                    uint8_t* freePtr  = (uint8_t*)beg;

                    // Right.
                    void*    rightBeg = (uint8_t*)beg + totalSize;
                    uint64_t rightUsedSize = readUsedSize(rightBeg);
                    if (isFree(rightUsedSize))
                    {
                        const uint64_t rightSize      = unpackSize(rightUsedSize);
                        const uint64_t rightTotalSize = rightSize + HeaderFooterSize;

                        if (rightTotalSize <= BiggestRegion)
                        {
                            #if CS_ALLOCAOTR_IMPL
                                removeFreeSpace(rightBeg, uint32_t(rightTotalSize));
                            #else
                                const uint16_t group  = unpackGroup(rightUsedSize);
                                const uint16_t handle = unpackHandle(rightUsedSize);
                                removeFreeSpace(rightBeg, uint32_t(rightTotalSize), group, handle);
                            #endif //CS_ALLOCAOTR_IMPL
                        }
                        else
                        {
                            removeBigFreeSpace(rightBeg);
                        }

                        freeSize += rightTotalSize;
                    }

                    // Left.
                    const uint64_t leftUsedSize = readLeftUsedSize(beg);
                    if (UINT64_MAX == leftUsedSize)
                    {
                        *m_end += freeSize;

                        uint64_t* terminator = (uint64_t*)*m_end;
                        *terminator = UINT64_MAX;

                        return;
                    }
                    else
                    {
                        const bool leftUsed = unpackUsed(leftUsedSize);
                        if (!leftUsed)
                        {
                            const uint64_t leftSize      = unpackSize(leftUsedSize);
                            const uint64_t leftTotalSize = leftSize + HeaderFooterSize;

                            void* leftBeg = (uint8_t*)beg - leftTotalSize;
                            if (leftTotalSize <= BiggestRegion)
                            {
                                #if CS_ALLOCAOTR_IMPL
                                    removeFreeSpace(leftBeg, uint32_t(leftTotalSize));
                                #else
                                    const uint16_t group  = unpackGroup(leftUsedSize);
                                    const uint16_t handle = unpackHandle(leftUsedSize);
                                    removeFreeSpace(leftBeg, uint32_t(leftTotalSize), group, handle);
                                #endif //CS_ALLOCAOTR_IMPL
                            }
                            else
                            {
                                removeBigFreeSpace(leftBeg);
                            }

                            freeSize += leftTotalSize;
                            freePtr  -= leftTotalSize;
                        }
                    }

                    addSpace(freePtr, freeSize);
                }

                size_t getSize(void* _ptr) const
                {
                    const void* beg = ptrToBegin(_ptr);
                    const uint64_t size = readUsedSize(beg);

                    return size_t(size);
                }

                size_t getRemainingSpace() const
                {
                    return size_t((uint8_t*)*m_end - *m_stackPtr - sizeof(uint64_t)); //Between stack and heap.
                }

                size_t total() const
                {
                    return (uint8_t*)m_begin - *m_end - 2*sizeof(uint64_t);
                }

                bool contains(void* _ptr) const
                {
                    return (*m_end <= _ptr && _ptr < m_begin);
                }

                bx::LwMutex m_mutex;
                void*     m_begin;
                uint8_t** m_end;
                uint8_t** m_stackPtr;

                uint8_t  m_bigFreeSlotsCount;
                uint64_t m_bigFreeSlotsSize[MaxBigFreeSlots];
                void*    m_bigFreeSlotsPtr [MaxBigFreeSlots];

                struct RegionInfo
                {
                    uint16_t m_first;
                    uint16_t m_next;
                };
                RegionInfo m_regionInfo[NumRegions];

                uint16_t m_freeSlotsMax[NumRegions];
                uint16_t m_freeSlotsCount[NumRegions*NumSubRegions];

                #if CS_ALLOCAOTR_IMPL
                    BX_ALIGN_DECL_16(uint32_t* m_freeSlotsSize[NumRegions*NumSubRegions]);
                    BX_ALIGN_DECL_16(uint32_t m_freeSlotsSize0[NumSubRegions][NumSlots0]);
                    BX_ALIGN_DECL_16(uint32_t m_freeSlotsSize1[NumSubRegions][NumSlots1]);
                    BX_ALIGN_DECL_16(uint32_t m_freeSlotsSize2[NumSubRegions][NumSlots2]);
                    BX_ALIGN_DECL_16(uint32_t m_freeSlotsSize3[NumSubRegions][NumSlots3]);
                    BX_ALIGN_DECL_16(uint32_t m_freeSlotsSize4[NumSubRegions][NumSlots4]);
                    BX_ALIGN_DECL_16(uint32_t m_freeSlotsSize5[NumSubRegions][NumSlots5]);
                    BX_ALIGN_DECL_16(uint32_t m_freeSlotsSize6[NumSubRegions][NumSlots6]);
                    BX_ALIGN_DECL_16(uint32_t m_freeSlotsSize7[NumSubRegions][NumSlots7]);
                    BX_ALIGN_DECL_16(uint32_t m_freeSlotsSize8[NumSubRegions][NumSlots8]);
                    BX_ALIGN_DECL_16(uint32_t m_freeSlotsSize9[NumSubRegions][NumSlots9]);

                    BX_ALIGN_DECL_16(void** m_freeSlotsPtr[NumRegions*NumSubRegions]);
                    BX_ALIGN_DECL_16(void* m_freeSlotsPtr0[NumSubRegions][NumSlots0]);
                    BX_ALIGN_DECL_16(void* m_freeSlotsPtr1[NumSubRegions][NumSlots1]);
                    BX_ALIGN_DECL_16(void* m_freeSlotsPtr2[NumSubRegions][NumSlots2]);
                    BX_ALIGN_DECL_16(void* m_freeSlotsPtr3[NumSubRegions][NumSlots3]);
                    BX_ALIGN_DECL_16(void* m_freeSlotsPtr4[NumSubRegions][NumSlots4]);
                    BX_ALIGN_DECL_16(void* m_freeSlotsPtr5[NumSubRegions][NumSlots5]);
                    BX_ALIGN_DECL_16(void* m_freeSlotsPtr6[NumSubRegions][NumSlots6]);
                    BX_ALIGN_DECL_16(void* m_freeSlotsPtr7[NumSubRegions][NumSlots7]);
                    BX_ALIGN_DECL_16(void* m_freeSlotsPtr8[NumSubRegions][NumSlots8]);
                    BX_ALIGN_DECL_16(void* m_freeSlotsPtr9[NumSubRegions][NumSlots9]);
                #else
                    struct FreeSlot
                    {
                        uint32_t m_size;
                        void*    m_ptr;
                    };
                    typedef dm::List<FreeSlot> FreeSlotList;

                    FreeSlotList m_freeSlots[NumRegions*NumSubRegions];
                    uint8_t m_freeSlotsData[TotalSlotCount*FreeSlotList::SizePerElement];
                #endif //CS_ALLOCAOTR_IMPL
            };

            StaticStorage   m_staticStorage;
            SegregatedLists m_segregatedLists;
            DynamicStack    m_stack;
            Heap            m_heap;

            uint8_t* m_stackPtr;
            uint8_t* m_heapEnd;
            void*    m_memory;
            size_t   m_size;
            void*    m_orig;
            #if CS_ALLOC_PRINT_STATS
            uint16_t m_externalAlloc;
            uint16_t m_externalFree;
            size_t   m_externalSize;
            #endif //CS_ALLOC_PRINT_STATS
        };
        static Memory s_memory;

        template <typename StackTy>
        struct StackAllocatorImpl : public cs::StackAllocatorI
        {
            virtual ~StackAllocatorImpl()
            {
            }

            virtual void* alloc(size_t _size, size_t _align = CS_NATURAL_ALIGNMENT, const char* _file = NULL, uint32_t _line = 0) BX_OVERRIDE
            {
                BX_UNUSED(_align, _file, _line);

                void* ptr = m_stack.alloc(_size);
                if (NULL == ptr)
                {
                    ptr = s_memory.alloc(_size);
                }

                return ptr;
            }

            virtual void free(void* _ptr, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
            {
                BX_UNUSED(_align, _file, _line);

                if (!m_stack.contains(_ptr))
                {
                    s_memory.free(_ptr);
                }
            }

            virtual void* realloc(void* _ptr, size_t _size, size_t _align = CS_NATURAL_ALIGNMENT, const char* _file = NULL, uint32_t _line = 0) BX_OVERRIDE
            {
                BX_UNUSED(_ptr, _size, _align, _file, _line);

                void* ptr = m_stack.realloc(_ptr, _size);
                if (NULL == ptr)
                {
                    ptr = s_memory.realloc(_ptr, _size);
                }

                return ptr;
            }

            virtual void push() BX_OVERRIDE
            {
                m_stack.push();
            }

            virtual void pop() BX_OVERRIDE
            {
                m_stack.pop();
            }

            StackTy m_stack;
        };

        struct FixedStackAllocator : public StackAllocatorImpl<FixedStack>
        {
            virtual ~FixedStackAllocator()
            {
            }

            void init(void* _begin, size_t _size)
            {
                m_stack.init(_begin, _size);
            }

            void* mem()
            {
                return m_stack.begin();
            }
        };

        struct DynamicStackAllocator : public StackAllocatorImpl<DynamicStack>
        {
            virtual ~DynamicStackAllocator()
            {
            }

            void init(uint8_t** _stackPtr, uint8_t** _stackLimit)
            {
                m_stack.init(_stackPtr, _stackLimit);
            }
        };

        struct StackList
        {
            cs::StackAllocatorI* createFixed(size_t _size)
            {
                void* mem = s_memory.alloc(_size);
                CS_CHECK(mem, "Memory for stack could not be allocated. Requested %llu.%llu", dm::U_UMB(_size));

                FixedStackAllocator* stackAlloc = m_fixedStacks.addNew();
                stackAlloc->init(mem, _size);

                return (cs::StackAllocatorI*)stackAlloc;
            }

            bool freeFixed(cs::StackAllocatorI* _fixedStackAlloc)
            {
                for (uint16_t ii = m_fixedStacks.count(); ii--; )
                {
                    FixedStackAllocator* stackAlloc = m_fixedStacks.get(ii);
                    if (stackAlloc == _fixedStackAlloc)
                    {
                        s_memory.free(stackAlloc->mem());

                        m_fixedStacks.remove(ii);
                        return true;
                    }
                }

                return false;
            }

            cs::StackAllocatorI* createSplit(size_t _awayFromStackPtr, size_t _preferredSize)
            {
                if (s_memory.sizeBetweenStackAndHeap() < _awayFromStackPtr)
                {
                    return createFixed(_preferredSize);
                }

                // Determine split point.
                uint8_t* split = s_memory.m_stackPtr + _awayFromStackPtr;

                // Limit previous stack.
                const uint16_t count = m_dynamicStacks.count();
                DynamicStack& prev = (0 == count) ? s_memory.m_stack : m_dynamicStacks[count-1].m_stack;
                prev.setInternal(split);

                // Advance stack pointer to point at split point.
                s_memory.m_stackPtr = split;

                // Init a new stack that will take the second split.
                DynamicStackAllocator* stack = m_dynamicStacks.addNew();
                stack->init(&s_memory.m_stackPtr, &s_memory.m_heapEnd);

                CS_PRINT_STACK("Stack split: %llu.%lluMB and %llu.%lluMB.", dm::U_UMB(s_memory.sizeBetweenStackAndHeap()), dm::U_UMB(prev.available()));

                return (cs::StackAllocatorI*)stack;
            }

            bool freeDynamic(cs::StackAllocatorI* _dynamicStackAlloc)
            {
                for (uint16_t ii = m_dynamicStacks.count(); ii--; )
                {
                    DynamicStackAllocator* stackAlloc = m_dynamicStacks.get(ii);
                    if (stackAlloc == _dynamicStackAlloc)
                    {
                        // Determine previous stack.
                        DynamicStack& prev = (0 == ii) ? s_memory.m_stack : m_dynamicStacks[ii-1].m_stack;

                        // Adjust stack ptr.
                        s_memory.m_stackPtr = prev.getStackPtr();

                        // Make previous stack use it.
                        prev.setExternal(&s_memory.m_stackPtr, &s_memory.m_heapEnd);

                        CS_PRINT_STACK("Stack split freed: Available %llu.%lluMB.", dm::U_UMB(s_memory.sizeBetweenStackAndHeap()));

                        m_dynamicStacks.remove(ii);
                        return true;
                    }
                }

                return false;
            }

            void free(cs::StackAllocatorI* _stackAlloc)
            {
                freeFixed(_stackAlloc) || freeDynamic(_stackAlloc);
            }

        private:
            enum { MaxFixedStacks   = 4 };
            enum { MaxDynamicStacks = 4 };

            dm::OpListT<FixedStackAllocator,   MaxFixedStacks>   m_fixedStacks;
            dm::OpListT<DynamicStackAllocator, MaxDynamicStacks> m_dynamicStacks;
        };
        static StackList s_stackList;

        struct StaticAllocator : public bx::ReallocatorI
        {
            StaticAllocator()
            {
                #if CS_ALLOC_PRINT_STATS
                m_allocCount = 0;
                #endif //CS_ALLOC_PRINT_STATS
            }

            virtual ~StaticAllocator()
            {
            }

            virtual void* alloc(size_t _size, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
            {
                BX_UNUSED(_align, _file, _line);

                #if CS_ALLOC_PRINT_STATS
                m_allocCount++;
                #endif //CS_ALLOC_PRINT_STATS

                return s_memory.staticAlloc(_size);
            }

            virtual void free(void* _ptr, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
            {
                BX_UNUSED(_ptr, _align, _file, _line);

                // Do nothing.
            }

            virtual void* realloc(void* _ptr, size_t _size, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
            {
                BX_UNUSED(_ptr, _size, _align, _file, _line);

                return s_memory.staticRealloc(_ptr, _size);
            }

            #if CS_ALLOC_PRINT_STATS
            void printStats()
            {
                fprintf(stderr, "Static allocator:\n\t%4d (alloc)\n\n", m_allocCount);
            }
            #endif //CS_ALLOC_PRINT_STATS

            #if CS_ALLOC_PRINT_STATS
            uint32_t m_allocCount;
            #endif //CS_ALLOC_PRINT_STATS
        };
        static StaticAllocator s_staticAllocator;

        struct StackAllocator : public cs::StackAllocatorI
        {
            StackAllocator()
            {
                #if CS_ALLOC_PRINT_STATS
                m_alloc   = 0;
                m_realloc = 0;
                #endif //CS_ALLOC_PRINT_STATS
            }

            virtual ~StackAllocator()
            {
            }

            virtual void* alloc(size_t _size, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
            {
                BX_UNUSED(_align, _file, _line);

                #if CS_ALLOC_PRINT_STATS
                m_alloc++;
                #endif //CS_ALLOC_PRINT_STATS

                return s_memory.stackAlloc(_size);
            }

            virtual void free(void* _ptr, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
            {
                BX_UNUSED(_align, _file, _line);

                if (!s_memory.fromStackRegion(_ptr))
                {
                    s_memory.free(_ptr);
                }
            }

            virtual void* realloc(void* _ptr, size_t _size, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
            {
                BX_UNUSED(_align, _file, _line);

                #if CS_ALLOC_PRINT_STATS
                m_realloc++;
                #endif //CS_ALLOC_PRINT_STATS

                return s_memory.stackRealloc(_ptr, _size);
            }

            virtual void push() BX_OVERRIDE
            {
                s_memory.stackPush();
            }

            virtual void pop() BX_OVERRIDE
            {
                s_memory.stackPop();
            }

            #if CS_ALLOC_PRINT_STATS
            void printStats()
            {
                fprintf(stderr
                      , "Stack allocator:\n"
                        "\t%4d (alloc)\n"
                        "\t%4d (realloc)\n\n"
                      , m_alloc
                      , m_realloc
                      );
            }
            #endif //CS_ALLOC_PRINT_STATS

        private:
            #if CS_ALLOC_PRINT_STATS
            uint32_t m_alloc;
            uint32_t m_realloc;
            #endif //CS_ALLOC_PRINT_STATS
        };
        static StackAllocator s_stackAllocator;

        struct MainAllocator : public bx::ReallocatorI
        {
            virtual ~MainAllocator()
            {
            }

            virtual void* alloc(size_t _size, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
            {
                BX_UNUSED(_align, _file, _line);

                return s_memory.alloc(_size);
            }

            virtual void free(void* _ptr, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
            {
                BX_UNUSED(_align, _file, _line);

                s_memory.free(_ptr);
            }

            virtual void* realloc(void* _ptr, size_t _size, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
            {
                BX_UNUSED(_align, _file, _line);

                return s_memory.realloc(_ptr, _size);
            }
        };
        static MainAllocator s_mainAllocator;

        struct DelayedFreeAllocator : public bx::AllocatorI
        {
            virtual ~DelayedFreeAllocator()
            {
            }

            virtual void* alloc(size_t _size, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
            {
                BX_UNUSED(_align, _file, _line);

                return s_memory.alloc(_size);
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
                        s_memory.free(m_free[ii].m_ptr);
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
                #if CS_ALLOC_PRINT_STATS
                m_alloc   = 0;
                m_realloc = 0;
                m_free    = 0;
                #endif //CS_ALLOC_PRINT_STATS
            }

            virtual ~BgfxAllocator()
            {
            }

            virtual void* alloc(size_t _size, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
            {
                BX_UNUSED(_align, _file, _line);

                #if CS_ALLOC_PRINT_STATS
                m_mutex.lock();
                m_alloc++;
                m_mutex.unlock();
                #endif //CS_ALLOC_PRINT_STATS

                CS_PRINT_BGFX("Bgfx alloc: %zuB", _size);

                return s_memory.alloc(_size);
            }

            virtual void free(void* _ptr, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
            {
                BX_UNUSED(_align, _file, _line);

                #if CS_ALLOC_PRINT_STATS
                m_mutex.lock();
                m_free++;
                m_mutex.unlock();
                #endif //CS_ALLOC_PRINT_STATS

                CS_PRINT_BGFX("Bgfx free: %llu.%lluKB - (0x%p)", dm::U_UKB(s_memory.getSize(_ptr)), _ptr);

                return s_memory.free(_ptr);
            }

            virtual void* realloc(void* _ptr, size_t _size, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
            {
                BX_UNUSED(_align, _file, _line);

                #if CS_ALLOC_PRINT_STATS
                m_mutex.lock();
                m_realloc++;
                m_mutex.unlock();
                #endif //CS_ALLOC_PRINT_STATS

                CS_PRINT_BGFX("Bgfx realloc: %llu.%lluKB - (0x%p)", dm::U_UKB(s_memory.getSize(_ptr)), _ptr);

                return s_memory.realloc(_ptr, _size);
            }

            #if CS_ALLOC_PRINT_STATS
            void printStats()
            {
                fprintf(stderr
                      , "Bgfx allocator:\n"
                        "\t%d/%d/%d (alloc/realloc/free)\n\n"
                      , m_alloc, m_realloc, m_free
                      );
            }
            #endif //CS_ALLOC_PRINT_STATS

        private:
            #if CS_ALLOC_PRINT_STATS
            uint32_t m_alloc;
            uint32_t m_realloc;
            uint32_t m_free;
            bx::LwMutex m_mutex;
            #endif //CS_ALLOC_PRINT_STATS
        };
        static BgfxAllocator s_bgfxAllocator;
    #endif // !CS_USE_INTERNAL_ALLOCATOR

    struct CrtAllocator : public bx::ReallocatorI
    {
        virtual ~CrtAllocator()
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

            #if CS_USE_INTERNAL_ALLOCATOR
                if (s_memory.contains(_ptr))
                {
                    s_memory.free(_ptr);
                }
                else
                {
                    ::free(_ptr);
                }
            #else
                ::free(_ptr);
            #endif // CS_USE_INTERNAL_ALLOCATOR
        }

        virtual void* realloc(void* _ptr, size_t _size, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
        {
            BX_UNUSED(_align, _file, _line);

            #if CS_USE_INTERNAL_ALLOCATOR
                if (s_memory.contains(_ptr))
                {
                    return s_memory.realloc(_ptr, _size);
                }
                else
                {
                    return ::realloc(_ptr, _size);
                }
            #else
                return ::realloc(_ptr, _size);
            #endif // CS_USE_INTERNAL_ALLOCATOR
        }
    };
    static CrtAllocator s_crtAllocator;

    struct CrtStackAllocator : public StackAllocatorI
    {
        CrtStackAllocator()
        {
            #if CS_ALLOC_PRINT_STATS
            m_alloc   = 0;
            m_realloc = 0;
            #endif //CS_ALLOC_PRINT_STATS

            m_stackFrame = 0;
            m_pointers.init(MaxStackFramesEstimate, &s_crtAllocator);
            for (uint32_t ii = MaxStackFramesEstimate; ii--; )
            {
                m_pointers[ii].init(MaxPointersPerFrameEstiamte, &s_crtAllocator);
            }
        }

        virtual ~CrtStackAllocator()
        {
            for (uint32_t ii = m_pointers.count(); ii--; )
            {
                m_pointers[ii].destroy();
            }

            m_pointers.destroy();
        }

        virtual void* alloc(size_t _size, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
        {
            BX_UNUSED(_align, _file, _line);

            #if CS_ALLOC_PRINT_STATS
            m_alloc++;
            #endif //CS_ALLOC_PRINT_STATS

            void* ptr = ::malloc(_size);
            m_pointers[m_stackFrame].add(ptr);

            return ptr;
        }

        virtual void free(void* _ptr, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
        {
            BX_UNUSED(_ptr, _align, _file, _line);

            // do nothing.
        }

        virtual void* realloc(void* _ptr, size_t _size, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
        {
            BX_UNUSED(_align, _file, _line);

            #if CS_ALLOC_PRINT_STATS
            m_realloc++;
            #endif //CS_ALLOC_PRINT_STATS

            //TODO: in debug mode, check that the pointer is present in m_pointers[m_stackFrame].

            return ::realloc(_ptr, _size);
        }

        virtual void push() BX_OVERRIDE
        {
            //TODO: debug print/count.

            m_stackFrame++;
        }

        virtual void pop() BX_OVERRIDE
        {
            PtrArray& ptrArray = m_pointers[m_stackFrame];
            for (uint32_t ii = ptrArray.count(); ii--; )
            {
                ::free(ptrArray[ii]);
            }
            ptrArray.reset();

            m_stackFrame--;
        }

        #if CS_ALLOC_PRINT_STATS
        void printStats()
        {
            fprintf(stderr
                  , "Crt stack allocator:\n"
                    "\t%4d (alloc)\n"
                    "\t%4d (realloc)\n\n"
                  , m_alloc
                  , m_realloc
                  );
        }
        #endif //CS_ALLOC_PRINT_STATS

    private:
        enum
        {
            MaxStackFramesEstimate      = 16,
            MaxPointersPerFrameEstiamte = 128,
        };

        typedef dm::Array<void*> PtrArray;

        #if CS_ALLOC_PRINT_STATS
        uint32_t m_alloc;
        uint32_t m_realloc;
        #endif //CS_ALLOC_PRINT_STATS

        uint32_t m_stackFrame;
        dm::ObjArray<PtrArray> m_pointers;
    };
    static CrtStackAllocator s_crtStackAllocator;

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

#if 0 // Debug only.
    struct StackAllocatorEmul : public StackAllocatorI
    {
        StackAllocatorEmul()
        {
            #if CS_ALLOC_PRINT_STATS
            m_alloc   = 0;
            m_realloc = 0;
            #endif //CS_ALLOC_PRINT_STATS

            m_stackFrame = 0;
            m_pointers.init(MaxStackFramesEstimate, &s_crtAllocator);
            for (uint32_t ii = MaxStackFramesEstimate; ii--; )
            {
                m_pointers[ii].init(MaxPointersPerFrameEstiamte, &s_crtAllocator);
            }
        }

        virtual ~StackAllocatorEmul()
        {
            for (uint32_t ii = m_pointers.count(); ii--; )
            {
                m_pointers[ii].destroy();
            }

            m_pointers.destroy();
        }

        virtual void* alloc(size_t _size, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
        {
            BX_UNUSED(_align, _file, _line);

            #if CS_ALLOC_PRINT_STATS
            m_alloc++;
            #endif //CS_ALLOC_PRINT_STATS

            void* ptr = s_memory.alloc(_size);
            m_pointers[m_stackFrame].add(ptr);

            return ptr;
        }

        virtual void free(void* _ptr, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
        {
            BX_UNUSED(_ptr, _align, _file, _line);

            // do nothing.
        }

        virtual void* realloc(void* _ptr, size_t _size, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
        {
            BX_UNUSED(_align, _file, _line);

            #if CS_ALLOC_PRINT_STATS
            m_realloc++;
            #endif //CS_ALLOC_PRINT_STATS

            //TODO: in debug mode, check that the pointer is present in m_pointers[m_stackFrame].

            return s_memory.realloc(_ptr, _size);
        }

        virtual void push() BX_OVERRIDE
        {
            //TODO: debug print/count.

            m_stackFrame++;
        }

        virtual void pop() BX_OVERRIDE
        {
            PtrArray& ptrArray = m_pointers[m_stackFrame];
            for (uint32_t ii = ptrArray.count(); ii--; )
            {
                s_memory.free(ptrArray[ii]);
            }
            ptrArray.reset();

            m_stackFrame--;
        }

        #if CS_ALLOC_PRINT_STATS
        void printStats()
        {
            fprintf(stderr
                  , "Crt stack allocator:\n"
                    "\t%4d (alloc)\n"
                    "\t%4d (realloc)\n\n"
                  , m_alloc
                  , m_realloc
                  );
        }
        #endif //CS_ALLOC_PRINT_STATS

    private:
        enum
        {
            MaxStackFramesEstimate      = 16,
            MaxPointersPerFrameEstiamte = 128,
        };

        typedef dm::Array<void*> PtrArray;

        #if CS_ALLOC_PRINT_STATS
        uint32_t m_alloc;
        uint32_t m_realloc;
        #endif //CS_ALLOC_PRINT_STATS

        uint32_t m_stackFrame;
        dm::ObjArray<PtrArray> m_pointers;
    };
    static StackAllocatorEmul s_stackAllocatorEmul;
#endif // 0

    bx::ReallocatorI* g_crtAlloc      = &s_crtAllocator;
    StackAllocatorI*  g_crtStackAlloc = &s_crtStackAllocator;

    #if CS_USE_INTERNAL_ALLOCATOR
        bx::ReallocatorI* g_staticAlloc = &s_staticAllocator;
        StackAllocatorI*  g_stackAlloc  = &s_stackAllocator;
        bx::ReallocatorI* g_mainAlloc   = &s_mainAllocator;
        bx::AllocatorI*   g_delayedFree = &s_delayedFreeAllocator;
        bx::ReallocatorI* g_bgfxAlloc   = &s_bgfxAllocator;
    #else
        bx::ReallocatorI* g_staticAlloc = &s_crtAllocator;
        StackAllocatorI*  g_stackAlloc  = &s_crtStackAllocator;
        bx::ReallocatorI* g_mainAlloc   = &s_crtAllocator;
        bx::AllocatorI*   g_delayedFree = &s_crtDelayedFreeAllocator;
        bx::ReallocatorI* g_bgfxAlloc   = &s_crtAllocator;
    #endif // CS_USE_INTERNAL_ALLOCATOR


    #if CS_USE_INTERNAL_ALLOCATOR
        bool allocInit()
        {
            return s_memory.init();
        }

        size_t allocRemainingStaticMemory()
        {
            return s_memory.remainingStaticMemory();
        }

        void allocGc()
        {
            s_delayedFreeAllocator.cleanup();

            #if CS_ALLOC_PRINT_USAGE
                s_memory.printUsage();
            #endif //CS_ALLOC_PRINT_USAGE
        }

        void allocDestroy()
        {
            #if CS_ALLOC_PRINT_STATS
            printf("----------------------------------------------\n");
            printf(" Memory usage:\n");
            printf("----------------------------------------------\n\n");
            s_staticAllocator.printStats();
            s_stackAllocator.printStats();
            s_bgfxAllocator.printStats();
            #endif //CS_ALLOC_PRINT_STATS

            s_memory.destroy();

            #if CS_ALLOC_PRINT_STATS
            printf("----------------------------------------------\n");
            #endif //CS_ALLOC_PRINT_STATS
        }

        StackAllocatorI* allocCreateStack(size_t _size)
        {
            return s_stackList.createFixed(_size);
        }

        StackAllocatorI* allocSplitStack(size_t _awayFromStackPtr, size_t _preferedSize)
        {
            return s_stackList.createSplit(_awayFromStackPtr, _preferedSize);
        }

        void allocFreeStack(StackAllocatorI* _stackAlloc)
        {
            s_stackList.free(_stackAlloc);
        }
    #else
        bool allocInit()
        {
            return true;
        }

        size_t allocRemainingStaticMemory()
        {
            return cs::StaticStorageSize;
        }

        void allocGc()
        {
            s_crtDelayedFreeAllocator.cleanup();
        }

        void allocDestroy()
        {
        }

        StackAllocatorI* allocCreateStack(size_t /*_size*/)
        {
            return &s_crtStackAllocator;
        }

        StackAllocatorI* allocSplitStack(size_t /*_awayFromStackPtr*/, size_t /*_preferedSize*/)
        {
            return &s_crtStackAllocator;
        }

        void allocFreeStack(StackAllocatorI* /*_stackAlloc*/)
        {
        }
    #endif //CS_USE_INTERNAL_ALLOCATOR

} // namespace cs

// Alloc redirection.
//-----

#if CS_OVERRIDE_NEWDELETE && CS_USE_INTERNAL_ALLOCATOR
    void* operator new(size_t _size)
    {
        // Make sure memory is initialized.
        static const bool assertInitialized = cs::s_memory.init();
        BX_UNUSED(assertInitialized);
        return cs::s_memory.alloc(_size);
    }

    void* operator new[](size_t _size)
    {
        static const bool assertInitialized = cs::s_memory.init();
        BX_UNUSED(assertInitialized);
        return cs::s_memory.alloc(_size);
    }

    void operator delete(void* _ptr)
    {
        static const bool assertInitialized = cs::s_memory.init();
        BX_UNUSED(assertInitialized);
        return cs::s_memory.free(_ptr);
    }

    void operator delete[](void* _ptr)
    {
        static const bool assertInitialized = cs::s_memory.init();
        BX_UNUSED(assertInitialized);
        return cs::s_memory.free(_ptr);
    }
#endif // CS_OVERRIDE_NEWDELETE && CS_OVERRIDE_TINYSTL_ALLOCATOR

#if IMGUI_CONFIG_CUSTOM_ALLOCATOR && CS_USE_INTERNAL_ALLOCATOR
    void* imguiMalloc(size_t _size, void* /*_userptr*/)
    {
        static const bool assertInitialized = cs::s_memory.init();
        BX_UNUSED(assertInitialized);

        return cs::s_memory.alloc(_size);
    }

    void imguiFree(void* _ptr, void* /*_userptr*/)
    {
        static const bool assertInitialized = cs::s_memory.init();
        BX_UNUSED(assertInitialized);

        return cs::s_memory.free(_ptr);
    }
#elif IMGUI_CONFIG_CUSTOM_ALLOCATOR
    void* imguiMalloc(size_t _size, void* /*_userptr*/)
    {
        return ::malloc(_size);
    }

    void imguiFree(void* _ptr, void* /*_userptr*/)
    {
        return ::free(_ptr);
    }
#endif // IMGUI_CONFIG_CUSTOM_ALLOCATOR && CS_USE_INTERNAL_ALLOCATOR

#if !ENTRY_CONFIG_IMPLEMENT_DEFAULT_ALLOCATOR
    namespace entry
    {
        bx::ReallocatorI* getDefaultAllocator()
        {
            #if CS_USE_INTERNAL_ALLOCATOR
                static const bool assertInitialized = cs::s_memory.init();
                BX_UNUSED(assertInitialized);

                return &cs::s_mainAllocator;
            #else
                return &cs::s_crtAllocator;
            #endif // CS_USE_INTERNAL_ALLOCATOR
        }
    }
#endif // !ENTRY_CONFIG_IMPLEMENT_DEFAULT_ALLOCATOR

namespace cs
{
    #if CS_OVERRIDE_TINYSTL_ALLOCATOR && CS_USE_INTERNAL_ALLOCATOR
        void* TinyStlAllocator::static_allocate(size_t _bytes)
        {
            static const bool assertInitialized = cs::s_memory.init();
            BX_UNUSED(assertInitialized);
            return cs::s_memory.alloc(_bytes);
        }

        void TinyStlAllocator::static_deallocate(void* _ptr, size_t /*_bytes*/)
        {
            static const bool assertInitialized = cs::s_memory.init();
            BX_UNUSED(assertInitialized);

            return cs::s_memory.free(_ptr);
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
    #endif // CS_OVERRIDE_TINYSTL_ALLOCATOR && CS_USE_INTERNAL_ALLOCATOR
} // namespace cs

/* vim: set sw=4 ts=4 expandtab: */
