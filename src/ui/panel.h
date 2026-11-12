/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Davidian. See LICENSE and NOTICE.md. */
/* The GUI panel: layout, drawing and mouse handling, independent of any windowing system.
 *
 * The design is original, in the Davidian house style (graphite panel, cream lettering, amber
 * accents, red footswitch LED). It keeps the functional grouping of the circuit (three input
 * channels with their volumes, the vib-trem controls, the cut control) but uses its own name,
 * lettering and colours, and none of the original amplifier's branding or panel artwork. */
#ifndef UI_PANEL_H
#define UI_PANEL_H

#include <stdio.h>
#include <stdlib.h>

#include "../ports.h"
#include "canvas.h"
#include "font.h"
#include "style.h"

#define PANEL_W 960
#define PANEL_H 470

enum { W_KNOB, W_TRIM, W_SWITCH, W_FOOT };

typedef struct {
    int kind, port;
    float x, y, r;
    float min, max, def;
    int log;
    const char* label; /* on the panel */
    const char* name;  /* in the display */
    const char* lo;    /* scale end labels */
    const char* hi;
    const char* opts[3];
    int nopts;
} Widget;

static const Widget WIDGETS[] = {
    { W_SWITCH, PORT_CHANNEL, 44, 196, 0, 0, 2, 1, 0, "CHANNEL", "INPUT CHANNEL", 0, 0, { "BRIGHT", "NORMAL", "VIB-TREM" }, 3 },
    { W_SWITCH, PORT_JACK, 128, 196, 0, 0, 1, 0, 0, "JACK", "INPUT JACK", 0, 0, { "INPUT 1", "INPUT 2" }, 2 },
    { W_KNOB, PORT_BRIGHT, 266, 172, 30, 0, 1, 0, 0, "VOLUME", "BRIGHT VOLUME", "0", "10", {0}, 0 },
    { W_KNOB, PORT_NORMAL, 396, 172, 30, 0, 1, 0.5f, 0, "VOLUME", "NORMAL VOLUME", "0", "10", {0}, 0 },
    { W_KNOB, PORT_VIB, 532, 172, 30, 0, 1, 0, 0, "VOLUME", "VIB-TREM VOLUME", "0", "10", {0}, 0 },
    { W_SWITCH, PORT_SPEED, 612, 196, 0, 0, 2, 1, 0, "SPEED", "VIB-TREM SPEED", 0, 0, { "SLOW", "MEDIUM", "FAST" }, 3 },
    { W_SWITCH, PORT_MODE, 696, 196, 0, 0, 1, 0, 0, "MODE", "VIB-TREM MODE", 0, 0, { "VIBRATO", "TREMOLO" }, 2 },
    { W_KNOB, PORT_CUT, 865, 172, 30, 0, 1, 0.3f, 0, "CUT", "CUT", "0", "10", {0}, 0 },
    { W_TRIM, PORT_DEPTH, 71, 402, 17, 0, 1, 0.5f, 0, "DEPTH", "DEPTH PRESET", 0, 0, {0}, 0 },
    { W_TRIM, PORT_VOLTS, 151, 402, 17, 0.1f, 10.0f, 1.0f, 1, "INPUT V", "VOLTS AT 0 DBFS", 0, 0, {0}, 0 },
    { W_FOOT, PORT_FOOT, 300, 404, 25, 0, 1, 1, 0, "VIB-TREM", "VIB-TREM FOOTSWITCH", 0, 0, { "OFF", "ON" }, 2 },
    { W_SWITCH, PORT_SPEAKER, 790, 404, 0, 0, 1, 1, 0, "SPEAKER", "SPEAKER", 0, 0, { "OFF", "ON" }, 2 },
    { W_TRIM, PORT_OUTPUT, 896, 402, 17, -24.0f, 12.0f, 0.0f, 0, "OUTPUT", "OUTPUT LEVEL", 0, 0, {0}, 0 },
};
#define N_WIDGETS ((int)(sizeof WIDGETS / sizeof WIDGETS[0]))

/* The oscillator lamp, driven by the plugin's lamp output port. */
static const struct { int port; float x, y; } LAMPS[] = {
    { PORT_LAMP, 392, 396 },
};
#define N_LAMPS ((int)(sizeof LAMPS / sizeof LAMPS[0]))

#define KNOB_SPAN (0.75f * CV_PI) /* +/-135 degrees */

typedef void (*PanelWrite)(void* host, int port, float value);
/* Brackets each edit (a drag, a click, a wheel step) for hosts that record automation. */
typedef void (*PanelGesture)(void* host, int port, int begin);

typedef struct {
    uint32_t* px; /* what the window shows */
    uint32_t* bg; /* static artwork */
    Canvas c;
    float val[PORT_COUNT];
    int drag;      /* widget being dragged, or -1 */
    float drag_x, drag_y, drag_norm;
    int shown;     /* widget in the display, or -1 */
    int dirty;
    PanelWrite write;
    PanelGesture gesture; /* optional; set it after panel_init */
    void* host;
} Panel;

/* ---- Values ---- */

static float w_norm(const Widget* w, float v)
{
    float n = w->log ? logf(v / w->min) / logf(w->max / w->min) : (v - w->min) / (w->max - w->min);
    return n < 0.0f ? 0.0f : (n > 1.0f ? 1.0f : n);
}

static float w_value(const Widget* w, float n)
{
    n = n < 0.0f ? 0.0f : (n > 1.0f ? 1.0f : n);
    return w->log ? w->min * powf(w->max / w->min, n) : w->min + n * (w->max - w->min);
}

static int w_index(const Panel* p, const Widget* w)
{
    int i = (int)(p->val[w->port] + 0.5f);
    int n = w->kind == W_FOOT ? 2 : w->nopts;
    return i < 0 ? 0 : (i >= n ? n - 1 : i);
}

static void w_format(const Panel* p, const Widget* w, char* buf, size_t n)
{
    const float v = p->val[w->port];
    switch (w->kind) {
    case W_SWITCH:
    case W_FOOT:
        snprintf(buf, n, "%s", w->opts[w_index(p, w)]);
        break;
    default:
        if (w->port == PORT_VOLTS)
            snprintf(buf, n, "%.2f V", v);
        else if (w->port == PORT_OUTPUT)
            snprintf(buf, n, "%+.1f DB", v);
        else
            snprintf(buf, n, "%.1f", 10.0f * v);
    }
}

/* ---- Static artwork ---- */

static void draw_screw(Canvas* c, float x, float y)
{
    cv_circle(c, x + 1, y + 1.5f, 6, 0x000000, 0.5f);
    cv_shaded_circle(c, x, y, 6, 0xb8bbc2, 0x4a4d54, -0.4f, -0.5f, 1);
    cv_line(c, x - 4, y + 2, x + 4, y - 2, 1.6f, 0x2a2c30, 0.9f);
}

static void draw_section(Canvas* c, float x0, float y0, float x1, float y1, const char* title)
{
    const float tw = text_width(title, 9, 2.5f);
    const float cx = 0.5f * (x0 + x1);
    cv_rrect(c, x0, y0, x1, y1, 10, 0x000000, 0.18f);
    cv_rrect_line(c, x0, y0, x1, y1, 10, 1.4f, COL_LINE, 0.9f);
    /* Title sits in a gap in the top edge. */
    cv_rrect(c, cx - 0.5f * tw - 8, y0 - 6, cx + 0.5f * tw + 8, y0 + 6, 3,
             mix(COL_PANEL_TOP, COL_PANEL_BOT, y0 / PANEL_H), 1);
    text_center(c, cx, y0 - 4.5f, title, 9, 2.5f, 1.5f, COL_AMBER, 1);
}

static void draw_knob_scale(Canvas* c, const Widget* w)
{
    const int big = w->kind == W_KNOB;
    const int ticks = big ? 11 : 7;
    int i;
    for (i = 0; i < ticks; ++i) {
        const float a = -KNOB_SPAN + 2.0f * KNOB_SPAN * i / (ticks - 1);
        const float r0 = w->r + (big ? 9.0f : 6.0f), r1 = r0 + ((i % 5 == 0 || !big) ? 6.0f : 3.5f);
        cv_line(c, cv_ax(w->x, r0, a), cv_ay(w->y, r0, a), cv_ax(w->x, r1, a), cv_ay(w->y, r1, a),
                1.3f, COL_INK, 0.8f);
    }
    /* Dim track under the value arc. */
    cv_arc(c, w->x, w->y, w->r + 4.5f, 2.2f, -KNOB_SPAN, KNOB_SPAN, 0x000000, 0.55f);
    if (w->lo) {
        const float rr = w->r + 22.0f;
        text_center(c, cv_ax(w->x, rr, -KNOB_SPAN) - 2, cv_ay(w->y, rr, -KNOB_SPAN) - 3, w->lo, 7, 1, 1.1f, COL_DIM, 1);
        text_center(c, cv_ax(w->x, rr, KNOB_SPAN) + 2, cv_ay(w->y, rr, KNOB_SPAN) - 3, w->hi, 7, 1, 1.1f, COL_DIM, 1);
    }
    text_center(c, w->x, w->y + w->r + (big ? 20.0f : 14.0f), w->label, big ? 10.0f : 8.0f, 2.0f,
                big ? 1.7f : 1.4f, COL_INK, 1);
}

static void draw_switch_static(Canvas* c, const Widget* w)
{
    text_center(c, w->x + 14, w->y - 44, w->label, 8, 2.0f, 1.4f, COL_INK, 1);
    /* Mounting plate and nut. */
    cv_rrect(c, w->x - 9, w->y - 25, w->x + 9, w->y + 25, 4, 0x000000, 0.35f);
    cv_rrect_line(c, w->x - 9, w->y - 25, w->x + 9, w->y + 25, 4, 1, 0x71757d, 0.7f);
    cv_circle(c, w->x + 1, w->y + 2, 8.5f, 0x000000, 0.5f);
    cv_shaded_circle(c, w->x, w->y, 8.5f, COL_METAL_LIGHT, COL_METAL_DARK, -0.4f, -0.6f, 1);
    cv_circle(c, w->x, w->y, 5, 0x2d2f33, 1);
}

static void draw_foot_static(Canvas* c, const Widget* w)
{
    cv_circle(c, w->x + 2, w->y + 4, w->r + 9, 0x000000, 0.5f);
    cv_shaded_circle(c, w->x, w->y, w->r + 8, 0xa9adb5, 0x3c3f45, -0.5f, -0.7f, 1);
    cv_ring(c, w->x, w->y, w->r + 8, 1.2f, 0x202226, 0.8f);
    text_center(c, w->x, w->y + w->r + 18, w->label, 10, 2.5f, 1.7f, COL_INK, 1);
    /* LED bezel */
    cv_circle(c, w->x, w->y - w->r - 26, 7.5f, 0x0e0e10, 1);
    cv_ring(c, w->x, w->y - w->r - 26, 7.5f, 1.5f, 0x8a8e96, 0.8f);
}

static void draw_lamp_static(Canvas* c, float x, float y)
{
    cv_circle(c, x + 1, y + 2, 11, 0x000000, 0.5f);
    cv_shaded_circle(c, x, y, 11, 0xb9bcc3, 0x45484e, -0.4f, -0.6f, 1);
    cv_circle(c, x, y, 7.5f, 0x140c04, 1);
    text_center(c, x, y + 20, "OSCILLATOR", 7, 2.0f, 1.2f, COL_DIM, 1);
}

/* An engraved data plate: the valve line-up and what the model covers. */
static void draw_plate(Canvas* c, float x0, float y0, float x1, float y1)
{
    static const char* const lines[] = {
        "30 WATTS - FOUR EL84 - CLASS A",
        "12AX7 X4 - 12AU7 - GZ34 RECTIFIER",
        "CIRCUIT MODEL OF THE 1961 WORKS DRAWING",
    };
    const float cx = 0.5f * (x0 + x1);
    int i;
    cv_rrect(c, x0 + 1, y0 + 2, x1 + 1, y1 + 2, 6, 0x000000, 0.45f);
    cv_rrect_vgrad(c, x0, y0, x1, y1, 6, 0x3a3d43, 0x24262a, 1);
    cv_rrect_line(c, x0, y0, x1, y1, 6, 1.2f, 0x6d7179, 0.8f);
    cv_rrect_line(c, x0 + 5, y0 + 5, x1 - 5, y1 - 5, 3, 1.0f, COL_AMBER, 0.35f);
    text_center(c, cx, y0 + 18, "TREBLE BOOSTER 30", 9, 2.5f, 1.5f, COL_AMBER, 1);
    for (i = 0; i < 3; ++i)
        text_center(c, cx, y0 + 40 + 16.0f * i, lines[i], 7, 1.8f, 1.15f, COL_DIM, 1);
    draw_screw(c, x0 + 12, y0 + 12);
    draw_screw(c, x1 - 12, y0 + 12);
    draw_screw(c, x0 + 12, y1 - 12);
    draw_screw(c, x1 - 12, y1 - 12);
}

static void draw_background(Panel* p)
{
    Canvas c;
    int i, y;
    cv_init(&c, p->bg, PANEL_W, PANEL_H);
    for (y = 0; y < PANEL_H; ++y) {
        uint32_t col = mix(COL_PANEL_TOP, COL_PANEL_BOT, (float)y / PANEL_H);
        int x;
        for (x = 0; x < PANEL_W; ++x)
            p->bg[y * PANEL_W + x] = col;
    }
    cv_texture(&c, PANEL_TEXTURE);
    /* Bevelled edge */
    cv_rrect_line(&c, 1.5f, 1.5f, PANEL_W - 1.5f, PANEL_H - 1.5f, 6, 1.5f, 0x55595f, 0.8f);
    cv_rrect_line(&c, 3.5f, 3.5f, PANEL_W - 3.5f, PANEL_H - 3.5f, 5, 1.5f, 0x000000, 0.4f);

    /* Header */
    text_draw(&c, 30, 18, "DAVIDIAN TREBLE BOOSTER 30", 20, 3.2f, 2.8f, COL_INK, 1);
    text_draw(&c, 32, 46, "THREE-CHANNEL VALVE AMPLIFIER WITH VIBRATO AND TREMOLO", 7, 2.4f, 1.1f, COL_DIM, 1);
    cv_line(&c, 30, 60, 590, 60, 1.2f, COL_AMBER, 0.55f);
    /* Readout window */
    cv_rrect(&c, 616, 14, 944, 56, 6, 0x000000, 0.6f);
    cv_rrect(&c, 618, 16, 942, 54, 5, 0x0d0a06, 1);
    cv_rrect_line(&c, 616, 14, 944, 56, 6, 1.2f, 0x6d7179, 0.8f);

    draw_section(&c, 16, 76, 196, 306, "INPUT");
    draw_section(&c, 206, 76, 326, 306, "BRIGHT");
    draw_section(&c, 336, 76, 456, 306, "NORMAL");
    draw_section(&c, 466, 76, 776, 306, "VIB-TREM");
    draw_section(&c, 786, 76, 944, 306, "TONE");
    draw_section(&c, 16, 326, 206, 456, "PRESETS");
    draw_section(&c, 756, 326, 944, 456, "OUTPUT");
    draw_plate(&c, 476, 338, 736, 448);

    for (i = 0; i < N_WIDGETS; ++i) {
        const Widget* w = &WIDGETS[i];
        switch (w->kind) {
        case W_KNOB:
        case W_TRIM: draw_knob_scale(&c, w); break;
        case W_SWITCH: draw_switch_static(&c, w); break;
        case W_FOOT: draw_foot_static(&c, w); break;
        }
    }
    for (i = 0; i < N_LAMPS; ++i)
        draw_lamp_static(&c, LAMPS[i].x, LAMPS[i].y);

    draw_screw(&c, 14, 14);
    draw_screw(&c, PANEL_W - 14, 14);
    draw_screw(&c, 14, PANEL_H - 14);
    draw_screw(&c, PANEL_W - 14, PANEL_H - 14);
}

/* ---- Moving parts ---- */

static void draw_knob(Canvas* c, const Widget* w, float n, int active)
{
    const float r = w->r, x = w->x, y = w->y;
    const float ang = -KNOB_SPAN + 2.0f * KNOB_SPAN * n;
    const int knurls = w->kind == W_KNOB ? 36 : 20;
    int i;
    cv_arc(c, x, y, r + 4.5f, 2.2f, -KNOB_SPAN, ang, active ? 0xffd27f : COL_AMBER, 0.95f);
    cv_circle(c, x + 2, y + 4, r + 1, 0x000000, 0.5f);
    cv_shaded_circle(c, x, y, r, COL_KNOB_LIGHT, COL_KNOB_DARK, -0.45f, -0.65f, 1);
    for (i = 0; i < knurls; ++i) {
        const float a = ang + 2.0f * CV_PI * i / knurls;
        cv_line(c, cv_ax(x, r * 0.84f, a), cv_ay(y, r * 0.84f, a), cv_ax(x, r * 0.98f, a),
                cv_ay(y, r * 0.98f, a), w->kind == W_KNOB ? 1.6f : 1.2f, 0x000000, 0.55f);
    }
    cv_shaded_circle(c, x, y, r * 0.74f, 0x3c3e44, 0x18191c, -0.35f, -0.55f, 1);
    cv_ring(c, x, y, r * 0.74f, 1.0f, 0x6a6d74, 0.5f);
    cv_line(c, cv_ax(x, r * 0.2f, ang), cv_ay(y, r * 0.2f, ang), cv_ax(x, r * 0.93f, ang),
            cv_ay(y, r * 0.93f, ang), w->kind == W_KNOB ? 3.0f : 2.2f, COL_INK, 1);
}

static void draw_switch(Canvas* c, const Widget* w, int idx)
{
    const float x = w->x, y = w->y;
    float ey;
    int i;
    for (i = 0; i < w->nopts; ++i) {
        const float oy = w->nopts == 3 ? y - 20.0f + 20.0f * i : y - 20.0f + 40.0f * i;
        text_draw(c, x + 15, oy - 3.5f, w->opts[i], 7, 1.6f, 1.25f, i == idx ? COL_AMBER : COL_DIM, 1);
    }
    if (w->nopts == 3 && idx == 1) {
        /* Centre: the lever points at the viewer. */
        cv_circle(c, x + 1, y + 2, 5.5f, 0x000000, 0.5f);
        cv_shaded_circle(c, x, y, 5.5f, 0xf2f3f5, 0x8d9097, -0.3f, -0.4f, 1);
        return;
    }
    ey = (idx == 0) ? y - 19.0f : y + 19.0f;
    cv_line(c, x + 1.5f, y + 2, x + 1.5f, ey + 2, 6, 0x000000, 0.45f);
    cv_line(c, x, y, x, ey, 5.5f, 0xb6b9bf, 1);
    cv_line(c, x - 1.2f, y, x - 1.2f, ey, 1.6f, 0xf0f1f3, 0.7f);
    cv_shaded_circle(c, x, ey, 4.8f, 0xf4f5f7, 0x7c7f86, -0.35f, -0.45f, 1);
}

static void draw_foot(Canvas* c, const Widget* w, int on)
{
    const float lx = w->x, ly = w->y - w->r - 26;
    cv_shaded_circle(c, w->x, w->y, w->r, 0xe3e5e9, 0x6b6e75, -0.4f, -0.55f, 1);
    cv_ring(c, w->x, w->y, w->r * 0.62f, 1.2f, 0x8e9198, 0.6f);
    cv_ring(c, w->x, w->y, w->r, 1.0f, 0x34363b, 0.8f);
    if (on) {
        cv_glow(c, lx, ly, 5, 16, COL_RED, 0.35f);
        cv_shaded_circle(c, lx, ly, 5.5f, 0xffd0c8, COL_RED, -0.3f, -0.4f, 1);
    } else {
        cv_shaded_circle(c, lx, ly, 5.5f, 0x5a1f1b, 0x240a08, -0.3f, -0.4f, 1);
    }
}

static void draw_lamp(Canvas* c, float x, float y, float b)
{
    b = b < 0.0f ? 0.0f : (b > 1.0f ? 1.0f : b);
    cv_circle(c, x, y, 7.5f, mix(0x1e1206, COL_AMBER, b), 1);
    if (b > 0.02f) {
        cv_glow(c, x, y, 4, 20, COL_AMBER, 0.45f * b);
        cv_circle(c, x - 2, y - 2, 2.2f, 0xfff2d6, 0.8f * b);
    }
}

static void draw_display(Panel* p)
{
    char val[64];
    Canvas* c = &p->c;
    if (p->shown < 0) {
        text_draw(c, 632, 22, "TREBLE BOOSTER 30", 8, 2.2f, 1.3f, COL_AMBER_DK, 1);
        text_draw(c, 632, 36, "DRAG KNOBS - DOUBLE-CLICK RESETS", 7, 1.6f, 1.2f, 0x9c6a24, 1);
        return;
    }
    w_format(p, &WIDGETS[p->shown], val, sizeof val);
    text_draw(c, 632, 22, WIDGETS[p->shown].name, 8, 2.2f, 1.3f, 0xc58b33, 1);
    text_draw(c, 632, 35, val, 13, 2.0f, 2.0f, COL_AMBER, 1);
}

static void panel_draw(Panel* p)
{
    int i;
    memcpy(p->px, p->bg, sizeof(uint32_t) * PANEL_W * PANEL_H);
    for (i = 0; i < N_WIDGETS; ++i) {
        const Widget* w = &WIDGETS[i];
        switch (w->kind) {
        case W_KNOB:
        case W_TRIM: draw_knob(&p->c, w, w_norm(w, p->val[w->port]), p->drag == i); break;
        case W_SWITCH: draw_switch(&p->c, w, w_index(p, w)); break;
        case W_FOOT: draw_foot(&p->c, w, w_index(p, w)); break;
        }
    }
    for (i = 0; i < N_LAMPS; ++i)
        draw_lamp(&p->c, LAMPS[i].x, LAMPS[i].y, p->val[LAMPS[i].port]);
    draw_display(p);
    p->dirty = 0;
}

/* ---- Setup and host values ---- */

static int panel_init(Panel* p, PanelWrite write, void* host)
{
    int i;
    memset(p, 0, sizeof *p);
    p->px = (uint32_t*)malloc(sizeof(uint32_t) * PANEL_W * PANEL_H);
    p->bg = (uint32_t*)malloc(sizeof(uint32_t) * PANEL_W * PANEL_H);
    if (!p->px || !p->bg) {
        free(p->px);
        free(p->bg);
        return 0;
    }
    cv_init(&p->c, p->px, PANEL_W, PANEL_H);
    for (i = 0; i < N_WIDGETS; ++i)
        p->val[WIDGETS[i].port] = WIDGETS[i].def;
    p->val[PORT_ENABLED] = 1.0f;
    p->drag = -1;
    p->shown = -1;
    p->write = write;
    p->host = host;
    draw_background(p);
    panel_draw(p);
    return 1;
}

static void panel_free(Panel* p)
{
    free(p->px);
    free(p->bg);
    p->px = p->bg = NULL;
}

/* A value from the host. */
static void panel_set(Panel* p, int port, float v)
{
    if (port < 0 || port >= PORT_COUNT || !isfinite(v))
        return;
    if (p->val[port] != v) {
        p->val[port] = v;
        p->dirty = 1;
    }
}

/* ---- Mouse ---- */

static void set_value(Panel* p, int wi, float v)
{
    const Widget* w = &WIDGETS[wi];
    if (w->kind == W_SWITCH || w->kind == W_FOOT)
        v = floorf(v + 0.5f);
    if (v < w->min && !w->log) v = w->min;
    if (v > w->max) v = w->max;
    if (w->log && v < w->min) v = w->min;
    p->shown = wi;
    p->dirty = 1;
    if (p->val[w->port] != v) {
        p->val[w->port] = v;
        if (p->write)
            p->write(p->host, w->port, v);
    }
}

static void gesture(Panel* p, int wi, int begin)
{
    if (p->gesture)
        p->gesture(p->host, WIDGETS[wi].port, begin);
}

/* A whole edit in one go: a click on a switch, a double-click reset, a wheel step. */
static void set_once(Panel* p, int wi, float v)
{
    gesture(p, wi, 1);
    set_value(p, wi, v);
    gesture(p, wi, 0);
}

static int hit(float x, float y)
{
    int i;
    for (i = 0; i < N_WIDGETS; ++i) {
        const Widget* w = &WIDGETS[i];
        const float dx = x - w->x, dy = y - w->y;
        switch (w->kind) {
        case W_KNOB:
        case W_TRIM:
        case W_FOOT:
            if (dx * dx + dy * dy <= (w->r + 8) * (w->r + 8))
                return i;
            break;
        case W_SWITCH:
            if (dx >= -14 && dx <= 62 && dy >= -30 && dy <= 30)
                return i;
            break;
        }
    }
    return -1;
}

static int switch_pos_at(const Widget* w, float y)
{
    const float dy = y - w->y;
    if (w->nopts == 3)
        return dy < -8.0f ? 0 : (dy > 8.0f ? 2 : 1);
    return dy < 0.0f ? 0 : 1;
}

static void panel_mouse_down(Panel* p, float x, float y, int dbl)
{
    const int i = hit(x, y);
    const Widget* w;
    if (i < 0)
        return;
    w = &WIDGETS[i];
    switch (w->kind) {
    case W_KNOB:
    case W_TRIM:
        if (dbl) {
            set_once(p, i, w->def);
            return;
        }
        gesture(p, i, 1);
        p->drag = i;
        p->drag_y = y;
        p->drag_norm = w_norm(w, p->val[w->port]);
        p->shown = i;
        p->dirty = 1;
        break;
    case W_SWITCH:
        set_once(p, i, (float)switch_pos_at(w, y));
        break;
    case W_FOOT:
        set_once(p, i, w_index(p, w) ? 0.0f : 1.0f);
        break;
    }
}

static void panel_mouse_move(Panel* p, float x, float y, int fine)
{
    const Widget* w;
    (void)x;
    if (p->drag < 0)
        return;
    w = &WIDGETS[p->drag];
    {
        const float n = p->drag_norm + (p->drag_y - y) / (fine ? 900.0f : 180.0f);
        /* Fine mode needs a fresh reference so switching modes mid-drag doesn't jump. */
        set_value(p, p->drag, w_value(w, n));
        p->drag_norm = w_norm(w, p->val[w->port]);
        p->drag_y = y;
    }
}

static void panel_mouse_up(Panel* p)
{
    if (p->drag >= 0) {
        gesture(p, p->drag, 0);
        p->dirty = 1;
    }
    p->drag = -1;
}

static void panel_wheel(Panel* p, float x, float y, float steps, int fine)
{
    const int i = hit(x, y);
    const Widget* w;
    if (i < 0)
        return;
    w = &WIDGETS[i];
    if (w->kind == W_SWITCH) {
        int n = w_index(p, w) - (steps > 0 ? 1 : -1);
        set_once(p, i, (float)(n < 0 ? 0 : (n >= w->nopts ? w->nopts - 1 : n)));
    } else if (w->kind != W_FOOT) {
        set_once(p, i, w_value(w, w_norm(w, p->val[w->port]) + steps * (fine ? 0.004f : 0.025f)));
    }
}

#endif
