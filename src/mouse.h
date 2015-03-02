/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTVIEWER_MOUSE_H_HEADER_GUARD
#define CMFTVIEWER_MOUSE_H_HEADER_GUARD

#include <stdint.h> //uint8_t
#include <common.h> //entry::MouseState

struct Mouse
{
    Mouse()
    {
        m_vx = 0.0f;
        m_vy = 0.0f;

        m_dx = 0.0f;
        m_dy = 0.0f;

        m_scroll     = 0;
        m_scrollPrev = 0;

        m_left   = 0;
        m_middle = 0;
        m_right  = 0;
    }

    void update(entry::MouseState& _mouseState, uint32_t _width, uint32_t _height)
    {
        const float widthf  = float(int32_t(_width));
        const float heightf = float(int32_t(_height));

        const uint8_t buttonEnumIds[ButtonCount] =
        {
            entry::MouseButton::Left,
            entry::MouseButton::Middle,
            entry::MouseButton::Right
        };

        //Buttons.
        memcpy(&m_curr, &_mouseState, sizeof(entry::MouseState));

        for (uint8_t btn = 0; btn < ButtonCount; ++btn)
        {
            const uint8_t p = m_prev.m_buttons[buttonEnumIds[btn]];
            const uint8_t c = m_curr.m_buttons[buttonEnumIds[btn]] << 1;
            m_buttons[btn] = p|c;
        }

        if (Down == (m_left|m_middle|m_right))
        {
            m_prev.m_mx = m_curr.m_mx;
            m_prev.m_my = m_curr.m_my;
            m_prev.m_mz = m_curr.m_mz;
        }

        memcpy(&m_prev.m_buttons, &m_curr.m_buttons, sizeof(m_prev.m_buttons));

        // Screen position.
        m_vx = 2.0f*m_curr.m_mx/widthf  - 1.0f;
        m_vy = 2.0f*m_curr.m_my/heightf - 1.0f;

        // Delta movement.
        m_dx = float(m_curr.m_mx - m_prev.m_mx)/widthf;
        m_dy = float(m_curr.m_my - m_prev.m_my)/heightf;
        m_prev.m_mx = m_curr.m_mx;
        m_prev.m_my = m_curr.m_my;
        m_prev.m_mz = m_curr.m_mz;

        // Scroll.
        int32_t scrollNow = m_curr.m_mz;
        m_scroll = scrollNow - m_scrollPrev;
        m_scrollPrev = scrollNow;
    }

    enum Enum
    {
        None = 0x0,
        Down = 0x2,
        Hold = 0x3,
        Up   = 0x1,
    };

    enum
    {
        ButtonCount = 3,
    };

    float m_vx; // Screen space [-1.0, 1.0].
    float m_vy;
    float m_dx; // Screen space.
    float m_dy;
    int32_t m_scroll;
    int32_t m_scrollPrev;
    union
    {
        struct
        {
            uint8_t m_left;
            uint8_t m_middle;
            uint8_t m_right;
        };

        uint8_t m_buttons[3];
    };
    entry::MouseState m_curr;
    entry::MouseState m_prev;
};

#endif // CMFTSTUDIO_MOUSE_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
