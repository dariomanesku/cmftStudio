/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef CMFTSTUDIO_STATIC_RESOURCES_H_HEADER_GUARD
#define CMFTSTUDIO_STATIC_RESOURCES_H_HEADER_GUARD

#include <stdint.h>

// Extern declarations.
#define SR_DESC(_name, _dataArray) \
    extern void*    g_ ## _name;   \
    extern uint32_t g_ ## _name ## Size;
#include "staticres_res.h"

#define SR_DESC_COMPR(_name, _dataArray) \
    extern void*    g_ ## _name;         \
    extern uint32_t g_ ## _name ## Size;
#include "staticres_res.h"

void initStaticResources();

#endif // CMFTSTUDIO_STATIC_RESOURCES_H_HEADER_GUARD

/* vim: set sw=4 ts=4 expandtab: */
