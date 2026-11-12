/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Davidian. See LICENSE and NOTICE.md. */
/* LV2 entry points for the Treble Booster 30. The circuit itself is behind engine.h. */
#include <stddef.h>

#include "engine.h"
#include "lv2_abi.h"

static LV2_Handle instantiate(const LV2_Descriptor* descriptor, double sample_rate,
                              const char* bundle_path, const LV2_Feature* const* features)
{
    (void)descriptor;
    (void)bundle_path;
    (void)features;
    return tb_engine_new(sample_rate);
}

static void connect_port(LV2_Handle h, uint32_t port, void* data) { tb_engine_connect((TbEngine*)h, port, data); }
static void activate(LV2_Handle h) { tb_engine_activate((TbEngine*)h); }
static void run(LV2_Handle h, uint32_t n) { tb_engine_run((TbEngine*)h, n); }
static void deactivate(LV2_Handle h) { (void)h; }
static void cleanup(LV2_Handle h) { tb_engine_free((TbEngine*)h); }
static const void* extension_data(const char* uri) { (void)uri; return NULL; }

static const LV2_Descriptor descriptor = {
    TB_URI, instantiate, connect_port, activate, run, deactivate, cleanup, extension_data
};

LV2_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
    return index == 0 ? &descriptor : NULL;
}
