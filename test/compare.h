/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Davidian. See LICENSE and NOTICE.md. */
/* The reference run for vst3_check and au_check: the LV2 build, driven through its own ABI with
 * a fixed test signal and settings, so the other formats can be compared with it sample for
 * sample. Each host starts from the defaults, changes CMP_CHANGES before the first block and
 * bypasses from block CMP_BYPASS on. The amplifier is mono: the hosts feed the same signal to
 * both input channels and should give the same output on both. */
#ifndef TB_COMPARE_H
#define TB_COMPARE_H

#include <math.h>
#include <stdlib.h>

#include "../src/lv2_abi.h"
#include "../src/params.h"

#ifdef _WIN32
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#    define CMP_OPEN(p) ((void*)LoadLibraryA(p))
#    define CMP_SYM(l, s) ((void*)GetProcAddress((HMODULE)(l), s))
#    define CMP_CLOSE(l) FreeLibrary((HMODULE)(l))
#    define CMP_LIB "treble30.dll"
#else
#    include <dlfcn.h>
#    define CMP_OPEN(p) dlopen(p, RTLD_NOW | RTLD_LOCAL)
#    define CMP_SYM(l, s) dlsym(l, s)
#    define CMP_CLOSE(l) dlclose(l)
#    ifdef __APPLE__
#        define CMP_LIB "treble30.dylib"
#    else
#        define CMP_LIB "treble30.so"
#    endif
#endif

#define CMP_BLOCK 512
#define CMP_BLOCKS 60
#define CMP_BYPASS 40
#define CMP_RATE 48000.0
#define CMP_FRAMES (CMP_BLOCK * CMP_BLOCKS)
#define CMP_LATENCY 16 /* at 48 kHz */

typedef struct { int port; float plain; } CmpChange;

/* The Vib-Trem channel with its vibrato running, so the comparison covers every section. */
static const CmpChange CMP_CHANGES[] = {
    { PORT_CHANNEL, 2.0f }, { PORT_VIB, 0.6f }, { PORT_CUT, 0.5f }, { PORT_OUTPUT, -6.0f },
};
#define CMP_NUM_CHANGES ((int)(sizeof CMP_CHANGES / sizeof CMP_CHANGES[0]))

/* Two tones plus a little deterministic noise; the same on both channels. */
static float cmp_input(int ch, int n)
{
    const double t = n / CMP_RATE;
    unsigned x = (unsigned)n * 2654435761u;
    (void)ch;
    x ^= x >> 13;
    return (float)(0.2 * sin(2 * 3.14159265358979 * 220.0 * t) + 0.1 * sin(2 * 3.14159265358979 * 330.0 * t)
                   + 0.02 * ((x & 0xffff) / 32768.0 - 1.0));
}

/* Interleaved stereo output of the LV2 build (CMP_FRAMES frames, the mono output on both
 * channels), or NULL. Free it with free(). */
static float* cmp_run_lv2(const char* bundle)
{
    char path[1024];
    void* lib;
    const LV2_Descriptor* (*get)(uint32_t);
    const LV2_Descriptor* d;
    LV2_Handle h;
    float ctl[PORT_COUNT] = { 0 }, in[CMP_BLOCK], o[CMP_BLOCK];
    float* out;
    int i, b;

    snprintf(path, sizeof path, "%s/%s", bundle, CMP_LIB);
    lib = CMP_OPEN(path);
    if (!lib)
        return NULL;
    *(void**)&get = CMP_SYM(lib, "lv2_descriptor");
    d = get ? get(0) : NULL;
    h = d ? d->instantiate(d, CMP_RATE, bundle, NULL) : NULL;
    out = (float*)malloc(sizeof(float) * 2 * CMP_FRAMES);
    if (!h || !out) {
        free(out);
        CMP_CLOSE(lib);
        return NULL;
    }
    for (i = 0; i < TB_NUM_PARAMS; ++i)
        ctl[TB_PARAMS[i].port] = TB_PARAMS[i].def;
    for (i = TB_FIRST_CONTROL; i < PORT_COUNT; ++i)
        d->connect_port(h, (uint32_t)i, &ctl[i]);
    d->connect_port(h, PORT_IN, in);
    d->connect_port(h, PORT_OUT, o);
    d->activate(h);
    for (i = 0; i < CMP_NUM_CHANGES; ++i)
        ctl[CMP_CHANGES[i].port] = CMP_CHANGES[i].plain;
    for (b = 0; b < CMP_BLOCKS; ++b) {
        if (b == CMP_BYPASS)
            ctl[PORT_ENABLED] = 0.0f;
        for (i = 0; i < CMP_BLOCK; ++i)
            in[i] = cmp_input(0, b * CMP_BLOCK + i);
        d->run(h, CMP_BLOCK);
        for (i = 0; i < CMP_BLOCK; ++i)
            out[2 * (b * CMP_BLOCK + i)] = out[2 * (b * CMP_BLOCK + i) + 1] = o[i];
    }
    d->cleanup(h);
    CMP_CLOSE(lib);
    return out;
}

/* Largest difference between two interleaved outputs, their peak, and how far the bypassed
 * tail (two blocks after the bypass, once the 20 ms fade is over) is from the input delayed by
 * the plugin's latency. */
static void cmp_measure(const float* a, const float* ref, double* peak, double* diff, double* bypass_diff)
{
    int i;
    *peak = *diff = *bypass_diff = 0.0;
    for (i = 0; i < 2 * CMP_FRAMES; ++i) {
        *peak = fmax(*peak, fabs((double)a[i]));
        *diff = fmax(*diff, fabs((double)a[i] - ref[i]));
        if (i >= 2 * (CMP_BYPASS + 2) * CMP_BLOCK)
            *bypass_diff = fmax(*bypass_diff, fabs((double)a[i] - cmp_input(i & 1, i / 2 - CMP_LATENCY)));
    }
}

#endif
