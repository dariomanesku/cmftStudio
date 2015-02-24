/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "common.h"
#include "config.h"

#include <stdio.h>
#include <stdint.h>

#include <bx/string.h>
#include <bx/commandline.h>

Config g_config;

void configFromFile(Config& _config, const char* _path)
{
    FILE* file = fopen(_path, "r");
    if (NULL == file)
    {
        return;
    }

    uint32_t size = (uint32_t)dm::fsize(file);
    char* data = (char*)malloc(size+1);
    size = (uint32_t)fread(data, 1, size, file);
    data[size] = '\0';
    fclose(file);

    const char* ptr = data;
    while ('\0' != *ptr)
    {
        const char* str = bx::strws(ptr);
        const char* eol = bx::streol(str);
        const char* nl  = bx::strnl(eol);
        const char* comment = bx::stristr(str, "#", eol-str);

        const size_t toEnd = (NULL != comment) ? comment-str : eol-str;

#if BX_PLATFORM_WINDOWS
        // Renderer.
        const char* rendererParam = bx::stristr(str, "Renderer", toEnd);
        if (NULL != rendererParam)
        {
            enum { RendererParamLen = 8 }; // "renderer"
            const char* cursor = rendererParam+RendererParamLen;

            const char* equals = bx::stristr(cursor, "=", eol-cursor);
            if (NULL != equals)
            {
                const char* value = bx::strws(equals+1);
                if (value[0] == '\"')
                {
                    ++value;
                }

                if (NULL != bx::stristr(value, "dx9", 3)
                ||  NULL != bx::stristr(value, "directx9", 8))
                {
                    _config.m_renderer = bgfx::RendererType::Direct3D9;
                }
                else if (NULL != bx::stristr(value, "dx11", 4)
                     ||  NULL != bx::stristr(value, "directx11", 9))
                {
                    _config.m_renderer = bgfx::RendererType::Direct3D11;
                }
                else if (NULL != bx::stristr(value, "ogl", 3)
                     ||  NULL != bx::stristr(value, "opengl", 6))
                {
                    _config.m_renderer = bgfx::RendererType::OpenGL;
                }
            }
        }
#endif // BX_PLATFORM_WINDOWS

        // Window size.
        const char* windowSize = bx::stristr(str, "WindowSize", toEnd);
        if (NULL != windowSize)
        {
            enum { WindowSizeLen = 10 }; // "WindowSize"
            const char* cursor = windowSize+WindowSizeLen;

            const char* equals = bx::stristr(cursor, "=", eol-cursor);
            if (NULL != equals)
            {
                const char* begin = bx::strws(equals+1);
                if (begin[0] == '\"')
                {
                    ++begin;
                }

                const char* widthStr  = bx::strws(begin);
                const char* delimiter = bx::stristr(begin, "x", eol-begin);
                const char* heightStr = bx::strws(delimiter+1);

                uint32_t width  = 0;
                uint32_t height = 0;
                sscanf(widthStr, "%d", &width);
                sscanf(heightStr,"%d", &height);

                _config.m_width  = DM_CLAMP(width,  1690, 4096);
                _config.m_height = DM_CLAMP(height,  950, 2160);
            }
        }

        // Startup project.
        const char* projectParam  = bx::stristr(str, "StartupProject", toEnd);
        if (NULL != projectParam)
        {
            enum { StartupProjectLen = 14 }; // "StartupProject"
            const char* cursor = projectParam+StartupProjectLen;

            const char* equals = bx::stristr(cursor, "=", eol-cursor);
            if (NULL != equals)
            {
                const char* begin = bx::strws(equals+1);
                if (begin[0] == '\"')
                {
                    ++begin;
                }
                const char* closingQuotes = bx::stristr(begin, "\"", eol-begin);
                const char* endValue = (NULL != closingQuotes) ? closingQuotes : eol;
                const size_t valueLen = endValue - begin;

                if (valueLen < DM_PATH_LEN)
                {
                    memcpy(_config.m_startupProject, begin, valueLen);
                    _config.m_startupProject[valueLen] = '\0';
                }
            }
        }

        // Memory.
        const char* memoryParam  = bx::stristr(str, "Memory", toEnd);
        if (NULL != memoryParam)
        {
            enum { MemoryLen = 6 }; // "Memory"
            const char* cursor = memoryParam+MemoryLen;

            const char* equals = bx::stristr(cursor, "=", eol-cursor);
            if (NULL != equals)
            {
                const char* begin = bx::strws(equals+1);
                if (begin[0] == '\"')
                {
                    ++begin;
                }

                const char* gb = bx::stristr(begin, "gb", eol-begin);
                const char* end = dm::min(gb, eol);

                char floatVal[8];
                memcpy(floatVal, begin, end-begin);

                float sizeGB;
                sscanf(floatVal, "%f", &sizeGB);

                const uint64_t size = uint64_t(sizeGB*(1000.0f*1000.0f*1024.0f));
                _config.m_memorySize = dm::clamp(size, DM_GIGABYTES_ULL(1), DM_GIGABYTES_ULL(7));
            }
        }

        // Advance pointer.
        ptr = nl;
    }

    free(data);
}

void configFromCli(Config& _config, int _argc, const char* const* _argv)
{
    bx::CommandLine cmdLine(_argc, _argv);

#if BX_PLATFORM_WINDOWS
    const char* renderer = cmdLine.findOption('r', "renderer");
    if (NULL != renderer)
    {
        if (0 == bx::stricmp(renderer, "dx9")
        ||  0 == bx::stricmp(renderer, "directx9"))
        {
            _config.m_renderer = bgfx::RendererType::Direct3D9;
        }
        else if (0 == bx::stricmp(renderer, "dx11")
             ||  0 == bx::stricmp(renderer, "directx11"))
        {
            _config.m_renderer = bgfx::RendererType::Direct3D11;
        }
        else if (0 == bx::stricmp(renderer, "ogl")
             ||  0 == bx::stricmp(renderer, "opengl"))
        {
            _config.m_renderer = bgfx::RendererType::OpenGL;
        }
    }
#endif //BX_PLATFORM_WINDOWS

    const char* project = cmdLine.findOption('p', "project");
    if (NULL != project)
    {
        dm::strscpya(_config.m_startupProject, project);
    }
}

void printCliHelp()
{
    fprintf(stderr
          , "cmftStudio - cubemap filtering tool studio\n"
            "Copyright 2014-2015 Dario Manesku. All rights reserved.\n"
            "License: http://www.opensource.org/licenses/BSD-2-Clause\n\n"
          );

    fprintf(stderr
        , "Usage: cmftstudio -r <renderer>\n"
          "\n"
          "Options:\n"
          "  -r [--renderer] <renderer>                              Select renderer backend.\n"
          "     dx9  [directx9]\n"
          "     dx11 [directx11]\n"
          "     ogl  [opengl]\n"
          "  -p [--project] \"<path_to_cmftStudio_project_file>\"    Specify startup project.\n"
          "\n"
          "Example usage:\n"
          "    cmftstudio -r dx9 -p \"MyProject.cmftStudio\"\n"
          "\n"
          "For additional information, see https://github.com/dariomanesku/cmftstudio\n"
        );
}

/* vim: set sw=4 ts=4 expandtab: */
