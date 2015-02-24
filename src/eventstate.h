/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_EVENTS_H_HEADER_GUARD
#define CMFTSTUDIO_EVENTS_H_HEADER_GUARD

struct Event
{
    enum Enum
    {
        ProjectIsLoading       = 0x1,
        ProjectLoaded          = 0x2,
        BeginLoadTransition    = 0x4,
        LoadTransitionComplete = 0x8,
    };
};

void eventTrigger(Event::Enum _event);
void eventTriggerAfter(uint8_t _num, Event::Enum _event);
void eventTriggerAfterTime(float _sec, Event::Enum _event);
bool eventCheck(Event::Enum _event);
bool eventHandle(Event::Enum _event);
void eventFrame(); // Call this in the current frame, before handling events.

struct State
{
    enum Enum
    {
        None,
        SplashScreen,
        IntroAnimation,
        MainState,
        SendResourcesToGpu,
        ProjectLoadTransition,
    };
};

bool onState(State::Enum _state);
bool onStateEnter(State::Enum _state);
bool onStateLeave(State::Enum _state);
bool onStateChange(State::Enum _from, State::Enum _to);
void stateEnter(State::Enum _newState);
void stateFrame(); // Call this in the current frame, after handling state changes.

#endif // CMFTSTUDIO_EVENTS_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
