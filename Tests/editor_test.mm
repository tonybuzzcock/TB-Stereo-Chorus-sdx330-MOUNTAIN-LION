// editor_test.mm  (macOS only)
// Loads the built plugin, opens its panel in an offscreen window the way a host does,
// renders screenshots, drives the controls with synthetic mouse events at their drawn
// positions (knobs, alpha dial, buttons, LCD, drawer), checks host automation messages
// and the program file, and closes it again.
//
//   editor_test <plugin binary> <output dir for PNGs>

#import <Cocoa/Cocoa.h>

#include "../Source/VST2/vst2_abi.h"
#include "../Source/VST2/Params.h"
#include "../Source/VST2/Editor.h"

#include <dlfcn.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace tbsdx;

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("  %s  %s\n", ok ? "ok  " : "FAIL", what);
    if (! ok) ++failures;
}

static int automateCalls = 0, beginCalls = 0, endCalls = 0;
static VstIntPtr VST_CALL hostCallback (AEffect*, VstInt32 opcode, VstInt32, VstIntPtr, void*, float)
{
    switch (opcode)
    {
        case audioMasterVersion:   return 2400;
        case audioMasterAutomate:  ++automateCalls; return 0;
        case audioMasterBeginEdit: ++beginCalls; return 1;
        case audioMasterEndEdit:   ++endCalls; return 1;
        default:                   return 0;
    }
}

typedef AEffect* (*EntryProc) (audioMasterCallback);

static VstIntPtr call (AEffect* e, VstInt32 op, VstInt32 idx = 0, VstIntPtr val = 0, void* ptr = 0, float opt = 0.0f)
{
    return e->dispatcher (e, op, idx, val, ptr, opt);
}

static NSData* renderPNG (NSView* v, const std::string& path)
{
    const NSRect b = [v bounds];
    NSBitmapImageRep* rep = [v bitmapImageRepForCachingDisplayInRect: b];
    [v cacheDisplayInRect: b toBitmapImageRep: rep];
    NSData* png = [rep representationUsingType: NSPNGFileType properties: [NSDictionary dictionary]];
    [png writeToFile: [NSString stringWithUTF8String: path.c_str()] atomically: YES];
    std::printf ("  wrote %s (%lu bytes)\n", path.c_str(), (unsigned long) [png length]);
    return png;
}

static void mouse (NSWindow* w, NSView* v, NSEventType type, NSPoint panelPt, int clicks = 1)
{
    const NSPoint wp = [v convertPoint: panelPt toView: nil];
    NSEvent* e = [NSEvent mouseEventWithType: type location: wp modifierFlags: 0 timestamp: 0
                                windowNumber: [w windowNumber] context: nil eventNumber: 0
                                  clickCount: clicks pressure: 1.0f];
    if (type == NSLeftMouseDown) [v mouseDown: e];
    else if (type == NSLeftMouseDragged) [v mouseDragged: e];
    else [v mouseUp: e];
}

static void click (NSWindow* w, NSView* v, CGFloat x, CGFloat y)
{
    mouse (w, v, NSLeftMouseDown, NSMakePoint (x, y));
    mouse (w, v, NSLeftMouseUp, NSMakePoint (x, y));
}

static void drag (NSWindow* w, NSView* v, CGFloat x, CGFloat y, CGFloat dy)
{
    mouse (w, v, NSLeftMouseDown, NSMakePoint (x, y));
    const int steps = 5;
    for (int k = 1; k <= steps; ++k) mouse (w, v, NSLeftMouseDragged, NSMakePoint (x, y + dy * k / steps));
    mouse (w, v, NSLeftMouseUp, NSMakePoint (x, y + dy));
}

static void pump (double secs)
{
    [[NSRunLoop currentRunLoop] runUntilDate: [NSDate dateWithTimeIntervalSinceNow: secs]];
}

static NSTextField* nameBox (NSView* panel)
{
    NSTextField* f = nil;
    for (NSView* sv in [panel subviews]) if ([sv isKindOfClass: [NSTextField class]]) f = (NSTextField*) sv;
    return f;
}

static bool fileHas (const std::string& path, const char* what)
{
    FILE* f = std::fopen (path.c_str(), "r");
    if (! f) return false;
    static char ln[8192];
    bool found = false;
    while (std::fgets (ln, sizeof (ln), f)) if (std::strstr (ln, what)) found = true;
    std::fclose (f);
    return found;
}

static int prog (AEffect* e) { return programNumber (e->getParameter (e, kProgram)); }

// Control positions, as drawn by PanelView.mm
#define IN_L      112, 172
#define IN_R      142, 172
#define OUT       236, 172
#define B_PROGRAM 104, 286
#define B_FUNC1   220, 286
#define B_FUNC2   104, 342
#define B_FUNC3   220, 342
#define B_UP      530, 286
#define B_DOWN    530, 342
#define B_EDIT    636, 286
#define B_SYSTEM  752, 286
#define B_EXIT    868, 286
#define B_PAGE    636, 342
#define B_MEMORY  752, 342
#define B_BYPASS  868, 342
#define DIAL      402, 318
#define LCD       800, 150
#define DRAWER    142, 447

int main (int argc, char** argv)
{
    if (argc < 3) { std::printf ("usage: %s <plugin binary> <png dir>\n", argv[0]); return 2; }
    setvbuf (stdout, 0, _IONBF, 0);
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    [NSApplication sharedApplication];
    const std::string dir = argv[2];

    const std::string progFile = dir + "/test_programs/Programs.txt";
    setenv ("TBSDX_PROGRAM_FILE", progFile.c_str(), 1);
    std::remove (progFile.c_str());

    void* lib = dlopen (argv[1], RTLD_NOW | RTLD_LOCAL);
    if (! lib) { std::printf ("dlopen failed: %s\n", dlerror()); return 1; }
    EntryProc entry = (EntryProc) dlsym (lib, "VSTPluginMain");
    if (! entry) { std::printf ("no entry point\n"); return 1; }
    AEffect* e = entry (hostCallback);
    call (e, effOpen);
    call (e, effSetSampleRate, 0, 0, 0, 44100.0f);
    call (e, effMainsChanged, 0, 1);

    std::printf ("Editor\n");
    check ((e->flags & effFlagsHasEditor) != 0, "plugin reports a custom editor");
    ERect* r = 0;
    call (e, effEditGetRect, 0, 0, &r);
    check (r && r->right - r->left == kEditorWidth && r->bottom - r->top == kEditorHeight, "editor size as declared");
    NSWindow* win = [[NSWindow alloc] initWithContentRect: NSMakeRect (0, 0, kEditorWidth, kEditorHeight)
                                                styleMask: NSBorderlessWindowMask backing: NSBackingStoreBuffered defer: NO];
    NSView* parent = [win contentView];
    check (call (e, effEditOpen, 0, 0, parent) == 1, "effEditOpen succeeds");
    check ([[parent subviews] count] == 1, "panel view added to the host's view");
    NSView* panel = [[parent subviews] count] ? [[parent subviews] objectAtIndex: 0] : nil;
    if (! panel) return 1;
    check (NSEqualSizes ([panel frame].size, NSMakeSize (kEditorWidth, kEditorHeight)), "panel has the declared size");
    NSData* pngDefault = renderPNG (panel, dir + "/panel_default.png");
    check ([pngDefault length] > 10000, "panel renders");

    std::printf ("\nLevel knobs\n");
    {
        const int knobs[3] = { kInputL, kInputR, kOutput };
        const CGFloat kx[3] = { 112, 142, 236 }, ky[3] = { 172, 172, 172 };
        bool all = true;
        for (int k = 0; k < 3; ++k)
        {
            for (int j = 0; j < 3; ++j) e->setParameter (e, knobs[j], 0.5f);
            automateCalls = beginCalls = endCalls = 0;
            drag (win, panel, kx[k], ky[k], -20.0);
            bool others = true;
            for (int j = 0; j < 3; ++j) if (j != k && e->getParameter (e, knobs[j]) != 0.5f) others = false;
            if (std::fabs (e->getParameter (e, knobs[k]) - 0.6f) > 0.01f || ! others || beginCalls != 1 || endCalls != 1)
            { all = false; std::printf ("  knob %d: %.3f\n", knobs[k], e->getParameter (e, knobs[k])); }
        }
        check (all, "INPUT L (inner), INPUT R (outer) and OUTPUT each turn on their own, with begin / automate / end");
        mouse (win, panel, NSLeftMouseDown, NSMakePoint (OUT), 2);
        mouse (win, panel, NSLeftMouseUp, NSMakePoint (OUT), 2);
        check (e->getParameter (e, kOutput) == 0.5f, "double-click resets a knob");
        for (int j = 0; j < 3; ++j) e->setParameter (e, knobs[j], 0.5f);
    }

    std::printf ("\nPlay mode: the alpha dial and arrows choose programs\n");
    {
        check (prog (e) == 201, "starts on 201");
        drag (win, panel, DIAL, -10.0);
        check (prog (e) == 202, "dial up one step: 202");
        float v[2] = { e->getParameter (e, kAlgo), e->getParameter (e, kRate) };
        check (algoIndex (v[0]) == kStereo3D, "202 DIMENSIONAL CHO loads its algorithm (Stereo 3D Chorus)");
        click (win, panel, B_UP);
        check (prog (e) == 203, "increment button: 203");
        click (win, panel, B_DOWN); click (win, panel, B_DOWN);
        check (prog (e) == 201, "decrement twice: 201");
        click (win, panel, B_DOWN);
        check (prog (e) == 200, "below 201 into the user area (200)");
        for (int k = 0; k < 175; ++k) click (win, panel, B_DOWN);
        check (prog (e) == 25, "down to 25");
        click (win, panel, B_UP);
        check (prog (e) == 26, "user 26 holds a copy of SMOOTH 3D CHORUS");
        renderPNG (panel, dir + "/panel_program_026.png");
        click (win, panel, B_PROGRAM);
        pump (0.05);
        renderPNG (panel, dir + "/panel_show_algorithm.png");
        click (win, panel, B_PROGRAM);
    }

    std::printf ("\nEdit mode\n");
    {
        click (win, panel, B_EDIT);
        pump (0.05);
        renderPNG (panel, dir + "/panel_edit_page.png");
        // PAGE lit: the dial turns pages. Stereo 3D Chorus: [Algo Rate Depth] [PrDly LFO Phase] [AzmtL ElevL AzmtR]
        drag (win, panel, DIAL, -20.0);
        const float az0 = e->getParameter (e, kAzmtL);
        click (win, panel, B_FUNC1);                 // AzmtL
        drag (win, panel, DIAL, -30.0);              // three steps
        const float az1 = e->getParameter (e, kAzmtL);
        char d0[16], d1[16];
        formatParam (kAzmtL, az0, d0, sizeof (d0)); formatParam (kAzmtL, az1, d1, sizeof (d1));
        std::printf ("  AzmtL %s -> %s\n", d0, d1);
        check (az1 > az0 && std::fabs (toReal (kAzmtL, az1) - toReal (kAzmtL, az0) - 3.0) < 0.6, "on page 3 FUNC 1 picks AzmtL; the dial steps it");
        pump (0.05);
        renderPNG (panel, dir + "/panel_edit_value.png");
        // Back to page 1: PAGE, dial down, FUNC 1 = Algo
        click (win, panel, B_PAGE);
        drag (win, panel, DIAL, 20.0);
        click (win, panel, B_FUNC2);                 // Rate
        const float rate0 = e->getParameter (e, kRate);
        click (win, panel, B_UP);
        check (e->getParameter (e, kRate) > rate0, "FUNC 2 on page 1 is Rate; the arrow steps it");
        click (win, panel, B_FUNC1);
        click (win, panel, B_UP);
        check (algoIndex (e->getParameter (e, kAlgo)) == kPanner3D, "FUNC 1 on page 1 is the algorithm");
        click (win, panel, B_DOWN);
        check (algoIndex (e->getParameter (e, kAlgo)) == kStereo3D, "and back");
        click (win, panel, B_EXIT);
        pump (0.05);
        renderPNG (panel, dir + "/panel_edited.png");
    }

    std::printf ("\nSYSTEM: Direct Mix\n");
    {
        click (win, panel, B_SYSTEM);
        click (win, panel, B_UP);
        check (toIndex (kDirectMix, e->getParameter (e, kDirectMix)) == 1, "Direct Mix PROG -> ON");
        pump (0.05);
        renderPNG (panel, dir + "/panel_system.png");
        click (win, panel, B_DOWN);
        check (toIndex (kDirectMix, e->getParameter (e, kDirectMix)) == 0, "and back to PROG");
        click (win, panel, B_EXIT);
    }

    std::printf ("\nMEMORY: write a user program and name it\n");
    {
        click (win, panel, B_MEMORY);
        check (true, "MEMORY opens the write screen");
        click (win, panel, B_UP);                    // 26 -> 27
        pump (0.05);
        renderPNG (panel, dir + "/panel_write.png");
        click (win, panel, B_MEMORY);
        check (prog (e) == 27, "written to 27, now the current program");
        check (fileHas (progFile, "prog 27:"), "the program file holds 27");
        NSTextField* f = nameBox (panel);
        check (f != nil, "writing opens a box over the LCD to name it");
        renderPNG (panel, dir + "/panel_naming.png");
        if (f)
        {
            [f setStringValue: @"WIDE 3D PAD"];
            [f sendAction: [f action] to: [f target]];
            pump (0.2);
        }
        check (fileHas (progFile, "name 27: WIDE 3D PAD"), "Return saves the name");
        check (nameBox (panel) == nil, "and the box goes away");
        renderPNG (panel, dir + "/panel_written.png");
        // Clicking the LCD renames; Esc keeps the old name
        click (win, panel, LCD);
        f = nameBox (panel);
        check (f != nil && [[f stringValue] isEqualToString: @"WIDE 3D PAD"], "clicking the LCD opens the name to edit");
        if (f)
        {
            [f setStringValue: @"SOMETHING ELSE"];
            [(id) panel control: f textView: nil doCommandBySelector: @selector (cancelOperation:)];
            pump (0.2);
        }
        check (fileHas (progFile, "name 27: WIDE 3D PAD"), "Esc keeps the old name");
        // Recall: away and back
        click (win, panel, B_UP);
        e->setParameter (e, kBypass, 1.0f);
        click (win, panel, B_DOWN);
        check (prog (e) == 27 && algoIndex (e->getParameter (e, kAlgo)) == kStereo3D, "the written program comes back");
        check (e->getParameter (e, kBypass) >= 0.5f, "loading a program leaves BYPASS alone");
        e->setParameter (e, kBypass, 0.0f);
    }

    std::printf ("\nBYPASS\n");
    {
        click (win, panel, B_BYPASS);
        check (e->getParameter (e, kBypass) >= 0.5f, "BYPASS latches in");
        const int n = 22050;
        std::vector<float> a (n), b (n), oa (n), ob (n);
        for (int i = 0; i < n; ++i) { a[i] = (float) (0.3 * std::sin (0.031 * i)); b[i] = (float) (0.2 * std::sin (0.017 * i)); }
        float* in[2] = { &a[0], &b[0] };
        float* out[2] = { &oa[0], &ob[0] };
        e->processReplacing (e, in, out, n);
        bool exact = true;
        for (int i = n / 2; i < n; ++i) if (oa[i] != a[i] || ob[i] != b[i]) exact = false;
        check (exact, "bypassed: the input comes out bit for bit");
        pump (0.1);
        renderPNG (panel, dir + "/panel_bypassed.png");
        click (win, panel, B_BYPASS);
        check (e->getParameter (e, kBypass) < 0.5f, "and out again");
    }

    std::printf ("\nPARAMETERS drawer\n");
    {
        e->setParameter (e, kAlgo, algoValue (kStereoChorus));
        click (win, panel, DRAWER);
        pump (0.6);                                  // the slide
        renderPNG (panel, dir + "/panel_drawer.png");
        const float r0 = e->getParameter (e, kRate);
        drag (win, panel, 112, 94, -20.0);           // first knob: Rate
        check (std::fabs (e->getParameter (e, kRate) - (r0 + 0.1f)) < 0.01f, "the drawer's first knob turns Rate");
        click (win, panel, 723, 27);                 // next algorithm
        check (algoIndex (e->getParameter (e, kAlgo)) == kStereo3D, "the drawer's arrow changes the algorithm");
        e->setParameter (e, kAlgo, algoValue (kPhase8));
        pump (0.1);
        renderPNG (panel, dir + "/panel_drawer_8phase.png");
        e->setParameter (e, kAlgo, algoValue (kRotary));
        pump (0.1);
        renderPNG (panel, dir + "/panel_drawer_rotary.png");
        const float lv = e->getParameter (e, kInputL);
        drag (win, panel, IN_L, -20.0);              // covered by the drawer: no change
        check (e->getParameter (e, kInputL) == lv, "the drawer covers the face's controls");
        click (win, panel, DRAWER);
        pump (0.6);
    }

    std::printf ("\nHost automation shows on the panel\n");
    {
        NSData* before = renderPNG (panel, dir + "/panel_before_host.png");
        e->setParameter (e, kOutput, 0.9f);
        e->setParameter (e, kAlgo, algoValue (kFlanger));
        pump (0.3);
        NSData* after = renderPNG (panel, dir + "/panel_changed.png");
        check (! [after isEqualToData: before], "panel redraws after host changes");
        e->setParameter (e, kOutput, 0.5f);
    }

    std::printf ("\nInput meter\n");
    {
        // Program 226 SMOOTH 3D CHORUS, as in the photo
        for (int guard = 0; guard < 400 && prog (e) != 226; ++guard)
        {
            if (prog (e) < 226) click (win, panel, B_UP); else click (win, panel, B_DOWN);
        }
        check (prog (e) == 226, "arrows reach 226");
        e->setParameter (e, kInputL, 0.62f);
        e->setParameter (e, kInputR, 0.55f);
        NSData* quiet = renderPNG (panel, dir + "/panel_meter_off.png");
        const int n = 22050;
        std::vector<float> a (n), b (n), oa (n), ob (n);
        for (int i = 0; i < n; ++i) { a[i] = (float) (0.55 * std::sin (2.0 * 3.14159265358979 * 220.0 * i / 44100.0)); b[i] = 0.6f * a[i]; }
        float* in[2] = { &a[0], &b[0] };
        float* out[2] = { &oa[0], &ob[0] };
        e->processReplacing (e, in, out, n);
        pump (0.3);
        NSData* lit = renderPNG (panel, dir + "/panel_showcase.png");
        check (! [lit isEqualToData: quiet], "the meter lights with the input");
    }

    std::printf ("\nClose\n");
    call (e, effEditClose);
    check ([[parent subviews] count] == 0, "effEditClose removes the panel");
    check (call (e, effEditOpen, 0, 0, parent) == 1 && [[parent subviews] count] == 1, "reopens");
    call (e, effEditClose);
    call (e, effMainsChanged, 0, 0);
    call (e, effClose);
    [win release];
    [pool release];
    dlclose (lib);

    std::printf ("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL EDITOR CHECKS PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
