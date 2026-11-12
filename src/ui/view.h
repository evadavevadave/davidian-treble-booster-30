/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Davidian. See LICENSE and NOTICE.md. */
/* The Treble Booster 30 panel in a native window: a Win32 child window, a Cocoa NSView or an X11 window.
 * The LV2 UI, the VST3 editor and the Audio Unit view all use this, each with its own host
 * callbacks. view.c is compiled as Objective-C on macOS. */
#ifndef TB_VIEW_H_INCLUDED
#define TB_VIEW_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#define TB_VIEW_W 960
#define TB_VIEW_H 470

typedef struct TbView TbView;

typedef struct {
    void* host;
    /* A plain value (as in ports.h) the user set on the panel. */
    void (*write)(void* host, int port, float value);
    /* Optional: brackets each edit, begin = 1 then begin = 0. */
    void (*gesture)(void* host, int port, int begin);
    /* Optional: called about 30 times a second on the UI thread, before the panel redraws. */
    void (*tick)(void* host);
} TbViewHost;

/* Opens the panel inside parent (an HWND, NSView* or X11 Window, cast to void*), or as its own
 * view or window when parent is NULL. Returns NULL on failure. */
TbView* tb_view_open(void* parent, const TbViewHost* host);
/* The HWND, NSView* or X11 Window. */
void* tb_view_widget(TbView* v);
/* A value from the host; it shows on the next redraw. */
void tb_view_set(TbView* v, int port, float value);
/* X11 has no event loop of its own here, so the host's idle call pumps it. Returns 0. */
int tb_view_idle(TbView* v);
void tb_view_close(TbView* v);

#ifdef __cplusplus
}
#endif

#endif
