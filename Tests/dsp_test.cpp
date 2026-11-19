// dsp_test.cpp
// Offline checks of the SDX-330 model: the parameter table, the factory programs, every
// algorithm's behaviour, stability, the EQ, the front-panel stages, and demo renders.
//   dsp_test [render folder]

#include "../Source/DSP/Sdx330.h"
#include "../Source/DSP/Factory.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>
#include <algorithm>

using namespace tbsdx;

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("  %s  %s\n", ok ? "ok  " : "FAIL", what);
    if (! ok) ++failures;
}

static const double SR = 44100.0;

static void sine (std::vector<float>& x, double f, double a, double sr = SR)
{
    for (size_t i = 0; i < x.size(); ++i) x[i] = (float) (a * std::sin (2.0 * kPi * f * i / sr));
}

static unsigned rngState = 12345u;
static double rnd() { rngState = rngState * 1664525u + 1013904223u; return (rngState >> 8) / 16777216.0; }

static void run (Sdx330& u, std::vector<float>& l, std::vector<float>& r, std::vector<float>& ol, std::vector<float>& orr, int block = 256)
{
    for (size_t p = 0; p < l.size(); p += block)
    {
        const int n = (int) ((l.size() - p) < (size_t) block ? l.size() - p : block);
        u.process (&l[p], &r[p], &ol[p], &orr[p], n);
    }
}

static double rms (const std::vector<float>& x, size_t a, size_t b)
{
    double s = 0.0;
    for (size_t i = a; i < b; ++i) s += (double) x[i] * x[i];
    return std::sqrt (s / (double) (b - a));
}

static bool finite (const std::vector<float>& x)
{
    for (size_t i = 0; i < x.size(); ++i) if (! (x[i] == x[i]) || std::fabs (x[i]) > 50.0f) return false;
    return true;
}

// Frequency by zero crossings
static double freqOf (const std::vector<float>& x, size_t a, size_t b)
{
    int z = 0; double first = -1, last = -1;
    for (size_t i = a + 1; i < b; ++i)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f)
        {
            const double t = (i - 1) + x[i - 1] / (x[i - 1] - x[i]);
            if (first < 0) first = t;
            last = t; ++z;
        }
    return z > 1 ? (z - 1) * SR / (last - first) : 0.0;
}

static void program (Sdx330& u, int factoryIndex)
{
    float v[kNumParams];
    for (int p = 0; p < kNumParams; ++p) v[p] = u.getParam (p);
    factoryValues (factoryIndex, v);
    for (int p = kAlgo; p < kNumParams; ++p) u.setParam (p, v[p]);
    u.reset();
}

static void defaults (Sdx330& u)
{
    for (int p = 0; p < kNumParams; ++p) u.setParam (p, defaultNorm (p));
}

static void setReal (Sdx330& u, int p, double r) { u.setParam (p, (float) toNorm (p, r)); }

static void writeWav (const char* path, const std::vector<float>& l, const std::vector<float>& r)
{
    FILE* f = std::fopen (path, "wb");
    if (! f) return;
    const unsigned n = (unsigned) l.size(), bytes = n * 4;
    unsigned v;
    std::fwrite ("RIFF", 1, 4, f); v = 36 + bytes; std::fwrite (&v, 4, 1, f);
    std::fwrite ("WAVEfmt ", 1, 8, f); v = 16; std::fwrite (&v, 4, 1, f);
    unsigned short s = 1; std::fwrite (&s, 2, 1, f); s = 2; std::fwrite (&s, 2, 1, f);
    v = 44100; std::fwrite (&v, 4, 1, f); v = 44100 * 4; std::fwrite (&v, 4, 1, f);
    s = 4; std::fwrite (&s, 2, 1, f); s = 16; std::fwrite (&s, 2, 1, f);
    std::fwrite ("data", 1, 4, f); std::fwrite (&bytes, 4, 1, f);
    for (unsigned i = 0; i < n; ++i)
    {
        short a = (short) (std::max (-1.0f, std::min (1.0f, l[i])) * 32767.0f);
        short b = (short) (std::max (-1.0f, std::min (1.0f, r[i])) * 32767.0f);
        std::fwrite (&a, 2, 1, f); std::fwrite (&b, 2, 1, f);
    }
    std::fclose (f);
}

int main (int argc, char** argv)
{
    const char* renders = argc > 1 ? argv[1] : 0;

    std::printf ("Parameter table\n");
    {
        check ((int) (sizeof (kInfo) / sizeof (kInfo[0])) == kNumParams, "one entry per parameter");
        bool names = true, disp = true, round = true, lcd = true;
        for (int p = 0; p < kNumParams; ++p)
        {
            if (! kInfo[p].vst || std::strlen (kInfo[p].vst) > 8) names = false;
            if (! kInfo[p].lcd || std::strlen (kInfo[p].lcd) > 6) lcd = false;
            for (int k = 0; k <= 50; ++k)
            {
                char b[64];
                formatParam (p, k / 50.0f, b, sizeof (b));
                if (std::strlen (b) > 8 || ! b[0]) disp = false;
                const double r = toReal (p, k / 50.0);
                const double back = toReal (p, toNorm (p, r));
                if (std::fabs (back - r) > 1e-6 * (1.0 + std::fabs (r))) round = false;
            }
            const double d = toReal (p, defaultNorm (p));
            if (kInfo[p].taper != tChoice && std::fabs (d - kInfo[p].def) > 0.02 * (1.0 + std::fabs (kInfo[p].def))) round = false;
        }
        check (names, "VST names are 8 characters or fewer");
        check (lcd, "LCD names are 6 characters or fewer");
        check (disp, "every value displays in 8 characters or fewer");
        check (round, "values round-trip and defaults land on the manual's values");
        char b[16];
        formatParam (kRate, defaultNorm (kRate), b, sizeof (b));
        check (! std::strcmp (b, "0.65"), "Rate default shows 0.65 (as the manual's power-on screen)");
        formatParam (kPanL, 0.0f, b, sizeof (b));
        check (! std::strcmp (b, "100:0"), "Pan shows 100:0 fully left");
        formatParam (kAzmtL, 0.0f, b, sizeof (b));
        check (! std::strcmp (b, "L180"), "Azimuth shows L180");
        formatParam (kLowQ, 0.0f, b, sizeof (b));
        check (! std::strcmp (b, "SHELV"), "EQ Q shows SHELV at its first setting");
        formatParamFor (kMode, (float) toNorm (kMode, 4), kSpaceChorus, b, sizeof (b));
        check (! std::strcmp (b, "1+4"), "Space Chorus Mode shows 1+4");
        formatParamFor (kMode, (float) toNorm (kMode, 4), kEnsemble, b, sizeof (b));
        check (! std::strcmp (b, "5"), "Ensemble Mode shows 5");
        float v = defaultNorm (kRate);
        float up = stepParam (kRate, v, +1);
        check (up > v, "the alpha dial steps a value up");
        float c = (float) toNorm (kLfo, 0);
        check (toIndex (kLfo, stepParam (kLfo, c, +1)) == 1, "the alpha dial steps a choice");
    }

    std::printf ("\nFactory programs (201-300)\n");
    {
        int bad = 0, longNames = 0;
        float v[kNumParams];
        for (int i = 0; i < kNumFactory; ++i)
        {
            bad += factoryValues (i, v);
            if (std::strlen (kFactory[i].name) > 17) ++longNames;
        }
        check (bad == 0, "every setting in the table names a real parameter and value");
        check (longNames == 0, "names fit the 17-character LCD");
        check (! std::strcmp (kFactory[0].name, "STEREO CHORUS") && ! std::strcmp (kFactory[99].name, "LIGHT WEIGHT AMB"), "first and last names as in the manual");
        factoryValues (25, v);
        char b[16]; formatParam (kRate, v[kRate], b, sizeof (b));
        check (! std::strcmp (b, "0.14"), "226 SMOOTH 3D CHORUS Rate 0.14 (as the photo shows)");
        int perAlgo[16] = { 0 };
        for (int i = 0; i < kNumFactory; ++i) ++perAlgo[kFactory[i].algo - 1];
        bool all = true;
        for (int a = 0; a < 16; ++a) if (perAlgo[a] == 0) all = false;
        check (all, "every algorithm has factory programs");
    }

    Sdx330 u;
    u.prepare (SR);
    const int N = (int) SR;
    std::vector<float> l (N), r (N), ol (N), orr (N);

    std::printf ("\nFront panel and mixing\n");
    {
        check (std::fabs (Sdx330::knobGain (0.5) - 1.0) < 1e-12, "INPUT / OUTPUT LEVEL unity at 12 o'clock (5 of 10)");
        check (std::fabs (20.0 * std::log10 (Sdx330::knobGain (1.0)) - 12.04) < 0.1, "+12 dB fully up");
        defaults (u);
        setReal (u, kELvl, 0);
        u.reset();
        sine (l, 1000.0, 0.3); sine (r, 500.0, 0.3);
        run (u, l, r, ol, orr);
        double d = 0.0;
        for (int i = 2000; i < N; ++i) d = std::max (d, (double) std::fabs (ol[i] - l[i]) + std::fabs (orr[i] - r[i]));
        std::printf ("  E.Lvl 0: largest difference from the input %.2g\n", d);
        check (d < 1e-4, "E.Lvl 0, EQ flat: the direct sound passes at unity");
        setReal (u, kDirectMix, 2);              // OFF
        run (u, l, r, ol, orr);
        check (rms (ol, N / 2, N) < 1e-4, "Direct Mix OFF removes the direct sound");
        setReal (u, kDirectMix, 0);
        setReal (u, kDirect, 0);
        run (u, l, r, ol, orr);
        check (rms (ol, N / 2, N) < 1e-4, "Dirct OFF removes it in PROG mode");
        setReal (u, kDirectMix, 1);
        run (u, l, r, ol, orr);
        check (rms (ol, N / 2, N) > 0.2, "Direct Mix ON forces it back on");
        defaults (u);
        u.reset();
        sine (l, 200.0, 1.5); sine (r, 200.0, 0.01);
        run (u, l, r, ol, orr);
        check (u.meter (0) > 1.0 && u.meter (1) < 0.02, "the meter reads each side at the converter (OVER on the left)");
        double pk = 0.0;
        setReal (u, kELvl, 0);
        run (u, l, r, ol, orr);
        for (int i = N / 2; i < N; ++i) pk = std::max (pk, (double) std::fabs (ol[i]));
        check (pk <= 1.0001, "the converter clips at full scale");
    }

    std::printf ("\nEQ\n");
    {
        defaults (u);
        setReal (u, kELvl, 0);
        setReal (u, kLowGn, 12); setReal (u, kLowFq, 200);
        u.reset();
        sine (l, 50.0, 0.05); sine (r, 50.0, 0.05);
        run (u, l, r, ol, orr);
        const double g1 = 20.0 * std::log10 (rms (ol, N / 2, N) / rms (l, N / 2, N));
        setReal (u, kLowGn, 0); setReal (u, kHigGn, -12); setReal (u, kHigFq, 3000);
        u.reset();
        sine (l, 12000.0, 0.05); sine (r, 12000.0, 0.05);
        run (u, l, r, ol, orr);
        const double g2 = 20.0 * std::log10 (rms (ol, N / 2, N) / rms (l, N / 2, N));
        setReal (u, kHigGn, 0); setReal (u, kMidGn, 9); setReal (u, kMidFq, 1000); u.setParam (kMidQ, (float) toNorm (kMidQ, 5));
        u.reset();
        sine (l, 1000.0, 0.05); sine (r, 1000.0, 0.05);
        run (u, l, r, ol, orr);
        const double g3 = 20.0 * std::log10 (rms (ol, N / 2, N) / rms (l, N / 2, N));
        std::printf ("  low shelf +12 at 50 Hz: %+.1f dB, high shelf -12 at 12 kHz: %+.1f dB, mid +9 at 1 kHz: %+.1f dB\n", g1, g2, g3);
        check (g1 > 10.5 && g1 < 12.5, "low shelf boosts");
        check (g2 < -10.5 && g2 > -12.5, "high shelf cuts");
        check (std::fabs (g3 - 9.0) < 0.5, "mid peak at its frequency");
    }

    std::printf ("\nAlgorithms\n");
    for (int a = 0; a < kNumAlgos; ++a)
    {
        // The first factory program for each algorithm
        int idx = 0;
        while (kFactory[idx].algo != a + 1) ++idx;
        program (u, idx);
        sine (l, 330.0, 0.3);
        for (int i = 0; i < N; ++i) r[i] = l[i];
        std::clock_t t0 = std::clock();
        run (u, l, r, ol, orr);
        const double cpu = (double) (std::clock() - t0) / CLOCKS_PER_SEC;
        const double rl = rms (ol, N / 2, N), rr = rms (orr, N / 2, N);
        double diff = 0.0;
        for (int i = N / 2; i < N; ++i) diff += std::fabs (ol[i] - orr[i]);
        char what[128];
        std::snprintf (what, sizeof (what), "%2d %-17s (%s): output L %.3f R %.3f, L-R %.1f, CPU %.2f%%", a + 1, kAlgoNames[a],
                       kFactory[idx].name, rl, rr, diff / (N / 2), 100.0 * cpu);
        check (finite (ol) && finite (orr) && rl > 0.03 && rr > 0.03 && rl < 1.0 && rr < 1.0, what);
    }

    std::printf ("\nBehaviour\n");
    {
        // Stereo Chorus: Depth sweeps the delay (pitch wobbles), LFO Phase 180 widens
        defaults (u);
        setReal (u, kAlgo, 1); setReal (u, kDirect, 0); setReal (u, kRate, 1.0); setReal (u, kDepth, 100);
        u.reset();
        sine (l, 1000.0, 0.3); r = l;
        run (u, l, r, ol, orr);
        double fmin = 1e9, fmax = 0;
        for (int w = N / 4; w + 2205 < N; w += 2205) { const double f = freqOf (ol, w, w + 2205); fmin = std::min (fmin, f); fmax = std::max (fmax, f); }
        std::printf ("  Stereo Chorus Rate 1 Hz Depth 100: pitch %.1f .. %.1f Hz on a 1 kHz tone\n", fmin, fmax);
        check (fmax - fmin > 15.0 && fmax - fmin < 120.0, "chorus bends pitch, within about +-3.5 %");
        double side = 0.0;
        for (int i = N / 2; i < N; ++i) side += std::fabs (ol[i] - orr[i]);
        check (side / (N / 2) > 0.02, "Phase 180 makes the sides differ");
        check (Sdx330::chorusHalfSwingMs (40.0, 100) < 0.2 && Sdx330::chorusHalfSwingMs (0.5, 100) == 3.0, "depth scaled down at fast rates");

        // Detune: +100 cents on one voice, others off
        defaults (u);
        setReal (u, kAlgo, 10); setReal (u, kDirect, 0); setReal (u, kPrDly, 0);
        for (int k = 0; k < 6; ++k) setReal (u, kDtLvl1 + k, 0);
        setReal (u, kDtLvl1, 100); setReal (u, kDtPit1, 100); setReal (u, kDtPan1, 0);
        setReal (u, kDtLvl1 + 3, 100); setReal (u, kDtPit1 + 3, -100); setReal (u, kDtPan1 + 3, 100);
        u.reset();
        sine (l, 440.0, 0.3); r = l;
        run (u, l, r, ol, orr);
        const double fu = freqOf (ol, N / 2, N), fd = freqOf (orr, N / 2, N);
        std::printf ("  Detune +100 / -100 cents on 440 Hz: %.1f / %.1f Hz\n", fu, fd);
        check (std::fabs (fu - 466.16) < 3.0 && std::fabs (fd - 415.30) < 3.0, "pitch shifters land on the semitone");

        // Flanger: Resonance +-100 rings but stays bounded
        defaults (u);
        setReal (u, kAlgo, 14); setReal (u, kResnc, 100); setReal (u, kDepth, 100); setReal (u, kRate, 0.3);
        u.reset();
        for (int i = 0; i < N; ++i) { l[i] = (float) (0.5 * (rnd() - 0.5)); r[i] = l[i]; }
        run (u, l, r, ol, orr);
        const double fpos = rms (ol, N / 2, N);
        setReal (u, kResnc, -100);
        run (u, l, r, ol, orr);
        std::printf ("  Flanger on noise, Resonance +100: %.3f rms, -100: %.3f rms\n", fpos, rms (ol, N / 2, N));
        check (finite (ol) && fpos < 2.0, "flanger feedback stays bounded at both extremes");
        check (Sdx330::flangerDelayMs (0) < 0.06 && Sdx330::flangerDelayMs (1) > 9.9, "Manual sweeps 0.05 .. 10 ms");

        // Rotary: FAST from SLOW speeds up after the rise time
        defaults (u);
        setReal (u, kAlgo, 15); setReal (u, kRotSpeed, 0); setReal (u, kHRise, 10);
        u.reset();
        sine (l, 1500.0, 0.3); r = l;
        run (u, l, r, ol, orr);
        const double slowVar = std::fabs (rms (ol, N / 2, N / 2 + 4410) - rms (ol, N - 4410, N));
        setReal (u, kRotSpeed, 1);
        run (u, l, r, ol, orr);
        run (u, l, r, ol, orr);
        // Tremolo cycles: count envelope peaks over the last half second
        std::vector<double> env;
        for (int i = N / 2; i + 220 < N; i += 220) { double m = 0; for (int k = 0; k < 220; ++k) m = std::max (m, (double) std::fabs (ol[i + k])); env.push_back (m); }
        int peaks = 0;
        for (size_t k = 1; k + 1 < env.size(); ++k) if (env[k] > env[k - 1] && env[k] >= env[k + 1] && env[k] > 0.9 * 0.3 * 0.5) ++peaks;
        std::printf ("  Rotary FAST: about %d tremolo peaks in 0.5 s (horn set to 6.8 Hz), slow change %.3f\n", peaks, slowVar);
        check (peaks >= 2 && peaks <= 6, "horn spins up to its fast speed");

        // Overdrive Gain 0 = no sound (the manual's note)
        setReal (u, kOdSw, 1); setReal (u, kOdGain, 0);
        run (u, l, r, ol, orr);
        check (rms (ol, N / 2, N) < 1e-3, "Rotary overdrive at Gain 0 outputs nothing, as the manual says");

        // Ambience: tail decays and longer Decay rings longer
        defaults (u);
        setReal (u, kAlgo, 16); setReal (u, kDirect, 0); setReal (u, kChLvl, 0); setReal (u, kAmLvl, 100);
        double tails[2];
        for (int t = 0; t < 2; ++t)
        {
            setReal (u, kAmDcy, t == 0 ? 3 : 25);
            u.reset();
            for (int i = 0; i < N; ++i) { l[i] = i < 2000 ? (float) (0.5 * (rnd() - 0.5)) : 0.0f; r[i] = l[i]; }
            run (u, l, r, ol, orr);
            tails[t] = rms (ol, N / 2, N);
        }
        std::printf ("  Ambience tail 0.5-1 s after a burst: Decay 3 %.2g, Decay 25 %.2g\n", tails[0], tails[1]);
        check (tails[1] > 10.0 * tails[0] && tails[1] < 0.5, "Ambience Decay sets the tail length");

        // 3D Panner: the image moves (L/R balance changes over time)
        defaults (u);
        setReal (u, kAlgo, 3); setReal (u, kDirect, 0); setReal (u, kPanSpeed, 8);
        u.reset();
        for (int i = 0; i < N; ++i) { l[i] = (float) (0.3 * (rnd() - 0.5)); r[i] = l[i]; }
        run (u, l, r, ol, orr);
        double bmin = 1e9, bmax = -1e9;
        for (int w = 0; w + 2205 < N; w += 2205)
        {
            const double b = 20.0 * std::log10 ((rms (ol, w, w + 2205) + 1e-9) / (rms (orr, w, w + 2205) + 1e-9));
            bmin = std::min (bmin, b); bmax = std::max (bmax, b);
        }
        std::printf ("  3D Panner Speed 8: L/R balance swings %.1f .. %+.1f dB\n", bmin, bmax);
        check (bmax - bmin > 6.0, "the sound moves round");

        // Band chorus: crossovers sum flat (depth 0, all mixes 100)
        defaults (u);
        setReal (u, kAlgo, 7); setReal (u, kDirect, 0);
        for (int b = 0; b < 4; ++b) { setReal (u, kB1Depth + 6 * b, 0); setReal (u, kB1Mix + 6 * b, 100); setReal (u, kB1PD + 6 * b, 0); }
        u.reset();
        double gmin = 1e9, gmax = -1e9;
        const double freqs[6] = { 60, 180, 700, 2000, 5000, 12000 };
        for (int k = 0; k < 6; ++k)
        {
            sine (l, freqs[k], 0.1); r = l;
            run (u, l, r, ol, orr);
            const double g = 20.0 * std::log10 (rms (ol, N / 2, N) / rms (l, N / 2, N));
            gmin = std::min (gmin, g); gmax = std::max (gmax, g);
        }
        std::printf ("  4 Band Chorus, Depth 0: response %.2f .. %.2f dB\n", gmin, gmax);
        check (gmax - gmin < 1.0, "band splits add back up flat");

        // Vintage Mode 4: dry left, effect right
        defaults (u);
        setReal (u, kAlgo, 13); u.setParam (kMode, (float) toNorm (kMode, 3));
        u.reset();
        sine (l, 500.0, 0.3); r = l;
        run (u, l, r, ol, orr);
        double dd = 0.0;
        for (int i = N / 2; i < N; ++i) dd = std::max (dd, (double) std::fabs (ol[i] - l[i]));
        check (dd < 1e-3 && rms (orr, N / 2, N) > 0.1, "Vintage Mode 4: direct from the left, effect from the right");

        // Space Chorus: mono in, a wide but mono-compatible image
        program (u, 3);
        sine (l, 440.0, 0.3); r = l;
        run (u, l, r, ol, orr);
        double sum = 0.0, dif = 0.0;
        for (int i = N / 2; i < N; ++i) { sum += std::fabs (ol[i] + orr[i]); dif += std::fabs (ol[i] - orr[i]); }
        std::printf ("  Space Chorus mode 2: L+R %.3f, L-R %.3f\n", sum / (N / 2), dif / (N / 2));
        check (dif > 0.02 * sum && sum > dif, "Space Chorus widens a mono source and keeps a strong centre");
    }

    std::printf ("\nStability sweep (random settings, every algorithm, loud noise and sines)\n");
    {
        bool ok = true;
        for (int trial = 0; trial < 64; ++trial)
        {
            for (int p = kInputL; p < kNumParams; ++p) if (p != kBypass && p != kProgram) u.setParam (p, (float) rnd());
            u.setParam (kAlgo, algoValue (trial % 16));
            u.setParam (kOutput, 0.5f);
            u.reset();
            const double amp = trial & 1 ? 1.5 : 0.05;
            for (int i = 0; i < N / 4; ++i) { l[i] = (float) (amp * (trial & 2 ? std::sin (i * 0.05 * (1 + trial)) : (rnd() - 0.5))); r[i] = (float) (amp * (rnd() - 0.5)); }
            std::vector<float> a (l.begin(), l.begin() + N / 4), b (r.begin(), r.begin() + N / 4), oa (N / 4), ob (N / 4);
            run (u, a, b, oa, ob, 1 + trial * 7);
            if (! finite (oa) || ! finite (ob)) { ok = false; std::printf ("  trial %d (algo %d) unstable\n", trial, trial % 16 + 1); }
        }
        check (ok, "64 random settings stay finite and bounded");
    }

    std::printf ("\nAutomation while playing, and algorithm changes\n");
    {
        defaults (u);
        u.reset();
        sine (l, 220.0, 0.3); r = l;
        bool ok = true;
        for (int pos = 0; pos < N; pos += 64)
        {
            const float t = (float) pos / N;
            for (int p = kELvl; p < kNumParams; ++p) u.setParam (p, 0.5f + 0.5f * std::sin (6.2831853f * (p + 1) * t));
            u.setParam (kAlgo, algoValue ((pos / 2048) % 16));
            const int n = N - pos < 64 ? N - pos : 64;
            u.process (&l[pos], &r[pos], &ol[pos], &orr[pos], n);
        }
        for (int i = 0; i < N; ++i) if (! (ol[i] == ol[i])) ok = false;
        check (ok && finite (ol) && finite (orr), "every parameter moving and the algorithm changing: finite");

        // Program changes on a steady tone: the effect fades out and in, no clicks
        float prev = 0.0f; double jump = 0.0;
        defaults (u);
        u.reset();
        for (int pos = 0; pos < N; pos += 64)
        {
            if (pos % 4096 == 0)
            {
                float v[kNumParams];
                for (int p = 0; p < kNumParams; ++p) v[p] = u.getParam (p);
                factoryValues ((pos / 4096) * 9 % kNumFactory, v);
                u.setParam (kProgram, programValue (201 + (pos / 4096) * 9 % kNumFactory));   // as the plugin does: number first
                for (int p = kAlgo; p < kNumParams; ++p) u.setParam (p, v[p]);
            }
            const int n = N - pos < 64 ? N - pos : 64;
            u.process (&l[pos], &r[pos], &ol[pos], &orr[pos], n);
            for (int i = pos; i < pos + n; ++i) { jump = std::max (jump, (double) std::fabs (ol[i] - prev)); prev = ol[i]; }
        }
        std::printf ("  program changes every 93 ms: biggest sample-to-sample step %.3f (the tone alone steps 0.009)\n", jump);
        check (jump < 0.12, "program changes don't click");
    }

    std::printf ("\n96 kHz\n");
    {
        Sdx330* hi = new Sdx330();
        hi->prepare (96000.0);
        bool ok = true; double worst = 0.0;
        std::vector<float> a (96000), b (96000), oa (96000), ob (96000);
        for (int i = 0; i < 96000; ++i) { a[i] = (float) (0.3 * std::sin (2.0 * kPi * 330.0 * i / 96000.0)); b[i] = a[i]; }
        for (int k = 0; k < kNumFactory; k += 3)
        {
            program (*hi, k);
            std::clock_t t0 = std::clock();
            run (*hi, a, b, oa, ob, 512);
            worst = std::max (worst, (double) (std::clock() - t0) / CLOCKS_PER_SEC);
            if (! finite (oa) || rms (oa, 48000, 96000) < 0.02) ok = false;
        }
        std::printf ("  worst program: %.1f%% of one core at 96 kHz\n", 100.0 * worst);
        check (ok, "programs run at 96 kHz");
        delete hi;
    }

    if (renders)
    {
        std::printf ("\nRenders\n");
        // A plucked-ish test riff: decaying saw notes
        const int L = (int) (SR * 4);
        std::vector<float> a (L), b (L), oa (L), ob (L);
        const double notes[8] = { 110.0, 164.8, 220.0, 261.6, 329.6, 261.6, 220.0, 164.8 };
        for (int i = 0; i < L; ++i)
        {
            const int n = (i / (L / 8)) % 8; const double t = (i % (L / 8)) / SR;
            const double ph = std::fmod (notes[n] * t, 1.0);
            a[i] = b[i] = (float) (0.35 * (2.0 * ph - 1.0) * std::exp (-2.5 * t));
        }
        const int demo[8] = { 0, 3, 12, 10, 13, 89, 73, 98 };
        for (int k = 0; k < 8; ++k)
        {
            program (u, demo[k]);
            run (u, a, b, oa, ob, 512);
            char path[512];
            std::snprintf (path, sizeof (path), "%s/%03d_%s.wav", renders, 201 + demo[k], kFactory[demo[k]].name);
            for (char* c = path + std::strlen (renders) + 1; *c; ++c) if (*c == ' ' || *c == '\'' || *c == '&' || *c == '!') *c = '_';
            writeWav (path, oa, ob);
            std::printf ("  wrote %s\n", path);
        }
    }

    std::printf ("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL DSP CHECKS PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
