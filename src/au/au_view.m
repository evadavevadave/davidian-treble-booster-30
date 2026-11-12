/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Davidian. See LICENSE and NOTICE.md. */
/* The Audio Unit's Cocoa view: a factory the host finds through kAudioUnitProperty_CocoaUI,
 * and a container view that holds the panel from view.c and talks to the Audio Unit through
 * its parameters. Built without ARC. */
#import <AudioToolbox/AudioUnitUtilities.h>
#import <AudioUnit/AUCocoaUIView.h>
#import <Cocoa/Cocoa.h>

#include "../params.h"
#include "../ui/view.h"

/* Class names are global to the process; CMakeLists.txt sets these to Davidian names. */
#ifndef TB_AU_VIEW_FACTORY
#    error "TB_AU_VIEW_FACTORY must name the Cocoa view factory class"
#endif
#ifndef TB_AU_CONTAINER
#    error "TB_AU_CONTAINER must name the container view class"
#endif

@interface TB_AU_CONTAINER : NSView {
@public
    AudioUnit au;
    TbView* panel;
}
@end

@implementation TB_AU_CONTAINER

/* The host owns this view. When it lets go, the panel and its timer go too. */
- (void)dealloc
{
    tb_view_close(panel);
    panel = NULL;
    [super dealloc];
}

@end

static AudioUnitParameter param(TB_AU_CONTAINER* c, int port)
{
    AudioUnitParameter p;
    p.mAudioUnit = c->au;
    p.mParameterID = (AudioUnitParameterID)port;
    p.mScope = kAudioUnitScope_Global;
    p.mElement = 0;
    return p;
}

static void on_write(void* host, int port, float value)
{
    TB_AU_CONTAINER* c = (TB_AU_CONTAINER*)host;
    AudioUnitParameter p = param(c, port);
    AudioUnitSetParameter(c->au, p.mParameterID, kAudioUnitScope_Global, 0, value, 0);
    AUParameterListenerNotify(NULL, NULL, &p); /* tells the host, for automation and its own controls */
}

static void on_gesture(void* host, int port, int begin)
{
    TB_AU_CONTAINER* c = (TB_AU_CONTAINER*)host;
    AudioUnitEvent e;
    memset(&e, 0, sizeof e);
    e.mEventType = begin ? kAudioUnitEvent_BeginParameterChangeGesture : kAudioUnitEvent_EndParameterChangeGesture;
    e.mArgument.mParameter = param(c, port);
    AUEventListenerNotify(NULL, NULL, &e);
}

/* Reads every parameter, so automation, presets and the oscillator lamp all reach the panel. */
static void on_tick(void* host)
{
    TB_AU_CONTAINER* c = (TB_AU_CONTAINER*)host;
    int i;
    if (!c->panel)
        return;
    for (i = 0; i < TB_NUM_PARAMS; ++i) {
        AudioUnitParameterValue v;
        const int port = TB_PARAMS[i].port;
        if (port == PORT_ENABLED)
            continue;
        if (AudioUnitGetParameter(c->au, (AudioUnitParameterID)port, kAudioUnitScope_Global, 0, &v) == noErr)
            tb_view_set(c->panel, port, v);
    }
}

@interface TB_AU_VIEW_FACTORY : NSObject <AUCocoaUIBase>
@end

@implementation TB_AU_VIEW_FACTORY

- (unsigned)interfaceVersion { return 0; }

- (NSString*)description { return @"Davidian Treble Booster 30"; }

- (NSView*)uiViewForAudioUnit:(AudioUnit)au withSize:(NSSize)size
{
    TbViewHost host;
    TB_AU_CONTAINER* c = [[TB_AU_CONTAINER alloc] initWithFrame:NSMakeRect(0, 0, TB_VIEW_W, TB_VIEW_H)];
    (void)size; /* the panel has one size */
    if (!c)
        return nil;
    c->au = au;
    memset(&host, 0, sizeof host);
    host.host = c; /* not retained: the container outlives its panel */
    host.write = on_write;
    host.gesture = on_gesture;
    host.tick = on_tick;
    c->panel = tb_view_open(c, &host);
    if (!c->panel) {
        [c release];
        return nil;
    }
    on_tick(c);
    return [c autorelease];
}

@end
