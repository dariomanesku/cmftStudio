/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_INFLATEDEFLATE_H_HEADER_GUARD
#define CMFTSTUDIO_INFLATEDEFLATE_H_HEADER_GUARD

#include <stdio.h>           // FILE
#include <dm/misc.h>         // DM_MEGABYTES()
#include <bx/readerwriter.h> // bx::AllocatorI*

#if !defined(CS_USE_CRT_ALLOC_FUNCTIONS)
    #include "common/common.h"   // allocator, g_stackAlloc, g_mainAlloc
#endif

#include "common/memblock.h" // DynamicMemoryBlockWriter
#define MINIZ_HEADER_FILE_ONLY
#include "common/miniz.h"    // deflate(), inflate()

namespace cs
{
    struct DeflateFileWriter : public bx::WriterSeekerI
    {
        DeflateFileWriter(FILE* _file
                        , void* _writeCacheBuffer
                        , void* _deflateBuffer
                        , uint32_t _writeCacheBufferSize
                        , uint32_t _deflateBufferSize
                        , int _compressionLevel = MZ_BEST_SPEED
                        )
        {
            init(_file, _writeCacheBuffer, _deflateBuffer, _writeCacheBufferSize, _deflateBufferSize, _compressionLevel);
        }

        DeflateFileWriter(FILE* _file
                        , bx::AllocatorI* _allocator
                        , uint32_t _writeCacheBufferSize = DM_MEGABYTES(128)
                        , uint32_t _deflateBufferSize    = DM_MEGABYTES(128)
                        , int _compressionLevel          = MZ_BEST_SPEED
                        )
        {
            uint8_t* writeCacheBuffer = (uint8_t*)DM_ALLOC(_allocator, _writeCacheBufferSize+_deflateBufferSize);
            uint8_t* deflateBuffer = writeCacheBuffer + _writeCacheBufferSize;

            init(_file, writeCacheBuffer, deflateBuffer, _writeCacheBufferSize, _deflateBufferSize, _compressionLevel, _allocator);
        }

        private: void init(FILE* _file
                         , void* _writeCacheBuffer
                         , void* _deflateBuffer
                         , uint32_t _writeCacheBufferSize
                         , uint32_t _deflateBufferSize
                         , int _compressionLevel = MZ_BEST_SPEED
                         , bx::AllocatorI* _cleanupAlloc = NULL
                         )
        {
            m_inBuf                = (uint8_t*)_writeCacheBuffer;
            m_outBuf               = (uint8_t*)_deflateBuffer;
            m_writeCacheBufferSize = _writeCacheBufferSize;
            m_deflateBufferSize    = _deflateBufferSize;
            m_cleanupAlloc         = _cleanupAlloc;

            m_file            = _file;
            m_consumed        = 0;
            m_total           = 0;
            m_totalCompressed = 0;

            memset(&m_stream, 0, sizeof(m_stream));
            m_stream.next_in   = m_inBuf;
            m_stream.avail_in  = 0;
            m_stream.next_out  = m_outBuf;
            m_stream.avail_out = m_deflateBufferSize;

            deflateInit(&m_stream, _compressionLevel);
        } public:

        virtual ~DeflateFileWriter()
        {
            if (NULL != m_cleanupAlloc)
            {
                DM_FREE(m_cleanupAlloc, m_inBuf);
            }
        }

        virtual int64_t seek(int64_t _offset = 0, bx::Whence::Enum _whence = bx::Whence::Current) BX_OVERRIDE
        {
            fseeko64(m_file, _offset, _whence);
            return ftello64(m_file);
        }

        virtual int32_t write(const void* _data, int32_t _size) BX_OVERRIDE
        {
            int32_t queued = _size;

            while (queued > 0)
            {
                const uint32_t available = m_writeCacheBufferSize - m_consumed;
                const uint32_t consume = dm::min(available, uint32_t(queued));

                const uint32_t dstOffset = m_consumed;
                const uint32_t srcOffset = _size-queued;

                memcpy((uint8_t*)m_inBuf+dstOffset, (uint8_t*)_data+srcOffset, consume);
                m_consumed += consume;
                m_total    += consume;
                queued     -= consume;

                if (m_consumed == m_writeCacheBufferSize)
                {
                    if (0 != deflateWrite())
                    {
                        return EXIT_FAILURE;
                    }
                }

            }

            return _size;
        }

        private: int32_t deflateWrite(int _flush = MZ_NO_FLUSH)
        {
            m_stream.next_in   = m_inBuf;
            m_stream.avail_in  = m_consumed;
            m_stream.next_out  = m_outBuf;
            m_stream.avail_out = m_deflateBufferSize;

            m_consumed = 0;

            while (0 != m_stream.avail_in)
            {
                if (deflate(&m_stream, _flush) < 0)
                {
                    return EXIT_FAILURE;
                }

                if (m_stream.avail_out != m_deflateBufferSize)
                {
                    const size_t size = m_deflateBufferSize - m_stream.avail_out;
                    if (fwrite(m_outBuf, 1, size, m_file) != size)
                    {
                        return EXIT_FAILURE;
                    }
                    m_totalCompressed += size;
                    m_stream.next_out = m_outBuf;
                    m_stream.avail_out = m_deflateBufferSize;
                }
            }

            return EXIT_SUCCESS;
        } public:

        int32_t flush()
        {
            deflateWrite(MZ_FINISH);
            return deflateEnd(&m_stream);
        }

        uint64_t getTotal() const
        {
            return m_total;
        }

        uint64_t getTotalCompressed() const
        {
            return m_totalCompressed;
        }

    private:
        uint32_t m_writeCacheBufferSize;
        uint32_t m_deflateBufferSize;
        FILE* m_file;
        uint32_t m_consumed;
        uint64_t m_total;
        uint64_t m_totalCompressed;
        uint8_t* m_inBuf;
        uint8_t* m_outBuf;
        z_stream m_stream;
        bx::AllocatorI* m_cleanupAlloc;
    };

    static inline bool readInflate(bx::WriterI* _out
                                 , bx::ReaderI* _in
                                 , uint32_t _inSize
                                 , void* _tmpReadBuffer
                                 , void* _tmpWriteBuffer
                                 , uint32_t _readBufferSize
                                 , uint32_t _writeBufferSize
                                 )
    {
        uint8_t* inBuf  = (uint8_t*)_tmpReadBuffer;
        uint8_t* outBuf = (uint8_t*)_tmpWriteBuffer;

        // Create stream.
        z_stream stream;
        memset(&stream, 0, sizeof(stream));
        stream.next_in = inBuf;
        stream.avail_in = 0;
        stream.next_out = outBuf;
        stream.avail_out = _writeBufferSize;

        // Init stream.
        if (MZ_OK != inflateInit(&stream))
        {
            return false;
        }

        int32_t remaining = _inSize;
        for (;;)
        {
            // Fill read buffer.
            if (0 != remaining)
            {
                const uint32_t size = dm::min(_readBufferSize, uint32_t(remaining));
                bx::read(_in, inBuf, size);

                stream.next_in = inBuf;
                stream.avail_in = size;

                remaining -= size;
            }
            else
            {
                break;
            }

            // Consume read buffer.
            while (0 != stream.avail_in)
            {
                // Decompress.
                if (inflate(&stream, MZ_SYNC_FLUSH) < 0)
                {
                    return false;
                }

                // Write result.
                if (stream.avail_out != _writeBufferSize)
                {
                    const int32_t size = _writeBufferSize - stream.avail_out;
                    bx::write(_out, outBuf, size);

                    stream.next_out = outBuf;
                    stream.avail_out = _writeBufferSize;
                }
            }
        }

        // Cleanup.
        inflateEnd(&stream);

        return true;
    }

    static inline bool readInflate(bx::WriterI* _out
                                 , bx::ReaderI* _in
                                 , uint32_t _inSize
                                 , bx::AllocatorI* _tempAlloc = dm::stackAlloc
                                 , uint32_t _readBufferSize   = DM_MEGABYTES(2)
                                 , uint32_t _writeBufferSize  = DM_MEGABYTES(4)
                                 )
    {
        // Alloc temporary read/write buffers.
        uint8_t* readBuf  = (uint8_t*)DM_ALLOC(_tempAlloc, _readBufferSize+_writeBufferSize);
        uint8_t* writeBuf = readBuf + _readBufferSize;

        // Execute.
        const bool result = readInflate(_out, _in, _inSize, readBuf, writeBuf, _readBufferSize, _writeBufferSize);

        // Cleanup.
        DM_FREE(_tempAlloc, readBuf);

        return result;
    }

    static inline bool readInflate(void*& _outData, uint32_t& _outSize, bx::ReaderI& _reader, uint32_t _size
                                 , bx::ReallocatorI* _outAlloc  = dm::mainAlloc
                                 , bx::AllocatorI*   _tempAlloc = dm::stackAlloc
                                 , uint32_t _initialDataSize = DM_MEGABYTES(16)
                                 , uint32_t _readBufferSize  = DM_MEGABYTES(2)
                                 , uint32_t _writeBufferSize = DM_MEGABYTES(4)
                                 )
    {
        // Alloc read/write buffers.
        uint8_t* inBuf = (uint8_t*)DM_ALLOC(_tempAlloc, _readBufferSize+_writeBufferSize);
        uint8_t* outBuf = inBuf + _readBufferSize;

        // Create stream.
        z_stream stream;
        memset(&stream, 0, sizeof(stream));
        stream.next_in = inBuf;
        stream.avail_in = 0;
        stream.next_out = outBuf;
        stream.avail_out = _writeBufferSize;

        // Init stream.
        if (MZ_OK != inflateInit(&stream))
        {
            return false;
        }

        // Output memory.
        DynamicMemoryBlockWriter memory(_outAlloc, _initialDataSize);

        int32_t remaining = _size;
        for (;;)
        {
            // Fill read buffer.
            if (0 != remaining)
            {
                const uint32_t size = dm::min(_readBufferSize, uint32_t(remaining));
                _reader.read(inBuf, size);

                stream.next_in = inBuf;
                stream.avail_in = size;

                remaining -= size;
            }
            else
            {
                break;
            }

            // Consume read buffer.
            while (0 != stream.avail_in)
            {
                // Decompress.
                if (inflate(&stream, MZ_SYNC_FLUSH) < 0)
                {
                    return false;
                }

                // Write result.
                if (stream.avail_out != _writeBufferSize)
                {
                    const int32_t size = _writeBufferSize - stream.avail_out;
                    memory.write(outBuf, size);

                    stream.next_out = outBuf;
                    stream.avail_out = _writeBufferSize;
                }
            }
        }

        // Cleanup.
        inflateEnd(&stream);
        DM_FREE(_tempAlloc, inBuf);

        // Out.
        _outData = memory.getData();
        _outSize = memory.getDataSize();
        return true;
    }

} //namespace cs

#endif // CMFTSTUDIO_INFLATEDEFLATE_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
