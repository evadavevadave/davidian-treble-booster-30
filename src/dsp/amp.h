// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Davidian. See LICENSE and NOTICE.md.
// The amplifier: three sections of the schematic, each solved as its own nonlinear circuit
// and joined at points where the following stage barely loads the one before.
//   PRE: the Bright and Normal inputs, V1, and the volume network up to the mixing node.
//   VIB: the Vib/Trem input, V7, the split networks, the V8 modulator and the Vib/Trem volume.
//   LFO: the oscillator V9 and the depth preset, stepped at about 12 kHz.
//   PWR: the phase splitter V2, the Cut control, the four EL84s, the output transformer and
//        speaker load, and the power supply with its sag.
#pragma once
#include "circuit.h"
#include "oversampler.h"

namespace tb30 {

enum Param {
    P_ROUTE,       // input jack: channel * 2 + jack (0 Bright 1, 1 Bright 2, 2 Normal 1, ...)
    P_VOL_BRIGHT,  // VR2, 0-1 travel
    P_VOL_NORMAL,  // VR1
    P_VOL_VIB,     // VR4
    P_CUT,         // VR3, 0-1 travel, 1 = most cut
    P_SPEED,       // SW1: 0 slow (1M), 1 medium (560K), 2 fast (330K)
    P_MODE,        // SW3: 0 vibrato, 1 tremolo
    P_FOOT,        // SW2: 0 effect stopped, 1 running
    P_DEPTH,       // VR5 depth preset, 0-1 travel
    P_COUNT
};

class Amp {
public:
    Amp();
    // Builds the circuits and finds the operating point. Not real-time safe.
    void init(double sampleRate);
    void setParam(int p, double value);
    double param(int p) const { return params_[p]; }
    // Applies pending control changes (rebuilds the affected matrices). Real-time safe.
    void applyControls();
    // Input is volts at the jack; output is volts across the speakers.
    void process(const double* in, double* out, int n);

    int latency() const { return os_.latency(); }
    // About 96 kHz inside: twice the host rate below 60 kHz, the host rate above.
    static int oversampling(double sampleRate) { return sampleRate < 60000.0 ? 2 : 1; }
    double internalRate() const { return fsInt_; }
    bool dcConverged() const { return dcOk_; }
    bool built() const { return built_; }
    long solverFailures() const;
    const Circuit& section(int i) const { return seg_[i]; }
    // The oscillator, as the voltage between V8's two grid returns (volts), for the panel lamp.
    double lfoVolts() const { return lfo_; }

    // Operating point and test access.
    enum Probe {
        T_RAIL_RES, T_RAIL_A, T_RAIL_B, T_RAIL_P, T_V1_PLATE, T_V1_CATHODE, T_PI_PLATE,
        T_PI_TAIL, T_EL84_CATHODE, T_EL84_PLATE, T_V7_PLATE, T_V8_SUPPLY, T_V9_PLATE, T_V9_CATHODE,
        T_COUNT
    };
    double probeVolts(int which) const;

private:
    void build();
    void buildPre();
    void buildVib();
    void buildLfo();
    void buildPwr();
    void solveOperatingPoint();
    void stepLfo();
    void jack(Circuit& c, int in, int grid, int channel);

    enum { PRE, VIB, LFO, PWR, NSEG };
    Circuit seg_[NSEG];

    // Inputs and outputs of each section.
    int preSig_ = 0, preRail_ = 0, preOutM_ = 0, preFeed_ = 0, prePlate_ = 0, preCathode_ = 0;
    int vibSig_ = 0, vibRail_ = 0, vibLfoA_ = 0, vibLfoB_ = 0, vibLfoLoad_ = 0, vibOutW_ = 0,
        vibFeed_ = 0, vibLfo_ = 0, vibP_ = 0, vibV7_ = 0, vibH_ = 0;
    int lfoRail_ = 0, lfoKick_ = 0, lfoOutA_ = 0, lfoOutB_ = 0, lfoFeed60_ = 0, lfoFeed62_ = 0, lfoV9_ = 0, lfoLamp_ = 0,
        lfoK9_ = 0;
    int pwrEmf_ = 0, pwrM_ = 0, pwrW_ = 0, pwrLoadA_ = 0, pwrLoadB_ = 0, pwrSpk_ = 0, pwrA_ = 0, pwrB_ = 0,
        pwrRes_ = 0, pwrK_ = 0, pwrPlate_ = 0, pwrPi_ = 0, pwrTail_ = 0;

    // Switch contact values. Open contacts are 1 G ohm.
    double route_[3][4][6];  // per channel: R1 to jack, R1 to ground, R2 to jack, R2 to ground
    double params_[P_COUNT];
    bool changed_[P_COUNT];

    double railA_ = 0.0, railB_ = 0.0, railP_ = 0.0, feedA_ = 0.0, feedB_ = 0.0, feedP_ = 0.0;
    double mixV_ = 0.0, vibV_ = 0.0, lfo_ = 0.0;
    // Oscillator outputs at the last two LFO steps, interpolated between them.
    double lfoA0_ = 0.0, lfoA1_ = 0.0, lfoB0_ = 0.0, lfoB1_ = 0.0;
    int lfoDiv_ = 8, lfoPhase_ = 0, kick_ = 0;
    bool dcOk_ = false, built_ = false;

    Oversampler os_;
    double fs_ = 48000.0, fsInt_ = 96000.0, T_ = 1.0 / 96000.0, Tlfo_ = 8.0 / 96000.0;
};

}  // namespace tb30
