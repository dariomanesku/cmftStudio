/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_UTILS_H_HEADER_GUARD
#define CMFTSTUDIO_UTILS_H_HEADER_GUARD

#include <stdint.h>
#include <math.h>
#include <dm/misc.h>
#include <dm/pi.h>

#include "timer.h" //timerCurrentSec()

static inline float lerp(float _a, float _b, float _dt, float _d)
{
    return _a + (_b-_a)*dm::min(_dt/_d, 1.0f);
}

static inline float gaussian(float _x, float _mean, float _stdDev)
{
    const float stdDevSq = _stdDev*_stdDev;
    const float diff = _x - _mean;
    const float diffSq = diff*diff;

    const float exp = -diffSq/(2.0f*stdDevSq);
    const float coeff = 1.0f/sqrtf(dm::twoPi*stdDevSq);

    return coeff * expf(exp);
}

static inline void latLongFromVec(float _uv[2], const float _vec[3])
{
    const float phi = atan2f(_vec[0], _vec[2]);
    const float theta = acosf(_vec[1]);

    _uv[0] = (dm::pi + phi)*dm::invPiHalf;
    _uv[1] = theta*dm::invPi;
}

static inline void vecFromLatLong(float _vec[3], float _u, float _v)
{
    const float phi   = _u * dm::twoPi;
    const float theta = _v * dm::pi;

    _vec[0] = -sinf(theta)*sinf(phi);
    _vec[1] = cosf(theta);
    _vec[2] = -sinf(theta)*cosf(phi);
}

static inline void rightFromLong(float _vec[3], float _rot)
{
    const float phi = _rot * dm::twoPi;

    _vec[0] = cosf(phi);
    _vec[1] = 0.0f;
    _vec[2] = -sinf(phi);
}

struct Transition
{
    Transition()
    {
        m_begin  = 0.0;
        m_end    = 0.0;
        m_active = false;
    }

    void start(double _transitionDuration = 0.35)
    {
        m_begin = timerCurrentSec();
        m_end = m_begin+_transitionDuration;
        m_active = true;
    }

    float progress()
    {
        if (!m_active)
        {
            return 0.0f;
        }

        const double now = timerCurrentSec();
        const float prog = float((now - m_begin)/(m_end - m_begin));
        m_active = (prog < 1.0f);
        return prog;
    }

    bool active() const
    {
        return m_active;
    }

    void end()
    {
        m_active = false;
    }

private:
    double m_begin;
    double m_end;
    bool m_active;
};

struct WidgetAnimator
{
    void reset(bool _visible = false
             , float _x = 0.0f
             , float _y = 0.0f
             , float _treshold = 35.0f
             )
    {
        m_x = _x;
        m_y = _y;
        m_treshold = _treshold;
        m_visible = _visible;
        m_state = uint8_t(_visible ? AtEnd : AtStart);
    }

    void setKeyPoints(float _startX
                    , float _startY
                    , float _endX
                    , float _endY
                    )
    {
        m_startX = _startX;
        m_startY = _startY;
        m_endX   = _endX;
        m_endY   = _endY;
        m_exitX  = _startX;
        m_exitY  = _startY;

        if (AtEnd == m_state)
        {
            fadeIn();
        }
        else if (AtStart == m_state)
        {
            m_x = m_startX;
            m_y = m_startY;
        }
    }

    void setKeyPoints(float _startX
                    , float _startY
                    , float _endX
                    , float _endY
                    , float _exitX
                    , float _exitY
                    )
    {
        m_startX = _startX;
        m_startY = _startY;
        m_endX   = _endX;
        m_endY   = _endY;
        m_exitX  = _exitX;
        m_exitY  = _exitY;

        if (AtEnd == m_state)
        {
            fadeIn();
        }
        else if (AtStart == m_state)
        {
            m_x = m_startX;
            m_y = m_startY;
        }
    }

    void update(float _dt, float _duration)
    {
        CS_CHECK(_duration >= _dt, "Duration param must be longer or equals to deltaTime!");

        if (FadingIn == m_state)
        {
            m_visible = true;

            m_x = lerp(m_x, m_endX + 0.5f, _dt, _duration);
            m_y = lerp(m_y, m_endY + 0.5f, _dt, _duration);

            if (fabsf(m_endX-m_x) < 0.1f
            &&  fabsf(m_endY-m_y) < 0.1f)
            {
                m_x = m_endX;
                m_y = m_endY;
                m_state = AtEnd;
            }
        }
        else if (FadingOut == m_state)
        {
            m_visible = true;

            m_x = lerp(m_x, m_exitX, _dt, _duration);
            m_y = lerp(m_y, m_exitY, _dt, _duration);

            if (fabsf(m_exitX-m_x) < m_treshold
            &&  fabsf(m_exitY-m_y) < m_treshold)
            {
                m_x = m_exitX;
                m_y = m_exitY;
                m_visible = false;
                m_state = AtStart;
            }
        }
    }

    void fadeIn()
    {
        m_state = FadingIn;
    }

    void fadeOut()
    {
        if (m_visible)
        {
            m_state = FadingOut;
        }
    }

    void toggle()
    {
        if (m_visible)
        {
            fadeOut();
        }
        else
        {
            fadeIn();
        }
    }

    bool isVisible() const
    {
        return m_visible;
    }

    enum State
    {
        AtStart,
        AtEnd,
        FadingIn,
        FadingOut,
    };

    float m_x;
    float m_y;
    float m_startX;
    float m_startY;
    float m_endX;
    float m_endY;
    float m_exitX;
    float m_exitY;
    float m_treshold;
    bool m_visible;
    uint8_t m_state;
};

#endif // CMFTSTUDIO_UTILS_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
