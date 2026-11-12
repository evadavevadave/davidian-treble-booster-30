// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Davidian. See LICENSE and NOTICE.md.
// Valve models: Norman Koren's triode and pentode equations, evaluated from lookup tables so
// they are cheap per sample. The schematic gives no valve curves or operating voltages, so
// every parameter here is a published fit or an estimate, noted on each one.
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace tb30 {

struct TriodeParams {
    double mu, ex, kg1, kp, kvb;
};

// Koren's published fits.
inline const TriodeParams& ecc83() {  // 12AX7: V1, V2, V7, V9
    static const TriodeParams p{100.0, 1.4, 1060.0, 600.0, 300.0};
    return p;
}
inline const TriodeParams& ecc82() {  // 12AU7: V8
    static const TriodeParams p{21.5, 1.3, 1180.0, 84.0, 300.0};
    return p;
}

// Anode current in amps for grid and anode voltages relative to the cathode.
inline double triodeIa(const TriodeParams& p, double vg, double vp) {
    if (vp <= 0.0) return 0.0;
    const double x = p.kp * (1.0 / p.mu + vg / std::sqrt(p.kvb + vp * vp));
    const double l = x > 30.0 ? x : std::log1p(std::exp(x));
    return 2.0 * std::pow(vp / p.kp * l, p.ex) / p.kg1;
}

// Triode anode current on a 2-D table with bilinear interpolation.
class TriodeTable {
public:
    static const TriodeTable& ecc83Table() {
        static const TriodeTable t(ecc83());
        return t;
    }
    static const TriodeTable& ecc82Table() {
        static const TriodeTable t(ecc82());
        return t;
    }
    // Plate current; dg and dp receive the partial derivatives.
    // Outside the table the current is held at its edge value, and the slope across that edge
    // is zero, so Newton's method is never told the current can change where it cannot.
    inline double eval(double vg, double vp, double& dg, double& dp) const {
        double fg = (vg - kVgMin) / kVgStep;
        double fp = vp / kVpStep;
        const double gmax = double(kNg - 1.001), pmax = double(kNp - 1.001);
        const bool gOut = fg < 0.0 || fg > gmax, pOut = fp < 0.0 || fp > pmax;
        fg = std::min(std::max(fg, 0.0), gmax);
        fp = std::min(std::max(fp, 0.0), pmax);
        const int ig = int(fg), ip = int(fp);
        const double tg = fg - ig, tp = fp - ip;
        const double* row = &data_[size_t(ig) * kNp + ip];
        const double a = row[0], b = row[1], c = row[kNp], d = row[kNp + 1];
        dg = gOut ? 0.0 : ((c - a) * (1 - tp) + (d - b) * tp) / kVgStep;
        dp = pOut ? 0.0 : ((b - a) * (1 - tg) + (d - c) * tg) / kVpStep;
        return (a * (1 - tg) + c * tg) * (1 - tp) + (b * (1 - tg) + d * tg) * tp;
    }

private:
    static constexpr double kVgMin = -80.0, kVgStep = 0.1, kVpStep = 2.0;
    static constexpr int kNg = 900, kNp = 301;  // Vg -80..+10 V, Vp 0..600 V.
    std::vector<double> data_;
    explicit TriodeTable(const TriodeParams& p) : data_(size_t(kNg) * kNp) {
        for (int g = 0; g < kNg; ++g)
            for (int q = 0; q < kNp; ++q) data_[size_t(g) * kNp + q] = triodeIa(p, kVgMin + g * kVgStep, q * kVpStep);
    }
};

struct PentodeParams {
    double mu, ex, kg1, kg2, kp, kvb, rgi;
};

// EL84 (V3-V6). Koren's form; mu, ex, kp and kvb are from his published EL84 fit, kg1 is set
// so the datasheet point (250 V anode and screen, -7.3 V grid) gives 48 mA, and kg2 so the
// screen takes the datasheet's 5.5 mA there.
inline const PentodeParams& el84() {
    static const PentodeParams p{19.5, 1.35, 650.0, 4700.0, 135.0, 12.0, 1000.0};
    return p;
}

inline double pentodeIa(const PentodeParams& p, double vg1, double vg2, double va) {
    if (vg2 < 1.0) vg2 = 1.0;
    const double x = p.kp * (1.0 / p.mu + vg1 / vg2);
    const double l = x > 30.0 ? x : std::log1p(std::exp(x));
    return 2.0 * std::pow(vg2 / p.kp * l, p.ex) / p.kg1 * std::atan(std::max(va, 0.0) / p.kvb);
}

// Pentode functions on 1-D tables: Ia = A(vg2) F(x) T(va).
class PentodeTable {
public:
    static const PentodeTable& el84Table() {
        static const PentodeTable t(el84());
        return t;
    }
    const PentodeParams& params() const { return p_; }
    inline double F(double x, double& d) const { return lookup(f_, (x - kXMin) / kXStep, kXStep, d); }
    inline double A(double v, double& d) const { return lookup(a_, v / kVStep, kVStep, d); }
    inline double T(double v, double& d) const { return lookup(t_, v / kVStep, kVStep, d); }

private:
    static constexpr double kXMin = -80.0, kXStep = 0.02, kVStep = 1.0;
    static constexpr int kNx = 8001, kNv = 1001;
    PentodeParams p_;
    std::vector<double> f_, a_, t_;
    // Held at the edge value outside the table, with zero slope there.
    static inline double lookup(const std::vector<double>& t, double f, double step, double& d) {
        const double lim = double(t.size()) - 1.001;
        const bool out = f < 0.0 || f > lim;
        f = f < 0.0 ? 0.0 : (f > lim ? lim : f);
        const int i = int(f);
        const double a = t[i], b = t[i + 1];
        d = out ? 0.0 : (b - a) / step;
        return a + (b - a) * (f - i);
    }
    explicit PentodeTable(const PentodeParams& p) : p_(p), f_(kNx), a_(kNv), t_(kNv) {
        for (int i = 0; i < kNx; ++i) {
            const double x = kXMin + i * kXStep;
            f_[i] = std::pow(x > 30.0 ? x : std::log1p(std::exp(x)), p.ex);
        }
        for (int i = 0; i < kNv; ++i) {
            a_[i] = 2.0 / p.kg1 * std::pow(i * kVStep / p.kp, p.ex);
            t_[i] = std::atan(i * kVStep / p.kvb);
        }
    }
};

// Grid conduction above zero volts: i = v^1.5 / r.
inline double gridCurrent(double v, double r, double& d) {
    if (v <= 0.0) {
        d = 1e-12;
        return v * 1e-12;
    }
    const double s = std::sqrt(v);
    d = 1.5 * s / r;
    return v * s / r;
}

}  // namespace tb30
