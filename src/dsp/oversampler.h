// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Davidian. See LICENSE and NOTICE.md.
// Windowed-sinc oversampling around the nonlinear circuit.
#pragma once
#include <cmath>
#include <cstring>
#include <vector>

namespace tb30 {

class Oversampler {
public:
    void init(int factor) {
        factor_ = factor;
        const int taps = factor == 1 ? 1 : 16 * factor + 1;
        h_.resize(taps);
        const double pi = 3.14159265358979323846;
        double sum = 0.0;
        for (int i = 0; i < taps; ++i) {
            const double t = i - (taps - 1) * 0.5;
            const double a = taps > 1 ? 2.0 * pi * i / (taps - 1) : 0.0;
            const double w = 0.42 - 0.5 * std::cos(a) + 0.08 * std::cos(2.0 * a);
            const double fc = 0.92 / factor;  // Relative to base Nyquist.
            const double s = t == 0.0 ? fc : std::sin(pi * fc * t) / (pi * t);
            h_[i] = s * w;
            sum += h_[i];
        }
        for (double& v : h_) v /= sum;
        up_.assign(taps * 2, 0.0);
        dn_.assign(taps * 2, 0.0);
        upPos_ = dnPos_ = 0;
        taps_ = taps;
    }
    int factor() const { return factor_; }
    // Total delay of the up and down filters, in base-rate samples.
    int latency() const { return factor_ == 1 ? 0 : (taps_ - 1) / factor_; }

    inline double up(double x) { return filter(up_, upPos_, x) * factor_; }
    inline double down(double x) { return filter(dn_, dnPos_, x); }
    inline void pushDown(double x) { push(dn_, dnPos_, x); }
    inline double readDown() const { return read(dn_, dnPos_); }

private:
    inline void push(std::vector<double>& d, int& pos, double x) {
        if (--pos < 0) pos = taps_ - 1;
        d[pos] = d[pos + taps_] = x;
    }
    inline double read(const std::vector<double>& d, int pos) const {
        double s = 0.0;
        const double* p = &d[pos];
        for (int i = 0; i < taps_; ++i) s += h_[i] * p[i];
        return s;
    }
    inline double filter(std::vector<double>& d, int& pos, double x) {
        push(d, pos, x);
        return read(d, pos);
    }
    int factor_ = 1, taps_ = 1, upPos_ = 0, dnPos_ = 0;
    std::vector<double> h_, up_, dn_;
};

}  // namespace tb30
