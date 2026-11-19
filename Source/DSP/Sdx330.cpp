// Sdx330.cpp
// See Sdx330.h. Every constant marked "estimate" is not fixed by the Owner's Manual.

#include "Sdx330.h"

#include <cstring>

namespace tbsdx
{

namespace
{
    // --- Estimates -------------------------------------------------------------
    const double kChorusBaseMs   = 1.5;     // chorus delay above Pre Delay at the LFO's low point
    const double kMaxHalfSwingMs = 3.0;     // Depth 100 at slow rates: +-3 ms around the centre
    const double kMaxPitchDev    = 0.035;   // Depth 100 never bends pitch by more than ~0.6 semitone
    const double kMainLineSec    = 0.30;    // Pre Delay 200 ms + chorus + pitch-shift window
    const double kSpaceCentreMs  = 3.2;     // Space Chorus (SDD-320 style) BBD centre delay
    const double kSpaceCross     = 0.42;    // ... each side's wet fed inverted to the other
    const double kSpaceLpHz      = 9000.0;  // ... BBD bandwidth
    const double kVintageCentreMs= 3.5;     // Vintage Chorus (CE-1 style)
    const double kVintageLpHz    = 7000.0;
    const double kDetuneWindowMs = 45.0;    // pitch shifter splice window (long: little tremble)
    const double kFlangerMinMs   = 0.05;    // Manual 0 .. 100 sweeps 0.05 .. 10 ms
    const double kFlangerMaxMs   = 10.0;
    const double kFlangerMaxFb   = 0.93;    // Resonance +-100
    const double kRotXoverHz     = 800.0;   // rotary speaker crossover
    const double kHornDopplerMs  = 0.60;    // horn depth 100: +-0.6 ms (about 0.2 m radius)
    const double kRotorDopplerMs = 0.35;
    const double kAlgoFadeMs     = 12.0;    // effect fades out and back in on an algorithm change

    // Space Chorus modes 1, 2, 3, 4, 1+4, 2+4, 3+4: LFO rate, half swing (ms), wet gain (estimates
    // after published measurements of the SDD-320: slow triangle sweeps, deeper per button, 4 faster)
    const double kSpaceModes[7][3] = {
        { 0.25, 0.30, 0.60 }, { 0.25, 0.55, 0.70 }, { 0.25, 0.90, 0.80 }, { 0.50, 0.45, 0.70 },
        { 0.37, 0.40, 0.70 }, { 0.37, 0.60, 0.75 }, { 0.37, 0.80, 0.80 }
    };

    // Ensemble modes 1-6: slow Hz, slow half swing ms, fast Hz, fast half swing ms, centre ms,
    // regeneration, diffusion, low-pass Hz (estimates: a string-machine ensemble, 3 taps 120 deg apart)
    const double kEnsModes[6][8] = {
        { 0.63, 1.6, 6.3, 0.18, 6.0, 0.00, 0.0, 9000.0 },   // 1 classic strings
        { 0.45, 2.4, 5.0, 0.12, 8.0, 0.00, 0.0, 8000.0 },   // 2 slow and deep
        { 0.90, 1.2, 7.5, 0.25, 5.0, 0.00, 0.0, 10000.0 },  // 3 bright shimmer
        { 0.63, 1.6, 6.3, 0.18, 7.0, 0.45, 0.0, 7000.0 },   // 4 with regeneration (reverberant)
        { 0.55, 2.0, 6.0, 0.15, 6.0, 0.00, 0.5, 9000.0 },   // 5 wide, diffused
        { 0.50, 2.2, 5.6, 0.20, 9.0, 0.35, 0.7, 7000.0 }    // 6 lush
    };

    inline double softClip (double x)
    {
        // The converter: linear to 0.9 of full scale, then rounding into a hard limit at 1.0
        const double a = std::fabs (x);
        if (a <= 0.9) return x;
        const double over = (a - 0.9) / 0.1;
        const double y = 0.9 + 0.1 * (over / (1.0 + over));
        return x < 0 ? -y : y;
    }

    inline void panGains (double pan01, double& gl, double& gr)
    {
        gl = std::cos (pan01 * 0.5 * kPi);
        gr = std::sin (pan01 * 0.5 * kPi);
    }

    inline double centsRatio (double c) { return std::pow (2.0, c / 1200.0); }
}

//==============================================================================
Sdx330::Sdx330()
    : fs (44100.0), eqDirty (true), algo (0), swapPending (false), fade (1.0), fadeStep (0.0),
      peakRel (0.999), posCount (0), panAngle (0.0), flLastL (0.0), flLastR (0.0),
      hornPh (0.0), rotorPh (0.0), hornHz (0.8), rotorHz (0.66)
{
    for (int i = 0; i < kNumParams; ++i) tgt[i] = prm[i] = defaultNorm (i);
    peak[0] = peak[1] = 0.0;
    for (int i = 0; i < 6; ++i) dtPhase[i] = 0.0;
    for (int i = 0; i < 4; ++i) { fdnGain[i] = 0.0; fdnLen[i] = 1; }
    apLen[0] = apLen[1] = 1;
    algo = algoIndex (prm[kAlgo]);
}

double Sdx330::knobGain (double k) { k = clampd (k, 0.0, 1.0); return 4.0 * k * k; }

double Sdx330::chorusHalfSwingMs (double rateHz, double depth)
{
    const double byPitch = kMaxPitchDev / (kTwoPi * (rateHz > 0.001 ? rateHz : 0.001)) * 1000.0;
    return clampd (depth, 0.0, 100.0) / 100.0 * (byPitch < kMaxHalfSwingMs ? byPitch : kMaxHalfSwingMs);
}

double Sdx330::flangerDelayMs (double pos)
{
    return kFlangerMinMs * std::pow (kFlangerMaxMs / kFlangerMinMs, clampd (pos, 0.0, 1.0));
}

double Sdx330::pannerHz (int speed)
{
    // Speed 1 = one turn in about 20 s, 10 = about 2 turns a second (estimate)
    return 0.05 * std::pow (1.5, clampd (speed, 1, 10) - 1.0);
}

void Sdx330::prepare (double sampleRate)
{
    fs = sampleRate > 1000.0 ? sampleRate : 44100.0;
    const int mainLen = (int) (kMainLineSec * fs) + 64;
    dlL.allocate (mainLen);
    dlR.allocate (mainLen);
    for (int b = 0; b < 4; ++b) { band[b].f[0].stages = band[b].f[1].stages = 0; }
    flL.allocate ((int) (0.012 * fs)); flR.allocate ((int) (0.012 * fs));
    enL.allocate ((int) (0.03 * fs)); enR.allocate ((int) (0.03 * fs));
    posL.prepare (fs);
    posR.prepare (fs);
    for (int c = 0; c < 2; ++c) { enAp[c][0].allocate ((int) (0.02 * fs)); enAp[c][1].allocate ((int) (0.02 * fs)); }
    hornDl.allocate ((int) (0.01 * fs));
    rotorDl.allocate ((int) (0.01 * fs));
    roomDl[0].allocate ((int) (0.03 * fs));
    roomDl[1].allocate ((int) (0.03 * fs));
    const double fdnMs[4] = { 17.3, 22.9, 29.1, 35.7 };
    for (int i = 0; i < 4; ++i) { fdnLen[i] = (int) (fdnMs[i] * 0.001 * fs); fdn[i].allocate (fdnLen[i] + 4); }
    apLen[0] = (int) (0.0047 * fs); apLen[1] = (int) (0.0063 * fs);
    amAp[0].allocate (apLen[0] + 4); amAp[1].allocate (apLen[1] + 4);

    const double ctl = 0.02;
    gInL.setTime (0.01, fs); gInR.setTime (0.01, fs); gOut.setTime (0.01, fs);
    gEff.setTime (0.01, fs); gDir.setTime (0.01, fs);
    pdS.setTime (0.05, fs); swS.setTime (ctl, fs); xmixS.setTime (ctl, fs);
    panLS.setTime (ctl, fs); panRS.setTime (ctl, fs);
    for (int b = 0; b < 4; ++b) { band[b].pd.setTime (0.05, fs); band[b].sw.setTime (ctl, fs); band[b].mix.setTime (ctl, fs); }
    for (int k = 0; k < 8; ++k) { p8pd[k].setTime (0.05, fs); p8dp[k].setTime (ctl, fs); p8mix[k].setTime (ctl, fs); }
    for (int k = 0; k < 6; ++k) { dtLvl[k].setTime (ctl, fs); dtPanS[k].setTime (ctl, fs); }
    flPos.setTime (ctl, fs); flFb.setTime (ctl, fs);
    rotBal.setTime (ctl, fs);
    peakRel = std::exp (-1.0 / (0.3 * fs));
    fadeStep = 1.0 / (kAlgoFadeMs * 0.001 * fs);
    reset();
}

void Sdx330::clearMain()
{
    dlL.clear(); dlR.clear();
    clearTails();
}

// Everything but the input-history lines, which stay filled so a new program's delayed
// sound doesn't start with a jump
void Sdx330::clearTails()
{
    for (int c = 0; c < 2; ++c) { enAp[c][0].clear(); enAp[c][1].clear(); }
    hornDl.clear(); rotorDl.clear(); roomDl[0].clear(); roomDl[1].clear();
    for (int i = 0; i < 4; ++i) { fdn[i].clear(); fdnDamp[i].reset(); }
    amAp[0].clear(); amAp[1].clear();
    posL.reset(); posR.reset();
    spLpL.reset(); spLpR.reset(); enLpL.reset(); enLpR.reset();
    vnLp.reset(); vnLp2.reset();
    chFilL.reset(); chFilR.reset(); amFil.reset();
    for (int b = 0; b < 4; ++b) for (int c = 0; c < 2; ++c) for (int k = 0; k < 3; ++k) { band[b].f[c].st[k][0].reset(); band[b].f[c].st[k][1].reset(); }
    flL.clear(); flR.clear(); enL.clear(); enR.clear();
    rotXo.reset(); hornLp.reset(); rotorLp.reset(); odHp.reset(); odLp.reset();
    flLastL = flLastR = 0.0;
}

void Sdx330::reset()
{
    clearMain();
    for (int c = 0; c < 2; ++c) for (int k = 0; k < 3; ++k) eq[c][k].reset();
    lfo.phase = 0.0; lfo2.phase = 0.0; enSlow.phase = 0.0; enFast.phase = 0.0;
    for (int b = 0; b < 4; ++b) band[b].lfo.phase = 0.0;
    for (int i = 0; i < 6; ++i) dtPhase[i] = 0.0;
    hornPh = 0.0; rotorPh = 0.25;
    peak[0] = peak[1] = 0.0;
    for (int i = 0; i < kNumParams; ++i) prm[i] = tgt[i];
    swapPending = false;
    algo = algoIndex (prm[kAlgo]);
    fade = 1.0;
    eqDirty = true;
    snapControls();
    // The mix levels too (a program change lets these glide instead)
    gEff.snap (rp (kELvl) / 100.0);
    {
        const int dm = ip (kDirectMix);
        const bool has = ! (algo == kEnsemble || algo == kVintage || algo == kRotary);
        gDir.snap (dm == 1 ? 1.0 : (dm == 2 ? 0.0 : (has && ip (kDirect) == 1 ? 1.0 : 0.0)));
    }
    gInL.snap (knobGain (prm[kInputL])); gInR.snap (knobGain (prm[kInputR])); gOut.snap (knobGain (prm[kOutput]));
}

// Settle the algorithm's smoothers on their targets (start-up, and after a program change)
void Sdx330::snapControls()
{
    updateControls();
    pdS.snap (rp (kPrDly)); swS.snap (chorusHalfSwingMs (rp (kRate), rp (kDepth)));
    xmixS.snap (rp (kXMix) / 100.0); panLS.snap (rp (kPanL) / 100.0); panRS.snap (rp (kPanR) / 100.0);
    for (int b = 0; b < 4; ++b)
    {
        const int o = b * 6;
        band[b].pd.snap (rp (kB1PD + o));
        band[b].sw.snap (chorusHalfSwingMs (rp (kB1Rate + o), rp (kB1Depth + o)));
        band[b].mix.snap (rp (kB1Mix + o) / 100.0);
    }
    for (int k = 0; k < 8; ++k)
    {
        p8pd[k].snap (rp (kP8PD1 + k));
        p8dp[k].snap (chorusHalfSwingMs (rp (kRate), rp (kP8Dpt1 + k)));
        p8mix[k].snap (rp (kP8Mix1 + k) / 100.0);
    }
    for (int k = 0; k < 6; ++k) { dtLvl[k].snap (rp (kDtLvl1 + k) / 100.0); dtPanS[k].snap (rp (kDtPan1 + k) / 100.0); }
    flPos.snap (rp (kManual) / 100.0); flFb.snap (kFlangerMaxFb * rp (kResnc) / 100.0);
    hornHz = ip (kRotSpeed) ? rp (kHFast) : rp (kHSlow);
    rotorHz = ip (kRotSpeed) ? rp (kRFast) : rp (kRSlow);
    panAngle = rp (kPanStart);
    rotBal.snap (rp (kRHBal));
}

void Sdx330::setParam (int index, float value)
{
    if (index < 0 || index >= kNumParams) return;
    if (! (value == value)) return;
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;
    const float old = tgt[index];
    tgt[index] = value;
    // A new program number or algorithm: the unit mutes the effect while it changes over.
    // Until then the old algorithm keeps its own settings.
    if ((index == kProgram && value != old) || (index == kAlgo && algoIndex (value) != algo))
        swapPending = true;
    if (index < kAlgo || ! swapPending)
    {
        prm[index] = value;
        if (index >= kLowFq && index <= kHigQ) eqDirty = true;
        if (index == kPanStart) panAngle = rp (kPanStart);
    }
}

void Sdx330::swapProgram()
{
    swapPending = false;
    const int was = algo;
    for (int i = kAlgo; i < kNumParams; ++i) prm[i] = tgt[i];
    algo = algoIndex (prm[kAlgo]);
    eqDirty = true;
    if (algo != was)
    {
        clearTails();
        lfo.phase = 0.0;
        posCount = 0;
    }
    panAngle = rp (kPanStart);       // the 3D Panner starts from its Start Position
    snapControls();
}

void Sdx330::updateEq()
{
    eqDirty = false;
    const double lf = rp (kLowFq), lg = rp (kLowGn), mf = rp (kMidFq), mg = rp (kMidGn), hf = rp (kHigFq), hg = rp (kHigGn);
    const int lq = ip (kLowQ), mq = ip (kMidQ), hq = ip (kHigQ);
    for (int c = 0; c < 2; ++c)
    {
        if (lq == 0) eq[c][0].lowShelf (lf, lg, fs); else eq[c][0].peak (lf, kQValues[lq], lg, fs);
        eq[c][1].peak (mf, kQValues[mq + 1], mg, fs);
        if (hq == 0) eq[c][2].highShelf (hf, hg, fs); else eq[c][2].peak (hf, kQValues[hq], hg, fs);
    }
}

// Each band's crossover cascade (Linkwitz-Riley, 4th order), applied after its own delay:
// low = LP1; next = HP1 LP2; next = HP1 HP2 LP3; top = HP1 HP2 HP3 (2 Band: LP / HP at the
// one crossover, 3 Band: LP1 / HP1 LP3 / HP1 HP3). Coefficients change, state is kept.
void Sdx330::setBandFilters (int bands)
{
    const double f1 = rp (kXov1), f2 = rp (kXov2), f3 = rp (kXov3);
    struct S { int lp; double f; };
    S plan[4][3]; int count[4] = { 0, 0, 0, 0 };
    #define ADD(b, l, fr) { plan[b][count[b]].lp = l; plan[b][count[b]].f = fr; ++count[b]; }
    if (bands == 2)      { ADD (0, 1, f2); ADD (3, 0, f2); }
    else if (bands == 3) { ADD (0, 1, f1); ADD (1, 0, f1); ADD (1, 1, f3); ADD (3, 0, f1); ADD (3, 0, f3); }
    else
    {
        ADD (0, 1, f1);
        ADD (1, 0, f1); ADD (1, 1, f2);
        ADD (2, 0, f1); ADD (2, 0, f2); ADD (2, 1, f3);
        ADD (3, 0, f1); ADD (3, 0, f2); ADD (3, 0, f3);
    }
    #undef ADD
    for (int b = 0; b < 4; ++b)
        for (int c = 0; c < 2; ++c)
        {
            BandFilter& bf = band[b].f[c];
            bf.stages = count[b];
            for (int k = 0; k < count[b]; ++k)
                for (int j = 0; j < 2; ++j)
                {
                    Biquad& q = bf.st[k][j];
                    const double z1 = q.z1, z2 = q.z2;
                    if (plan[b][k].lp) q.lowpass (plan[b][k].f, 0.70710678, fs); else q.highpass (plan[b][k].f, 0.70710678, fs);
                    q.z1 = z1; q.z2 = z2;
                }
        }
}

void Sdx330::updateControls()
{
    if (eqDirty) updateEq();
    // Shared LFO
    double rate = rp (kRate);
    if (algo == kSpaceChorus) rate = kSpaceModes[modeFor (kSpaceChorus, prm[kMode])][0];
    if (algo == kVintage)
    {
        const double i = rp (kIntns) / 100.0;
        rate = 0.25 + 4.75 * std::pow (i, 1.7);
    }
    lfo.setRate (rate, fs);
    lfo2.setRate (rate * 1.47, fs);
    for (int b = 0; b < 4; ++b)
    {
        band[b].lfo.setRate (rp (kB1Rate + b * 6), fs);
        band[b].phaseOff = rp (kB1Ph + b * 6) / 360.0;
        band[b].wave = ip (kB1Lfo + b * 6);
    }
    if (algo == kBand2) setBandFilters (2);
    if (algo == kBand3) setBandFilters (3);
    if (algo == kBand4) setBandFilters (4);

    if (algo == kStereo3D)
    {
        posL.setPosition (rp (kAzmtL), rp (kElevL));
        posR.setPosition (rp (kAzmtR), rp (kElevR));
    }
    if (algo == kSpaceChorus) { spLpL.lowpass (kSpaceLpHz, 0.6, fs); spLpR = spLpL; }
    if (algo == kEnsemble)
    {
        const double* m = kEnsModes[modeFor (kEnsemble, prm[kMode])];
        enSlow.setRate (m[0], fs); enFast.setRate (m[2], fs);
        enLpL.lowpass (m[7], 0.7, fs); enLpR.lowpass (m[7], 0.7, fs);
    }
    if (algo == kVintage) { vnLp.lowpass (kVintageLpHz, 0.75, fs); vnLp2.lowpass (kVintageLpHz * 1.4, 0.6, fs); }
    if (algo == kRotary)
    {
        rotXo.setKeep (kRotXoverHz, fs);
        hornLp.lowpass (6000.0, 0.7, fs);
        rotorLp.lowpass (2500.0, 0.7, fs);
        odHp.setLowpass (30.0, fs);
        odLp.setLowpass (5000.0, fs);
    }
    if (algo == kAmbience)
    {
        const int cf = ip (kChFil), af = ip (kAmFil);
        if (cf == 1) chFilL.lowpass (rp (kChFrq), 0.7, fs); else if (cf == 2) chFilL.highpass (rp (kChFrq), 0.7, fs); else chFilL.setIdentity();
        chFilR.b0 = chFilL.b0; chFilR.b1 = chFilL.b1; chFilR.b2 = chFilL.b2; chFilR.a1 = chFilL.a1; chFilR.a2 = chFilL.a2;
        if (af == 1) amFil.lowpass (rp (kAmFrq), 0.7, fs); else if (af == 2) amFil.highpass (rp (kAmFrq), 0.7, fs); else amFil.setIdentity();
        // Decay 1..30 -> RT60 0.1 .. 3.0 s; HF damp -100..0 -> loop low-pass 1 kHz .. 20 kHz (estimates)
        const double rt = 0.1 * rp (kAmDcy);
        const double dampHz = 20000.0 * std::pow (0.05, -rp (kAmDmp) / 100.0);
        for (int i = 0; i < 4; ++i)
        {
            fdnGain[i] = std::pow (10.0, -3.0 * fdnLen[i] / (rt * fs));
            fdnDamp[i].setLowpass (dampHz, fs);
        }
    }
}

//==============================================================================
void Sdx330::process (const float* inL, const float* inR, float* outL, float* outR, int numSamples)
{
    int done = 0;
    while (done < numSamples)
    {
        const int n = (numSamples - done) < kBlock ? (numSamples - done) : kBlock;

        // Algorithm change: fade the effect out, switch, fade back in
        if (swapPending && fade <= 0.0) swapProgram();
        updateControls();

        const bool mono = ip (kInMode) == 1;
        const double tInL = knobGain (prm[kInputL]), tInR = knobGain (prm[kInputR]);
        double dryL[kBlock], dryR[kBlock];
        for (int i = 0; i < n; ++i)
        {
            const double xl = inL[done + i] * gInL.tick (tInL);
            const double xr = inR[done + i] * gInR.tick (tInR);
            const double al = std::fabs (xl), ar = std::fabs (xr);
            peak[0] = al > peak[0] ? al : peak[0] * peakRel;
            peak[1] = ar > peak[1] ? ar : peak[1] * peakRel;
            dryL[i] = softClip (xl);
            dryR[i] = softClip (xr);
            if (mono) { bl[i] = br[i] = 0.5 * (dryL[i] + dryR[i]); }
            else      { bl[i] = dryL[i]; br[i] = dryR[i]; }
            wl[i] = wr[i] = 0.0;
        }

        switch (algo)
        {
            case kStereoChorus: runChorus (bl, br, wl, wr, n, false, false); break;
            case kStereo3D:     runChorus (bl, br, wl, wr, n, true, false); break;
            case kWaveChorus:   runChorus (bl, br, wl, wr, n, false, true); break;
            case kPanner3D:     runPanner (bl, br, wl, wr, n); break;
            case kSpaceChorus:  runSpace (bl, br, wl, wr, n); break;
            case kBand2:        runBands (bl, br, wl, wr, n, 2); break;
            case kBand3:        runBands (bl, br, wl, wr, n, 3); break;
            case kBand4:        runBands (bl, br, wl, wr, n, 4); break;
            case kPhase8:       runPhase8 (bl, br, wl, wr, n); break;
            case kPhase16:      runPhase16 (bl, br, wl, wr, n); break;
            case kDetune:       runDetune (bl, br, wl, wr, n); break;
            case kEnsemble:     runEnsemble (bl, br, wl, wr, n); break;
            case kVintage:      runVintage (bl, br, wl, wr, n); break;
            case kFlanger:      runFlanger (bl, br, wl, wr, n); break;
            case kRotary:       runRotary (bl, br, wl, wr, n); break;
            case kAmbience:     runAmbience (bl, br, wl, wr, n); break;
            default: break;
        }

        // Direct: the program's Dirct, unless the system's Direct Mix forces it on or off.
        // Ensemble, Vintage Chorus and Rotary have no direct path of their own.
        const int dm = ip (kDirectMix);
        const bool algoHasDirect = ! (algo == kEnsemble || algo == kVintage || algo == kRotary);
        double dirT = 0.0;
        if (dm == 0)      dirT = (algoHasDirect && ip (kDirect) == 1) ? 1.0 : 0.0;
        else if (dm == 1) dirT = 1.0;
        const double effT = rp (kELvl) / 100.0;
        const bool total = algo == kSpaceChorus;          // T.Lvl scales direct and effect
        const double tOut = knobGain (prm[kOutput]);
        const double fadeTarget = swapPending ? 0.0 : 1.0;

        for (int i = 0; i < n; ++i)
        {
            if (fade < fadeTarget) { fade += fadeStep; if (fade > 1.0) fade = 1.0; }
            else if (fade > fadeTarget) { fade -= fadeStep; if (fade < 0.0) fade = 0.0; }
            const double e = gEff.tick (effT) * fade;
            const double d = gDir.tick (dirT);
            double yl, yr;
            if (total) { yl = e * wl[i] + d * dryL[i] * gEff.y; yr = e * wr[i] + d * dryR[i] * gEff.y; }
            else       { yl = e * wl[i] + d * dryL[i];          yr = e * wr[i] + d * dryR[i]; }
            yl = eq[0][2].tick (eq[0][1].tick (eq[0][0].tick (yl)));
            yr = eq[1][2].tick (eq[1][1].tick (eq[1][0].tick (yr)));
            const double g = gOut.tick (tOut);
            outL[done + i] = (float) (yl * g);
            outR[done + i] = (float) (yr * g);
        }
        done += n;
    }
}

//==============================================================================
// 1 Stereo Chorus, 2 Stereo 3D Chorus, 12 Wave Chorus
void Sdx330::runChorus (const double* l, const double* r, double* ol, double* orr, int n, bool threeD, bool wave)
{
    const double msToS = fs * 0.001;
    const double pdT = rp (kPrDly), swT = chorusHalfSwingMs (rp (kRate), rp (kDepth));
    const double ph = rp (kPhase) / 360.0;
    int w = ip (kLfo);
    if (! lfoAllowed (wave ? kWaveChorus : kStereoChorus, w)) w = kTri;
    const double xT = (threeD ? 0.0 : rp (kXMix) / 100.0);
    const double plT = wave ? 0.0 : rp (kPanL) / 100.0, prT = wave ? 1.0 : rp (kPanR) / 100.0;
    for (int i = 0; i < n; ++i)
    {
        dlL.write (l[i]); dlR.write (r[i]);
        lfo.tick();
        const double pd = pdS.tick (pdT), sw = swS.tick (swT);
        const double dL = (pd + kChorusBaseMs + 2.0 * sw * lfoShape (w, lfo.phase)) * msToS;
        const double dR = (pd + kChorusBaseMs + 2.0 * sw * lfoShape (w, lfo.phase + ph)) * msToS;
        const double cl = dlL.read (dL), cr = dlR.read (dR);
        if (threeD)
        {
            double al, ar, bl2, br2;
            posL.process (cl, al, ar);
            posR.process (cr, bl2, br2);
            ol[i] = 0.85 * (al + bl2);
            orr[i] = 0.85 * (ar + br2);
        }
        else
        {
            const double x = xmixS.tick (xT);
            const double norm = 1.0 / (1.0 + 0.5 * x);
            const double L = (cl + x * cr) * norm, R = (cr + x * cl) * norm;
            double gll, glr, grl, grr;
            panGains (panLS.tick (plT), gll, glr);
            panGains (panRS.tick (prT), grl, grr);
            ol[i] = L * gll + R * grl;
            orr[i] = L * glr + R * grr;
        }
    }
}

// 3 3D Panner: L+R, pre delay, then a source that circles the listener
void Sdx330::runPanner (const double* l, const double* r, double* ol, double* orr, int n)
{
    const double msToS = fs * 0.001;
    const double pdT = rp (kPrDly);
    const double dir = ip (kPanDir) == 0 ? 1.0 : -1.0;          // CW: left -> front -> right
    const double hz = pannerHz ((int) rp (kPanSpeed));
    panAngle += dir * 360.0 * hz * n / fs;
    while (panAngle > 180.0) panAngle -= 360.0;
    while (panAngle < -180.0) panAngle += 360.0;
    posL.setPosition (panAngle, 0.0);
    for (int i = 0; i < n; ++i)
    {
        dlL.write (l[i]); dlR.write (r[i]);
        const double pd = pdS.tick (pdT);
        double a, b;
        posL.process (0.5 * (dlL.read (pd * msToS + 1.0) + dlR.read (pd * msToS + 1.0)), a, b);
        ol[i] = a * 1.2; orr[i] = b * 1.2;
    }
}

// 4 Space Chorus (after the SDD-320 Dimension D): two delays swept by one triangle LFO in
// opposite directions, each side's wet also fed inverted to the other side
void Sdx330::runSpace (const double* l, const double* r, double* ol, double* orr, int n)
{
    const double msToS = fs * 0.001;
    const double* m = kSpaceModes[modeFor (kSpaceChorus, prm[kMode])];
    const double hs = m[1], g = m[2];
    for (int i = 0; i < n; ++i)
    {
        dlL.write (l[i]); dlR.write (r[i]);
        lfo.tick();
        const double a = lfoShape (kTri, lfo.phase);
        const double wa = spLpL.tick (dlL.read ((kSpaceCentreMs + hs * (2.0 * a - 1.0)) * msToS));
        const double wb = spLpR.tick (dlR.read ((kSpaceCentreMs - hs * (2.0 * a - 1.0)) * msToS));
        ol[i] = g * (wa - kSpaceCross * wb);
        orr[i] = g * (wb - kSpaceCross * wa);
    }
}

// 5/6/7 Band choruses: each band reads the input history at its own chorus delay and is
// then filtered to its slice of the spectrum (the same as splitting first)
void Sdx330::runBands (const double* l, const double* r, double* ol, double* orr, int n, int bands)
{
    const double msToS = fs * 0.001;
    int use[4]; int nb = 0;
    if (bands == 2)      { use[0] = 0; use[1] = 3; nb = 2; }
    else if (bands == 3) { use[0] = 0; use[1] = 1; use[2] = 3; nb = 3; }
    else                 { use[0] = 0; use[1] = 1; use[2] = 2; use[3] = 3; nb = 4; }
    double pdT[4], swT[4], mxT[4];
    for (int b = 0; b < 4; ++b)
    {
        const int o = b * 6;
        pdT[b] = rp (kB1PD + o);
        swT[b] = chorusHalfSwingMs (rp (kB1Rate + o), rp (kB1Depth + o));
        mxT[b] = rp (kB1Mix + o) / 100.0;
    }
    for (int i = 0; i < n; ++i)
    {
        dlL.write (l[i]); dlR.write (r[i]);
        double yl = 0.0, yr = 0.0;
        for (int k = 0; k < nb; ++k)
        {
            Band& B = band[use[k]];
            B.lfo.tick();
            const double pd = B.pd.tick (pdT[use[k]]), sw = B.sw.tick (swT[use[k]]), mx = B.mix.tick (mxT[use[k]]);
            const double dL = (pd + kChorusBaseMs + 2.0 * sw * lfoShape (B.wave, B.lfo.phase)) * msToS;
            const double dR = (pd + kChorusBaseMs + 2.0 * sw * lfoShape (B.wave, B.lfo.phase + B.phaseOff)) * msToS;
            double xl, xr;
            if (bands == 4)
            {
                // mono in: both sides read the sum, at their own LFO phase
                xl = 0.5 * (dlL.read (dL) + dlR.read (dL));
                xr = 0.5 * (dlL.read (dR) + dlR.read (dR));
            }
            else { xl = dlL.read (dL); xr = dlR.read (dR); }
            yl += mx * B.f[0].tick (xl);
            yr += mx * B.f[1].tick (xr);
        }
        ol[i] = yl; orr[i] = yr;
    }
}

// 8 Stereo 8 Phase Chorus: eight taps per side from one LFO, each with its own depth,
// pre delay, LFO phase, L/R polarity and level
void Sdx330::runPhase8 (const double* l, const double* r, double* ol, double* orr, int n)
{
    const double msToS = fs * 0.001;
    const double rate = rp (kRate);
    int w = ip (kLfo); if (w > 1) w = kTri;
    double pdT[8], dpT[8], mxT[8], ph[8]; bool inv[8];
    for (int k = 0; k < 8; ++k)
    {
        pdT[k] = rp (kP8PD1 + k);
        dpT[k] = chorusHalfSwingMs (rate, rp (kP8Dpt1 + k));
        mxT[k] = rp (kP8Mix1 + k) / 100.0;
        ph[k] = rp (kP8Ph1 + k) / 360.0;
        inv[k] = ip (kP8Pol1 + k) == 1;
    }
    for (int i = 0; i < n; ++i)
    {
        dlL.write (l[i]); dlR.write (r[i]);
        lfo.tick();
        double yl = 0.0, yr = 0.0;
        for (int k = 0; k < 8; ++k)
        {
            const double mx = p8mix[k].tick (mxT[k]);
            const double pd = p8pd[k].tick (pdT[k]), sw = p8dp[k].tick (dpT[k]);
            if (mx < 1e-4) continue;
            const double pL = lfo.phase + ph[k], pR = pL + (inv[k] ? 0.5 : 0.0);
            yl += mx * dlL.read ((pd + kChorusBaseMs + 2.0 * sw * lfoShape (w, pL)) * msToS);
            yr += mx * dlR.read ((pd + kChorusBaseMs + 2.0 * sw * lfoShape (w, pR)) * msToS);
        }
        ol[i] = 0.45 * yl; orr[i] = 0.45 * yr;
    }
}

// 9 16 Phase Chorus: sixteen taps, LFO phases 1/16 apart, mono in. The six modes are
// estimates of "how effects are applied": 1 all taps to both sides, 2 alternate taps
// left/right, 3 taps spread across the stereo field, 4 left in phase / right with every
// other tap inverted, 5 two LFO speeds, 6 staggered pre delays spread wide.
void Sdx330::runPhase16 (const double* l, const double* r, double* ol, double* orr, int n)
{
    const double msToS = fs * 0.001;
    const int mode = modeFor (kPhase16, prm[kMode]);
    const double pdT = rp (kPrDly), swT = chorusHalfSwingMs (rp (kRate), rp (kDepth));
    int w = ip (kLfo); if (w > 1) w = kTri;
    for (int i = 0; i < n; ++i)
    {
        dlL.write (l[i]); dlR.write (r[i]);
        lfo.tick(); lfo2.tick();
        const double pd = pdS.tick (pdT), sw = swS.tick (swT);
        double yl = 0.0, yr = 0.0;
        for (int k = 0; k < 16; ++k)
        {
            const double base = pd + kChorusBaseMs + 0.35 * (k & 3) + (mode == 5 ? 1.1 * k : 0.0);
            const double p = (mode == 4 && k >= 8 ? lfo2.phase : lfo.phase) + k / 16.0;
            const double d = (base + 2.0 * sw * lfoShape (w, p)) * msToS;
            const double v = 0.5 * (dlL.read (d) + dlR.read (d));
            switch (mode)
            {
                case 0: yl += v; yr += v; break;
                case 1: case 4: if (k & 1) yr += 1.41 * v; else yl += 1.41 * v; break;
                case 3: yl += v; yr += (k & 1) ? -v : v; break;
                default:
                {
                    double gl, gr;
                    panGains (k / 15.0, gl, gr);
                    yl += 1.41 * gl * v; yr += 1.41 * gr * v;
                    break;
                }
            }
        }
        ol[i] = 0.25 * yl; orr[i] = 0.25 * yr;
    }
}

// 10 Detune Chorus: six pitch shifters (three per side) by two crossfaded taps sweeping
// a long window
void Sdx330::runDetune (const double* l, const double* r, double* ol, double* orr, int n)
{
    const double msToS = fs * 0.001;
    const double W = kDetuneWindowMs * msToS;
    const double pdT = rp (kPrDly);
    double ratio[6], lvT[6], pnT[6];
    for (int k = 0; k < 6; ++k)
    {
        ratio[k] = centsRatio (rp (kDtPit1 + k));
        lvT[k] = rp (kDtLvl1 + k) / 100.0;
        pnT[k] = rp (kDtPan1 + k) / 100.0;
    }
    for (int i = 0; i < n; ++i)
    {
        dlL.write (l[i]); dlR.write (r[i]);
        const double pd = pdS.tick (pdT) * msToS + 1.0;
        double yl = 0.0, yr = 0.0;
        for (int k = 0; k < 6; ++k)
        {
            const double lv = dtLvl[k].tick (lvT[k]), pn = dtPanS[k].tick (pnT[k]);
            const DelayLine& src = k < 3 ? dlL : dlR;
            const double rr = ratio[k];
            double v;
            if (std::fabs (rr - 1.0) < 1e-7)
                v = src.read (pd + 0.5 * W);
            else
            {
                double& ph = dtPhase[k];
                ph += std::fabs (rr - 1.0) / W;
                if (ph >= 1.0) ph -= 1.0;
                const double pa = ph, pb = ph + 0.5 - (ph >= 0.5 ? 1.0 : 0.0);
                const double da = rr > 1.0 ? W * (1.0 - pa) : W * pa;
                const double db = rr > 1.0 ? W * (1.0 - pb) : W * pb;
                const double ga = std::sin (kPi * pa), gb = std::sin (kPi * pb);
                v = ga * ga * src.read (pd + da) + gb * gb * src.read (pd + db);
            }
            if (lv < 1e-5) continue;
            double gl, gr;
            panGains (pn, gl, gr);
            yl += lv * gl * v; yr += lv * gr * v;
        }
        ol[i] = 0.75 * yl; orr[i] = 0.75 * yr;
    }
}

// 11 Ensemble: a string-machine ensemble per side: three taps 120 degrees apart, each swept
// by a slow and a fast LFO; some modes regenerate or diffuse for a reverberant spread
void Sdx330::runEnsemble (const double* l, const double* r, double* ol, double* orr, int n)
{
    const double msToS = fs * 0.001;
    const double* m = kEnsModes[modeFor (kEnsemble, prm[kMode])];
    const double fb = m[5], diff = m[6];
    const int ap0 = (int) (0.0071 * fs), ap1 = (int) (0.0113 * fs);
    for (int i = 0; i < n; ++i)
    {
        dlL.write (l[i]); dlR.write (r[i]);
        enSlow.tick(); enFast.tick();
        double y[2];
        for (int c = 0; c < 2; ++c)
        {
            DelayLine& dl = c == 0 ? enL : enR;
            const double x = c == 0 ? l[i] : r[i];
            const double off = c * (1.0 / 6.0);         // the right side 60 degrees on
            double s = 0.0;
            for (int t = 0; t < 3; ++t)
            {
                const double p = t / 3.0 + off;
                const double d = m[4] + m[1] * (2.0 * sinWave (enSlow.phase + p) - 1.0)
                                     + m[3] * (2.0 * sinWave (enFast.phase + p) - 1.0);
                s += dl.read (d * msToS);
            }
            dl.write (x + fb * 0.4 * s);           // loop gain at most 3 x 0.4 x fb
            s *= 0.75;
            s = c == 0 ? enLpL.tick (s) : enLpR.tick (s);
            if (diff > 0.0)
            {
                // two Schroeder all-passes
                const double g = 0.55;
                for (int a = 0; a < 2; ++a)
                {
                    DelayLine& ap = enAp[c][a];
                    const int len = (a == 0 ? ap0 : ap1) + c * 37;
                    const double dd = ap.tap (len - 1);
                    const double v = s - g * dd;
                    ap.write (v);
                    s = (1.0 - diff) * s + diff * (dd + g * v);
                }
            }
            y[c] = s;
        }
        ol[i] = y[0]; orr[i] = y[1];
    }
}

// 13 Vintage Chorus (after the first chorus, the CE-1): mono in, a BBD-like delay swept by
// a triangle LFO; Intensity raises speed and depth together; the four output modes as the
// manual describes. Its output already holds the dry signal.
void Sdx330::runVintage (const double* l, const double* r, double* ol, double* orr, int n)
{
    const double msToS = fs * 0.001;
    const int mode = modeFor (kVintage, prm[kMode]);
    const double i01 = rp (kIntns) / 100.0;
    const double hs = 0.4 + 2.4 * i01;
    for (int i = 0; i < n; ++i)
    {
        const double x = 0.5 * (l[i] + r[i]);
        dlL.write (l[i]); dlR.write (r[i]);
        lfo.tick();
        const double a = lfoShape (kTri, lfo.phase);
        const double da = (kVintageCentreMs + hs * (2.0 * a - 1.0)) * msToS;
        // the BBD input stage rounds big peaks (memoryless, so it can follow the delay)
        const double wa = vnLp2.tick (vnLp.tick (std::tanh (0.6 * (dlL.read (da) + dlR.read (da))) / 1.2));
        double wb = wa;
        if (mode == 1)
        {
            const double db = (kVintageCentreMs + hs * (2.0 * (1.0 - a) - 1.0)) * msToS;
            wb = std::tanh (0.6 * (dlL.read (db) + dlR.read (db))) / 1.2;
        }
        const double dry = x;
        switch (mode)
        {
            case 0: ol[i] = 0.7 * (dry + wa); orr[i] = 0.7 * (dry + wa); break;
            case 1: ol[i] = 0.7 * (dry + wa); orr[i] = 0.7 * (dry + wb); break;
            case 2: ol[i] = 0.7 * (dry + wa); orr[i] = 0.7 * (dry - wa); break;
            default: ol[i] = dry; orr[i] = wa; break;
        }
    }
}

// 14 Stereo Flanger: Manual sets the delay, the LFO sweeps around it, Resonance feeds back
// (negative values invert the feedback)
void Sdx330::runFlanger (const double* l, const double* r, double* ol, double* orr, int n)
{
    const double msToS = fs * 0.001;
    const double depth = rp (kDepth) / 100.0, ph = rp (kPhase) / 360.0;
    int w = ip (kLfo);
    if (! lfoAllowed (kFlanger, w)) w = kTri;
    const double posT = rp (kManual) / 100.0, fbT = kFlangerMaxFb * rp (kResnc) / 100.0;
    for (int i = 0; i < n; ++i)
    {
        lfo.tick();
        const double pos = flPos.tick (posT), fb = flFb.tick (fbT);
        const double pL = clampd (pos + depth * (lfoShape (w, lfo.phase) - 0.5), 0.0, 1.0);
        const double pR = clampd (pos + depth * (lfoShape (w, lfo.phase + ph) - 0.5), 0.0, 1.0);
        dlL.write (l[i]); dlR.write (r[i]);
        const double dL = flL.read (flangerDelayMs (pL) * msToS);
        const double dR = flR.read (flangerDelayMs (pR) * msToS);
        flL.write (l[i] + std::tanh (fb * dL));
        flR.write (r[i] + std::tanh (fb * dR));
        ol[i] = dL; orr[i] = dR;
    }
}

// 15 Rotary: overdrive, an 800 Hz crossover, a horn and a rotor, each with Doppler (a swept
// delay) and tremolo, picked up by two mics; speed changes ramp with the rise / fall times
void Sdx330::runRotary (const double* l, const double* r, double* ol, double* orr, int n)
{
    const double msToS = fs * 0.001;
    const bool fast = ip (kRotSpeed) == 1;
    const double hT = fast ? rp (kHFast) : rp (kHSlow);
    const double rT = fast ? rp (kRFast) : rp (kRSlow);
    // Rise / fall 1..100 -> time constant 0.05 .. 5 s (estimate; the real horn takes about 1 s)
    const double hTau = 0.05 + 4.95 * ((hT > hornHz ? rp (kHRise) : rp (kHFall)) - 1.0) / 99.0;
    const double rTau = 0.05 + 4.95 * ((rT > rotorHz ? rp (kRRise) : rp (kRFall)) - 1.0) / 99.0;
    const double hk = 1.0 - std::exp (-(double) n / (hTau * fs));
    const double rk = 1.0 - std::exp (-(double) n / (rTau * fs));
    hornHz += (hT - hornHz) * hk;
    rotorHz += (rT - rotorHz) * rk;
    const bool onMic = ip (kMicMode) == 1;
    const double hD = rp (kHDepth) / 100.0 * kHornDopplerMs * (onMic ? 1.0 : 0.8);
    const double rD = rp (kRDepth) / 100.0 * kRotorDopplerMs * (onMic ? 1.0 : 0.8);
    const double hTr = rp (kHTrem) / 100.0 * (onMic ? 0.85 : 0.4);
    const double rTr = rp (kRTrem) / 100.0 * (onMic ? 0.7 : 0.35);
    const double dif = rp (kDiffusion) / 100.0;
    const double micOff = 0.15 + 0.35 * dif;                   // mic spacing round the cabinet, turns
    const double room = onMic ? 0.10 * dif : 0.25 + 0.25 * dif;
    const bool od = ip (kOdSw) == 1;
    const double odG = rp (kOdGain) / 100.0 * 4.0, odD = rp (kOdDrive) / 100.0, odL = rp (kOdLevel) / 100.0;
    const double balT = rp (kRHBal);
    const int r1 = (int) (0.013 * fs), r2 = (int) (0.017 * fs);
    for (int i = 0; i < n; ++i)
    {
        double x = 0.5 * (l[i] + r[i]);
        dlL.write (l[i]); dlR.write (r[i]);
        if (od)
        {
            // Overdrive: Gain into a valve-like asymmetric curve (0 = no sound, as the manual says),
            // Drive blends in the distortion, Level sets its output
            const double pre = odHp.hp (x * odG);
            const double sat = (std::tanh (2.5 * pre + 0.2) - std::tanh (0.2)) / 2.5;
            x = odLp.lp (((1.0 - odD) * pre + odD * sat * 1.6)) * odL * 1.3;
        }
        double lo, hi;
        rotXo.split (x, lo, hi);
        hornDl.write (hornLp.tick (hi));
        rotorDl.write (rotorLp.tick (lo));
        hornPh += hornHz / fs; if (hornPh >= 1.0) hornPh -= 1.0;
        rotorPh += rotorHz / fs; if (rotorPh >= 1.0) rotorPh -= 1.0;
        const double bal = rotBal.tick (balT);
        const double gR = (50.0 - bal) / 50.0, gH = (50.0 + bal) / 50.0;
        double mic[2];
        for (int c = 0; c < 2; ++c)
        {
            const double o = c == 0 ? 0.0 : micOff;
            const double hp = kTwoPi * (hornPh + o), rp2 = kTwoPi * (rotorPh + o);
            const double hs = hornDl.read ((1.2 + hD * std::sin (hp)) * msToS);
            const double rs = rotorDl.read ((1.2 + rD * std::sin (rp2)) * msToS);
            const double ha = 1.0 - hTr * 0.5 * (1.0 - std::cos (hp));
            const double ra = 1.0 - rTr * 0.5 * (1.0 - std::cos (rp2));
            mic[c] = gH * hs * ha + gR * rs * ra;
        }
        // A little room around the cabinet (more off mic, and with Diffusion)
        roomDl[0].write (mic[0]); roomDl[1].write (mic[1]);
        const double yl = mic[0] + room * (roomDl[1].tap (r1) - 0.6 * roomDl[0].tap (r2));
        const double yr = mic[1] + room * (roomDl[0].tap (r2) - 0.6 * roomDl[1].tap (r1));
        ol[i] = yl; orr[i] = yr;
    }
}

// 16 Ambience Chorus: a stereo chorus (with its own filter and level) plus an "ambience",
// a small room built from two diffusers and a 4-line feedback delay network
void Sdx330::runAmbience (const double* l, const double* r, double* ol, double* orr, int n)
{
    const double msToS = fs * 0.001;
    const double pdT = rp (kPrDly), swT = chorusHalfSwingMs (rp (kRate), rp (kDepth));
    const double ph = rp (kPhase) / 360.0;
    int w = ip (kLfo); if (w > 1) w = kTri;
    const double chL = rp (kChLvl) / 100.0, amL = rp (kAmLvl) / 100.0;
    const double g = 0.3 + 0.45 * rp (kAmDns) / 100.0;
    for (int i = 0; i < n; ++i)
    {
        dlL.write (l[i]); dlR.write (r[i]);
        lfo.tick();
        const double pd = pdS.tick (pdT), sw = swS.tick (swT);
        const double cl = chFilL.tick (dlL.read ((pd + kChorusBaseMs + 2.0 * sw * lfoShape (w, lfo.phase)) * msToS));
        const double cr = chFilR.tick (dlR.read ((pd + kChorusBaseMs + 2.0 * sw * lfoShape (w, lfo.phase + ph)) * msToS));
        // Ambience input: the pre-delayed sum, filtered, diffused
        double a = amFil.tick (0.5 * (dlL.read (pd * msToS + 1.0) + dlR.read (pd * msToS + 1.0)));
        for (int k = 0; k < 2; ++k)
        {
            const double dd = amAp[k].tap (apLen[k] - 1);
            const double v = a - g * dd;
            amAp[k].write (v);
            a = dd + g * v;
        }
        double d[4], s = 0.0;
        for (int k = 0; k < 4; ++k) { d[k] = fdnDamp[k].lp (fdn[k].tap (fdnLen[k] - 1)) * fdnGain[k]; s += d[k]; }
        s *= 0.5;                                   // Householder: x - 2/N sum
        for (int k = 0; k < 4; ++k) fdn[k].write ((k & 1 ? -a : a) * 0.5 + d[k] - s);
        const double al = 0.6 * (d[0] + d[2]), ar = 0.6 * (d[1] + d[3]);
        ol[i] = chL * cl + amL * al;
        orr[i] = chL * cr + amL * ar;
    }
}

} // namespace tbsdx
