/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_DATASTRUCTURES_H_HEADER_GUARD
#define CMFTSTUDIO_DATASTRUCTURES_H_HEADER_GUARD

#include <stdint.h> //uint32_t
#include <new>      // placement-new

#include <dm/misc.h>           // dm::strscpy, DM_PATH_LEN
#include <bx/uint32_t.h>       // bx::uint64_cnttz(), bx::uint64_cntlz()
#include <dm/datastructures.h> // dm::ArrayT

// Handle array.
//-----

#define CS_HANDLE(_name)                        \
    struct _name                                \
    {                                           \
        enum Enum { Invalid = UINT16_MAX };     \
        static inline _name invalid()           \
        {                                       \
            _name handle = { _name::Invalid };  \
            return handle;                      \
        }                                       \
        uint16_t m_idx;                         \
    };                                          \
    inline bool isValid(_name _handle)          \
    {                                           \
        return _name::Invalid != _handle.m_idx; \
    }

template <typename HandleArray, typename HandleTy>
DM_INLINE bool handleArrayRemove(HandleArray* _ha, HandleTy _handle)
{
    for (uint16_t ii = 0, end = _ha->count(); ii < end; ++ii)
    {
        const HandleTy* values = _ha->elements();
        if (_handle.m_idx == values[ii].m_idx)
        {
            _ha->remove(ii);
            return true;
        }
    }

    return false;
}

template <typename HandleArray, typename HandleTy>
DM_INLINE uint16_t handleArrayIdxOf(HandleArray* _ha, HandleTy _handle)
{
    for (uint16_t ii = 0, end = _ha->count(); ii < end; ++ii)
    {
        const HandleTy* values = _ha->elements();
        if (_handle.m_idx == values[ii].m_idx)
        {
            return ii;
        }
    }

    return UINT16_MAX;
}

template <typename Ty/*cs handle type*/, uint16_t MaxHandlesT>
struct HandleArrayT : public dm::ArrayT<Ty, MaxHandlesT>
{
    typedef dm::ArrayT<Ty, MaxHandlesT> BaseType;

    using BaseType::remove;

    // Avoid using this, it's O(n).
    bool remove(Ty _handle)
    {
        return handleArrayRemove<HandleArrayT,Ty>(this, _handle);
    }

    // Avoid using this, it's O(n).
    uint16_t idxOf(Ty _handle)
    {
        return handleArrayIdxOf<HandleArrayT,Ty>(this, _handle);
    }

    uint16_t count() const
    {
        return (uint16_t)BaseType::count();
    }
};

template <typename Ty/*cs handle type*/>
struct HandleArray : public dm::Array<Ty>
{
    typedef dm::Array<Ty> BaseType;

    using BaseType::remove;

    // Avoid using this, it's O(n).
    bool remove(Ty _handle)
    {
        return handleArrayRemove<HandleArray,Ty>(this, _handle);
    }

    // Avoid using this, it's O(n).
    uint16_t idxOf(Ty _handle)
    {
        return handleArrayIdxOf<HandleArray,Ty>(this, _handle);
    }

    uint16_t count() const
    {
        return (uint16_t)BaseType::count();
    }
};

template <typename Ty/*cs handle type*/>
DM_INLINE HandleArray<Ty>* createHandleArray(uint16_t _maxHandles, void* _mem, bx::AllocatorI* _allocator)
{
    return ::new (_mem) HandleArray<Ty>(_maxHandles, (uint8_t*)_mem + sizeof(HandleArray<Ty>), _allocator);
}

template <typename Ty/*cs handle type*/>
DM_INLINE HandleArray<Ty>* createHandleArray(uint16_t _maxHandles, bx::AllocatorI* _allocator)
{
    uint8_t* ptr = (uint8_t*)BX_ALLOC(_allocator, sizeof(HandleArray<Ty>) + HandleArray<Ty>::sizeFor(_maxHandles));
    return createHandleArray<Ty>(_maxHandles, ptr, _allocator);
}

template <typename Ty/*cs handle type*/>
DM_INLINE void destroyHandleArray(HandleArray<Ty>* _array)
{
    _array->~HandleArray<Ty>();
    BX_FREE(_array->m_allocator, _array);
}

template <typename Ty/*cs handle type*/>
struct StrHandleMap
{
    enum
    {
        MaxStrLength = 128,
    };

    // Uninitialized state, init() needs to be called !
    StrHandleMap()
    {
    }

    StrHandleMap(uint16_t _max)
    {
        init(_max);
    }

    StrHandleMap(uint16_t _max, void* _mem)
    {
        init(_max, _mem);
    }

    ~StrHandleMap()
    {
        destroy();
    }

    // Allocates memory internally.
    void init(uint16_t _max, bx::ReallocatorI* _reallocator)
    {
        m_max = _max;
        m_str = (char(*)[MaxStrLength])BX_ALLOC(_reallocator, sizeFor(_max));
        m_reallocator = _reallocator;
        m_cleanup = true;
    }

    static uint32_t sizeFor(uint16_t _max)
    {
        return _max*MaxStrLength*sizeof(char);
    }

    // Uses externaly allocated memory.
    void* init(uint16_t _max, void* _mem, bx::AllocatorI* _allocator = NULL)
    {
        m_max = _max;
        m_str = (char(*)[MaxStrLength])_mem;
        m_allocator = _allocator;
        m_cleanup = false;

        void* end = (void*)((uint8_t*)_mem + sizeFor(_max));
        return end;
    }

    void destroy()
    {
        if (m_cleanup && NULL != m_str)
        {
            BX_FREE(m_reallocator, m_str);
            m_str = NULL;
        }
    }

    void map(Ty _handle, const char* _str)
    {
        CS_CHECK(_handle.m_idx < m_max, "StrHandleMap::map | %d, %d", _handle.m_idx, m_max);

        dm::strscpy(m_str[_handle.m_idx], _str, MaxStrLength);
    }

    void unmap(Ty _handle)
    {
        CS_CHECK(_handle.m_idx < m_max, "StrHandleMap::unmap(_handle) | %d, %d", _handle.m_idx, m_max);

        m_str[_handle.m_idx][0] = '\0';
    }

    void unmap(uint16_t _idx)
    {
        CS_CHECK(_idx < m_max, "StrHandleMap::unmap(_idx) | %d, %d", _idx, m_max);

        m_str[_idx][0] = '\0';
    }

    char* get(Ty _handle)
    {
        CS_CHECK(_handle.m_idx < m_max, "StrHandleMap::get | %d, %d", _handle.m_idx, m_max);

        return m_str[_handle.m_idx];
    }

    // Avoid using this, it's O(n).
    Ty get(const char* _name)
    {
        if (NULL == _name
        ||  '\0' == _name[0])
        {
            const Ty invalid = { Ty::Invalid };
            return invalid;
        }

        for (uint16_t ii = 0, end = m_max; ii < end; ++ii)
        {
            if (0 == strcmp(_name, m_str[ii]))
            {
                const Ty handle = { ii };
                return handle;
            }
        }

        const Ty invalid = { Ty::Invalid };
        return invalid;
    }

    bx::AllocatorI* allocator()
    {
        return m_allocator;
    }

private:
    uint16_t m_max;
    char (*m_str)[MaxStrLength];
    union
    {
        bx::AllocatorI*   m_allocator;
        bx::ReallocatorI* m_reallocator;
    };
    bool m_cleanup;
};

template <typename Ty/*cs handle type*/>
static inline StrHandleMap<Ty>* createStrHandleMap(uint16_t _maxHandles, bx::AllocatorI* _allocator)
{
    uint8_t* ptr = (uint8_t*)BX_ALLOC(_allocator, sizeof(StrHandleMap<Ty>) + StrHandleMap<Ty>::sizeFor(_maxHandles));
    return ::new (ptr) StrHandleMap<Ty>(_maxHandles, ptr + sizeof(StrHandleMap<Ty>), _allocator);
}

template <typename Ty/*cs handle type*/>
static inline void destroyStrHandleMap(StrHandleMap<Ty>* _strHandleMap)
{
    _strHandleMap->~StrHandleMap<Ty>();
    BX_FREE(_strHandleMap->allocator(), _strHandleMap);
}

template <typename Ty/*cs handle type*/, uint16_t MaxElementsT=32>
struct StrHandleMapT
{
    enum
    {
        MaxStrLength = 128,
    };

    StrHandleMapT()
    {
        for (uint16_t ii = 0; ii < MaxElementsT; ++ii)
        {
            m_str[ii][0] = '\0';
        }
    }

    void map(Ty _handle, const char* _str)
    {
        CS_CHECK(_handle.m_idx < MaxElementsT, "StrHandleMapT::map | %d, %d", _handle.m_idx, MaxElementsT);

        dm::strscpy(m_str[_handle.m_idx], _str, MaxStrLength);
    }

    void unmap(Ty _handle)
    {
        CS_CHECK(_handle.m_idx < MaxElementsT, "StrHandleMapT::unmap(_handle) | %d, %d", _handle.m_idx, MaxElementsT);

        m_str[_handle.m_idx][0] = '\0';
    }

    void unmap(uint16_t _idx)
    {
        CS_CHECK(_idx < MaxElementsT, "StrHandleMapT::unmap(_idx) | %d, %d", _idx, MaxElementsT);

        m_str[_idx][0] = '\0';
    }

    char* get(Ty _handle)
    {
        CS_CHECK(_handle.m_idx < MaxElementsT, "StrHandleMapT::get | %d, %d", _handle.m_idx, MaxElementsT);

        return m_str[_handle.m_idx];
    }

    // Avoid using this, it's O(n).
    Ty get(const char* _name)
    {
        if (NULL == _name
        ||  '\0' == _name[0])
        {
            const Ty invalid = { Ty::Invalid };
            return invalid;
        }

        for (uint16_t ii = 0, end = MaxElementsT; ii < end; ++ii)
        {
            if (0 == strcmp(_name, m_str[ii]))
            {
                const Ty handle = { ii };
                return handle;
            }
        }

        const Ty invalid = { Ty::Invalid };
        return invalid;
    }

private:
    char m_str[MaxElementsT][MaxStrLength];
};

#endif // CMFTSTUDIO_DATASTRUCTURES_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
