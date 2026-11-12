/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Davidian. See LICENSE and NOTICE.md. */
/* LV2 GUI for the Treble Booster 30: the LV2 UI entry points around the panel view in view.c, which has
 * the Win32 (ui:WindowsUI), Cocoa (ui:CocoaUI) and X11 (ui:X11UI) window layers. The UI ABI is
 * declared here so no LV2 SDK is needed. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../lv2_abi.h"
#include "../ports.h"
#include "view.h"

/* ---- LV2 UI ABI (layout and names match lv2/ui/ui.h, Copyright 2009-2016 David Robillard
 * and Lars Luthman, ISC license; see NOTICE.md) ---- */

typedef void* LV2UI_Widget;
typedef void* LV2UI_Handle;
typedef void* LV2UI_Controller;
typedef void* LV2UI_Feature_Handle;
typedef void (*LV2UI_Write_Function)(LV2UI_Controller controller, uint32_t port_index,
                                     uint32_t buffer_size, uint32_t port_protocol,
                                     const void* buffer);
typedef struct LV2UI_Descriptor {
    const char* URI;
    LV2UI_Handle (*instantiate)(const struct LV2UI_Descriptor* descriptor, const char* plugin_uri,
                                const char* bundle_path, LV2UI_Write_Function write_function,
                                LV2UI_Controller controller, LV2UI_Widget* widget,
                                const LV2_Feature* const* features);
    void (*cleanup)(LV2UI_Handle ui);
    void (*port_event)(LV2UI_Handle ui, uint32_t port_index, uint32_t buffer_size,
                       uint32_t format, const void* buffer);
    const void* (*extension_data)(const char* uri);
} LV2UI_Descriptor;
typedef struct { int (*idle)(LV2UI_Handle ui); } LV2UI_Idle_Interface;
typedef struct {
    LV2UI_Feature_Handle handle;
    int (*ui_resize)(LV2UI_Feature_Handle handle, int width, int height);
} LV2UI_Resize;

#define LV2_UI__parent        "http://lv2plug.in/ns/extensions/ui#parent"
#define LV2_UI__resize        "http://lv2plug.in/ns/extensions/ui#resize"
#define LV2_UI__idleInterface "http://lv2plug.in/ns/extensions/ui#idleInterface"

typedef struct {
    TbView* view;
    LV2UI_Write_Function write;
    LV2UI_Controller controller;
} UI;

static void host_write(void* host, int port, float value)
{
    UI* ui = (UI*)host;
    if (ui->write)
        ui->write(ui->controller, (uint32_t)port, sizeof(float), 0, &value);
}

static LV2UI_Handle instantiate(const LV2UI_Descriptor* descriptor, const char* plugin_uri,
                                const char* bundle_path, LV2UI_Write_Function write_function,
                                LV2UI_Controller controller, LV2UI_Widget* widget,
                                const LV2_Feature* const* features)
{
    UI* ui;
    TbViewHost host;
    void* parent = NULL;
    const LV2UI_Resize* resize = NULL;
    int i;
    (void)descriptor;
    (void)bundle_path;
    if (!plugin_uri || strcmp(plugin_uri, TB_URI) != 0)
        return NULL;
    for (i = 0; features && features[i]; ++i) {
        if (!strcmp(features[i]->URI, LV2_UI__parent))
            parent = features[i]->data;
        else if (!strcmp(features[i]->URI, LV2_UI__resize))
            resize = (const LV2UI_Resize*)features[i]->data;
    }
    ui = (UI*)calloc(1, sizeof(UI));
    if (!ui)
        return NULL;
    ui->write = write_function;
    ui->controller = controller;
    memset(&host, 0, sizeof host);
    host.host = ui;
    host.write = host_write;
    ui->view = tb_view_open(parent, &host);
    if (!ui->view) {
        free(ui);
        return NULL;
    }
    if (resize)
        resize->ui_resize(resize->handle, TB_VIEW_W, TB_VIEW_H);
    *widget = tb_view_widget(ui->view);
    return ui;
}

static void cleanup(LV2UI_Handle h)
{
    UI* ui = (UI*)h;
    tb_view_close(ui->view);
    free(ui);
}

static void port_event(LV2UI_Handle h, uint32_t port, uint32_t size, uint32_t format, const void* buffer)
{
    UI* ui = (UI*)h;
    if (format != 0 || size != sizeof(float))
        return;
    tb_view_set(ui->view, (int)port, *(const float*)buffer);
}

static int idle(LV2UI_Handle h) { return tb_view_idle(((UI*)h)->view); }

static const LV2UI_Idle_Interface idle_iface = { idle };

static const void* extension_data(const char* uri)
{
    return !strcmp(uri, LV2_UI__idleInterface) ? &idle_iface : NULL;
}

static const LV2UI_Descriptor ui_descriptor = {
    TB_UI_URI, instantiate, cleanup, port_event, extension_data
};

LV2_EXPORT const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index)
{
    return index == 0 ? &ui_descriptor : NULL;
}
