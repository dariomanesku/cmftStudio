/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "../appconfig.h"
#include "allocator_config.h"
#include "allocator.h"

#include <stdio.h>             // fprintf
#include "stack.h"             // DynamicStack, FreeStack

#include "../globals.h"        // g_frameNum
#include "../config.h"         // g_config.m_memorySize
#include "../datastructures.h" // HandleAllocT

#include <dm/misc.h>           // DM_MEGABYTES
#include <bx/thread.h>         // bx::Mutex
#include <bx/uint32_t.h>       // bx::uint32_cntlz

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

    enum { StaticStorageSize = DM_MEGABYTES(32) };

    bool init()
    {
        // Make sure init() is called only once!
        static bool s_initialized = false;
        if (s_initialized)
        {
            return false;
        }
        s_initialized = true;

        // Read memory size from config.
        char home[DM_PATH_LEN];
        dm::homeDir(home);

        g_config.init();
        char configPath[DM_PATH_LEN];

        dm::strscpya(configPath, home);
        bx::strlcat(configPath, "/.cmftStudio.conf", DM_PATH_LEN);
        configFromFile(g_config, configPath);

        configWriteDefault(configPath); // Write default config if there isn't one.

        dm::strscpya(configPath, home);
        bx::strlcat(configPath, "/.cmftstudio.conf", DM_PATH_LEN);
        configFromFile(g_config, configPath);

        dm::strscpya(configPath, home);
        bx::strlcat(configPath, "/.cmftStudio/cmftStudio.conf", DM_PATH_LEN);
        configFromFile(g_config, configPath);

        dm::strscpya(configPath, home);
        bx::strlcat(configPath, "/.cmftStudio/cmftstudio.conf", DM_PATH_LEN);
        configFromFile(g_config, configPath);

        configFromFile(g_config, ".cmftstudio.conf");
        configFromFile(g_config, ".cmftStudio.conf");
        configFromFile(g_config, "cmftstudio.conf");
        configFromFile(g_config, "cmftStudio.conf");

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

        // Init memory regions.
        void* ptr = m_memory;
        ptr = m_staticStorage.init(ptr, StaticStorageSize);
        ptr = m_segregatedLists.init(ptr, SegregatedLists::DataSize);

        void* end = (void*)((uint8_t*)m_memory + m_size);
        m_stackPtr = (uint8_t*)dm::alignPtrNext(ptr, CS_NATURAL_ALIGNMENT);
        m_heapEnd  = (uint8_t*)dm::alignPtrPrev(end, CS_NATURAL_ALIGNMENT);

        m_stack.init(&m_stackPtr, &m_heapEnd);
        m_heap.init(&m_stackPtr, &m_heapEnd);

        // Touch every piece of memory, effectively forcing OS to add all memory pages to the process's address space.
        memset(m_memory, 0, m_size);

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

    void* allocBetweenStackAndHeap(size_t _size)
    {
        return m_heap.allocExpand(_size);
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
        enum
        {
            Size0  =                16, Num0  =  16*1024,
            Size1  =                32, Num1  =  16*1024,
            Size2  =                64, Num2  = 256*1024, // Notice: objToBin() uses over 100k of these for std containers.
            Size3  =               256, Num3  =  16*1024,
            Size4  =               512, Num4  =   8*1024,
            Size5  = DM_KILOBYTES(  1), Num5  =   8*1024,
            Size6  = DM_KILOBYTES( 16), Num6  =       64,
            Size7  = DM_KILOBYTES( 64), Num7  =       64,
            Size8  = DM_KILOBYTES(256), Num8  =       32,
            Size9  = DM_KILOBYTES(512), Num9  =       32,
            Size10 =   DM_MEGABYTES(1), Num10 =        8,
            Size11 =   DM_MEGABYTES(4), Num11 =        8,
            Size12 =   DM_MEGABYTES(8), Num12 =        8,

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

    // TODO: reimplement heap!
    struct Heap
    {
        enum { MaxAllocations = UINT16_MAX/4 };

        ///
        /// Notice: heap grows backwards!
        ///
        ///      _stackPtr                                  _heap
        ///      ___|_________________________________________|
        ///  ...    |           |..|  |..| <- Heap |...| |.|..|
        ///      ___|___________|_____________________________|
        ///         .           .                             .
        ///     m_stackPtr    m_end                        m_begin
        ///
        ///

        void init(uint8_t** _stackPtr, uint8_t** _heap)
        {
            m_begin = *_heap;
            m_end = _heap;
            m_stackPtr = _stackPtr;

            m_ptrs.init(MaxAllocations, m_ptrsData);

            // Add an empty chunk at the beginning.
            Pointer* mem = m_ptrs.addNew();
            mem->m_ptr = m_begin;
            writeSize(mem, HeaderSize);
            writeHandle(mem);
        }

        void* alloc(size_t _size)
        {
            const size_t alignedSize = dm::alignSizeNext(_size, CS_NATURAL_ALIGNMENT);
            const size_t totalSize = HeaderSize + alignedSize;

            bx::LwMutexScope lock(m_mutex);

            Pointer* curr = m_ptrs.firstElem();
            for (uint16_t ii = m_ptrs.count()-1; ii--; )
            {
                Pointer* next = m_ptrs.next(curr);

                const size_t spaceBetween = getSpaceBetween(curr, next);
                if (spaceBetween >= totalSize)
                {
                    Pointer* mem = m_ptrs.insertAfter(curr);
                    mem->m_ptr = (uint8_t*)curr->m_ptr - totalSize;
                    writeSize(mem, alignedSize);
                    writeHandle(mem);

                    CS_PRINT_HEAP("Heap alloc: %llu.%lluMB - (0x%p)", dm::U_UMB(totalSize-HeaderSize), mem->m_ptr);

                    return mem->m_ptr;
                }

                curr = next;
            }

            if (getRemainingSpace() >= totalSize)
            {
                Pointer* mem = m_ptrs.insertAfter(curr);
                mem->m_ptr = (uint8_t*)curr->m_ptr - totalSize;
                writeSize(mem, size_t(totalSize-HeaderSize));
                writeHandle(mem);

                CS_PRINT_HEAP("Heap alloc: Expand %llu.%lluMB - (0x%p)", dm::U_UMB(totalSize-HeaderSize), mem->m_ptr);

                // Adjust end pointer.
                *m_end = (uint8_t*)mem->m_ptr;

                return mem->m_ptr;
            }

            CS_PRINT_HEAP("Heap alloc: Failed - %llu.%lluMB", dm::U_UMB(totalSize-HeaderSize));

            return NULL;
        }

        void* allocExpand(size_t _size)
        {
            const size_t alignedSize = dm::alignSizeNext(_size, CS_NATURAL_ALIGNMENT);
            const size_t totalSize = HeaderSize + alignedSize;

            Pointer* last = m_ptrs.lastElem();

            if (getRemainingSpace() >= totalSize)
            {
                Pointer* mem = m_ptrs.insertAfter(last);
                mem->m_ptr = (uint8_t*)last->m_ptr - totalSize;
                writeSize(mem, size_t(totalSize-HeaderSize));
                writeHandle(mem);

                CS_PRINT_HEAP("Heap alloc: Expand %llu.%lluMB - (0x%p)", dm::U_UMB(totalSize-HeaderSize), mem->m_ptr);

                // Adjust end pointer.
                *m_end = (uint8_t*)mem->m_ptr;

                return mem->m_ptr;
            }

            return NULL;
        }

        void* realloc(void* _ptr, size_t _size)
        {
            bx::LwMutexScope lock(m_mutex);

            const uint16_t handle = readHandle(_ptr);
            Pointer* curr = m_ptrs.getObj(handle);

            const size_t currSize = getSize(_ptr);
            const size_t sizeAligned = dm::alignSizeNext(_size, CS_NATURAL_ALIGNMENT);

            if (sizeAligned <= currSize)
            {
                // Shrink.

                const size_t remaining = currSize - sizeAligned;
                if (remaining <= DM_KILOBYTES(1))
                {
                    CS_PRINT_HEAP("Heap realloc: Shrink. Left as is. (0x%p)", curr->m_ptr);
                    return curr->m_ptr;
                }

                writeSize(curr, sizeAligned);

                Pointer* prev = m_ptrs.prev(curr);
                Pointer* mem  = m_ptrs.insertAfter(prev);
                mem->m_ptr = (uint8_t*)curr->m_ptr + sizeAligned + HeaderSize;
                writeSize(mem, size_t(remaining-HeaderSize));
                writeHandle(mem);

                CS_PRINT_HEAP("Heap realloc: Shrink %llu.%lluMB -> %llu.%lluMB (0x%p)", dm::U_UMB(currSize), dm::U_UMB(sizeAligned), curr->m_ptr);

                return curr->m_ptr;
            }
            else
            {
                // Try to expand.

                const size_t expand = _size - currSize;
                const size_t expandAligned = dm::alignSizeNext(expand, CS_NATURAL_ALIGNMENT);

                Pointer* next = m_ptrs.next(curr);
                const size_t spaceBetween = getSpaceBetween(curr, next);
                if (spaceBetween >= expandAligned)
                {
                    writeSize(curr, expandAligned);

                    CS_PRINT_HEAP("Heap realloc: Expand %llu.%lluMB -> %llu.%lluMB (0x%p)", dm::U_UMB(currSize), dm::U_UMB(expandAligned), curr->m_ptr);

                    return curr->m_ptr;
                }
            }

            return NULL;
        }

        void free(void* _ptr)
        {
            CS_PRINT_HEAP("~Heap free: %llu.%lluMB - (0x%p)", dm::U_UMB(getSize(_ptr)), _ptr);

            bx::LwMutexScope lock(m_mutex);

            const uint16_t handle = readHandle(_ptr);

            // Adjust end pointer if it's the last allocation.
            if (_ptr == *m_end)
            {
                Pointer* curr = m_ptrs.getObj(handle);
                Pointer* prev = m_ptrs.prev(curr);
                *m_end = (uint8_t*)prev->m_ptr;
            }

            m_ptrs.remove(handle);
        }

        bool contains(void* _ptr) const
        {
            if (*m_end <= _ptr && _ptr < m_begin)
            {
                return true;
            }
            else
            {
                return false;
            }
        }

        size_t getSize(void* _ptr) const
        {
            size_t* size = (size_t*)_ptr-1;

            return *size;
        }

        size_t getRemainingSpace() const
        {
            return size_t(*m_end - *m_stackPtr - HeaderSize); //Between stack and heap.
        }

        size_t total() const
        {
            return (uint8_t*)m_begin - *m_end;
        }

        #if CS_ALLOC_PRINT_USAGE
        size_t getUsage()
        {
            size_t total = 0;
            Pointer* iter = m_ptrs.firstElem();
            for (uint16_t ii = m_ptrs.count(); ii--; )
            {
                total += readSize(iter);

                iter = m_ptrs.next(iter);
            }

            return total;
        }
        #endif //CS_ALLOC_PRINT_USAGE

        #if CS_ALLOC_PRINT_STATS
        void printStats()
        {
            printf("Heap:\n");

            Pointer* curr = m_ptrs.firstElem();

            int64_t used = (int32_t)readSize(curr);
            int64_t free = 0;

            for (uint16_t ii = m_ptrs.count()-1; ii--; )
            {
                Pointer* next = m_ptrs.next(curr);

                const int64_t nextSize = (int64_t)readSize(next);
                const int64_t spaceBetween = (uint8_t*)curr->m_ptr - (uint8_t*)next->m_ptr - nextSize;

                used += nextSize;
                free += spaceBetween;

                curr = next;
            }

            const size_t remaining = getRemainingSpace();

            printf("\tAllocations:  %3d\n\tUsed: %11llu.%03lluMB\n\tFragmented: %5llu.%03lluMB\n\tRemaining: %6llu.%03lluMB\n\n"
                  , m_ptrs.count()
                  , dm::U_UMB(used)
                  , dm::U_UMB(free)
                  , dm::U_UMB(remaining)
                  );
        }
        #endif //CS_ALLOC_PRINT_STATS

    private:
        struct Pointer
        {
            void* m_ptr;
        };

        void writeSize(Pointer* _pointer, size_t _size)
        {
            void*   mem  = _pointer->m_ptr;
            size_t* size = (size_t*)mem-1;

            *size = _size;
        }

        void writeHandle(Pointer* _pointer)
        {
            void*     mem    = _pointer->m_ptr;
            size_t*   size   = (size_t*)mem-1;
            uint16_t* handle = (uint16_t*)size-1;

            *handle = m_ptrs.getHandle(_pointer);
        }

        size_t readSize(Pointer* _pointer) const
        {
            void*   mem  = _pointer->m_ptr;
            size_t* size = (size_t*)mem-1;

            return *size;
        }

        uint16_t readHandle(void* _ptr) const
        {
            size_t*   size   = (size_t*)_ptr-1;
            uint16_t* handle = (uint16_t*)size-1;

            return *handle;
        }

        // Notice: _a should be the pointer closer the to beginning of the heap.
        inline size_t getSpaceBetween(Pointer* _a, Pointer* _b)
        {
            return size_t((uint8_t*)_a->m_ptr - (uint8_t*)_b->m_ptr - readSize(_b));
        }

        enum
        {
            Handle = sizeof(uint16_t),
            SizeTy = sizeof(size_t),
            Header = Handle+SizeTy,
            HeaderSize = ((Header/CS_NATURAL_ALIGNMENT)+1)*CS_NATURAL_ALIGNMENT,
        };

        void*       m_begin;
        uint8_t**   m_end;
        uint8_t**   m_stackPtr;
        bx::LwMutex m_mutex;

        dm::LinkedList<Pointer> m_ptrs;
        uint8_t m_ptrsData[dm::LinkedList<Pointer>::SizePerElement*MaxAllocations];
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
struct StackAllocator : public cs::StackAllocatorI
{
    virtual ~StackAllocator()
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

struct FixedStackAllocator : public StackAllocator<FixedStack>
{
    void init(void* _begin, size_t _size)
    {
        m_stack.init(_begin, _size);
    }

    void* mem()
    {
        return m_stack.begin();
    }
};

struct DynamicStackAllocator : public StackAllocator<DynamicStack>
{
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

namespace cs
{
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

    struct StackAllocatorImpl : public StackAllocatorI
    {
        StackAllocatorImpl()
        {
            #if CS_ALLOC_PRINT_STATS
            m_alloc   = 0;
            m_realloc = 0;
            #endif //CS_ALLOC_PRINT_STATS

            m_stackFrame = 0;
        }

        virtual ~StackAllocatorImpl()
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

        uint16_t m_stackFrame;
        bool m_lock;
    };

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

    struct DelayedFreeAllocator : public bx::ReallocatorI
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

        virtual void* realloc(void* _ptr, size_t _size, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
        {
            BX_UNUSED(_align, _file, _line);

            return s_memory.realloc(_ptr, _size);
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

        dm::ListT<PostponedFree, 128> m_free;
    };

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

            if (s_memory.contains(_ptr))
            {
                s_memory.free(_ptr);
            }
            else
            {
                ::free(_ptr);
            }
        }

        virtual void* realloc(void* _ptr, size_t _size, size_t _align, const char* _file, uint32_t _line) BX_OVERRIDE
        {
            BX_UNUSED(_align, _file, _line);

            if (s_memory.contains(_ptr))
            {
                return s_memory.realloc(_ptr, _size);
            }
            else
            {
                return ::realloc(_ptr, _size);
            }
        }
    };

    static CrtAllocator         s_crtAllocator;
    static StaticAllocator      s_staticAllocator;
    static StackAllocatorImpl   s_stackAllocator;
    static MainAllocator        s_mainAllocator;
    static DelayedFreeAllocator s_delayedFreeAllocator;
    static BgfxAllocator        s_bgfxAllocator;

    bx::ReallocatorI* g_crtAlloc    = &s_crtAllocator;
    bx::ReallocatorI* g_staticAlloc = &s_staticAllocator;
    StackAllocatorI*  g_stackAlloc  = &s_stackAllocator;
    bx::ReallocatorI* g_mainAlloc   = &s_mainAllocator;
    bx::AllocatorI*   g_delayedFree = &s_delayedFreeAllocator;
    bx::ReallocatorI* g_bgfxAlloc   = &s_bgfxAllocator;

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

} // namespace cs

// Alloc redirection.
//-----

void* operator new(size_t _size)
{
    // Make sure memory is initialized.
    static const bool assertInitialized = s_memory.init();
    BX_UNUSED(assertInitialized);

    return s_memory.alloc(_size);
}

void* operator new[](size_t _size)
{
    static const bool assertInitialized = s_memory.init();
    BX_UNUSED(assertInitialized);

    return s_memory.alloc(_size);
}

void operator delete(void* _ptr)
{
    static const bool assertInitialized = s_memory.init();
    BX_UNUSED(assertInitialized);

    return s_memory.free(_ptr);
}

void operator delete[](void* _ptr)
{
    static const bool assertInitialized = s_memory.init();
    BX_UNUSED(assertInitialized);

    return s_memory.free(_ptr);
}

#if !ENTRY_CONFIG_IMPLEMENT_DEFAULT_ALLOCATOR
namespace entry
{
    bx::ReallocatorI* getDefaultAllocator()
    {
        static const bool assertInitialized = s_memory.init();
        BX_UNUSED(assertInitialized);

        return &cs::s_mainAllocator;
    }
}
#endif //!ENTRY_CONFIG_IMPLEMENT_DEFAULT_ALLOCATOR

#if IMGUI_CONFIG_CUSTOM_ALLOCATOR
void* imguiMalloc(size_t _size, void* /*_userptr*/)
{
    static const bool assertInitialized = s_memory.init();
    BX_UNUSED(assertInitialized);

    return s_memory.alloc(_size);
}

void imguiFree(void* _ptr, void* /*_userptr*/)
{
    static const bool assertInitialized = s_memory.init();
    BX_UNUSED(assertInitialized);

    return s_memory.free(_ptr);
}
#endif //IMGUI_CONFIG_CUSTOM_ALLOCATOR

namespace cs
{
    void* TinyStlAllocator::static_allocate(size_t _bytes)
    {
        static const bool assertInitialized = s_memory.init();
        BX_UNUSED(assertInitialized);

        return s_memory.alloc(_bytes);
    }

    void TinyStlAllocator::static_deallocate(void* _ptr, size_t /*_bytes*/)
    {
        static const bool assertInitialized = s_memory.init();
        BX_UNUSED(assertInitialized);

        return s_memory.free(_ptr);
    }
} // namespace cs

/* vim: set sw=4 ts=4 expandtab: */
