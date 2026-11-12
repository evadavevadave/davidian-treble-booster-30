/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Davidian. See LICENSE and NOTICE.md. */
/* Port indices, shared by the plugin, its GUI and the VST3 and Audio Unit wrappers. They must
 * match bundle/treble30.ttl. */
#ifndef PORTS_H
#define PORTS_H

#define TB_URI    "urn:davidian:treble-30"
#define TB_UI_URI "urn:davidian:treble-30#ui"

enum {
    PORT_IN, PORT_OUT,
    PORT_CHANNEL,  /* input jack pair: 0 Bright, 1 Normal, 2 Vib/Trem */
    PORT_JACK,     /* 0 Input 1, 1 Input 2 */
    PORT_BRIGHT, PORT_NORMAL, PORT_VIB, /* the three volume controls, 0-1 */
    PORT_CUT,      /* 0-1 */
    PORT_SPEED,    /* 0 Slow, 1 Medium, 2 Fast */
    PORT_MODE,     /* 0 Vibrato, 1 Tremolo */
    PORT_FOOT,     /* 0 Off, 1 On: the vib/trem footswitch */
    PORT_DEPTH,    /* 0-1: the depth preset */
    PORT_SPEAKER,  /* 0 Off (speaker terminals), 1 On (with the cabinet's acoustic response) */
    PORT_VOLTS,    /* volts at the input jack for a full-scale sample */
    PORT_OUTPUT,   /* dB */
    PORT_ENABLED,
    PORT_LAMP,     /* output: the oscillator lamp, 0-1 */
    PORT_LATENCY,  /* output: latency in samples */
    PORT_COUNT
};

/* Speaker terminal volts that make a full-scale sample at 0 dB output: about the rated
 * 30 watts into 15 ohms, at its peak. */
#define SPEAKER_FULL_SCALE 30.0

#endif
