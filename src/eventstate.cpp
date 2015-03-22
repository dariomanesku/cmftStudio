/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "common/common.h"
#include "eventstate.h"

#include <stdint.h>

#include "common/timer.h"
#include <dm/datastructures/array.h>

struct EventImpl
{
    EventImpl()
    {
        m_currFrame = 0;
        m_activeEvents = 0;
    }

    void trigger(Event::Enum _event)
    {
        m_activeEvents |= _event;
    }

    void triggerAfter(uint8_t _numFrames, Event::Enum _event)
    {
        const uint32_t frame = m_currFrame + _numFrames;
        m_frame.      add(frame);
        m_frameEvents.add(_event);
    }

    void triggerAfter(float _sec, Event::Enum _event)
    {
        const float time = float(timerCurrentSec()) + _sec;
        m_time      .add(time);
        m_timeEvents.add(_event);
    }

    void frame()
    {
        ++m_currFrame;
        for (uint32_t ii = m_frame.count(); ii--; )
        {
            if (m_currFrame == m_frame[ii])
            {
                trigger(m_frameEvents[ii]);

                m_frame      .removeSwap(ii);
                m_frameEvents.removeSwap(ii);
            }
        }

        const float currTime = float(timerCurrentSec());
        for (uint32_t ii = m_time.count(); ii--; )
        {
            if (currTime > m_time[ii])
            {
                trigger(m_timeEvents[ii]);

                m_time      .removeSwap(ii);
                m_timeEvents.removeSwap(ii);
            }
        }
    }

    bool check(Event::Enum _event)
    {
        return (0 != (m_activeEvents & _event));
    }

    void markAsHandled(Event::Enum _event)
    {
        m_activeEvents &= ~_event;
    }

    bool handle(Event::Enum _event)
    {
        const bool didHappen = check(_event);
        if (didHappen)
        {
            markAsHandled(_event);
            return true;
        }
        else
        {
            return false;
        }
    }

    void clear()
    {
        m_activeEvents = 0;
    }

    enum { MaxPendingEvents = 32 };

    uint32_t m_currFrame;
    uint8_t m_activeEvents;
    dm::ArrayT<uint32_t,      MaxPendingEvents> m_frame;
    dm::ArrayT<Event::Enum,  MaxPendingEvents> m_frameEvents;
    dm::ArrayT<float,         MaxPendingEvents> m_time;
    dm::ArrayT<Event::Enum,  MaxPendingEvents> m_timeEvents;
};
static EventImpl s_events;

void eventTrigger(Event::Enum _event)
{
    s_events.trigger(_event);
}

void eventTriggerAfter(uint8_t _numFrames, Event::Enum _event)
{
    s_events.triggerAfter(_numFrames, _event);
}

void eventTriggerAfterTime(float _sec, Event::Enum _event)
{
    s_events.triggerAfter(_sec, _event);
}

bool eventCheck(Event::Enum _event)
{
    return s_events.check(_event);
}

bool eventHandle(Event::Enum _event)
{
    return s_events.handle(_event);
}

void eventFrame()
{
    s_events.frame();
}

struct StateImpl
{
    StateImpl()
    {
        m_curr     = State::None;
        m_lastCurr = State::None;
        m_prev     = State::None;
        m_lastPrev = State::None;
    }

    void enter(State::Enum _newState)
    {
        m_prev = m_curr;
        m_curr = _newState;
    }

    void frame()
    {
        m_lastCurr = m_curr;
        m_lastPrev = m_prev;
    }

    State::Enum m_curr;
    State::Enum m_lastCurr;
    State::Enum m_prev;
    State::Enum m_lastPrev;
};
static StateImpl s_state;

bool onState(State::Enum _state)
{
    return (_state == s_state.m_curr);
}

bool onStateEnter(State::Enum _state)
{
    return (s_state.m_curr != s_state.m_lastCurr) && (_state == s_state.m_curr);
}

bool onStateLeave(State::Enum _state)
{
    return (s_state.m_prev != s_state.m_lastPrev) && (_state == s_state.m_prev);
}

bool onStateChange(State::Enum _from, State::Enum _to)
{
    return (_to == s_state.m_curr) && (_from == s_state.m_prev);
}

void stateEnter(State::Enum _newState)
{
    s_state.enter(_newState);
}

void stateFrame()
{
    s_state.frame();
}

/* vim: set sw=4 ts=4 expandtab: */
