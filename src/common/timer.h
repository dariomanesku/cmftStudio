/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_TIMER_H_HEADER_GUARD
#define CMFTSTUDIO_TIMER_H_HEADER_GUARD

#include <stdint.h>

void    timerUpdate();
int64_t timerCurrentTick();
double  timerCurrentSec();
double  timerCurrentMs();
int64_t timerDeltaTick();
double  timerDeltaSec();
double  timerToSec(int64_t _tick);
double  timerToMs(int64_t _tick);

#endif // CMFTSTUDIO_TIMER_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
