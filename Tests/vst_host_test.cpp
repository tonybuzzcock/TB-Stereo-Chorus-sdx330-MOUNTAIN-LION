// vst_host_test.cpp
// A minimal VST 2.4 host that loads the built plugin binary through its entry point and
// checks it behaves: opcodes, parameters, processing, BYPASS, in-place buffers, block
// sizes, sample-rate changes, automation and shutdown.
//
//   vst_host_test <path to plugin binary>
// On macOS pass the binary inside the bundle: "TB SDX-330.vst/Contents/MacOS/TBSDX330"

#include "../Source/VST2/vst2_abi.h"
#include "../Source/VST2/Params.h"
#include "../Source/DSP/Sdx330.h"
#include "../Source/DSP/Factory.h"

#include <dlfcn.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <vector>

using namespace tbsdx;

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("  %s  %s\n", ok ? "ok  " : "FAIL", what);
    if (! ok) ++failures;
}

static int automateCalls = 0;
static VstIntPtr VST_CALL hostCallback (AEffect*, VstInt32 opcode, VstInt32, VstIntPtr, void*, float)
{
    switch (opcode)
    {
        case audioMasterVersion:   return 2400;
        case audioMasterAutomate:  ++automateCalls; return 0;
        default:                   return 0;
    }
}

typedef AEffect* (*EntryProc) (audioMasterCallback);

static VstIntPtr call (AEffect* e, VstInt32 op, VstInt32 idx = 0, VstIntPtr val = 0, void* ptr = 0, float opt = 0.0f)
{
    return e->dispatcher (e, op, idx, val, ptr, opt);
}

static bool finite (const std::vector<float>& x)
{
    for (size_t i = 0; i < x.size(); ++i) if (! (x[i] == x[i]) || std::fabs (x[i]) > 100.0f) return false;
    return true;
}

static void runBlocks (AEffect* e, std::vector<float>& l, std::vector<float>& r, std::vector<float>& ol, std::vector<float>& orr, int block)
{
    for (size_t pos = 0; pos < l.size(); pos += block)
    {
        const int n = (int) ((l.size() - pos) < (size_t) block ? l.size() - pos : block);
        float* in[2]  = { &l[pos], &r[pos] };
        float* out[2] = { &ol[pos], &orr[pos] };
        e->processReplacing (e, in, out, n);
    }
}

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("usage: %s <plugin binary>\n", argv[0]); return 2; }
    setenv ("TBSDX_PROGRAM_FILE", "/tmp/tbsdx_host_test_programs.txt", 1);

    std::printf ("Loading %s\n", argv[1]);
    void* lib = dlopen (argv[1], RTLD_NOW | RTLD_LOCAL);
    if (! lib) { std::printf ("dlopen failed: %s\n", dlerror()); return 1; }
    EntryProc entry = (EntryProc) dlsym (lib, "VSTPluginMain");
    check (entry != 0, "exports VSTPluginMain");
#if defined(__APPLE__)
    check (dlsym (lib, "main_macho") != 0, "exports main_macho (older Mac hosts)");
#endif
    if (! entry) return 1;
    AEffect* e = entry (hostCallback);
    check (e != 0, "entry point returns an effect");
    if (! e) return 1;

    std::printf ("\nIdentity\n");
    check (e->magic == kEffectMagic, "magic is 'VstP'");
    check (e->numInputs == 2 && e->numOutputs == 2, "stereo in/out");
    check (e->numParams == kNumParams, "every parameter of the 16 algorithms, the EQ and the panel");
    check ((e->flags & effFlagsCanReplacing) != 0, "processReplacing supported");
    check ((e->flags & effFlagsIsSynth) == 0, "not flagged as an instrument");
    check (e->initialDelay == 0, "no latency");
    check (call (e, effGetTailSize) > 0, "reports a tail");
    check (e->uniqueID == VST_FOURCC ('T', 'B', 'd', 'x'), "unique ID 'TBdx'");
    char buf[256];
    std::memset (buf, 0, sizeof (buf));
    call (e, effGetEffectName, 0, 0, buf);
    std::printf ("  name: %s, %d parameters\n", buf, e->numParams);
    check (std::strcmp (buf, "TB SDX-330") == 0, "effect name");
    check (call (e, effGetPlugCategory) == kPlugCategEffect, "category: effect");
    check (call (e, effGetVstVersion) == 2400, "VST 2.4");
    check (call (e, effCanDo, 0, 0, (void*) "receiveVstMidiEvent") == -1, "declines MIDI");

    call (e, effOpen);
    call (e, effSetSampleRate, 0, 0, 0, 44100.0f);
    call (e, effSetBlockSize, 0, 512);

    std::printf ("\nParameters\n");
    {
        bool lengths = true;
        for (int i = 0; i < kNumParams; ++i)
        {
            char name[64] = { 0 }, disp[64] = { 0 }, label[64] = { 0 };
            call (e, effGetParamName, i, 0, name);
            call (e, effGetParamDisplay, i, 0, disp);
            call (e, effGetParamLabel, i, 0, label);
            if (i < 12 || i == kRate || i == kPrDly) std::printf ("  %-8s = %-6s %-4s (%.3f)\n", name, disp, label, e->getParameter (e, i));
            if (std::strcmp (name, kInfo[i].vst) || std::strlen (name) > 8 || std::strlen (disp) > 8 || std::strlen (label) > 8) lengths = false;
        }
        check (lengths, "names, displays and labels fit 8 characters");
        char d[64] = { 0 };
        call (e, effGetParamDisplay, kProgram, 0, d);
        check (! std::strcmp (d, "201"), "powers on at program 201, as the unit");
        call (e, effGetParamDisplay, kRate, 0, d);
        check (! std::strcmp (d, "0.65"), "201 STEREO CHORUS Rate 0.65");
        call (e, effGetParamDisplay, kInputL, 0, d);
        check (! std::strcmp (d, "5.0"), "INPUT LEVEL at 5 (unity)");
        call (e, effGetParamDisplay, kBypass, 0, d);
        check (! std::strcmp (d, "In"), "BYPASS off");
        e->setParameter (e, kOutput, 0.25f);
        check (std::fabs (e->getParameter (e, kOutput) - 0.25f) < 1e-6f, "set/get round trip");
        e->setParameter (e, kOutput, 7.0f);
        check (e->getParameter (e, kOutput) == 1.0f, "out-of-range values are clamped");
        e->setParameter (e, kOutput, 0.5f);
        e->setParameter (e, 9999, 0.5f);
        check (e->getParameter (e, 9999) == 0.0f, "invalid parameter index is harmless");
    }

    std::printf ("\nProcessing\n");
    call (e, effMainsChanged, 0, 1);
    const int sr = 44100;
    std::vector<float> l (sr), r (sr), ol (sr), orr (sr);
    for (int i = 0; i < sr; ++i) { l[i] = (float) (0.2 * std::sin (2.0 * kPi * 196.0 * i / sr)); r[i] = (float) (0.2 * std::sin (2.0 * kPi * 247.0 * i / sr)); }

    // Reference: the DSP core with program 201's settings
    {
        Sdx330* ref = new Sdx330();
        float v[kNumParams];
        for (int i = 0; i < kNumParams; ++i) v[i] = defaultNorm (i);
        factoryValues (0, v);
        v[kProgram] = programValue (201);
        for (int i = 0; i < kNumParams; ++i) ref->setParam (i, v[i]);
        ref->prepare (sr);
        std::vector<float> rl (sr), rr (sr);
        for (int pos = 0; pos < sr; pos += 512)
        {
            const int n = std::min (512, sr - pos);
            ref->process (&l[pos], &r[pos], &rl[pos], &rr[pos], n);
        }
        delete ref;
        runBlocks (e, l, r, ol, orr, 512);
        double maxDiff = 0.0, pk = 0.0;
        for (int i = 0; i < sr; ++i)
        {
            maxDiff = std::max (maxDiff, (double) std::max (std::fabs (ol[i] - rl[i]), std::fabs (orr[i] - rr[i])));
            pk = std::max (pk, (double) std::fabs (ol[i]));
        }
        std::printf ("  output peak %.3f, max diff vs DSP core %.2g\n", pk, maxDiff);
        check (pk > 0.05, "produces output");
        check (maxDiff < 1e-6, "matches the DSP core sample for sample");
        check (finite (ol) && finite (orr), "output finite");
    }

    // In place, odd blocks, tiny and huge blocks
    {
        std::vector<float> a (l), b (r);
        for (size_t pos = 0; pos < a.size(); pos += 333)
        {
            const int n = (int) ((a.size() - pos) < 333 ? a.size() - pos : 333);
            float* io[2] = { &a[pos], &b[pos] };
            e->processReplacing (e, io, io, n);
        }
        check (finite (a) && finite (b), "in-place processing, odd block size");
        std::vector<float> oa (sr), ob (sr), c (l), d (r);
        runBlocks (e, c, d, oa, ob, 1);
        check (finite (oa) && finite (ob), "1-sample blocks");
        runBlocks (e, c, d, oa, ob, 8192);
        check (finite (oa) && finite (ob), "8192-sample blocks");
    }

    // Every algorithm through the plugin; silence then settles
    {
        bool all = true;
        for (int a = 0; a < kNumAlgos; ++a)
        {
            e->setParameter (e, kAlgo, algoValue (a));
            std::vector<float> c (l), d (r), oa (sr), ob (sr);
            runBlocks (e, c, d, oa, ob, 512);
            double s = 0.0;
            for (int i = sr / 2; i < sr; ++i) s += std::fabs (oa[i]) + std::fabs (ob[i]);
            if (! finite (oa) || ! finite (ob) || s < 1.0) all = false;
        }
        check (all, "all 16 algorithms run in the plugin");
        e->setParameter (e, kAlgo, algoValue (kAmbience));
        e->setParameter (e, kAmDcy, 1.0f);
        std::vector<float> z (sr * 6, 0.0f), z2 (sr * 6, 0.0f), oa (sr * 6), ob (sr * 6);
        runBlocks (e, z, z2, oa, ob, 512);
        double tail = 0.0;
        for (int i = sr * 5; i < sr * 6; ++i) tail = std::max (tail, (double) std::fabs (oa[i]));
        std::printf ("  residual after 5 s of silence (longest ambience): %.2g\n", tail);
        check (tail < 1e-4, "silence settles");
        e->setParameter (e, kAlgo, algoValue (0));
    }

    // BYPASS: this plugin only; fades to the dry input, then passes it bit for bit
    {
        e->setParameter (e, kBypass, 1.0f);
        std::vector<float> a (l), b (r), oa (sr), ob (sr);
        runBlocks (e, a, b, oa, ob, 512);
        bool exact = true;
        for (int i = sr / 4; i < sr; ++i) if (oa[i] != a[i] || ob[i] != b[i]) exact = false;
        check (exact, "BYPASS passes the input bit for bit");
        std::vector<float> ia (l), ib (r);
        runBlocks (e, ia, ib, ia, ib, 37);
        bool exact2 = true;
        for (int i = sr / 4; i < sr; ++i) if (ia[i] != l[i] || ib[i] != r[i]) exact2 = false;
        check (exact2, "and in place with odd block sizes");
        e->setParameter (e, kBypass, 0.0f);
        runBlocks (e, a, b, oa, ob, 512);
        double d = 0.0;
        for (int i = sr / 4; i < sr; ++i) d += std::fabs (oa[i] - a[i]);
        check (d > 1.0, "effect back in after BYPASS");
    }

    std::printf ("\nSample-rate change to 96 kHz\n");
    call (e, effMainsChanged, 0, 0);
    call (e, effSetSampleRate, 0, 0, 0, 96000.0f);
    call (e, effMainsChanged, 0, 1);
    {
        const int sr2 = 96000;
        std::vector<float> a (sr2), b (sr2), oa (sr2), ob (sr2);
        for (int i = 0; i < sr2; ++i) { a[i] = (float) (0.2 * std::sin (2.0 * kPi * 196.0 * i / sr2)); b[i] = 0.5f * a[i]; }
        std::clock_t t0 = std::clock();
        runBlocks (e, a, b, oa, ob, 512);
        const double secs = (double) (std::clock() - t0) / CLOCKS_PER_SEC;
        check (finite (oa) && finite (ob), "runs at 96 kHz");
        std::printf ("  CPU at 96 kHz: %.1f%% of one core\n", 100.0 * secs);
    }

    std::printf ("\nAutomation while processing\n");
    {
        std::vector<float> a (sr), b (sr), oa (sr), ob (sr);
        for (int i = 0; i < sr; ++i) a[i] = b[i] = (float) (0.3 * std::sin (2.0 * kPi * 110.0 * i / sr));
        for (size_t pos = 0; pos < a.size(); pos += 64)
        {
            const float t = (float) pos / a.size();
            for (int p = 0; p < kNumParams; ++p) if (p != kBypass) e->setParameter (e, p, 0.5f + 0.5f * std::sin (6.2831853f * (p + 1) * t));
            const int n = (int) ((a.size() - pos) < 64 ? a.size() - pos : 64);
            float* in[2]  = { &a[pos], &b[pos] };
            float* out[2] = { &oa[pos], &ob[pos] };
            e->processReplacing (e, in, out, n);
        }
        check (finite (oa) && finite (ob), "all parameters automated together");
    }

    call (e, effMainsChanged, 0, 0);
    call (e, effClose);
    check (true, "effClose");
    dlclose (lib);

    std::printf ("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL HOST CHECKS PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
