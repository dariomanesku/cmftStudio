/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_GUIMANAGER_H_HEADER_GUARD
#define CMFTSTUDIO_GUIMANAGER_H_HEADER_GUARD

#include "common/common.h"
#include "gui.h"
#include <stdint.h>

// Imgui status.
//-----

struct StatusWindowId
{
    enum Enum
    {
        None,

        LoadWarning,
        LoadError,

        FilterIem,
        FilterPmrem,

        WarpFilterInfo0,
        WarpFilterInfo1,

        ProjectSave,
        ProjectLoad,

        MeshConversion,
    };
};

void imguiStatusMessage(const char* _msg
                      , float _durationSec
                      , bool _isWarning          = false
                      , const char* _button      = NULL
                      , uint16_t _buttonEvent    = 0
                      , StatusWindowId::Enum _id = StatusWindowId::None
                      );
void imguiRemoveStatusMessage(StatusWindowId::Enum _id);
uint16_t imguiGetStatusEvent();

// Output window.
//-----

void outputWindowInit(OutputWindowState* _state);
void outputWindowPrint(const char* _format, ...);
void outputWindowClear();
void outputWindowShow();

// Widgets.
//-----

struct Widget
{
    enum Enum
    {
        // LeftSideWidget
        MeshSaveWidget          = 0x00000001,
        MeshBrowser             = 0x00000002,
        TexPickerAlbedo         = 0x00000004,
        TexPickerNormal         = 0x00000008,
        TexPickerSurface        = 0x00000010,
        TexPickerReflectivity   = 0x00000020,
        TexPickerOcclusion      = 0x00000040,
        TexPickerEmissive       = 0x00000080,
        TexPickerMask           = 0x000000fc,
        LeftSideWidgetMask      = 0x000000ff,
        LeftSideWidgetShift     = 0,

        // LeftSideSubwidget
        TextureFileBrowser      = 0x00000100,
        LeftSideSubWidgetMask   = 0x00000100,
        LeftSideSubWidgetShift  = 8,

        // RightSideWidget
        EnvWidget               = 0x00000200,
        RightSideWidgetMask     = 0x00000200,
        RightSideWidgetShift    = 9,

        // RightSideSubwidget
        CmftInfoSkyboxWidget    = 0x00000400,
        CmftInfoPmremWidget     = 0x00000800,
        CmftInfoIemWidget       = 0x00001000,
        CmftSaveSkyboxWidget    = 0x00002000,
        CmftSavePmremWidget     = 0x00004000,
        CmftSaveIemWidget       = 0x00008000,
        CmftPmremWidget         = 0x00010000,
        CmftIemWidget           = 0x00020000,
        CmftTransformWidget     = 0x00040000,
        TonemapWidget           = 0x00080000,
        SkyboxBrowser           = 0x00100000,
        PmremBrowser            = 0x00200000,
        IemBrowser              = 0x00400000,
        RightSideSubwidgetMask  = 0x007ffc00,
        RightSideSubwidgetShift = 10,

        // ModalWindow
        OutputWindow            = 0x00800000,
        AboutWindow             = 0x01000000,
        ProjectWindow           = 0x02000000,
        MagnifyWindow           = 0x04000000,
        ModalWindowMask         = 0x07800000,
        ModalWindowShift        = 23,

        // ScrollArea
        Left                    = 0x08000000,
        Right                   = 0x10000000,
        ScrollAreaMask          = 0x18000000,
        ScrollAreaShift         = 26,
    };
};

void widgetShow(Widget::Enum _widget);
void widgetHide(Widget::Enum _widget);
void widgetHide(uint64_t _mask);
void widgetToggle(Widget::Enum _widget);
void widgetSetOrClear(Widget::Enum _widget, uint64_t _mask);
bool widgetIsVisible(Widget::Enum _widget);

/// Draws entire gui.
bool guiDraw(ImguiState& _guiState
           , Settings& _settings
           , GuiWidgetState& _widgetStates
           , const Mouse& _mouse
           , char _ascii
           , float _deltaTime
           , cs::MeshInstanceList& _meshInstList
           , const cs::TextureList& _textureList
           , const cs::MaterialList& _materialList
           , const cs::EnvList& _envList
           , float _widgetAnimDuration = 0.1f
           , float _modalWindowAnimDuration = 0.06f
           , uint8_t _viewId = RenderPipeline::ViewIdGui
           );

#endif // CMFTSTUDIO_GUIMANAGER_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
