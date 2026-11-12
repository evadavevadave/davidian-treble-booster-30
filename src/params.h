/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Davidian. See LICENSE and NOTICE.md. */
/* Control metadata for the hosts that don't read bundle/treble30.ttl: the VST3 and Audio Unit
 * builds. Values are the same plain values as the LV2 ports, so a setting means the same thing
 * in every format. test/params_check.c checks this table against the TTL and the panel. */
#ifndef TB_PARAMS_H
#define TB_PARAMS_H

#include <math.h>
#include <stdio.h>

#include "ports.h"

typedef struct {
    int port;          /* also the parameter ID in VST3 and the Audio Unit */
    const char* name;
    const char* short_name;
    float min, max, def;
    int log;           /* logarithmic */
    int steps;         /* 0: continuous; otherwise a switch with steps + 1 positions */
    const char* unit;  /* "V", "dB", or "" for 0-1 knobs shown as 0-10 like the panel */
    const char* opts[3];
    int output;        /* read only: the oscillator lamp */
} TbParam;

static const TbParam TB_PARAMS[] = {
    { PORT_CHANNEL, "Input Channel", "Channel", 0, 2, 1, 0, 2, "", { "Bright", "Normal", "Vib-Trem" }, 0 },
    { PORT_JACK, "Input Jack", "Jack", 0, 1, 0, 0, 1, "", { "Input 1", "Input 2", 0 }, 0 },
    { PORT_BRIGHT, "Bright Volume", "Bright", 0, 1, 0, 0, 0, "", { 0, 0, 0 }, 0 },
    { PORT_NORMAL, "Normal Volume", "Normal", 0, 1, 0.5f, 0, 0, "", { 0, 0, 0 }, 0 },
    { PORT_VIB, "Vib-Trem Volume", "Vib-Trem", 0, 1, 0, 0, 0, "", { 0, 0, 0 }, 0 },
    { PORT_CUT, "Cut", "Cut", 0, 1, 0.3f, 0, 0, "", { 0, 0, 0 }, 0 },
    { PORT_SPEED, "Vib-Trem Speed", "Speed", 0, 2, 1, 0, 2, "", { "Slow", "Medium", "Fast" }, 0 },
    { PORT_MODE, "Vib-Trem Mode", "Mode", 0, 1, 0, 0, 1, "", { "Vibrato", "Tremolo", 0 }, 0 },
    { PORT_FOOT, "Vib-Trem Footswitch", "Foot", 0, 1, 1, 0, 1, "", { "Off", "On", 0 }, 0 },
    { PORT_DEPTH, "Depth Preset", "Depth", 0, 1, 0.5f, 0, 0, "", { 0, 0, 0 }, 0 },
    { PORT_SPEAKER, "Speaker", "Speaker", 0, 1, 1, 0, 1, "", { "Off", "On", 0 }, 0 },
    { PORT_VOLTS, "Volts at 0 dBFS", "Volts", 0.1f, 10.0f, 1.0f, 1, 0, "V", { 0, 0, 0 }, 0 },
    { PORT_OUTPUT, "Output", "Output", -24.0f, 12.0f, 0.0f, 0, 0, "dB", { 0, 0, 0 }, 0 },
    { PORT_ENABLED, "Enabled", "Enabled", 0, 1, 1, 0, 1, "", { "Off", "On", 0 }, 0 },
    { PORT_LAMP, "Oscillator Lamp", "Lamp", 0, 1, 0, 0, 0, "", { 0, 0, 0 }, 1 },
};
#define TB_NUM_PARAMS ((int)(sizeof TB_PARAMS / sizeof TB_PARAMS[0]))

/* The first control port and the number of saved values: every input control, in port order. */
#define TB_FIRST_CONTROL PORT_CHANNEL
#define TB_NUM_INPUTS (PORT_ENABLED - PORT_CHANNEL + 1)

static inline const TbParam* tb_param(int port)
{
    int i;
    for (i = 0; i < TB_NUM_PARAMS; ++i)
        if (TB_PARAMS[i].port == port)
            return &TB_PARAMS[i];
    return NULL;
}

static inline float tb_clamp(const TbParam* p, float v)
{
    if (!(v >= p->min)) /* also catches NaN */
        v = p->min;
    if (v > p->max)
        v = p->max;
    return p->steps ? floorf(v + 0.5f) : v;
}

/* 0-1 host value to plain value. Switches split 0-1 into equal parts, as the VST3 SDK does. */
static inline float tb_from_norm(const TbParam* p, double n)
{
    n = n < 0.0 ? 0.0 : (n > 1.0 ? 1.0 : n);
    if (p->steps) {
        int i = (int)(n * (p->steps + 1));
        return p->min + (float)(i > p->steps ? p->steps : i);
    }
    if (p->log)
        return (float)(p->min * pow((double)p->max / p->min, n));
    return (float)(p->min + n * (p->max - p->min));
}

static inline double tb_to_norm(const TbParam* p, float v)
{
    v = tb_clamp(p, v);
    if (p->steps)
        return (double)(v - p->min) / p->steps;
    if (p->log)
        return log((double)v / p->min) / log((double)p->max / p->min);
    return (double)(v - p->min) / (p->max - p->min);
}

/* Text for a value, matching the panel's display. */
static inline void tb_format(const TbParam* p, float v, char* buf, size_t n)
{
    v = tb_clamp(p, v);
    if (p->steps)
        snprintf(buf, n, "%s", p->opts[(int)(v - p->min)]);
    else if (p->unit[0] == 'd')
        snprintf(buf, n, "%+.1f", v);
    else if (p->unit[0])
        snprintf(buf, n, "%.2f", v);
    else if (p->output)
        snprintf(buf, n, "%.0f%%", 100.0f * v);
    else
        snprintf(buf, n, "%.1f", 10.0f * v);
}

/* The reverse of tb_format; returns 0 if the text means nothing. */
static inline int tb_parse(const TbParam* p, const char* s, float* out)
{
    double d;
    int i;
    for (i = 0; p->steps && i <= p->steps; ++i) {
        const char *a = p->opts[i], *b = s;
        while (*a && *b && (*a | 32) == (*b | 32))
            ++a, ++b;
        if (!*a && !*b) {
            *out = p->min + (float)i;
            return 1;
        }
    }
#ifdef _MSC_VER
#    pragma warning(push)
#    pragma warning(disable : 4996) /* sscanf_s is not portable */
#endif
    if (sscanf(s, "%lf", &d) != 1)
        return 0;
#ifdef _MSC_VER
#    pragma warning(pop)
#endif
    if (!p->unit[0] && !p->steps)
        d /= 10.0; /* 0-10 scale */
    *out = tb_clamp(p, (float)d);
    return 1;
}

#endif
