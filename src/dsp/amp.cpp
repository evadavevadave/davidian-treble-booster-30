// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Davidian. See LICENSE and NOTICE.md.
// Netlists transcribed from the reference schematic (a works drawing dated 29-4-60 with its
// modifications to 8-5-61). Part numbers are the drawing's. Values the drawing does not give
// (valve curves, transformer and choke details, the speakers) are estimates, marked "est.".
#include "amp.h"

#include <algorithm>

namespace tb30 {

namespace {
const double k = 1e3, M = 1e6, u = 1e-6, n = 1e-9, p = 1e-12;
const double kOpen = 1e9;  // An open switch contact.

// SW1, the speed switch: the third resistor of the oscillator's phase-shift ladder.
const double kSpeed[3] = {1 * M /* R65 */, 560 * k /* R64 */, 330 * k /* R63 */};
// SW3 (trem on/off) and SW2 (footswitch): closed contacts are 1 ohm.
const double kTremSw[2] = {kOpen, 1.0};
const double kFootSw[2] = {1.0, kOpen};  // Closed stops the oscillator.

// Power supply (est.). The mains transformer gives 285-0-285 V into a GZ34; the drawing has no
// operating voltages, so the rectifier and reservoir are a source of 390 V behind 330 ohm,
// which puts the output stage's supply near 325 V at idle. The 100 Hz ripple is left out.
const double kSupplyEmf = 390.0, kSupplyR = 330.0;
const double kChokeL = 15.0, kChokeR = 150.0;  // CH1: "10-20 HY 100 MA"; resistance est.

// Output transformer T1 (est.): 4 k anode to anode, 15 ohm tap.
const double kRaa = 4000.0, kRload = 15.0;
}  // namespace

Amp::Amp() {
    for (int c = 0; c < 3; ++c)
        for (int r = 0; r < 6; ++r) {
            const bool sel = r / 2 == c, high = r % 2 == 0;
            // Input 1: both 68K resistors from the plug (34K). Input 2: one from the plug and
            // one to ground through the empty jack's switch (a 6 dB divider). An unused
            // channel has both to ground.
            route_[c][0][r] = sel ? 68 * k : kOpen;
            route_[c][1][r] = sel ? kOpen : 68 * k;
            route_[c][2][r] = sel && high ? 68 * k : kOpen;
            route_[c][3][r] = sel && high ? kOpen : 68 * k;
        }
    params_[P_ROUTE] = 2;
    params_[P_VOL_BRIGHT] = 0.0;
    params_[P_VOL_NORMAL] = 0.5;
    params_[P_VOL_VIB] = 0.0;
    params_[P_CUT] = 0.3;
    params_[P_SPEED] = 1;
    params_[P_MODE] = 0;
    params_[P_FOOT] = 1;
    params_[P_DEPTH] = 0.5;
    for (bool& b : changed_) b = false;
}

void Amp::init(double sr) {
    fs_ = sr;
    const int factor = oversampling(sr);
    os_.init(factor);
    fsInt_ = sr * factor;
    T_ = 1.0 / fsInt_;
    lfoDiv_ = std::max(1, int(fsInt_ / 12000.0 + 0.5));
    Tlfo_ = lfoDiv_ * T_;
    build();
    for (int s = 0; s < NSEG; ++s)
        for (int q = 0; q < P_COUNT; ++q) seg_[s].setParam(q, params_[q]);
    built_ = true;
    for (int s = 0; s < NSEG; ++s) built_ = built_ && !seg_[s].overflow();
    solveOperatingPoint();
}

void Amp::setParam(int q, double v) {
    if (q < 0 || q >= P_COUNT || params_[q] == v) return;
    params_[q] = v;
    changed_[q] = true;
}

void Amp::applyControls() {
    for (int q = 0; q < P_COUNT; ++q) {
        if (!changed_[q]) continue;
        changed_[q] = false;
        for (int s = 0; s < NSEG; ++s) seg_[s].setParam(q, params_[q]);
        if (q == P_FOOT && params_[q] >= 0.5) kick_ = 8;  // Releasing the short restarts it.
    }
    for (int s = 0; s < NSEG; ++s) seg_[s].update(s == LFO ? Tlfo_ : T_);
}

long Amp::solverFailures() const {
    long f = 0;
    for (int s = 0; s < NSEG; ++s) f += seg_[s].fails();
    return f;
}

void Amp::build() {
    for (int s = 0; s < NSEG; ++s) seg_[s].clear();
    buildPre();
    buildVib();
    buildLfo();
    buildPwr();
}

// One channel's pair of input jacks (J1-J6) and their 68K resistors, into a grid.
void Amp::jack(Circuit& c, int in, int grid, int ch) {
    c.switched(in, grid, P_ROUTE, route_[ch][0], 6);
    c.switched(GND, grid, P_ROUTE, route_[ch][1], 6);
    c.switched(in, grid, P_ROUTE, route_[ch][2], 6);
    c.switched(GND, grid, P_ROUTE, route_[ch][3], 6);
}

// Bright and Normal inputs, V1 (ECC83, one half per channel, sharing the cathode resistor), and
// the volume controls VR2 (Bright) and VR1 (Normal), mixed through 220K resistors into C5.
void Amp::buildPre() {
    Circuit& c = seg_[PRE];
    const TriodeTable& ax = TriodeTable::ecc83Table();
    const int aSrc = c.node(), n4 = c.node();
    preRail_ = c.input(300.0);
    c.V(aSrc, preRail_);
    c.R(aSrc, n4, 22 * k);  // R10
    c.C(n4, GND, 8 * u);    // C4
    const int in = c.node();
    preSig_ = c.input(0.0);
    c.V(in, preSig_);

    // V1a (grid pin 2, anode pin 1) is the Bright channel, V1b (pin 7, pin 6) the Normal one.
    const int gB = c.node(), gN = c.node(), pB = c.node(), pN = c.node(), kk = c.node();
    jack(c, in, gB, 0);  // J1, J2 with R1, R2
    jack(c, in, gN, 1);  // J3, J4 with R12, R13
    c.triode(gB, pB, kk, ax);
    c.triode(gN, pN, kk, ax);
    c.R(kk, GND, 1.5 * k);  // R4
    c.C(kk, GND, 25 * u);   // C1
    c.R(n4, pB, 220 * k);   // R5
    c.R(n4, pN, 220 * k);   // R6

    // Normal: C2 into VR1. Bright: only C3 (500 pF) into VR2, loaded by R74, which makes this
    // channel's bass roll off from about 2.8 kHz.
    const int nN = c.node(), w1 = c.node(), nB = c.node(), w2 = c.node(), m = c.node();
    c.C(pN, nN, 50 * n);                                      // C2
    c.pot(nN, w1, GND, 500 * k, Circuit::LOG, P_VOL_NORMAL);  // VR1
    c.R(w1, m, 220 * k);                                      // R7
    c.C(pB, nB, 500 * p);                                     // C3
    c.R(nB, GND, 150 * k);                                    // R74
    c.pot(nB, w2, GND, 500 * k, Circuit::LOG, P_VOL_BRIGHT);  // VR2
    c.R(w2, m, 220 * k);                                      // mixing resistor from VR2
    // Load of the phase splitter's input (C5 into R8), which the PWR section models in full.
    const int g = c.node();
    c.C(m, g, 50 * n);
    c.R(g, GND, 1 * M);

    preOutM_ = c.probe(m);
    preFeed_ = c.probe(aSrc, n4);
    prePlate_ = c.probe(pN);
    preCathode_ = c.probe(kk);
}

// Vib/Trem channel: V7a preamp, V7b split-load stage, the two RC networks (R54-R59, C22-C27)
// that feed V8's grids with differently phased copies, V8 (ECC82) as a long-tailed pair whose
// halves the oscillator crossfades, the four-section high-pass (C12-C15, R35-R37) and VR4.
void Amp::buildVib() {
    Circuit& c = seg_[VIB];
    const TriodeTable& ax = TriodeTable::ecc83Table();
    const TriodeTable& au = TriodeTable::ecc82Table();
    const int bSrc = c.node(), P = c.node();
    vibRail_ = c.input(300.0);
    c.V(bSrc, vibRail_);
    c.R(bSrc, P, 22 * k);  // R42
    c.C(P, GND, 32 * u);   // C19
    const int in = c.node();
    vibSig_ = c.input(0.0);
    c.V(in, vibSig_);

    // V7a
    const int g7a = c.node(), k7a = c.node(), p7a = c.node(), x38 = c.node();
    jack(c, in, g7a, 2);  // J5, J6 with R31, R32
    c.triode(g7a, p7a, k7a, ax);
    c.R(k7a, GND, 2.2 * k);  // R34
    c.C(k7a, GND, 25 * u);   // C17
    c.R(p7a, x38, 47 * k);   // R38
    c.R(x38, P, 100 * k);    // R39

    // V7b, split load: anode R48 to the supply, cathode R47 and R46 to ground, grid leak R45
    // returned to their junction.
    const int g7b = c.node(), k7b = c.node(), j7 = c.node(), p7b = c.node();
    c.C(x38, g7b, 10 * n);   // C18
    c.R(g7b, j7, 1 * M);     // R45
    c.triode(g7b, p7b, k7b, ax);
    c.R(k7b, j7, 2.2 * k);   // R47
    c.R(j7, GND, 22 * k);    // R46
    c.R(P, p7b, 22 * k);     // R48

    // Two compensated dividers across R46, each with an anti-phase feed from the anode.
    const int J1 = c.node(), J2 = c.node(), n26 = c.node(), n27 = c.node();
    c.R(GND, J1, 180 * k);  // R54
    c.C(GND, J1, 200 * p);  // C22
    c.R(J1, j7, 330 * k);   // R55
    c.C(J1, j7, 100 * p);   // C23
    c.R(GND, J2, 180 * k);  // R56
    c.C(GND, J2, 750 * p);  // C24
    c.R(J2, j7, 330 * k);   // R57
    c.C(J2, j7, 500 * p);   // C25
    c.C(p7b, n26, 500 * p);  // C26
    c.R(n26, J1, 68 * k);    // R58
    c.C(p7b, n27, 2 * n);    // C27
    c.R(n27, J2, 68 * k);    // R59
    // SW3 with C38: in the Trem position it shorts V8's second feed to ground.
    const int tr = c.node();
    c.C(J2, tr, 100 * n);  // C38
    c.switched(tr, GND, P_MODE, kTremSw, 2);

    // V8, the modulator.
    const int g8a = c.node(), g8b = c.node(), ja = c.node(), jb = c.node(), k8 = c.node(),
              p8a = c.node(), p8b = c.node(), H = c.node(), L = c.node();
    c.C(J1, g8a, 100 * n);  // C20
    c.C(J2, g8b, 100 * n);  // C21
    c.R(g8a, ja, 470 * k);  // R49
    c.R(ja, GND, 1 * M);    // R50
    c.R(g8b, jb, 470 * k);  // R52
    c.R(jb, GND, 1 * M);    // R53
    c.triode(g8a, p8a, k8, au);
    c.triode(g8b, p8b, k8, au);
    c.R(k8, GND, 1.5 * k);  // R51
    c.R(p8a, H, 10 * k);    // R43
    c.R(p8b, H, 10 * k);    // R44
    c.R(H, L, 47 * k);      // R41
    c.C(H, L, 5 * n);       // C41
    c.R(L, P, 15 * k);      // R40

    // High-pass ladder to VR4.
    const int l1 = c.node(), l2 = c.node(), l3 = c.node(), l4 = c.node(), W = c.node();
    c.C(L, l1, 5 * n);     // C12
    c.R(l1, GND, 1 * M);   // R35
    c.C(l1, l2, 5 * n);    // C13
    c.R(l2, GND, 1 * M);   // R36
    c.C(l2, l3, 5 * n);    // C14
    c.R(l3, GND, 1 * M);   // R37
    c.C(l3, l4, 5 * n);    // C15
    c.pot(l4, W, GND, 500 * k, Circuit::LOG, P_VOL_VIB);  // VR4
    // Load of the phase splitter's second input (C7 into R17), modelled in full in PWR.
    const int wl = c.node();
    c.C(W, wl, 50 * n);
    c.R(wl, GND, 1 * M);

    // The oscillator's outputs, from the LFO section: V9b's anode through C34 and its cathode
    // tap through C37 into V8's grid returns, and its supply current drawn from P.
    const int la = c.node(), lb = c.node();
    vibLfoA_ = c.input(0.0);
    c.V(la, vibLfoA_);
    vibLfoB_ = c.input(0.0);
    c.V(lb, vibLfoB_);
    c.C(la, ja, 100 * n);  // C34
    c.C(lb, jb, 100 * n);  // C37
    vibLfoLoad_ = c.input(0.0);
    c.I(P, vibLfoLoad_);

    vibOutW_ = c.probe(W);
    vibFeed_ = c.probe(bSrc, P);
    vibLfo_ = c.probe(ja, jb);
    vibP_ = c.probe(P);
    vibV7_ = c.probe(p7a);
    vibH_ = c.probe(H);
}

// The oscillator: V9a phase-shift oscillator with SW1 and SW2, then V9b, a split-load stage
// fed through R61/C32, C33 and the depth divider (R69 against VR5 + R73). It runs at a lower
// rate than the audio sections. Its anode and cathode outputs drive V8's grid returns in VIB,
// which are loaded here by R49 + R50 and R52 + R53 (V8's grids are near ground for the
// oscillator, through C20 and C21).
void Amp::buildLfo() {
    Circuit& c = seg_[LFO];
    const TriodeTable& ax = TriodeTable::ecc83Table();
    const int P = c.node();
    lfoRail_ = c.input(160.0);
    c.V(P, lfoRail_);

    // V9a phase-shift oscillator.
    const int g9a = c.node(), k9a = c.node(), p9a = c.node(), o1 = c.node(), o2 = c.node();
    c.R(P, p9a, 270 * k);  // R60
    c.triode(g9a, p9a, k9a, ax);
    c.R(k9a, GND, 3.9 * k);  // R68
    c.C(k9a, GND, 25 * u);   // C31
    c.C(p9a, o1, 10 * n);    // C28
    c.R(o1, GND, 1 * M);     // R66
    c.C(o1, o2, 10 * n);     // C29
    c.switched(o2, GND, P_SPEED, kSpeed, 3);  // SW1: R63, R64, R65
    c.C(o2, g9a, 10 * n);    // C30
    c.R(g9a, GND, 1 * M);    // R67
    c.switched(o1, GND, P_FOOT, kFootSw, 2);  // SW2
    lfoKick_ = c.input(0.0);
    c.I(g9a, lfoKick_);  // A start-up nudge, as switch-on noise would give.

    // V9b: low-pass R61/C32, coupling C33, depth divider R69 against VR5 + R73, then a
    // split-load stage whose anode (via C34) and cathode (via C37) feed V8's grid returns.
    const int E = c.node(), G0 = c.node(), D = c.node(), dr = c.node(), g9b = c.node(), k9b = c.node(),
              J9 = c.node(), p9b = c.node();
    c.R(p9a, E, 220 * k);    // R61
    c.C(E, GND, 100 * n);    // C32
    c.C(E, G0, 100 * n);     // C33
    c.R(G0, D, 150 * k);     // R69
    c.C(D, GND, 20 * n);     // C36
    c.rheostat(D, dr, 500 * k, Circuit::LOG, P_DEPTH);  // VR5
    c.R(dr, GND, 150 * k);   // R73
    c.C(D, g9b, 100 * n);    // C35
    c.R(g9b, J9, 560 * k);   // R71
    c.triode(g9b, p9b, k9b, ax);
    c.R(k9b, J9, 1.5 * k);   // R70
    c.R(J9, GND, 22 * k);    // R72
    c.R(P, p9b, 22 * k);     // R62
    const int ja = c.node(), jb = c.node();
    c.C(p9b, ja, 100 * n);   // C34
    c.R(ja, GND, 1 * M);     // R50
    c.R(ja, GND, 470 * k);   // R49
    c.C(J9, jb, 100 * n);    // C37
    c.R(jb, GND, 1 * M);     // R53
    c.R(jb, GND, 470 * k);   // R52

    lfoOutA_ = c.probe(p9b);
    lfoOutB_ = c.probe(J9);
    lfoFeed60_ = c.probe(P, p9a);
    lfoFeed62_ = c.probe(P, p9b);
    lfoV9_ = c.probe(p9a);
    lfoK9_ = c.probe(k9b);
    lfoLamp_ = c.probe(ja, jb);
}

// Phase splitter V2 (ECC83 long-tailed pair: the mixed channels on one grid, the Vib/Trem
// channel on the other), Cut control, output stage (V3-V6, EL84, two per side in parallel,
// cathode biased by R24 and C11), output transformer and speaker load, and the supply.
void Amp::buildPwr() {
    Circuit& c = seg_[PWR];
    const TriodeTable& ax = TriodeTable::ecc83Table();
    const PentodeTable& el = PentodeTable::el84Table();

    // Supply: rectifier and reservoir C39, choke CH1 and C40 (rail A), R11 and C8 (rail B).
    const int src = c.node(), res = c.node(), mid = c.node(), A = c.node(), B = c.node();
    pwrEmf_ = c.input(kSupplyEmf);
    c.V(src, pwrEmf_);
    c.R(src, res, kSupplyR);
    c.C(res, GND, 16 * u);  // C39
    c.R(res, mid, kChokeR);
    c.L(mid, A, kChokeL);   // CH1
    c.C(A, GND, 16 * u);    // C40
    pwrLoadA_ = c.input(0.0);
    c.I(A, pwrLoadA_);      // the PRE section, through R10
    c.R(A, B, 22 * k);      // R11
    c.C(B, GND, 8 * u);     // C8
    pwrLoadB_ = c.input(0.0);
    c.I(B, pwrLoadB_);      // the VIB section, through R42

    // Phase splitter inputs: C5 from the mixing node, C7 from VR4.
    const int mS = c.node(), wS = c.node(), g2 = c.node(), g7 = c.node(), T = c.node(), kp = c.node(),
              pa = c.node(), pb = c.node();
    pwrM_ = c.input(0.0);
    c.V(mS, pwrM_);
    pwrW_ = c.input(0.0);
    c.V(wS, pwrW_);
    c.C(mS, g2, 50 * n);   // C5
    c.R(g2, T, 1 * M);     // R8
    c.C(wS, g7, 50 * n);   // C7
    c.R(g7, T, 1 * M);     // R17
    c.triode(g2, pa, kp, ax);
    c.triode(g7, pb, kp, ax);
    c.R(kp, T, 1.2 * k);   // R16
    c.R(T, GND, 47 * k);   // R15
    c.R(B, pa, 100 * k);   // R18
    c.R(B, pb, 100 * k);   // R19

    // Coupling to the output grids, and the Cut control VR3 with C10 between them.
    const int GA = c.node(), GB = c.node(), cm = c.node();
    c.C(pa, GA, 150 * n);  // C6
    c.R(GA, GND, 220 * k); // R21
    c.C(pb, GB, 150 * n);  // C9
    c.R(GB, GND, 220 * k); // R20
    c.rheostat(GA, cm, 250 * k, Circuit::LOG, P_CUT, true);  // VR3
    c.C(cm, GB, 5 * n);    // C10

    // V3 + V5 and V4 + V6: grid stoppers R22/R27 and R23/R28 (1.5K each), screen stoppers
    // R25/R29 and R26/R30 (100 ohm each), shared cathode resistor R24 and C11.
    const int g1a = c.node(), g1b = c.node(), sa = c.node(), sb = c.node(), K = c.node(), va = c.node(),
              vb = c.node();
    c.R(GA, g1a, 750.0);
    c.R(GB, g1b, 750.0);
    c.R(A, sa, 50.0);
    c.R(A, sb, 50.0);
    c.pentode(g1a, sa, va, K, el, 2.0);
    c.pentode(g1b, sb, vb, K, el, 2.0);
    c.R(K, GND, 80.0);     // R24
    c.C(K, GND, 250 * u);  // C11

    // T1 (est.): magnetising inductance for an 18 Hz corner, leakage for 16 kHz, core loss.
    // The speakers' impedance is reflected to the primary; the output is the voltage across them.
    const double n2 = kRaa / kRload;
    const double lm = kRaa / (2.0 * 3.14159265358979 * 18.0);
    const double lk = kRaa / (2.0 * 3.14159265358979 * 16000.0);
    const int x = c.node(), s1 = c.node(), s2 = c.node(), spk = c.node();
    c.centreTap(va, vb, res);
    c.L(va, vb, lm);
    c.R(va, vb, kRaa * 20.0);
    c.L(va, x, lk);
    // Two 12-inch speakers making 15 ohm nominal (est.): 12 ohm voice coils with lossy
    // inductance, and an 85 Hz cone resonance rising to 60 ohm.
    c.R(x, s1, 12.0 * n2);
    c.L(s1, s2, 0.6e-3 * n2);
    c.R(s1, s2, 30.0 * n2);
    c.R(s2, vb, 48.0 * n2);
    c.L(s2, vb, 22.5e-3 * n2);
    c.C(s2, vb, 156 * u / n2);
    c.vcvs(spk, x, vb, 1.0 / std::sqrt(n2));

    pwrSpk_ = c.probe(spk);
    pwrA_ = c.probe(A);
    pwrB_ = c.probe(B);
    pwrRes_ = c.probe(res);
    pwrK_ = c.probe(K);
    pwrPlate_ = c.probe(va);
    pwrPi_ = c.probe(pa);
    pwrTail_ = c.probe(kp);
}

// Finds the DC operating point, passing supply voltages and currents between the sections
// until they agree.
void Amp::solveOperatingPoint() {
    Circuit& pre = seg_[PRE];
    Circuit& vib = seg_[VIB];
    Circuit& lfo = seg_[LFO];
    Circuit& pwr = seg_[PWR];
    double a = 300.0, b = 290.0, pv = 160.0, la = 0.0, lb = 0.0, lp = 0.0, oa = 0.0, ob = 0.0;
    bool ok = true;
    for (int pass = 0; pass < 8; ++pass) {
        pwr.setInput(pwrLoadA_, la);
        pwr.setInput(pwrLoadB_, lb);
        pwr.setInput(pwrM_, 0.0);
        pwr.setInput(pwrW_, 0.0);
        ok = pwr.solveDC(T_);
        a = pwr.output(pwrA_);
        b = pwr.output(pwrB_);
        pre.setInput(preRail_, a);
        pre.setInput(preSig_, 0.0);
        ok = pre.solveDC(T_) && ok;
        la = pre.output(preFeed_) / (22 * k);
        lfo.setInput(lfoRail_, pv);
        lfo.setInput(lfoKick_, 0.0);
        ok = lfo.solveDC(Tlfo_) && ok;
        lp = lfo.output(lfoFeed60_) / (270 * k) + lfo.output(lfoFeed62_) / (22 * k);
        oa = lfo.output(lfoOutA_);
        ob = lfo.output(lfoOutB_);
        vib.setInput(vibRail_, b);
        vib.setInput(vibSig_, 0.0);
        vib.setInput(vibLfoA_, oa);
        vib.setInput(vibLfoB_, ob);
        vib.setInput(vibLfoLoad_, lp);
        ok = vib.solveDC(T_) && ok;
        lb = vib.output(vibFeed_) / (22 * k);
        pv = vib.output(vibP_);
    }
    railA_ = a;
    railB_ = b;
    railP_ = pv;
    feedA_ = la;
    feedB_ = lb;
    feedP_ = lp;
    lfoA0_ = lfoA1_ = oa;
    lfoB0_ = lfoB1_ = ob;
    lfoPhase_ = 0;
    mixV_ = pre.output(preOutM_);
    vibV_ = vib.output(vibOutW_);
    dcOk_ = ok;
    kick_ = 8;
}

double Amp::probeVolts(int t) const {
    switch (t) {
        case T_RAIL_RES: return seg_[PWR].output(pwrRes_);
        case T_RAIL_A: return seg_[PWR].output(pwrA_);
        case T_RAIL_B: return seg_[PWR].output(pwrB_);
        case T_RAIL_P: return seg_[VIB].output(vibP_);
        case T_V1_PLATE: return seg_[PRE].output(prePlate_);
        case T_V1_CATHODE: return seg_[PRE].output(preCathode_);
        case T_PI_PLATE: return seg_[PWR].output(pwrPi_);
        case T_PI_TAIL: return seg_[PWR].output(pwrTail_);
        case T_EL84_CATHODE: return seg_[PWR].output(pwrK_);
        case T_EL84_PLATE: return seg_[PWR].output(pwrPlate_);
        case T_V7_PLATE: return seg_[VIB].output(vibV7_);
        case T_V8_SUPPLY: return seg_[VIB].output(vibH_);
        case T_V9_PLATE: return seg_[LFO].output(lfoV9_);
        case T_V9_CATHODE: return seg_[LFO].output(lfoK9_);
        default: return 0.0;
    }
}

// One step of the oscillator section, fed from the VIB section's supply node P.
void Amp::stepLfo() {
    Circuit& lfo = seg_[LFO];
    lfo.setInput(lfoRail_, railP_);
    lfo.setInput(lfoKick_, kick_ > 0 ? 2e-8 : 0.0);
    if (kick_ > 0) --kick_;
    lfo.step();
    lfoA0_ = lfoA1_;
    lfoB0_ = lfoB1_;
    lfoA1_ = lfo.output(lfoOutA_);
    lfoB1_ = lfo.output(lfoOutB_);
    feedP_ = lfo.output(lfoFeed60_) / (270 * k) + lfo.output(lfoFeed62_) / (22 * k);
}

void Amp::process(const double* in, double* out, int count) {
    Circuit& pre = seg_[PRE];
    Circuit& vib = seg_[VIB];
    Circuit& pwr = seg_[PWR];
    const int os = os_.factor();
    const double step = 1.0 / lfoDiv_;
    // With VR4 fully down its wiper is 1 ohm from ground, so nothing from the Vib/Trem
    // section reaches the phase splitter (under -110 dB) and it is not stepped. Its supply
    // current holds its last value; the oscillator keeps running.
    const bool vibLive = params_[P_VOL_VIB] > 0.0;
    if (!vibLive) vibV_ = 0.0;
    for (int i = 0; i < count; ++i) {
        for (int j = 0; j < os; ++j) {
            const double x = os == 1 ? in[i] : os_.up(j == 0 ? in[i] : 0.0);
            pre.setInput(preSig_, x);
            pre.setInput(preRail_, railA_);
            pre.step();
            mixV_ = pre.output(preOutM_);
            feedA_ = pre.output(preFeed_) / (22 * k);

            // The oscillator runs one step ahead and is interpolated, so it never jumps.
            if (lfoPhase_ == 0) stepLfo();
            const double t = (lfoPhase_ + 1) * step;
            if (++lfoPhase_ >= lfoDiv_) lfoPhase_ = 0;
            if (vibLive) {
                vib.setInput(vibSig_, x);
                vib.setInput(vibRail_, railB_);
                vib.setInput(vibLfoA_, lfoA0_ + (lfoA1_ - lfoA0_) * t);
                vib.setInput(vibLfoB_, lfoB0_ + (lfoB1_ - lfoB0_) * t);
                vib.setInput(vibLfoLoad_, feedP_);
                vib.step();
                vibV_ = vib.output(vibOutW_);
                feedB_ = vib.output(vibFeed_) / (22 * k);
                railP_ = vib.output(vibP_);
            }

            pwr.setInput(pwrM_, mixV_);
            pwr.setInput(pwrW_, vibV_);
            pwr.setInput(pwrLoadA_, feedA_);
            pwr.setInput(pwrLoadB_, feedB_);
            pwr.step();
            railA_ = pwr.output(pwrA_);
            railB_ = pwr.output(pwrB_);
            const double y = pwr.output(pwrSpk_);
            if (os == 1) out[i] = y;
            else os_.pushDown(y);
        }
        if (os != 1) out[i] = os_.readDown();
    }
    lfo_ = seg_[LFO].output(lfoLamp_);
}

}  // namespace tb30
