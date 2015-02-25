--
-- Copyright 2014-2015 Dario Manesku. All rights reserved.
-- License: http://www.opensource.org/licenses/BSD-2-Clause
--

local CMFTSTUDIO_DIR          = (path.getabsolute("..") .. "/")
local CMFTSTUDIO_SRC_DIR      = (CMFTSTUDIO_DIR .. "src/")
local CMFTSTUDIO_RES_DIR      = (CMFTSTUDIO_DIR .. "res/")
local CMFTSTUDIO_RUNTIME_DIR  = (CMFTSTUDIO_DIR .. "runtime/")
local CMFTSTUDIO_BUILD_DIR    = (CMFTSTUDIO_DIR .. "_build/")
local CMFTSTUDIO_PROJECTS_DIR = (CMFTSTUDIO_DIR .. "_projects/")

local ROOT_DIR       = (CMFTSTUDIO_DIR .. "../")
local DEPENDENCY_DIR = (CMFTSTUDIO_DIR .. "dependency/")

local CMFT_DIR  = (ROOT_DIR .. "cmft/")
BGFX_DIR        = (ROOT_DIR .. "bgfx/") -- TODO put all the content from bgfx.lua into a function.
BX_DIR          = (ROOT_DIR .. "bx/")   -- TODO similarly for this.
DM_DIR          = (ROOT_DIR .. "dm/")

local BGFX_PREMAKE_DIR        = (BGFX_DIR       .. "scripts/")
local CMFT_PREMAKE_DIR        = (CMFT_DIR       .. "scripts/")
local CMFTSTUDIO_PREMAKE_DIR  = (CMFTSTUDIO_DIR .. "scripts/")

-- Required for bgfx and example-common
function copyLib()
end

newoption
{
    trigger = "with-tools",
    description = "Enable building tools.",
}

--
-- Solution
--
solution "cmftStudio"
language "C++"
configurations { "Debug", "Release" }
platforms { "x32", "x64" }
location (CMFTSTUDIO_PROJECTS_DIR .. _ACTION)

configuration { }

defines
{
    "BX_CONFIG_ENABLE_MSVC_LEVEL4_WARNINGS=1",
    "ENTRY_CONFIG_IMPLEMENT_DEFAULT_ALLOCATOR=0",
}

configuration { "Debug" }
    defines
    {
        "CMFTSTUDIO_CONFIG_DEBUG=1",
    }

configuration { "Release" }
    defines
    {
        "CMFTSTUDIO_CONFIG_DEBUG=0",
    }

configuration { "vs*" }
    buildoptions
    {
        "/wd 4127", -- Disable 'Conditional expression is constant' for do {} while(0).
        "/wd 4201", -- Disable 'Nonstandard extension used: nameless struct/union'. Used for uniforms in the project.
        "/wd 4345", -- Disable 'An object of POD type constructed with an initializer of the form () will be default-initialized'. It's an obsolete warning.
    }

configuration { "x32", "vs*" }
    libdirs
    {
        "$(DXSDK_DIR)/lib/x86",
    }

configuration { "x64", "vs*" }
    libdirs
    {
        "$(DXSDK_DIR)/lib/x64",
    }

configuration {}

-- Use cmft toolchain for cmft and cmftStudio
dofile (CMFT_PREMAKE_DIR .. "toolchain.lua")
compat(BX_DIR)

--
-- bgfx project.
--
project ("bgfx")
dofile (CMFTSTUDIO_PREMAKE_DIR .. "bgfx_toolchain.lua")
bgfx_toolchain(CMFTSTUDIO_BUILD_DIR, CMFTSTUDIO_PROJECTS_DIR, BX_DIR, DEPENDENCY_DIR)
dofile (BGFX_PREMAKE_DIR .. "bgfx.lua")
bgfxProject("", "StaticLib", "")

--
-- example-common project.
--
project ("example-common")
dofile (CMFTSTUDIO_PREMAKE_DIR .. "bgfx_toolchain.lua")
bgfx_toolchain(CMFTSTUDIO_BUILD_DIR, CMFTSTUDIO_PROJECTS_DIR, BX_DIR, DEPENDENCY_DIR)
dofile (BGFX_PREMAKE_DIR .. "example-common.lua")

--
-- cmft project.
--
project ("cmft")
dofile (CMFT_PREMAKE_DIR .. "cmft.lua")
cmft_toolchain(CMFTSTUDIO_BUILD_DIR, CMFTSTUDIO_PROJECTS_DIR)
cmftProject(CMFT_DIR)

--
-- Tools/rawcompress project.
--
project "rawcompress"
    uuid "4b0e6dae-6486-44ea-a57b-840de7c3a9fe"
    kind "ConsoleApp"
    cmft_toolchain(CMFTSTUDIO_BUILD_DIR, CMFTSTUDIO_PROJECTS_DIR)

    includedirs
    {
        BX_DIR .. "include",
        DEPENDENCY_DIR,
    }

    files
    {
        "../tools/src/rawcompress.cpp",
    }

    configuration {}

--
-- cmftStudio project.
--
project "cmftStudio"
    uuid("c8847cba-775c-40fd-bcb2-f40f9abb04a7")
    kind "WindowedApp"
    cmft_toolchain(CMFTSTUDIO_BUILD_DIR, CMFTSTUDIO_PROJECTS_DIR)

    configuration {}

    debugdir  (CMFTSTUDIO_RUNTIME_DIR)

    includedirs
    {
        BX_DIR   .. "include",
        BX_DIR   .. "3rdparty",
        BGFX_DIR .. "include",
        BGFX_DIR .. "3rdparty",
        BGFX_DIR .. "examples/common",
        BGFX_DIR .. "3rdparty/forsyth-too",
        DM_DIR   .. "include",
        CMFT_DIR .. "include",
        DEPENDENCY_DIR,
    }

    files
    {
        CMFTSTUDIO_SRC_DIR .. "**.cpp",
        CMFTSTUDIO_SRC_DIR .. "**.h",
        BGFX_DIR .. "3rdparty/forsyth-too/**.cpp",
        BGFX_DIR .. "3rdparty/forsyth-too/**.h",
        CMFTSTUDIO_RES_DIR .. "icon/*",
    }

    links
    {
        "bgfx",
        "example-common",
        "cmft",
    }

    configuration { "vs*" }
        linkoptions
        {
            "/ignore:4199", -- LNK4199: /DELAYLOAD:*.dll ignored; no imports found from *.dll
        }
        links
        { -- this is needed only for testing with GLES2/3 on Windows with VS2008
            "DelayImp",
        }

    configuration { "vs*", "x32" }
        links
        {
            "psapi",
        }

    configuration { "vs2010" }
        linkoptions
        { -- this is needed only for testing with GLES2/3 on Windows with VS201x
            "/DELAYLOAD:\"libEGL.dll\"",
            "/DELAYLOAD:\"libGLESv2.dll\"",
        }

    configuration { "linux-*" }
        links
        {
            "X11",
            "GL",
            "pthread",
        }

    configuration { "osx" }
        files
        {
            BGFX_DIR .. "examples/common/**.mm",
        }
        links {
            "Cocoa.framework",
            "OpenGL.framework",
--          "SDL2",
        }

    configuration {}

strip()

if _OPTIONS["with-tools"] then
    dofile (BGFX_PREMAKE_DIR .. "makedisttex.lua")
    dofile (BGFX_PREMAKE_DIR .. "shaderc.lua")
    dofile (BGFX_PREMAKE_DIR .. "texturec.lua")
    dofile (BGFX_PREMAKE_DIR .. "geometryc.lua")
end

-- vim: set sw=4 ts=4 expandtab:
