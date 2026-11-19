// Sdx330.h
// Roland SDX-330 Dimensional Expander (1994): a 16-bit, 44.1 kHz digital multi-chorus
// with 16 algorithms. There is no schematic or published DSP code, so this is a
// behavioural model built from the Owner's Manual: each algorithm follows its block
// diagram and parameter list (chapter 3) and the parameter descriptions (chapter 4).
//
//   INPUT LEVEL L / R -> converter (clips at full scale; the level meter reads here)
//     -> INPUT MODE (stereo, or L+R to both)
//     -> the algorithm (wet) ...........................  x EFFECT LEVEL --+
//     -> DIRECT (dry, on/off; the system's Direct Mix can force it) -------+--> 3-band EQ
//     -> OUTPUT LEVEL
//
// Every value the manual doesn't fix (chorus delay ranges, the Space Chorus modes, the
// 16 Phase and Ensemble modes, the 3D processing, rotary speaker dimensions...) is a
// named estimate in Sdx330.cpp and listed in the README.
//
// Parameters are set as the plugin's 0..1 values (Params.h); the engine converts them.
// Stereo in, stereo out. Plain C++98; buffers are allocated in prepare() only.

#ifndef TBSDX_SDX330_H
#define TBSDX_SDX330_H

#include "Blocks.h"
#include "../VST2/Params.h"

namespace tbsdx
{

class Sdx330
{
public:
    Sdx330();

    void prepare (double sampleRate);
    void reset();

    void  setParam (int index, float value);        // 0..1
    float getParam (int index) const { return tgt[index]; }

    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples);

    static int latencySamples() { return 0; }

    // Peak level at the converter's input (1.0 = full scale), per side, for the meter
    float meter (int side) const { return (float) peak[side & 1]; }

    // Introspection for the tests and the README
    static double knobGain (double knob01);                       // INPUT / OUTPUT LEVEL
    static double chorusHalfSwingMs (double rateHz, double depth); // depth 0..100
    static double flangerDelayMs (double pos01);
    static double pannerHz (int speed);
    int  currentAlgo() const { return algo; }

private:
    enum { kBlock = 32 };

    struct Voice                // one modulated delay read: predelay + base + swing x LFO
    {
        Smooth pd, sw;
        double phase;           // LFO phase offset, 0..1
    };

    struct BandFilter           // a band's place in the crossover cascade, after its delay
    {
        Biquad st[3][2];        // up to three LR4 stages (two biquads each)
        int stages;
        inline double tick (double x)
        {
            for (int k = 0; k < stages; ++k) x = st[k][1].tick (st[k][0].tick (x));
            return x;
        }
    };

    struct Band                 // one band of the 2/3/4 Band Chorus
    {
        BandFilter f[2];
        Lfo lfo;
        Smooth pd, sw, mix;
        double phaseOff;
        int wave;
    };

    void updateControls();
    void updateEq();
    void swapProgram();
    void snapControls();
    void clearMain();
    void clearTails();

    // Algorithms: write the wet signal (and, for the Space Chorus and Vintage Chorus,
    // their own dry mix) for n samples
    void runChorus (const double* l, const double* r, double* wl, double* wr, int n, bool threeD, bool wave);
    void runPanner (const double* l, const double* r, double* wl, double* wr, int n);
    void runSpace (const double* l, const double* r, double* wl, double* wr, int n);
    void runBands (const double* l, const double* r, double* wl, double* wr, int n, int bands);
    void runPhase8 (const double* l, const double* r, double* wl, double* wr, int n);
    void runPhase16 (const double* l, const double* r, double* wl, double* wr, int n);
    void runDetune (const double* l, const double* r, double* wl, double* wr, int n);
    void runEnsemble (const double* l, const double* r, double* wl, double* wr, int n);
    void runVintage (const double* l, const double* r, double* wl, double* wr, int n);
    void runFlanger (const double* l, const double* r, double* wl, double* wr, int n);
    void runRotary (const double* l, const double* r, double* wl, double* wr, int n);
    void runAmbience (const double* l, const double* r, double* wl, double* wr, int n);

    double rp (int p) const { return toReal (p, prm[p]); }
    int    ip (int p) const { return toIndex (p, prm[p]); }

    double fs;
    float  tgt[kNumParams];         // as set by the host / panel
    float  prm[kNumParams];         // what the algorithm is running with
    bool   eqDirty;
    int    algo;
    bool   swapPending;             // a program or algorithm change waits for the effect to fade out
    double fade, fadeStep;          // effect gain while changing programs

    // Front panel and mixing
    Smooth gInL, gInR, gOut, gEff, gDir;
    double peak[2], peakRel;

    // Input history, written by every algorithm every sample so a new program never reads stale audio
    DelayLine dlL, dlR;
    Lfo lfo;
    Smooth pdS, swS, xmixS, panLS, panRS;

    // 3D
    Positioner posL, posR;
    int posCount;
    double panAngle;                // 3D Panner, degrees

    // Space Chorus
    Biquad spLpL, spLpR;

    // Band choruses
    Band band[4];
    void setBandFilters (int bands);

    // 8 Phase
    Smooth p8pd[8], p8dp[8], p8mix[8];

    // 16 Phase
    Lfo lfo2;

    // Detune
    double dtPhase[6];
    Smooth dtLvl[6], dtPanS[6];

    // Ensemble (own lines: they regenerate)
    DelayLine enL, enR;
    Lfo enSlow, enFast;
    Biquad enLpL, enLpR;
    DelayLine enAp[2][2];

    // Vintage
    Biquad vnLp, vnLp2;

    // Flanger (own lines: they carry the feedback)
    DelayLine flL, flR;
    Smooth flPos, flFb;
    double flLastL, flLastR;

    // Rotary
    DelayLine hornDl, rotorDl, roomDl[2];
    Crossover rotXo;
    Biquad hornLp, rotorLp;
    double hornPh, rotorPh, hornHz, rotorHz;
    OnePole odHp, odLp;
    Smooth rotBal;

    // Ambience
    Biquad chFilL, chFilR, amFil;
    DelayLine fdn[4], amAp[2];
    OnePole fdnDamp[4];
    double fdnGain[4];
    int fdnLen[4], apLen[2];

    // EQ
    Biquad eq[2][3];

    // Scratch for one block
    double bl[kBlock], br[kBlock], wl[kBlock], wr[kBlock];
};

} // namespace tbsdx

#endif
