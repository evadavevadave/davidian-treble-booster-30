// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Davidian. See LICENSE and NOTICE.md.
// The engine behind every plugin format: reads the control ports, smooths the knobs, runs the
// circuit model (src/dsp/amp.cpp), scales the speaker voltage to samples, applies the speaker
// switch and output level, and crossfades to the (latency-aligned) dry signal when disabled.
#include "engine.h"

#include <cmath>
#include <cstring>
#include <new>

#if defined(_M_X64) || defined(__SSE2__)
#    include <xmmintrin.h>
#    define TB_HAVE_MXCSR 1
#endif

#include "dsp/amp.h"
#include "dsp/cabinet.h"
#include "params.h"

namespace {

const int kChunk = 256;
const int kMaxDelay = 128;

// Knob ports and the circuit parameter each one turns.
const struct { int port, param; } kKnobs[] = {
    {PORT_BRIGHT, tb30::P_VOL_BRIGHT}, {PORT_NORMAL, tb30::P_VOL_NORMAL}, {PORT_VIB, tb30::P_VOL_VIB},
    {PORT_CUT, tb30::P_CUT},           {PORT_DEPTH, tb30::P_DEPTH},
};
const int kNumKnobs = int(sizeof kKnobs / sizeof kKnobs[0]);

}  // namespace

struct TbEngine {
    tb30::Amp amp;
    tb30::Cabinet cab;
    double fs = 48000.0;
    float* port[PORT_COUNT] = {};
    double knob[kNumKnobs] = {};
    int route = -1, speed = -1, mode = -1, foot = -1;
    double inGain = 1.0, outGain = 1.0, spkMix = 1.0, wet = 1.0;
    float dry[kMaxDelay] = {};
    int dryPos = 0, latency = 0;
    double x[kChunk], y[kChunk];
};

static float control(const TbEngine* e, int port) {
    const TbParam* p = tb_param(port);
    const float v = e->port[port] ? *e->port[port] : p->def;
    return tb_clamp(p, v);
}

static double dbToGain(double db) { return std::pow(10.0, db / 20.0); }

// Sends the switch positions to the circuit when they change.
static void applySwitches(TbEngine* e) {
    const int route = int(control(e, PORT_CHANNEL)) * 2 + int(control(e, PORT_JACK));
    const int speed = int(control(e, PORT_SPEED)), mode = int(control(e, PORT_MODE));
    const int foot = int(control(e, PORT_FOOT));
    if (route != e->route) e->amp.setParam(tb30::P_ROUTE, e->route = route);
    if (speed != e->speed) e->amp.setParam(tb30::P_SPEED, e->speed = speed);
    if (mode != e->mode) e->amp.setParam(tb30::P_MODE, e->mode = mode);
    if (foot != e->foot) e->amp.setParam(tb30::P_FOOT, e->foot = foot);
}

extern "C" {

TbEngine* tb_engine_new(double sample_rate) {
    TbEngine* e = new (std::nothrow) TbEngine;
    if (!e) return nullptr;
    e->fs = sample_rate > 0.0 ? sample_rate : 48000.0;
    e->cab.init(e->fs);
    e->amp.init(e->fs);
    e->latency = e->amp.latency();
    if (!e->amp.built() || e->latency >= kMaxDelay) {
        delete e;
        return nullptr;
    }
    return e;
}

void tb_engine_connect(TbEngine* e, uint32_t port, void* data) {
    if (port < PORT_COUNT) e->port[port] = static_cast<float*>(data);
}

void tb_engine_activate(TbEngine* e) {
    // Start from the current settings, with nothing mid-glide, at the circuit's operating point.
    for (int i = 0; i < kNumKnobs; ++i) {
        e->knob[i] = control(e, kKnobs[i].port);
        e->amp.setParam(kKnobs[i].param, e->knob[i]);
    }
    e->route = e->speed = e->mode = e->foot = -1;
    applySwitches(e);
    e->amp.init(e->fs);
    e->cab.reset();
    e->inGain = control(e, PORT_VOLTS);
    e->outGain = dbToGain(control(e, PORT_OUTPUT)) / SPEAKER_FULL_SCALE;
    e->spkMix = control(e, PORT_SPEAKER);
    e->wet = control(e, PORT_ENABLED) > 0.5f ? 1.0 : 0.0;
    std::memset(e->dry, 0, sizeof e->dry);
    e->dryPos = 0;
}

uint32_t tb_engine_latency(const TbEngine* e) { return uint32_t(e->latency); }

uint32_t tb_latency_for_rate(double sample_rate) {
    tb30::Oversampler os;
    os.init(tb30::Amp::oversampling(sample_rate));
    return uint32_t(os.latency());
}

void tb_engine_run(TbEngine* e, uint32_t frames) {
    const float* in = e->port[PORT_IN];
    float* out = e->port[PORT_OUT];
    if (e->port[PORT_LATENCY]) *e->port[PORT_LATENCY] = float(e->latency);
    if (!in || !out || frames == 0) return;

#ifdef TB_HAVE_MXCSR
    // Flush denormals to zero, the same in every host, so every format gives the same output.
    const unsigned csr = _mm_getcsr();
    _mm_setcsr(csr | 0x8040);
#endif

    // Knobs glide to their settings with a 30 ms time constant, per block.
    const double glide = 1.0 - std::exp(-double(frames) / (0.03 * e->fs));
    for (int i = 0; i < kNumKnobs; ++i) {
        const double target = control(e, kKnobs[i].port);
        double v = e->knob[i] + (target - e->knob[i]) * glide;
        if (std::fabs(target - v) < 1e-4) v = target;
        if (v != e->knob[i]) {
            e->knob[i] = v;
            e->amp.setParam(kKnobs[i].param, v);
        }
    }
    applySwitches(e);
    e->amp.applyControls();

    // Level and speaker: 10 ms one-pole glides per sample. Bypass: a 20 ms straight fade, which
    // ends exactly on the dry signal.
    const double rc = 1.0 - std::exp(-1.0 / (0.01 * e->fs));
    const double inTarget = control(e, PORT_VOLTS);
    const double outTarget = dbToGain(control(e, PORT_OUTPUT)) / SPEAKER_FULL_SCALE;
    const double spkTarget = control(e, PORT_SPEAKER);
    const double wetTarget = control(e, PORT_ENABLED) > 0.5f ? 1.0 : 0.0;
    const double wetStep = 1.0 / (0.02 * e->fs);

    for (uint32_t done = 0; done < frames;) {
        const int n = int(frames - done < uint32_t(kChunk) ? frames - done : uint32_t(kChunk));
        const float* src = in + done;
        float* dst = out + done;
        double ig = e->inGain;
        for (int i = 0; i < n; ++i) {
            ig += (inTarget - ig) * rc;
            e->x[i] = double(src[i]) * ig;
        }
        e->inGain = ig;
        e->amp.process(e->x, e->y, n);
        for (int i = 0; i < n; ++i) {
            // Read the input before writing the output, so in-place buffers work.
            const float drySample = src[i];
            const int rd = (e->dryPos - e->latency + kMaxDelay) % kMaxDelay;
            e->dry[e->dryPos] = drySample;
            const float delayed = e->dry[rd];
            e->dryPos = (e->dryPos + 1) % kMaxDelay;

            e->outGain += (outTarget - e->outGain) * rc;
            e->spkMix += (spkTarget - e->spkMix) * rc;
            if (e->wet < wetTarget) e->wet = std::fmin(e->wet + wetStep, wetTarget);
            else if (e->wet > wetTarget) e->wet = std::fmax(e->wet - wetStep, wetTarget);
            double v = e->y[i] * e->outGain;
            if (!std::isfinite(v)) v = 0.0;
            const double c = e->cab.process(v);
            v += (c - v) * e->spkMix;
            v = v > 4.0 ? 4.0 : (v < -4.0 ? -4.0 : v);  // A safety limit far above full scale.
            dst[i] = float(delayed + (v - delayed) * e->wet);
        }
        done += uint32_t(n);
    }

    if (e->port[PORT_LAMP]) {
        const double lamp = e->amp.lfoVolts() / 14.0;
        *e->port[PORT_LAMP] = float(lamp < 0.0 ? 0.0 : (lamp > 1.0 ? 1.0 : lamp));
    }
#ifdef TB_HAVE_MXCSR
    _mm_setcsr(csr);
#endif
}

void tb_engine_free(TbEngine* e) { delete e; }

}  // extern "C"
