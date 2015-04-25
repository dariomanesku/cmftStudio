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
BGFX_DIR        = (ROOT_DIR .. "bgfx/")
BX_DIR          = (ROOT_DIR .. "bx/")
DM_DIR          = (ROOT_DIR .. "dm/")

local BGFX_SCRIPTS_DIR       = (BGFX_DIR       .. "scripts/")
local DM_SCRIPTS_DIR         = (DM_DIR         .. "scripts/")
local CMFT_SCRIPTS_DIR       = (CMFT_DIR       .. "scripts/")
local CMFTSTUDIO_SCRIPTS_DIR = (CMFTSTUDIO_DIR .. "scripts/")

-- Required for bgfx and example-common
function copyLib()
end

newoption
{
    trigger = "with-tools",
    description = "Enable building tools.",
}

newoption
{
    trigger = "unity-build",
    description = "Single compilation unit build.",
}

newoption
{
    trigger = "with-amalgamated",
    description = "Enable amalgamated build.",
}

newoption
{
    trigger = "with-sdl",
    description = "Enable SDL entry.",
}

if _OPTIONS["with-sdl"] then
    if os.is("windows") then
        if not os.getenv("SDL2_DIR") then
            print("Set SDL2_DIR enviroment variable.")
        end
    end
end

--
-- Solution
--
solution "cmftStudio"
language "C++"
configurations { "Debug", "Release" }
platforms { "x32", "x64" }

defines
{
    "ENTRY_CONFIG_IMPLEMENT_DEFAULT_ALLOCATOR=0",
    "BX_CONFIG_ENABLE_MSVC_LEVEL4_WARNINGS=1",
    "ENTRY_DEFAULT_WIDTH=1920",
    "ENTRY_DEFAULT_HEIGHT=1027",
    "BGFX_CONFIG_RENDERER_OPENGL=21",
}

configuration { "vs* or mingw-*" }
    defines
    {
        "BGFX_CONFIG_RENDERER_DIRECT3D9=1",
        "BGFX_CONFIG_RENDERER_DIRECT3D11=1",
    }

configuration {}

-- Use cmft toolchain for cmft and cmftStudio
dofile (DM_SCRIPTS_DIR    .. "dm_toolchain.lua")
dofile (CMFT_SCRIPTS_DIR  .. "toolchain.lua")
dofile (CMFT_SCRIPTS_DIR  .. "cmft.lua")
dofile (BGFX_SCRIPTS_DIR  .. "bgfx.lua")

dm_toolchain(CMFTSTUDIO_BUILD_DIR, CMFTSTUDIO_PROJECTS_DIR, DEPENDENCY_DIR, BX_DIR)

--
-- External projects.
--
bgfxProject("", "StaticLib", {})
dofile (BGFX_SCRIPTS_DIR .. "example-common.lua")
cmftProject(CMFT_DIR)

--
-- Tools/rawcompress project.
--
project "rawcompress"
    uuid "4b0e6dae-6486-44ea-a57b-840de7c3a9fe"
    kind "ConsoleApp"

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
        DM_DIR   .. "include/**.h",
    }

    if _OPTIONS["unity-build"] then
        excludes
        {
            CMFTSTUDIO_SRC_DIR .. "*.cpp",
            CMFTSTUDIO_SRC_DIR .. "geometry/*.cpp",
            CMFTSTUDIO_SRC_DIR .. "common/*.cpp",
            CMFTSTUDIO_SRC_DIR .. "common/allocator/*.cpp",
        }
    else
        excludes
        {
            CMFTSTUDIO_SRC_DIR .. "build/*.cpp",
        }
    end

    links
    {
        "bgfx",
        "example-common",
        "cmft",
    }

    if _OPTIONS["with-sdl"] then
        defines { "ENTRY_CONFIG_USE_SDL=1" }
        links   { "SDL2" }

        configuration { "x32", "windows" }
            libdirs { "$(SDL2_DIR)/lib/x86" }

        configuration { "x64", "windows" }
            libdirs { "$(SDL2_DIR)/lib/x64" }

        configuration {}
    end

    configuration { "vs*" }
        buildoptions
        {
            "/wd 4127", -- Disable 'Conditional expression is constant' for do {} while(0).
            "/wd 4201", -- Disable 'Nonstandard extension used: nameless struct/union'. Used for uniforms in the project.
            "/wd 4345", -- Disable 'An object of POD type constructed with an initializer of the form () will be default-initialized'. It's an obsolete warning.
        }
        linkoptions
        {
            "/ignore:4199", -- LNK4199: /DELAYLOAD:*.dll ignored; no imports found from *.dll
        }
        links
        { -- this is needed only for testing with GLES2/3 on Windows with VS2008
            "DelayImp",
        }
        files
        {
            CMFTSTUDIO_RES_DIR .. "icon/*"
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

    configuration { "xcode4 or osx" }
        includedirs
        {
            path.join(BGFX_DIR, "3rdparty/khronos"),
        }
        files
        {
            path.join(BGFX_DIR, "src/**.mm"),
            path.join(BGFX_DIR, "examples/common/**.mm"),
        }
        links
        {
            "Cocoa.framework",
            "OpenGL.framework",
        }

    configuration { "xcode4" }
        linkoptions
        {
            "-framework Cocoa",
            "-lc++",
        }

    configuration {}

if _OPTIONS["with-tools"] then
    dofile (BGFX_SCRIPTS_DIR .. "makedisttex.lua")
    dofile (BGFX_SCRIPTS_DIR .. "shaderc.lua")
    dofile (BGFX_SCRIPTS_DIR .. "texturec.lua")
    dofile (BGFX_SCRIPTS_DIR .. "geometryc.lua")
end

strip()

-- vim: set sw=4 ts=4 expandtab:
