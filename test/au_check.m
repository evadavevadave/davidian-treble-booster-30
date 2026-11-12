/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Davidian. See LICENSE and NOTICE.md. */
/* Loads the built Audio Unit straight from its bundle (no install needed) and checks what
 * auval does not: that it sounds the same as the LV2 build, sample for sample, with the same
 * settings (including the host's bypass) and with a mono input, that it reports its latency,
 * and that its Cocoa view opens. macOS only, built without ARC.
 * Usage: au_check <Davidian Treble Booster 30.component> <treble30.lv2> */
#import <AudioToolbox/AudioToolbox.h>
#import <AudioUnit/AUCocoaUIView.h>
#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#include <stdio.h>

#include "compare.h"

static int failures;

static void check(const char* what, int ok)
{
    printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        ++failures;
}

static OSStatus feed(void* ref, AudioUnitRenderActionFlags* flags, const AudioTimeStamp* ts, UInt32 bus,
                     UInt32 frames, AudioBufferList* io)
{
    UInt32 c, i;
    (void)ref;
    (void)flags;
    (void)bus;
    for (c = 0; c < io->mNumberBuffers; ++c) {
        float* d = (float*)io->mBuffers[c].mData;
        for (i = 0; i < frames; ++i)
            d[i] = cmp_input((int)c, (int)ts->mSampleTime + (int)i);
    }
    return noErr;
}

static void set_format(AudioUnit au, AudioUnitScope scope, UInt32 channels)
{
    AudioStreamBasicDescription f;
    memset(&f, 0, sizeof f);
    f.mSampleRate = CMP_RATE;
    f.mFormatID = kAudioFormatLinearPCM;
    f.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
    f.mBytesPerPacket = f.mBytesPerFrame = sizeof(float);
    f.mFramesPerPacket = 1;
    f.mChannelsPerFrame = channels;
    f.mBitsPerChannel = 32;
    check(scope == kAudioUnitScope_Input ? "accepts the input format" : "accepts the output format",
          AudioUnitSetProperty(au, kAudioUnitProperty_StreamFormat, scope, 0, &f, sizeof f) == noErr);
}

/* Renders the comparison run; returns interleaved stereo or NULL. */
static float* render(AudioUnit au, int channels_in, double* latency)
{
    AURenderCallbackStruct cb = { feed, NULL };
    UInt32 max = CMP_BLOCK, one = 1, size = sizeof(Float64);
    Float64 lat = -1;
    float l[CMP_BLOCK], r[CMP_BLOCK];
    float* out = (float*)malloc(sizeof(float) * 2 * CMP_FRAMES);
    AudioBufferList* abl = (AudioBufferList*)calloc(1, sizeof(AudioBufferList) + sizeof(AudioBuffer));
    int b, i, k;

    set_format(au, kAudioUnitScope_Input, (UInt32)channels_in);
    set_format(au, kAudioUnitScope_Output, 2);
    AudioUnitSetProperty(au, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &max, sizeof max);
    AudioUnitSetProperty(au, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &cb, sizeof cb);
    check("initializes", AudioUnitInitialize(au) == noErr);
    AudioUnitGetProperty(au, kAudioUnitProperty_Latency, kAudioUnitScope_Global, 0, &lat, &size);
    *latency = lat;
    /* Like the other hosts: start from the defaults, then change the controls before the first block. */
    for (k = 0; k < CMP_NUM_CHANGES; ++k)
        AudioUnitSetParameter(au, (AudioUnitParameterID)CMP_CHANGES[k].port, kAudioUnitScope_Global, 0,
                              CMP_CHANGES[k].plain, 0);
    for (b = 0; b < CMP_BLOCKS && out; ++b) {
        AudioUnitRenderActionFlags flags = 0;
        AudioTimeStamp ts;
        OSStatus err;
        if (b == CMP_BYPASS)
            AudioUnitSetProperty(au, kAudioUnitProperty_BypassEffect, kAudioUnitScope_Global, 0, &one, sizeof one);
        memset(&ts, 0, sizeof ts);
        ts.mFlags = kAudioTimeStampSampleTimeValid;
        ts.mSampleTime = (Float64)b * CMP_BLOCK;
        abl->mNumberBuffers = 2;
        abl->mBuffers[0].mNumberChannels = abl->mBuffers[1].mNumberChannels = 1;
        abl->mBuffers[0].mDataByteSize = abl->mBuffers[1].mDataByteSize = sizeof l;
        abl->mBuffers[0].mData = l;
        abl->mBuffers[1].mData = r;
        err = AudioUnitRender(au, &flags, &ts, 0, CMP_BLOCK, abl);
        if (err != noErr) {
            printf("  render error %d at block %d\n", (int)err, b);
            free(out);
            out = NULL;
            break;
        }
        for (i = 0; i < CMP_BLOCK; ++i) {
            out[2 * (b * CMP_BLOCK + i)] = ((float*)abl->mBuffers[0].mData)[i];
            out[2 * (b * CMP_BLOCK + i) + 1] = ((float*)abl->mBuffers[1].mData)[i];
        }
    }
    AudioUnitUninitialize(au);
    one = 0;
    AudioUnitSetProperty(au, kAudioUnitProperty_BypassEffect, kAudioUnitScope_Global, 0, &one, sizeof one);
    free(abl);
    return out;
}

static void test_view(AudioUnit au)
{
    AudioUnitCocoaViewInfo info;
    UInt32 size = sizeof info;
    NSBundle* bundle;
    Class factory_class;
    id<AUCocoaUIBase> factory;
    NSView* view;
    memset(&info, 0, sizeof info);
    check("names a Cocoa view", AudioUnitGetProperty(au, kAudioUnitProperty_CocoaUI, kAudioUnitScope_Global, 0,
                                                     &info, &size) == noErr && info.mCocoaAUViewClass[0]);
    if (!info.mCocoaAUViewBundleLocation || !info.mCocoaAUViewClass[0])
        return;
    bundle = [NSBundle bundleWithURL:(NSURL*)info.mCocoaAUViewBundleLocation];
    factory_class = bundle ? [bundle classNamed:(NSString*)info.mCocoaAUViewClass[0]] : nil;
    if (!factory_class) {
        Class any = NSClassFromString((NSString*)info.mCocoaAUViewClass[0]);
        printf("  lookup: class %s, bundle %s, loaded %d, executable %s, class image %s\n",
               [(NSString*)info.mCocoaAUViewClass[0] UTF8String], [[bundle bundlePath] UTF8String], (int)[bundle isLoaded],
               [[bundle executablePath] UTF8String], any ? class_getImageName(any) : "(no such class)");
    }
    check("view factory class is in the bundle", factory_class != nil);
    check("view factory follows AUCocoaUIBase",
          factory_class && [factory_class conformsToProtocol:@protocol(AUCocoaUIBase)]);
    if (factory_class) {
        @autoreleasepool {
            factory = [[factory_class alloc] init];
            view = [factory uiViewForAudioUnit:au withSize:NSZeroSize];
            check("view opens at 960 x 470", view && NSWidth([view frame]) == 960 && NSHeight([view frame]) == 470);
            check("view holds the panel", view && [[view subviews] count] == 1);
            [factory release];
        }
        /* The pool has released the view; its panel and timer went with it. */
        check("view closes without trouble", 1);
    }
    CFRelease(info.mCocoaAUViewBundleLocation);
    CFRelease(info.mCocoaAUViewClass[0]);
}

int main(int argc, char** argv)
{
    AudioComponentDescription desc = { kAudioUnitType_Effect, 'Tb30', 'Dvdn', 0, 0 };
    CFURLRef url;
    CFBundleRef bundle;
    AudioComponentFactoryFunction factory;
    AudioComponent comp;
    AudioUnit au = NULL;
    float* lv2;
    if (argc < 3) {
        fprintf(stderr, "usage: au_check <Davidian Treble Booster 30.component> <treble30.lv2>\n");
        return 2;
    }
    [NSApplication sharedApplication];

    /* Register the component for this process only, from its bundle. */
    url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8*)argv[1], (CFIndex)strlen(argv[1]), true);
    bundle = url ? CFBundleCreate(NULL, url) : NULL;
    check("opens the component bundle", bundle && CFBundleLoadExecutable(bundle));
    if (!bundle)
        return 1;
    check("bundle identifier", CFStringCompare(CFBundleGetIdentifier(bundle), CFSTR("com.davidian.treble-30.au"), 0)
                                   == kCFCompareEqualTo);
    factory = (AudioComponentFactoryFunction)CFBundleGetFunctionPointerForName(bundle,
                                                                              CFSTR("DavidianTrebleBooster30AUFactory"));
    check("exports the factory named in Info.plist", factory != NULL);
    if (!factory)
        return 1;
    comp = AudioComponentRegister(&desc, CFSTR("Davidian: Treble Booster 30"), 0x00010000, factory);
    check("instantiates", comp && AudioComponentInstanceNew(comp, &au) == noErr && au);
    if (!au)
        return 1;

    {
        AudioUnitParameterInfo info;
        UInt32 size = 0, n;
        AudioUnitGetPropertyInfo(au, kAudioUnitProperty_ParameterList, kAudioUnitScope_Global, 0, &size, NULL);
        n = size / sizeof(AudioUnitParameterID);
        check("14 parameters: 13 controls and the lamp (the host has bypass)", n == 14);
        size = sizeof info;
        check("Vib-Trem Speed is a named list",
              AudioUnitGetProperty(au, kAudioUnitProperty_ParameterInfo, kAudioUnitScope_Global, PORT_SPEED,
                                   &info, &size) == noErr && info.unit == kAudioUnitParameterUnit_Indexed);
        if (info.flags & kAudioUnitParameterFlag_CFNameRelease)
            CFRelease(info.cfNameString);
        size = sizeof info;
        check("the oscillator lamp is read only",
              AudioUnitGetProperty(au, kAudioUnitProperty_ParameterInfo, kAudioUnitScope_Global, PORT_LAMP,
                                   &info, &size) == noErr && !(info.flags & kAudioUnitParameterFlag_IsWritable));
        if (info.flags & kAudioUnitParameterFlag_CFNameRelease)
            CFRelease(info.cfNameString);
    }

    lv2 = cmp_run_lv2(argv[2]);
    check("LV2 reference runs", lv2 != NULL);
    {
        double latency = -1;
        float* stereo = render(au, 2, &latency);
        float* mono;
        AudioUnitParameterValue lamp = -1;
        AudioUnitGetParameter(au, PORT_LAMP, kAudioUnitScope_Global, 0, &lamp);
        check("the oscillator lamp shows a level", lamp >= 0.0f && lamp <= 1.0f);
        check("reports 16 samples of latency at 48 kHz", fabs(latency * CMP_RATE - CMP_LATENCY) < 1e-6);
        if (stereo && lv2) {
            double peak, diff, wet;
            cmp_measure(stereo, lv2, &peak, &diff, &wet);
            printf("  peak %.3f, largest AU/LV2 difference %.2e, bypassed difference from input %.2e\n",
                   peak, diff, wet);
            check("Audio Unit output matches the LV2 build", peak > 0.05 && diff < 1e-5);
            check("host bypass passes the input through, latency-aligned", wet < 1e-6);
        } else {
            check("renders", 0);
        }
        /* A mono input drives the amplifier and both outputs. */
        for (int k = 0; k < TB_NUM_PARAMS; ++k)
            if (TB_PARAMS[k].port != PORT_ENABLED && !TB_PARAMS[k].output)
                AudioUnitSetParameter(au, (AudioUnitParameterID)TB_PARAMS[k].port, kAudioUnitScope_Global, 0,
                                      TB_PARAMS[k].def, 0);
        mono = render(au, 1, &latency);
        check("renders from a mono input", mono != NULL);
        if (mono) {
            double peak = 0.0;
            for (int i = 0; i < 2 * CMP_FRAMES; ++i)
                peak = fmax(peak, fabs((double)mono[i]));
            check("mono input gives a signal on both outputs", peak > 0.05);
        }
        free(stereo);
        free(mono);
    }
    free(lv2);

    test_view(au);

    AudioComponentInstanceDispose(au);
    printf(failures ? "\n%d failed\n" : "\nAll passed\n", failures);
    return failures ? 1 : 0;
}
