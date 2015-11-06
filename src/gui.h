/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_GUI_H_HEADER_GUARD
#define CMFTSTUDIO_GUI_H_HEADER_GUARD

#include <stdint.h>
#include <bgfx/bgfx.h>          // bgfx::TextureHandle
#include <cmft/cubemapfilter.h> // cmft::LightingModel
#include <dm/misc.h>            // dm::realpath, dm::strscpya
#include "mouse.h"              // Mouse
#include "context.h"            // cs::*Handle
#include "settings.h"           // Settings
#include "renderpipeline.h"     // RenderPipeline::ViewIdGui
#include "common/imgui.h"

// Constants.
//-----

struct Gui
{
    enum
    {
        PaneWidth              = 286,
        LatlongWidgetHeight    = 118,
        LeftAreaHeaderHeight   = 67,
        RightAreaHeaderHeight  = 44,
        LeftAreaFooterHeight   = 134,
        SpheresTabFooterHeight = 209,
        RightAreaFooterHeight  = 105,

        MeshScrollRegion     = 173,
        MaterialScrollRegion = 147,

        BorderButtonWidth = 16,
        HorizontalSpacing = 8,
        FirstPaneY        = -2,
        SecondPaneY       = 7,
    };
};

static const char* sc_cubemapTypeStr[cs::Environment::Count] =
{
    "Skybox",
    "Radiance",
    "Irradiance",
};

static inline const char* getCubemapTypeStr(cs::Environment::Enum _env)
{
    return sc_cubemapTypeStr[_env];
}


// Fonts.
//-----

struct Fonts
{
    enum Enum
    {
        Default,

        #define FONT_DESC_FILE(_name, _fontSize, _path) _name,
        #define FONT_DESC_MEM(_name, _fontSize, _data) _name,
        #include "gui_res.h"

        Count,
    };
};

void imguiSetFont(Fonts::Enum _font);
float imguiGetTextLength(const char* _text, Fonts::Enum _font);

struct ImguiFontScope
{
    ImguiFontScope(Fonts::Enum _font)
    {
        m_previousFont = imguiGetCurrentFont();
        imguiSetFont(_font);
    }

    ~ImguiFontScope()
    {
        imguiSetFont(m_previousFont);
    }

private:
    ImguiFontHandle m_previousFont;
};


// Gui.
//-----

void guiInit();
void guiDrawOverlay();
void guiDestroy();

struct GuiEvent
{
    enum Enum
    {
        None = 0x0,

        HandleAction  = 0x1,
        DismissWindow = 0x2,
        GuiUpdate     = 0x4,
    };
};

static inline bool guiEvent(GuiEvent::Enum _event, uint8_t& _events)
{
    if (_events & _event)
    {
        _events ^= _event;
        return true;
    }

    return false;
}

struct SelectedTexture
{
    enum Enum
    {
        None,
        Albedo,
        Normal,
        Reflectivity,
        Surface,
        Occlusion,
        Emissive,

        Count,
    };
};

struct AntiAlias
{
    enum Enum
    {
        None,
        Msaa,
        Fxaa,
    };
};

struct ImguiState
{
    ImguiState()
    {
        // Regions.
        m_showLightingOptions       = false;
        m_showEnvironmentMapOptions = true;
        m_showBackgroundOptions     = true;
        m_showToneMapping           = true;
        m_showPostEffects           = true;
        m_showAaOptions             = true;
        m_showCameraOptions         = true;
        m_showAmbientLighting       = true;
        m_showDirectLighting        = true;
        m_showHdrOptions            = true;
        m_showMeshesOptions         = true;
        m_showMaterials             = true;
        m_showMaterialOptions       = true;
        m_showAlbedoOptions         = true;
        m_showNormalOptions         = true;
        m_showSurfaceOptions        = true;
        m_showReflectivityOptions   = true;
        m_showOcclusionOptions      = true;
        m_showEmissiveOptions       = true;
        m_showAlbedoTexture         = false;
        m_showNormalTexture         = false;
        m_showSurfaceTexture        = false;
        m_showReflectivityTexture   = false;
        m_showOcclusionTexture      = false;
        m_showEmissiveTexture       = false;
        // Preview.
        m_showMeshPreview       = true;
        m_showMeshGroups        = true;
        m_showEnvPreview        = true;
        m_showEnvSelection      = true;
        m_showMaterialPreview   = true;
        // Confirmation buttons.
        m_confirmMeshRemoval     = false;
        m_confirmMaterialRemoval = false;
        m_confirmEnvMapRemoval   = false;
        // Color wheel.
        m_showCwAlbedo   = false;
        m_showCwSpecular = false;
        m_showCwEmissive = false;
        m_showCwLight    = false;
        // Spheres scene.
        m_showCwSpheresAlbedo0 = true;
        m_showCwSpheresAlbedo1 = true;
        m_sameColor            = false;
    }

    // Regions.
    bool m_showLightingOptions;
    bool m_showEnvironmentMapOptions;
    bool m_showBackgroundOptions;
    bool m_showToneMapping;
    bool m_showPostEffects;
    bool m_showAaOptions;
    bool m_showCameraOptions;
    bool m_showAmbientLighting;
    bool m_showDirectLighting;
    bool m_showHdrOptions;
    bool m_showMeshesOptions;
    bool m_showMaterials;
    bool m_showMaterialOptions;
    bool m_showAlbedoOptions;
    bool m_showNormalOptions;
    bool m_showSurfaceOptions;
    bool m_showReflectivityOptions;
    bool m_showOcclusionOptions;
    bool m_showEmissiveOptions;
    bool m_showAlbedoTexture;
    bool m_showNormalTexture;
    bool m_showSurfaceTexture;
    bool m_showReflectivityTexture;
    bool m_showOcclusionTexture;
    bool m_showEmissiveTexture;

    // Preview.
    bool m_showMeshPreview;
    bool m_showMeshGroups;
    bool m_showEnvPreview;
    bool m_showEnvSelection;
    bool m_showMaterialPreview;

    // Confirmation buttons.
    bool m_confirmMeshRemoval;
    bool m_confirmMaterialRemoval;
    bool m_confirmEnvMapRemoval;

    // Color wheel.
    bool m_showCwAlbedo;
    bool m_showCwSpecular;
    bool m_showCwEmissive;
    bool m_showCwLight;

    // Spheres scene.
    bool m_showCwSpheresAlbedo0;
    bool m_showCwSpheresAlbedo1;
    bool m_sameColor;
};


// Imgui directory listing. To be called between imguiAreaBegin/End.
//-----

struct BrowserState
{
    BrowserState(const char* _directory = ".")
    {
        reset();
        dm::realpath(m_directory, _directory);
    }

    void reset()
    {
        m_scroll         = 0;
        m_windowsDrives  = false;
        m_filePath[0]    = '\0';
        m_fileName[0]    = '\0';
        m_fileExt[0]     = '\0';
        m_fileNameExt[0] = '\0';
    }

    int32_t m_scroll;
    bool m_windowsDrives;
    char m_filePath[256];
    char m_fileName[128];
    char m_fileExt[16];
    char m_fileNameExt[128+16];
    char m_directory[DM_PATH_LEN];
};

bool imguiBrowser(int32_t _height
                , BrowserState& _state
                , const char (*_ext)[16]  = NULL
                , uint8_t _extCount       = 0
                , const char* _selectFile = NULL
                , bool _showDirsOnly      = false
                , bool _showHidden        = false
                );

template <uint8_t MaxSelectedT>
struct BrowserStateFor
{
    BrowserStateFor(const char* _directory = ".")
    {
        m_scroll        = 0;
        m_windowsDrives = false;
        dm::realpath(m_directory, _directory);
    }

    struct File
    {
        char m_path[256];
        char m_name[128];
        char m_ext[16];
        char m_nameExt[128+16];
    };

    int32_t m_scroll;
    bool m_windowsDrives;
    char m_directory[DM_PATH_LEN];
    dm::ListT<File, MaxSelectedT> m_files;
};

template <uint8_t MaxSelectedT>
void imguiBrowser(int32_t _height
                , BrowserStateFor<MaxSelectedT>& _state
                , const char (*_ext)[16] = NULL
                , uint8_t _extCount      = 0
                , bool _showDirsOnly     = false
                , bool _showHidden       = false
                );

// TextureBrowser widget.
//-----

struct TextureExt
{
    enum Enum
    {
        Dds,
        Ktx,
        Tga,
        Hdr,
        Bmp,
        Gif,
        Jpg,
        Jpeg,
        Png,
        Pvr,
        Tif,
        Tiff,

        Count,
    };
};

static const char* sc_textureExtStr[TextureExt::Count] =
{
    "dds",
    "ktx",
    "tga",
    "hdr",
    "bmp",
    "gif",
    "jpg",
    "jpeg",
    "png",
    "pvr",
    "tif",
    "tiff",
};

static inline const char* getTextureExtensionStr(uint8_t _ext)
{
    CS_CHECK(_ext < TextureExt::Count, "Accessing array out of bounds.");
    return sc_textureExtStr[_ext];
}

enum { TextureBrowser_MaxSelectedFiles = 8 };
struct TextureBrowserWidgetState : public BrowserStateFor<TextureBrowser_MaxSelectedFiles>
{
    typedef BrowserStateFor<TextureBrowser_MaxSelectedFiles> BrowserState;

    TextureBrowserWidgetState(const char* _directory = ".")
        : BrowserState(_directory)
    {
        m_scroll = 0;
        for (uint8_t ii = 0; ii < TextureExt::Count; ++ii)
        {
            m_extFlag[ii] = true;
        }
        m_events = GuiEvent::None;
        m_texPickerFor = cs::Material::Albedo;
    }

    int32_t m_scroll;
    bool m_extFlag[TextureExt::Count];
    uint8_t m_events;
    cs::Material::Texture m_texPickerFor;
};

void imguiTextureBrowserWidget(int32_t _x
                             , int32_t _y
                             , int32_t _width
                             , TextureBrowserWidgetState& _state
                             , bool _enabled = true
                             );

// MeshSave widget.
//-----

struct MeshSaveWidgetState : public BrowserState
{
    MeshSaveWidgetState()
    {
        m_events = GuiEvent::None;
        dm::strscpya(m_outputName, "mesh");
    }

    cs::MeshHandle m_mesh;
    uint8_t m_events;
    char m_outputName[128];
};

void imguiMeshSaveWidget(int32_t _x
                       , int32_t _y
                       , int32_t _width
                       , MeshSaveWidgetState& _state
                       );


// MeshBrowser widget.
//-----

struct MeshBrowserState : public BrowserState
{
    MeshBrowserState(const char* _directory = ".")
        : BrowserState(_directory)
    {
        for (uint8_t ii = 0; ii < CS_MAX_GEOMETRY_LOADERS; ++ii)
        {
            m_extFlag[ii] = true;
        }
        m_objSelected     = false;
        m_flipV           = true;
        m_ccw             = false;
        m_extensionsCount = 0;
        m_events          = GuiEvent::None;
    }

    bool m_extFlag[CS_MAX_GEOMETRY_LOADERS];
    bool m_objSelected;
    bool m_flipV;
    bool m_ccw;
    uint8_t m_extensionsCount;
    uint8_t m_events;
};

void imguiMeshBrowserWidget(int32_t _x
                          , int32_t _y
                          , int32_t _width
                          , MeshBrowserState& _state
                          );


// EnvMap widget.
//-----

struct EnvMapWidgetState
{
    EnvMapWidgetState()
    {
        m_scroll    = 0;
        m_lod       = 0.0f;
        m_action    = (Action)0;
        m_events    = GuiEvent::None;
        m_selection = (cs::Environment::Enum)0;
    }

    enum Action
    {
        Info,
        Transform,
        Process,
        Save,
        Browse,
    };

    int32_t m_scroll;
    float m_lod;
    Action m_action;
    uint8_t m_events;
    cs::Environment::Enum m_selection;
};

void imguiEnvMapWidget(cs::EnvHandle _env
                     , int32_t _x
                     , int32_t _y
                     , int32_t _width
                     , EnvMapWidgetState& _state
                     , bool _enabled = true
                     );


// CmftImageInfo widget.
//-----

struct CmftInfoWidgetState
{
    CmftInfoWidgetState()
    {
        m_resize       = 256.0f;
        m_lod          = 0.0f;
        m_envType      = (cs::Environment::Enum)0;
        m_env          = cs::EnvHandle::invalid();
        m_imguiCubemap = ImguiCubemap::Cross;
        m_convertTo    = BGRA8;
        m_action       = (Action)0;
        m_events       = GuiEvent::None;
    }

    enum Action
    {
        Resize,
        Convert,
        Magnify,
    };

    enum ImageFormat
    {
        BGRA8,
        RGBA16,
        RGBA16F,
        RGBA32F,
    };

    inline cmft::TextureFormat::Enum selectedCmftFormat()
    {
        static cmft::TextureFormat::Enum s_formats[4] =
        {
            cmft::TextureFormat::BGRA8,   // BGRA8
            cmft::TextureFormat::RGBA16,  // RGBA16
            cmft::TextureFormat::RGBA16F, // RGBA16F
            cmft::TextureFormat::RGBA32F, // RGBA32F
        };

        return s_formats[m_convertTo];
    }

    float m_resize;
    float m_lod;
    cs::Environment::Enum m_envType;
    cs::EnvHandle m_env;
    uint8_t m_imguiCubemap;
    ImageFormat m_convertTo;
    Action m_action;
    uint8_t m_events;
};

void imguiCmftInfoWidget(cs::EnvHandle _env
                       , int32_t _x
                       , int32_t _y
                       , int32_t _width
                       , CmftInfoWidgetState& _state
                       , bool _enabled = true
                       );


// CmftSave widget.
//-----

struct CmftSaveWidgetState : public BrowserState
{
    CmftSaveWidgetState()
    {
        m_scrollFileType        = 0;
        m_scrollOutputType      = 0;
        m_scrollFormat          = 0;
        m_selectedFileType      = 0;
        m_selectedOutputType    = 0;
        m_selectedTextureFormat = 0;
        m_envType               = (cs::Environment::Enum)0;
        m_fileType              = (cmft::ImageFileType::Enum)0;
        m_textureFormat         = (cmft::TextureFormat::Enum)0;
        m_outputType            = (cmft::OutputType::Enum)0;
        m_events                = GuiEvent::None;
        dm::strscpya(m_outputName, "output");
        m_outputNameExt[0]      = '\0';
    }

    int32_t m_scrollFileType;
    int32_t m_scrollOutputType;
    int32_t m_scrollFormat;
    uint16_t m_selectedFileType;
    uint16_t m_selectedOutputType;
    uint16_t m_selectedTextureFormat;
    cs::Environment::Enum m_envType;
    cmft::ImageFileType::Enum m_fileType;
    cmft::TextureFormat::Enum m_textureFormat;
    cmft::OutputType::Enum m_outputType;
    uint8_t m_events;
    char m_outputName[128];
    char m_outputNameExt[128];
};

void imguiCmftSaveWidget(int32_t _x
                       , int32_t _y
                       , int32_t _width
                       , CmftSaveWidgetState& _state
                       , bool _enabled = true
                       );


// CmftPmrem widget.
//-----

struct CmftPmremWidgetState
{
    CmftPmremWidgetState()
    {
        m_srcSize       = 256.0f;
        m_dstSize       = 256.0f;
        m_inputGamma    = 2.2f;
        m_outputGamma   = 1.0f/2.2f;
        m_mipCount      = 7.0f;
        m_glossScale    = 10.0f;
        m_glossBias     = 3.0f;
        m_numCpuThreads = 4.0f;
        m_lightingModel = cmft::LightingModel::BlinnBrdf;
        m_edgeFixup     = cmft::EdgeFixup::None;
        m_events        = GuiEvent::None;
        m_excludeBase   = false;
        m_useOpenCL     = true;
    }

    float m_srcSize;
    float m_dstSize;
    float m_inputGamma;
    float m_outputGamma;
    float m_mipCount;
    float m_glossScale;
    float m_glossBias;
    float m_numCpuThreads;
    uint8_t m_lightingModel;
    uint8_t m_edgeFixup;
    uint8_t m_events;
    bool m_excludeBase;
    bool m_useOpenCL;
    bool m_powerOfTwoSrcSize;
    bool m_powerOfTwoDstSize;
};

void imguiCmftPmremWidget(cs::EnvHandle _env
                        , int32_t _x
                        , int32_t _y
                        , int32_t _width
                        , CmftPmremWidgetState& _state
                        , bool _enabled = true
                        );


// CmftIem widget.
//-----

struct CmftIemWidgetState
{
    CmftIemWidgetState()
    {
        m_srcSize     = 999999.0f;
        m_dstSize     = 128.0f;
        m_inputGamma  = 2.2f;
        m_outputGamma = 1.0f/2.2f;
        m_events      = GuiEvent::None;
    }


    float m_srcSize;
    float m_dstSize;
    float m_inputGamma;
    float m_outputGamma;
    uint8_t m_events;
};

void imguiCmftIemWidget(cs::EnvHandle _env
                      , int32_t _x
                      , int32_t _y
                      , int32_t _width
                      , CmftIemWidgetState& _state
                      , bool _enabled = true
                      );


// CmftTransform widget.
//-----

struct CmftTransformWidgetState
{
    CmftTransformWidgetState()
    {
        m_env    = cs::EnvHandle::invalid();
        m_events = GuiEvent::None;
    }

    uint16_t getTrasformaArgs()
    {
        static uint16_t s_selectionToFace[6] =
        {
            cmft::IMAGE_FACE_NEGATIVEX, // -X
            cmft::IMAGE_FACE_POSITIVEY, // +Y
            cmft::IMAGE_FACE_POSITIVEZ, // +Z
            cmft::IMAGE_FACE_NEGATIVEY, // -Y
            cmft::IMAGE_FACE_POSITIVEX, // +X
            cmft::IMAGE_FACE_NEGATIVEZ, // -Z
        };

        static uint16_t s_selectionToImageOp[5] =
        {
            cmft::IMAGE_OP_ROT_90,  // Rotate 90
            cmft::IMAGE_OP_ROT_180, // Rotate 180
            cmft::IMAGE_OP_ROT_270, // Rotate 270
            cmft::IMAGE_OP_FLIP_X,  // Flip H
            cmft::IMAGE_OP_FLIP_Y,  // Flip V
        };

        return s_selectionToFace[m_face] | s_selectionToImageOp[m_imageOp];
    }

    cs::EnvHandle m_env;
    uint8_t m_face;
    uint8_t m_imageOp;
    uint8_t m_events;
};

void imguiCmftTransformWidget(cs::EnvHandle _env
                            , int32_t _x
                            , int32_t _y
                            , int32_t _width
                            , CmftTransformWidgetState& _state
                            , const Mouse& _click
                            , bool _enabled = true
                            );


// Tonemap widget.
//-----

struct TonemapWidgetState
{
    TonemapWidgetState()
    {
        resetParams();
        m_invGamma   = 2.2f;
        m_env        = cs::EnvHandle::invalid();
        m_selection  = Tonemapped;
        m_events     = GuiEvent::None;
    }

    void resetParams()
    {
        m_lumRange = 1.0f;
        m_minLum   = 0.0f;
    }

    enum Selection
    {
        Original,
        Tonemapped,
    };

    float m_lumRange;
    float m_minLum;
    float m_invGamma;
    cs::EnvHandle m_env;
    uint8_t m_selection;
    uint8_t m_events;
};

void imguiTonemapWidget(cs::EnvHandle _env
                      , int32_t _x
                      , int32_t _y
                      , int32_t _width
                      , TonemapWidgetState& _state
                      , bool _enabled = true
                      );


// EnvMapBrowser widget.
//-----

struct EnvMapBrowserState : public BrowserState
{
    EnvMapBrowserState(const char* _directory = ".")
        : BrowserState(_directory)
    {
        m_selection = (cs::Environment::Enum)0;
        m_events    = GuiEvent::None;
        m_extDds    = true;
        m_extKtx    = true;
        m_extTga    = true;
        m_extHdr    = true;
    }

    cs::Environment::Enum m_selection;
    uint8_t m_events;
    bool m_extDds;
    bool m_extKtx;
    bool m_extTga;
    bool m_extHdr;
};

void imguiEnvMapBrowserWidget(const char* _name
                            , int32_t _x
                            , int32_t _y
                            , int32_t _width
                            , EnvMapBrowserState& _state
                            );


// TexPicker widget.
//-----

struct TexPickerWidgetState
{
    enum Action
    {
        Remove,
        Pick,
    };

    TexPickerWidgetState()
    {
        m_scroll        = 0;
        m_selection     = 0;
        m_events        = GuiEvent::None;
        m_confirmButton = false;
        dm::strscpya(m_title, "Texture");
    }

    int32_t m_scroll;
    uint16_t m_selection;
    uint8_t m_events;
    Action m_action;
    bool m_confirmButton;
    char m_title[32];
};

void imguiTexPickerWidget(int32_t _x
                        , int32_t _y
                        , int32_t _width
                        , const cs::TextureList& _textureList
                        , TexPickerWidgetState& _state
                        );


// Modal output window.
//-----

struct OutputWindowState
{
    enum
    {
        Width  = 800,
        Height = 400,

        ScrollHeight = Height-82,
        LineHeight   = 20,

        MaxLines   = 128, // Must be a power of two !
        LineLength = 128,
    };

    OutputWindowState()
    {
        for (uint8_t ii = 0; ii < MaxLines; ++ii)
        {
            m_lines[ii] = NULL;
        }
        m_scroll     = 0;
        m_linesCount = 0;
        m_events     = GuiEvent::None;
    }

    void scrollDown()
    {
        const int32_t scroll = ScrollHeight - m_linesCount*LineHeight;
        m_scroll = dm::min(scroll, 0);
    }

    char* m_lines[MaxLines];
    int32_t m_scroll;
    uint16_t m_linesCount;
    uint8_t m_events;
};

void imguiModalOutputWindow(int32_t _x, int32_t _y, OutputWindowState& _state);


// About window.
//-----

struct AboutWindowState
{
    AboutWindowState()
    {
        m_scroll = 0;
        m_events = GuiEvent::None;
    }

    int32_t m_scroll;
    uint8_t m_events;
};

void imguiModalAboutWindow(int32_t _x
                         , int32_t _y
                         , AboutWindowState& _state
                         , int32_t _width  = 800
                         , int32_t _height = 400
                         );
// Magnify window.
//-----

struct MagnifyWindowState
{
    MagnifyWindowState()
    {
        m_lod          = 0.0f;
        m_env          = cs::EnvHandle::invalid();
        m_envType      = cs::Environment::Skybox;
        m_imguiCubemap = ImguiCubemap::Latlong;
        m_events       = GuiEvent::None;
    }

    float m_lod;
    cs::EnvHandle m_env;
    cs::Environment::Enum m_envType;
    uint8_t m_imguiCubemap;
    uint8_t m_events;
};

void imguiModalMagnifyWindow(int32_t _x, int32_t _y, MagnifyWindowState& _state);


// Project window.
//-----

struct ProjectWindowState
{
    ProjectWindowState()
    {
        m_compressionLevel = 6.0f;
        m_events           = GuiEvent::None;
        m_action           = 0;
        m_tabs             = 0;
        m_confirmButton    = false;
        dm::strscpya(m_projectName, "MyProject");
    }

    enum Action
    {
        Load, // must be 0 to match gui tabs
        Save, // must be 1 to match gui tabs
        Warning,
        ShowLoadDir,
        ShowSaveDir,
    };

    float m_compressionLevel;
    uint8_t m_events;
    uint8_t m_action;
    uint8_t m_tabs;
    bool m_confirmButton;
    char m_projectName[128];
    BrowserState m_load;
    BrowserState m_save;
};

void imguiModalProjectWindow(int32_t _x
                           , int32_t _y
                           , ProjectWindowState& _state
                           );


// Custom GUI elements.
//-----

void imguiLatlongWidget(int32_t _screenX
                      , int32_t _screenY
                      , int32_t _height
                      , cs::EnvHandle _env
                      , uint8_t& _selectedLight
                      , const Mouse& _click
                      , bool _enabled = true
                      , uint8_t _viewId = RenderPipeline::ViewIdGui
                      );

bool imguiEnvPreview(uint32_t _screenX
                   , uint32_t _screenY
                   , uint32_t _areaWidth
                   , cs::EnvHandle _env
                   , const Mouse& _click
                   , bool _enabled = true
                   , uint8_t _viewId = RenderPipeline::ViewIdGui
                   );

struct LeftScrollAreaState
{
    LeftScrollAreaState()
    {
        // Scroll bar values.
        m_modelScroll             = 0;
        m_meshSelectionScroll     = 0;
        m_meshGroupsScroll        = 0;
        m_materialSelectionScroll = 0;
        m_materialScroll          = 0;
        m_spheresScroll           = 0;

        m_data                = 0;
        m_action              = (Action)0;
        m_events              = GuiEvent::None;
        m_selectedTab         = 0;
        m_prevSelectedTab     = 0;
        m_selectedTexture     = (cs::Material::Texture)0;
        m_inactiveTexPreviews = 0;
    }

    enum SelectedTab
    {
        ModelTab,
        MaterialTab,
        SpheresTab,
    };

    enum Action
    {
        SaveMesh,
        RemoveMesh,
        RemoveMaterial,
        CopyMaterial,
        CreateMaterial,
        MeshSelectionChange,
        TabSelectionChange,
        UpdateMaterialPreview,

        ResetMeshPosition,
        ResetMeshScale,

        SpheresControlChanged,
        ResetSpheresPosition,
        ResetSpheresScale,
        ResetSpheresInclination,

        ToggleMeshBrowser,
        ToggleTexPicker,
        HideLeftSideWidget,
    };

    uint16_t m_data;
    Action m_action;
    uint8_t m_events;
    uint8_t m_selectedTab;
    uint8_t m_prevSelectedTab;
    cs::Material::Texture m_selectedTexture;
    uint32_t m_inactiveTexPreviews;
    Mouse m_mouse;
    int32_t m_modelScroll;
    int32_t m_meshSelectionScroll;
    int32_t m_meshGroupsScroll;
    int32_t m_materialSelectionScroll;
    int32_t m_materialScroll;
    int32_t m_spheresScroll;
};

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
                       , bool _enabled = true
                       );

struct RightScrollAreaState
{
    RightScrollAreaState()
    {
        // Scroll bar values.
        m_envScroll          = 0;
        m_optionsScroll      = 0;
        m_envSelectionScroll = 0;

        m_data               = 0;
        m_action             = (Action)0;
        m_events             = GuiEvent::None;
        m_selectedTab        = 0;
    }

    enum Action
    {
        // HandleAction
        RemoveEnv,
        CreateEnv,
        ToggleFullscreen,

        // GuiUpdate
        ShowOutputWindow,
        ShowAboutWindow,
        ShowProjectWindow,
        ToggleEnvWidget,
        HideEnvWidget,
    };

    int32_t m_envScroll;
    int32_t m_optionsScroll;
    int32_t m_envSelectionScroll;
    uint16_t m_data;
    Action m_action;
    uint8_t m_events;
    uint8_t m_selectedTab;
};

void imguiRightScrollArea(int32_t _x
                        , int32_t _y
                        , uint32_t _width
                        , uint32_t _height
                        , ImguiState& _guiState
                        , Settings& _settings
                        , const Mouse& _mouse
                        , const cs::EnvList& _envList
                        , RightScrollAreaState& _state
                        , bool _enabled = true
                        , uint8_t _viewId = RenderPipeline::ViewIdGui
                        );


// All states.
//-----

struct GuiWidgetState
{
    GuiWidgetState()
    {
        dm::strscpya(m_texPicker[cs::Material::Albedo].m_title,       "Albedo");
        dm::strscpya(m_texPicker[cs::Material::Normal].m_title,       "Normal");
        dm::strscpya(m_texPicker[cs::Material::Surface].m_title,      "Surface");
        dm::strscpya(m_texPicker[cs::Material::Reflectivity].m_title, "Reflectivity");
        dm::strscpya(m_texPicker[cs::Material::Occlusion].m_title,    "Occlusion");
        dm::strscpya(m_texPicker[cs::Material::Emissive].m_title,     "Emissive");

        m_cmftInfoSkybox.m_envType = cs::Environment::Skybox;
        m_cmftInfoPmrem.m_envType  = cs::Environment::Pmrem;
        m_cmftInfoIem.m_envType    = cs::Environment::Iem;

        m_cmftSaveSkybox.m_envType = cs::Environment::Skybox;
        m_cmftSavePmrem.m_envType  = cs::Environment::Pmrem;
        m_cmftSaveIem.m_envType    = cs::Environment::Iem;

        m_skyboxBrowser.m_selection = cs::Environment::Skybox;
        m_pmremBrowser.m_selection  = cs::Environment::Pmrem;
        m_iemBrowser.m_selection    = cs::Environment::Iem;

        dm::strscpya(m_cmftSaveSkybox.m_outputName, "output_skybox");
        dm::strscpya(m_cmftSavePmrem.m_outputName,  "output_pmrem");
        dm::strscpya(m_cmftSaveIem.m_outputName,    "output_iem");
    }

    EnvMapWidgetState         m_envMap;
    TexPickerWidgetState      m_texPicker[cs::Material::TextureCount];
    CmftInfoWidgetState       m_cmftInfoSkybox;
    CmftInfoWidgetState       m_cmftInfoPmrem;
    CmftInfoWidgetState       m_cmftInfoIem;
    CmftSaveWidgetState       m_cmftSaveSkybox;
    CmftSaveWidgetState       m_cmftSavePmrem;
    CmftSaveWidgetState       m_cmftSaveIem;
    CmftPmremWidgetState      m_cmftPmrem;
    CmftIemWidgetState        m_cmftIem;
    CmftTransformWidgetState  m_cmftTransform;
    TonemapWidgetState        m_tonemapWidget;
    MeshSaveWidgetState       m_meshSave;
    MeshBrowserState          m_meshBrowser;
    EnvMapBrowserState        m_skyboxBrowser;
    EnvMapBrowserState        m_pmremBrowser;
    EnvMapBrowserState        m_iemBrowser;
    TextureBrowserWidgetState m_textureBrowser;
    OutputWindowState         m_outputWindow;
    AboutWindowState          m_aboutWindow;
    MagnifyWindowState        m_magnifyWindow;
    ProjectWindowState        m_projectWindow;
    LeftScrollAreaState       m_leftScrollArea;
    RightScrollAreaState      m_rightScrollArea;
};

#endif // CMFTSTUDIO_GUI_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
