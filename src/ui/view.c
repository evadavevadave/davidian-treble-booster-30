/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Davidian. See LICENSE and NOTICE.md. */
/* Native window layers for the panel: Win32, Cocoa (built as Objective-C without ARC) and X11.
 * Everything drawn and every mouse rule lives in panel.h, which knows nothing about windows. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "panel.h"
#include "view.h"

#if PANEL_W != TB_VIEW_W || PANEL_H != TB_VIEW_H
#    error "view.h and panel.h disagree on the panel size"
#endif

#define DOUBLE_CLICK_MS 350

static void view_redraw(TbView* v);

static void on_write(void* host, int port, float value)
{
    const TbViewHost* h = (const TbViewHost*)host;
    if (h->write)
        h->write(h->host, port, value);
}

static void on_gesture(void* host, int port, int begin)
{
    const TbViewHost* h = (const TbViewHost*)host;
    if (h->gesture)
        h->gesture(h->host, port, begin);
}

static void tick(const TbViewHost* h)
{
    if (h->tick)
        h->tick(h->host);
}

/* ---- Win32 ---- */
#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#    include <windowsx.h>

/* Window classes belong to the module that registers them, so the LV2 and VST3 libraries can
 * share this name in one host. */
#    ifndef TB_WINDOW_CLASS
#        define TB_WINDOW_CLASS "DavidianTrebleBooster30Panel"
#    endif

struct TbView {
    Panel panel;
    TbViewHost host;
    HWND hwnd;
    BITMAPINFO bmi;
};

static int class_users;

static HINSTANCE this_module(void)
{
    HMODULE m = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCSTR)(void*)&this_module, &m);
    return (HINSTANCE)m;
}

static void blit(TbView* v, HDC dc)
{
    if (v->panel.dirty)
        panel_draw(&v->panel);
    SetDIBitsToDevice(dc, 0, 0, PANEL_W, PANEL_H, 0, 0, 0, PANEL_H, v->panel.px, &v->bmi, DIB_RGB_COLORS);
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    TbView* v = (TbView*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    if (!v)
        return DefWindowProcA(hwnd, msg, wp, lp);
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        blit(v, dc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_PRINTCLIENT:
        blit(v, (HDC)wp);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        SetCapture(hwnd);
        panel_mouse_down(&v->panel, (float)GET_X_LPARAM(lp), (float)GET_Y_LPARAM(lp), msg == WM_LBUTTONDBLCLK);
        view_redraw(v);
        return 0;
    case WM_MOUSEMOVE:
        panel_mouse_move(&v->panel, (float)GET_X_LPARAM(lp), (float)GET_Y_LPARAM(lp), (wp & MK_SHIFT) != 0);
        view_redraw(v);
        return 0;
    case WM_LBUTTONUP:
    case WM_CAPTURECHANGED:
        if (msg == WM_LBUTTONUP)
            ReleaseCapture();
        panel_mouse_up(&v->panel);
        view_redraw(v);
        return 0;
    case WM_MOUSEWHEEL: {
        POINT pt;
        pt.x = GET_X_LPARAM(lp);
        pt.y = GET_Y_LPARAM(lp);
        ScreenToClient(hwnd, &pt);
        panel_wheel(&v->panel, (float)pt.x, (float)pt.y, GET_WHEEL_DELTA_WPARAM(wp) / (float)WHEEL_DELTA,
                    (GET_KEYSTATE_WPARAM(wp) & MK_SHIFT) != 0);
        view_redraw(v);
        return 0;
    }
    case WM_MOUSEACTIVATE:
        SetFocus(hwnd); /* so the wheel reaches this window */
        return MA_ACTIVATE;
    case WM_TIMER:
        tick(&v->host);
        view_redraw(v);
        return 0;
    default:
        break;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static int window_open(TbView* v, void* parent)
{
    WNDCLASSEXA wc;
    HINSTANCE inst = this_module();
    if (class_users++ == 0) {
        memset(&wc, 0, sizeof wc);
        wc.cbSize = sizeof wc;
        wc.style = CS_DBLCLKS;
        wc.lpfnWndProc = wndproc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = TB_WINDOW_CLASS;
        if (!RegisterClassExA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            --class_users;
            return 0;
        }
    }
    memset(&v->bmi, 0, sizeof v->bmi);
    v->bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    v->bmi.bmiHeader.biWidth = PANEL_W;
    v->bmi.bmiHeader.biHeight = -PANEL_H; /* top-down */
    v->bmi.bmiHeader.biPlanes = 1;
    v->bmi.bmiHeader.biBitCount = 32;
    v->bmi.bmiHeader.biCompression = BI_RGB;
    v->hwnd = CreateWindowExA(0, TB_WINDOW_CLASS, "Treble Booster 30",
                              (parent ? WS_CHILD : WS_OVERLAPPEDWINDOW) | WS_VISIBLE | WS_CLIPSIBLINGS,
                              0, 0, PANEL_W, PANEL_H, (HWND)parent, NULL, inst, NULL);
    if (!v->hwnd) {
        if (--class_users == 0)
            UnregisterClassA(TB_WINDOW_CLASS, inst);
        return 0;
    }
    SetWindowLongPtrA(v->hwnd, GWLP_USERDATA, (LONG_PTR)v);
    SetTimer(v->hwnd, 1, 33, NULL); /* picks up host updates such as the oscillator lamp */
    return 1;
}

static void window_close(TbView* v)
{
    if (v->hwnd) {
        KillTimer(v->hwnd, 1);
        SetWindowLongPtrA(v->hwnd, GWLP_USERDATA, 0);
        DestroyWindow(v->hwnd);
        v->hwnd = NULL;
        if (--class_users == 0)
            UnregisterClassA(TB_WINDOW_CLASS, this_module());
    }
}

void* tb_view_widget(TbView* v) { return (void*)v->hwnd; }

static void view_redraw(TbView* v)
{
    if (v->panel.dirty && v->hwnd)
        InvalidateRect(v->hwnd, NULL, FALSE);
}

/* The window's own timer drives redraws. */
int tb_view_idle(TbView* v) { view_redraw(v); return 0; }

/* ---- X11 ---- */
#elif defined(__unix__) && !defined(__APPLE__)
#    include <X11/Xlib.h>
#    include <X11/Xutil.h>

struct TbView {
    Panel panel;
    TbViewHost host;
    Display* dpy;
    Window win;
    GC gc;
    XImage* img;
    Time last_click;
    int last_x, last_y;
};

static int window_open(TbView* v, void* parent)
{
    XSetWindowAttributes a;
    Visual* vis;
    int screen, depth;
    Window root;
    v->dpy = XOpenDisplay(NULL);
    if (!v->dpy)
        return 0;
    screen = DefaultScreen(v->dpy);
    vis = DefaultVisual(v->dpy, screen);
    depth = DefaultDepth(v->dpy, screen);
    if (depth < 24 || vis->red_mask != 0xff0000 || vis->blue_mask != 0xff) {
        XCloseDisplay(v->dpy); /* this simple layer needs a standard 24/32-bit RGB visual */
        v->dpy = NULL;
        return 0;
    }
    root = parent ? (Window)(uintptr_t)parent : RootWindow(v->dpy, screen);
    memset(&a, 0, sizeof a);
    a.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask;
    a.background_pixel = 0;
    v->win = XCreateWindow(v->dpy, root, 0, 0, PANEL_W, PANEL_H, 0, depth, InputOutput, vis,
                           CWEventMask | CWBackPixel, &a);
    v->gc = XCreateGC(v->dpy, v->win, 0, NULL);
    v->img = XCreateImage(v->dpy, vis, (unsigned)depth, ZPixmap, 0, (char*)v->panel.px,
                          PANEL_W, PANEL_H, 32, 0);
    if (!v->img) {
        XFreeGC(v->dpy, v->gc);
        XDestroyWindow(v->dpy, v->win);
        XCloseDisplay(v->dpy);
        v->dpy = NULL;
        return 0;
    }
    XMapRaised(v->dpy, v->win);
    XFlush(v->dpy);
    return 1;
}

static void window_close(TbView* v)
{
    if (!v->dpy)
        return;
    v->img->data = NULL; /* the panel owns the pixels */
    XDestroyImage(v->img);
    XFreeGC(v->dpy, v->gc);
    XDestroyWindow(v->dpy, v->win);
    XCloseDisplay(v->dpy);
    v->dpy = NULL;
}

void* tb_view_widget(TbView* v) { return (void*)(uintptr_t)v->win; }

static void blit(TbView* v)
{
    XPutImage(v->dpy, v->win, v->gc, v->img, 0, 0, 0, 0, PANEL_W, PANEL_H);
    XFlush(v->dpy);
}

static void view_redraw(TbView* v)
{
    if (v->panel.dirty && v->dpy) {
        panel_draw(&v->panel);
        blit(v);
    }
}

int tb_view_idle(TbView* v)
{
    if (!v->dpy)
        return 0;
    while (XPending(v->dpy)) {
        XEvent e;
        XNextEvent(v->dpy, &e);
        switch (e.type) {
        case Expose:
            if (e.xexpose.count == 0) {
                if (v->panel.dirty)
                    panel_draw(&v->panel);
                blit(v);
            }
            break;
        case ButtonPress:
            if (e.xbutton.button == 1) {
                const int dbl = e.xbutton.time - v->last_click < DOUBLE_CLICK_MS
                                && abs(e.xbutton.x - v->last_x) < 4 && abs(e.xbutton.y - v->last_y) < 4;
                panel_mouse_down(&v->panel, (float)e.xbutton.x, (float)e.xbutton.y, dbl);
                v->last_click = dbl ? 0 : e.xbutton.time;
                v->last_x = e.xbutton.x;
                v->last_y = e.xbutton.y;
            } else if (e.xbutton.button == 4 || e.xbutton.button == 5) {
                panel_wheel(&v->panel, (float)e.xbutton.x, (float)e.xbutton.y,
                            e.xbutton.button == 4 ? 1.0f : -1.0f, (e.xbutton.state & ShiftMask) != 0);
            }
            break;
        case MotionNotify:
            panel_mouse_move(&v->panel, (float)e.xmotion.x, (float)e.xmotion.y,
                             (e.xmotion.state & ShiftMask) != 0);
            break;
        case ButtonRelease:
            if (e.xbutton.button == 1)
                panel_mouse_up(&v->panel);
            break;
        default:
            break;
        }
    }
    tick(&v->host);
    view_redraw(v);
    return 0;
}

/* ---- Cocoa (macOS) ---- */
#elif defined(__APPLE__)
#    import <Cocoa/Cocoa.h>

/* Objective-C class names are global to the process, so each library that contains this
 * view gives it its own name (the LV2 UI and the Audio Unit can be loaded together). */
#    ifndef TB_VIEW_CLASS
#        define TB_VIEW_CLASS DavidianTrebleBooster30View
#    endif

struct TbView {
    Panel panel;
    TbViewHost host;
    void* view; /* TB_VIEW_CLASS, retained */
};

@interface TB_VIEW_CLASS : NSView {
@public
    TbView* ui;
    NSTimer* timer;
}
@end

@implementation TB_VIEW_CLASS

- (BOOL)isFlipped { return YES; } /* y grows downwards, as in panel.h */
- (BOOL)acceptsFirstMouse:(NSEvent*)e { (void)e; return YES; }
- (BOOL)acceptsFirstResponder { return YES; }

- (void)drawRect:(NSRect)dirty
{
    CGContextRef ctx;
    CGColorSpaceRef cs;
    CGDataProviderRef dp;
    CGImageRef img;
    (void)dirty;
    if (!ui)
        return;
    if (ui->panel.dirty)
        panel_draw(&ui->panel);
    ctx = [[NSGraphicsContext currentContext] CGContext];
    cs = CGColorSpaceCreateDeviceRGB();
    dp = CGDataProviderCreateWithData(NULL, ui->panel.px, (size_t)PANEL_W * PANEL_H * 4, NULL);
    /* 0x00RRGGBB words: little-endian 32-bit with the unused byte first. */
    img = CGImageCreate(PANEL_W, PANEL_H, 8, 32, (size_t)PANEL_W * 4, cs,
                        kCGBitmapByteOrder32Little | (CGBitmapInfo)kCGImageAlphaNoneSkipFirst, dp,
                        NULL, false, kCGRenderingIntentDefault);
    if (img) {
        /* The view is flipped, so flip back for the image, which Quartz draws bottom-up. */
        CGContextSaveGState(ctx);
        CGContextTranslateCTM(ctx, 0, PANEL_H);
        CGContextScaleCTM(ctx, 1, -1);
        CGContextSetInterpolationQuality(ctx, kCGInterpolationNone);
        CGContextDrawImage(ctx, CGRectMake(0, 0, PANEL_W, PANEL_H), img);
        CGContextRestoreGState(ctx);
        CGImageRelease(img);
    }
    CGDataProviderRelease(dp);
    CGColorSpaceRelease(cs);
}

- (NSPoint)panelPoint:(NSEvent*)e
{
    return [self convertPoint:[e locationInWindow] fromView:nil];
}

static int shift_held(NSEvent* e) { return ([e modifierFlags] & NSEventModifierFlagShift) != 0; }

- (void)mouseDown:(NSEvent*)e
{
    const NSPoint p = [self panelPoint:e];
    if (!ui) return;
    panel_mouse_down(&ui->panel, (float)p.x, (float)p.y, [e clickCount] == 2);
    view_redraw(ui);
}

- (void)mouseDragged:(NSEvent*)e
{
    const NSPoint p = [self panelPoint:e];
    if (!ui) return;
    panel_mouse_move(&ui->panel, (float)p.x, (float)p.y, shift_held(e));
    view_redraw(ui);
}

- (void)mouseUp:(NSEvent*)e
{
    (void)e;
    if (!ui) return;
    panel_mouse_up(&ui->panel);
    view_redraw(ui);
}

- (void)scrollWheel:(NSEvent*)e
{
    const NSPoint p = [self panelPoint:e];
    /* Trackpads report pixels, wheels report lines. */
    float steps = (float)[e scrollingDeltaY];
    if ([e hasPreciseScrollingDeltas])
        steps /= 10.0f;
    if (!ui || steps == 0.0f) return;
    panel_wheel(&ui->panel, (float)p.x, (float)p.y, steps, shift_held(e));
    view_redraw(ui);
}

- (void)tick:(NSTimer*)t
{
    (void)t;
    if (!ui)
        return;
    tick(&ui->host);
    view_redraw(ui);
}

@end

static int window_open(TbView* v, void* parent)
{
    TB_VIEW_CLASS* nv = [[TB_VIEW_CLASS alloc] initWithFrame:NSMakeRect(0, 0, PANEL_W, PANEL_H)];
    if (!nv)
        return 0;
    nv->ui = v;
    /* Picks up host updates such as the oscillator lamp, also while a control is being dragged. */
    nv->timer = [NSTimer timerWithTimeInterval:1.0 / 30.0 target:nv selector:@selector(tick:) userInfo:nil repeats:YES];
    [[NSRunLoop currentRunLoop] addTimer:nv->timer forMode:NSRunLoopCommonModes];
    if (parent)
        [(NSView*)parent addSubview:nv];
    v->view = nv;
    return 1;
}

static void window_close(TbView* v)
{
    TB_VIEW_CLASS* nv = (TB_VIEW_CLASS*)v->view;
    if (!nv)
        return;
    [nv->timer invalidate]; /* the timer retains the view until then */
    nv->timer = nil;
    nv->ui = NULL;
    [nv removeFromSuperview];
    [nv release];
    v->view = NULL;
}

void* tb_view_widget(TbView* v) { return v->view; }

static void view_redraw(TbView* v)
{
    if (v->panel.dirty && v->view)
        [(NSView*)v->view setNeedsDisplay:YES];
}

/* The view's own timer drives redraws. */
int tb_view_idle(TbView* v) { view_redraw(v); return 0; }

#else
#    error "No window layer for this platform (Windows, macOS and X11 are supported)."
#endif

/* ---- Common ---- */

TbView* tb_view_open(void* parent, const TbViewHost* host)
{
    TbView* v = (TbView*)calloc(1, sizeof(TbView));
    if (!v)
        return NULL;
    v->host = *host;
    if (!panel_init(&v->panel, on_write, &v->host)) {
        free(v);
        return NULL;
    }
    v->panel.gesture = on_gesture;
    if (!window_open(v, parent)) {
        panel_free(&v->panel);
        free(v);
        return NULL;
    }
    return v;
}

void tb_view_set(TbView* v, int port, float value)
{
    panel_set(&v->panel, port, value);
}

void tb_view_close(TbView* v)
{
    if (!v)
        return;
    window_close(v);
    panel_free(&v->panel);
    free(v);
}
