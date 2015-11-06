/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_CONFIG_H_HEADER_GUARD
#define CMFTSTUDIO_CONFIG_H_HEADER_GUARD

#include <bgfx/bgfx.h>    // bgfx::RendererType
#include <dm/misc.h> // DM_GIGABYTES, DM_PATH_LEN

struct Config
{
    Config()
    {
        #if BX_ARCH_64BIT
        m_memorySize        = DM_GIGABYTES(2);
        #else // Windows 32bit build cannot allocate 2GB.
        m_memorySize        = DM_MEGABYTES(1536);
        #endif // BX_ARCH_64BIT
        m_width             = 1920;
        m_height            = 1027;
        m_renderer          = bgfx::RendererType::Count;
        m_loaded            = false;
        m_startupProject[0] = '\0';
    }

    uint64_t m_memorySize;
    uint32_t m_width;
    uint32_t m_height;
    bgfx::RendererType::Enum m_renderer;
    bool m_loaded;
    char m_startupProject[DM_PATH_LEN];
};

void configWriteDefault(const char* _path);
void configFromFile(Config& _config, const char* _path);
void configFromDefaultPaths(Config& _config);
void configFromCli(Config& _config, int _argc, const char* const* _argv);
void printCliHelp();

extern Config g_config;

#endif // CMFTSTUDIO_CONFIG_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
