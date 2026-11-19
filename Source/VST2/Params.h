// Params.h
// Parameter table shared by the DSP, the VST wrapper, the editor, the factory programs
// and the program store.
//
// The SDX-330 is a program-based digital unit: 16 algorithms, each with its own set of
// parameters edited through the LCD. The plugin exposes the union of every algorithm's
// parameters as one flat list (parameters with the same meaning and range are shared,
// e.g. Rate / Depth / Pre Delay), so each one can be automated and every program is just
// a full set of values. Each entry carries the manual's LCD name and range (Owner's
// Manual, chapter 3) and how the value is shown.
//
// Order: front-panel knobs and switches, system settings, the program's common part
// (algorithm, effect level, direct, input mode, the 3-band EQ), then each algorithm's
// own parameters. New parameters go on the end, so saved sessions and programs keep
// working.
//
// Plain C++98, header only.

#ifndef TBSDX_PARAMS_H
#define TBSDX_PARAMS_H

#include <cmath>
#include <cstdio>
#include <cstring>

namespace tbsdx
{

enum Param
{
    // Front panel
    kInputL = 0, kInputR, kOutput, kBypass,
    // System / plugin state
    kProgram, kDirectMix,
    // Program: common
    kAlgo, kELvl, kDirect, kInMode,
    kLowFq, kLowGn, kLowQ, kMidFq, kMidGn, kMidQ, kHigFq, kHigGn, kHigQ,
    // Chorus (shared by Stereo Chorus, 3D Chorus, 16 Phase, Wave, Flanger, Ambience, 8 Phase rate/LFO)
    kRate, kDepth, kPrDly, kLfo, kPhase, kXMix, kPanL, kPanR,
    // 3D effects
    kAzmtL, kElevL, kAzmtR, kElevR,
    // 3D Panner
    kPanSpeed, kPanDir, kPanStart,
    // Mode (Space Chorus, 16 Phase, Ensemble, Vintage)
    kMode,
    // Band choruses: 4 bands x (Rate, Depth, Pre delay, LFO, Phase, Mix)
    kB1Rate, kB1Depth, kB1PD, kB1Lfo, kB1Ph, kB1Mix,
    kB2Rate, kB2Depth, kB2PD, kB2Lfo, kB2Ph, kB2Mix,
    kB3Rate, kB3Depth, kB3PD, kB3Lfo, kB3Ph, kB3Mix,
    kB4Rate, kB4Depth, kB4PD, kB4Lfo, kB4Ph, kB4Mix,
    kXov1, kXov2, kXov3,
    // Stereo 8 Phase Chorus: 8 x Depth, Pre delay, LFO phase, Polarity, Mix
    kP8Dpt1, kP8PD1 = kP8Dpt1 + 8, kP8Ph1 = kP8PD1 + 8, kP8Pol1 = kP8Ph1 + 8, kP8Mix1 = kP8Pol1 + 8,
    // Detune Chorus: L1 L2 L3 R1 R2 R3 x (Pitch, Pan, Level)
    kDtPit1 = kP8Mix1 + 8, kDtPan1 = kDtPit1 + 6, kDtLvl1 = kDtPan1 + 6,
    // Vintage Chorus
    kIntns = kDtLvl1 + 6,
    // Stereo Flanger
    kManual, kResnc,
    // Rotary
    kRotSpeed, kHFast, kRFast, kHSlow, kRSlow, kHRise, kRRise, kHFall, kRFall, kRHBal, kMicMode,
    kHDepth, kRDepth, kHTrem, kRTrem, kOdSw, kOdGain, kOdDrive, kOdLevel, kDiffusion,
    // Ambience Chorus
    kChFil, kChFrq, kChLvl, kAmDcy, kAmDmp, kAmDns, kAmFil, kAmFrq, kAmLvl,
    kNumParams
};

enum { kNumAlgos = 16, kNumFactory = 100, kNumUser = 200, kNumPrograms = 300 };

// Algorithms, numbered as in the manual (1..16); stored as 0..15
enum Algo
{
    kStereoChorus = 0, kStereo3D, kPanner3D, kSpaceChorus, kBand2, kBand3, kBand4, kPhase8,
    kPhase16, kDetune, kEnsemble, kWaveChorus, kVintage, kFlanger, kRotary, kAmbience
};

const char* const kAlgoNames[kNumAlgos] = {
    "Stereo Chorus", "Stereo 3D Chorus", "3D Panner", "Space Chorus", "2 Band Chorus",
    "3 Band Chorus", "4 Band Chorus", "St 8 Phase Chorus", "16 Phase Chorus", "Detune Chorus",
    "Ensemble", "Wave Chorus", "Vintage Chorus", "Stereo Flanger", "Rotary", "Ambience Chorus"
};

// How a stored 0..1 value maps to the unit's value
enum Taper { tLin, tLog, tSq, tChoice };

// How the value is shown
enum Unit
{
    uNum,       // plain number, `res` decimals
    uHz,        // 0.01 .. 40.00 Hz style, or 1.50k
    uMs,        // ms
    uDeg,       // degrees
    udB,        // signed dB
    uPan,       // 100:0 .. 0:100 (left:right)
    uAzim,      // R180 .. 0 .. L180 / L180 .. R180
    uElev,      // signed
    uSigned,    // signed number
    uBal,       // 90:10 .. 10:90
    uChoice,    // names
    uKnob       // front-panel knob 0..10
};

struct ParamInfo
{
    const char* vst;        // VST name, <= 8 characters
    const char* lcd;        // LCD name as in the manual, <= 6 characters
    int    taper;
    double lo, hi;          // unit's range (choices: 0 .. n-1)
    int    unit;
    int    decimals;
    double step;            // the alpha dial's step in the unit's terms (0 = 1/200 of the knob)
    double def;             // default in the unit's terms
    const char* const* choices;
};

const char* const kOnOff[2]      = { "OFF", "ON" };
const char* const kStereoMono[2] = { "STERO", "MONO" };        // the LCD spells it STERO
const char* const kDirMix[3]     = { "PROG", "ON", "OFF" };
const char* const kLfoNames[8]   = { "TRI", "SIN", "EXP", "OCW1", "OCW2", "OCW3", "OFW1", "OFW2" };
const char* const kQNames[12]    = { "SHELV", "0.3", "0.5", "0.7", "1.0", "1.4", "2.0", "2.8", "4.0", "5.6", "8.0", "10.0" };
const char* const kMidQNames[11] = { "0.3", "0.5", "0.7", "1.0", "1.4", "2.0", "2.8", "4.0", "5.6", "8.0", "10.0" };
const double kQValues[12]        = { 0.0, 0.3, 0.5, 0.7, 1.0, 1.4, 2.0, 2.8, 4.0, 5.6, 8.0, 10.0 };
const char* const kModeNames[7]  = { "1", "2", "3", "4", "1+4", "2+4", "3+4" };
const char* const kDirNames[2]   = { "CW", "CCW" };
const char* const kPolNames[2]   = { "NORML", "INV" };
const char* const kSpeedNames[2] = { "SLOW", "FAST" };
const char* const kMicNames[2]   = { "OfMic", "OnMic" };
const char* const kFilNames[3]   = { "OFF", "LPF", "HPF" };
const char* const kBypNames[2]   = { "In", "Bypass" };

#define TBSDX_CH(n, c) n, c

const ParamInfo kInfo[kNumParams] = {
    // vst        lcd      taper    lo      hi      unit     dec  step   def     choices
    { "In L",     "InL",   tLin,    0,      10,     uKnob,   1,   0,     5,      0 },
    { "In R",     "InR",   tLin,    0,      10,     uKnob,   1,   0,     5,      0 },
    { "Output",   "Out",   tLin,    0,      10,     uKnob,   1,   0,     5,      0 },
    { "Bypass",   "Bypas", tChoice, 0,      1,      uChoice, 0,   1,     0,      kBypNames },
    { "Program",  "Prog",  tLin,    1,      300,    uNum,    0,   1,     201,    0 },
    { "DirMix",   "Direct",tChoice, 0,      2,      uChoice, 0,   1,     0,      kDirMix },

    { "Algo",     "Algo",  tLin,    1,      16,     uNum,    0,   1,     1,      0 },
    { "E.Lvl",    "E.Lvl", tLin,    0,      100,    uNum,    0,   1,     100,    0 },
    { "Direct",   "Dirct", tChoice, 0,      1,      uChoice, 0,   1,     1,      kOnOff },
    { "InMode",   "Input", tChoice, 0,      1,      uChoice, 0,   1,     0,      kStereoMono },
    { "LowFq",    "LowFq", tLog,    20,     2000,   uHz,     0,   0,     200,    0 },
    { "LowGn",    "LowGn", tLin,    -12,    12,     udB,     1,   0.5,   0,      0 },
    { "Low_Q",    "Low_Q", tChoice, 0,      11,     uChoice, 0,   1,     0,      kQNames },
    { "MidFq",    "MidFq", tLog,    200,    8000,   uHz,     0,   0,     1000,   0 },
    { "MidGn",    "MidGn", tLin,    -12,    12,     udB,     1,   0.5,   0,      0 },
    { "Mid_Q",    "Mid_Q", tChoice, 0,      10,     uChoice, 0,   1,     3,      kMidQNames },
    { "HigFq",    "HigFq", tLog,    1500,   20000,  uHz,     0,   0,     6000,   0 },
    { "HigGn",    "HigGn", tLin,    -12,    12,     udB,     1,   0.5,   0,      0 },
    { "Hig_Q",    "Hig_Q", tChoice, 0,      11,     uChoice, 0,   1,     0,      kQNames },

    { "Rate",     "Rate",  tLog,    0.01,   40,     uHz,     2,   0,     0.65,   0 },
    { "Depth",    "Depth", tLin,    0,      100,    uNum,    0,   1,     50,     0 },
    { "PreDly",   "PrDly", tSq,     0,      200,    uMs,     1,   0.1,   8,      0 },
    { "LFO",      "LFO",   tChoice, 0,      7,      uChoice, 0,   1,     0,      kLfoNames },
    { "Phase",    "Phase", tLin,    0,      360,    uDeg,    0,   1,     180,    0 },
    { "XMix",     "XMix",  tLin,    0,      100,    uNum,    0,   1,     0,      0 },
    { "Pan-L",    "Pan-L", tLin,    0,      100,    uPan,    0,   1,     0,      0 },
    { "Pan-R",    "Pan-R", tLin,    0,      100,    uPan,    0,   1,     100,    0 },

    { "AzmtL",    "AzmtL", tLin,    -180,   180,    uAzim,   0,   1,     -90,    0 },
    { "ElevL",    "ElevL", tLin,    -60,    60,     uElev,   0,   1,     0,      0 },
    { "AzmtR",    "AzmtR", tLin,    -180,   180,    uAzim,   0,   1,     90,     0 },
    { "ElevR",    "ElevR", tLin,    -60,    60,     uElev,   0,   1,     0,      0 },

    { "PanSpeed", "Speed", tLin,    1,      10,     uNum,    0,   1,     3,      0 },
    { "PanDir",   "Dir",   tChoice, 0,      1,      uChoice, 0,   1,     0,      kDirNames },
    { "PanStart", "Start", tLin,    -180,   180,    uAzim,   0,   1,     -90,    0 },

    { "Mode",     "Mode",  tChoice, 0,      6,      uChoice, 0,   1,     0,      kModeNames },

    { "Lo-Rt",    "Lo-Rt", tLog,    0.01,   40,     uHz,     2,   0,     0.4,    0 },
    { "Lo-Dp",    "Lo-Dp", tLin,    0,      100,    uNum,    0,   1,     20,     0 },
    { "Lo-PD",    "Lo-PD", tSq,     0,      200,    uMs,     1,   0.1,   6,      0 },
    { "LoLFO",    "LoLFO", tChoice, 0,      1,      uChoice, 0,   1,     1,      kLfoNames },
    { "Lo-Ph",    "Lo-Ph", tLin,    0,      360,    uDeg,    0,   1,     180,    0 },
    { "LoMix",    "LoMix", tLin,    0,      120,    uNum,    0,   1,     100,    0 },
    { "LM-Rt",    "Md-Rt", tLog,    0.01,   40,     uHz,     2,   0,     0.6,    0 },
    { "LM-Dp",    "Md-Dp", tLin,    0,      100,    uNum,    0,   1,     40,     0 },
    { "LM-PD",    "Md-PD", tSq,     0,      200,    uMs,     1,   0.1,   8,      0 },
    { "LMLFO",    "MdLFO", tChoice, 0,      1,      uChoice, 0,   1,     0,      kLfoNames },
    { "LM-Ph",    "Md-Ph", tLin,    0,      360,    uDeg,    0,   1,     180,    0 },
    { "LMMix",    "MdMix", tLin,    0,      120,    uNum,    0,   1,     100,    0 },
    { "HM-Rt",    "HM-Rt", tLog,    0.01,   40,     uHz,     2,   0,     0.8,    0 },
    { "HM-Dp",    "HM-Dp", tLin,    0,      100,    uNum,    0,   1,     50,     0 },
    { "HM-PD",    "HM-PD", tSq,     0,      200,    uMs,     1,   0.1,   10,     0 },
    { "HMLFO",    "HMLFO", tChoice, 0,      1,      uChoice, 0,   1,     0,      kLfoNames },
    { "HM-Ph",    "HM-Ph", tLin,    0,      360,    uDeg,    0,   1,     180,    0 },
    { "HMMix",    "HMMix", tLin,    0,      120,    uNum,    0,   1,     100,    0 },
    { "Hi-Rt",    "Hi-Rt", tLog,    0.01,   40,     uHz,     2,   0,     1.0,    0 },
    { "Hi-Dp",    "Hi-Dp", tLin,    0,      100,    uNum,    0,   1,     60,     0 },
    { "Hi-PD",    "Hi-PD", tSq,     0,      200,    uMs,     1,   0.1,   12,     0 },
    { "HiLFO",    "HiLFO", tChoice, 0,      1,      uChoice, 0,   1,     0,      kLfoNames },
    { "Hi-Ph",    "Hi-Ph", tLin,    0,      360,    uDeg,    0,   1,     180,    0 },
    { "HiMix",    "HiMix", tLin,    0,      120,    uNum,    0,   1,     100,    0 },
    { "Xov1",     "LMXov", tLog,    100,    1000,   uHz,     0,   0,     250,    0 },
    { "Xov2",     "Xover", tLog,    400,    4000,   uHz,     0,   0,     800,    0 },
    { "Xov3",     "MHXov", tLog,    800,    8000,   uHz,     0,   0,     2500,   0 },

    // 8 Phase: Depth 1-8
    { "Dpt-1", "Dpt-1", tLin, 0, 100, uNum, 0, 1, 50, 0 }, { "Dpt-2", "Dpt-2", tLin, 0, 100, uNum, 0, 1, 50, 0 },
    { "Dpt-3", "Dpt-3", tLin, 0, 100, uNum, 0, 1, 50, 0 }, { "Dpt-4", "Dpt-4", tLin, 0, 100, uNum, 0, 1, 50, 0 },
    { "Dpt-5", "Dpt-5", tLin, 0, 100, uNum, 0, 1, 50, 0 }, { "Dpt-6", "Dpt-6", tLin, 0, 100, uNum, 0, 1, 50, 0 },
    { "Dpt-7", "Dpt-7", tLin, 0, 100, uNum, 0, 1, 50, 0 }, { "Dpt-8", "Dpt-8", tLin, 0, 100, uNum, 0, 1, 50, 0 },
    // Pre Delay 1-8 (0..60 ms)
    { "PD-1", "PD-1", tSq, 0, 60, uMs, 1, 0.1, 2,  0 }, { "PD-2", "PD-2", tSq, 0, 60, uMs, 1, 0.1, 5,  0 },
    { "PD-3", "PD-3", tSq, 0, 60, uMs, 1, 0.1, 8,  0 }, { "PD-4", "PD-4", tSq, 0, 60, uMs, 1, 0.1, 11, 0 },
    { "PD-5", "PD-5", tSq, 0, 60, uMs, 1, 0.1, 3,  0 }, { "PD-6", "PD-6", tSq, 0, 60, uMs, 1, 0.1, 6,  0 },
    { "PD-7", "PD-7", tSq, 0, 60, uMs, 1, 0.1, 9,  0 }, { "PD-8", "PD-8", tSq, 0, 60, uMs, 1, 0.1, 12, 0 },
    // LFO Phase 1-8
    { "Ph-1", "Ph-1", tLin, 0, 360, uDeg, 0, 1, 0,   0 }, { "Ph-2", "Ph-2", tLin, 0, 360, uDeg, 0, 1, 45,  0 },
    { "Ph-3", "Ph-3", tLin, 0, 360, uDeg, 0, 1, 90,  0 }, { "Ph-4", "Ph-4", tLin, 0, 360, uDeg, 0, 1, 135, 0 },
    { "Ph-5", "Ph-5", tLin, 0, 360, uDeg, 0, 1, 180, 0 }, { "Ph-6", "Ph-6", tLin, 0, 360, uDeg, 0, 1, 225, 0 },
    { "Ph-7", "Ph-7", tLin, 0, 360, uDeg, 0, 1, 270, 0 }, { "Ph-8", "Ph-8", tLin, 0, 360, uDeg, 0, 1, 315, 0 },
    // Polarity 1-8
    { "Pol-1", "Pol-1", tChoice, 0, 1, uChoice, 0, 1, 1, kPolNames }, { "Pol-2", "Pol-2", tChoice, 0, 1, uChoice, 0, 1, 1, kPolNames },
    { "Pol-3", "Pol-3", tChoice, 0, 1, uChoice, 0, 1, 1, kPolNames }, { "Pol-4", "Pol-4", tChoice, 0, 1, uChoice, 0, 1, 1, kPolNames },
    { "Pol-5", "Pol-5", tChoice, 0, 1, uChoice, 0, 1, 1, kPolNames }, { "Pol-6", "Pol-6", tChoice, 0, 1, uChoice, 0, 1, 1, kPolNames },
    { "Pol-7", "Pol-7", tChoice, 0, 1, uChoice, 0, 1, 1, kPolNames }, { "Pol-8", "Pol-8", tChoice, 0, 1, uChoice, 0, 1, 1, kPolNames },
    // Mix Level 1-8
    { "Mix-1", "Mix-1", tLin, 0, 100, uNum, 0, 1, 100, 0 }, { "Mix-2", "Mix-2", tLin, 0, 100, uNum, 0, 1, 100, 0 },
    { "Mix-3", "Mix-3", tLin, 0, 100, uNum, 0, 1, 100, 0 }, { "Mix-4", "Mix-4", tLin, 0, 100, uNum, 0, 1, 100, 0 },
    { "Mix-5", "Mix-5", tLin, 0, 100, uNum, 0, 1, 0,   0 }, { "Mix-6", "Mix-6", tLin, 0, 100, uNum, 0, 1, 0,   0 },
    { "Mix-7", "Mix-7", tLin, 0, 100, uNum, 0, 1, 0,   0 }, { "Mix-8", "Mix-8", tLin, 0, 100, uNum, 0, 1, 0,   0 },

    // Detune: pitch (cents) L1 L2 L3 R1 R2 R3
    { "L1Pit", "L1Pit", tLin, -100, 100, uSigned, 0, 1, -8,  0 }, { "L2Pit", "L2Pit", tLin, -100, 100, uSigned, 0, 1, 6,   0 },
    { "L3Pit", "L3Pit", tLin, -100, 100, uSigned, 0, 1, -14, 0 }, { "R1Pit", "R1Pit", tLin, -100, 100, uSigned, 0, 1, 8,   0 },
    { "R2Pit", "R2Pit", tLin, -100, 100, uSigned, 0, 1, -6,  0 }, { "R3Pit", "R3Pit", tLin, -100, 100, uSigned, 0, 1, 14,  0 },
    // Pan
    { "L1Pan", "L1Pan", tLin, 0, 100, uPan, 0, 1, 0,   0 }, { "L2Pan", "L2Pan", tLin, 0, 100, uPan, 0, 1, 25,  0 },
    { "L3Pan", "L3Pan", tLin, 0, 100, uPan, 0, 1, 40,  0 }, { "R1Pan", "R1Pan", tLin, 0, 100, uPan, 0, 1, 100, 0 },
    { "R2Pan", "R2Pan", tLin, 0, 100, uPan, 0, 1, 75,  0 }, { "R3Pan", "R3Pan", tLin, 0, 100, uPan, 0, 1, 60,  0 },
    // Mix level
    { "L1Lvl", "L1Lvl", tLin, 0, 100, uNum, 0, 1, 80, 0 }, { "L2Lvl", "L2Lvl", tLin, 0, 100, uNum, 0, 1, 60, 0 },
    { "L3Lvl", "L3Lvl", tLin, 0, 100, uNum, 0, 1, 0,  0 }, { "R1Lvl", "R1Lvl", tLin, 0, 100, uNum, 0, 1, 80, 0 },
    { "R2Lvl", "R2Lvl", tLin, 0, 100, uNum, 0, 1, 60, 0 }, { "R3Lvl", "R3Lvl", tLin, 0, 100, uNum, 0, 1, 0,  0 },

    { "Intns",    "Intns", tLin,    0,      100,    uNum,    0,   1,     50,     0 },
    { "Manual",   "Manu",  tLin,    0,      100,    uNum,    0,   1,     50,     0 },
    { "Resnc",    "Resnc", tLin,    -100,   100,    uSigned, 0,   1,     40,     0 },

    { "RotSpeed", "Speed", tChoice, 0,      1,      uChoice, 0,   1,     0,      kSpeedNames },
    { "HFast",    "HFast", tLin,    5,      10,     uHz,     2,   0.05,  6.8,    0 },
    { "RFast",    "RFast", tLin,    5,      10,     uHz,     2,   0.05,  5.9,    0 },
    { "HSlow",    "HSlow", tLog,    0.05,   5,      uHz,     2,   0,     0.8,    0 },
    { "RSlow",    "RSlow", tLog,    0.05,   5,      uHz,     2,   0,     0.66,   0 },
    { "H-Rise",   "H-Rls", tLin,    1,      100,    uNum,    0,   1,     30,     0 },
    { "R-Rise",   "R-Rls", tLin,    1,      100,    uNum,    0,   1,     60,     0 },
    { "H-Fall",   "H-Fal", tLin,    1,      100,    uNum,    0,   1,     35,     0 },
    { "R-Fall",   "R-Fal", tLin,    1,      100,    uNum,    0,   1,     70,     0 },
    { "R:H",      "R:H",   tLin,    -40,    40,     uBal,    0,   1,     0,      0 },
    { "MicMode",  "Mode",  tChoice, 0,      1,      uChoice, 0,   1,     1,      kMicNames },
    { "H-Dpt",    "H-Dpt", tLin,    0,      100,    uNum,    0,   1,     60,     0 },
    { "R-Dpt",    "R-Dpt", tLin,    0,      100,    uNum,    0,   1,     50,     0 },
    { "H-Trm",    "H-Trm", tLin,    0,      100,    uNum,    0,   1,     60,     0 },
    { "R-Trm",    "R-Trm", tLin,    0,      100,    uNum,    0,   1,     50,     0 },
    { "OD-SW",    "OD-SW", tChoice, 0,      1,      uChoice, 0,   1,     0,      kOnOff },
    { "OD Gain",  "Gain",  tLin,    0,      100,    uNum,    0,   1,     60,     0 },
    { "OD Drive", "Drive", tLin,    1,      100,    uNum,    0,   1,     30,     0 },
    { "OD Lvl",   "OD-Lv", tLin,    0,      100,    uNum,    0,   1,     70,     0 },
    { "Difsn",    "Difsn", tLin,    0,      100,    uNum,    0,   1,     50,     0 },

    { "ChFil",    "ChFil", tChoice, 0,      2,      uChoice, 0,   1,     0,      kFilNames },
    { "ChFrq",    "ChFrq", tLog,    100,    10000,  uHz,     0,   0,     5000,   0 },
    { "ChLvl",    "ChLvl", tLin,    0,      100,    uNum,    0,   1,     100,    0 },
    { "AmDcy",    "AmDcy", tLin,    1,      30,     uNum,    0,   1,     8,      0 },
    { "AmDmp",    "AmDmp", tLin,    -100,   0,      uSigned, 0,   1,     -30,    0 },
    { "AmDns",    "AmDns", tLin,    0,      100,    uNum,    0,   1,     70,     0 },
    { "AmFil",    "AmFil", tChoice, 0,      2,      uChoice, 0,   1,     0,      kFilNames },
    { "AmFrq",    "AmFrq", tLog,    100,    10000,  uHz,     0,   0,     4000,   0 },
    { "AmLvl",    "AmLvl", tLin,    0,      100,    uNum,    0,   1,     50,     0 },
};

// --- Conversions ---------------------------------------------------------------------

inline double clamp01 (double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

// 0..1 -> unit's value (choices: index as a double)
inline double toReal (int p, double v)
{
    const ParamInfo& i = kInfo[p];
    v = clamp01 (v);
    switch (i.taper)
    {
        case tLog:    return i.lo * std::pow (i.hi / i.lo, v);
        case tSq:     return i.lo + (i.hi - i.lo) * v * v;
        case tChoice: return std::floor (v * i.hi + 0.5);
        default:
        {
            const double r = i.lo + (i.hi - i.lo) * v;
            // Integer-stepped linear values (Algo, Program, Speed, Depth...) round to their step
            if (i.unit != uKnob && i.decimals == 0 && i.step >= 1.0) return std::floor (r + 0.5);
            return r;
        }
    }
}

inline double toNorm (int p, double r)
{
    const ParamInfo& i = kInfo[p];
    switch (i.taper)
    {
        case tLog:    if (r <= i.lo) return 0.0; return clamp01 (std::log (r / i.lo) / std::log (i.hi / i.lo));
        case tSq:     return clamp01 (std::sqrt (clamp01 ((r - i.lo) / (i.hi - i.lo))));
        case tChoice: return i.hi > 0 ? clamp01 (r / i.hi) : 0.0;
        default:      return clamp01 ((r - i.lo) / (i.hi - i.lo));
    }
}

inline int  toIndex (int p, double v) { return (int) toReal (p, v); }
inline bool isOn (float v) { return v >= 0.5f; }

inline float defaultNorm (int p) { return (float) toNorm (p, kInfo[p].def); }

// Algorithm number 0..15 from the stored value
inline int algoIndex (float v) { return (int) toReal (kAlgo, v) - 1; }
inline float algoValue (int a) { return (float) toNorm (kAlgo, a + 1); }
// Program number 1..300
inline int programNumber (float v) { return (int) toReal (kProgram, v); }
inline float programValue (int n) { return (float) toNorm (kProgram, n); }

// The unit's display of a value, e.g. "0.65", "100:0", "L180", "SHELV". Fits 8 chars.
inline void formatReal (int p, double r, char* out, int size)
{
    const ParamInfo& i = kInfo[p];
    char b[32];
    switch (i.unit)
    {
        case uChoice:
        {
            int k = (int) r;
            if (k < 0) k = 0;
            if (k > (int) i.hi) k = (int) i.hi;
            std::snprintf (b, sizeof (b), "%s", i.choices[k]);
            break;
        }
        case uHz:
            if (r >= 1000.0)      std::snprintf (b, sizeof (b), r >= 10000.0 ? "%.1fk" : "%.2fk", r / 1000.0);
            else if (i.decimals)  std::snprintf (b, sizeof (b), "%.*f", i.decimals, r);
            else                  std::snprintf (b, sizeof (b), "%.0f", r);
            break;
        case uMs:     std::snprintf (b, sizeof (b), "%.1f", r); break;
        case uDeg:    std::snprintf (b, sizeof (b), "%.0f", r); break;
        case udB:     std::snprintf (b, sizeof (b), "%+.1f", r); break;
        case uPan:
        {
            const int rr = (int) std::floor (r + 0.5);
            std::snprintf (b, sizeof (b), "%d:%d", 100 - rr, rr);
            break;
        }
        case uAzim:
        {
            const int a = (int) std::floor (r + 0.5);
            if (a == 0) std::snprintf (b, sizeof (b), "0");
            else        std::snprintf (b, sizeof (b), "%c%d", a < 0 ? 'L' : 'R', a < 0 ? -a : a);
            break;
        }
        case uElev: case uSigned:
        {
            const int a = (int) std::floor (r + 0.5);
            std::snprintf (b, sizeof (b), a > 0 ? "+%d" : "%d", a);
            break;
        }
        case uBal:
        {
            const int a = (int) std::floor (r + 0.5);       // -40 = 90:10 (rotor heavy) .. +40 = 10:90
            std::snprintf (b, sizeof (b), "%d:%d", 50 - a, 50 + a);
            break;
        }
        case uKnob:   std::snprintf (b, sizeof (b), "%.1f", r); break;
        default:      std::snprintf (b, sizeof (b), "%.*f", i.decimals, r); break;
    }
    std::strncpy (out, b, (size_t) size - 1);
    out[size - 1] = 0;
}

inline void formatParam (int p, float v, char* out, int size)
{
    if (p == kAlgo) { std::snprintf (out, (size_t) size, "%d", algoIndex (v) + 1); return; }
    formatReal (p, toReal (p, v), out, size);
}

// One alpha-dial step (dir = +1 / -1) from value v
inline float stepParam (int p, float v, int dir)
{
    const ParamInfo& i = kInfo[p];
    if (i.taper == tChoice)
    {
        int k = (int) toReal (p, v) + dir;
        if (k < 0) k = 0;
        if (k > (int) i.hi) k = (int) i.hi;
        return (float) toNorm (p, k);
    }
    if (i.step <= 0.0)
        return (float) clamp01 (v + dir / 200.0);
    double r = toReal (p, v);
    r = std::floor (r / i.step + 0.5) * i.step + dir * i.step;
    if (r < i.lo) r = i.lo;
    if (r > i.hi) r = i.hi;
    double n = toNorm (p, r);
    // A tSq step at the bottom of the range can be too small to move: nudge
    if (dir > 0 && n <= v) n = v + 1e-4;
    if (dir < 0 && n >= v) n = v - 1e-4;
    return (float) clamp01 (n);
}

// Parameters each algorithm shows on its LCD pages (and in the edit drawer), in the
// manual's order. The common E.Lvl / Dirct / Input follow, then the EQ.
// -1 ends a list.
enum { kMaxAlgoParams = 48 };

inline int algoParams (int algo, int* list)
{
    int n = 0;
    #define P(x) list[n++] = (x)
    switch (algo)
    {
        case kStereoChorus: P(kRate); P(kDepth); P(kPrDly); P(kLfo); P(kPhase); P(kXMix); P(kPanL); P(kPanR); P(kELvl); P(kDirect); P(kInMode); break;
        case kStereo3D:     P(kRate); P(kDepth); P(kPrDly); P(kLfo); P(kPhase); P(kAzmtL); P(kElevL); P(kAzmtR); P(kElevR); P(kELvl); P(kDirect); P(kInMode); break;
        case kPanner3D:     P(kPanSpeed); P(kPanDir); P(kPanStart); P(kPrDly); P(kELvl); P(kDirect); break;
        case kSpaceChorus:  P(kMode); P(kDirect); P(kELvl); P(kInMode); break;
        case kBand2:
            for (int b = 0; b < 6; ++b) P(kB1Rate + b);
            for (int b = 0; b < 6; ++b) P(kB4Rate + b);
            P(kXov2); P(kELvl); P(kDirect); P(kInMode); break;
        case kBand3:
            for (int b = 0; b < 6; ++b) P(kB1Rate + b);
            for (int b = 0; b < 6; ++b) P(kB2Rate + b);
            for (int b = 0; b < 6; ++b) P(kB4Rate + b);
            P(kXov1); P(kXov3); P(kELvl); P(kDirect); P(kInMode); break;
        case kBand4:
            for (int b = 0; b < 24; ++b) P(kB1Rate + b);
            P(kXov1); P(kXov2); P(kXov3); P(kELvl); P(kDirect); break;
        case kPhase8:
            P(kRate); P(kLfo);
            for (int k = 0; k < 8; ++k) { P(kP8Dpt1 + k); P(kP8PD1 + k); P(kP8Ph1 + k); P(kP8Pol1 + k); P(kP8Mix1 + k); }
            P(kELvl); P(kDirect); P(kInMode); break;
        case kPhase16:      P(kMode); P(kRate); P(kDepth); P(kPrDly); P(kLfo); P(kELvl); P(kDirect); break;
        case kDetune:
            for (int k = 0; k < 6; ++k) { P(kDtPit1 + k); P(kDtPan1 + k); P(kDtLvl1 + k); }
            P(kPrDly); P(kELvl); P(kDirect); P(kInMode); break;
        case kEnsemble:     P(kMode); P(kELvl); P(kInMode); break;
        case kWaveChorus:   P(kLfo); P(kRate); P(kDepth); P(kPrDly); P(kPhase); P(kXMix); P(kELvl); P(kDirect); P(kInMode); break;
        case kVintage:      P(kIntns); P(kMode); P(kELvl); break;
        case kFlanger:      P(kRate); P(kDepth); P(kManual); P(kResnc); P(kLfo); P(kPhase); P(kELvl); P(kDirect); P(kInMode); break;
        case kRotary:
            for (int k = kRotSpeed; k <= kDiffusion; ++k) P(k);
            P(kELvl); break;
        case kAmbience:
            P(kRate); P(kDepth); P(kPrDly); P(kLfo); P(kPhase);
            for (int k = kChFil; k <= kAmLvl; ++k) P(k);
            P(kELvl); P(kDirect); P(kInMode); break;
        default: break;
    }
    #undef P
    return n;
}

// The EQ is part of every algorithm
const int kEqParams[9] = { kLowFq, kLowGn, kLowQ, kMidFq, kMidGn, kMidQ, kHigFq, kHigGn, kHigQ };

// Which LFO waveforms an algorithm offers (Wave Chorus 6, Flanger 5, others TRI/SIN)
inline bool lfoAllowed (int algo, int wave)
{
    if (algo == kWaveChorus) return wave <= 5;
    if (algo == kFlanger)    return wave <= 2 || wave >= 6;
    return wave <= 1;
}

// How many modes an algorithm's Mode parameter has
inline int modeCount (int algo)
{
    switch (algo)
    {
        case kSpaceChorus: return 7;
        case kPhase16:     return 6;
        case kEnsemble:    return 6;
        case kVintage:     return 4;
        default:           return 1;
    }
}

// The LCD name of a parameter in a given algorithm (the band choruses name their bands
// and crossovers differently: 3 Band says Md-, 4 Band says LM- and HM-)
inline void lcdNameFor (int p, int algo, char* out, int size)
{
    const char* n = kInfo[p].lcd;
    char b[16];
    std::snprintf (b, sizeof (b), "%s", n);
    if (algo == kBand4 && p >= kB2Rate && p <= kB2Mix) { b[0] = 'L'; b[1] = 'M'; }
    if (algo == kBand4)
    {
        if (p == kXov1) std::snprintf (b, sizeof (b), "LoxLM");
        if (p == kXov2) std::snprintf (b, sizeof (b), "LMxHM");
        if (p == kXov3) std::snprintf (b, sizeof (b), "HMxHi");
    }
    if (algo == kSpaceChorus && p == kELvl) std::snprintf (b, sizeof (b), "T.Lvl");
    std::snprintf (out, (size_t) size, "%s", b);
}

// Mode index clamped to what an algorithm has
inline int modeFor (int algo, float v)
{
    const int k = toIndex (kMode, v), n = modeCount (algo);
    return k < n ? k : n - 1;
}

// A value as the LCD shows it for a given algorithm (Mode reads 1..6 except on the
// Space Chorus, whose modes include 1+4, 2+4, 3+4)
inline void formatParamFor (int p, float v, int algo, char* out, int size)
{
    if (p == kMode && algo != kSpaceChorus)
    {
        std::snprintf (out, (size_t) size, "%d", modeFor (algo, v) + 1);
        return;
    }
    formatParam (p, v, out, size);
}

} // namespace tbsdx

#endif
