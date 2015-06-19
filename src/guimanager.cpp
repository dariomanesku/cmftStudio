/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "common/common.h"
#include "guimanager.h"

#include "common/utils.h"   // WidgetAnimator

#include <bx/string.h>      // bx::snprintf

#include <cmft/callbacks.h> // cmft::setCallback

#include <bx/string.h>
#include <bx/thread.h>      // bx::MutexScope
#include <bx/macros.h>      // BX_STATIC_ASSERT
#include <bx/os.h>

// Imgui status.
//-----

struct StatusManager
{
    StatusManager()
    {
        m_deltaTime = 0.0f;
        m_time      = 0.0f;
        m_eventIdx  = 0;
    }

    void update(float _deltaTime)
    {
        m_deltaTime = _deltaTime;
        m_time += _deltaTime;
    }

    void draw()
    {
        for (int16_t last = m_messageList.count()-1, ii = last; ii >= 0; --ii)
        {
            const uint16_t idx = last - ii;

            const uint16_t handle = m_messageList.getHandleAt(idx);
            const Message* msg    = m_messageList.getFromHandle(handle);

            if (0.0f != msg->m_endTime && msg->m_endTime < m_time)
            {
                m_animators[handle].fadeOut();
            }

            if (!m_animators[handle].m_visible)
            {
                m_messageList.removeAt(idx);
                last--;
            }
            else
            {
                const bool hasButton = ('\0' != msg->m_button[0]);
                const int32_t statusHeight = 34;
                const int32_t statusWidth = int32_t(msg->m_width)+11;
                const int32_t buttonWidth = 86;

                const float startY = 0.0f;
                const float endY = 10.0f;

                m_animators[handle].update(m_deltaTime, 0.1f);
                m_animators[handle].setKeyPoints(msg->m_posX, startY, msg->m_posX, endY + float(idx*(statusHeight+5)));

                imguiBeginArea(NULL
                             , int32_t(m_animators[handle].m_x) - int32_t(hasButton)*buttonWidth/2
                             , int32_t(m_animators[handle].m_y)
                             , statusWidth, statusHeight, true, 5
                             );
                imguiSeparator(2);

                { // Font scope.
                    ImguiFontScope fontScope(Fonts::StatusFont);
                    imguiLabel(msg->m_warning?imguiRGBA(255, 128, 128, 255):imguiRGBA(255,255,255,230), msg->m_str);
                }

                imguiEndArea();

                if (hasButton)
                {
                    imguiBeginArea(NULL
                                 , int32_t(m_animators[handle].m_x) + statusWidth - buttonWidth/2 + 4
                                 , int32_t(m_animators[handle].m_y)
                                 , buttonWidth, statusHeight, true, 5
                                 );

                    if (imguiButton(msg->m_button))
                    {
                        m_animators[handle].fadeOut();
                        if (m_eventIdx < MaxEvents)
                        {
                            m_eventQueue[m_eventIdx++] = msg->m_buttonEvent;
                        }
                    }

                    imguiEndArea();
                }

            }
        }
    }

    void add(const char* _msg, float _durationSec, bool _isWarning, const char* _button, uint16_t _buttonEvent, StatusWindowId::Enum _id)
    {
        if (m_messageList.count() >= MaxMessages)
        {
            return;
        }

        if (StatusWindowId::None != _id)
        {
            for (uint16_t ii = m_messageList.count(); ii--; )
            {
                const Message* msg = m_messageList.getAt(ii);
                if (msg->m_id == _id)
                {
                    return;
                }
            }
        }

        const float msgLength = imguiGetTextLength(_msg, Fonts::StatusFont);
        const float msgPosX = (float(g_width)-msgLength)*0.5f;

        Message* msg = m_messageList.addNew();
        msg->m_endTime = 0.0f == _durationSec ? 0.0f : m_time+_durationSec;
        msg->m_posX = msgPosX;
        msg->m_width = msgLength;
        msg->m_buttonEvent = _buttonEvent;
        msg->m_warning = _isWarning;
        msg->m_id = _id;
        dm::strscpy(msg->m_str, _msg, 128);
        dm::strscpy(msg->m_button, _button, 32);

        const uint16_t handle = m_messageList.getHandleOf(msg);
        m_animators[handle].reset(true, msgPosX, 0.0f);
        m_animators[handle].fadeIn();
    }

    uint16_t getNextEvent()
    {
        return m_eventIdx > 0 ? m_eventQueue[--m_eventIdx] : 0;
    }

    void remove(StatusWindowId::Enum _id)
    {
        for (uint16_t ii = m_messageList.count(); ii--; )
        {
            const Message* msg = m_messageList.getAt(ii);
            if (msg->m_id == _id)
            {
                const uint16_t handle = m_messageList.getHandleOf(msg);
                m_animators[handle].fadeOut();
            }
        }
    }

    struct Message
    {
        float m_endTime;
        float m_posX;
        float m_width;
        uint16_t m_buttonEvent;
        bool m_warning;
        StatusWindowId::Enum m_id;
        char m_str[128];
        char m_button[32];
    };

    enum
    {
        MaxMessages = 8,
        MaxEvents = 16,
    };

    float m_deltaTime;
    float m_time;
    int16_t m_eventIdx;
    uint16_t m_eventQueue[MaxEvents];
    dm::OpListT<Message, MaxMessages> m_messageList;
    WidgetAnimator m_animators[MaxMessages];
};
static StatusManager s_statusManager;

void imguiStatusMessage(const char* _msg
                      , float _durationSec
                      , bool _isWarning
                      , const char* _button
                      , uint16_t _buttonEvent
                      , StatusWindowId::Enum _id
                      )
{
    s_statusManager.add(_msg, _durationSec, _isWarning, _button, _buttonEvent, _id);
}

void imguiRemoveStatusMessage(StatusWindowId::Enum _id)
{
    s_statusManager.remove(_id);
}

uint16_t imguiGetStatusEvent()
{
    return s_statusManager.getNextEvent();
}

// Circular string array
//-----

struct CircularStringArray
{
    CircularStringArray()
    {
        clear();
    }

    void clear()
    {
        bx::MutexScope lock(m_mutex);
        m_last       = UINT16_MAX;
        m_lastListed = UINT16_MAX;
        m_count      = 0;
        m_unlisted   = 0;
    }

    void add(const char* _str, uint32_t _len = OutputWindowState::LineLength)
    {
        // Copy _str contents into the next line.
        dm::strscpy(getLine(++m_last), _str, _len);
        ++m_unlisted;
    }

    void addSplitNewLine(const char _str[OutputWindowState::LineLength])
    {
        char temp[2048];
        dm::strscpy(temp, _str, 2048);

        bx::MutexScope lock(m_mutex);

        const char* ptr = strtok(temp, "\n");
        do
        {
            enum { LineLength = 105 };

            // Split and add line by line of specified length.
            while (strlen(ptr) > LineLength)
            {
                add(ptr, LineLength);
                ptr += (LineLength-1);
            }

            // Add the rest.
            add(ptr);
        }
        while ((ptr = strtok(NULL, "\n")) != NULL);
    }

    char* getLine(uint16_t _idx)
    {
        return m_lines[wrapAround(_idx)];
    }

    uint16_t getLines(char** _ptrs)
    {
        bx::MutexScope lock(m_mutex);

        for (uint16_t ii = 0; ii < m_count; ++ii)
        {
            _ptrs[ii] = getLine(m_lastListed-ii);
        }

        return m_count;
    }

    void submitNext()
    {
        if (0 == m_unlisted)
        {
            return;
        }

        bx::MutexScope lock(m_mutex);

        --m_unlisted;
        ++m_lastListed;
        m_count = (++m_count > OutputWindowState::MaxLines) ? uint16_t(OutputWindowState::MaxLines) : m_count;
    }

private:
    static inline uint16_t wrapAround(uint16_t _idx)
    {
        return _idx&(OutputWindowState::MaxLines-1);
    }

public:
    uint16_t m_last;
    uint16_t m_lastListed;
    uint16_t m_count;
    uint16_t m_unlisted;
    char m_lines[OutputWindowState::MaxLines][OutputWindowState::LineLength];
    bx::Mutex m_mutex;
};
static CircularStringArray s_csa;

static const char* printfVargs(const char* _format, va_list _argList)
{
    static char s_buffer[8192];

    char* out = s_buffer;
    int32_t len = bx::vsnprintf(out, sizeof(s_buffer), _format, _argList);
    if ((int32_t)sizeof(s_buffer) < len)
    {
        out = (char*)alloca(len+1);
        len = bx::vsnprintf(out, len, _format, _argList);
    }
    out[len] = '\0';

    return out;
}

// Output window.
//-----

static OutputWindowState* s_outputWindowState;

struct PrintCallback: public cmft::PrintCallbackI
{
    virtual ~PrintCallback()
    {
    }

    virtual void printWarning(const char* _msg) const
    {
        outputWindowPrint(_msg);
    }

    virtual void printInfo(const char* _msg) const
    {
        outputWindowPrint(_msg);
    }
};
static PrintCallback s_printCallback;

void outputWindowInit(OutputWindowState* _state)
{
    s_outputWindowState = _state;

    // Redirect cmft output to output window.
    cmft::setCallback(&s_printCallback);
}

void outputWindowPrint(const char* _format, ...)
{
    va_list argList;
    va_start(argList, _format);
    const char* str = printfVargs(_format, argList);
    va_end(argList);

    s_csa.addSplitNewLine(str);
}

void outputWindowUpdate(float _deltaTime/*sec*/, float _updateFreq = 0.014f/*sec*/)
{
    static float timeNow = 0.0f;
    timeNow += _deltaTime;

    if (timeNow > _updateFreq)
    {
        s_csa.submitNext();
        timeNow = 0.0f;
    }

    const uint16_t before = s_outputWindowState->m_linesCount;
    const uint16_t now    = s_csa.getLines(s_outputWindowState->m_lines);
    s_outputWindowState->m_linesCount = now;

    if (before != now)
    {
        s_outputWindowState->scrollDown();
    }
}

void outputWindowClear()
{
    s_outputWindowState->m_scroll = 0;
    s_csa.clear();
}

void outputWindowShow()
{
    widgetShow(Widget::OutputWindow);
}

// Animators.
//-----

static inline Widget::Enum texPickerFor(cs::Material::Texture _matTexture)
{
    static const Widget::Enum sc_materialToWidget[cs::Material::TextureCount] =
    {
        Widget::TexPickerAlbedo,       // Material::Albedo
        Widget::TexPickerNormal,       // Material::Normal
        Widget::TexPickerSurface,      // Material::Surface
        Widget::TexPickerReflectivity, // Material::Reflectivity
        Widget::TexPickerOcclusion,    // Material::Occlusion
        Widget::TexPickerEmissive,     // Material::Emissive
    };

    return sc_materialToWidget[_matTexture];
}

// Widgets.
//-----

static uint64_t s_widgets = Widget::Right | Widget::Left;

void widgetShow(Widget::Enum _widget)
{
    s_widgets |= _widget;
}

void widgetHide(Widget::Enum _widget)
{
    s_widgets &= ~_widget;
}

void widgetHide(uint64_t _mask)
{
    s_widgets &= ~_mask;
}

void widgetToggle(Widget::Enum _widget)
{
    s_widgets ^= _widget;
}

void widgetSetOrClear(Widget::Enum _widget, uint64_t _mask)
{
    s_widgets ^= _widget;
    s_widgets &= ~(_widget^_mask);
}

bool widgetIsVisible(Widget::Enum _widget)
{
    return (0 != (s_widgets & _widget));
}

struct Animators
{
    enum Enum
    {
        LeftScrollArea,
        RightScrollArea,
        MeshSaveWidget,
        MeshBrowser,
        TexPickerAlbedo,
        TexPickerNormal,
        TexPickerSurface,
        TexPickerReflectivity,
        TexPickerOcclusion,
        TexPickerEmissive,
        LeftTextureBrowser,
        EnvMapWidget,
        SkyboxBrowser,
        PmremBrowser,
        IemBrowser,
        TonemapWidget,
        CmftTransform,
        CmftIem,
        CmftPmrem,
        CmftSaveSkybox,
        CmftSavePmrem,
        CmftSaveIem,
        CmftInfoSkybox,
        CmftInfoPmrem,
        CmftInfoIem,
        OutputWindow,
        AboutWindow,
        MagnifyWindow,
        ProjectWindow,

        Count
    };

    static inline Animators::Enum texPickerFor(cs::Material::Texture _matTexture)
    {
        static const Animators::Enum sc_materialToAnimator[cs::Material::TextureCount] =
        {
            TexPickerAlbedo,       //Material::Albedo
            TexPickerNormal,       //Material::Normal
            TexPickerSurface,      //Material::Surface
            TexPickerReflectivity, //Material::Reflectivity
            TexPickerOcclusion,    //Material::Occlusion
            TexPickerEmissive,     //Material::Emissive
        };

        return sc_materialToAnimator[_matTexture];
    }

    void init(float _width, float _height
            , float _lx, float _ly, float _lw
            , float _rx, float _ry
            )
    {
        const float width = _width - 10.0f;
        m_animators[LeftScrollArea       ].reset(false, _lx, _ly);
        m_animators[RightScrollArea      ].reset(false, _rx, _ry);
        m_animators[MeshSaveWidget       ].reset(false, _lx-_lw, _ly);
        m_animators[MeshBrowser          ].reset(false, _lx-_lw, _ly);
        m_animators[TexPickerAlbedo      ].reset(false, _lx-_lw, _ly);
        m_animators[TexPickerNormal      ].reset(false, _lx-_lw, _ly);
        m_animators[TexPickerSurface     ].reset(false, _lx-_lw, _ly);
        m_animators[TexPickerReflectivity].reset(false, _lx-_lw, _ly);
        m_animators[TexPickerOcclusion   ].reset(false, _lx-_lw, _ly);
        m_animators[TexPickerEmissive    ].reset(false, _lx-_lw, _ly);
        m_animators[LeftTextureBrowser   ].reset(false, _lx-_lw, _ly);
        m_animators[EnvMapWidget         ].reset(false, width, 0.0f);
        m_animators[SkyboxBrowser        ].reset(false, width, 0.0f);
        m_animators[PmremBrowser         ].reset(false, width, 0.0f);
        m_animators[IemBrowser           ].reset(false, width, 0.0f);
        m_animators[TonemapWidget        ].reset(false, width, 0.0f);
        m_animators[CmftTransform        ].reset(false, width, 0.0f);
        m_animators[CmftIem              ].reset(false, width, 0.0f);
        m_animators[CmftPmrem            ].reset(false, width, 0.0f);
        m_animators[CmftSaveSkybox       ].reset(false, width, 0.0f);
        m_animators[CmftSavePmrem        ].reset(false, width, 0.0f);
        m_animators[CmftSaveIem          ].reset(false, width, 0.0f);
        m_animators[CmftInfoSkybox       ].reset(false, width, 0.0f);
        m_animators[CmftInfoPmrem        ].reset(false, width, 0.0f);
        m_animators[CmftInfoIem          ].reset(false, width, 0.0f);
        m_animators[OutputWindow         ].reset(false, (width-800.0f)*0.5f, _height);
        m_animators[AboutWindow          ].reset(false, (width-800.0f)*0.5f, -400.0f);
        m_animators[MagnifyWindow        ].reset(false, (width-800.0f)*0.5f, -400.0f);
        m_animators[ProjectWindow        ].reset(false, _lx-_lw, (_height-600.0f)*0.5f);
    }

    void setKeyPoints(float _width, float _height, float _lw, float _rw
                    , float _l0x, float  _l0y
                    , float _l1x, float  _l1y
                    , float _l2x, float  _l2y
                    , float _r0x, float _r0y
                    , float _r1x, float _r1y
                    , float _r2x, float _r2y
                    )
    {
        // This input happens on Windows when window is minimized.
        if (0.0f == _width
        ||  0.0f == _height)
        {
            return;
        }

        const float width = _width - 10.0f;
        m_animators[LeftScrollArea       ].setKeyPoints(_l0x - _rw, _l0y, _l0x, _l0y);
        m_animators[RightScrollArea      ].setKeyPoints(_r0x + _rw, _r0y, _r0x, _r0y);
        m_animators[MeshSaveWidget       ].setKeyPoints(_l0x - _lw, _l0y, _l1x, _l1y);
        m_animators[MeshBrowser          ].setKeyPoints(_l0x - _lw, _l0y, _l1x, _l1y);
        m_animators[TexPickerAlbedo      ].setKeyPoints(_l0x - _lw, _l0y, _l1x, _l1y);
        m_animators[TexPickerNormal      ].setKeyPoints(_l0x - _lw, _l0y, _l1x, _l1y);
        m_animators[TexPickerSurface     ].setKeyPoints(_l0x - _lw, _l0y, _l1x, _l1y);
        m_animators[TexPickerReflectivity].setKeyPoints(_l0x - _lw, _l0y, _l1x, _l1y);
        m_animators[TexPickerOcclusion   ].setKeyPoints(_l0x - _lw, _l0y, _l1x, _l1y);
        m_animators[TexPickerEmissive    ].setKeyPoints(_l0x - _lw, _l0y, _l1x, _l1y);
        m_animators[LeftTextureBrowser   ].setKeyPoints(_l0x - _lw, _l0y, _l2x, _l2y);
        m_animators[EnvMapWidget         ].setKeyPoints(width, _r0y, _r1x, _r1y);
        m_animators[SkyboxBrowser        ].setKeyPoints(width, _r0y, _r2x, _r2y);
        m_animators[PmremBrowser         ].setKeyPoints(width, _r0y, _r2x, _r2y);
        m_animators[IemBrowser           ].setKeyPoints(width, _r0y, _r2x, _r2y);
        m_animators[TonemapWidget        ].setKeyPoints(width, _r0y, _r2x, _r2y);
        m_animators[CmftTransform        ].setKeyPoints(width, _r0y, _r2x, _r2y);
        m_animators[CmftIem              ].setKeyPoints(width, _r0y, _r2x, _r2y);
        m_animators[CmftPmrem            ].setKeyPoints(width, _r0y, _r2x, _r2y);
        m_animators[CmftSaveSkybox       ].setKeyPoints(width, _r0y, _r2x-Gui::PaneWidth, _r2y);
        m_animators[CmftSavePmrem        ].setKeyPoints(width, _r0y, _r2x-Gui::PaneWidth, _r2y);
        m_animators[CmftSaveIem          ].setKeyPoints(width, _r0y, _r2x-Gui::PaneWidth, _r2y);
        m_animators[CmftInfoSkybox       ].setKeyPoints(width, _r0y, _r2x, _r2y);
        m_animators[CmftInfoPmrem        ].setKeyPoints(width, _r0y, _r2x, _r2y);
        m_animators[CmftInfoIem          ].setKeyPoints(width, _r0y, _r2x, _r2y);
        m_animators[OutputWindow         ].setKeyPoints((_width -800.0f)*0.5f
                                                      , _height
                                                      , (_width -800.0f)*0.5f
                                                      , (_height-400.0f)*0.5f
                                                      );
        m_animators[AboutWindow          ].setKeyPoints((_width -800.0f)*0.5f
                                                      , -400.0f
                                                      , (_width -800.0f)*0.5f
                                                      , (_height-400.0f)*0.5f
                                                      );
        m_animators[MagnifyWindow        ].setKeyPoints(_width*0.06f
                                                      , -_height-10.0f
                                                      , _width*0.06f
                                                      , _height*0.07f
                                                      );
        m_animators[ProjectWindow        ].setKeyPoints(_l0x-_lw
                                                      , (_height-800.0f)*0.5f
                                                      , (_width -350.0f)*0.5f
                                                      , (_height-800.0f)*0.5f
                                                      , width
                                                      , (_height-800.0f)*0.5f
                                                      );
    }

    void tick(float _deltaTime, float _widgetAnimDuration = 0.1f, float _modalWindowAnimDuration = 0.06f)
    {
        m_animators[LeftScrollArea       ].update(_deltaTime, _widgetAnimDuration);
        m_animators[RightScrollArea      ].update(_deltaTime, _widgetAnimDuration);
        m_animators[MeshSaveWidget       ].update(_deltaTime, _widgetAnimDuration);
        m_animators[MeshBrowser          ].update(_deltaTime, _widgetAnimDuration);
        m_animators[TexPickerAlbedo      ].update(_deltaTime, _widgetAnimDuration);
        m_animators[TexPickerNormal      ].update(_deltaTime, _widgetAnimDuration);
        m_animators[TexPickerSurface     ].update(_deltaTime, _widgetAnimDuration);
        m_animators[TexPickerReflectivity].update(_deltaTime, _widgetAnimDuration);
        m_animators[TexPickerOcclusion   ].update(_deltaTime, _widgetAnimDuration);
        m_animators[TexPickerEmissive    ].update(_deltaTime, _widgetAnimDuration);
        m_animators[LeftTextureBrowser   ].update(_deltaTime, _widgetAnimDuration);
        m_animators[EnvMapWidget         ].update(_deltaTime, _widgetAnimDuration);
        m_animators[SkyboxBrowser        ].update(_deltaTime, _widgetAnimDuration);
        m_animators[PmremBrowser         ].update(_deltaTime, _widgetAnimDuration);
        m_animators[IemBrowser           ].update(_deltaTime, _widgetAnimDuration);
        m_animators[TonemapWidget        ].update(_deltaTime, _widgetAnimDuration);
        m_animators[CmftTransform        ].update(_deltaTime, _widgetAnimDuration);
        m_animators[CmftIem              ].update(_deltaTime, _widgetAnimDuration);
        m_animators[CmftPmrem            ].update(_deltaTime, _widgetAnimDuration);
        m_animators[CmftSaveSkybox       ].update(_deltaTime, _widgetAnimDuration);
        m_animators[CmftSavePmrem        ].update(_deltaTime, _widgetAnimDuration);
        m_animators[CmftSaveIem          ].update(_deltaTime, _widgetAnimDuration);
        m_animators[CmftInfoSkybox       ].update(_deltaTime, _widgetAnimDuration);
        m_animators[CmftInfoPmrem        ].update(_deltaTime, _widgetAnimDuration);
        m_animators[CmftInfoIem          ].update(_deltaTime, _widgetAnimDuration);
        m_animators[OutputWindow         ].update(_deltaTime, _modalWindowAnimDuration);
        m_animators[AboutWindow          ].update(_deltaTime, _modalWindowAnimDuration);
        m_animators[MagnifyWindow        ].update(_deltaTime, _modalWindowAnimDuration);
        m_animators[ProjectWindow        ].update(_deltaTime, _modalWindowAnimDuration);
    }

    void updateVisibility()
    {
        #define CS_HANDLE_ANIMATION(_widget, _animator) \
            if (widgetIsVisible(_widget))               \
            {                                           \
                m_animators[_animator].fadeIn();        \
            }                                           \
            else                                        \
            {                                           \
                m_animators[_animator].fadeOut();       \
            }

        // Left side.
        if (widgetIsVisible(Widget::Left))
        {
            // Scroll area.
            m_animators[Animators::LeftScrollArea].fadeIn();

            CS_HANDLE_ANIMATION(Widget::MeshSaveWidget,        Animators::MeshSaveWidget)
            CS_HANDLE_ANIMATION(Widget::MeshBrowser,           Animators::MeshBrowser)
            CS_HANDLE_ANIMATION(Widget::TexPickerAlbedo,       Animators::TexPickerAlbedo)
            CS_HANDLE_ANIMATION(Widget::TexPickerNormal,       Animators::TexPickerNormal)
            CS_HANDLE_ANIMATION(Widget::TexPickerSurface,      Animators::TexPickerSurface)
            CS_HANDLE_ANIMATION(Widget::TexPickerReflectivity, Animators::TexPickerReflectivity)
            CS_HANDLE_ANIMATION(Widget::TexPickerOcclusion,    Animators::TexPickerOcclusion)
            CS_HANDLE_ANIMATION(Widget::TexPickerEmissive,     Animators::TexPickerEmissive)

            if (widgetIsVisible(Widget::TextureFileBrowser)
            &&  widgetIsVisible(Widget::TexPickerMask))
            {
                m_animators[Animators::LeftTextureBrowser].fadeIn();
            }
            else
            {
                m_animators[Animators::LeftTextureBrowser].fadeOut();
            }
        }
        else
        {
            m_animators[Animators::LeftScrollArea       ].fadeOut();
            m_animators[Animators::MeshSaveWidget       ].fadeOut();
            m_animators[Animators::MeshBrowser          ].fadeOut();
            m_animators[Animators::TexPickerAlbedo      ].fadeOut();
            m_animators[Animators::TexPickerNormal      ].fadeOut();
            m_animators[Animators::TexPickerSurface     ].fadeOut();
            m_animators[Animators::TexPickerReflectivity].fadeOut();
            m_animators[Animators::TexPickerOcclusion   ].fadeOut();
            m_animators[Animators::TexPickerEmissive    ].fadeOut();
            m_animators[Animators::LeftTextureBrowser   ].fadeOut();
        }

        // Right side.
        if (widgetIsVisible(Widget::Right))
        {
            // Scroll area.
            m_animators[Animators::RightScrollArea].fadeIn();

            // Env widget.
            if (widgetIsVisible(Widget::EnvWidget))
            {
                m_animators[Animators::EnvMapWidget].fadeIn();

                CS_HANDLE_ANIMATION(Widget::CmftInfoSkyboxWidget, Animators::CmftInfoSkybox)
                CS_HANDLE_ANIMATION(Widget::CmftInfoPmremWidget,  Animators::CmftInfoPmrem)
                CS_HANDLE_ANIMATION(Widget::CmftInfoIemWidget,    Animators::CmftInfoIem)
                CS_HANDLE_ANIMATION(Widget::CmftSaveSkyboxWidget, Animators::CmftSaveSkybox)
                CS_HANDLE_ANIMATION(Widget::CmftSavePmremWidget,  Animators::CmftSavePmrem)
                CS_HANDLE_ANIMATION(Widget::CmftSaveIemWidget,    Animators::CmftSaveIem)
                CS_HANDLE_ANIMATION(Widget::TonemapWidget,        Animators::TonemapWidget)
                CS_HANDLE_ANIMATION(Widget::CmftTransformWidget,  Animators::CmftTransform)
                CS_HANDLE_ANIMATION(Widget::CmftIemWidget,        Animators::CmftIem)
                CS_HANDLE_ANIMATION(Widget::CmftPmremWidget,      Animators::CmftPmrem)
                CS_HANDLE_ANIMATION(Widget::SkyboxBrowser,        Animators::SkyboxBrowser)
                CS_HANDLE_ANIMATION(Widget::PmremBrowser,         Animators::PmremBrowser)
                CS_HANDLE_ANIMATION(Widget::IemBrowser,           Animators::IemBrowser)
            }
            else
            {
                widgetHide(Widget::RightSideSubwidgetMask);
                m_animators[Animators::EnvMapWidget  ].fadeOut();
                m_animators[Animators::CmftInfoSkybox].fadeOut();
                m_animators[Animators::CmftInfoPmrem ].fadeOut();
                m_animators[Animators::CmftInfoIem   ].fadeOut();
                m_animators[Animators::CmftSaveSkybox].fadeOut();
                m_animators[Animators::CmftSavePmrem ].fadeOut();
                m_animators[Animators::CmftSaveIem   ].fadeOut();
                m_animators[Animators::CmftPmrem     ].fadeOut();
                m_animators[Animators::CmftIem       ].fadeOut();
                m_animators[Animators::CmftTransform ].fadeOut();
                m_animators[Animators::TonemapWidget ].fadeOut();
                m_animators[Animators::SkyboxBrowser ].fadeOut();
                m_animators[Animators::PmremBrowser  ].fadeOut();
                m_animators[Animators::IemBrowser    ].fadeOut();
            }

        }
        else
        {
            m_animators[Animators::RightScrollArea].fadeOut();
            m_animators[Animators::EnvMapWidget   ].fadeOut();
            m_animators[Animators::CmftInfoSkybox ].fadeOut();
            m_animators[Animators::CmftInfoPmrem  ].fadeOut();
            m_animators[Animators::CmftInfoIem    ].fadeOut();
            m_animators[Animators::CmftSaveSkybox ].fadeOut();
            m_animators[Animators::CmftSavePmrem  ].fadeOut();
            m_animators[Animators::CmftSaveIem    ].fadeOut();
            m_animators[Animators::CmftPmrem      ].fadeOut();
            m_animators[Animators::CmftIem        ].fadeOut();
            m_animators[Animators::CmftTransform  ].fadeOut();
            m_animators[Animators::TonemapWidget  ].fadeOut();
            m_animators[Animators::SkyboxBrowser  ].fadeOut();
            m_animators[Animators::PmremBrowser   ].fadeOut();
            m_animators[Animators::IemBrowser     ].fadeOut();
        }

        // Modal.
        CS_HANDLE_ANIMATION(Widget::OutputWindow,  Animators::OutputWindow)
        CS_HANDLE_ANIMATION(Widget::AboutWindow,   Animators::AboutWindow)
        CS_HANDLE_ANIMATION(Widget::MagnifyWindow, Animators::MagnifyWindow)
        CS_HANDLE_ANIMATION(Widget::ProjectWindow, Animators::ProjectWindow)

        #undef CS_HANDLE_ANIMATION
    }

    inline WidgetAnimator& operator[](uint32_t _index)
    {
        return m_animators[_index];
    }

private:
    WidgetAnimator m_animators[Animators::Count];
};
static Animators s_animators;

static inline void setOrClear(uint8_t& _field, uint8_t _flag)
{
    _field = (_field == _flag) ? 0 : _flag;
}

// Draw entire GUI.
bool guiDraw(ImguiState& _guiState
           , Settings& _settings
           , GuiWidgetState& _widgetState
           , const Mouse& _mouse
           , char _ascii
           , float _deltaTime
           // lists
           , cs::MeshInstanceList& _meshInstList
           , const cs::TextureList& _textureList
           , const cs::MaterialList& _materialList
           , const cs::EnvList& _envList
           // imguiLeftScrollArea
           , float _widgetAnimDuration
           , float _modalWindowAnimDuration
           , uint8_t _viewId
           )
{
    g_guiHeight = DM_MAX(1022, g_height);
    const float aspect = g_widthf/g_heightf;
    g_guiWidth = dm::ftou(float(g_guiHeight)*aspect);

    const float columnLeft0X = float(Gui::BorderButtonWidth) - g_texelHalf;
    const float columnLeft1X = float(columnLeft0X + Gui::PaneWidth + Gui::HorizontalSpacing);
    const float columnLeft2X = float(columnLeft1X + Gui::PaneWidth + Gui::HorizontalSpacing);

    const float columnRight0X = float(g_guiWidth    - Gui::PaneWidth - Gui::BorderButtonWidth + 1);
    const float columnRight1X = float(columnRight0X - Gui::PaneWidth - Gui::HorizontalSpacing);
    const float columnRight2X = float(columnRight1X - Gui::PaneWidth - Gui::HorizontalSpacing);

    // Setup animators.
    static bool s_once = false;
    if (!s_once)
    {
        s_animators.init(float(g_guiWidth), float(g_guiHeight)
                       , columnLeft0X,  Gui::FirstPaneY, Gui::PaneWidth
                       , columnRight0X, Gui::FirstPaneY
                       );
        s_once = true;
    }

    s_animators.setKeyPoints(float(g_guiWidth), float(g_guiHeight), Gui::PaneWidth, Gui::PaneWidth
                           , columnLeft0X,  Gui::FirstPaneY
                           , columnLeft1X,  Gui::SecondPaneY
                           , columnLeft2X,  Gui::SecondPaneY
                           , columnRight0X, Gui::FirstPaneY
                           , columnRight1X, Gui::SecondPaneY
                           , columnRight2X, Gui::SecondPaneY
                           );
    s_animators.updateVisibility();
    s_animators.tick(_deltaTime, _widgetAnimDuration, _modalWindowAnimDuration);

    // Update output window.
    outputWindowUpdate(_deltaTime);

    // Dismiss modal window when escape is pressed.
    if (27 == _ascii)
    {
        widgetHide(Widget::ModalWindowMask);
    }

    // Begin imgui.
    imguiSetFont(Fonts::Default);
    const uint8_t mbuttons = (Mouse::Hold == _mouse.m_left  ? IMGUI_MBUT_LEFT  : 0)
                           | (Mouse::Hold == _mouse.m_right ? IMGUI_MBUT_RIGHT : 0)
                           ;
    imguiBeginFrame(_mouse.m_curr.m_mx
                  , _mouse.m_curr.m_my
                  , mbuttons
                  , _mouse.m_scroll
                  , g_width
                  , g_height
                  , g_guiWidth
                  , g_guiHeight
                  , _ascii
                  , _viewId
                  );

    const bool enabled = !widgetIsVisible(Widget::ModalWindowMask);

    // Border buttons logic.
    if (imguiBorderButton(ImguiBorder::Right, widgetIsVisible(Widget::Right), enabled))
    {
        widgetToggle(Widget::Right);
    }

    if (imguiBorderButton(ImguiBorder::Left, widgetIsVisible(Widget::Left), enabled))
    {
        widgetToggle(Widget::Left);
    }

    // Adjust mouse input according to gui scale.
    Mouse mouse;
    memcpy(&mouse, &_mouse, sizeof(Mouse));
    const float xScale = float(g_guiWidth)/g_widthf;
    const float yScale = float(g_guiHeight)/g_heightf;
    mouse.m_curr.m_mx = int32_t(float(mouse.m_curr.m_mx)*xScale);
    mouse.m_curr.m_my = int32_t(float(mouse.m_curr.m_my)*yScale);
    mouse.m_prev.m_mx = int32_t(float(mouse.m_prev.m_mx)*xScale);
    mouse.m_prev.m_my = int32_t(float(mouse.m_prev.m_my)*yScale);

    // Right scroll area.
    if (s_animators[Animators::RightScrollArea].isVisible())
    {
        imguiRightScrollArea(int32_t(s_animators[Animators::RightScrollArea].m_x)
                           , int32_t(s_animators[Animators::RightScrollArea].m_y)
                           , Gui::PaneWidth
                           , g_guiHeight
                           , _guiState
                           , _settings
                           , mouse
                           , _envList
                           , _widgetState.m_rightScrollArea
                           , enabled
                           , _viewId
                           );

        if (guiEvent(GuiEvent::GuiUpdate, _widgetState.m_rightScrollArea.m_events))
        {
            if (RightScrollAreaState::ShowOutputWindow == _widgetState.m_rightScrollArea.m_action)
            {
                widgetShow(Widget::OutputWindow);
            }
            else if (RightScrollAreaState::ShowAboutWindow == _widgetState.m_rightScrollArea.m_action)
            {
                widgetShow(Widget::AboutWindow);
            }
            else if (RightScrollAreaState::ShowProjectWindow == _widgetState.m_rightScrollArea.m_action)
            {
                //Reset project window state.
                imguiRemoveStatusMessage(StatusWindowId::LoadWarning);
                imguiRemoveStatusMessage(StatusWindowId::LoadError);
                _widgetState.m_projectWindow.m_tabs = 0;
                _widgetState.m_projectWindow.m_confirmButton = false;

                widgetShow(Widget::ProjectWindow);
            }
            else if (RightScrollAreaState::ToggleEnvWidget == _widgetState.m_rightScrollArea.m_action)
            {
                widgetSetOrClear(Widget::EnvWidget, Widget::RightSideWidgetMask);
            }
            else if (RightScrollAreaState::HideEnvWidget == _widgetState.m_rightScrollArea.m_action)
            {
                widgetHide(Widget::RightSideWidgetMask);
            }
        }
    }

    // Left scroll area.
    if (s_animators[Animators::LeftScrollArea].isVisible())
    {
        imguiLeftScrollArea(int32_t(s_animators[Animators::LeftScrollArea].m_x)
                          , int32_t(s_animators[Animators::LeftScrollArea].m_y)
                          , Gui::PaneWidth
                          , g_guiHeight
                          , _guiState
                          , _settings
                          , mouse
                          , _envList[_settings.m_selectedEnvMap]
                          , _meshInstList
                          , _meshInstList[_settings.m_selectedMeshIdx]
                          , _materialList
                          , _widgetState.m_leftScrollArea
                          , enabled
                          );

        // Hide texture pickers for hidden texture previews.
        for (uint8_t ii = 0; ii < cs::Material::TextureCount; ++ii)
        {
            if (_widgetState.m_leftScrollArea.m_inactiveTexPreviews&(1<<ii))
            {
                const cs::Material::Texture matTex = (cs::Material::Texture)ii;
                const Widget::Enum widget = texPickerFor(matTex);
                if (widgetIsVisible(widget))
                {
                    widgetHide(Widget::LeftSideWidgetMask);
                }
            }
        }

        if (_widgetState.m_leftScrollArea.m_events & GuiEvent::HandleAction)
        {
            if (LeftScrollAreaState::MeshSelectionChange == _widgetState.m_leftScrollArea.m_action)
            {
                // Get mesh name.
                const cs::MeshHandle mesh = { _widgetState.m_leftScrollArea.m_data };
                const char* meshName = cs::getName(mesh);

                // Update selected mesh name.
                dm::strscpya(_widgetState.m_meshSave.m_outputName, meshName);
            }
            else if (LeftScrollAreaState::TabSelectionChange == _widgetState.m_leftScrollArea.m_action)
            {
                widgetHide(Widget::LeftSideWidgetMask);
            }
        }

        if (guiEvent(GuiEvent::GuiUpdate, _widgetState.m_leftScrollArea.m_events))
        {
            if (LeftScrollAreaState::SaveMesh == _widgetState.m_leftScrollArea.m_action)
            {
                // Get mesh name.
                const cs::MeshHandle mesh = { _widgetState.m_leftScrollArea.m_data };
                const char* meshName = cs::getName(mesh);

                // Pass parameters to MeshSaveWidget.
                _widgetState.m_meshSave.m_mesh = mesh;
                dm::strscpya(_widgetState.m_meshSave.m_outputName, meshName);

                // Show MeshSaveWidget.
                widgetSetOrClear(Widget::MeshSaveWidget, Widget::LeftSideWidgetMask);
            }
            else if (LeftScrollAreaState::ToggleMeshBrowser == _widgetState.m_leftScrollArea.m_action)
            {
                widgetSetOrClear(Widget::MeshBrowser, Widget::LeftSideWidgetMask);
            }
            else if (LeftScrollAreaState::ToggleTexPicker == _widgetState.m_leftScrollArea.m_action)
            {
                const cs::Material::Texture matTex = (cs::Material::Texture)_widgetState.m_leftScrollArea.m_data;
                const Widget::Enum widget = texPickerFor(matTex);
                widgetSetOrClear(widget, Widget::LeftSideWidgetMask);
            }
            else if (LeftScrollAreaState::HideLeftSideWidget == _widgetState.m_leftScrollArea.m_action)
            {
                widgetHide(Widget::LeftSideWidgetMask);
            }
        }
    }

    // Left side.
    if (s_animators[Animators::MeshSaveWidget].isVisible())
    {
        imguiMeshSaveWidget(int32_t(s_animators[Animators::MeshSaveWidget].m_x)
                          , int32_t(s_animators[Animators::MeshSaveWidget].m_y)
                          , Gui::PaneWidth
                          , _widgetState.m_meshSave
                          );

        if (guiEvent(GuiEvent::GuiUpdate, _widgetState.m_meshSave.m_events))
        {
            char msg[128];
            bx::snprintf(msg, sizeof(msg), "Directory: %s", _widgetState.m_meshSave.m_directory);
            imguiStatusMessage(msg, 4.0f);
        }

        if (guiEvent(GuiEvent::DismissWindow, _widgetState.m_meshSave.m_events))
        {
            widgetSetOrClear(Widget::MeshSaveWidget, Widget::LeftSideWidgetMask);
        }
    }

    if (s_animators[Animators::MeshBrowser].isVisible())
    {
        const bool objSelected = (0 == strcmp("obj", _widgetState.m_meshBrowser.m_fileExt));
        _widgetState.m_meshBrowser.m_objSelected = objSelected;

        imguiMeshBrowserWidget(int32_t(s_animators[Animators::MeshBrowser].m_x)
                             , int32_t(s_animators[Animators::MeshBrowser].m_y)
                             , Gui::PaneWidth
                             , _widgetState.m_meshBrowser
                             );

        if (guiEvent(GuiEvent::GuiUpdate, _widgetState.m_meshBrowser.m_events))
        {
            char msg[128];
            bx::snprintf(msg, sizeof(msg), "Directory: %s", _widgetState.m_meshBrowser.m_directory);
            imguiStatusMessage(msg, 4.0f);
        }

        if (guiEvent(GuiEvent::DismissWindow, _widgetState.m_meshBrowser.m_events))
        {
            widgetSetOrClear(Widget::MeshBrowser, Widget::LeftSideWidgetMask);
        }
    }

    // Handle all texture pickers.
    for (uint8_t ii = 0; ii < cs::Material::TextureCount; ++ii)
    {
        const cs::Material::Texture matTex = (cs::Material::Texture)ii;
        if (s_animators[Animators::texPickerFor(matTex)].isVisible())
        {
            imguiTexPickerWidget(int32_t(s_animators[Animators::texPickerFor(matTex)].m_x)
                               , int32_t(s_animators[Animators::texPickerFor(matTex)].m_y)
                               , Gui::PaneWidth
                               , _textureList
                               , _widgetState.m_texPicker[matTex]
                               );

            if (guiEvent(GuiEvent::DismissWindow, _widgetState.m_texPicker[matTex].m_events))
            {
                widgetHide(Widget::LeftSideWidgetMask);
                widgetHide(Widget::LeftSideSubWidgetMask);
            }

            if (guiEvent(GuiEvent::GuiUpdate, _widgetState.m_texPicker[matTex].m_events))
            {
                _widgetState.m_textureBrowser.m_texPickerFor = matTex;
                widgetShow(Widget::TextureFileBrowser);
            }
        }
    }

    if (s_animators[Animators::LeftTextureBrowser].isVisible())
    {
        imguiTextureBrowserWidget(int32_t(s_animators[Animators::LeftTextureBrowser].m_x)
                                , int32_t(s_animators[Animators::LeftTextureBrowser].m_y)
                                , Gui::PaneWidth
                                , _widgetState.m_textureBrowser
                                , enabled
                                );

        if (guiEvent(GuiEvent::GuiUpdate, _widgetState.m_textureBrowser.m_events))
        {
            char msg[128];
            bx::snprintf(msg, sizeof(msg), "Directory: %s", _widgetState.m_textureBrowser.m_directory);
            imguiStatusMessage(msg, 4.0f);
        }

        if (guiEvent(GuiEvent::DismissWindow, _widgetState.m_textureBrowser.m_events))
        {
            widgetHide(Widget::LeftSideSubWidgetMask);
        }
    }

    // Right side.
    if (s_animators[Animators::EnvMapWidget].isVisible())
    {
        const cs::EnvHandle envHandle = _envList[_settings.m_selectedEnvMap];

        imguiEnvMapWidget(envHandle
                        , int32_t(s_animators[Animators::EnvMapWidget].m_x)
                        , int32_t(s_animators[Animators::EnvMapWidget].m_y)
                        , Gui::PaneWidth
                        , _widgetState.m_envMap
                        , enabled
                        );

        if (guiEvent(GuiEvent::DismissWindow, _widgetState.m_envMap.m_events))
        {
            widgetHide(Widget::RightSideWidgetMask);
        }

        if (guiEvent(GuiEvent::GuiUpdate, _widgetState.m_envMap.m_events))
        {
            if (EnvMapWidgetState::Save == _widgetState.m_envMap.m_action)
            {
                static const uint32_t s_selectionToWidget[cs::Environment::Count] =
                {
                    Widget::CmftSaveSkyboxWidget,
                    Widget::CmftSavePmremWidget,
                    Widget::CmftSaveIemWidget,
                };

                const Widget::Enum widget = (Widget::Enum)s_selectionToWidget[_widgetState.m_envMap.m_selection];
                widgetSetOrClear(widget, Widget::RightSideSubwidgetMask);
            }
            else if (EnvMapWidgetState::Info == _widgetState.m_envMap.m_action)
            {
                static const uint32_t s_selectionToWidget[cs::Environment::Count] =
                {
                    Widget::CmftInfoSkyboxWidget,
                    Widget::CmftInfoPmremWidget,
                    Widget::CmftInfoIemWidget,
                };

                const Widget::Enum widget = (Widget::Enum)s_selectionToWidget[_widgetState.m_envMap.m_selection];
                widgetSetOrClear(widget, Widget::RightSideSubwidgetMask);
            }
            else if (EnvMapWidgetState::Transform == _widgetState.m_envMap.m_action)
            {
                widgetSetOrClear(Widget::CmftTransformWidget, Widget::RightSideSubwidgetMask);
            }
            else if (EnvMapWidgetState::Browse == _widgetState.m_envMap.m_action)
            {
                static const uint32_t s_selectionToWidget[cs::Environment::Count] =
                {
                    Widget::SkyboxBrowser,
                    Widget::PmremBrowser,
                    Widget::IemBrowser,
                };

                const Widget::Enum widget = (Widget::Enum)s_selectionToWidget[_widgetState.m_envMap.m_selection];
                widgetSetOrClear(widget, Widget::RightSideSubwidgetMask);
            }
            else if (EnvMapWidgetState::Process == _widgetState.m_envMap.m_action)
            {
                static const uint32_t s_selectionToWidget[cs::Environment::Count] =
                {
                    Widget::TonemapWidget,
                    Widget::CmftPmremWidget,
                    Widget::CmftIemWidget,
                };

                const Widget::Enum widget = (Widget::Enum)s_selectionToWidget[_widgetState.m_envMap.m_selection];
                widgetSetOrClear(widget, Widget::RightSideSubwidgetMask);
            }
        }

        // Env browsers.
        if (s_animators[Animators::SkyboxBrowser].isVisible())
        {
            imguiEnvMapBrowserWidget(getCubemapTypeStr(_widgetState.m_envMap.m_selection)
                                   , int32_t(s_animators[Animators::SkyboxBrowser].m_x)
                                   , int32_t(s_animators[Animators::SkyboxBrowser].m_y)
                                   , Gui::PaneWidth
                                   , _widgetState.m_skyboxBrowser
                                   );

            if (guiEvent(GuiEvent::GuiUpdate, _widgetState.m_skyboxBrowser.m_events))
            {
                char msg[128];
                bx::snprintf(msg, sizeof(msg), "Directory: %s", _widgetState.m_skyboxBrowser.m_directory);
                imguiStatusMessage(msg, 4.0f);
            }

            if (guiEvent(GuiEvent::DismissWindow, _widgetState.m_skyboxBrowser.m_events))
            {
                widgetHide(Widget::RightSideSubwidgetMask);
            }
        }

        if (s_animators[Animators::PmremBrowser].isVisible())
        {
            imguiEnvMapBrowserWidget(getCubemapTypeStr(_widgetState.m_envMap.m_selection)
                                   , int32_t(s_animators[Animators::PmremBrowser].m_x)
                                   , int32_t(s_animators[Animators::PmremBrowser].m_y)
                                   , Gui::PaneWidth
                                   , _widgetState.m_pmremBrowser
                                   );

            if (guiEvent(GuiEvent::GuiUpdate, _widgetState.m_pmremBrowser.m_events))
            {
                char msg[128];
                bx::snprintf(msg, sizeof(msg), "Directory: %s", _widgetState.m_pmremBrowser.m_directory);
                imguiStatusMessage(msg, 4.0f);
            }

            if (guiEvent(GuiEvent::DismissWindow, _widgetState.m_pmremBrowser.m_events))
            {
                widgetHide(Widget::RightSideSubwidgetMask);
            }
        }

        if (s_animators[Animators::IemBrowser].isVisible())
        {
            imguiEnvMapBrowserWidget(getCubemapTypeStr(_widgetState.m_envMap.m_selection)
                                   , int32_t(s_animators[Animators::IemBrowser].m_x)
                                   , int32_t(s_animators[Animators::IemBrowser].m_y)
                                   , Gui::PaneWidth
                                   , _widgetState.m_iemBrowser
                                   );

            if (guiEvent(GuiEvent::GuiUpdate, _widgetState.m_iemBrowser.m_events))
            {
                char msg[128];
                bx::snprintf(msg, sizeof(msg), "Directory: %s", _widgetState.m_iemBrowser.m_directory);
                imguiStatusMessage(msg, 4.0f);
            }

            if (guiEvent(GuiEvent::DismissWindow, _widgetState.m_iemBrowser.m_events))
            {
                widgetHide(Widget::RightSideSubwidgetMask);
            }
        }

        // Cmft info widgets.
        if (s_animators[Animators::CmftInfoSkybox].isVisible())
        {
            imguiCmftInfoWidget(envHandle
                              , int32_t(s_animators[Animators::CmftInfoSkybox].m_x)
                              , int32_t(s_animators[Animators::CmftInfoSkybox].m_y)
                              , Gui::PaneWidth
                              , _widgetState.m_cmftInfoSkybox
                              );

            if (guiEvent(GuiEvent::DismissWindow, _widgetState.m_cmftInfoSkybox.m_events))
            {
                widgetHide(Widget::RightSideSubwidgetMask);
            }
            else if (guiEvent(GuiEvent::GuiUpdate, _widgetState.m_cmftInfoSkybox.m_events))
            {
                _widgetState.m_magnifyWindow.m_env     = _widgetState.m_cmftInfoSkybox.m_env;
                _widgetState.m_magnifyWindow.m_envType = cs::Environment::Skybox;
                _widgetState.m_magnifyWindow.m_lod     = 0.0f;
                widgetShow(Widget::MagnifyWindow);
            }
        }

        if (s_animators[Animators::CmftInfoPmrem].isVisible())
        {
            imguiCmftInfoWidget(envHandle
                              , int32_t(s_animators[Animators::CmftInfoPmrem].m_x)
                              , int32_t(s_animators[Animators::CmftInfoPmrem].m_y)
                              , Gui::PaneWidth
                              , _widgetState.m_cmftInfoPmrem
                              );

            if (guiEvent(GuiEvent::DismissWindow, _widgetState.m_cmftInfoPmrem.m_events))
            {
                widgetHide(Widget::RightSideSubwidgetMask);
            }
            else if (guiEvent(GuiEvent::GuiUpdate, _widgetState.m_cmftInfoPmrem.m_events))
            {
                _widgetState.m_magnifyWindow.m_env     = _widgetState.m_cmftInfoPmrem.m_env;
                _widgetState.m_magnifyWindow.m_envType = cs::Environment::Pmrem;
                _widgetState.m_magnifyWindow.m_lod     = _widgetState.m_cmftInfoPmrem.m_lod;
                widgetShow(Widget::MagnifyWindow);
            }
        }

        if (s_animators[Animators::CmftInfoIem].isVisible())
        {
            imguiCmftInfoWidget(envHandle
                              , int32_t(s_animators[Animators::CmftInfoIem].m_x)
                              , int32_t(s_animators[Animators::CmftInfoIem].m_y)
                              , Gui::PaneWidth
                              , _widgetState.m_cmftInfoIem
                              );

            if (guiEvent(GuiEvent::DismissWindow, _widgetState.m_cmftInfoIem.m_events))
            {
                widgetHide(Widget::RightSideSubwidgetMask);
            }
            else if (guiEvent(GuiEvent::GuiUpdate, _widgetState.m_cmftInfoIem.m_events))
            {
                _widgetState.m_magnifyWindow.m_env     = _widgetState.m_cmftInfoIem.m_env;
                _widgetState.m_magnifyWindow.m_envType = cs::Environment::Iem;
                _widgetState.m_magnifyWindow.m_lod     = 0.0f;
                widgetShow(Widget::MagnifyWindow);
            }
        }

        // Cmft save widgets.
        if (s_animators[Animators::CmftSaveSkybox].isVisible())
        {
            imguiCmftSaveWidget(int32_t(s_animators[Animators::CmftSaveSkybox].m_x)
                              , int32_t(s_animators[Animators::CmftSaveSkybox].m_y)
                              , Gui::PaneWidth
                              , _widgetState.m_cmftSaveSkybox
                              );

            if (guiEvent(GuiEvent::GuiUpdate, _widgetState.m_cmftSaveSkybox.m_events))
            {
                char msg[128];
                bx::snprintf(msg, sizeof(msg), "Directory: %s", _widgetState.m_cmftSaveSkybox.m_directory);
                imguiStatusMessage(msg, 4.0f);
            }

            if (guiEvent(GuiEvent::DismissWindow, _widgetState.m_cmftSaveSkybox.m_events))
            {
                widgetHide(Widget::RightSideSubwidgetMask);
            }
        }

        if (s_animators[Animators::CmftSavePmrem].isVisible())
        {
            imguiCmftSaveWidget(int32_t(s_animators[Animators::CmftSavePmrem].m_x)
                              , int32_t(s_animators[Animators::CmftSavePmrem].m_y)
                              , Gui::PaneWidth
                              , _widgetState.m_cmftSavePmrem
                              );

            if (guiEvent(GuiEvent::DismissWindow, _widgetState.m_cmftSavePmrem.m_events))
            {
                widgetHide(Widget::RightSideSubwidgetMask);
            }
        }

        if (s_animators[Animators::CmftSaveIem].isVisible())
        {
            imguiCmftSaveWidget(int32_t(s_animators[Animators::CmftSaveIem].m_x)
                              , int32_t(s_animators[Animators::CmftSaveIem].m_y)
                              , Gui::PaneWidth
                              , _widgetState.m_cmftSaveIem
                              );

            if (guiEvent(GuiEvent::DismissWindow, _widgetState.m_cmftSaveIem.m_events))
            {
                widgetHide(Widget::RightSideSubwidgetMask);
            }
        }

        // Cmft filter widgets.
        if (s_animators[Animators::CmftPmrem].isVisible())
        {
            imguiCmftPmremWidget(envHandle
                               , int32_t(s_animators[Animators::CmftPmrem].m_x)
                               , int32_t(s_animators[Animators::CmftPmrem].m_y)
                               , Gui::PaneWidth
                               , _widgetState.m_cmftPmrem
                               );

            if (guiEvent(GuiEvent::DismissWindow, _widgetState.m_cmftPmrem.m_events))
            {
                widgetHide(Widget::RightSideSubwidgetMask);
            }
            else if (_widgetState.m_cmftPmrem.m_events & GuiEvent::GuiUpdate)
            {
                imguiStatusMessage("Edge fixup is required for DX9 and OGL without ARB_seamless_cube_map!"
                                  , 8.0f, false, "Close", 0, StatusWindowId::WarpFilterInfo0);
                imguiStatusMessage("Whan used, it should be also handled at runtime. 'Warp filtered cubemap' option will be enabled."
                                  , 8.0f, false, "Close", 0, StatusWindowId::WarpFilterInfo1);
            }
        }

        if (s_animators[Animators::CmftIem].isVisible())
        {
            imguiCmftIemWidget(envHandle
                             , int32_t(s_animators[Animators::CmftIem].m_x)
                             , int32_t(s_animators[Animators::CmftIem].m_y)
                             , Gui::PaneWidth
                             , _widgetState.m_cmftIem
                             );

            if (guiEvent(GuiEvent::DismissWindow, _widgetState.m_cmftIem.m_events))
            {
                widgetHide(Widget::RightSideSubwidgetMask);
            }
        }

        // Cmft transform widget.
        if (s_animators[Animators::CmftTransform].isVisible())
        {
            imguiCmftTransformWidget(envHandle
                                   , int32_t(s_animators[Animators::CmftTransform].m_x)
                                   , int32_t(s_animators[Animators::CmftTransform].m_y)
                                   , Gui::PaneWidth
                                   , _widgetState.m_cmftTransform
                                   , mouse
                                   );

            if (guiEvent(GuiEvent::DismissWindow, _widgetState.m_cmftTransform.m_events))
            {
                widgetHide(Widget::RightSideSubwidgetMask);
            }
        }

        // Tonemap widget.
        if (s_animators[Animators::TonemapWidget].isVisible())
        {
            imguiTonemapWidget(envHandle
                             , int32_t(s_animators[Animators::TonemapWidget].m_x)
                             , int32_t(s_animators[Animators::TonemapWidget].m_y)
                             , Gui::PaneWidth
                             , _widgetState.m_tonemapWidget
                             );

            if (guiEvent(GuiEvent::DismissWindow, _widgetState.m_tonemapWidget.m_events))
            {
                widgetHide(Widget::RightSideSubwidgetMask);
            }
        }
    }

    // Modal windows.
    if (s_animators[Animators::OutputWindow].isVisible())
    {
        imguiModalOutputWindow(int32_t(s_animators[Animators::OutputWindow].m_x)
                             , int32_t(s_animators[Animators::OutputWindow].m_y)
                             , *s_outputWindowState
                             );

        if (guiEvent(GuiEvent::DismissWindow, s_outputWindowState->m_events))
        {
            widgetHide(Widget::ModalWindowMask);
        }
    }

    if (s_animators[Animators::AboutWindow].isVisible())
    {
        imguiModalAboutWindow(int32_t(s_animators[Animators::AboutWindow].m_x)
                            , int32_t(s_animators[Animators::AboutWindow].m_y)
                            , _widgetState.m_aboutWindow
                            );

        if (guiEvent(GuiEvent::DismissWindow, _widgetState.m_aboutWindow.m_events))
        {
            widgetHide(Widget::ModalWindowMask);
        }
    }

    if (s_animators[Animators::MagnifyWindow].isVisible())
    {
        imguiModalMagnifyWindow(int32_t(s_animators[Animators::MagnifyWindow].m_x)
                              , int32_t(s_animators[Animators::MagnifyWindow].m_y)
                              , _widgetState.m_magnifyWindow
                              );

        if (guiEvent(GuiEvent::DismissWindow, _widgetState.m_magnifyWindow.m_events))
        {
            widgetHide(Widget::ModalWindowMask);
        }
    }

    if (s_animators[Animators::ProjectWindow].isVisible())
    {
        imguiModalProjectWindow(int32_t(s_animators[Animators::ProjectWindow].m_x)
                              , int32_t(s_animators[Animators::ProjectWindow].m_y)
                              , _widgetState.m_projectWindow
                              );

        if (guiEvent(GuiEvent::DismissWindow, _widgetState.m_projectWindow.m_events))
        {
            widgetHide(Widget::ModalWindowMask);
            imguiRemoveStatusMessage(StatusWindowId::LoadWarning);
        }

        if (guiEvent(GuiEvent::GuiUpdate, _widgetState.m_projectWindow.m_events))
        {
            if (ProjectWindowState::Warning == _widgetState.m_projectWindow.m_action)
            {
                const char* msg = "Warning: you will lose all your unsaved changes if you proceed!";
                imguiStatusMessage(msg, 6.0f, true, NULL, 0, StatusWindowId::LoadWarning);
            }
            else if (ProjectWindowState::ShowLoadDir == _widgetState.m_projectWindow.m_action)
            {
                char msg[128];
                bx::snprintf(msg, sizeof(msg), "Directory: %s", _widgetState.m_projectWindow.m_load.m_directory);
                imguiStatusMessage(msg, 4.0f);
            }
            else if (ProjectWindowState::ShowSaveDir == _widgetState.m_projectWindow.m_action)
            {
                char msg[128];
                bx::snprintf(msg, sizeof(msg), "Directory: %s", _widgetState.m_projectWindow.m_load.m_directory);
                imguiStatusMessage(msg, 4.0f);
            }
        }
    }

    // Status bars.
    s_statusManager.update(_deltaTime);
    s_statusManager.draw();

    // End imgui.
    imguiEndFrame();
    return imguiMouseOverArea();
}

/* vim: set sw=4 ts=4 expandtab: */
