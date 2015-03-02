/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include <stdio.h>
#include <stdint.h>

#include <bx/macros.h>
#include <bx/commandline.h>
#include <bx/readerwriter.h>
#include <bx/string.h>

// Miniz configuration.
BX_PRAGMA_DIAGNOSTIC_PUSH_GCC()
BX_PRAGMA_DIAGNOSTIC_IGNORED_GCC("-Wstrict-aliasing")
#define MINIZ_NO_TIME
#define MINIZ_NO_ARCHIVE_APIS
#define MINIZ_NO_ARCHIVE_WRITING_APIS
#include <miniz/miniz.c>
BX_PRAGMA_DIAGNOSTIC_POP_GCC()

// Misc.
#define MEGABYTES(_MB) (_MB<<20)

#define RC_MIN(_a, _b) (_a)<(_b)?(_a):(_b)
#define RC_MAX(_a, _b) (_a)>(_b)?(_a):(_b)
#define RC_CLAMP(_val, _min, _max) RC_MIN(RC_MAX(_val, _min), _max)

struct DeflateFileWriter : public bx::FileWriterI
{
    DeflateFileWriter(int32_t _writeCacheBufferSize, uint32_t _deflateBufferSize, int _compressionLevel)
    {
        m_writeCacheBufferSize = _writeCacheBufferSize;
        m_deflateBufferSize    = _deflateBufferSize;

        m_file            = NULL;
        m_consumed        = 0;
        m_total           = 0;
        m_totalCompressed = 0;

        m_inBuf = (uint8_t*)malloc(m_writeCacheBufferSize+m_deflateBufferSize);
        m_outBuf = m_inBuf + m_writeCacheBufferSize;

        memset(&m_stream, 0, sizeof(m_stream));
        m_stream.next_in = m_inBuf;
        m_stream.avail_in = 0;
        m_stream.next_out = m_outBuf;
        m_stream.avail_out = m_deflateBufferSize;

        deflateInit(&m_stream, _compressionLevel);
    }

    virtual ~DeflateFileWriter()
    {
        if (NULL != m_inBuf)
        {
            free(m_inBuf);
        }
    }

    void assign(FILE* _file)
    {
        m_file = _file;
    }

    virtual int32_t open(const char* _filePath, bool _append = false) BX_OVERRIDE
    {
        if (_append)
        {
            m_file = fopen(_filePath, "ab");
        }
        else
        {
            m_file = fopen(_filePath, "wb");
        }

        return (NULL == m_file);
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
            const uint32_t srcOffset = _size-queued;
            const uint32_t dstOffset = m_consumed;

            const uint32_t available = m_writeCacheBufferSize - m_consumed;
            const uint32_t size = RC_MIN(available, uint32_t(queued));

            memcpy((uint8_t*)m_inBuf+dstOffset, (uint8_t*)_data+srcOffset, size);
            m_consumed += size;
            m_total    += size;
            queued     -= size;

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

    int32_t deflateWrite(int _flush = MZ_NO_FLUSH)
    {
        m_stream.next_in = m_inBuf;
        m_stream.avail_in = m_consumed;
        m_stream.next_out = m_outBuf;
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
    }

    uint64_t getTotal() const
    {
        return m_total;
    }

    uint64_t getTotalCompressed() const
    {
        return m_totalCompressed;
    }

    virtual int32_t close() BX_OVERRIDE
    {
        deflateWrite(MZ_FINISH);
        const int32_t status = deflateEnd(&m_stream);
        fclose(m_file);

        return status;
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
};

static void help(const char* _error = NULL)
{
    if (NULL != _error)
    {
        fprintf(stderr, "Error:\n%s\n\n", _error);
    }

    fprintf(stderr
           , "Usage: rawcompress -i <in> -o <out>\n"

             "\n"
             "Options:\n"
             "  -i <file path>    Input file path.\n"
             "  -o <file path>    Output file path.\n"
           );
}

int main(int _argc, const char* _argv[])
{
    bx::CommandLine cmdLine(_argc, _argv);

    if (cmdLine.hasArg('h', "help") )
    {
        help();
        return EXIT_FAILURE;
    }

    const char* filePath = cmdLine.findOption('i');
    if (NULL == filePath)
    {
        help("Input file name must be specified.");
        return EXIT_FAILURE;
    }

    const char* outFilePath = cmdLine.findOption('o');
    if (NULL == outFilePath)
    {
        help("Output file name must be specified.");
        return EXIT_FAILURE;
    }

    void* data = NULL;
    uint32_t size = 0;

    bx::CrtFileReader fr;
    if (0 == bx::open(&fr, filePath) )
    {
        size = (uint32_t)bx::getSize(&fr);
        data = malloc(size);
        bx::read(&fr, data, size);

        FILE* file = fopen(outFilePath, "wb");
        if (NULL != file)
        {
            enum { CompressionLevel = 10 };
            DeflateFileWriter writer(MEGABYTES(20), MEGABYTES(20), CompressionLevel);
            writer.assign(file);
            writer.write(data, size);
            writer.close();
        }

        free(data);
    }

    return 0;
}

/* vim: set sw=4 ts=4 expandtab: */
