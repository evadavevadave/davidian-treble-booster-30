/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Davidian. See LICENSE and NOTICE.md. */
/* The Treble Booster 30 engine as a small C API, shared by the LV2, VST3 and Audio Unit builds.
 * It follows the LV2 model: every port in ports.h is a float pointer the wrapper connects,
 * control ports are read once per block, and the lamp and latency outputs are written per
 * block. The circuit model behind it is C++ (src/dsp). */
#ifndef TB_ENGINE_H
#define TB_ENGINE_H

#include <stdint.h>

#include "ports.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TbEngine TbEngine;

TbEngine* tb_engine_new(double sample_rate); /* allocates and builds the circuit; not real-time safe */
void tb_engine_connect(TbEngine* e, uint32_t port, void* data);
/* Resets the circuit to its operating point; connect the controls first. Not real-time safe. */
void tb_engine_activate(TbEngine* e);
void tb_engine_run(TbEngine* e, uint32_t frames); /* in place is fine */
uint32_t tb_engine_latency(const TbEngine* e);
/* The latency an engine at this sample rate will have, for hosts that ask before activation. */
uint32_t tb_latency_for_rate(double sample_rate);
void tb_engine_free(TbEngine* e);

#ifdef __cplusplus
}
#endif

#endif
