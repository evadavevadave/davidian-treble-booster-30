// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Davidian. See LICENSE and NOTICE.md.
// Checks for the Treble Booster 30. Usage: test_dsp <bundle folder>
// Part 1 drives the circuit model directly: the operating point, the volume network's voicing,
// the Cut control, the oscillator's three speeds, vibrato, tremolo and the footswitch, the
// output power and supply sag, and agreement between sample rates. The schematic gives no
// voltages or measurements, so most limits are sanity bounds for a 30 watt class A amplifier;
// each says what it guards. Part 2 loads the built LV2 library the way a host does.
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

#include "../src/dsp/amp.h"
#include "../src/dsp/cabinet.h"
#include "../src/lv2_abi.h"
#include "../src/ports.h"

#ifdef _WIN32
#    define NOMINMAX
#    include <windows.h>
#    define LIB_NAME "treble30.dll"
#    define LIB_OPEN(p) ((void*)LoadLibraryA(p))
#    define LIB_SYM(l, s) ((void*)GetProcAddress((HMODULE)(l), s))
#else
#    include <dlfcn.h>
#    ifdef __APPLE__
#        define LIB_NAME "treble30.dylib"
#    else
#        define LIB_NAME "treble30.so"
#    endif
#    define LIB_OPEN(p) dlopen(p, RTLD_NOW)
#    define LIB_SYM(l, s) dlsym(l, s)
#endif

using namespace tb30;

namespace {

const double kPi = 3.14159265358979323846;
int failures = 0;

void check(const char* what, bool ok) {
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

struct Setting {
    int route = 0;
    double bright = 0.5, normal = 0.0, vib = 0.0, cut = 0.3, depth = 0.5;
    int speed = 1, mode = 0, foot = 1;
};

std::unique_ptr<Amp> make(double fs, const Setting& s) {
    std::unique_ptr<Amp> a(new Amp);
    a->setParam(P_ROUTE, s.route);
    a->setParam(P_VOL_BRIGHT, s.bright);
    a->setParam(P_VOL_NORMAL, s.normal);
    a->setParam(P_VOL_VIB, s.vib);
    a->setParam(P_CUT, s.cut);
    a->setParam(P_DEPTH, s.depth);
    a->setParam(P_SPEED, s.speed);
    a->setParam(P_MODE, s.mode);
    a->setParam(P_FOOT, s.foot);
    a->init(fs);
    return a;
}

// Runs a sine through and returns the output, after a settling time.
std::vector<double> run(Amp& a, double fs, double hz, double volts, double seconds, double settle = 0.3) {
    const int n = int(fs * (seconds + settle)), skip = int(fs * settle);
    std::vector<double> x(n), y(n);
    for (int i = 0; i < n; ++i) x[i] = volts * std::sin(2 * kPi * hz * i / fs);
    for (int i = 0; i < n; i += 512) a.process(&x[i], &y[i], std::min(512, n - i));
    return std::vector<double>(y.begin() + skip, y.end());
}

double rms(const std::vector<double>& y) {
    double s = 0.0;
    for (double v : y) s += v * v;
    return std::sqrt(s / double(y.size()));
}

bool finite(const std::vector<double>& y) {
    for (double v : y)
        if (!std::isfinite(v)) return false;
    return true;
}

double db(double r) { return 20.0 * std::log10(std::fmax(r, 1e-12)); }

// Small-signal gain (dB) at hz for a setting: 20 mV in, speaker volts out.
double gainAt(double fs, const Setting& s, double hz) {
    auto a = make(fs, s);
    return db(rms(run(*a, fs, hz, 0.02, 0.2)) / (0.02 / std::sqrt(2.0)));
}

// Pitch swing (cents) and level swing (dB) of a 1 kHz tone, cycle by cycle.
void swing(const std::vector<double>& y, double fs, double& cents, double& levelDb) {
    double pmin = 1e9, pmax = 0, amin = 1e9, amax = 0, prev = -1, pk = 0;
    for (size_t i = 1; i < y.size(); ++i) {
        pk = std::fmax(pk, std::fabs(y[i]));
        if (y[i - 1] < 0 && y[i] >= 0) {
            const double tc = double(i) - 1 + y[i - 1] / (y[i - 1] - y[i]);
            if (prev > 0) {
                pmin = std::fmin(pmin, tc - prev);
                pmax = std::fmax(pmax, tc - prev);
                amin = std::fmin(amin, pk);
                amax = std::fmax(amax, pk);
            }
            prev = tc;
            pk = 0;
        }
    }
    (void)fs;
    cents = 1200.0 * std::log2(pmax / pmin);
    levelDb = db(amax / std::fmax(amin, 1e-9));
}

// Oscillator frequency from the lamp signal.
double lfoHz(Amp& a, double fs, double seconds) {
    const int n = int(fs * seconds), blk = 64;
    std::vector<double> x(blk, 0.0), y(blk), lfo;
    for (int i = 0; i < n; i += blk) {
        a.process(x.data(), y.data(), blk);
        lfo.push_back(a.lfoVolts());
    }
    const size_t half = lfo.size() / 2;
    double lo = 1e9, hi = -1e9;
    for (size_t i = half; i < lfo.size(); ++i) {
        lo = std::fmin(lo, lfo[i]);
        hi = std::fmax(hi, lfo[i]);
    }
    const double mid = 0.5 * (lo + hi);
    int cross = 0;
    long first = -1, last = -1;
    for (size_t i = half + 1; i < lfo.size(); ++i)
        if (lfo[i - 1] < mid && lfo[i] >= mid) {
            if (first < 0) first = long(i);
            last = long(i);
            ++cross;
        }
    if (cross < 2 || hi - lo < 1.0) return 0.0;
    return (cross - 1) * (fs / blk) / double(last - first);
}

void part1() {
    char what[256];
    const double fs = 48000.0;
    std::printf("Circuit model\n");

    {
        Setting s;
        auto a = make(fs, s);
        check("all four sections fit the solver", a->built());
        check("DC operating point converges", a->dcConverged());
        const double k = a->probeVolts(Amp::T_EL84_CATHODE), va = a->probeVolts(Amp::T_EL84_PLATE);
        const double v1 = a->probeVolts(Amp::T_V1_PLATE), pi = a->probeVolts(Amp::T_PI_PLATE);
        std::printf("  supply %.0f V (reservoir), %.0f V (C40), %.0f V (C8), %.0f V (C19)\n",
                    a->probeVolts(Amp::T_RAIL_RES), a->probeVolts(Amp::T_RAIL_A), a->probeVolts(Amp::T_RAIL_B),
                    a->probeVolts(Amp::T_RAIL_P));
        std::printf("  V1 anode %.0f V, cathode %.2f V; V2 anodes %.0f V, cathodes %.1f V; EL84 anodes %.0f V, cathodes %.1f V\n",
                    v1, a->probeVolts(Amp::T_V1_CATHODE), pi, a->probeVolts(Amp::T_PI_TAIL), va, k);
        const double ia = k / 80.0 / 4.0;  // cathode current per valve through R24
        std::snprintf(what, sizeof what, "EL84s idle in class A: %.1f V on R24 is %.0f mA a valve, %.1f W anode dissipation (EL84 rating 12 W)",
                      k, 1e3 * ia, ia * (va - k) * 0.92);
        check(what, k > 8.0 && k < 16.0 && ia * (va - k) * 0.92 < 13.0);
        check("V1 and V2 anodes sit between a quarter and nine tenths of their supplies (neither cut off nor saturated)",
              v1 > 0.25 * a->probeVolts(Amp::T_RAIL_A) && v1 < 0.9 * a->probeVolts(Amp::T_RAIL_A) &&
                  pi > 0.25 * a->probeVolts(Amp::T_RAIL_B) && pi < 0.9 * a->probeVolts(Amp::T_RAIL_B));
    }

    // Voicing: the Bright volume reaches the shared anode through C3 (500 pF), the Normal volume
    // through C2 (0.05 uF), so the Bright path is the brighter of the two.
    {
        Setting b, nrm;
        b.bright = 0.6;
        b.normal = 0.0;
        b.cut = 0.0;
        nrm.bright = 0.0;
        nrm.normal = 0.6;
        nrm.cut = 0.0;
        nrm.route = 2;
        const double bLo = gainAt(fs, b, 150), bHi = gainAt(fs, b, 3000);
        const double nLo = gainAt(fs, nrm, 150), nHi = gainAt(fs, nrm, 3000);
        std::printf("  Bright volume path: %.1f dB at 150 Hz, %.1f dB at 3 kHz; Normal: %.1f dB, %.1f dB\n", bLo, bHi,
                    nLo, nHi);
        std::snprintf(what, sizeof what, "Bright path tilts %.1f dB more towards treble than the Normal path",
                      (bHi - bLo) - (nHi - nLo));
        check(what, (bHi - bLo) - (nHi - nLo) > 6.0);
    }

    // Cut: VR3 and C10 between the output grids take the top off, not the bottom.
    {
        Setting s0, s1;
        s0.cut = 0.0;
        s1.cut = 1.0;
        const double lo0 = gainAt(fs, s0, 200), lo1 = gainAt(fs, s1, 200);
        const double hi0 = gainAt(fs, s0, 5000), hi1 = gainAt(fs, s1, 5000);
        std::snprintf(what, sizeof what, "Cut at 10 removes %.1f dB at 5 kHz and %.1f dB at 200 Hz", hi0 - hi1, lo0 - lo1);
        check(what, hi0 - hi1 > 6.0 && std::fabs(lo0 - lo1) < 2.0);
    }

    // Oscillator: SW1 picks the third resistor of the phase-shift ladder.
    {
        double hz[3];
        for (int sp = 0; sp < 3; ++sp) {
            Setting s;
            s.speed = sp;
            auto a = make(fs, s);
            hz[sp] = lfoHz(*a, fs, 6.0);
        }
        std::snprintf(what, sizeof what, "oscillator runs at %.2f, %.2f and %.2f Hz (slow, medium, fast)", hz[0], hz[1], hz[2]);
        check(what, hz[0] > 2.0 && hz[0] < hz[1] && hz[1] < hz[2] && hz[2] < 15.0);
    }

    // Vibrato, tremolo and the footswitch, on a 1 kHz tone into the Vib/Trem channel.
    {
        double cents[3], level[3];
        long fails = 0;
        for (int c = 0; c < 3; ++c) {
            Setting s;
            s.route = 4;
            s.bright = 0.0;
            s.vib = 0.5;
            s.mode = c == 1 ? 1 : 0;
            s.foot = c == 2 ? 0 : 1;
            auto a = make(fs, s);
            const std::vector<double> y = run(*a, fs, 1000.0, 0.05, 2.0, 1.0);
            swing(y, fs, cents[c], level[c]);
            fails += a->solverFailures();
        }
        std::snprintf(what, sizeof what, "vibrato swings the pitch %.0f cents (with %.1f dB of level)", cents[0], level[0]);
        check(what, cents[0] > 15.0 && cents[0] < 200.0);
        std::snprintf(what, sizeof what, "tremolo swings the level %.1f dB", level[1]);
        check(what, level[1] > 10.0);
        std::snprintf(what, sizeof what, "footswitch off stops it: %.2f cents, %.2f dB", cents[2], level[2]);
        check(what, cents[2] < 2.0 && level[2] < 0.5);
        check("no solver failures in the vib-trem runs", fails == 0);
    }

    // Output power and sag: driven hard, the four EL84s give about their rating, and the
    // valve rectifier's supply sags.
    {
        Setting s;
        s.bright = 1.0;
        auto a = make(fs, s);
        const double idle = a->probeVolts(Amp::T_RAIL_RES);
        const std::vector<double> y = run(*a, fs, 440.0, 1.0, 1.0, 0.5);
        const double watts = rms(y) * rms(y) / 15.0;
        const double sag = idle - a->probeVolts(Amp::T_RAIL_RES);
        std::snprintf(what, sizeof what, "flat out: %.1f W into the 15 ohm speakers, supply sags %.1f V", watts, sag);
        check(what, watts > 18.0 && watts < 45.0 && sag > 3.0);
        check("no solver failures when driven hard", a->solverFailures() == 0 && finite(y));
    }

    // Sample rates: the circuit runs near 96 kHz inside at every host rate.
    {
        Setting s;
        double g[3];
        const double rates[3] = {44100.0, 48000.0, 96000.0};
        for (int r = 0; r < 3; ++r) g[r] = gainAt(rates[r], s, 1000.0);
        std::snprintf(what, sizeof what, "1 kHz gain %.2f / %.2f / %.2f dB at 44.1 / 48 / 96 kHz", g[0], g[1], g[2]);
        check(what, std::fabs(g[0] - g[1]) < 0.3 && std::fabs(g[2] - g[1]) < 0.3);
    }

    // Speaker switch filter: level-matched and shaped like a guitar speaker.
    {
        Cabinet c;
        c.init(48000.0);
        std::snprintf(what, sizeof what, "speaker filter: %.1f dB at 1 kHz, %.1f dB at 50 Hz, %.1f dB at 10 kHz",
                      db(c.response(1000)), db(c.response(50)), db(c.response(10000)));
        check(what, std::fabs(db(c.response(1000))) < 6.0 && db(c.response(50)) < -6.0 && db(c.response(10000)) < -15.0);
    }

    // Cost, reported only.
    {
        Setting s;
        for (int v = 0; v < 2; ++v) {
            s.vib = v ? 0.5 : 0.0;
            auto a = make(fs, s);
            const auto t0 = std::chrono::steady_clock::now();
            run(*a, fs, 440.0, 0.2, 2.0, 0.0);
            const double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            std::printf("  cost: %.1f%% of one core at 48 kHz (Vib-Trem volume %s)\n", 100.0 * t / 2.0, v ? "up" : "down");
        }
    }
}

// ---- Part 2: the built plugin, through its LV2 ABI ----

void part2(const char* bundle) {
    char path[1024], what[256];
    std::printf("LV2 plugin library\n");
    std::snprintf(path, sizeof path, "%s/%s", bundle, LIB_NAME);
    void* lib = LIB_OPEN(path);
    check("plugin library loads", lib != nullptr);
    if (!lib) return;
    const LV2_Descriptor* (*get)(uint32_t) = nullptr;
    *(void**)&get = LIB_SYM(lib, "lv2_descriptor");
    check("exports lv2_descriptor", get != nullptr);
    if (!get) return;
    const LV2_Descriptor* d = get(0);
    check("descriptor has the plugin's URI", d && std::strcmp(d->URI, TB_URI) == 0);
    check("only one plugin in the library", get(1) == nullptr);
    if (!d) return;

    const double fs = 48000.0;
    LV2_Handle h = d->instantiate(d, fs, bundle, nullptr);
    check("instantiates at 48 kHz", h != nullptr);
    if (!h) return;
    const float defaults[PORT_COUNT] = {0, 0, 1, 0, 0, 0.5f, 0, 0.3f, 1, 0, 1, 0.5f, 1, 1.0f, 0, 1, 0, 0};
    float ctl[PORT_COUNT];
    std::memcpy(ctl, defaults, sizeof ctl);
    const int n = 512, blocks = 400;
    std::vector<float> in(n), out(n), all;
    for (int p = PORT_CHANNEL; p < PORT_COUNT; ++p) d->connect_port(h, uint32_t(p), &ctl[p]);
    d->connect_port(h, PORT_IN, in.data());
    d->connect_port(h, PORT_OUT, out.data());
    d->activate(h);
    double lampLo = 1, lampHi = 0;
    for (int b = 0; b < blocks; ++b) {
        if (b == 100) ctl[PORT_VIB] = 0.5f;           // vib-trem volume up
        if (b == 300) ctl[PORT_ENABLED] = 0.0f;       // then bypass
        for (int i = 0; i < n; ++i) in[i] = float(0.2 * std::sin(2 * kPi * 330.0 * (b * n + i) / fs));
        d->run(h, n);
        all.insert(all.end(), out.begin(), out.end());
        if (b > 20) {
            lampLo = std::fmin(lampLo, ctl[PORT_LAMP]);
            lampHi = std::fmax(lampHi, ctl[PORT_LAMP]);
        }
    }
    double peak = 0;
    bool ok = true;
    for (int i = 0; i < 300 * n; ++i) {
        ok = ok && std::isfinite(all[i]);
        peak = std::fmax(peak, std::fabs(all[i]));
    }
    std::snprintf(what, sizeof what, "default settings, 0.2 V at 330 Hz: output peak %.2f, all finite", peak);
    check(what, ok && peak > 0.05 && peak < 1.5);
    std::snprintf(what, sizeof what, "latency port reports %.0f samples", ctl[PORT_LATENCY]);
    check(what, ctl[PORT_LATENCY] == 16.0f);
    std::snprintf(what, sizeof what, "oscillator lamp moves (%.2f to %.2f)", lampLo, lampHi);
    check(what, lampHi - lampLo > 0.3);
    // Bypassed (after the 20 ms fade), the output is the input delayed by the latency.
    double worst = 0;
    const int lat = int(ctl[PORT_LATENCY]);
    for (int i = 310 * n; i < blocks * n; ++i) {
        const double x = 0.2 * std::sin(2 * kPi * 330.0 * (i - lat) / fs);
        worst = std::fmax(worst, std::fabs(all[i] - float(x)));
    }
    std::snprintf(what, sizeof what, "bypass passes the input through, delayed by the latency (error %.1e)", worst);
    check(what, worst < 1e-6);

    // In-place buffers, and a second instance at 96 kHz (no oversampling, no latency).
    d->cleanup(h);
    h = d->instantiate(d, 96000.0, bundle, nullptr);
    check("instantiates at 96 kHz", h != nullptr);
    if (h) {
        std::memcpy(ctl, defaults, sizeof ctl);
        for (int p = PORT_CHANNEL; p < PORT_COUNT; ++p) d->connect_port(h, uint32_t(p), &ctl[p]);
        d->connect_port(h, PORT_IN, in.data());
        d->connect_port(h, PORT_OUT, in.data());
        d->activate(h);
        bool fin = true;
        double pk = 0;
        for (int b = 0; b < 100; ++b) {
            for (int i = 0; i < n; ++i) in[i] = float(0.2 * std::sin(2 * kPi * 330.0 * (b * n + i) / 96000.0));
            d->run(h, n);
            for (int i = 0; i < n; ++i) {
                fin = fin && std::isfinite(in[i]);
                pk = std::fmax(pk, std::fabs(in[i]));
            }
        }
        std::snprintf(what, sizeof what, "in-place processing at 96 kHz: peak %.2f, latency %.0f", pk, ctl[PORT_LATENCY]);
        check(what, fin && pk > 0.05 && ctl[PORT_LATENCY] == 0.0f);
        d->cleanup(h);
    }
}

}  // namespace

int main(int argc, char** argv) {
    part1();
    if (argc > 1) part2(argv[1]);
    std::printf(failures ? "\n%d FAILED\n" : "\nAll passed\n", failures);
    return failures ? 1 : 0;
}
