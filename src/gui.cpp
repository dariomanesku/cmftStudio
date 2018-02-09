/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#define IMGUI_SCROLL_AREA_R 0
#define IMGUI_SCROLL_BAR_R  4
#define IMGUI_BUTTON_R      4
#define IMGUI_INPUT_R       4

#include "common/common.h"
#include "gui.h"

#include <stdint.h>
#include <string.h>

#include <bgfx_utils.h>             // load()
#include <bx/string.h>              // bx::snprintf
#include <bx/os.h>                  // bx::pwd

#include <dm/misc.h>                // dm::utof, dm::equals

#include "renderpipeline.h"
#include "staticres.h"              // gui_res.h
#include "common/utils.h"           // vecFromLatLong(), latLongFromVec()
#include "geometry/loadermanager.h" // cs::geometryLoaderCount(), cs::geometryLoaderGetExtensions()

/// Notice: Always call tinydir functions between push/pop(_stackAlloc);
#define _TINYDIR_MALLOC(_size) DM_ALLOC(dm::stackAlloc, _size)
#define _TINYDIR_FREE(_ptr)    DM_FREE(dm::stackAlloc, _ptr)
#include <tinydir/tinydir.h>

// Constants.
//-----

enum
{
    FileExtensionLength = 16,
    ListElementHeight   = 24,
};

// Imgui helper functions.
//-----

static void imguiBool(const char* _str, float& _flag, bool _enabled = true)
{
    bool bFlag = (1.0f == _flag);
    imguiBool(_str, bFlag, _enabled);

    _flag = float(bFlag);
}

static void imguiRegion(const char* _str, const char* _strRight, bool& _flag, bool _enabled = true)
{
    if (imguiCollapse(_str, _strRight, _flag, _enabled))
    {
        _flag = !_flag;
    }
}

static void imguiRegionBorder(const char* _str, const char* _strRight, bool& _flag, bool _enabled = true)
{
    imguiSeparatorLine(1);
    imguiRegion(_str, _strRight, _flag, _enabled);
    imguiSeparatorLine(1);
    imguiSeparator(4);
}

static void imguiLabelBorder(const char* _str)
{
    imguiSeparatorLine(1);
    imguiSeparator(2);
    imguiIndent(8);
    imguiLabel(_str);
    imguiUnindent(8);
    imguiSeparator(2);
    imguiSeparatorLine(1);

    imguiSeparator(8);
}

static void imguiSelection(uint16_t& _selection, const char* _choice[], uint16_t _count, bool _enabled = true)
{
    for (uint16_t ii = 0; ii < _count; ++ii)
    {
        if (imguiCheck(_choice[ii], _selection == ii, _enabled))
        {
            _selection = ii;
        }
    }
}

static void imguiCheckSelection(const char* _str, uint16_t& _selectionValue, uint16_t _checkBoxValue, bool _enabled = true)
{
    if (imguiCheck(_str, _selectionValue == _checkBoxValue, _enabled))
    {
        _selectionValue = _checkBoxValue;
    }
}

static void imguiTabsSize(float& _size, float _currSize = 65536.0f)
{
    // Max tabs.
    const uint8_t maxTabs = (uint8_t)dm::min(5, int32_t(dm::log2f(_currSize))-5);

    // Selected index.
    const float   pwr  = dm::log2f(_size);
    const int32_t pwri = int32_t(pwr);
    const uint8_t idx  = (0 == (pwr - float(pwri))) ? uint8_t(pwri-6) : UINT8_MAX;

    const uint8_t tab = imguiTabs(idx, true, ImguiAlign::CenterIndented, 16, 2, maxTabs, "64", "128", "256", "512", "1024");
    if (UINT8_MAX != tab)
    {
        _size = float(1<<(tab+6));
    }
}

static void imguiTabsToLinear(float& _gamma)
{
    uint8_t idx;
    if (1.0f == _gamma)
    {
        idx = 0;
    }
    else if (2.2f == _gamma)
    {
        idx = 1;
    }
    else
    {
        idx = UINT8_MAX;
    }

    idx = imguiTabs(idx, true, ImguiAlign::CenterIndented, 16, 2, 2, "None", "2.2");
    if (UINT8_MAX != idx)
    {
        _gamma = (0==idx) ? 1.0f : 2.2f;
    }
}

static void imguiTabsToGamma(float& _gamma)
{
    uint8_t idx;
    if (1.0f == _gamma)
    {
        idx = 0;
    }
    else if (1.0f/2.2f == _gamma)
    {
        idx = 1;
    }
    else
    {
        idx = UINT8_MAX;
    }

    idx = imguiTabs(idx, true, ImguiAlign::CenterIndented, 16, 2, 2, "None", "0.45");
    if (UINT8_MAX != idx)
    {
        _gamma = (0==idx) ? 1.0f : 1.0f/2.2f;
    }
}

static void imguiTabsFov(float& _fov)
{
    uint8_t idx;
    if (45.0f == _fov)
    {
        idx = 0;
    }
    else if (60.0f == _fov)
    {
        idx = 1;
    }
    else if (90.0f == _fov)
    {
        idx = 2;
    }
    else
    {
        idx = UINT8_MAX;
    }

    idx = imguiTabs(idx, true, ImguiAlign::LeftIndented, 16, 2, 3, "45", "60", "90");
    if (UINT8_MAX != idx)
    {
        static const float sc_fov[3] = { 45.0f, 60.0f, 90.0f };
        _fov = sc_fov[idx];
    }
}

// For input { 0.0f, 0.0f, 1.0f, 0.0f } returns 2.
static uint8_t getIdx(const float* _f4)
{
    const bool selected[4] =
    {
        1.0f == _f4[0],
        1.0f == _f4[1],
        1.0f == _f4[2],
        1.0f == _f4[3],
    };

    uint8_t idx;
    if (selected[3])
    {
        idx = 3;
    }
    else if (selected[2])
    {
        idx = 2;
    }
    else if (selected[1])
    {
        idx = 1;
    }
    else //(selected[0]).
    {
        idx = 0;
    }

    return idx;
}

static void imguiTexSwizzle(float* _swizz, bool _enabled = true)
{
    const uint8_t idx = imguiTabs(getIdx(_swizz), _enabled, ImguiAlign::CenterIndented, 14, 4, 4, 0, "R", "G", "B", "A");

    _swizz[0] = 0.0f;
    _swizz[1] = 0.0f;
    _swizz[2] = 0.0f;
    _swizz[3] = 0.0f;

    _swizz[idx] = 1.0f;
}

static bool imguiConfirmButton(const char* _btn
                             , const char* _btnCancel
                             , const char* _confirm
                             , bool& _pressed
                             , bool _enabled = true
                             , ImguiAlign::Enum _align = ImguiAlign::LeftIndented
                             )
{
    if (imguiButton(_pressed?_btnCancel:_btn, _enabled, _align))
    {
        _pressed = !_pressed;
    }

    if (_pressed)
    {
        imguiIndent();
        const bool confirmed = imguiButton(_confirm, true, _align, imguiRGBA(255, 0, 0, 0), _enabled);
        imguiUnindent();

        if (confirmed)
        {
            _pressed = false;
        }

        return confirmed;
    }

    return false;
}

bool imguiDirectoryLabel(const char* _directory, uint16_t _width = 32)
{
    const uint16_t end = (uint16_t)strlen(_directory);
    const uint16_t pos = (uint16_t)dm::max(0, end-_width);

    if (0 == pos)
    {
        imguiLabel(_directory);
        imguiSeparator(4);
    }
    else
    {
        char label[64];
        bx::snprintf(label, sizeof(label), "...%s", &_directory[pos]);

        return imguiButton(label, true, ImguiAlign::CenterIndented);
    }

    return false;
}

bool imguiCube(bgfx::TextureHandle _cubemap, float _lod, ImguiCubemap::Enum _display, int32_t _areaWidth, bool _enabled = true)
{
    const bool isHexOrLatlong = (ImguiCubemap::Hex == _display) || (ImguiCubemap::Latlong == _display);
    const uint16_t previewWidth = uint16_t(_areaWidth-16);
    const uint16_t padding = previewWidth/8-3;

    if (isHexOrLatlong) { imguiSeparator(padding); }
    const bool result = imguiCube(_cubemap, _lod, _display, false, ImguiAlign::CenterIndented, _enabled);
    if (isHexOrLatlong) { imguiSeparator(padding); }

    return result;
}

// Resources.
//-----

struct GuiResorces
{
    ImguiFontHandle m_fonts[Fonts::Count];
    bgfx::TextureHandle m_texSunIcon;
};
static GuiResorces s_res;

void initFonts()
{
    #define FONT_DESC_FILE(_name, _fontSize, _path)                     \
    {                                                                   \
        void* data = load(_path);                                       \
        s_res.m_fonts[Fonts::_name] = imguiCreateFont(data, _fontSize); \
        free(data);                                                     \
    }

    #define FONT_DESC_MEM(_name, _fontSize, _data)                       \
    {                                                                    \
        s_res.m_fonts[Fonts::_name] = imguiCreateFont(_data, _fontSize); \
    }

    #include "gui_res.h"
}

void imguiSetFont(Fonts::Enum _font)
{
    imguiSetFont(s_res.m_fonts[_font]);
}

float imguiGetTextLength(const char* _text, Fonts::Enum _font)
{
    return imguiGetTextLength(_text, s_res.m_fonts[_font]);
}

void guiInit()
{
    #define RES_DESC(_defaultFont, _defaultFontDataSize, _defaultFontSize, _sunIconData, _sunIconDataSize) \
        const void*    fontData        = _defaultFont;                                                     \
        const uint32_t fontDataSize    = _defaultFontDataSize;                                             \
        const float    fontSize        = _defaultFontSize;                                                 \
        const void*    sunIconData     = _sunIconData;                                                     \
        const uint32_t sunIconDataSize = _sunIconDataSize;
    #include "gui_res.h"

    // Init imgui.
    s_res.m_fonts[Fonts::Default] = imguiCreate(fontData, fontDataSize, fontSize, dm::mainAlloc);

    // Init fonts.
    initFonts();

    // Init textures.
    const bgfx::Memory* mem = bgfx::makeRef(sunIconData, sunIconDataSize);
    s_res.m_texSunIcon = bgfx::createTexture(mem, BGFX_TEXTURE_U_CLAMP|BGFX_TEXTURE_V_CLAMP);
}

void guiDrawOverlay()
{
    screenQuad(0, 0, g_guiWidth, g_guiHeight);
    bgfx::setState(BGFX_STATE_RGB_WRITE
                  |BGFX_STATE_ALPHA_WRITE
                  |BGFX_STATE_BLEND_ALPHA
                  );
    bgfx_submit(RenderPipeline::ViewIdGui, cs::Program::Overlay);
}

void guiDestroy()
{
    imguiDestroy();
}

// FileBrowser widget.
//-----

void imguiTextureBrowserWidget(int32_t _x
                             , int32_t _y
                             , int32_t _width
                             , TextureBrowserWidgetState& _state
                             , bool _enabled
                             )
{
    enum
    {
        ListVisibleElements = 6,
        ListElementHeight   = 24,
        ListHeight = ListVisibleElements*ListElementHeight,
    };

    const int32_t browserHeight = 400;
    const int32_t height = 293 + ListHeight + browserHeight;

    _state.m_events = GuiEvent::None;

    imguiBeginArea("Texture", _x, _y, _width, height, true);
    imguiSeparator(7);

    imguiLabelBorder("Directory:");
    imguiIndent();
    {
        if (imguiDirectoryLabel(_state.m_directory))
        {
            _state.m_events = GuiEvent::GuiUpdate;
        }
    }
    imguiSeparator();
    imguiUnindent();

    imguiLabelBorder("Filter:");
    imguiIndent();
    {
        imguiBeginScroll(ListHeight, &_state.m_scroll, _enabled);
        {
            const uint8_t selection = imguiTabs(UINT8_MAX, true, ImguiAlign::LeftIndented, 21, 4, 2, "Select all", "Select none");
            if (0 == selection)
            {
                for (uint8_t ii = 0; ii < TextureExt::Count; ++ii)
                {
                    _state.m_extFlag[ii] = true;
                }
            }
            else if (1 == selection)
            {
                for (uint8_t ii = 0; ii < TextureExt::Count; ++ii)
                {
                    _state.m_extFlag[ii] = false;
                }
            }

            char extLabel[FileExtensionLength+2];
            for (uint8_t ii = 0; ii < TextureExt::Count; ++ii)
            {
                extLabel[0] = '*';
                extLabel[1] = '.';
                strcpy(&extLabel[2], getTextureExtensionStr(ii));

                imguiBool(extLabel, _state.m_extFlag[ii]);
            }

            imguiSeparator(2);
        }
        imguiEndScroll();
    }
    imguiSeparator();
    imguiUnindent();

    imguiLabelBorder("Browse:");
    imguiIndent();
    {
        uint8_t count = 0;
        char extensions[TextureExt::Count][FileExtensionLength];
        for (uint8_t ii = 0; ii < TextureExt::Count; ++ii)
        {
            if (_state.m_extFlag[ii])
            {
                strcpy(extensions[count++], getTextureExtensionStr(ii));
            }
        }
        imguiBrowser(browserHeight, _state, extensions, count);
    }
    imguiUnindent();

    imguiLabelBorder("Action:");
    imguiIndent();
    {
        if (imguiButton("Load", true, ImguiAlign::CenterIndented))
        {
            _state.m_events = GuiEvent::HandleAction | GuiEvent::DismissWindow;
        }
        if (imguiButton("Close", true, ImguiAlign::CenterIndented))
        {
            _state.m_events = GuiEvent::DismissWindow;
        }
    }

    imguiEndArea();
}

// MeshSave widget.
//-----

void imguiMeshSaveWidget(int32_t _x
                       , int32_t _y
                       , int32_t _width
                       , MeshSaveWidgetState& _state
                       )
{
    const int32_t browserHeight = 352;
    const int32_t height = browserHeight + 445;

    _state.m_events = GuiEvent::None;

    imguiBeginArea("Save mesh", _x, _y, _width, height, true);
    imguiSeparator(7);

    imguiLabelBorder("Directory:");
    imguiIndent();
    {
        if (imguiDirectoryLabel(_state.m_directory))
        {
            _state.m_events = GuiEvent::GuiUpdate;
        }
    }
    imguiSeparator();
    imguiUnindent();

    imguiLabelBorder("Filter:");
    imguiIndent();
    {
        imguiCheck("*.bin (bgfx geometry binary format)", true, false);
    }
    imguiSeparator();
    imguiUnindent();

    imguiLabelBorder("Browse:");
    imguiIndent();
    {
        char select[128+6];
        bx::snprintf(select, sizeof(select), "%s.bin", _state.m_outputName);
        const char extensions[1][FileExtensionLength] = { "bin" };
        const bool fileClicked = imguiBrowser(browserHeight, _state, extensions, 1, select);
        if (fileClicked)
        {
            dm::strscpya(_state.m_outputName, _state.m_fileName);
        }
    }
    imguiUnindent();

    char projName[128+6];
    bx::snprintf(projName, sizeof(projName), "%s.bin", _state.m_outputName);

    imguiLabelBorder("Output file:");
    imguiIndent();
    {
        imguiLabel(projName);
    }
    imguiSeparator();
    imguiUnindent();

    imguiLabelBorder("Name:");
    imguiIndent();
    {
        imguiInput(NULL, _state.m_outputName, 128, true, ImguiAlign::CenterIndented);
    }
    imguiSeparator();
    imguiUnindent();

    imguiLabelBorder("Action:");
    imguiIndent();

    if (imguiButton("Save", true, ImguiAlign::CenterIndented))
    {
        _state.m_events = GuiEvent::HandleAction | GuiEvent::DismissWindow;
    }

    if (imguiButton("Close", true, ImguiAlign::CenterIndented))
    {
        _state.m_events = GuiEvent::DismissWindow;
    }

    imguiEndArea();
}

// MeshBrowser widget.
//-----

void imguiMeshBrowserWidget(int32_t _x
                          , int32_t _y
                          , int32_t _width
                          , MeshBrowserState& _state
                          )
{
    _state.m_events = GuiEvent::None;

    const char* loaderExt[CS_MAX_GEOMETRY_LOADERS];
    cs::geometryLoaderGetExtensions(loaderExt);
    const uint8_t loaderCount = cs::geometryLoaderCount();

    const int32_t browserHeight = 300;
    const int32_t height = browserHeight + 449 + loaderCount*24;

    imguiBeginArea("Load mesh", _x, _y, _width, height, true);
    imguiSeparator(7);

    imguiLabelBorder("Directory:");
    imguiIndent();
    {
        if (imguiDirectoryLabel(_state.m_directory))
        {
            _state.m_events = GuiEvent::GuiUpdate;
        }
    }
    imguiSeparator();
    imguiUnindent();

    imguiLabelBorder("Filter:");
    imguiIndent();
    {
        char extLabel[FileExtensionLength+2];
        for (uint8_t ii = 0; ii < loaderCount; ++ii)
        {
            extLabel[0] = '*';
            extLabel[1] = '.';
            dm::strscpy(&extLabel[2], loaderExt[ii], FileExtensionLength);

            imguiBool(extLabel, _state.m_extFlag[ii]);
        }
    }
    imguiSeparator(8);
    imguiUnindent();

    imguiLabelBorder("Browse:");
    imguiIndent();
    {
        uint8_t count = 0;
        char extensions[CS_MAX_GEOMETRY_LOADERS][FileExtensionLength];
        for (uint8_t ii = 0; ii < loaderCount; ++ii)
        {
            if (_state.m_extFlag[ii])
            {
                dm::strscpy(extensions[count++], loaderExt[ii], FileExtensionLength);
            }
        }
        imguiBrowser(browserHeight, _state, extensions, count);
    }
    imguiUnindent();

    imguiLabelBorder("Filename:");
    imguiIndent();
    {
        imguiLabel('\0' == _state.m_fileNameExt[0] ? "<invalid>" : _state.m_fileNameExt);
    }
    imguiSeparator();
    imguiUnindent();

    imguiLabelBorder("Obj conversion:");
    imguiIndent();
    {
        imguiBool("FlipV", _state.m_flipV, _state.m_objSelected);
        imguiBool("CCW",   _state.m_ccw,   _state.m_objSelected);
    }
    imguiSeparator();
    imguiUnindent();

    imguiLabelBorder("Action:");
    imguiIndent();
    {
        if (imguiButton("Load", loaderCount>0, ImguiAlign::CenterIndented))
        {
            _state.m_events = GuiEvent::HandleAction | GuiEvent::DismissWindow;
        }

        if (imguiButton("Close", true, ImguiAlign::CenterIndented))
        {
            _state.m_events = GuiEvent::DismissWindow;
        }
    }

    imguiEndArea();
}

// EnvMap widget.
//-----

void imguiEnvMapWidget(cs::EnvHandle _env
                     , int32_t _x
                     , int32_t _y
                     , int32_t _width
                     , EnvMapWidgetState& _state
                     , bool _enabled
                     )
{
    const int32_t height = 934;
    const cs::Environment& env = cs::getObj(_env);

    _state.m_events = GuiEvent::None;

    char title[64] = "Environment";
    const char* name = cs::getName(_env);
    if (NULL != name
    &&  '\0' != name[0])
    {
        bx::snprintf(title, sizeof(title), "Environment - %s", name);
    }

    imguiBeginArea(title, _x, _y, _width, height, _enabled);
    imguiSeparator(7);

    imguiLabelBorder("Skybox:");
    imguiIndent();
    {
        const bgfx::TextureHandle texture = cs::textureGetBgfxHandle(env.m_cubemap[cs::Environment::Skybox]);
        const bool clicked = imguiCube(texture, 0.0f, ImguiCubemap::Latlong, true, ImguiAlign::CenterIndented);

        if (imguiButton("Info", true, ImguiAlign::CenterIndented) || clicked)
        {
            _state.m_selection = cs::Environment::Skybox;
            _state.m_action = EnvMapWidgetState::Info;
            _state.m_events = GuiEvent::GuiUpdate;
        }

        if (imguiButton("Transform", true, ImguiAlign::CenterIndented))
        {
            _state.m_selection = cs::Environment::Skybox;
            _state.m_action = EnvMapWidgetState::Transform;
            _state.m_events = GuiEvent::GuiUpdate;
        }

        if (imguiButton("Tonemap", true, ImguiAlign::CenterIndented))
        {
            _state.m_selection = cs::Environment::Skybox;
            _state.m_action = EnvMapWidgetState::Process;
            _state.m_events = GuiEvent::GuiUpdate;
        }

        const bool validImage = cmft::imageIsValid(env.m_cubemapImage[cs::Environment::Skybox]);
        if (imguiButton("Save", validImage, ImguiAlign::CenterIndented))
        {
            _state.m_selection = cs::Environment::Skybox;
            _state.m_action = EnvMapWidgetState::Save;
            _state.m_events = GuiEvent::GuiUpdate;
        }

        if (imguiButton("Browse...", true, ImguiAlign::CenterIndented))
        {
            _state.m_selection = cs::Environment::Skybox;
            _state.m_action = EnvMapWidgetState::Browse;
            _state.m_events = GuiEvent::GuiUpdate;
        }
    }
    imguiSeparator();
    imguiUnindent();

    imguiLabelBorder("Radiance:");
    imguiIndent();
    {
        const float maxMip = float(cs::getObj(env.m_cubemap[cs::Environment::Pmrem]).m_numMips-1);
        _state.m_lod = dm::min(_state.m_lod, maxMip);

        const bgfx::TextureHandle texture = cs::textureGetBgfxHandle(env.m_cubemap[cs::Environment::Pmrem]);
        const bool clicked = imguiCube(texture, _state.m_lod, ImguiCubemap::Latlong, true, ImguiAlign::CenterIndented);

        imguiSlider("Lod", _state.m_lod, 0.0f, maxMip, 0.1f, true, ImguiAlign::CenterIndented);

        if (imguiButton("Info", true, ImguiAlign::CenterIndented) || clicked)
        {
            _state.m_selection = cs::Environment::Pmrem;
            _state.m_action = EnvMapWidgetState::Info;
            _state.m_events = GuiEvent::GuiUpdate;
        }

        if (cmft::imageIsValid(env.m_cubemapImage[cs::Environment::Skybox]))
        {
            if (imguiButton("Filter skybox with cmft", true, ImguiAlign::CenterIndented))
            {
                _state.m_selection = cs::Environment::Pmrem;
                _state.m_action = EnvMapWidgetState::Process;
                _state.m_events = GuiEvent::GuiUpdate;
            }
        }
        else
        {
            imguiButton("Filter with cmft (load skybox first!)", false, ImguiAlign::CenterIndented);
        }

        const bool validImage = cmft::imageIsValid(env.m_cubemapImage[cs::Environment::Pmrem]);
        if (imguiButton("Save", validImage, ImguiAlign::CenterIndented))
        {
            _state.m_selection = cs::Environment::Pmrem;
            _state.m_action = EnvMapWidgetState::Save;
            _state.m_events = GuiEvent::GuiUpdate;
        }

        if (imguiButton("Browse...", true, ImguiAlign::CenterIndented))
        {
            _state.m_selection = cs::Environment::Pmrem;
            _state.m_action = EnvMapWidgetState::Browse;
            _state.m_events = GuiEvent::GuiUpdate;
        }
    }
    imguiSeparator();
    imguiUnindent();

    imguiLabelBorder("Irradiance:");
    imguiIndent();
    {
        const bgfx::TextureHandle texture = cs::textureGetBgfxHandle(env.m_cubemap[cs::Environment::Iem]);
        const bool clicked = imguiCube(texture, 0.0f, ImguiCubemap::Latlong, true, ImguiAlign::CenterIndented);

        if (imguiButton("Info", true, ImguiAlign::CenterIndented) || clicked)
        {
            _state.m_selection = cs::Environment::Iem;
            _state.m_action = EnvMapWidgetState::Info;
            _state.m_events = GuiEvent::GuiUpdate;
        }

        if (cmft::imageIsValid(env.m_cubemapImage[cs::Environment::Skybox]))
        {
            if (imguiButton("Filter skybox with cmft", true, ImguiAlign::CenterIndented))
            {
                _state.m_selection = cs::Environment::Iem;
                _state.m_action = EnvMapWidgetState::Process;
                _state.m_events = GuiEvent::GuiUpdate;
            }
        }
        else
        {
            imguiButton("Filter with cmft (load skybox first!)", false, ImguiAlign::CenterIndented);
        }

        const bool validImage = cmft::imageIsValid(env.m_cubemapImage[cs::Environment::Iem]);
        if (imguiButton("Save", validImage, ImguiAlign::CenterIndented))
        {
            _state.m_selection = cs::Environment::Iem;
            _state.m_action = EnvMapWidgetState::Save;
            _state.m_events = GuiEvent::GuiUpdate;
        }

        if (imguiButton("Browse...", true, ImguiAlign::CenterIndented))
        {
            _state.m_selection = cs::Environment::Iem;
            _state.m_action = EnvMapWidgetState::Browse;
            _state.m_events = GuiEvent::GuiUpdate;
        }
    }
    imguiSeparator();
    imguiUnindent();

    imguiIndent();
    imguiSeparator();
    if (imguiButton("Close", true, ImguiAlign::CenterIndented))
    {
        _state.m_events = GuiEvent::DismissWindow;
    }

    imguiEndArea();
}

// CmftImageInfo widget.
//-----

void imguiCmftInfoWidget(cs::EnvHandle _env
                       , int32_t _x
                       , int32_t _y
                       , int32_t _width
                       , CmftInfoWidgetState& _state
                       , bool _enabled
                       )
{
    const cs::Environment& env = cs::getObj(_env);
    const cmft::Image& image = env.m_cubemapImage[_state.m_envType];
    const bgfx::TextureHandle texture = cs::textureGetBgfxHandle(env.m_cubemap[_state.m_envType]);

    const bool isSkybox = (cs::Environment::Skybox == _state.m_envType);
    const bool hasLod = (image.m_numMips!=1);
    const int32_t resizeHeight = isSkybox ? 134 : 0;
    const int32_t lodBarHeight = hasLod ? 28 : 0;
    const int32_t height = 714 + resizeHeight + lodBarHeight;

    _state.m_events = GuiEvent::None;

    const char* areaTitle = getCubemapTypeStr(_state.m_envType);
    imguiBeginArea(areaTitle, _x, _y, _width, height, _enabled);
    imguiSeparator(7);

    imguiLabelBorder("Preview:");
    imguiIndent();
    {
        if (imguiCube(texture, _state.m_lod, (ImguiCubemap::Enum)_state.m_imguiCubemap, _width))
        {
            _state.m_imguiCubemap = ImguiCubemap::Enum((_state.m_imguiCubemap+1) % ImguiCubemap::Count);
        }

        if (hasLod)
        {
            imguiSeparator(4);
            imguiSlider("Lod", _state.m_lod, 0.0f, dm::utof(image.m_numMips-1), 0.1f, true, ImguiAlign::CenterIndented);
        }

        _state.m_imguiCubemap = (uint8_t)imguiTabs(_state.m_imguiCubemap, true, ImguiAlign::CenterIndented, 16, 2, 3, "Cross", "Latlong", "Hex");

        if (imguiButton("Magnify", true, ImguiAlign::CenterIndented))
        {
            _state.m_env = _env;
            _state.m_action = CmftInfoWidgetState::Magnify;
            _state.m_events = GuiEvent::GuiUpdate;
        }
    }
    imguiUnindent();
    imguiSeparator();

    imguiLabelBorder("Proprieties:");
    imguiIndent();
    {
        ImguiFontScope fontScope(Fonts::DroidSansMono);

        imguiLabel("Width:     %23u", image.m_width);
        imguiLabel("Height:    %23u", image.m_height);
        imguiLabel("Faces:     %23u", image.m_numFaces);
        imguiLabel("Mip count: %23u", image.m_numMips);
        imguiLabel("Format:    %23s", cmft::getTextureFormatStr(image.m_format));
        imguiLabel("Size:      %17u.%03uMB", dm::asMBInt(image.m_dataSize), dm::asMBDec(image.m_dataSize)%1000, image.m_dataSize);
    }
    imguiSeparator();
    imguiUnindent();

    if (isSkybox)
    {
        imguiLabelBorder("Resize:");
        imguiIndent();
        {
            const float currentSize = dm::utof(image.m_width);
            _state.m_resize = dm::min(_state.m_resize, currentSize);

            imguiLabel("Current size: %u", image.m_width);
            imguiSlider("New size", _state.m_resize, 32.0f, currentSize, 1.0f, true, ImguiAlign::CenterIndented);
            imguiTabsSize(_state.m_resize, dm::utof(image.m_width));

            if (imguiButton("Resize", image.m_width != uint32_t(_state.m_resize), ImguiAlign::CenterIndented))
            {
                _state.m_env = _env;
                _state.m_action = CmftInfoWidgetState::Resize;
                _state.m_events = GuiEvent::HandleAction | GuiEvent::DismissWindow;
            }
        }
        imguiSeparator();
        imguiUnindent();
    }

    imguiLabelBorder("Convert to:");
    imguiIndent();
    {
        _state.m_convertTo = (CmftInfoWidgetState::ImageFormat)imguiChoose(_state.m_convertTo, "BGRA8", "RGBA16", "RGBA16F", "RGBA32F");

        const bool enabled = _state.selectedCmftFormat() != image.m_format;
        if (imguiButton("Convert", enabled, ImguiAlign::CenterIndented))
        {
            _state.m_env = _env;
            _state.m_action = CmftInfoWidgetState::Convert;
            _state.m_events = GuiEvent::HandleAction;
        }
    }
    imguiSeparator();
    imguiUnindent();

    imguiLabelBorder("Action:");
    imguiIndent();
    if (imguiButton("Close", true, ImguiAlign::CenterIndented))
    {
        _state.m_events = GuiEvent::DismissWindow;
    }

    imguiEndArea();
}

// CmftSave widget.
//-----

void imguiCmftSaveWidget(int32_t _x
                       , int32_t _y
                       , int32_t _width
                       , CmftSaveWidgetState& _state
                       , bool _enabled
                       )
{
    enum
    {
        HeightFileType   =  96,
        HeightOutputType = 173,
        HeightFormat     = 192,
        HeightBrowser    = 517,
        HeightBrowserTab = 672,
        Height           = 773,
    };

    _state.m_events = GuiEvent::None;

    const char s_extensions[4][FileExtensionLength] = { "dds", "ktx", "tga", "hdr" };
    bx::snprintf(_state.m_outputNameExt, sizeof(_state.m_outputNameExt)
               , "%s.%s", _state.m_outputName, s_extensions[_state.m_selectedFileType]);

    const char* areaTitle = getCubemapTypeStr(_state.m_envType);
    imguiBeginArea(areaTitle, _x, _y, _width, HeightBrowserTab, _enabled);
    imguiSeparator(4);

    imguiLabelBorder("Directory:");
    imguiIndent();
    {
        imguiSeparator(2);
        if (imguiDirectoryLabel(_state.m_directory))
        {
            _state.m_events = GuiEvent::GuiUpdate;
        }
        imguiSeparator(2);
    }
    imguiSeparator();
    imguiUnindent();

    imguiLabelBorder("Change directory:");
    imguiIndent();
    {
        imguiSeparator(4);
        const bool fileClicked = imguiBrowser(HeightBrowser, _state, s_extensions, 4, _state.m_outputNameExt);
        if (fileClicked)
        {
            dm::strscpya(_state.m_outputName, _state.m_fileName);
            for (uint16_t ii = 0; ii < 4; ++ii)
            {
                if (0 == strncmp(_state.m_fileExt, s_extensions[ii], 3))
                {
                    _state.m_selectedFileType = ii;
                }
            }
        }
    }
    imguiSeparator();
    imguiUnindent();

    imguiEndArea();

    imguiBeginArea(areaTitle, _x+_width, _y, _width, Height, _enabled);
    imguiSeparator(4);

    imguiLabelBorder("Name:");
    imguiIndent();
    {
        imguiInput("", _state.m_outputName, 128, true, ImguiAlign::CenterIndented);
    }
    imguiSeparator();
    imguiUnindent();

    imguiLabelBorder("File type:");
    imguiIndent();
    {
        imguiBeginScroll(HeightFileType, &_state.m_scrollFileType, true);
        for (uint16_t ii = 0; ii < cmft::ImageFileType::Count; ++ii)
        {
            const cmft::ImageFileType::Enum ft = cmft::ImageFileType::Enum(ii);
            imguiCheckSelection(cmft::getFilenameExtensionStr(ft), _state.m_selectedFileType, ii);
        }
        imguiEndScroll();
    }
    imguiUnindent();

    // Save file type selection.
    _state.m_fileType = cmft::ImageFileType::Enum(_state.m_selectedFileType);

    imguiLabelBorder("Output type:");
    imguiIndent();
    {
        imguiBeginScroll(HeightOutputType, &_state.m_scrollOutputType, true);

        const cmft::OutputType::Enum* validOutputTypes = cmft::getValidOutputTypes(_state.m_fileType);

        uint16_t idx = 0;
        for ( ; validOutputTypes[idx] != cmft::OutputType::Null; ++idx)
        {
            imguiCheckSelection(cmft::getOutputTypeStr(validOutputTypes[idx]), _state.m_selectedOutputType, idx);
        }

        if (idx < _state.m_selectedOutputType)
        {
            _state.m_selectedOutputType = 0;
        }

        // Save output type selection.
        _state.m_outputType = cmft::getValidOutputTypes(_state.m_fileType)[_state.m_selectedOutputType];

        imguiEndScroll();
    }
    imguiUnindent();

    imguiLabelBorder("Format:");
    imguiIndent();
    {
        imguiBeginScroll(HeightFormat, &_state.m_scrollFormat, true);

        const cmft::TextureFormat::Enum* validTextureFormats = cmft::getValidTextureFormats(_state.m_fileType);

        uint16_t idx = 0;
        for ( ; validTextureFormats[idx] != cmft::TextureFormat::Null; ++idx)
        {
            imguiCheckSelection(cmft::getTextureFormatStr(validTextureFormats[idx]), _state.m_selectedTextureFormat, idx);
        }

        if (idx < _state.m_selectedTextureFormat)
        {
            _state.m_selectedTextureFormat = 0;
        }

        // Save texture format selection.
        _state.m_textureFormat = cmft::getValidTextureFormats(_state.m_fileType)[_state.m_selectedTextureFormat];

        imguiEndScroll();
    }
    imguiUnindent();

    imguiLabelBorder("Action:");
    imguiIndent();

    if (imguiButton("Save", true, ImguiAlign::CenterIndented))
    {
        _state.m_events = GuiEvent::HandleAction | GuiEvent::DismissWindow;
    }

    if (imguiButton("Cancel", true, ImguiAlign::CenterIndented))
    {
        _state.m_events = GuiEvent::DismissWindow;
    }

    imguiEndArea();
}

// CmftPmrem widget.
//-----

void imguiCmftPmremWidget(cs::EnvHandle _env
                        , int32_t _x
                        , int32_t _y
                        , int32_t _width
                        , CmftPmremWidgetState& _state
                        , bool _enabled
                        )
{
    const int32_t pmremWidth = _width;
    const int32_t pmremHeight = 1017-3-9;
    const cs::Environment& env = cs::getObj(_env);

    _state.m_events = GuiEvent::None;

    char resultStr[128];
    bool warn = false;

    imguiBeginArea("cmft - Radiance filter", _x, _y, pmremWidth, pmremHeight, _enabled);
    imguiSeparator(7);

    imguiLabelBorder("Filter options:");
    imguiIndent();
    {
        char faceSize[128];
        bx::snprintf(faceSize, 128, "Face size: %u", env.m_cubemapImage[cs::Environment::Skybox].m_width);
        imguiLabel(faceSize);
        imguiSeparator(6);

        const float skyboxSize = dm::utof(env.m_cubemapImage[cs::Environment::Skybox].m_width);
        _state.m_srcSize = dm::min(_state.m_srcSize, skyboxSize);

        imguiSlider("Resize source", _state.m_srcSize, 64.0f, skyboxSize, 1.0f, true, ImguiAlign::CenterIndented);
        imguiTabsSize(_state.m_srcSize, skyboxSize);
        if (imguiCheck("No resize", skyboxSize == _state.m_srcSize))
        {
            _state.m_srcSize = skyboxSize;
        }
        imguiSeparator();

        const float maxNumMipf = dm::log2f(_state.m_dstSize) + 1.0f;
        _state.m_mipCount = dm::min(_state.m_mipCount, maxNumMipf);
        imguiSlider("Result size", _state.m_dstSize, 32.0f, 1024.0f, 1.0f, true, ImguiAlign::CenterIndented);
        imguiTabsSize(_state.m_dstSize);
        imguiSlider("Num mipmap to filter", _state.m_mipCount, 1.0f, maxNumMipf, 1.0f, true, ImguiAlign::CenterIndented);
        imguiBool("Exclude base", _state.m_excludeBase);
        imguiSeparator(3);

        imguiLabel(imguiRGBA(128,128,128,196), " (slow)\t                  (fast)");
        imguiSlider("Gloss scale", _state.m_glossScale, 1.0f, 12.0f, 1.0f, true, ImguiAlign::CenterIndented);
        imguiSlider("Gloss bias",  _state.m_glossBias,  0.0f,  6.0f, 1.0f, true, ImguiAlign::CenterIndented);
        imguiSeparator();

        strcpy(resultStr, "|");
        int32_t num = 1;

        if (_state.m_excludeBase)
        {
            strcat(resultStr, "E|");
            num += 2;
        }

        int32_t ii = int32_t(_state.m_excludeBase);
        for (int32_t end = int32_t(_state.m_mipCount); ii < end; ++ii)
        {
            const float specPowerRef = cmft::specularPowerFor(float(ii)
                                                            , float(_state.m_mipCount)
                                                            , float(_state.m_glossScale)
                                                            , float(_state.m_glossBias)
                                                            );
            const float specPower = cmft::applyLightningModel(specPowerRef, (cmft::LightingModel::Enum)_state.m_lightingModel);
            num += sprintf(resultStr+num, "%d|", int32_t(specPower));
        }

        const int32_t maxNumMip = int32_t(maxNumMipf);
        if (maxNumMip-ii < 2)
        {
            // Warn user not to use 2x2 and 1x1 cubemap sizes.
            warn = true;
        }

        for (int32_t end = maxNumMip; ii < end; ++ii)
        {
            strcat(resultStr, "-|");
        }

        if (num > 33)
        {
            strcpy(resultStr+33,"...");
        }
    }
    imguiUnindent();

    imguiLabelBorder("Lighting model:");
    imguiIndent();
    {
        _state.m_lightingModel = (uint8_t)imguiChoose(_state.m_lightingModel
                                                     , "Phong"
                                                     , "PhongBrdf"
                                                     , "Blinn"
                                                     , "BlinnBrdf"
                                                     );
        imguiSeparator(6);
    }
    imguiUnindent();

    imguiLabelBorder("Result:");
    imguiIndent();
    {
        imguiLabel(warn?imguiRGBA(220,0,0,196):imguiRGBA(255,255,255,255), resultStr);
        imguiLabel(imguiRGBA(128,128,128,196), "  E     No filter");
        imguiLabel(imguiRGBA(128,128,128,196), "{N}   Filter specular power");
        imguiLabel(imguiRGBA(128,128,128,196), "  -     Discarded mip map");
        imguiSeparator(6);
    }
    imguiUnindent();

    imguiLabelBorder("Edge fixup:");
    imguiIndent();
    {
        const uint8_t before = _state.m_edgeFixup;
        _state.m_edgeFixup = (uint8_t)imguiTabs(_state.m_edgeFixup, true, ImguiAlign::CenterIndented, 16, 2, 2, "None", "Warp");
        if (1 == _state.m_edgeFixup && _state.m_edgeFixup != before)
        {
            // Warp was clicked.
            _state.m_events = GuiEvent::GuiUpdate;
        }
        imguiSeparator(9);
    }
    imguiUnindent();

    imguiLabelBorder("Processing hardware:");
    imguiIndent();
    {
        imguiBool("Use OpenCL", _state.m_useOpenCL);
        imguiSlider("Num CPU processing threads",  _state.m_numCpuThreads,  0.0f, 12.0f, 1.0f, true, ImguiAlign::CenterIndented);
        imguiSeparator();
    }
    imguiUnindent();

    imguiLabelBorder("Gamma:");
    imguiIndent();
    {
        imguiSlider("Apply gamma before processing", _state.m_inputGamma, 0.01f, 4.0f, 0.01f, true, ImguiAlign::CenterIndented);
        if (fabs(_state.m_inputGamma-(2.2f)) < 0.1f)
        {
            _state.m_inputGamma = 2.2f;
        }
        imguiTabsToLinear(_state.m_inputGamma);
        imguiSeparator();

        imguiSlider("Apply gamma after processing", _state.m_outputGamma, 0.01f, 4.0f, 0.01f, true, ImguiAlign::CenterIndented);
        if (fabs(_state.m_outputGamma-(1.0f/2.2f)) < 0.05f)
        {
            _state.m_outputGamma = 1.0f/2.2f;
        }
        imguiTabsToGamma(_state.m_outputGamma);
        imguiSeparator();
    }
    imguiUnindent();

    imguiLabelBorder("Action:");
    imguiIndent();

    if (imguiButton("Process", true, ImguiAlign::CenterIndented))
    {
        _state.m_events = GuiEvent::HandleAction | GuiEvent::DismissWindow;
    }

    if (imguiButton("Cancel", true, ImguiAlign::CenterIndented))
    {
        _state.m_events = GuiEvent::DismissWindow;
    }

    imguiEndArea();
}

// CmftIem widget.
//-----

void imguiCmftIemWidget(cs::EnvHandle _env
                      , int32_t _x
                      , int32_t _y
                      , int32_t _width
                      , CmftIemWidgetState& _state
                      , bool _enabled
                      )
{
    const int32_t iemWidth = _width;
    const int32_t iemHeight = 513;
    const cs::Environment& env = cs::getObj(_env);

    _state.m_events = GuiEvent::None;

    imguiBeginArea("cmft - Irradiance filter", _x, _y, iemWidth, iemHeight, _enabled);
    imguiSeparator(7);

    imguiLabelBorder("Skybox:");
    imguiIndent();
    {
        char faceSize[128];
        bx::snprintf(faceSize, 127, "Face size: %u", env.m_cubemapImage[cs::Environment::Skybox].m_width);
        imguiLabel(faceSize);
        imguiSeparator();
    }
    imguiUnindent();

    imguiLabelBorder("Filter options:");
    imguiIndent();
    {
        const float skyboxSize = dm::utof(env.m_cubemapImage[cs::Environment::Skybox].m_width);
        _state.m_srcSize = dm::min(_state.m_srcSize, skyboxSize);

        imguiSlider("Resize source", _state.m_srcSize, 64.0f, skyboxSize, 1.0f, true, ImguiAlign::CenterIndented);
        imguiTabsSize(_state.m_srcSize, skyboxSize);
        if (imguiCheck("No resize", skyboxSize == _state.m_srcSize))
        {
            _state.m_srcSize = skyboxSize;
        }
        imguiSeparator();

        imguiSlider("Destination size", _state.m_dstSize, 32.0f, 2048.0f, 1.0f, true, ImguiAlign::CenterIndented);
        imguiTabsSize(_state.m_dstSize);
        imguiSeparator();
    }
    imguiUnindent();

    imguiLabelBorder("Gamma:");
    imguiIndent();
    {
        imguiSlider("Apply gamma before processing", _state.m_inputGamma, 0.01f, 4.0f, 0.01f, true, ImguiAlign::CenterIndented);
        if (dm::equals(_state.m_inputGamma, 2.2f, 0.1f))
        {
            _state.m_inputGamma = 2.2f;
        }
        imguiTabsToLinear(_state.m_inputGamma);
        imguiSeparator();

        imguiSlider("Apply gamma after processing", _state.m_outputGamma, 0.01f, 4.0f, 0.01f, true, ImguiAlign::CenterIndented);
        if (dm::equals(_state.m_outputGamma, 1.0f/2.2f, 0.05f))
        {
            _state.m_outputGamma = 1.0f/2.2f;
        }
        imguiTabsToGamma(_state.m_outputGamma);
        imguiSeparator();
    }
    imguiUnindent();

    imguiLabelBorder("Action:");
    imguiIndent();

    if (imguiButton("Process", true, ImguiAlign::CenterIndented))
    {
        _state.m_events = GuiEvent::HandleAction | GuiEvent::DismissWindow;
    }

    if (imguiButton("Cancel", true, ImguiAlign::CenterIndented))
    {
        _state.m_events = GuiEvent::DismissWindow;
    }

    imguiEndArea();
}

// CmftTransform widget.
//-----

void imguiCmftTransformWidget(cs::EnvHandle _env
                            , int32_t _x
                            , int32_t _y
                            , int32_t _width
                            , CmftTransformWidgetState& _state
                            , const Mouse& _click
                            , bool _enabled
                            )
{
    const int32_t height = 594;
    const cs::Environment& env = cs::getObj(_env);

    _state.m_events = GuiEvent::None;

    imguiBeginArea("Cubemap transform", _x, _y, _width, height, _enabled);
    imguiSeparator(7);


    imguiLabelBorder("Preview:");
    imguiIndent();
    {
        const int32_t previewX = imguiGetWidgetX();
        const int32_t previewY = imguiGetWidgetY();
        const int32_t previewWidth = _width - 44;
        const int32_t faceSize  = int32_t(float(previewWidth)*0.25f);
        const int32_t faceSize2 = int32_t(float(previewWidth)*0.5f);
        const int32_t faceSize3 = int32_t(float(previewWidth)*0.75f);

        ///         _____
        ///        |     |
        ///        | +Y  |
        ///   _____|_____|_____ _____
        ///  |     |     |     |     |
        ///  | -X  | +Z  | +X  | -Z  |
        ///  |_____|_____|_____|_____|
        ///        |     |
        ///        | -Y  |
        ///        |_____|
        ///
        const int32_t faceCoords[6][2] =
        {
            { previewX,           previewY+faceSize  }, // -X
            { previewX+faceSize,  previewY           }, // +Y
            { previewX+faceSize,  previewY+faceSize  }, // +Z
            { previewX+faceSize,  previewY+faceSize2 }, // -Y
            { previewX+faceSize2, previewY+faceSize  }, // +X
            { previewX+faceSize3, previewY+faceSize  }, // -Z
        };

        // Handle click.
        if (Mouse::Up == _click.m_right || Mouse::Up == _click.m_left)
        {
            for (uint8_t ii = 0; ii < 6; ++ii)
            {
                if (dm::inside(_click.m_curr.m_mx, _click.m_curr.m_my, faceCoords[ii][0], faceCoords[ii][1], faceSize, faceSize))
                {
                    _state.m_face = ii;
                }
            }
        }

        // Draw selection outline.
        const int32_t faceSelectionX = faceCoords[_state.m_face][0];
        const int32_t faceSelectionY = faceCoords[_state.m_face][1];
        imguiDrawRect(float(faceSelectionX-4)
                    , float(faceSelectionY-4)
                    , float(faceSize+10)
                    , float(faceSize+10)
                    , imguiRGBA(255,196,0,255)
                    );

        // Draw cube.
        const bgfx::TextureHandle texture = cs::textureGetBgfxHandle(env.m_cubemap[cs::Environment::Skybox]);
        imguiCube(texture, 0.0f, ImguiCubemap::Cross, false, ImguiAlign::CenterIndented);

        // Draw selection semi-transparent overlay.
        imguiDrawRect(float(faceSelectionX)
                    , float(faceSelectionY)
                    , float(faceSize+2)
                    , float(faceSize+2)
                    , imguiRGBA(255,255,255,46)
                    );

    }
    imguiSeparator();
    imguiUnindent();

    imguiLabelBorder("Face selection:");
    imguiIndent();
    {
        _state.m_face = imguiTabs(_state.m_face, true, ImguiAlign::CenterIndented, 16, 2, 6, "-x", "+y", "+z", "-y", "+x", "-z");
    }
    imguiSeparator();
    imguiUnindent();

    imguiLabelBorder("Operation:");
    imguiIndent();
    {
        _state.m_imageOp = (uint8_t)imguiChoose(_state.m_imageOp
                                              , "Rotate 90"
                                              , "Rotate 180"
                                              , "Rotate 270"
                                              , "Flip H"
                                              , "Flip V"
                                              );

        if (imguiButton("Transform", true, ImguiAlign::CenterIndented))
        {
            _state.m_env = _env;
            _state.m_events = GuiEvent::HandleAction;
        }
    }
    imguiSeparator();
    imguiUnindent();

    imguiLabelBorder("Action:");
    imguiIndent();

    if (imguiButton("Close", true, ImguiAlign::CenterIndented))
    {
        _state.m_events = GuiEvent::DismissWindow;
    }

    imguiEndArea();
}

// Tonemap widget.
//-----

void imguiTonemapWidget(cs::EnvHandle _env
                      , int32_t _x
                      , int32_t _y
                      , int32_t _width
                      , TonemapWidgetState& _state
                      , bool _enabled
                      )
{
    const int32_t height = 648;
    const uint8_t latlongPreviewHeight = 120;
    const cs::Environment& env = cs::getObj(_env);

    _state.m_events = GuiEvent::None;

    imguiBeginArea("Tonemap", _x, _y, _width, height, _enabled);
    imguiSeparator(7);

    imguiLabelBorder("Selection:");
    imguiIndent();
    {
        // Original.
        if (imguiCheck("Original", TonemapWidgetState::Original == _state.m_selection))
        {
            _state.m_selection = TonemapWidgetState::Original;
        }

        const cs::TextureHandle skybox = cs::isValid(env.m_origSkybox)
                                       ? env.m_origSkybox
                                       : env.m_cubemap[cs::Environment::Skybox];

        const bgfx::TextureHandle tex = cs::textureGetBgfxHandle(skybox);
        const bool originalClicked = imguiCube(tex, 0.0f, ImguiCubemap::Latlong, true, ImguiAlign::CenterIndented);

        if (originalClicked)
        {
            _state.m_selection = TonemapWidgetState::Original;
        }

        updateTonemapImage(skybox, 1.0f/_state.m_invGamma, _state.m_minLum, _state.m_lumRange);

        // Tonemapped.
        if (imguiCheck("Tonemapped", TonemapWidgetState::Tonemapped == _state.m_selection))
        {
            _state.m_selection = TonemapWidgetState::Tonemapped;
        }
        const bool tonemapClicked = imguiImage(getTonemapTexture(), 0.0f
                                             , latlongPreviewHeight*2
                                             , latlongPreviewHeight
                                             , ImguiAlign::LeftIndented
                                             );
        if (tonemapClicked)
        {
            _state.m_selection = TonemapWidgetState::Tonemapped;
        }
    }
    imguiSeparator();
    imguiUnindent();

    imguiLabelBorder("Tonemap options:");
    imguiIndent();
    {
        const bool tonemapSelected = (TonemapWidgetState::Tonemapped == _state.m_selection);

        imguiSlider("Gamma", _state.m_invGamma, 0.01f, 4.0f, 0.01f, tonemapSelected, ImguiAlign::CenterIndented);

        // Gamma tabs.
        uint8_t idx;
        if (1.0f == _state.m_invGamma)
        {
            idx = 0;
        }
        else if (1.4f == _state.m_invGamma)
        {
            idx = 1;
        }
        else if (1.8f == _state.m_invGamma)
        {
            idx = 2;
        }
        else if (2.2f == _state.m_invGamma)
        {
            idx = 3;
        }
        else if (2.6f == _state.m_invGamma)
        {
            idx = 4;
        }
        else
        {
            idx = UINT8_MAX;
        }

        idx = imguiTabs(idx, tonemapSelected, ImguiAlign::CenterIndented, 16, 2, 5, "1.0", "1.4", "1.8", "2.2", "2.6");
        imguiSeparator();

        if (UINT8_MAX != idx)
        {
            switch (idx)
            {
            case 0: { _state.m_invGamma = 1.0f; } break;
            case 1: { _state.m_invGamma = 1.4f; } break;
            case 2: { _state.m_invGamma = 1.8f; } break;
            case 3: { _state.m_invGamma = 2.2f; } break;
            case 4: { _state.m_invGamma = 2.6f; } break;
            }
        }

        imguiSlider("Lum range", _state.m_lumRange,  0.0f, 5.0f, 0.01f, tonemapSelected, ImguiAlign::CenterIndented);
        imguiSlider("Min lum",   _state.m_minLum,   -0.5f, 0.5f, 0.01f, tonemapSelected, ImguiAlign::CenterIndented);

        if (imguiButton("Reset", tonemapSelected, ImguiAlign::CenterIndented))
        {
            _state.resetParams();
        }
    }
    imguiSeparator();
    imguiUnindent();


    imguiLabelBorder("Action:");
    imguiIndent();

    if (imguiButton((TonemapWidgetState::Original == _state.m_selection)?"Restore":"Apply", true, ImguiAlign::CenterIndented))
    {
        _state.m_env = _env;
        _state.m_events = GuiEvent::HandleAction | GuiEvent::DismissWindow;
    }

    if (imguiButton("Close", true, ImguiAlign::CenterIndented))
    {
        _state.m_events = GuiEvent::DismissWindow;
    }

    imguiEndArea();
}

// EnvMapBrowser widget.
//-----

void imguiEnvMapBrowserWidget(const char* _name
                            , int32_t _x
                            , int32_t _y
                            , int32_t _width
                            , EnvMapBrowserState& _state
                            )
{
    const int32_t browserHeight = 400;
    const int32_t height = browserHeight + 457;

    _state.m_events = GuiEvent::None;

    imguiBeginArea(_name, _x, _y, _width, height, true);
    imguiSeparator(7);

    imguiLabelBorder("Directory:");
    imguiIndent();
    {
        if (imguiDirectoryLabel(_state.m_directory))
        {
            _state.m_events = GuiEvent::GuiUpdate;
        }
    }
    imguiSeparator();
    imguiUnindent();

    imguiLabelBorder("Filter:");
    imguiIndent();
    {
        imguiBool("*.dds", _state.m_extDds);
        imguiBool("*.ktx", _state.m_extKtx);
        imguiBool("*.tga", _state.m_extTga);
        imguiBool("*.hdr", _state.m_extHdr);
    }
    imguiSeparator();
    imguiUnindent();

    imguiLabelBorder("Browse:");
    imguiIndent();
    {
        uint8_t count = 0;
        char extensions[4][FileExtensionLength];
        if (_state.m_extDds) { strcpy(extensions[count++], "dds"); }
        if (_state.m_extKtx) { strcpy(extensions[count++], "ktx"); }
        if (_state.m_extTga) { strcpy(extensions[count++], "tga"); }
        if (_state.m_extHdr) { strcpy(extensions[count++], "hdr"); }
        imguiBrowser(browserHeight, _state, extensions, count);
    }
    imguiUnindent();

    imguiLabelBorder("Filename:");
    imguiIndent();
    {
        imguiLabel('\0' == _state.m_fileNameExt[0] ? "<invalid>" : _state.m_fileNameExt);
    }
    imguiSeparator();
    imguiUnindent();

    imguiLabelBorder("Action:");
    imguiIndent();
    {
        if (imguiButton("Load", true, ImguiAlign::CenterIndented))
        {
            _state.m_events = GuiEvent::HandleAction | GuiEvent::DismissWindow;
        }
        if (imguiButton("Close", true, ImguiAlign::CenterIndented))
        {
            _state.m_events = GuiEvent::DismissWindow;
        }
    }

    imguiEndArea();
}

// TexPicker widget.
//-----

void imguiTexPickerWidget(int32_t _x
                        , int32_t _y
                        , int32_t _width
                        , const cs::TextureList& _textureList
                        , TexPickerWidgetState& _state
                        )
{
    const int32_t texturesHeight = (20+4)*_textureList.count()
                                 + (20+4)
                                 + 14
                                 ;
    const int32_t listHeight = dm::min(400, texturesHeight);
    const int32_t texHeight = 176;
    const int32_t height = 460 + int32_t(_state.m_confirmButton)*24 + listHeight;

    _state.m_events = GuiEvent::None;

    imguiBeginArea(_state.m_title, _x, _y, _width, height, true);
    imguiSeparator(6);
    imguiLabelBorder("Preview:");
    if (_textureList.count() == 0)
    {
        const bgfx::TextureHandle invalid = BGFX_INVALID_HANDLE;
        imguiImage(invalid, 0.0f, texHeight, texHeight, ImguiAlign::CenterIndented);
    }
    else
    {
        imguiImage(cs::textureGetBgfxHandle(_textureList[_state.m_selection]), 0.0f, texHeight, texHeight, ImguiAlign::CenterIndented);
    }
    imguiSeparator(6);

    imguiLabelBorder("Edit:");
    imguiIndent();
    {
        if (imguiConfirmButton("Remove", "Remove", "Confirm removal!", _state.m_confirmButton, _textureList.count() > 0, ImguiAlign::CenterIndented))
        {
            _state.m_selection = DM_MIN(_textureList.count()-1, _state.m_selection);
            _state.m_action = TexPickerWidgetState::Remove;
            _state.m_events = GuiEvent::HandleAction;
        }
    }
    imguiSeparator();
    imguiUnindent();

    imguiLabelBorder("Texture list:");

    imguiBeginScroll(listHeight, &_state.m_scroll, true);
    imguiIndent();
    {
        for (uint16_t ii = 0, end = _textureList.count(); ii < end; ++ii)
        {
            imguiCheckSelection(cs::getName(_textureList[ii]), _state.m_selection, ii);
        }

        if (imguiButton("Browse..."))
        {
            _state.m_events = GuiEvent::GuiUpdate;
        }

        imguiSeparator(2);
    }
    imguiEndScroll();

    imguiLabelBorder("Action:");
    imguiIndent();
    {
        if (imguiButton("Ok", _textureList.count() > 0, ImguiAlign::CenterIndented))
        {
            _state.m_action = TexPickerWidgetState::Pick;
            _state.m_events = GuiEvent::HandleAction | GuiEvent::DismissWindow;
        }
        if (imguiButton("Cancel", true, ImguiAlign::CenterIndented))
        {
            _state.m_events = GuiEvent::DismissWindow;
        }
    }
    imguiEndArea();
}

// Modal output window.
//-----

// Check if OutputWindowState::MaxLines is a power of two number.
#define IS_POWER_OF_TWO(_v) (_v) && !((_v)&((_v)-1))
BX_STATIC_ASSERT(IS_POWER_OF_TWO(OutputWindowState::MaxLines));

void imguiModalOutputWindow(int32_t _x, int32_t _y, OutputWindowState& _state)
{
    _state.m_events = GuiEvent::None;

    guiDrawOverlay();
    imguiBeginArea("Output", _x, _y, OutputWindowState::Width, OutputWindowState::Height, true);

    imguiSeparator(8);
    imguiBeginScroll(OutputWindowState::ScrollHeight, &_state.m_scroll);

    imguiIndent();
    {
        ImguiFontScope fontScope(Fonts::DroidSansMono);

        for (int16_t ii = _state.m_linesCount; ii--; )
        {
            imguiLabel(_state.m_lines[ii]);
        }
    }
    imguiSeparator(2);
    imguiUnindent();

    imguiEndScroll();
    imguiSeparator(9);

    imguiIndent();
    if (imguiButton("Close", true, ImguiAlign::CenterIndented))
    {
        _state.m_events = GuiEvent::DismissWindow;
    }

    imguiEndArea();
}

// About window.
//-----

void imguiModalAboutWindow(int32_t _x
                         , int32_t _y
                         , AboutWindowState& _state
                         , int32_t _width
                         , int32_t _height
                         )
{
    _state.m_events = GuiEvent::None;

    guiDrawOverlay();
    imguiBeginArea("About", _x, _y, _width, _height, true);

    imguiSeparator(8);
    imguiBeginScroll(_height-85, &_state.m_scroll);

    imguiIndent();
    {
        #define ABOUT_LINE(_str) imguiLabel(_str);
        #include "gui_res.h"
    }
    imguiUnindent();

    imguiEndScroll();
    imguiSeparator(11);

    imguiIndent();
    if (imguiButton("Close", true, ImguiAlign::CenterIndented))
    {
        _state.m_events = GuiEvent::DismissWindow;
    }

    imguiEndArea();

}

// Magnify window.
//-----

void imguiModalMagnifyWindow(int32_t _x, int32_t _y, MagnifyWindowState& _state)
{
    _state.m_events = GuiEvent::None;

    const cs::Environment& env = cs::getObj(_state.m_env);
    const cmft::Image& image = env.m_cubemapImage[_state.m_envType];
    const bgfx::TextureHandle texture = cs::textureGetBgfxHandle(env.m_cubemap[_state.m_envType]);
    const bool hasLod = (image.m_numMips!=1);

    guiDrawOverlay();

    const float wPadding = 0.06f;
    const float hPadding = 0.07f;
    const float wScale = (1.0f-wPadding*2.0f);
    const float hScale = (1.0f-hPadding*2.0f);

    const uint32_t width  = dm::ftou(float(g_guiWidth)*wScale);
    const uint32_t height = dm::ftou(float(g_guiHeight)*hScale);
    const int32_t heightAdj = (hasLod ? 0 : -21);
    imguiBeginArea(NULL, _x, _y, width, height + heightAdj);
    {
        imguiSeparator();
        imguiIndent(12);

        const int32_t indent = DM_MAX(0, (int32_t(width)-1650)/2);
        imguiIndent(indent);

        if (imguiCube(texture, _state.m_lod, (ImguiCubemap::Enum)_state.m_imguiCubemap, true, ImguiAlign::CenterIndented))
        {
            _state.m_imguiCubemap = ImguiCubemap::Enum((_state.m_imguiCubemap+1) % ImguiCubemap::Count);
        }

        imguiSeparator(4);

        if (hasLod)
        {
            imguiSlider("Lod", _state.m_lod, 0.0f, dm::utof(image.m_numMips-1), 0.1f, true, ImguiAlign::CenterIndented);
        }
    }
    imguiEndArea();

    // Close button.
    enum
    {
        xWidth  = 40,
        xHeight = 34,
    };
    imguiBeginArea(NULL, _x+width-xWidth, _y-xHeight, xWidth, xHeight);
    if (imguiButton("X"))
    {
        _state.m_events = GuiEvent::DismissWindow;
    }
    imguiEndArea();
}

// Imgui directory listing.
//-----

bool imguiBrowser(int32_t _height
                , BrowserState& _state
                , const char (*_ext)[16]
                , uint8_t _extCount
                , const char* _selectFile
                , bool _showDirsOnly
                , bool _showHidden
                )
{
    bool result = false;
    bool modifyDir = false;
    size_t dirNum = 0;

    const char* selectFile = _selectFile ? _selectFile : _state.m_fileNameExt;

    dm::StackAllocScope scope(dm::stackAlloc);

    tinydir_dir dir;
    tinydir_open_sorted(&dir, _state.m_directory);

    imguiSeparatorLine(1, ImguiAlign::CenterIndented);
    imguiSeparator(2);
    const uint8_t button = imguiTabs(UINT8_MAX, true, ImguiAlign::CenterIndented, 21, 0, 4, "Root", "Home", "Desktop", "Runtime");
    imguiSeparatorLine(1, ImguiAlign::CenterIndented);
    imguiSeparator(4);

    enum { BookmarkButtonsHeight = 29 };
    imguiBeginScroll(_height-BookmarkButtonsHeight, &_state.m_scroll);

    const bool windowsRootDir = _state.m_directory[1] == ':'
                             && _state.m_directory[2] == '\0'
                              ;

    if (windowsRootDir)
    {
        if (imguiButton(".."))
        {
            _state.m_windowsDrives = true;
        }
    }

    if (_state.m_windowsDrives)
    {
        const uint32_t drives = dm::windowsDrives();
        for (uint32_t ii = 0; ii < 32; ++ii)
        {
            if (0 != (drives&(1<<ii)))
            {
                const char driveLetter = 'A'+ii;
                const char buttonStr[4] = { driveLetter, ':', '\\', '\0' };
                if (imguiButton(buttonStr))
                {
                    _state.m_directory[0] = buttonStr[0];
                    _state.m_directory[1] = buttonStr[1];
                    _state.m_directory[2] = buttonStr[2];
                    _state.m_directory[3] = buttonStr[3];
                    _state.m_windowsDrives = false;
                }
            }
        }
    }
    else
    {
        for (size_t ii = 0, end = dir.n_files; ii < end; ii++)
        {
            tinydir_file file;
            if (-1 == tinydir_readfile_n(&dir, &file, ii))
            {
                continue;
            }

            const bool dot      = ('.' == file.name[0]);
            const bool dotEnd   = dot && ('\0' == file.name[1]); // never show
            const bool dotDot   = dot && ( '.' == file.name[1]); // always show
            const bool isHidden = dot && ('\0' != file.name[1]);

            if (file.is_dir && !dotEnd && (dotDot || !isHidden || _showHidden))
            {
                if (imguiButton(file.name))
                {
                    modifyDir = true;
                    dirNum = ii;
                }
            }
            else if (!_showDirsOnly)
            {
                bool show;
                if (NULL == _ext
                ||  0    == _extCount)
                {
                    show = !isHidden;
                }
                else
                {
                    show = false;
                    for(uint8_t jj = 0; jj < _extCount; ++jj)
                    {
                        if (('.' != file.name[0])
                        &&  (0 == bx::stricmp(_ext[jj], file.extension)))
                        {
                            show = true;
                            break;
                        }
                    }
                }

                if (show)
                {
                    const bool checked = selectFile ? (0 == bx::stricmp(file.name, selectFile)) : false;
                    const bool click = imguiCheck(file.name, checked);
                    if (click || checked)
                    {
                        result = click;
                        selectFile = NULL;

                        dm::realpath(_state.m_filePath, file.path);
                        dm::strscpya(_state.m_fileName, file.name);
                        //Remove extension.
                        const bool hasExt = ('\0' != file.extension[0]);
                        if (hasExt)
                        {
                            _state.m_fileName[file.extension-file.name-1] = '\0';
                        }
                        dm::strscpya(_state.m_fileNameExt, file.name);
                        dm::strscpya(_state.m_fileExt, file.extension);
                    }
                }
            }
        }
    }

    imguiSeparator(4);
    imguiEndScroll();
    imguiSeparator(4);

    if (modifyDir)
    {
        tinydir_open_subdir_n(&dir, dirNum);

        _state.reset();
        dm::realpath(_state.m_directory, dir.path);
    }
    else if (UINT8_MAX != button)
    {
        _state.reset();

        if (0 == button) // root
        {
            #if BX_PLATFORM_WINDOWS
                _state.m_directory[3] = '\0';
            #else
                dm::rootDir(_state.m_directory);
            #endif // BX_PLATFORM_WINDOWS
        }
        else if (1 == button) // home
        {
            dm::homeDir(_state.m_directory);
        }
        else if (2 == button) // desktop
        {
            dm::desktopDir(_state.m_directory);
        }
        else //if (3 == button). // runtime
        {
            bx::pwd(_state.m_directory, DM_PATH_LEN);
        }
    }

    dm::trimDirPath(_state.m_directory);

    tinydir_close(&dir);

    return result;
}

template <uint8_t MaxSelectedT>
void imguiBrowser(int32_t _height
                , BrowserStateFor<MaxSelectedT>& _state
                , const char (*_ext)[16]
                , uint8_t _extCount
                , bool _showDirsOnly
                , bool _showHidden
                )
{
    typedef typename BrowserStateFor<MaxSelectedT>::File BrowserStateFile;

    bool modifyDir = false;
    size_t dirNum = 0;

    uint16_t numHandles = 0;
    uint16_t handles[MaxSelectedT];
    for (uint16_t ii = _state.m_files.count(); ii--; )
    {
        handles[numHandles++] = _state.m_files.getHandleAt(ii);
    }

    dm::StackAllocScope scope(dm::stackAlloc);

    tinydir_dir dir;
    tinydir_open_sorted(&dir, _state.m_directory);

    imguiBeginScroll(_height, &_state.m_scroll, true);

    imguiSeparatorLine(1);
    imguiSeparator(2);
    const uint8_t button = imguiTabs(UINT8_MAX, true, ImguiAlign::LeftIndented, 21, 0, 4, "Root", "Home", "Desktop", "Runtime");
    imguiSeparatorLine(1);
    imguiSeparator(4);

    const bool windowsRootDir = _state.m_directory[1] == ':'
                             && _state.m_directory[2] == '\0'
                              ;

    if (windowsRootDir)
    {
        if (imguiButton(".."))
        {
            _state.m_windowsDrives = true;
        }
    }

    if (_state.m_windowsDrives)
    {
        const uint32_t drives = dm::windowsDrives();
        for (uint32_t ii = 0; ii < 32; ++ii)
        {
            if (0 != (drives&(1<<ii)))
            {
                const char driveLetter = 'A'+ii;
                const char buttonStr[4] = { driveLetter, ':', '\\', '\0' };
                if (imguiButton(buttonStr))
                {
                    _state.m_directory[0] = buttonStr[0];
                    _state.m_directory[1] = buttonStr[1];
                    _state.m_directory[2] = buttonStr[2];
                    _state.m_directory[3] = buttonStr[3];
                    _state.m_windowsDrives = false;
                }
            }
        }
    }
    else
    {
        for (size_t ii = 0, end = dir.n_files; ii < end; ii++)
        {
            tinydir_file file;
            if (-1 == tinydir_readfile_n(&dir, &file, ii))
            {
                continue;
            }

            const bool dot      = ('.' == file.name[0]);
            const bool dotEnd   = dot && ('\0' == file.name[1]); // never show
            const bool dotDot   = dot && ( '.' == file.name[1]); // always show
            const bool isHidden = dot && ('\0' != file.name[1]);

            if (file.is_dir && !dotEnd && (dotDot || !isHidden || _showHidden))
            {
                if (imguiButton(file.name))
                {
                    modifyDir = true;
                    dirNum = ii;
                }
            }
            else if (!_showDirsOnly && _extCount > 0)
            {
                bool show;
                if (NULL == _ext
                ||  0    == _extCount)
                {
                    show = !isHidden;
                }
                else
                {
                    show = false;
                    for(uint8_t jj = 0; jj < _extCount; ++jj)
                    {
                        if (0 == bx::stricmp(_ext[jj], file.extension))
                        {
                            show = true;
                            break;
                        }
                    }
                }

                if (show)
                {
                    bool checked = false;
                    uint16_t selected = UINT16_MAX;
                    for (uint16_t ii = numHandles; ii--; )
                    {
                        const char* selectFile = _state.m_files.get(handles[ii])->m_nameExt;
                        if (0 == strcmp(file.name, selectFile))
                        {
                            selected = handles[ii];
                            handles[ii] = handles[--numHandles];
                            checked = true;
                            break;
                        }
                    }

                    const bool clicked = imguiCheck(file.name, checked);

                    if (clicked)
                    {
                        // Deselect.
                        if (checked && UINT16_MAX != selected)
                        {
                            handles[numHandles++] = selected;
                        }
                        // Select.
                        else if (_state.m_files.count() < MaxSelectedT)
                        {
                            BrowserStateFile* newFile = _state.m_files.addNew();

                            dm::strscpya(newFile->m_path, file.path);
                            dm::strscpya(newFile->m_name, file.name);
                            //Remove extension.
                            const bool hasExt = ('\0' != file.extension[0]);
                            if (hasExt)
                            {
                                newFile->m_name[file.extension-file.name-1] = '\0';
                            }
                            dm::strscpya(newFile->m_nameExt, file.name);
                            dm::strscpya(newFile->m_ext, file.extension);
                        }
                    }
                }
            }
        }
    }

    for (uint16_t ii = numHandles; ii--; )
    {
        _state.m_files.remove(handles[ii]);
    }

    imguiEndScroll();
    imguiSeparator(4);

    if (modifyDir)
    {
        tinydir_open_subdir_n(&dir, dirNum);

        _state.m_scroll = 0;
        _state.m_files.reset();
        dm::realpath(_state.m_directory, dir.path);
    }
    else if (UINT8_MAX != button)
    {
        _state.m_scroll = 0;
        _state.m_files.reset();

        if (0 == button) // root
        {
            #if BX_PLATFORM_WINDOWS
                _state.m_directory[3] = '\0';
            #else
                dm::rootDir(_state.m_directory);
            #endif // BX_PLATFORM_WINDOWS
        }
        else if (1 == button) // home
        {
            dm::homeDir(_state.m_directory);
        }
        else if (2 == button) // desktop
        {
            dm::desktopDir(_state.m_directory);
        }
        else //if (3 == button). // runtime
        {
            bx::pwd(_state.m_directory, DM_PATH_LEN);
        }
    }

    dm::trimDirPath(_state.m_directory);

    tinydir_close(&dir);
}

// Project window.
//-----

void imguiModalProjectWindow(int32_t _x
                           , int32_t _y
                           , ProjectWindowState& _state
                           )
{
    const int32_t width = 350;
    const int32_t height = 808 + int32_t(_state.m_confirmButton)*24;

    _state.m_events = GuiEvent::None;

    guiDrawOverlay();

    imguiBeginArea("cmftStudio project", _x, _y, width, height, true);
    imguiSeparator(7);

    const uint8_t currentAction = _state.m_tabs;
    _state.m_tabs = imguiTabs(_state.m_tabs, true, ImguiAlign::LeftIndented, 21, 0, 2, "Load", "Save");
    imguiSeparator();

    // Reset confirm button state on tab change.
    if (currentAction != _state.m_tabs)
    {
        _state.m_confirmButton = false;
    }

    const int32_t saveHeight = int32_t(ProjectWindowState::Save == _state.m_tabs)*94;
    const int32_t browserHeight = 388-saveHeight;

    imguiLabelBorder("Filter:");
    imguiIndent();
    {
        imguiCheck("*.csp", true, false);
    }
    imguiSeparator();
    imguiUnindent();

    const char extensions[1][FileExtensionLength] = { "csp" };
    if (ProjectWindowState::Load == _state.m_tabs)
    {
        imguiLabelBorder("Directory:");
        imguiIndent();
        {
            if (imguiDirectoryLabel(_state.m_load.m_directory))
            {
                _state.m_action = ProjectWindowState::ShowLoadDir;
                _state.m_events = GuiEvent::GuiUpdate;
            }
        }
        imguiSeparator();
        imguiUnindent();

        imguiLabelBorder("Browse:");
        imguiIndent();
        {
            imguiBrowser(browserHeight, _state.m_load, extensions, 1);
        }
        imguiUnindent();

        imguiLabelBorder("Filename:");
        imguiIndent();
        {
            imguiLabel('\0' == _state.m_load.m_fileNameExt[0] ? "<invalid>" : _state.m_load.m_fileNameExt);
        }
        imguiSeparator();
        imguiUnindent();

        imguiLabelBorder("Action:");
        imguiIndent();

        bool currentConfirmBtnState = _state.m_confirmButton;

        if (imguiConfirmButton("Load", "Load", "Click to confirm this action!", _state.m_confirmButton, true, ImguiAlign::CenterIndented))
        {
            _state.m_action = _state.m_tabs;
            _state.m_events = GuiEvent::HandleAction | GuiEvent::DismissWindow;
        }
        else if (currentConfirmBtnState != _state.m_confirmButton && _state.m_confirmButton)
        {
            _state.m_action = ProjectWindowState::Warning;
            _state.m_events = GuiEvent::GuiUpdate;
        }
    }
    else //(ProjectWindowState::Save == _state.m_tabs).
    {
        imguiLabelBorder("Directory:");
        imguiIndent();
        {
            if (imguiDirectoryLabel(_state.m_save.m_directory))
            {
                _state.m_action = ProjectWindowState::ShowSaveDir;
                _state.m_events = GuiEvent::GuiUpdate;
            }
        }
        imguiSeparator();
        imguiUnindent();

        imguiLabelBorder("Browse:");
        imguiIndent();
        {
            char select[128];
            bx::snprintf(select, 128, "%s.csp", _state.m_projectName);
            const bool fileClicked = imguiBrowser(browserHeight, _state.m_save, extensions, 1, select);
            if (fileClicked)
            {
                dm::strscpya(_state.m_projectName, _state.m_save.m_fileName);
            }
        }
        imguiUnindent();

        imguiLabelBorder("Options:");
        imguiIndent();
        {
            imguiInput("Name:", _state.m_projectName, 128, true, ImguiAlign::CenterIndented);
            imguiSlider("Compression level", _state.m_compressionLevel, 0.0f, 10.0f, 1.0f, true, ImguiAlign::CenterIndented);
        }
        imguiSeparator();
        imguiUnindent();

        imguiLabelBorder("Output file:");
        imguiIndent();
        {
            char projName[128];
            bx::snprintf(projName, sizeof(projName), "%s.csp", _state.m_projectName);
            imguiLabel(projName);
        }
        imguiSeparator();
        imguiUnindent();

        imguiLabelBorder("Action:");
        imguiIndent();

        if (imguiConfirmButton("Save", "Save", "Click to confirm this action!", _state.m_confirmButton, true, ImguiAlign::CenterIndented))
        {
            _state.m_action = _state.m_tabs;
            _state.m_events = GuiEvent::HandleAction | GuiEvent::DismissWindow;
        }
    }

    if (imguiButton("Close", true, ImguiAlign::CenterIndented))
    {
        _state.m_events = GuiEvent::DismissWindow;
    }

    imguiEndArea();
}

bool imguiEnvPreview(uint32_t _screenX
                   , uint32_t _screenY
                   , uint32_t _areaWidth
                   , cs::EnvHandle _env
                   , const Mouse& _click
                   , bool _enabled
                   , uint8_t _viewId
                   )
{
    const uint32_t llWidth  = (_areaWidth-2)/2;
    const uint32_t llHeight = (_areaWidth-2)/4;

    const uint32_t pos[] =
    {
        _screenX+llHeight,  _screenY,
        _screenX,           _screenY+llHeight+2,
        _screenX+llWidth+2, _screenY+llHeight+2,
    };

    cs::Environment& env = cs::getObj(_env);

    for (uint8_t ii = 0; ii < 3; ++ii)
    {
        cs::setTexture(cs::TextureUniform::Skybox, env.m_cubemap[ii]);
        screenQuad(pos[ii*2], pos[ii*2+1], llWidth, llHeight);
        imguiSetCurrentScissor();
        bgfx::setState(BGFX_STATE_RGB_WRITE|BGFX_STATE_ALPHA_WRITE);
        cs::bgfx_submit(_viewId, cs::Program::Latlong);
    }

    const bool click = _enabled
                    && (Mouse::Up == _click.m_right || Mouse::Up == _click.m_left)
                    && dm::inside(_click.m_curr.m_mx, _click.m_curr.m_my, _screenX, _screenY, _areaWidth, _areaWidth/2);
    return click;
}

// Custom LatLong GUI element.
//-----

void imguiLatlongWidget(int32_t _screenX
                      , int32_t _screenY
                      , int32_t _height
                      , cs::EnvHandle _env
                      , uint8_t& _selectedLight
                      , const Mouse& _click
                      , bool _enabled
                      , uint8_t _viewId
                      )
{
    enum
    {
        SunSize  = 28,
        QuadSize = 128,
    };

    const uint32_t llWidth  = _height*2;
    const uint32_t llHeight = _height;

    const float llWidthf  = dm::utof(llWidth);
    const float llHeightf = dm::utof(llHeight);

    cs::Environment& env = cs::getObj(_env);

    // Draw latlong image.
    cs::setTexture(cs::TextureUniform::Skybox, env.m_cubemap[cs::Environment::Skybox]);
    screenQuad(_screenX, _screenY, llWidth, llHeight);
    imguiSetCurrentScissor();
    bgfx::setState(BGFX_STATE_RGB_WRITE|BGFX_STATE_ALPHA_WRITE);
    cs::bgfx_submit(_viewId, cs::Program::Latlong);

    const bool insideLatlong = _enabled && dm::inside(_click.m_curr.m_mx, _click.m_curr.m_my, _screenX, _screenY, llWidth, llHeight);
    uint8_t mouseOverLight = UINT8_MAX;

    int32_t xx = 0;
    int32_t yy = 0;
    float   uu = 0.0f;
    float   vv = 0.0f;
    if (insideLatlong)
    {
        xx = _click.m_curr.m_mx - int32_t(_screenX);
        yy = _click.m_curr.m_my - int32_t(_screenY);
        uu = float(xx)/llWidthf;
        vv = float(yy)/llHeightf;
    }

    for (uint8_t ii = 0; ii < CS_MAX_LIGHTS; ++ii)
    {
        if (dm::isSet(env.m_lights[ii].m_enabled))
        {
            float luv[2];
            latLongFromVec(luv, env.m_lights[ii].m_dir);
            const uint32_t lx = uint32_t(luv[0]*llWidthf);
            const uint32_t ly = uint32_t(luv[1]*llHeightf);

            if (insideLatlong
            &&  UINT8_MAX == mouseOverLight
            &&  dm::inside(xx, yy, lx-(SunSize/2), ly-(SunSize/2), SunSize, SunSize))
            {
                mouseOverLight = ii;
            }

            if (env.m_lightUseBackgroundColor[ii] && cmft::imageIsValid(env.m_cubemapImage[cs::Environment::Skybox]))
            {
                cmft::imageCubemapGetPixel(env.m_lights[ii].m_color
                                         , cmft::TextureFormat::RGB32F
                                         , env.m_lights[ii].m_dir, 0
                                         , env.m_cubemapImage[cs::Environment::Skybox]
                                         );
            }

            cs::Uniforms& uniforms = cs::getUniforms();
            memcpy(uniforms.m_rgba, env.m_lights[ii].m_colorStrenght, 4*sizeof(float));
            uniforms.m_selectedLight = float(ii == _selectedLight);

            // Draw sun icon.
            cs::setTexture(cs::TextureUniform::Color, s_res.m_texSunIcon);
            screenQuad(_screenX + lx - (QuadSize/2)
                     , _screenY + ly - (QuadSize/2)
                     , QuadSize
                     , QuadSize
                     );
            bgfx::setState(BGFX_STATE_RGB_WRITE
                          |BGFX_STATE_ALPHA_WRITE
                          |BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA)
                          );
            cs::bgfx_submit(_viewId, cs::Program::SunIcon);
        }
    }

    if (insideLatlong)
    {
        static bool s_active = false;

        // Left click.
        if (Mouse::Down == _click.m_left)
        {
            s_active = (_selectedLight == mouseOverLight);
            _selectedLight = mouseOverLight;
        }
        else if (Mouse::Hold == _click.m_left)
        {
            if (s_active && UINT8_MAX != _selectedLight && dm::isSet(env.m_lights[_selectedLight].m_enabled))
            {
                // Update direction.
                vecFromLatLong(env.m_lights[_selectedLight].m_dir, uu, vv);
            }
        }

        // Right click.
        if (Mouse::Down == _click.m_right)
        {
            if (UINT8_MAX == mouseOverLight)
            {
                // Iterate to find a disabled light.
                for (uint8_t ii = 0; ii < CS_MAX_LIGHTS; ++ii)
                {
                    if (false == dm::isSet(env.m_lights[ii].m_enabled))
                    {
                        // Determine direction.
                        float dir[3];
                        vecFromLatLong(dir, uu, vv);

                        // Setup and enable light.
                        env.m_lights[ii].m_color[0] = 1.0f;
                        env.m_lights[ii].m_color[1] = 1.0f;
                        env.m_lights[ii].m_color[2] = 1.0f;
                        env.m_lights[ii].m_strenght = 1.0f;
                        env.m_lights[ii].m_dir[0] = dir[0];
                        env.m_lights[ii].m_dir[1] = dir[1];
                        env.m_lights[ii].m_dir[2] = dir[2];
                        env.m_lights[ii].m_enabled = 1.0f;

                        _selectedLight = ii;

                        break;
                    }
                }
            }
            else
            {
                // Disable clicked light.
                env.m_lights[mouseOverLight].m_enabled = 0.0f;
                _selectedLight = UINT8_MAX;
            }
        }
    }
}

void imguiLeftScrollArea(int32_t _x
                       , int32_t _y
                       , uint32_t _width
                       , uint32_t _height
                       , ImguiState& _guiState
                       , Settings& _settings
                       , const Mouse& _mouse
                       , cs::EnvHandle _currentEnv
                       , cs::MeshInstanceList& _meshInstList
                       , const cs::MeshInstance& _meshCurrent
                       , const cs::MaterialList& _materialList
                       , LeftScrollAreaState& _state
                       , bool _enabled
                       )
{
    cs::MeshInstance&        instance  = _meshInstList[_settings.m_selectedMeshIdx];
    const cs::MaterialHandle matHandle = instance.getActiveMaterial();
    cs::Material&            matObj    = cs::getObj(matHandle);
    char*                    matName   = cs::getName(matHandle);

    const uint32_t height = _height+2;
    _state.m_events = GuiEvent::None;
    _state.m_inactiveTexPreviews = 0;

    // Tabs.
    imguiBeginArea(NULL, _x, _y, _width, height, _enabled);
    imguiSeparator(7);
    imguiIndent();
    {
        _state.m_prevSelectedTab = _state.m_selectedTab;
        _state.m_selectedTab = imguiTabs(_state.m_selectedTab - 2, true, ImguiAlign::Center, 20, 9, 1, "Spheres") + 2;
        _state.m_selectedTab = imguiTabs(_state.m_selectedTab    , true, ImguiAlign::Center, 20, 9, 2, "Model", "Material");

        if (_state.m_prevSelectedTab != _state.m_selectedTab)
        {
            _state.m_action = LeftScrollAreaState::TabSelectionChange;
            _state.m_events = GuiEvent::HandleAction;
        }
    }
    imguiUnindent();
    imguiSeparator(9);

    // Content.
    if (LeftScrollAreaState::ModelTab == _state.m_selectedTab)
    {
        imguiBeginScroll(height - Gui::LeftAreaHeaderHeight - Gui::LeftAreaFooterHeight, &_state.m_modelScroll, _enabled);

        const cs::MeshHandle selectedMesh = _meshInstList[_settings.m_selectedMeshIdx].m_mesh;
        char* meshName = cs::getName(selectedMesh);

        imguiRegionBorder("Mesh", meshName, _guiState.m_showMeshesOptions);
        if (_guiState.m_showMeshesOptions)
        {
            imguiIndent();
            {
                imguiInput("Name:", meshName, 32);

                const uint8_t button = imguiTabs(UINT8_MAX, true, ImguiAlign::LeftIndented, 21, 4, 2, "Remove", "Save...");

                if (0 == button) // Remove
                {
                    _guiState.m_confirmMeshRemoval = !_guiState.m_confirmMeshRemoval;
                }
                else if (1 == button) // Save...
                {
                    _state.m_action = LeftScrollAreaState::SaveMesh;
                    _state.m_data   = _meshInstList[_settings.m_selectedMeshIdx].m_mesh.m_idx;
                    _state.m_events = GuiEvent::GuiUpdate;

                    _guiState.m_confirmMaterialRemoval = false;
                }

                // Removal confirm button.
                if (_guiState.m_confirmMeshRemoval)
                {
                    const bool confirmed = imguiButton("Confirm removal!", true, ImguiAlign::LeftIndented, imguiRGBA(255,0,0,0));

                    if (confirmed)
                    {
                        _state.m_action = LeftScrollAreaState::RemoveMesh;
                        _state.m_data   = _settings.m_selectedMeshIdx;
                        _state.m_events = GuiEvent::HandleAction;

                        _guiState.m_confirmMeshRemoval = false;
                    }

                }

                imguiRegion("Preview", NULL, _guiState.m_showMeshPreview);
                if (_guiState.m_showMeshPreview)
                {
                    updateWireframePreview(instance.m_mesh, instance.m_selGroup);
                    imguiImage(getWireframeTexture(), 0.0f, 0.5f, 1.0f, ImguiAlign::CenterIndented, true, g_originBottomLeft);
                }

                // Mesh selection.
                enum { MeshSelectionTopPadding = 1 };
                imguiSeparator(MeshSelectionTopPadding);
                imguiLabel("  Meshes:");
                const int32_t scrollHeight = Gui::MeshScrollRegion
                                           - MeshSelectionTopPadding
                                           + (_guiState.m_showMeshPreview ? 0 : 124);
                imguiBeginScroll(scrollHeight, &_state.m_meshSelectionScroll, _enabled);
                imguiIndent();
                {
                    const uint16_t currentSelection = _settings.m_selectedMeshIdx;
                    for (uint16_t ii = 0, end = _meshInstList.count(); ii < end; ++ii)
                    {
                        imguiCheckSelection(cs::getName(_meshInstList[ii].m_mesh), _settings.m_selectedMeshIdx, ii);
                    }

                    // On selection change.
                    if (currentSelection != _settings.m_selectedMeshIdx)
                    {
                        _state.m_action = LeftScrollAreaState::MeshSelectionChange;
                        _state.m_data   = _meshInstList[_settings.m_selectedMeshIdx].m_mesh.m_idx;
                        _state.m_events = GuiEvent::HandleAction;
                    }

                    if (imguiButton("Browse..."))
                    {
                        _state.m_action = LeftScrollAreaState::ToggleMeshBrowser;
                        _state.m_events = GuiEvent::GuiUpdate;
                    }
                    imguiSeparator(1);
                }
                imguiUnindent();
                imguiSeparator(2);

                //imguiRegion("Mesh groups", NULL, _guiState.m_showMeshGroups);
                imguiIndent();
                {
                    bool alwaysTrue = true;
                    imguiRegion("Mesh groups", NULL, alwaysTrue, false);
                    if (_guiState.m_showMeshGroups)
                    {
                        imguiIndent();
                        {
                            char groupName[16];
                            for (uint16_t ii = 0, end = (uint16_t)cs::meshNumGroups(selectedMesh); ii < end; ++ii)
                            {
                                bx::snprintf(groupName, sizeof(groupName), "Group - %u", ii);
                                imguiCheckSelection(groupName, instance.m_selGroup, ii);
                            }
                        }
                        imguiUnindent();
                    }
                }
                imguiUnindent();

                imguiEndScroll();
            }
            imguiUnindent();
        }
        else
        {
            _state.m_action = LeftScrollAreaState::HideLeftSideWidget;
            _state.m_events = GuiEvent::GuiUpdate;
        }

        imguiSeparator(4);

        // Materials.
        imguiRegionBorder("Material", matName, _guiState.m_showMaterials);
        if (_guiState.m_showMaterials)
        {
            imguiIndent();
            {
                imguiInput("Name:", matName, 32);

                const uint8_t button = imguiTabs(UINT8_MAX, true, ImguiAlign::LeftIndented, 21, 4, 2, "Remove", "Make copy");

                if (0 == button) // Remove
                {
                    _guiState.m_confirmMaterialRemoval = !_guiState.m_confirmMaterialRemoval;
                }
                else if (1 == button) // Make copy
                {
                    _state.m_action = LeftScrollAreaState::CopyMaterial;
                    _state.m_data   = matHandle.m_idx;
                    _state.m_events = GuiEvent::HandleAction;

                    _guiState.m_confirmMaterialRemoval = false;
                }

                // Removal confirm button.
                if (_guiState.m_confirmMaterialRemoval)
                {
                    const bool confirmed = imguiButton("Confirm removal!", true, ImguiAlign::LeftIndented, imguiRGBA(255,0,0,0));

                    if (confirmed)
                    {
                        _state.m_action = LeftScrollAreaState::RemoveMaterial;
                        _state.m_data   = matHandle.m_idx;
                        _state.m_events = GuiEvent::HandleAction;

                        _guiState.m_confirmMaterialRemoval = false;
                    }

                }

                imguiRegion("Preview", NULL, _guiState.m_showMaterialPreview);
                if (_guiState.m_showMaterialPreview)
                {
                    static const float s_eye[3] = { 0.0f, 0.0f, -1.0f };
                    updateMaterialPreview(matHandle, _currentEnv, s_eye);
                    imguiImage(getMaterialTexture(), 0.0f, 0.5f, 1.0f, ImguiAlign::CenterIndented, true, g_originBottomLeft);
                }

                // Material selection.
                enum { MaterialSelectionTopPadding = 2 };
                imguiSeparator(MaterialSelectionTopPadding);
                imguiLabel("  Selected material:");
                const int32_t scrollHeight = Gui::MaterialScrollRegion
                                           - MaterialSelectionTopPadding
                                           + (_guiState.m_showMaterialPreview ? 0 : 124);
                imguiBeginScroll(scrollHeight, &_state.m_materialSelectionScroll, _enabled);
                imguiIndent();
                {
                    uint16_t selectedMaterial = matHandle.m_idx;
                    for (uint16_t ii = 0, end = _materialList.count(); ii < end; ++ii)
                    {
                        imguiCheckSelection(getName(_materialList[ii]), selectedMaterial, _materialList[ii].m_idx);
                    }

                    // OnMaterialChange.
                    if (selectedMaterial != matHandle.m_idx)
                    {
                        const cs::MaterialHandle handle = { selectedMaterial };
                        instance.set(handle, instance.m_selGroup);
                    }

                    if (imguiButton("Create new"))
                    {
                        _state.m_action = LeftScrollAreaState::CreateMaterial;
                        _state.m_events = GuiEvent::HandleAction;
                    }

                    imguiSeparator(2);
                }
                imguiUnindent();
                imguiEndScroll();
            }
            imguiUnindent();
        }

        imguiEndScroll();

        // Spheres options.
        {
            bool alwaysTrue = true;
            imguiRegionBorder("Model", NULL, alwaysTrue, false);
            imguiIndent();
            {
                // Position.
                imguiLabel(imguiRGBA(128,128,128,196)
                         , "Postition [x, y, z]:           [%5.2f, %5.2f, %5.2f]"
                         , _meshCurrent.m_pos[0]
                         , _meshCurrent.m_pos[1]
                         , _meshCurrent.m_pos[2]
                         );
                if (imguiButton("Reset position", true, ImguiAlign::CenterIndented))
                {
                    _state.m_action = LeftScrollAreaState::ResetMeshPosition;
                    _state.m_events = GuiEvent::HandleAction;
                }

                // Scale.
                imguiLabel(imguiRGBA(128,128,128,196)
                         , "Scale:\t                 [%4.2f]"
                         , _meshCurrent.m_scale
                         );
                if (imguiButton("Reset scale", true, ImguiAlign::CenterIndented))
                {
                    _state.m_action = LeftScrollAreaState::ResetMeshScale;
                    _state.m_events = GuiEvent::HandleAction;
                }
            }
            imguiUnindent();
        }
    }
    else if (LeftScrollAreaState::MaterialTab == _state.m_selectedTab)
    {
        // Material options.

        imguiInput("Name:", matName, 32);
        imguiIndent();
        {
            imguiSeparator(10);

            const int32_t widgetX = imguiGetWidgetX();
            const int32_t widgetY = imguiGetWidgetY();
            const int32_t widgetW = imguiGetWidgetW();

            const float imageScale = 0.8f;
            const int32_t imageSize = int32_t(imageScale*float(widgetW));
            const int32_t imageX = (widgetX + widgetW + _x - imageSize)/2 + 2;
            const int32_t imageY = widgetY;

            if (_enabled)
            {
                imguiDrawRect(float(imageX    -  5)
                            , float(imageY    -  5)
                            , float(imageSize + 10)
                            , float(imageSize + 10)
                            , imguiRGBA(255,196,0,128)
                            );
            }
            imguiImage(getMaterialTexture(), 0.0f, imageScale, 1.0f, ImguiAlign::CenterIndented, true, g_originBottomLeft);

            const bool mouseDown = Mouse::Down == _mouse.m_left
                                || Mouse::Down == _mouse.m_right
                                || Mouse::Down == _mouse.m_middle;
            if (_enabled
            &&  mouseDown
            &&  dm::inside(_mouse.m_curr.m_mx, _mouse.m_curr.m_my, imageX, imageY, imageSize, imageSize))
            {
                _state.m_action = LeftScrollAreaState::UpdateMaterialPreview;
                _state.m_events = GuiEvent::HandleAction;
                memcpy(&_state.m_mouse, &_mouse, sizeof(Mouse));
            }
        }
        imguiUnindent();
        imguiSeparator(10);

        enum { PreviewHeight = 266 };
        imguiBeginScroll(height - Gui::LeftAreaHeaderHeight - PreviewHeight, &_state.m_materialScroll, _enabled);

        // Albedo.
        imguiRegionBorder("Albedo:", "", _guiState.m_showAlbedoOptions);
        if (_guiState.m_showAlbedoOptions)
        {
            imguiIndent();
            {
                const bool sampleAlbedo = (1.0f == matObj.m_albedo.sample);
                const char* albedoDesc = sampleAlbedo ? "RGB" : "---";

                // Color.
                imguiColorWheel("Color", (float*)&matObj.m_albedo, _guiState.m_showCwAlbedo, 0.6f);

                // Texture.
                imguiRegion("Texture", albedoDesc, _guiState.m_showAlbedoTexture);
                if (_guiState.m_showAlbedoTexture)
                {
                    imguiIndent();
                    {
                        const bgfx::TextureHandle tex = matObj.getBgfxHandle(cs::Material::Albedo);
                        const bool hasMat = matObj.has(cs::Material::Albedo);

                        if (imguiCheck("Sample albedo", sampleAlbedo, isValid(tex)))
                        {
                            matObj.m_albedo.sample = hasMat ? float(!sampleAlbedo) : 0.0f;
                        }

                        imguiImage(tex, 0.0f, 0.5f, 1.0f, ImguiAlign::CenterIndented, sampleAlbedo);

                        if (imguiButton(hasMat?"Change texture...":"Set", true, ImguiAlign::CenterIndented))
                        {
                            _state.m_selectedTexture = cs::Material::Albedo;

                            _state.m_action = LeftScrollAreaState::ToggleTexPicker;
                            _state.m_data   = cs::Material::Albedo;
                            _state.m_events = GuiEvent::GuiUpdate;
                        }

                        imguiSeparator(4);
                    }
                    imguiUnindent();
                }
                else
                {
                    _state.m_inactiveTexPreviews |= (1<<cs::Material::Albedo);
                }
            }
            imguiUnindent();
        }
        imguiSeparator(4);

        // Normal.
        imguiRegionBorder("Normal:", "", _guiState.m_showNormalOptions);
        if (_guiState.m_showNormalOptions)
        {
            imguiSeparator(4);
            imguiIndent();
            {
                const bool sampleNormal = (1.0f == matObj.m_normal.sample);
                const char* normalDesc = sampleNormal ? "RGB" : "---";

                // Options.
                imguiSlider("Normal multiplier", matObj.m_normal.mul, 0.0f, 1.6f, 0.1f, sampleNormal);
                imguiSlider("Specular fixup",    matObj.m_specAttn,   0.0f, 4.0f, 0.1f, sampleNormal);

                // Texture.
                imguiRegion("Texture", normalDesc, _guiState.m_showNormalTexture);
                if (_guiState.m_showNormalTexture)
                {
                    imguiIndent();
                    {
                        const bgfx::TextureHandle tex = matObj.getBgfxHandle(cs::Material::Normal);
                        const bool hasMat = matObj.has(cs::Material::Normal);

                        if (imguiCheck("Sample normal", sampleNormal, isValid(tex)))
                        {
                            matObj.m_normal.sample = hasMat ? float(!sampleNormal) : 0.0f;
                        }

                        imguiImage(tex, 0.0f, 0.5f, 1.0f, ImguiAlign::CenterIndented, sampleNormal);

                        if (imguiButton(hasMat?"Change texture...":"Set", true, ImguiAlign::CenterIndented))
                        {
                            _state.m_selectedTexture = cs::Material::Normal;

                            _state.m_action = LeftScrollAreaState::ToggleTexPicker;
                            _state.m_data   = cs::Material::Normal;
                            _state.m_events = GuiEvent::GuiUpdate;
                        }

                        imguiSeparator(4);
                    }
                    imguiUnindent();
                }
                else
                {
                    _state.m_inactiveTexPreviews |= (1<<cs::Material::Normal);
                }
            }
            imguiUnindent();
        }
        imguiSeparator(4);

        // Surface.
        imguiRegionBorder("Surface:", "", _guiState.m_showSurfaceOptions);
        if (_guiState.m_showSurfaceOptions)
        {
            imguiSeparator(4);
            imguiIndent();
            {
                const bool hasMat = matObj.has(cs::Material::Surface);
                const bool sampleSurface = (1.0f == matObj.m_surface.sample);
                const uint8_t surfaceChannel = getIdx(matObj.m_swizSurface);
                const char* surfaceDesc = sampleSurface ? &("R\0G\0B\0A\0"[surfaceChannel*2]) : "-";

                // Glossiness/Roughness selection tabs.
                const int8_t highlighted = hasMat ? int8_t(matObj.m_invGloss) : 1;
                const int8_t selected = imguiTabs(highlighted, hasMat, ImguiAlign::LeftIndented, 18, 2, 2, "Roughness", "Glossiness");
                matObj.m_invGloss = float(selected);
                imguiSeparator(2);

                // Glossiness/Roughness slider.
                const bool isGlossiness = hasMat ? 1.0f == matObj.m_invGloss : true;
                imguiSlider(isGlossiness ? "Glossiness" : "Roughness", matObj.m_surface.g, 0.0f, 1.0f, 0.01f);

                // Texture.
                imguiRegion("Texture", surfaceDesc, _guiState.m_showSurfaceTexture);
                if (_guiState.m_showSurfaceTexture)
                {
                    imguiIndent();
                    {
                        const bgfx::TextureHandle tex = matObj.getBgfxHandle(cs::Material::Surface);

                        if (imguiCheck("Sample surface", sampleSurface, isValid(tex)))
                        {
                            matObj.m_surface.sample = hasMat ? float(!sampleSurface) : 0.0f;
                        }

                        imguiImageChannel(tex, surfaceChannel, 0.0f, 0.5f, 1.0f, ImguiAlign::CenterIndented, sampleSurface);
                        imguiTexSwizzle(matObj.m_swizSurface, sampleSurface);

                        if (imguiButton(hasMat?"Change texture...":"Set", true, ImguiAlign::CenterIndented))
                        {
                            _state.m_selectedTexture = cs::Material::Surface;

                            _state.m_action = LeftScrollAreaState::ToggleTexPicker;
                            _state.m_data   = cs::Material::Surface;
                            _state.m_events = GuiEvent::GuiUpdate;
                        }

                        imguiSeparator(4);
                    }
                    imguiUnindent();
                }
                else
                {
                    _state.m_inactiveTexPreviews |= (1<<cs::Material::Surface);
                }
            }
            imguiUnindent();
        }
        imguiSeparator(4);

        // Reflectivity.
        imguiRegionBorder("Reflectivity:", "", _guiState.m_showReflectivityOptions);
        if (_guiState.m_showReflectivityOptions)
        {
            imguiSeparator(4);
            imguiIndent();
            {
                int8_t reflectivityModel = int8_t(matObj.m_metalOrSpec);

                const bool isMetalness = (0 == reflectivityModel);
                const bool sampleSpecular = (1.0f == matObj.m_specular.sample);
                const uint8_t reflectivityChannel = getIdx(matObj.m_swizReflectivity);
                const char* reflectivityDesc = (!sampleSpecular) ? "-"
                                             : (isMetalness)         ? &("R\0G\0B\0A\0"[reflectivityChannel*2])
                                             : "RGBA";

                // Options.
                reflectivityModel = imguiTabs(reflectivityModel, true, ImguiAlign::LeftIndented, 18, 2, 2, "Metalness", "Specular");
                matObj.m_metalOrSpec = float(reflectivityModel);
                imguiSeparator(2);

                imguiSlider(isMetalness?"Metalness":"Diff - Spec", matObj.m_reflectivity, 0.0f, 1.0f, 0.01f);

                if (!isMetalness)
                {
                    imguiSlider("Fresnel", matObj.m_fresnel, 0.0f, 1.0f, 0.01f);
                    imguiColorWheel("Color", (float*)&matObj.m_specular, _guiState.m_showCwSpecular, 0.6f);
                }

                // Texture.
                imguiRegion("Texture", reflectivityDesc, _guiState.m_showReflectivityTexture);
                if (_guiState.m_showReflectivityTexture)
                {
                    imguiIndent();
                    {
                        const bgfx::TextureHandle tex = matObj.getBgfxHandle(cs::Material::Reflectivity);
                        const bool hasMat = matObj.has(cs::Material::Reflectivity);

                        if (imguiCheck("Sample reflectivity", sampleSpecular, isValid(tex)))
                        {
                            matObj.m_specular.sample = hasMat ? float(!sampleSpecular) : 0.0f;
                        }

                        imguiImageChannel(tex, reflectivityChannel, 0.0f, 0.5f, 1.0f, ImguiAlign::CenterIndented, sampleSpecular);

                        if (isMetalness)
                        {
                            imguiBool("Inverse", matObj.m_invMetalness, sampleSpecular);
                            imguiTexSwizzle(matObj.m_swizReflectivity, sampleSpecular && isMetalness);
                        }

                        if (imguiButton(hasMat?"Change texture...":"Set", true, ImguiAlign::CenterIndented))
                        {
                            _state.m_selectedTexture = cs::Material::Reflectivity;

                            _state.m_action = LeftScrollAreaState::ToggleTexPicker;
                            _state.m_data   = cs::Material::Reflectivity;
                            _state.m_events = GuiEvent::GuiUpdate;
                        }

                        imguiSeparator(4);
                    }
                    imguiUnindent();
                }
                else
                {
                    _state.m_inactiveTexPreviews |= (1<<cs::Material::Reflectivity);
                }

            }
            imguiUnindent();
        }
        imguiSeparator(4);

        // Occlusion.
        imguiRegionBorder("Occlusion:", "", _guiState.m_showOcclusionOptions);
        if (_guiState.m_showOcclusionOptions)
        {
            imguiSeparator(4);
            imguiIndent();
            {
                const bool sampleOcclusion = (1.0f == matObj.m_occlusionSample);
                const uint8_t occlusionChannel = getIdx(matObj.m_swizOcclusion);
                const char* occlusionDesc = sampleOcclusion ? &("R\0G\0B\0A\0"[occlusionChannel*2]) : "-";

                // Options.
                imguiSlider("Intensity", matObj.m_aoBias, -0.7f, 0.7f, 0.01f, sampleOcclusion);

                // Texture.
                imguiRegion("Texture", occlusionDesc, _guiState.m_showOcclusionTexture);
                if (_guiState.m_showOcclusionTexture)
                {
                    imguiIndent();
                    {
                        const bgfx::TextureHandle tex = matObj.getBgfxHandle(cs::Material::Occlusion);
                        const bool hasMat = matObj.has(cs::Material::Occlusion);

                        if (imguiCheck("Sample occlusion", sampleOcclusion, isValid(tex)))
                        {
                            matObj.m_occlusionSample = hasMat ? float(!sampleOcclusion) : 0.0f;
                        }

                        imguiImageChannel(tex, occlusionChannel, 0.0f, 0.5f, 1.0f, ImguiAlign::CenterIndented, sampleOcclusion);
                        imguiTexSwizzle(matObj.m_swizOcclusion, sampleOcclusion);

                        if (imguiButton(hasMat?"Change texture...":"Set", true, ImguiAlign::CenterIndented))
                        {
                            _state.m_selectedTexture = cs::Material::Occlusion;

                            _state.m_action = LeftScrollAreaState::ToggleTexPicker;
                            _state.m_data   = cs::Material::Occlusion;
                            _state.m_events = GuiEvent::GuiUpdate;
                        }

                        imguiSeparator(4);
                    }
                    imguiUnindent();
                }
                else
                {
                    _state.m_inactiveTexPreviews |= (1<<cs::Material::Occlusion);
                }
            }
            imguiUnindent();
        }
        imguiSeparator(4);

        // Emissive.
        imguiRegionBorder("Emissive:", "", _guiState.m_showEmissiveOptions);
        if (_guiState.m_showEmissiveOptions)
        {
            imguiSeparator(4);
            imguiIndent();
            {
                const bool sampleEmissive = (1.0f == matObj.m_emissive.sample);
                const char* emissiveDesc = sampleEmissive ? "RGB" : "---";

                // Options.
                imguiSlider("Intensity", matObj.m_emissiveIntensity, 0.0f, 8.0f, 0.1f, sampleEmissive);
                imguiColorWheel("Color", (float*)&matObj.m_emissive, _guiState.m_showCwEmissive, 0.6f, sampleEmissive);

                // Texture.
                imguiRegion("Texture", emissiveDesc, _guiState.m_showEmissiveTexture);
                if (_guiState.m_showEmissiveTexture)
                {
                    imguiIndent();
                    {
                        const bgfx::TextureHandle tex = matObj.getBgfxHandle(cs::Material::Emissive);
                        const bool hasMat = matObj.has(cs::Material::Emissive);

                        if (imguiCheck("Sample emissive", sampleEmissive, isValid(tex)))
                        {
                            matObj.m_emissive.sample = hasMat ? float(!sampleEmissive) : 0.0f;
                        }

                        imguiImage(tex, 0.0f, 0.5f, 1.0f, ImguiAlign::CenterIndented, sampleEmissive);

                        if (imguiButton(hasMat?"Change texture...":"Set", true, ImguiAlign::CenterIndented))
                        {
                            _state.m_selectedTexture = cs::Material::Emissive;

                            _state.m_action = LeftScrollAreaState::ToggleTexPicker;
                            _state.m_data   = cs::Material::Emissive;
                            _state.m_events = GuiEvent::GuiUpdate;
                        }

                        imguiSeparator(4);
                    }
                    imguiUnindent();
                }
                else
                {
                    _state.m_inactiveTexPreviews |= (1<<cs::Material::Emissive);
                }
            }
            imguiUnindent();
        }
        imguiSeparator(4);

        imguiEndScroll();
    }
    else //if (LeftScrollAreaState::SpheresTab == _state.m_selectedTab).
    {
        // Spheres.
        imguiBeginScroll(height - Gui::LeftAreaHeaderHeight - Gui::SpheresTabFooterHeight, &_state.m_spheresScroll, _enabled);
        {
            char msg[8];
            bx::snprintf(msg, sizeof(msg), "%d - %d", int32_t(_settings.m_spheres.m_vertCount), int32_t(_settings.m_spheres.m_horizCount));
            bool alwaysTrue = true;
            imguiRegionBorder("Spheres", msg, alwaysTrue, false);
            imguiIndent();

            // Vertical check boxes.
            imguiLabel(imguiRGBA(128,128,128,196), _settings.m_spheres.m_horizMetalness
                      ? "Vertical:\t         Glossiness"
                      : "Vertical:\t         Metalness"
                      );
            imguiIndent();
            {
                if (imguiCheck("Decrement value", !_settings.m_spheres.m_vertIncr))
                {
                    _settings.m_spheres.m_vertIncr = false;
                }
                if (imguiCheck("Increment  value", _settings.m_spheres.m_vertIncr))
                {
                    _settings.m_spheres.m_vertIncr = true;
                }
            }
            imguiUnindent();

            // Horizontal check boxes.
            imguiLabel(imguiRGBA(128,128,128,196), _settings.m_spheres.m_horizMetalness
                      ? "Horizontal:\t         Metalness"
                      : "Horizontal:\t         Glossiness"
                      );
            imguiIndent();
            {
                if (imguiCheck("Decrement value", !_settings.m_spheres.m_horizIncr))
                {
                    _settings.m_spheres.m_horizIncr = false;
                }
                if (imguiCheck("Increment  value", _settings.m_spheres.m_horizIncr))
                {
                    _settings.m_spheres.m_horizIncr = true;
                }
            }
            imguiUnindent();

            // Swap button.
            if (imguiButton("Swap"))
            {
                _settings.m_spheres.m_horizMetalness = !_settings.m_spheres.m_horizMetalness;
            }
            imguiSeparator(4);

            // Count sliders.
            imguiSlider("Vertical count",   _settings.m_spheres.m_vertCount,  1.0f, 12.0f, 1.0f);
            imguiSlider("Horizontal count", _settings.m_spheres.m_horizCount, 1.0f, 12.0f, 1.0f);
            imguiSeparator(4);

            // Material selection.
            imguiLabel("Material:");
            imguiIndent();
            {
                _settings.m_spheres.m_matSelection = (uint8_t)imguiTabs(_settings.m_spheres.m_matSelection
                                                                       , true, ImguiAlign::LeftIndented
                                                                       , 16, 2, 3
                                                                       , "Plain"
                                                                       , "Stripes"
                                                                       , "Bricks"
                                                                       );
            }
            imguiUnindent();
            imguiSeparator(4);

            // Color options.
            imguiLabel("Color:");
            imguiIndent();
            {
                imguiBool("Same color", _guiState.m_sameColor);

                imguiColorWheel(_settings.m_spheres.m_leftToRightColor ? "Left" : "Top"
                              , _settings.m_spheres.m_color0, _guiState.m_showCwSpheresAlbedo0, 0.63f);

                if (_guiState.m_sameColor)
                {
                     memcpy(_settings.m_spheres.m_color1, _settings.m_spheres.m_color0, 3*sizeof(float));
                }

                imguiColorWheel(_settings.m_spheres.m_leftToRightColor ? "Right" : "Bottom"
                              , _settings.m_spheres.m_color1, _guiState.m_showCwSpheresAlbedo1, 0.63f);

                if (_guiState.m_sameColor)
                {
                     memcpy(_settings.m_spheres.m_color0, _settings.m_spheres.m_color1, 3*sizeof(float));
                }

                if (imguiButton("Swap orientation"))
                {
                    dm::toggle(_settings.m_spheres.m_leftToRightColor);
                }
            }
            imguiUnindent();

            imguiUnindent();
            imguiSeparator();

        }
        imguiEndScroll();

        // Model options.
        bool alwaysTrue = true;
        imguiRegionBorder("Model", NULL, alwaysTrue, false);
        imguiIndent();
        {
            imguiSeparator(4);
            const uint8_t followCamera = uint8_t(_settings.m_spheres.m_followCamera);
            const uint8_t sel = imguiTabs(followCamera, true, ImguiAlign::Center, 20, 2, 2, "Manual control", "Follow camera");
            _settings.m_spheres.m_followCamera = (1 == sel);
            imguiSeparator(3);

            // On control change.
            if (sel != followCamera)
            {
                _state.m_action = LeftScrollAreaState::SpheresControlChanged;
                _state.m_events = GuiEvent::HandleAction;
            }

            // Position.
            if (_settings.m_spheres.m_followCamera)
            {
                imguiLabel(imguiRGBA(128,128,128,196)
                         , "Vertical postition [y]:                         [%5.2f]"
                         , _settings.m_spheres.m_pos[1]
                         );
            }
            else
            {
                imguiLabel(imguiRGBA(128,128,128,196)
                         , "Postition [x, y, z]:           [%5.2f, %5.2f, %5.2f]"
                         , _settings.m_spheres.m_pos[0]
                         , _settings.m_spheres.m_pos[1]
                         , _settings.m_spheres.m_pos[2]
                         );
            }
            if (imguiButton("Reset position", true, ImguiAlign::Center))
            {
                _state.m_action = LeftScrollAreaState::ResetSpheresPosition;
                _state.m_events = GuiEvent::HandleAction;
            }

            // Inclination
            imguiLabel(imguiRGBA(128,128,128,196), "Inclination:\t                [%5.2f]", _settings.m_spheres.m_incl);
            if (imguiButton("Reset inclination", true, ImguiAlign::Center))
            {
                _state.m_action = LeftScrollAreaState::ResetSpheresInclination;
                _state.m_events = GuiEvent::HandleAction;
            }

            // Scale.
            imguiLabel(imguiRGBA(128,128,128,196), "Scale:\t                 [%4.2f]", _settings.m_spheres.m_scale);
            if (imguiButton("Reset scale", true, ImguiAlign::Center))
            {
                _state.m_action = LeftScrollAreaState::ResetSpheresScale;
                _state.m_events = GuiEvent::HandleAction;
            }

        }
        imguiUnindent();
        imguiSeparator();
    }

    imguiEndArea();
}

void imguiRightScrollArea(int32_t _x
                        , int32_t _y
                        , uint32_t _width
                        , uint32_t _height
                        , ImguiState& _guiState
                        , Settings& _settings
                        , const Mouse& _mouse
                        , const cs::EnvList& _envList
                        , RightScrollAreaState& _state
                        , bool _enabled
                        , uint8_t _viewId
                        )
{
    const uint32_t height = _height+2;

    const cs::EnvHandle envHandle = _envList[_settings.m_selectedEnvMap];
    cs::Environment&    envObj    = cs::getObj(envHandle);
    char*               envName   = cs::getName(envHandle);

    _state.m_events = GuiEvent::None;

    // Tabs.
    //
    imguiBeginArea(NULL, _x, _y, _width, height, _enabled);

    imguiSeparator(8);
    imguiIndent();
    {
        const uint8_t prevTabSelection = _state.m_selectedTab;
        _state.m_selectedTab = imguiTabs(_state.m_selectedTab, true, ImguiAlign::Center, 20, 9, 2, "Environment", "Options");

        if (prevTabSelection != _state.m_selectedTab)
        {
            _state.m_action = RightScrollAreaState::HideEnvWidget;
            _state.m_events = GuiEvent::GuiUpdate;
        }
    }
    imguiSeparator(11);
    imguiUnindent();

    // Content.
    //
    imguiBeginScroll(height - Gui::RightAreaHeaderHeight - Gui::RightAreaFooterHeight, &_state.m_envScroll, _enabled);

    if (0 == _state.m_selectedTab)
    {
        // Environment map options.
        imguiRegionBorder("Environment", envName, _guiState.m_showEnvironmentMapOptions);
        if (_guiState.m_showEnvironmentMapOptions)
        {
            imguiSeparator(4);
            imguiIndent();
            {
                imguiInput("Name:", envName, 32);
                if (imguiButton("Edit"))
                {
                    _state.m_action = RightScrollAreaState::ToggleEnvWidget;
                    _state.m_events = GuiEvent::GuiUpdate;
                }

                if (imguiConfirmButton("Remove", "Remove", "Confirm removal !", _guiState.m_confirmEnvMapRemoval))
                {
                    _state.m_action = RightScrollAreaState::RemoveEnv;
                    _state.m_data   = _settings.m_selectedEnvMap;
                    _state.m_events = GuiEvent::HandleAction;
                }

                imguiRegion("Preview", NULL, _guiState.m_showEnvPreview);
                if (_guiState.m_showEnvPreview)
                {
                    imguiIndent();
                    enum { EnvPreviewWidth = 200 };

                    const uint32_t widgetX = (uint32_t)imguiGetWidgetX();
                    const uint32_t widgetY = (uint32_t)imguiGetWidgetY();
                    imguiSeparator(EnvPreviewWidth/2 + 4);
                    const bool clicked = imguiEnvPreview(widgetX
                                                       , widgetY
                                                       , EnvPreviewWidth
                                                       , envHandle
                                                       , _mouse
                                                       , _enabled
                                                       , _viewId
                                                       );
                    if (clicked)
                    {
                        _state.m_action = RightScrollAreaState::ToggleEnvWidget;
                        _state.m_events = GuiEvent::GuiUpdate;
                    }
                    imguiUnindent();
                }

                imguiRegion("Selection", NULL, _guiState.m_showEnvSelection);
                if (_guiState.m_showEnvSelection)
                {
                    const uint16_t maxNum = DM_MIN(_envList.count()+1, 6);
                    const int32_t scrollHeight = ListElementHeight*maxNum
                                               + (_guiState.m_showEnvPreview ? 0 : 104);
                    imguiBeginScroll(scrollHeight, &_state.m_envSelectionScroll, _enabled);
                    imguiIndent();
                    {
                        for (uint16_t ii = 0, end = _envList.count(); ii < end; ++ii)
                        {
                            imguiCheckSelection(cs::getName(_envList[ii]), _settings.m_selectedEnvMap, ii);
                        }

                        if (imguiButton("Add new"))
                        {
                            _state.m_action = RightScrollAreaState::CreateEnv;
                            _state.m_events = GuiEvent::HandleAction;
                        }
                        imguiSeparator(2);
                    }
                    imguiUnindent();
                    imguiEndScroll();
                }
            }
            imguiUnindent();
            imguiSeparator();
        }
        else
        {
            _state.m_action = RightScrollAreaState::HideEnvWidget;
            _state.m_events = GuiEvent::GuiUpdate;
        }

        imguiSeparator(4);

        // Ambient lighting options.
        char ambientDescStr[32];
        bx::snprintf(ambientDescStr, sizeof(ambientDescStr), "%s%s%s"
                   , _settings.m_diffuseIbl                         ? "Diff" : ""
                   , _settings.m_diffuseIbl&_settings.m_specularIbl ? " - "  : ""
                   , _settings.m_specularIbl                        ? "Spec" : ""
                   );
        imguiRegionBorder("Ambient lighting", ambientDescStr, _guiState.m_showAmbientLighting);
        if (_guiState.m_showAmbientLighting)
        {
            imguiIndent();
            imguiBool("IBL Diffuse"
                     , _settings.m_diffuseIbl
                     , bgfx::isValid(cs::textureGetBgfxHandle(envObj.m_cubemap[cs::Environment::Iem]))
                     );
            imguiBool("IBL Specular"
                    , _settings.m_specularIbl
                    , bgfx::isValid(cs::textureGetBgfxHandle(envObj.m_cubemap[cs::Environment::Pmrem]))
                    );
            imguiSlider("Ambient light strenght"
                      , _settings.m_ambientLightStrenght
                      , 0.1f, 3.0f, 0.1f
                      , _settings.m_diffuseIbl || _settings.m_specularIbl
                      );

            bool warpFixup = (cmft::EdgeFixup::Warp == envObj.m_edgeFixup);
            imguiBool("Warp filtered cubemap", warpFixup);
            envObj.m_edgeFixup = warpFixup ? cmft::EdgeFixup::Warp : cmft::EdgeFixup::None;

            imguiUnindent();
            imguiSeparator();
        }
        imguiSeparator(4);

        // Count enabled lights.
        uint8_t lightCount = 0;
        for (uint8_t ii = 0; ii < CS_MAX_LIGHTS; ++ii)
        {
            if (1.0f == envObj.m_lights[ii].m_enabled)
            {
                lightCount++;
            }
        }
        char lightCountStr[8];
        bx::snprintf(lightCountStr, sizeof(lightCountStr), "%u", lightCount);

        // Direct lighting options.
        imguiRegionBorder("Direct lighting", lightCountStr, _guiState.m_showDirectLighting);
        if (_guiState.m_showDirectLighting)
        {
            imguiIndent();

            // Lighting.
            static const char* s_selLightingModel[] =
            {
                "Phong",
                "Phongbrdf",
                "Blinn",
                "Blinnbrdf",
            };

            imguiRegion("Lighting", s_selLightingModel[_settings.m_selectedLightingModel], _guiState.m_showLightingOptions);
            if (_guiState.m_showLightingOptions)
            {
                imguiIndent();
                imguiSelection(_settings.m_selectedLightingModel, s_selLightingModel, BX_COUNTOF(s_selLightingModel));
                imguiUnindent();
            }

            const int32_t widgetX = imguiGetWidgetX();
            const int32_t widgetY = imguiGetWidgetY();
            imguiSeparator(Gui::LatlongWidgetHeight + 4);
            imguiLatlongWidget(widgetX
                             , widgetY
                             , Gui::LatlongWidgetHeight
                             , envHandle
                             , _settings.m_selectedLight
                             , _mouse
                             , _enabled
                             , _viewId
                             );

            if (UINT8_MAX != _settings.m_selectedLight)
            {
                const bool cwEnabled = !envObj.m_lightUseBackgroundColor[_settings.m_selectedLight];

                imguiSlider("Strenght",          envObj.m_lights[_settings.m_selectedLight].m_strenght, 0.0f, 5.0f, 0.1f);
                imguiBool("Use backround color", envObj.m_lightUseBackgroundColor[_settings.m_selectedLight]);
                imguiColorWheel("Custom color",  envObj.m_lights[_settings.m_selectedLight].m_color, _guiState.m_showCwLight, 0.63f, cwEnabled);
            }
            else
            {
                imguiValue("(right click on the image to add a light)");
            }

            imguiUnindent();
            imguiSeparator();
        }
        imguiSeparator(4);

        // Background options.

        imguiRegionBorder("Background", getCubemapTypeStr((cs::Environment::Enum)_settings.m_backgroundType), _guiState.m_showBackgroundOptions);
        if (_guiState.m_showBackgroundOptions)
        {
            imguiIndent();
            imguiSelection(_settings.m_backgroundType, sc_cubemapTypeStr, cs::Environment::Count);

            if (1 == _settings.m_backgroundType) // Radiance
            {
                const float mipLevel = float(envObj.m_cubemapImage[cs::Environment::Pmrem].m_numMips-1);
                _settings.m_backgroundMipLevel = dm::min(_settings.m_backgroundMipLevel, mipLevel);

                imguiIndent();
                imguiSlider("Lod", _settings.m_backgroundMipLevel, 0.0f, mipLevel, 0.1f);
                imguiUnindent();
            }

            imguiUnindent();
            imguiSeparator(2);
        }

    }
    else if (1 == _state.m_selectedTab)
    {
        // Hdr options.
        imguiRegionBorder("Hdr", "", _guiState.m_showHdrOptions);
        if (_guiState.m_showHdrOptions)
        {
            imguiIndent();
            imguiBool("Bloom",          _settings.m_doBloom);
            imguiBool("Light adapt",    _settings.m_doLightAdapt);
            imguiSlider("White",        _settings.m_white,       0.1f,  2.0f, 0.001f, _settings.m_doBloom);
            imguiSlider("Treshold",     _settings.m_treshold,    0.01f, 5.0f, 0.001f, _settings.m_doBloom);
            imguiSlider("Gauss StdDev", _settings.m_gaussStdDev, 0.1f,  1.0f, 0.001f, _settings.m_doBloom);
            imguiSlider("Blur coeff",   _settings.m_blurCoeff,   0.0f,  2.2f, 0.001f, _settings.m_doBloom);
            imguiSlider("Middle gray",  _settings.m_middleGray,  0.1f,  1.0f, 0.001f);
            imguiSeparator();
            if (imguiButton("Reset"))
            {
                _settings.resetHdrSettings();
            }
            imguiUnindent();
            imguiSeparator();
        }
        imguiSeparator(4);

        // Tone mapping.
        static const char* s_toneMapping[ToneMapping::Count] =
        {
            "Linear",
            "Gamma",
            "Reinhard",
            "Reinhard2",
            "Filmic",
            "Uncharted2",
        };

        imguiRegionBorder("Tone mapping", s_toneMapping[_settings.m_selectedToneMapping], _guiState.m_showToneMapping);
        if (_guiState.m_showToneMapping)
        {
            imguiIndent();
            imguiSelection(_settings.m_selectedToneMapping, s_toneMapping, ToneMapping::Count);
            imguiUnindent();
            imguiSeparator();
        }
        imguiSeparator(4);

        // Camera options.
        imguiRegionBorder("Camera", NULL, _guiState.m_showCameraOptions);
        if (_guiState.m_showCameraOptions)
        {
            imguiIndent();

            imguiSlider("FOV", _settings.m_fovDest, 30.0f, 120.0f, 1.0f);
            imguiTabsFov(_settings.m_fovDest);

            imguiUnindent();
            imguiSeparator();
        }

        // Anti aliasing.
        imguiRegionBorder("Anti aliasing", NULL, _guiState.m_showAaOptions);
        if (_guiState.m_showAaOptions)
        {
            imguiIndent();
            imguiBool("MSAA", _settings.m_msaa);
            imguiBool("FXAA", _settings.m_fxaa);
            imguiUnindent();
            imguiSeparator();
        }

        // Post effects.
        imguiRegionBorder("Post effects", NULL, _guiState.m_showPostEffects);
        if (_guiState.m_showPostEffects)
        {
            imguiIndent();
            imguiSeparator(2);
            imguiSlider("Brightness", _settings.m_brightness, -1.0f, 1.0f, 0.01f);
            imguiSlider("Contrast",   _settings.m_contrast,    0.5f, 2.0f, 0.01f);
            imguiSlider("Exposure",   _settings.m_exposure,   -5.0f, 5.0f, 0.01f);
            imguiSlider("Gamma",      _settings.m_gamma,       0.1f, 8.0f, 0.01f);
            imguiSlider("Saturation", _settings.m_saturation,  0.0f, 1.0f, 0.01f);
            imguiSlider("Vignette",   _settings.m_vignette,    0.0f, 3.5f, 0.01f);

            imguiSeparator();
            if (imguiButton("Reset"))
            {
                _settings.resetPostProcess();
            }
            imguiUnindent();
            imguiSeparator();
        }

    }

    imguiEndScroll();
    imguiSeparator(4);

    bool alwaysTrue = true;
    imguiRegionBorder("cmftStudio", NULL, alwaysTrue, false);
    imguiIndent();
    {
        imguiSeparator(5);

        const uint8_t project_output = imguiTabs(UINT8_MAX, true, ImguiAlign::CenterIndented, 21, 4, 2, "Project load/save", "Output window");
        if (0 == project_output)
        {
            _state.m_action = RightScrollAreaState::ShowProjectWindow;
            _state.m_events = GuiEvent::GuiUpdate;
        }
        else if (1 == project_output)
        {
            _state.m_action = RightScrollAreaState::ShowOutputWindow;
            _state.m_events = GuiEvent::GuiUpdate;
        }

        const uint8_t button = imguiTabs(UINT8_MAX, true, ImguiAlign::CenterIndented, 21, 4, 2, "Help/About", "Full screen");
        if (0 == button)
        {
            _state.m_action = RightScrollAreaState::ShowAboutWindow;
            _state.m_events = GuiEvent::GuiUpdate;
        }
        else if (1 == button)
        {
            _state.m_action = RightScrollAreaState::ToggleFullscreen;
            _state.m_events = GuiEvent::HandleAction;
        }
    }
    imguiSeparator(4);
    imguiEndArea();
}

/* vim: set sw=4 ts=4 expandtab: */
