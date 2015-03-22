/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "common.h"
#include "timer.h"
#include <bx/timer.h>

struct Timer
{
    Timer()
    {
        m_once  = true;
        m_freq  = double(bx::getHPFrequency());
        m_toSec = 1.0/m_freq;
        m_toMs  = 1000.0/m_freq;
    }

    void update()
    {
        if (m_once)
        {
            start();
            m_once = false;
        }

        m_now = bx::getHPCounter();
        m_frameTime = m_now-m_last;
        m_last = m_now;
    }

    double toSec(int64_t _tick) const
    {
        return double(_tick)*m_toSec;
    }

    double toMs(int64_t _tick) const
    {
        return double(_tick)*m_toMs;
    }

    int64_t currentTick() const
    {
        return bx::getHPCounter();
    }

    double currentSec() const
    {
        return toSec(bx::getHPCounter());
    }

    double currentMs() const
    {
        return toMs(bx::getHPCounter());
    }

    int64_t deltaTick() const
    {
        return m_frameTime;
    }

    double deltaSec() const
    {
        return toSec(m_frameTime);
    }

private:
    void start()
    {
        m_begin = bx::getHPCounter();

        m_now  = m_begin;
        m_last = m_begin;
        m_frameTime = 0;
    }

    bool m_once;

    int64_t m_begin;
    int64_t m_now;
    int64_t m_last;
    int64_t m_frameTime;

    double m_freq;
    double m_toSec;
    double m_toMs;
};
static Timer s_timer;

void timerUpdate()
{
    s_timer.update();
}

int64_t timerCurrentTick()
{
    return s_timer.currentTick();
}

double timerCurrentSec()
{
    return s_timer.currentSec();
}

double timerCurrentMs()
{
    return s_timer.currentMs();
}

int64_t timerDeltaTick()
{
    return s_timer.deltaTick();
}

double timerDeltaSec()
{
    return s_timer.deltaSec();
}

double timerToSec(int64_t _tick)
{
    return s_timer.toSec(_tick);
}

double timerToMs(int64_t _tick)
{
    return s_timer.toMs(_tick);
}

/* vim: set sw=4 ts=4 expandtab: */
