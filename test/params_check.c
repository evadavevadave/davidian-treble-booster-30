/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Davidian. See LICENSE and NOTICE.md. */
/* Checks that src/params.h (the VST3 and Audio Unit parameters) agrees with bundle/treble30.ttl
 * (the LV2 ports) and with the panel, and that the value conversions round-trip.
 * Usage: params_check <path to treble30.ttl> */
#ifndef _CRT_SECURE_NO_WARNINGS
#    define _CRT_SECURE_NO_WARNINGS
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/params.h"
#include "../src/ui/panel.h"

static int failures;

static void check(const char* what, int ok)
{
    if (!ok) {
        printf("  [FAIL] %s\n", what);
        ++failures;
    }
}

static int near(double a, double b) { return fabs(a - b) <= 1e-4 * (1.0 + fabs(b)); }

/* The text of the port with this index: from its opening "[" (the last one before
 * "lv2:index", since scale points come later) to the next port's opening "[". */
static const char* port_block(const char* ttl, int index, size_t* len)
{
    char key[32];
    const char *s, *e;
    snprintf(key, sizeof key, "lv2:index %d ;", index);
    s = strstr(ttl, key);
    if (!s)
        return NULL;
    e = strstr(s + 1, "lv2:index ");
    if (!e)
        e = s + strlen(s);
    else
        while (e > s && *e != '[')
            --e;
    while (s > ttl && *s != '[')
        --s;
    *len = (size_t)(e - s);
    return s;
}

static int field(const char* block, size_t len, const char* key, double* out)
{
    const char* s = strstr(block, key);
    if (!s || s >= block + len)
        return 0;
    return sscanf(s + strlen(key), "%lf", out) == 1;
}

static int has(const char* block, size_t len, const char* text)
{
    const char* s = strstr(block, text);
    return s && s < block + len;
}

static int count(const char* block, size_t len, const char* text)
{
    int n = 0;
    const char* s = block;
    while ((s = strstr(s, text)) && s < block + len) {
        ++n;
        ++s;
    }
    return n;
}

int main(int argc, char** argv)
{
    FILE* f;
    char* ttl;
    long size;
    int i, k;
    if (argc < 2) {
        fprintf(stderr, "usage: params_check <treble30.ttl>\n");
        return 2;
    }
    f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", argv[1]);
        return 2;
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    ttl = (char*)calloc(1, (size_t)size + 1);
    if (!ttl || fread(ttl, 1, (size_t)size, f) != (size_t)size)
        return 2;
    fclose(f);

    check("every control port but latency has a parameter", TB_NUM_PARAMS == PORT_LATENCY - PORT_CHANNEL);
    {
        size_t len = 0;
        const char* b = port_block(ttl, PORT_LATENCY, &len);
        check("latency port reports latency", b && has(b, len, "lv2:reportsLatency") && has(b, len, "lv2:latency"));
    }
    for (i = 0; i < TB_NUM_PARAMS; ++i) {
        const TbParam* p = &TB_PARAMS[i];
        size_t len = 0;
        const char* b = port_block(ttl, p->port, &len);
        char what[160];
        double v;
        printf("  %-22s port %2d\n", p->name, p->port);
        snprintf(what, sizeof what, "%s: in port order", p->name);
        check(what, p->port == PORT_CHANNEL + i);
        snprintf(what, sizeof what, "%s: port in the TTL", p->name);
        check(what, b != NULL);
        if (!b)
            continue;
        snprintf(what, sizeof what, "%s: TTL name", p->name);
        {
            char name[96];
            snprintf(name, sizeof name, "lv2:name \"%s\"", p->name);
            check(what, has(b, len, name));
        }
        snprintf(what, sizeof what, "%s: minimum", p->name);
        check(what, field(b, len, "lv2:minimum", &v) && near(v, p->min));
        snprintf(what, sizeof what, "%s: maximum", p->name);
        check(what, field(b, len, "lv2:maximum", &v) && near(v, p->max));
        if (!p->output) {
            snprintf(what, sizeof what, "%s: default", p->name);
            check(what, field(b, len, "lv2:default", &v) && near(v, p->def));
        }
        snprintf(what, sizeof what, "%s: input or output", p->name);
        check(what, has(b, len, p->output ? "lv2:OutputPort" : "lv2:InputPort"));
        snprintf(what, sizeof what, "%s: logarithmic", p->name);
        check(what, has(b, len, "pprop:logarithmic") == p->log);
        snprintf(what, sizeof what, "%s: switch positions", p->name);
        if (has(b, len, "lv2:enumeration"))
            check(what, count(b, len, "rdf:value") == p->steps + 1);
        else if (has(b, len, "lv2:toggled"))
            check(what, p->steps == 1);
        else
            check(what, p->steps == 0);

        /* Conversions: default and both ends survive a round trip, switches land on positions. */
        snprintf(what, sizeof what, "%s: 0-1 round trip", p->name);
        check(what, near(tb_from_norm(p, tb_to_norm(p, p->def)), p->def)
                    && near(tb_from_norm(p, 0.0), p->min) && near(tb_from_norm(p, 1.0), p->max));
        for (k = 0; k <= p->steps && p->steps; ++k) {
            char text[32];
            float back = -1.0f;
            snprintf(what, sizeof what, "%s: position %d", p->name, k);
            check(what, near(tb_from_norm(p, tb_to_norm(p, p->min + (float)k)), p->min + (float)k));
            tb_format(p, p->min + (float)k, text, sizeof text);
            check(what, tb_parse(p, text, &back) && near(back, p->min + (float)k));
        }
        if (!p->steps && !p->output) {
            char text[32];
            float back = -1.0f;
            tb_format(p, p->def, text, sizeof text);
            snprintf(what, sizeof what, "%s: text \"%s\" reads back", p->name, text);
            check(what, tb_parse(p, text, &back) && fabs(back - p->def) <= 0.01f * (p->max - p->min));
        }
    }

    /* The panel's controls use the same ranges and defaults. */
    for (i = 0; i < N_WIDGETS; ++i) {
        const Widget* w = &WIDGETS[i];
        const TbParam* p = tb_param(w->port);
        char what[160];
        snprintf(what, sizeof what, "panel %s: matches its parameter", w->name);
        check(what, p && near(w->min, p->min) && near(w->max, p->max) && near(w->def, p->def)
                        && w->log == p->log
                        && (w->kind == W_SWITCH ? w->nopts == p->steps + 1 : 1)
                        && (w->kind == W_FOOT ? p->steps == 1 : 1));
    }

    free(ttl);
    printf(failures ? "\n%d failed\n" : "\nAll passed\n", failures);
    return failures ? 1 : 0;
}
