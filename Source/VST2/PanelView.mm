// PanelView.mm
// The custom panel for TB SDX-330: an original drawing laid out like the Roland SDX-330's
// front panel (photo in Docs/), at half the 19" width and twice the 1U height, in the
// unit's own colours: the black face, amber model lettering, black knobs, the green LED
// program display and the yellow-green LCD. The TB electronic logo (red TB, "electronic"
// in white on the black) is where the maker's badge was.
//
// Top row:    INPUT LEVEL (concentric: inner L, outer R), OUTPUT LEVEL, the L/R input
//             meter and MIDI lamp, then the display window: PROGRAM number and the 17 x 2
//             LCD with FUNC 1-3 over its three columns.
// Bottom row: PROGRAM, FUNC 1-3, the alpha dial, the increment / decrement buttons, EDIT,
//             SYSTEM, EXIT, PAGE, MEMORY and BYPASS (with their lamps), POWER.
// Strip:      PARAMETERS drops a drawer over the face with a knob for every parameter of
//             the current algorithm and the EQ (the unit edits them only through the LCD).
//
// The LCD works as on the unit: play mode (alpha dial / arrows choose the program), EDIT
// (PAGE lit: the dial turns pages; FUNC 1-3 pick a parameter and the dial sets it), SYSTEM
// (Direct Mix), MEMORY (choose a user program 1-200, MEMORY again writes it, then type its
// name and press Return). Clicking the LCD on a written user program renames it.
//
// Cocoa, manual retain/release, and only APIs that exist in Mac OS X 10.8.5.
// Everything is drawn in code; no image files.

#import <Cocoa/Cocoa.h>

#include "Editor.h"
#include "Params.h"
#include "Programs.h"
#include "TBLogo.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

using namespace tbsdx;

namespace
{

// --- Geometry (flipped: y down) -------------------------------------------------------
const CGFloat kEarW = 30.0;

// Top row
const NSPoint kInKnob  = { 112.0, 172.0 };
const CGFloat kInOuterR = 34.0, kInInnerR = 21.0;
const NSPoint kOutKnob = { 236.0, 172.0 };
const CGFloat kOutR = 28.0;
const NSRect  kMeterRect  = { { 296.0, 96.0 }, { 84.0, 140.0 } };
const NSRect  kWindowRect = { { 396.0, 88.0 }, { 658.0, 156.0 } };
const NSRect  kDigitsRect = { { 416.0, 128.0 }, { 148.0, 76.0 } };
const NSRect  kLcdRect    = { { 584.0, 118.0 }, { 452.0, 106.0 } };
const CGFloat kLcdX0 = 594.0, kLcdCw = 25.4, kLcdLine1 = 124.0, kLcdLine2 = 172.0, kLcdLineH = 44.0;

// Bottom row: buttons
const CGFloat kBtnW = 104.0, kBtnH = 40.0;
enum Btn { kBtnProgram = 0, kBtnFunc1, kBtnFunc2, kBtnFunc3, kBtnUp, kBtnDown,
           kBtnEdit, kBtnSystem, kBtnExit, kBtnPage, kBtnMemory, kBtnBypass, kNumBtns };
const NSRect kBtnRects[kNumBtns] = {
    { {  52.0, 266.0 }, { kBtnW, kBtnH } },     // PROGRAM
    { { 168.0, 266.0 }, { kBtnW, kBtnH } },     // FUNC 1
    { {  52.0, 322.0 }, { kBtnW, kBtnH } },     // FUNC 2
    { { 168.0, 322.0 }, { kBtnW, kBtnH } },     // FUNC 3
    { { 500.0, 266.0 }, { 60.0, kBtnH } },      // increment
    { { 500.0, 322.0 }, { 60.0, kBtnH } },      // decrement
    { { 584.0, 266.0 }, { kBtnW, kBtnH } },     // EDIT
    { { 700.0, 266.0 }, { kBtnW, kBtnH } },     // SYSTEM
    { { 816.0, 266.0 }, { kBtnW, kBtnH } },     // EXIT
    { { 584.0, 322.0 }, { kBtnW, kBtnH } },     // PAGE
    { { 700.0, 322.0 }, { kBtnW, kBtnH } },     // MEMORY
    { { 816.0, 322.0 }, { kBtnW, kBtnH } },     // BYPASS
};
const char* const kBtnNames[kNumBtns] = { "PROGRAM", "FUNC 1", "FUNC 2", "FUNC 3", "", "",
                                          "EDIT", "SYSTEM", "EXIT", "PAGE", "MEMORY", "BYPASS" };
const bool kBtnHasLamp[kNumBtns] = { false, false, false, false, false, false,
                                     true, true, false, true, true, true };
const NSPoint kDial = { 402.0, 318.0 };
const CGFloat kDialR = 52.0;
const NSRect  kPowerRect = { { 958.0, 296.0 }, { 72.0, 52.0 } };

// Strip and drawer
const NSRect  kDrawerBtn = { { 44.0, (CGFloat) kFaceHeight + 22.0 }, { 196.0, 46.0 } };
const NSRect  kDrawerRect = { { kEarW, 0.0 }, { (CGFloat) kEditorWidth - 2.0 * kEarW, (CGFloat) kFaceHeight } };
const CGFloat kCellW = 64.0, kCellH = 72.0, kGridX = 50.0, kGridY = 58.0, kEqY = 300.0;
const int     kPerRow = 15;
const CGFloat kSmallR = 15.0;
const NSRect  kAlgoPrev = { { 360.0, 12.0 }, { 34.0, 30.0 } };
const NSRect  kAlgoNext = { { 706.0, 12.0 }, { 34.0, 30.0 } };
const double  kDropSeconds = 0.35;

// Meter LEDs: OVER, 0, -6, -12, -18, -24 (0 on the scale = 10 dB under full scale, an estimate)
const double kMeterDb[6] = { -0.5, -10.0, -16.0, -22.0, -28.0, -34.0 };
const char* const kMeterNames[6] = { "OVER", "0", "-6", "-12", "-18", "-24" };

enum Mode { kModePlay = 0, kModeEdit, kModeSystem, kModeWrite };

inline CGFloat valueAngle (CGFloat v) { return (CGFloat) ((-150.0 + 300.0 * v) * M_PI / 180.0); }
inline NSPoint polar (CGFloat cx, CGFloat cy, CGFloat r, CGFloat a) { return NSMakePoint (cx + r * std::sin (a), cy - r * std::cos (a)); }

NSColor* rgb (int r, int g, int b) { return [NSColor colorWithCalibratedRed: r / 255.0 green: g / 255.0 blue: b / 255.0 alpha: 1.0]; }
NSColor* rgba (int r, int g, int b, double a) { return [NSColor colorWithCalibratedRed: r / 255.0 green: g / 255.0 blue: b / 255.0 alpha: a]; }
// The unit's colours (from the photo)
NSColor* ink()      { return rgb (214, 214, 210); }      // light grey lettering on the black
NSColor* dimInk()   { return rgb (128, 128, 124); }
NSColor* amber()    { return rgb (236, 150, 44); }       // SDX-330, DIMENSIONAL EXPANDER
NSColor* lcdBack()  { return rgb (150, 178, 64); }       // yellow-green backlight
NSColor* lcdInk()   { return rgb (22, 34, 12); }
NSColor* segOn()    { return rgb (92, 255, 140); }
NSColor* segOff()   { return rgb (18, 44, 26); }

NSFont* boldFont (CGFloat size)
{
    NSFont* f = [NSFont fontWithName: @"Helvetica-Bold" size: size];
    return f ? f : [NSFont boldSystemFontOfSize: size];
}
NSFont* italicBold (CGFloat size)
{
    NSFont* f = [NSFont fontWithName: @"Helvetica-BoldOblique" size: size];
    return f ? f : boldFont (size);
}
NSFont* lcdFont (CGFloat size)
{
    NSFont* f = [NSFont fontWithName: @"Monaco" size: size];
    if (! f) f = [NSFont fontWithName: @"Courier-Bold" size: size];
    return f ? f : [NSFont userFixedPitchFontOfSize: size];
}

NSDictionary* textAttrs (NSFont* font, NSColor* color, CGFloat kern)
{
    return [NSDictionary dictionaryWithObjectsAndKeys:
              font, NSFontAttributeName, color, NSForegroundColorAttributeName,
              [NSNumber numberWithDouble: kern], NSKernAttributeName, nil];
}

void drawText (const char* s, NSFont* font, NSColor* color, CGFloat cx, CGFloat cy, CGFloat kern = 0.0)
{
    NSString* str = [NSString stringWithUTF8String: s];
    if (! str) return;
    NSDictionary* attrs = textAttrs (font, color, kern);
    const NSSize sz = [str sizeWithAttributes: attrs];
    [str drawAtPoint: NSMakePoint (cx - (sz.width - kern) * 0.5, cy - sz.height * 0.5) withAttributes: attrs];
}

void drawTextLeft (const char* s, NSFont* font, NSColor* color, CGFloat x, CGFloat cy, CGFloat kern = 0.0)
{
    NSString* str = [NSString stringWithUTF8String: s];
    if (! str) return;
    NSDictionary* attrs = textAttrs (font, color, kern);
    const NSSize sz = [str sizeWithAttributes: attrs];
    [str drawAtPoint: NSMakePoint (x, cy - sz.height * 0.5) withAttributes: attrs];
}

void line (CGFloat x0, CGFloat y0, CGFloat x1, CGFloat y1, CGFloat w)
{
    NSBezierPath* p = [NSBezierPath bezierPath];
    [p moveToPoint: NSMakePoint (x0, y0)];
    [p lineToPoint: NSMakePoint (x1, y1)];
    [p setLineWidth: w];
    [p stroke];
}

void glowFill (NSBezierPath* p, NSColor* c, CGFloat blur)
{
    [NSGraphicsContext saveGraphicsState];
    NSShadow* sh = [[[NSShadow alloc] init] autorelease];
    [sh setShadowOffset: NSMakeSize (0.0, 0.0)];
    [sh setShadowBlurRadius: blur];
    [sh setShadowColor: c];
    [sh set];
    [c set];
    [p fill];
    [NSGraphicsContext restoreGraphicsState];
}

// Pad / cut a string to n characters
std::string fit (const char* s, int n, bool right = false)
{
    std::string t = s ? s : "";
    if ((int) t.size() > n) t = t.substr (0, (size_t) n);
    while ((int) t.size() < n) t = right ? " " + t : t + " ";
    return t;
}

} // namespace

//==============================================================================
@interface TBSDX330PanelView : NSView <NSTextFieldDelegate>
{
    EditorHost* host;
    float shown[kNumParams];
    float meterL, meterR;
    NSTimer* timer;
    NSTimer* anim;
    NSImage* backdrop;
    // LCD / buttons
    int mode;
    int page;               // edit: first parameter shown = page * 3
    int func;               // edit: 0..2 the selected column
    bool pageMode;          // edit: PAGE lamp lit, the dial turns pages
    bool showAlgo;          // play: second line shows the algorithm instead of a parameter
    int writeDest;
    int pressedBtn;
    char flash[40];         // a short message on the LCD's second line
    int flashTicks;
    // drag
    int dragParam;          // a knob being dragged (-1 none)
    bool dragDial;
    CGFloat dragStartY, dragStartValue, dialAcc, dialAngle;
    // drawer
    double drop, dropFrom, dropTo, dropStart;
    // naming
    NSTextField* nameField;
    NSTextField* retiredField;
    int namingProgram;
}
- (id) initWithFrame: (NSRect) frame host: (EditorHost*) h;
- (void) shutdown;
@end

@implementation TBSDX330PanelView

- (id) initWithFrame: (NSRect) frame host: (EditorHost*) h
{
    self = [super initWithFrame: frame];
    if (self)
    {
        host = h;
        for (int i = 0; i < kNumParams; ++i) shown[i] = host->getParam (i);
        meterL = meterR = 0.0f;
        mode = kModePlay; page = 0; func = 0; pageMode = true; showAlgo = false;
        writeDest = 1; pressedBtn = -1; flash[0] = 0; flashTicks = 0;
        dragParam = -1; dragDial = false; dialAcc = 0.0; dialAngle = 0.0;
        drop = dropFrom = dropTo = 0.0; dropStart = 0.0;
        anim = nil; backdrop = nil; nameField = nil; retiredField = nil; namingProgram = -1;
        host->refreshPrograms();
        timer = [[NSTimer timerWithTimeInterval: 1.0 / 20.0 target: self selector: @selector (tick:)
                                       userInfo: nil repeats: YES] retain];
        [[NSRunLoop currentRunLoop] addTimer: timer forMode: NSDefaultRunLoopMode];
        [[NSRunLoop currentRunLoop] addTimer: timer forMode: NSEventTrackingRunLoopMode];
    }
    return self;
}

- (void) shutdown
{
    [NSObject cancelPreviousPerformRequestsWithTarget: self];
    namingProgram = -1;
    [self dropRetired];
    if (nameField)
    {
        [nameField setDelegate: nil];
        [nameField setTarget: nil];
        [nameField removeFromSuperview];
        [nameField release];
        nameField = nil;
    }
    if (timer) { [timer invalidate]; [timer release]; timer = nil; }
    if (anim) { [anim invalidate]; [anim release]; anim = nil; }
    if (backdrop) { [backdrop release]; backdrop = nil; }
    host = 0;
}

- (void) dealloc
{
    [self shutdown];
    [super dealloc];
}

- (BOOL) isFlipped { return YES; }
- (BOOL) isOpaque { return YES; }
- (BOOL) acceptsFirstMouse: (NSEvent*) e { (void) e; return YES; }

- (void) tick: (NSTimer*) t
{
    (void) t;
    if (! host) return;
    bool changed = false;
    for (int i = 0; i < kNumParams; ++i)
    {
        const float v = host->getParam (i);
        if (v != shown[i]) { shown[i] = v; changed = true; }
    }
    const float l = host->getMeter (0), r = host->getMeter (1);
    const bool lamps = std::fabs (l - meterL) > 0.003f || std::fabs (r - meterR) > 0.003f;
    meterL = l; meterR = r;
    if (flashTicks > 0 && --flashTicks == 0) { flash[0] = 0; changed = true; }
    if (changed) [self setNeedsDisplay: YES];
    else if (lamps) [self setNeedsDisplayInRect: kMeterRect];
}

//------------------------------------------------------------------------------
// Helpers for the LCD's state

- (int) algo { return algoIndex (shown[kAlgo]); }
- (int) programNum { return programNumber (shown[kProgram]); }

// The parameters the LCD's edit pages walk through: the algorithm, its own parameters,
// then the EQ
- (int) editList: (int*) list
{
    int n = 0;
    list[n++] = kAlgo;
    int tmp[kMaxAlgoParams];
    const int m = algoParams ([self algo], tmp);
    for (int k = 0; k < m; ++k) list[n++] = tmp[k];
    for (int k = 0; k < 9; ++k) list[n++] = kEqParams[k];
    return n;
}

- (void) setFlash: (const char*) s
{
    std::snprintf (flash, sizeof (flash), "%s", s);
    flashTicks = 30;
    [self setNeedsDisplay: YES];
}

- (void) lcdLines: (std::string&) l1 second: (std::string&) l2
{
    char name[64] = { 0 }, val[32] = { 0 }, lab[16] = { 0 };
    const int a = [self algo];
    const int pn = [self programNum];
    if (mode == kModeEdit)
    {
        int list[kMaxAlgoParams + 16];
        const int n = [self editList: list];
        l1.clear(); l2.clear();
        for (int c = 0; c < 3; ++c)
        {
            const int k = page * 3 + c;
            if (k < n)
            {
                lcdNameFor (list[k], a, lab, sizeof (lab));
                formatParamFor (list[k], shown[list[k]], a, val, sizeof (val));
                l1 += fit (lab, 5);
                l2 += fit (val, 5, true);
            }
            else { l1 += "     "; l2 += "     "; }
            if (c < 2) { l1 += " "; l2 += " "; }
        }
        return;
    }
    if (mode == kModeSystem)
    {
        formatParam (kDirectMix, shown[kDirectMix], val, sizeof (val));
        l1 = fit ("11:Direct Mix", 17);
        std::snprintf (name, sizeof (name), "  Direct = %s", val);
        l2 = fit (name, 17);
        return;
    }
    if (mode == kModeWrite)
    {
        std::snprintf (name, sizeof (name), "WRITE TO PROG %03d", writeDest);
        l1 = fit (name, 17);
        char dn[32] = { 0 };
        host->programName (writeDest, dn, sizeof (dn));
        l2 = fit (dn, 17);
        return;
    }
    host->programName (pn, name, sizeof (name));
    l1 = fit (name, 17);
    if (flash[0]) { l2 = fit (flash, 17); return; }
    if (showAlgo)
    {
        std::snprintf (val, sizeof (val), "%d:%s", a + 1, kAlgoNames[a]);
        l2 = fit (val, 17);
        return;
    }
    // The "top screen" parameter: the algorithm's first
    int list[kMaxAlgoParams];
    algoParams (a, list);
    lcdNameFor (list[0], a, lab, sizeof (lab));
    formatParamFor (list[0], shown[list[0]], a, val, sizeof (val));
    std::snprintf (name, sizeof (name), "%s %s", lab, val);
    l2 = fit (name, 17, true);
}

//------------------------------------------------------------------------------
// Drawing: the fixed parts, once

- (void) drawEarScrew: (NSPoint) c
{
    // Oval rack slot as on the photo's ears
    NSBezierPath* slot = [NSBezierPath bezierPathWithRoundedRect: NSMakeRect (c.x - 7.0, c.y - 11.0, 14.0, 22.0) xRadius: 7.0 yRadius: 7.0];
    [rgb (4, 4, 4) set];
    [slot fill];
    [rgba (255, 255, 255, 0.10) set];
    [slot setLineWidth: 1.0];
    [slot stroke];
}

- (void) drawKnobFace: (NSPoint) c radius: (CGFloat) r value: (CGFloat) v ridges: (int) ridges
{
    const CGFloat a = valueAngle (v);
    NSBezierPath* body = [NSBezierPath bezierPath];
    for (int k = 0; k <= ridges * 4; ++k)
    {
        const CGFloat t = (CGFloat) (2.0 * M_PI * k / (ridges * 4));
        const CGFloat rr = r * (1.0 - 0.05 * (0.5 - 0.5 * std::cos (ridges * t)));
        const NSPoint p = polar (c.x, c.y, rr, t + a);
        if (k == 0) [body moveToPoint: p]; else [body lineToPoint: p];
    }
    [body closePath];
    [NSGraphicsContext saveGraphicsState];
    NSShadow* sh = [[[NSShadow alloc] init] autorelease];
    [sh setShadowOffset: NSMakeSize (1.5, -3.0)];
    [sh setShadowBlurRadius: 5.0];
    [sh setShadowColor: [NSColor colorWithCalibratedWhite: 0.0 alpha: 0.9]];
    [sh set];
    [rgb (10, 10, 10) set];
    [body fill];
    [NSGraphicsContext restoreGraphicsState];
    NSGradient* g = [[[NSGradient alloc] initWithColorsAndLocations:
                        rgb (86, 86, 86), 0.0, rgb (38, 38, 38), 0.5, rgb (10, 10, 10), 1.0, nil] autorelease];
    [g drawInBezierPath: body relativeCenterPosition: NSMakePoint (-0.35, 0.5)];
    [[NSColor colorWithCalibratedWhite: 0.0 alpha: 0.6] set];
    for (int k = 0; k < ridges; ++k)
    {
        const CGFloat t = (CGFloat) (2.0 * M_PI * k / ridges) + a;
        const NSPoint p0 = polar (c.x, c.y, r * 0.82, t), p1 = polar (c.x, c.y, r * 0.97, t);
        line (p0.x, p0.y, p1.x, p1.y, 0.9);
    }
    // Cap
    const CGFloat rc = r * 0.72;
    NSBezierPath* cap = [NSBezierPath bezierPathWithOvalInRect: NSMakeRect (c.x - rc, c.y - rc, 2 * rc, 2 * rc)];
    NSGradient* cg = [[[NSGradient alloc] initWithColorsAndLocations:
                         rgb (64, 64, 64), 0.0, rgb (26, 26, 26), 0.6, rgb (8, 8, 8), 1.0, nil] autorelease];
    [cg drawInBezierPath: cap relativeCenterPosition: NSMakePoint (-0.3, 0.45)];
    [[NSColor colorWithCalibratedWhite: 0.0 alpha: 0.7] set];
    [cap setLineWidth: 0.8];
    [cap stroke];
    // White position line
    [rgb (236, 236, 232) set];
    NSBezierPath* p = [NSBezierPath bezierPath];
    [p moveToPoint: polar (c.x, c.y, r * 0.30, a)];
    [p lineToPoint: polar (c.x, c.y, r * 0.92, a)];
    [p setLineWidth: r > 20.0 ? 2.6 : 2.0];
    [p setLineCapStyle: NSRoundLineCapStyle];
    [p stroke];
}

- (void) drawScaleAround: (NSPoint) c radius: (CGFloat) r
{
    [ink() set];
    for (int k = 0; k <= 10; ++k)
    {
        const CGFloat a = valueAngle (k / 10.0);
        const NSPoint p0 = polar (c.x, c.y, r + 4.0, a), p1 = polar (c.x, c.y, r + (k % 5 == 0 ? 10.0 : 7.0), a);
        line (p0.x, p0.y, p1.x, p1.y, k % 5 == 0 ? 1.6 : 1.0);
    }
    drawText ("MIN", boldFont (9.0), ink(), c.x - r - 4.0, c.y + r + 10.0);
    drawText ("MAX", boldFont (9.0), ink(), c.x + r + 4.0, c.y + r + 10.0);
}

- (void) drawPlainButton: (NSRect) r pressed: (bool) pressed
{
    NSBezierPath* bez = [NSBezierPath bezierPathWithRoundedRect: NSInsetRect (r, -3.0, -3.0) xRadius: 3.0 yRadius: 3.0];
    [rgb (6, 6, 6) set];
    [bez fill];
    const NSRect cap = pressed ? NSOffsetRect (r, 0.0, 1.5) : r;
    NSBezierPath* bp = [NSBezierPath bezierPathWithRoundedRect: cap xRadius: 2.5 yRadius: 2.5];
    NSGradient* g = [[[NSGradient alloc] initWithStartingColor: pressed ? rgb (54, 54, 54) : rgb (78, 78, 78)
                                                   endingColor: pressed ? rgb (34, 34, 34) : rgb (44, 44, 44)] autorelease];
    [g drawInBezierPath: bp angle: 90.0];
    [rgba (255, 255, 255, pressed ? 0.06 : 0.16) set];
    line (NSMinX (cap) + 3.0, NSMinY (cap) + 1.0, NSMaxX (cap) - 3.0, NSMinY (cap) + 1.0, 1.0);
    [rgba (0, 0, 0, 0.8) set];
    [bp setLineWidth: 1.0];
    [bp stroke];
}

- (NSImage*) backdropImage
{
    if (backdrop) return backdrop;
    const CGFloat w = (CGFloat) kEditorWidth, h = (CGFloat) kEditorHeight;
    backdrop = [[NSImage alloc] initWithSize: NSMakeSize (w, h)];
    [backdrop lockFocusFlipped: YES];

    // Black face, faintly grained, with the rack ears either side
    NSGradient* fg = [[[NSGradient alloc] initWithColorsAndLocations:
                         rgb (34, 34, 34), 0.0, rgb (20, 20, 20), 0.5, rgb (14, 14, 14), 1.0, nil] autorelease];
    [fg drawInRect: NSMakeRect (0, 0, w, h) angle: 90.0];
    unsigned seed = 330u;
    for (int k = 0; k < 14000; ++k)
    {
        seed = seed * 1664525u + 1013904223u; const CGFloat x = (seed >> 8) / 16777216.0 * w;
        seed = seed * 1664525u + 1013904223u; const CGFloat y = (seed >> 8) / 16777216.0 * h;
        seed = seed * 1664525u + 1013904223u; const double q = (seed >> 8) / 16777216.0;
        [[NSColor colorWithCalibratedWhite: q < 0.5 ? 1.0 : 0.0 alpha: 0.025 + 0.04 * q] set];
        NSRectFillUsingOperation (NSMakeRect (x, y, 1.0, 1.0), NSCompositeSourceOver);
    }
    [rgb (10, 10, 10) set];
    NSRectFill (NSMakeRect (0, 0, kEarW, h));
    NSRectFill (NSMakeRect (w - kEarW, 0, kEarW, h));
    [rgba (255, 255, 255, 0.07) set];
    line (kEarW, 0, kEarW, h, 1.0);
    line (w - kEarW, 0, w - kEarW, h, 1.0);
    [self drawEarScrew: NSMakePoint (kEarW * 0.5, 40.0)];
    [self drawEarScrew: NSMakePoint (kEarW * 0.5, (CGFloat) kFaceHeight - 40.0)];
    [self drawEarScrew: NSMakePoint (w - kEarW * 0.5, 40.0)];
    [self drawEarScrew: NSMakePoint (w - kEarW * 0.5, (CGFloat) kFaceHeight - 40.0)];

    // Title: TB logo where the maker's badge was, then the model name in amber
    tblogo::drawTBLogo (NSMakeRect (46.0, 14.0, 112.0, 60.0), rgb (236, 236, 232));
    {
        NSString* s = @"SDX-330";
        NSMutableDictionary* at = [NSMutableDictionary dictionaryWithDictionary: textAttrs (italicBold (40.0), amber(), 3.0)];
        [at setObject: [NSNumber numberWithDouble: -3.0] forKey: NSStrokeWidthAttributeName];   // outlined, filled
        [at setObject: rgb (120, 64, 10) forKey: NSStrokeColorAttributeName];
        [s drawAtPoint: NSMakePoint (180.0, 18.0) withAttributes: at];
        drawTextLeft ("DIMENSIONAL EXPANDER", boldFont (13.0), amber(), 382.0, 44.0, 2.4);
    }
    [rgba (236, 150, 44, 0.55) set];
    line (382.0, 56.0, 610.0, 56.0, 1.0);

    // INPUT / OUTPUT LEVEL
    drawText ("INPUT LEVEL", boldFont (10.5), ink(), kInKnob.x, 104.0, 1.0);
    drawText ("OUTPUT LEVEL", boldFont (10.5), ink(), kOutKnob.x, 104.0, 1.0);
    [self drawScaleAround: kInKnob radius: kInOuterR];
    [self drawScaleAround: kOutKnob radius: kOutR];
    drawText ("L  \xe2\x80\x94  R", boldFont (9.0), ink(), kInKnob.x, kInKnob.y + kInOuterR + 24.0);
    drawText ("inner \xc2\xb7 outer", boldFont (7.5), dimInk(), kInKnob.x, kInKnob.y + kInOuterR + 35.0);

    // Meter window
    {
        NSBezierPath* mw = [NSBezierPath bezierPathWithRect: kMeterRect];
        [rgb (6, 6, 6) set];
        [mw fill];
        [rgba (255, 255, 255, 0.10) set];
        [mw setLineWidth: 1.0];
        [mw stroke];
        drawText ("L INPUT R", boldFont (8.5), ink(), NSMidX (kMeterRect), NSMinY (kMeterRect) + 11.0, 0.5);
        for (int k = 0; k < 6; ++k)
            drawText (kMeterNames[k], boldFont (8.0), ink(), NSMidX (kMeterRect), NSMinY (kMeterRect) + 28.0 + k * 15.0);
        drawText ("MIDI", boldFont (8.0), ink(), NSMidX (kMeterRect) + 8.0, NSMaxY (kMeterRect) - 14.0);
    }

    // Display window (smoked glass), PROGRAM label, FUNC labels
    {
        NSBezierPath* dw = [NSBezierPath bezierPathWithRect: kWindowRect];
        NSGradient* gg = [[[NSGradient alloc] initWithStartingColor: rgb (12, 14, 13) endingColor: rgb (4, 5, 5)] autorelease];
        [gg drawInBezierPath: dw angle: 90.0];
        [rgba (255, 255, 255, 0.12) set];
        [dw setLineWidth: 1.0];
        [dw stroke];
        drawText ("PROGRAM", boldFont (10.0), dimInk(), NSMidX (kDigitsRect), NSMinY (kWindowRect) + 16.0, 1.0);
        for (int c = 0; c < 3; ++c)
        {
            char f[16]; std::snprintf (f, sizeof (f), "FUNC %d", c + 1);
            drawText (f, boldFont (10.0), dimInk(), kLcdX0 + (c * 6 + 2.5) * kLcdCw, NSMinY (kWindowRect) + 16.0, 1.0);
        }
    }

    // Button group outlines (dashed, as on the unit) and labels over the arrows
    {
        [rgba (214, 214, 210, 0.35) set];
        CGFloat dash[2] = { 3.0, 3.0 };
        NSBezierPath* g1 = [NSBezierPath bezierPathWithRect: NSMakeRect (42.0, 256.0, 240.0, 116.0)];
        [g1 setLineDash: dash count: 2 phase: 0.0];
        [g1 setLineWidth: 1.0];
        [g1 stroke];
        NSBezierPath* g2 = [NSBezierPath bezierPathWithRect: NSMakeRect (490.0, 244.0, 440.0, 128.0)];
        [g2 setLineDash: dash count: 2 phase: 0.0];
        [g2 setLineWidth: 1.0];
        [g2 stroke];
        drawText ("POWER", boldFont (10.0), ink(), NSMidX (kPowerRect), NSMinY (kPowerRect) - 12.0, 1.0);
    }

    // Strip: a rule, then the drawer button and the caption (drawn live)
    [rgba (255, 255, 255, 0.14) set];
    line (kEarW, (CGFloat) kFaceHeight + 0.5, w - kEarW, (CGFloat) kFaceHeight + 0.5, 1.0);
    [rgba (0, 0, 0, 0.6) set];
    line (kEarW, (CGFloat) kFaceHeight + 1.5, w - kEarW, (CGFloat) kFaceHeight + 1.5, 1.0);

    [backdrop unlockFocus];
    return backdrop;
}

//------------------------------------------------------------------------------
// Drawing: the live parts

- (void) drawLamp: (NSPoint) c on: (bool) on color: (NSColor*) col
{
    NSBezierPath* p = [NSBezierPath bezierPathWithOvalInRect: NSMakeRect (c.x - 3.5, c.y - 3.5, 7.0, 7.0)];
    if (on) glowFill (p, col, 8.0);
    else { [rgb (40, 14, 12) set]; [p fill]; }
    [rgba (0, 0, 0, 0.8) set];
    [p setLineWidth: 0.6];
    [p stroke];
}

- (void) drawMeter
{
    const double lv[2] = { meterL, meterR };
    for (int s = 0; s < 2; ++s)
    {
        const double db = lv[s] > 1e-6 ? 20.0 * std::log10 (lv[s]) : -120.0;
        const CGFloat x = s == 0 ? NSMinX (kMeterRect) + 12.0 : NSMaxX (kMeterRect) - 12.0;
        for (int k = 0; k < 6; ++k)
        {
            const bool on = db >= kMeterDb[k];
            const NSRect r = NSMakeRect (x - 6.0, NSMinY (kMeterRect) + 25.0 + k * 15.0, 12.0, 6.0);
            NSBezierPath* p = [NSBezierPath bezierPathWithRect: r];
            NSColor* c = k == 0 ? rgb (255, 50, 40) : rgb (80, 240, 100);
            if (on) glowFill (p, c, 6.0);
            else { [k == 0 ? rgb (50, 14, 12) : rgb (14, 40, 18) set]; [p fill]; }
        }
    }
    // MIDI lamp: the plugin takes no MIDI, so it stays dark
    [self drawLamp: NSMakePoint (NSMidX (kMeterRect) - 16.0, NSMaxY (kMeterRect) - 14.0) on: false color: rgb (255, 60, 40)];
}

// 7-segment digit in box r (slanted a little, as LED displays are)
- (void) drawDigit: (char) ch inRect: (NSRect) r dot: (bool) dot
{
    // segments a b c d e f g as bits 0..6
    unsigned m = 0;
    switch (ch)
    {
        case '0': m = 0x3F; break; case '1': m = 0x06; break; case '2': m = 0x5B; break; case '3': m = 0x4F; break;
        case '4': m = 0x66; break; case '5': m = 0x6D; break; case '6': m = 0x7D; break; case '7': m = 0x07; break;
        case '8': m = 0x7F; break; case '9': m = 0x6F; break;
        case 'S': m = 0x6D; break; case 'Y': m = 0x6E; break; case 'E': m = 0x79; break; case 'd': m = 0x5E; break;
        case 't': m = 0x78; break; case 'P': m = 0x73; break; case 'r': m = 0x50; break; case 'n': m = 0x54; break;
        case 'W': m = 0x3E; break; case '-': m = 0x40; break;
        default: m = 0; break;
    }
    const CGFloat x = NSMinX (r), y = NSMinY (r), w = r.size.width, h = r.size.height;
    const CGFloat t = w * 0.17, sl = w * 0.12;          // thickness, slant
    // segment endpoints (before slant) as rectangles with pointed ends
    struct Seg { CGFloat x0, y0, x1, y1; } segs[7] = {
        { t * 0.6, 0.0, w - t * 0.6, 0.0 },              // a
        { w, t * 0.6, w, h * 0.5 - t * 0.4 },            // b
        { w, h * 0.5 + t * 0.4, w, h - t * 0.6 },        // c
        { t * 0.6, h, w - t * 0.6, h },                  // d
        { 0.0, h * 0.5 + t * 0.4, 0.0, h - t * 0.6 },    // e
        { 0.0, t * 0.6, 0.0, h * 0.5 - t * 0.4 },        // f
        { t * 0.6, h * 0.5, w - t * 0.6, h * 0.5 }       // g
    };
    for (int k = 0; k < 7; ++k)
    {
        const Seg& s = segs[k];
        const bool horiz = s.y0 == s.y1;
        NSBezierPath* p = [NSBezierPath bezierPath];
        const CGFloat ht = t * 0.5;
        NSPoint pts[6];
        if (horiz)
        {
            pts[0] = NSMakePoint (s.x0, s.y0);       pts[1] = NSMakePoint (s.x0 + ht, s.y0 - ht);
            pts[2] = NSMakePoint (s.x1 - ht, s.y0 - ht); pts[3] = NSMakePoint (s.x1, s.y0);
            pts[4] = NSMakePoint (s.x1 - ht, s.y0 + ht); pts[5] = NSMakePoint (s.x0 + ht, s.y0 + ht);
        }
        else
        {
            pts[0] = NSMakePoint (s.x0, s.y0);       pts[1] = NSMakePoint (s.x0 + ht, s.y0 + ht);
            pts[2] = NSMakePoint (s.x0 + ht, s.y1 - ht); pts[3] = NSMakePoint (s.x0, s.y1);
            pts[4] = NSMakePoint (s.x0 - ht, s.y1 - ht); pts[5] = NSMakePoint (s.x0 - ht, s.y0 + ht);
        }
        for (int j = 0; j < 6; ++j)
        {
            const NSPoint q = NSMakePoint (x + pts[j].x + sl * (1.0 - pts[j].y / h), y + pts[j].y);
            if (j == 0) [p moveToPoint: q]; else [p lineToPoint: q];
        }
        [p closePath];
        if (m & (1u << k)) glowFill (p, segOn(), 7.0);
        else { [segOff() set]; [p fill]; }
    }
    NSBezierPath* d = [NSBezierPath bezierPathWithOvalInRect: NSMakeRect (x + w + 4.0, y + h - 3.0, 6.0, 6.0)];
    if (dot) glowFill (d, segOn(), 6.0); else { [segOff() set]; [d fill]; }
}

- (void) drawProgramDisplay
{
    char s[8];
    bool dot = false;
    if (mode == kModeSystem)      std::snprintf (s, sizeof (s), "SYS");
    else if (mode == kModeWrite)  std::snprintf (s, sizeof (s), "%03d", writeDest);
    else
    {
        std::snprintf (s, sizeof (s), "%03d", [self programNum]);
        dot = host && host->programEdited();     // the settings have been changed from the program
    }
    const CGFloat dw = 38.0, gap = 12.0;
    for (int k = 0; k < 3; ++k)
        [self drawDigit: s[k] inRect: NSMakeRect (NSMinX (kDigitsRect) + 6.0 + k * (dw + gap), NSMinY (kDigitsRect) + 6.0, dw, 62.0) dot: k == 2 && dot];
}

- (void) drawLcd
{
    // Backlit glass with a bezel
    NSBezierPath* frame = [NSBezierPath bezierPathWithRect: NSInsetRect (kLcdRect, -3.0, -3.0)];
    [rgb (2, 2, 2) set];
    [frame fill];
    NSGradient* bg = [[[NSGradient alloc] initWithColorsAndLocations:
                         rgb (166, 196, 74), 0.0, lcdBack(), 0.5, rgb (126, 152, 50), 1.0, nil] autorelease];
    [bg drawInRect: kLcdRect angle: 90.0];
    // Unlit 5x8 cells, faintly
    [rgba (22, 34, 12, 0.07) set];
    for (int ln = 0; ln < 2; ++ln)
        for (int c = 0; c < 17; ++c)
            NSRectFillUsingOperation (NSMakeRect (kLcdX0 + c * kLcdCw + 1.5, (ln ? kLcdLine2 : kLcdLine1) + 4.0, kLcdCw - 3.0, kLcdLineH - 8.0), NSCompositeSourceOver);
    std::string l1, l2;
    [self lcdLines: l1 second: l2];
    NSFont* f = lcdFont (38.0);
    for (int ln = 0; ln < 2; ++ln)
    {
        const std::string& s = ln ? l2 : l1;
        const CGFloat y = (ln ? kLcdLine2 : kLcdLine1) + kLcdLineH * 0.5;
        for (int c = 0; c < 17 && c < (int) s.size(); ++c)
        {
            char ch[2] = { s[(size_t) c], 0 };
            // edit mode: the selected parameter's name is shown reversed
            const bool rev = mode == kModeEdit && ! pageMode && ln == 0 && c >= func * 6 && c < func * 6 + 5;
            const CGFloat cx = kLcdX0 + (c + 0.5) * kLcdCw;
            if (rev)
            {
                [lcdInk() set];
                NSRectFill (NSMakeRect (kLcdX0 + c * kLcdCw, (ln ? kLcdLine2 : kLcdLine1) + 2.0, kLcdCw, kLcdLineH - 4.0));
            }
            if (ch[0] != ' ') drawText (ch, f, rev ? lcdBack() : lcdInk(), cx, y);
        }
    }
}

- (void) drawButtons
{
    for (int b = 0; b < kNumBtns; ++b)
    {
        const NSRect r = kBtnRects[b];
        [self drawPlainButton: r pressed: pressedBtn == b];
        const CGFloat dy = pressedBtn == b ? 1.5 : 0.0;
        if (b == kBtnUp || b == kBtnDown)
        {
            NSBezierPath* tri = [NSBezierPath bezierPath];
            const CGFloat cx = NSMidX (r), cy = NSMidY (r) + dy;
            if (b == kBtnUp) { [tri moveToPoint: NSMakePoint (cx, cy - 7)]; [tri lineToPoint: NSMakePoint (cx + 8, cy + 6)]; [tri lineToPoint: NSMakePoint (cx - 8, cy + 6)]; }
            else             { [tri moveToPoint: NSMakePoint (cx, cy + 7)]; [tri lineToPoint: NSMakePoint (cx + 8, cy - 6)]; [tri lineToPoint: NSMakePoint (cx - 8, cy - 6)]; }
            [tri closePath];
            [ink() set];
            [tri fill];
        }
        else
            drawText (kBtnNames[b], boldFont (11.0), ink(), NSMidX (r), NSMidY (r) + dy, 1.0);
        if (kBtnHasLamp[b])
        {
            bool on = false;
            if (b == kBtnEdit)   on = mode == kModeEdit;
            if (b == kBtnPage)   on = mode == kModeEdit && pageMode;
            if (b == kBtnSystem) on = mode == kModeSystem;
            if (b == kBtnMemory) on = mode == kModeWrite;
            if (b == kBtnBypass) on = isOn (shown[kBypass]);
            [self drawLamp: NSMakePoint (NSMidX (r), NSMinY (r) - 8.0) on: on color: rgb (255, 48, 36)];
        }
    }
    // POWER: always on in the plugin
    [self drawPlainButton: kPowerRect pressed: true];
    [self drawLamp: NSMakePoint (NSMidX (kPowerRect), NSMidY (kPowerRect) + 1.5) on: true color: rgb (255, 48, 36)];
}

- (void) drawDial
{
    // A big black dial with a finger dimple, turning freely
    const CGFloat r = kDialR;
    NSBezierPath* rim = [NSBezierPath bezierPathWithOvalInRect: NSMakeRect (kDial.x - r - 5, kDial.y - r - 5, 2 * r + 10, 2 * r + 10)];
    [rgb (4, 4, 4) set];
    [rim fill];
    NSBezierPath* body = [NSBezierPath bezierPathWithOvalInRect: NSMakeRect (kDial.x - r, kDial.y - r, 2 * r, 2 * r)];
    [NSGraphicsContext saveGraphicsState];
    NSShadow* sh = [[[NSShadow alloc] init] autorelease];
    [sh setShadowOffset: NSMakeSize (1.5, -3.0)];
    [sh setShadowBlurRadius: 6.0];
    [sh setShadowColor: [NSColor colorWithCalibratedWhite: 0.0 alpha: 0.9]];
    [sh set];
    [rgb (16, 16, 16) set];
    [body fill];
    [NSGraphicsContext restoreGraphicsState];
    NSGradient* g = [[[NSGradient alloc] initWithColorsAndLocations:
                        rgb (70, 70, 70), 0.0, rgb (30, 30, 30), 0.55, rgb (12, 12, 12), 1.0, nil] autorelease];
    [g drawInBezierPath: body relativeCenterPosition: NSMakePoint (-0.3, 0.45)];
    [[NSColor colorWithCalibratedWhite: 0.0 alpha: 0.5] set];
    for (int k = 0; k < 60; ++k)
    {
        const CGFloat t = (CGFloat) (2.0 * M_PI * k / 60.0) + dialAngle;
        const NSPoint p0 = polar (kDial.x, kDial.y, r * 0.92, t), p1 = polar (kDial.x, kDial.y, r * 0.99, t);
        line (p0.x, p0.y, p1.x, p1.y, 1.0);
    }
    const NSPoint d = polar (kDial.x, kDial.y, r * 0.62, dialAngle);
    NSBezierPath* dimple = [NSBezierPath bezierPathWithOvalInRect: NSMakeRect (d.x - 9, d.y - 9, 18, 18)];
    NSGradient* dg = [[[NSGradient alloc] initWithStartingColor: rgb (8, 8, 8) endingColor: rgb (60, 60, 60)] autorelease];
    [dg drawInBezierPath: dimple angle: -60.0];
}

- (void) drawStrip
{
    [self drawPlainButton: kDrawerBtn pressed: dropTo > 0.5];
    char cap[64];
    std::snprintf (cap, sizeof (cap), "%s PARAMETERS", dropTo > 0.5 ? "\xe2\x96\xb2" : "\xe2\x96\xbc");
    drawText (cap, boldFont (12.0), ink(), NSMidX (kDrawerBtn), NSMidY (kDrawerBtn), 1.0);
    const int a = [self algo];
    std::snprintf (cap, sizeof (cap), "ALGORITHM %d  \xc2\xb7  %s", a + 1, kAlgoNames[a]);
    drawTextLeft (cap, boldFont (13.0), ink(), 270.0, NSMidY (kDrawerBtn), 1.2);
    char pn[64] = { 0 };
    host->programName ([self programNum], pn, sizeof (pn));
    char right[96];
    std::snprintf (right, sizeof (right), "%03d  %s%s", [self programNum], pn, host->programEdited() ? "  (EDITED)" : "");
    NSDictionary* at = textAttrs (boldFont (12.0), dimInk(), 1.0);
    NSString* rs = [NSString stringWithUTF8String: right];
    if (rs)
    {
        const NSSize sz = [rs sizeWithAttributes: at];
        [rs drawAtPoint: NSMakePoint ((CGFloat) kEditorWidth - kEarW - 20.0 - sz.width, NSMidY (kDrawerBtn) - sz.height * 0.5) withAttributes: at];
    }
}

// Drawer geometry: knob k of the current list
- (NSPoint) cellCentre: (int) k eq: (bool) eq
{
    const CGFloat off = [self drawerOffset];
    if (eq) return NSMakePoint (kEarW + kGridX + (k + 3) * kCellW + kCellW * 0.5, kEqY + 36.0 + off);
    return NSMakePoint (kEarW + kGridX + (k % kPerRow) * kCellW + kCellW * 0.5, kGridY + (k / kPerRow) * kCellH + 36.0 + off);
}

- (CGFloat) drawerOffset { return (CGFloat) (-(1.0 - drop) * ((CGFloat) kFaceHeight + 8.0)); }

- (void) drawSmallKnob: (int) p at: (NSPoint) c
{
    char lab[16], val[32];
    lcdNameFor (p, [self algo], lab, sizeof (lab));
    formatParamFor (p, shown[p], [self algo], val, sizeof (val));
    drawText (lab, boldFont (9.5), ink(), c.x, c.y - kSmallR - 12.0, 0.4);
    [ink() set];
    for (int k = 0; k <= 10; ++k)
    {
        const CGFloat a = valueAngle (k / 10.0);
        const NSPoint p0 = polar (c.x, c.y, kSmallR + 3.0, a), p1 = polar (c.x, c.y, kSmallR + 6.0, a);
        line (p0.x, p0.y, p1.x, p1.y, 0.9);
    }
    [self drawKnobFace: c radius: kSmallR value: shown[p] ridges: 18];
    drawText (val, lcdFont (10.5), rgb (180, 220, 90), c.x, c.y + kSmallR + 12.0);
}

- (void) drawDrawer
{
    if (drop <= 0.001) return;
    const CGFloat off = [self drawerOffset];
    const NSRect r = NSOffsetRect (kDrawerRect, 0.0, off);
    [NSGraphicsContext saveGraphicsState];
    NSShadow* sh = [[[NSShadow alloc] init] autorelease];
    [sh setShadowOffset: NSMakeSize (0.0, -6.0)];
    [sh setShadowBlurRadius: 10.0];
    [sh setShadowColor: [NSColor colorWithCalibratedWhite: 0.0 alpha: 0.9]];
    [sh set];
    [rgb (28, 29, 30) set];
    NSRectFill (r);
    [NSGraphicsContext restoreGraphicsState];
    NSGradient* g = [[[NSGradient alloc] initWithStartingColor: rgb (44, 45, 46) endingColor: rgb (24, 25, 26)] autorelease];
    [g drawInRect: r angle: 90.0];
    [rgba (255, 255, 255, 0.12) set];
    NSFrameRectWithWidth (r, 1.0);

    const int a = [self algo];
    [self drawPlainButton: NSOffsetRect (kAlgoPrev, 0.0, off) pressed: false];
    [self drawPlainButton: NSOffsetRect (kAlgoNext, 0.0, off) pressed: false];
    drawText ("\xe2\x97\x80", boldFont (12.0), ink(), NSMidX (kAlgoPrev), NSMidY (kAlgoPrev) + off);
    drawText ("\xe2\x96\xb6", boldFont (12.0), ink(), NSMidX (kAlgoNext), NSMidY (kAlgoNext) + off);
    char t[64];
    std::snprintf (t, sizeof (t), "ALGORITHM %d : %s", a + 1, kAlgoNames[a]);
    drawText (t, boldFont (15.0), amber(), (NSMinX (kAlgoPrev) + NSMaxX (kAlgoNext)) * 0.5, NSMidY (kAlgoPrev) + off, 1.5);

    int list[kMaxAlgoParams];
    const int n = algoParams (a, list);
    for (int k = 0; k < n; ++k) [self drawSmallKnob: list[k] at: [self cellCentre: k eq: false]];
    [rgba (255, 255, 255, 0.12) set];
    line (NSMinX (r) + 20.0, kEqY + off, NSMaxX (r) - 20.0, kEqY + off, 1.0);
    drawText ("3 BAND EQ", boldFont (11.0), amber(), kEarW + kGridX + kCellW, kEqY + 36.0 + off, 1.2);
    for (int k = 0; k < 9; ++k) [self drawSmallKnob: kEqParams[k] at: [self cellCentre: k eq: true]];
}

- (void) drawRect: (NSRect) dirty
{
    (void) dirty;
    if (! host) return;
    [[self backdropImage] drawInRect: [self bounds] fromRect: NSZeroRect operation: NSCompositeCopy fraction: 1.0
                       respectFlipped: YES hints: nil];
    // the two level knobs (concentric INPUT: inner L, outer R)
    [self drawKnobFace: kInKnob radius: kInOuterR value: shown[kInputR] ridges: 40];
    [self drawKnobFace: kInKnob radius: kInInnerR value: shown[kInputL] ridges: 24];
    [self drawKnobFace: kOutKnob radius: kOutR value: shown[kOutput] ridges: 32];
    [self drawMeter];
    [self drawProgramDisplay];
    [self drawLcd];
    [self drawButtons];
    [self drawDial];
    [self drawDrawer];
    [self drawStrip];
}

//------------------------------------------------------------------------------
// Drawer animation

- (void) toggleDrawer
{
    dropFrom = drop;
    dropTo = dropTo > 0.5 ? 0.0 : 1.0;
    dropStart = [NSDate timeIntervalSinceReferenceDate];
    if (! anim)
    {
        anim = [[NSTimer timerWithTimeInterval: 1.0 / 60.0 target: self selector: @selector (animate:)
                                      userInfo: nil repeats: YES] retain];
        [[NSRunLoop currentRunLoop] addTimer: anim forMode: NSDefaultRunLoopMode];
        [[NSRunLoop currentRunLoop] addTimer: anim forMode: NSEventTrackingRunLoopMode];
    }
    [self setNeedsDisplay: YES];
}

- (void) animate: (NSTimer*) t
{
    (void) t;
    double u = ([NSDate timeIntervalSinceReferenceDate] - dropStart) / (kDropSeconds * std::fabs (dropTo - dropFrom) + 1.0e-6);
    if (u >= 1.0) u = 1.0;
    const double e = u * u * (3.0 - 2.0 * u);
    drop = dropFrom + (dropTo - dropFrom) * e;
    if (u >= 1.0)
    {
        drop = dropTo;
        if (anim) { [anim invalidate]; [anim release]; anim = nil; }
    }
    [self setNeedsDisplay: YES];
}

// For tests: open or close the drawer at once
- (void) setDrawerOpen: (BOOL) open
{
    if (anim) { [anim invalidate]; [anim release]; anim = nil; }
    drop = dropTo = dropFrom = open ? 1.0 : 0.0;
    [self setNeedsDisplay: YES];
}

//------------------------------------------------------------------------------
// Parameters

- (void) setParam: (int) p value: (float) v
{
    if (! host) return;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    if (kInfo[p].taper == tChoice) v = (float) toNorm (p, toReal (p, v));     // choices click into place
    host->guiSetParam (p, v);
    shown[p] = host->getParam (p);
    [self setNeedsDisplay: YES];
}

- (void) stepParam: (int) p dir: (int) dir
{
    if (! host) return;
    float v = host->getParam (p);
    if (p == kLfo || (p >= kB1Lfo && p <= kB4Lfo && (p - kB1Lfo) % 6 == 0))
    {
        // only the waveforms this algorithm offers
        int w = toIndex (p, v);
        for (int k = 0; k < 8; ++k)
        {
            w += dir;
            if (w < 0 || w > (int) kInfo[p].hi) { w -= dir; break; }
            if (p != kLfo || lfoAllowed ([self algo], w)) break;
        }
        if (p == kLfo && ! lfoAllowed ([self algo], w)) return;
        v = (float) toNorm (p, w);
    }
    else if (p == kMode)
    {
        int m = modeFor ([self algo], v) + dir;
        const int nm = modeCount ([self algo]);
        if (m < 0) m = 0;
        if (m > nm - 1) m = nm - 1;
        v = (float) toNorm (p, m);
    }
    else
        v = stepParam (p, v, dir);
    host->guiBeginEdit (p);
    [self setParam: p value: v];
    host->guiEndEdit (p);
}

//------------------------------------------------------------------------------
// The alpha dial and arrows: what they do depends on the mode

- (void) dialStep: (int) dir
{
    if (! host || dir == 0) return;
    dialAngle += (CGFloat) (dir * 0.18);
    if (mode == kModePlay)
    {
        int n = [self programNum] + dir;
        if (n < 1) n = kNumPrograms;
        if (n > kNumPrograms) n = 1;
        host->loadProgram (n);
        for (int i = 0; i < kNumParams; ++i) shown[i] = host->getParam (i);
        flash[0] = 0;
    }
    else if (mode == kModeEdit)
    {
        int list[kMaxAlgoParams + 16];
        const int n = [self editList: list];
        const int pages = (n + 2) / 3;
        if (pageMode)
        {
            page += dir;
            if (page < 0) page = 0;
            if (page > pages - 1) page = pages - 1;
        }
        else
        {
            const int k = page * 3 + func;
            if (k < n) [self stepParam: list[k] dir: dir];
            // the algorithm may have changed: keep the page on the list
            const int n2 = [self editList: list];
            if (page > (n2 + 2) / 3 - 1) page = (n2 + 2) / 3 - 1;
        }
    }
    else if (mode == kModeSystem)
        [self stepParam: kDirectMix dir: dir];
    else if (mode == kModeWrite)
    {
        writeDest += dir;
        if (writeDest < 1) writeDest = kNumUser;
        if (writeDest > kNumUser) writeDest = 1;
    }
    [self setNeedsDisplay: YES];
}

- (void) pressButton: (int) b
{
    if (! host) return;
    flash[0] = 0;
    switch (b)
    {
        case kBtnProgram:
            if (mode == kModePlay) showAlgo = ! showAlgo;   // which second line play mode shows
            else mode = kModePlay;
            break;
        case kBtnFunc1: case kBtnFunc2: case kBtnFunc3:
            if (mode == kModeEdit)
            {
                int list[kMaxAlgoParams + 16];
                const int n = [self editList: list];
                const int f = b - kBtnFunc1;
                if (page * 3 + f < n) { func = f; pageMode = false; }
            }
            break;
        case kBtnUp:   [self dialStep: +1]; break;
        case kBtnDown: [self dialStep: -1]; break;
        case kBtnEdit:
            if (mode == kModeEdit) mode = kModePlay;
            else { mode = kModeEdit; page = 0; func = 0; pageMode = true; }
            break;
        case kBtnSystem:
            mode = mode == kModeSystem ? kModePlay : kModeSystem;
            break;
        case kBtnExit:
            mode = kModePlay;
            break;
        case kBtnPage:
            if (mode == kModeEdit) pageMode = true;
            break;
        case kBtnMemory:
            if (mode == kModeWrite)
            {
                const int dest = writeDest;
                const bool ok = host->writeProgram (dest);
                host->refreshPrograms();
                for (int i = 0; i < kNumParams; ++i) shown[i] = host->getParam (i);
                mode = kModePlay;
                if (ok) [self startNaming: dest];
                else    [self setFlash: "WRITE FAILED"];
            }
            else
            {
                const int pn = [self programNum];
                writeDest = ProgramStore::isUser (pn) ? pn : 1;
                mode = kModeWrite;
            }
            break;
        case kBtnBypass:
            host->guiBeginEdit (kBypass);
            [self setParam: kBypass value: isOn (shown[kBypass]) ? 0.0f : 1.0f];
            host->guiEndEdit (kBypass);
            break;
        default: break;
    }
    [self setNeedsDisplay: YES];
}

//------------------------------------------------------------------------------
// Naming a user program, over the LCD's first line: after a write, or a click on the LCD
// while a written user program is in play mode. Return keeps the name, Esc the old one.

- (void) dropRetired
{
    if (! retiredField) return;
    [retiredField setDelegate: nil];
    [retiredField setTarget: nil];
    [retiredField removeFromSuperview];
    [retiredField release];
    retiredField = nil;
    if (! nameField && [self window]) [[self window] makeFirstResponder: self];
    [self setNeedsDisplay: YES];
}

- (void) startNaming: (int) number
{
    if (! host || ! ProgramStore::isUser (number)) return;
    if (nameField) [self finishNaming: YES];
    namingProgram = number;
    char name[64] = { 0 };
    host->programName (number, name, (int) sizeof (name));
    nameField = [[NSTextField alloc] initWithFrame: NSMakeRect (NSMinX (kLcdRect) + 4.0, kLcdLine1 + 2.0, kLcdRect.size.width - 8.0, kLcdLineH - 2.0)];
    [nameField setBezeled: NO];
    [nameField setBordered: NO];
    [nameField setDrawsBackground: YES];
    [nameField setBackgroundColor: rgb (176, 206, 90)];
    [nameField setTextColor: lcdInk()];
    [nameField setFont: lcdFont (30.0)];
    [nameField setAlignment: NSLeftTextAlignment];
    [nameField setFocusRingType: NSFocusRingTypeNone];
    [nameField setStringValue: [NSString stringWithUTF8String: name] ? [NSString stringWithUTF8String: name] : @""];
    char hint[64];
    std::snprintf (hint, sizeof (hint), "NAME %03d, RETURN", number);
    [[nameField cell] setPlaceholderString: [NSString stringWithUTF8String: hint]];
    [[nameField cell] setScrollable: YES];
    [nameField setTarget: self];
    [nameField setAction: @selector (nameEntered:)];
    [nameField setDelegate: self];
    [self addSubview: nameField];
    [[self window] makeFirstResponder: nameField];
    [self setNeedsDisplay: YES];
}

- (void) finishNaming: (BOOL) keep
{
    const int number = namingProgram;
    if (number < 0) return;
    namingProgram = -1;
    if (keep && host && nameField)
    {
        const char* utf8 = [[nameField stringValue] UTF8String];
        host->renameProgram (number, utf8 ? utf8 : "");
    }
    char msg[40];
    std::snprintf (msg, sizeof (msg), "WRITTEN TO %03d", number);
    [self setFlash: msg];
    [self dropRetired];
    retiredField = nameField;
    nameField = nil;
    [retiredField setDelegate: nil];
    [retiredField setTarget: nil];
    [self performSelector: @selector (dropRetired) withObject: nil afterDelay: 0.0];
}

- (void) nameEntered: (id) sender { (void) sender; [self finishNaming: YES]; }
- (void) controlTextDidEndEditing: (NSNotification*) n { (void) n; [self finishNaming: YES]; }
- (void) controlTextDidChange: (NSNotification*) n
{
    (void) n;
    // the LCD holds 17 characters
    if (! nameField) return;
    NSString* s = [nameField stringValue];
    if ([s length] > (NSUInteger) kMaxProgramName) [nameField setStringValue: [s substringToIndex: (NSUInteger) kMaxProgramName]];
}
- (BOOL) control: (NSControl*) c textView: (NSTextView*) tv doCommandBySelector: (SEL) sel
{
    (void) c; (void) tv;
    if (sel == @selector (cancelOperation:)) { [self finishNaming: NO]; return YES; }
    return NO;
}

//------------------------------------------------------------------------------
// Mouse

// A drawer knob under the point, or -1
- (int) drawerParamAt: (NSPoint) p
{
    if (drop < 0.999) return -1;
    int list[kMaxAlgoParams];
    const int n = algoParams ([self algo], list);
    for (int k = 0; k < n; ++k)
    {
        const NSPoint c = [self cellCentre: k eq: false];
        if ((p.x - c.x) * (p.x - c.x) + (p.y - c.y) * (p.y - c.y) < (kSmallR + 9.0) * (kSmallR + 9.0)) return list[k];
    }
    for (int k = 0; k < 9; ++k)
    {
        const NSPoint c = [self cellCentre: k eq: true];
        if ((p.x - c.x) * (p.x - c.x) + (p.y - c.y) * (p.y - c.y) < (kSmallR + 9.0) * (kSmallR + 9.0)) return kEqParams[k];
    }
    return -1;
}

- (int) knobAt: (NSPoint) p
{
    const CGFloat di = std::sqrt ((p.x - kInKnob.x) * (p.x - kInKnob.x) + (p.y - kInKnob.y) * (p.y - kInKnob.y));
    if (di < kInInnerR + 2.0) return kInputL;
    if (di < kInOuterR + 8.0) return kInputR;
    const CGFloat d2 = std::sqrt ((p.x - kOutKnob.x) * (p.x - kOutKnob.x) + (p.y - kOutKnob.y) * (p.y - kOutKnob.y));
    if (d2 < kOutR + 8.0) return kOutput;
    return -1;
}

- (void) mouseDown: (NSEvent*) e
{
    const NSPoint p = [self convertPoint: [e locationInWindow] fromView: nil];
    if (! host) return;
    if (nameField) [self finishNaming: YES];
    // the live values, even between timer ticks (the host may have just changed them)
    for (int i = 0; i < kNumParams; ++i) shown[i] = host->getParam (i);

    if (NSPointInRect (p, kDrawerBtn)) { [self toggleDrawer]; return; }

    if (drop > 0.001)
    {
        // The drawer covers the face
        const CGFloat off = [self drawerOffset];
        if (! NSPointInRect (p, NSOffsetRect (kDrawerRect, 0.0, off))) return;
        if (NSPointInRect (p, NSOffsetRect (kAlgoPrev, 0.0, off)) || NSPointInRect (p, NSOffsetRect (kAlgoNext, 0.0, off)))
        {
            int a = [self algo] + (NSPointInRect (p, NSOffsetRect (kAlgoNext, 0.0, off)) ? 1 : -1);
            if (a < 0) a = kNumAlgos - 1;
            if (a >= kNumAlgos) a = 0;
            host->guiBeginEdit (kAlgo);
            [self setParam: kAlgo value: algoValue (a)];
            host->guiEndEdit (kAlgo);
            return;
        }
        const int dp = [self drawerParamAt: p];
        if (dp < 0) return;
        shown[dp] = host->getParam (dp);
        host->guiBeginEdit (dp);
        if ([e clickCount] == 2) [self setParam: dp value: defaultNorm (dp)];
        dragParam = dp;
        dragStartY = p.y;
        dragStartValue = shown[dp];
        return;
    }

    for (int b = 0; b < kNumBtns; ++b)
        if (NSPointInRect (p, NSInsetRect (kBtnRects[b], -2.0, -2.0)))
        {
            pressedBtn = b;
            [self pressButton: b];
            return;
        }

    const CGFloat dd = std::sqrt ((p.x - kDial.x) * (p.x - kDial.x) + (p.y - kDial.y) * (p.y - kDial.y));
    if (dd < kDialR + 6.0)
    {
        dragDial = true;
        dragStartY = p.y;
        dialAcc = 0.0;
        return;
    }

    if (NSPointInRect (p, kLcdRect) && mode == kModePlay)
    {
        const int pn = [self programNum];
        if (ProgramStore::isUser (pn)) { host->refreshPrograms(); [self startNaming: pn]; }
        return;
    }

    const int k = [self knobAt: p];
    if (k >= 0)
    {
        shown[k] = host->getParam (k);     // the live value, even between timer ticks
        host->guiBeginEdit (k);
        if ([e clickCount] == 2) [self setParam: k value: defaultNorm (k)];
        dragParam = k;
        dragStartY = p.y;
        dragStartValue = shown[k];
    }
}

- (void) mouseDragged: (NSEvent*) e
{
    const NSPoint p = [self convertPoint: [e locationInWindow] fromView: nil];
    if (dragDial)
    {
        dialAcc += dragStartY - p.y;
        dragStartY = p.y;
        while (dialAcc >= 10.0)  { dialAcc -= 10.0; [self dialStep: +1]; }
        while (dialAcc <= -10.0) { dialAcc += 10.0; [self dialStep: -1]; }
        return;
    }
    if (dragParam < 0 || ! host) return;
    const bool fine = ([e modifierFlags] & NSShiftKeyMask) != 0;
    const CGFloat perPixel = fine ? 1.0 / 1000.0 : 1.0 / 200.0;
    [self setParam: dragParam value: (float) (dragStartValue + (dragStartY - p.y) * perPixel)];
}

- (void) mouseUp: (NSEvent*) e
{
    (void) e;
    if (dragParam >= 0 && host) host->guiEndEdit (dragParam);
    dragParam = -1;
    dragDial = false;
    if (pressedBtn >= 0) { pressedBtn = -1; [self setNeedsDisplay: YES]; }
}

- (void) scrollWheel: (NSEvent*) e
{
    const NSPoint p = [self convertPoint: [e locationInWindow] fromView: nil];
    if (! host) return;
    const CGFloat d = [e deltaY];
    if (d == 0.0) return;
    if (drop > 0.999)
    {
        const int dp = [self drawerParamAt: p];
        if (dp >= 0)
        {
            if (kInfo[dp].taper == tChoice) [self stepParam: dp dir: d > 0 ? 1 : -1];
            else { host->guiBeginEdit (dp); [self setParam: dp value: (float) (shown[dp] + d * 0.01)]; host->guiEndEdit (dp); }
        }
        return;
    }
    const CGFloat dd = std::sqrt ((p.x - kDial.x) * (p.x - kDial.x) + (p.y - kDial.y) * (p.y - kDial.y));
    if (dd < kDialR + 6.0) { [self dialStep: d > 0 ? 1 : -1]; return; }
    const int k = [self knobAt: p];
    if (k < 0) return;
    host->guiBeginEdit (k);
    [self setParam: k value: (float) (shown[k] + d * 0.01)];
    host->guiEndEdit (k);
}

@end

//==============================================================================
namespace tbsdx
{

void* editorOpen (void* parentView, EditorHost* host)
{
    NSView* parent = (NSView*) parentView;
    if (! parent || ! host) return 0;
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    TBSDX330PanelView* v = [[TBSDX330PanelView alloc]
                              initWithFrame: NSMakeRect (0, 0, kEditorWidth, kEditorHeight) host: host];
    if (v) [parent addSubview: v];
    [pool release];
    return (void*) v;    // retained until editorClose
}

void editorClose (void* handle)
{
    TBSDX330PanelView* v = (TBSDX330PanelView*) handle;
    if (! v) return;
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    [v shutdown];
    [v removeFromSuperview];
    [v release];
    [pool release];
}

} // namespace tbsdx
