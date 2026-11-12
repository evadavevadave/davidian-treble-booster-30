// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Davidian. See LICENSE and NOTICE.md.
// Speaker simulation for the "Speaker" switch. The schematic ends at the output transformer's
// speaker terminals, so this is the one part of the plugin that is not from it: a short
// cascade of filters shaped like an open-back 2x12 combo with a close microphone (low-end
// resonance, forward mids, presence peak, steep top roll-off). The shape is an estimate, not
// a measurement of any product. The circuit model itself already sees the speakers' electrical
// impedance; this adds their acoustic response. It runs at the host rate.
#pragma once
#include <cmath>

namespace tb30 {

class Cabinet {
public:
    void init(double fs) {
        fs_ = fs;
        n_ = 0;
        // Shared with the Davidian Ashgrove "2x12 Open" cabinet.
        design(HP, 95, 0.8, 0);
        design(PEAK, 200, 1.0, 1.5);
        design(PEAK, 1200, 1.0, 2);
        design(PEAK, 2800, 1.6, 3.5);
        design(LP, 5200, 0.54, 0);
        design(LP, 5200, 1.31, 0);
        // Unity power gain averaged over 100 Hz to 5 kHz, so the switch changes tone more than level.
        double sum = 0.0;
        const int pts = 64;
        for (int k = 0; k < pts; ++k) {
            const double m = magnitude(100.0 * std::pow(50.0, k / double(pts - 1)));
            sum += m * m;
        }
        norm_ = 1.0 / std::sqrt(sum / pts);
        reset();
    }

    void reset() {
        for (int i = 0; i < n_; ++i) bq_[i].z1 = bq_[i].z2 = 0.0;
    }

    double process(double x) {
        x *= norm_;
        for (int i = 0; i < n_; ++i) {
            Bq& b = bq_[i];
            const double y = b.b0 * x + b.z1;
            b.z1 = b.b1 * x - b.a1 * y + b.z2;
            b.z2 = b.b2 * x - b.a2 * y;
            x = y;
        }
        return x;
    }

    // Gain at frequency f (Hz), including the level match. Used by the tests.
    double response(double f) const { return norm_ * magnitude(f); }

private:
    enum Kind { HP, LP, PEAK };
    static const int kMax = 8;
    struct Bq {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    };

    // Audio EQ Cookbook (R. Bristow-Johnson) biquads, normalised so a0 = 1.
    void design(Kind kind, double f, double q, double db) {
        const double pi = 3.14159265358979323846;
        f = std::fmin(f, 0.45 * fs_);  // Keeps corners below Nyquist at low sample rates.
        const double w = 2.0 * pi * f / fs_, cw = std::cos(w), sw = std::sin(w);
        const double alpha = sw / (2.0 * q);
        double b0, b1, b2, a0, a1, a2;
        if (kind == PEAK) {
            const double A = std::pow(10.0, db / 40.0);
            b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A;
            a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A;
        } else if (kind == HP) {
            b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2;
            a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha;
        } else {
            b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = (1 - cw) / 2;
            a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha;
        }
        Bq& b = bq_[n_++];
        b.b0 = b0 / a0; b.b1 = b1 / a0; b.b2 = b2 / a0; b.a1 = a1 / a0; b.a2 = a2 / a0;
    }

    double magnitude(double f) const {
        const double pi = 3.14159265358979323846;
        const double w = 2.0 * pi * f / fs_;
        const double c1 = std::cos(w), s1 = std::sin(w), c2 = std::cos(2 * w), s2 = std::sin(2 * w);
        double m = 1.0;
        for (int i = 0; i < n_; ++i) {
            const Bq& b = bq_[i];
            const double nr = b.b0 + b.b1 * c1 + b.b2 * c2, ni = -(b.b1 * s1 + b.b2 * s2);
            const double dr = 1.0 + b.a1 * c1 + b.a2 * c2, di = -(b.a1 * s1 + b.a2 * s2);
            m *= std::sqrt((nr * nr + ni * ni) / (dr * dr + di * di));
        }
        return m;
    }

    double fs_ = 48000.0, norm_ = 1.0;
    int n_ = 0;
    Bq bq_[kMax];
};

}  // namespace tb30
