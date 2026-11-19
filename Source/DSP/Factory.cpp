// Factory.cpp
// The 100 preset programs. Names and algorithms from the manual's Preset Name Table;
// settings voiced by ear from the names (the real values aren't published). 201 STEREO
// CHORUS shows Rate 0.65 on the manual's power-on screen and 226 SMOOTH 3D CHORUS shows
// Rate 0.14 in the front-panel photo, so those two rates are the unit's own.

#include "Factory.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>

namespace tbsdx
{

const FactoryProgram kFactory[kNumFactory] = {
    // 201-216: one of each algorithm
    { "STEREO CHORUS",     1,  "Rate=0.65 Depth=50 PrDly=8.0 LFO=TRI Phase=180 XMix=0 Pan-L=100:0 Pan-R=0:100 E.Lvl=90" },
    { "DIMENSIONAL CHO",   2,  "Rate=0.50 Depth=55 PrDly=10.0 LFO=SIN Phase=180 AzmtL=L120 ElevL=0 AzmtR=R120 ElevR=0 E.Lvl=85" },
    { "SLOW SPINNER",      3,  "Speed=2 Dir=CW Start=L90 PrDly=0.0 E.Lvl=100 Dirct=OFF" },
    { "SDD-320 Dimension", 4,  "Mode=2 Dirct=ON T.Lvl=90" },
    { "LO END WIGGLE",     5,  "Lo-Rt=1.20 Lo-Dp=70 Lo-PD=6.0 LoLFO=SIN Lo-Ph=180 LoMix=110 Hi-Rt=0.40 Hi-Dp=15 Hi-PD=4.0 HiLFO=TRI Hi-Ph=90 HiMix=60 Xover=500" },
    { "MIDRANGE CHORUS",   6,  "Lo-Dp=0 LoMix=0 Md-Rt=0.70 Md-Dp=65 Md-PD=9.0 Md-Ph=180 MdMix=110 Hi-Rt=0.45 Hi-Dp=25 HiMix=60 LMXov=300 MHXov=3.00k" },
    { "RICH & SOLID",      7,  "Lo-Rt=0.30 Lo-Dp=10 LoMix=60 LM-Rt=0.55 LM-Dp=45 LM-PD=8.0 LMMix=100 HM-Rt=0.75 HM-Dp=55 HM-PD=11.0 HMMix=100 Hi-Rt=0.95 Hi-Dp=60 Hi-PD=14.0 HiMix=90 LoxLM=200 LMxHM=1.00k HMxHi=3.50k" },
    { "CREAM & SHIMMER",   8,  "Rate=0.55 LFO=SIN Dpt-1=40 Dpt-2=50 Dpt-3=45 Dpt-4=55 Mix-1=90 Mix-2=80 Mix-3=80 Mix-4=70 Mix-5=0 Mix-6=0 Mix-7=0 Mix-8=0 HigGn=+2.0 HigFq=8.00k" },
    { "FAST MOTION",       9,  "Mode=2 Rate=3.20 Depth=35 PrDly=4.0 LFO=SIN E.Lvl=80" },
    { "DETUNE SHIMMER",   10,  "L1Pit=-9 R1Pit=+9 L2Pit=+5 R2Pit=-5 L1Lvl=80 R1Lvl=80 L2Lvl=55 R2Lvl=55 L3Lvl=0 R3Lvl=0 PrDly=12.0 HigGn=+3.0 HigFq=7.00k" },
    { "CHEESE CHORUS",    11,  "Mode=3 E.Lvl=100" },
    { "FAST TIDES",       12,  "LFO=OCW2 Rate=2.40 Depth=45 PrDly=6.0 Phase=180 XMix=20 E.Lvl=90" },
    { "VINTAGE CE-2",     13,  "Intns=45 Mode=2 E.Lvl=100" },
    { "ST.PAD FLANGER",   14,  "Rate=0.12 Depth=80 Manu=45 Resnc=55 LFO=TRI Phase=180 E.Lvl=90" },
    { "SLOW CHURCH ST.",  15,  "Speed=SLOW HSlow=0.70 RSlow=0.55 Mode=OfMic H-Dpt=55 R-Dpt=45 H-Trm=45 R-Trm=35 Difsn=80 R:H=50:50" },
    { "SIZZLE ON TOP",    16,  "Rate=0.80 Depth=45 PrDly=10.0 LFO=SIN Phase=180 ChFil=HPF ChFrq=2.00k ChLvl=100 AmDcy=10 AmDmp=-10 AmDns=70 AmFil=HPF AmFrq=3.00k AmLvl=55" },

    // 217-223: Stereo Chorus
    { "ALL AROUND CHORUS", 1,  "Rate=0.45 Depth=60 PrDly=12.0 LFO=SIN Phase=180 XMix=30 Pan-L=100:0 Pan-R=0:100" },
    { "SHIMMER 1",         1,  "Rate=1.10 Depth=35 PrDly=6.0 LFO=SIN Phase=90 HigGn=+3.0 HigFq=8.00k" },
    { "SHIMMER 2",         1,  "Rate=1.60 Depth=30 PrDly=4.0 LFO=TRI Phase=180 XMix=15 HigGn=+4.0 HigFq=10.0k" },
    { "FAT TRIANGLE WAVE", 1,  "Rate=0.35 Depth=85 PrDly=14.0 LFO=TRI Phase=180 XMix=40 LowGn=+2.0" },
    { "FAT SINE WAVE",     1,  "Rate=0.35 Depth=85 PrDly=14.0 LFO=SIN Phase=180 XMix=40 LowGn=+2.0" },
    { "OLD VIBRATO",       1,  "Rate=5.50 Depth=70 PrDly=0.0 LFO=SIN Phase=0 Pan-L=50:50 Pan-R=50:50 Dirct=OFF E.Lvl=100 HigGn=-3.0 HigFq=6.00k" },
    { "SUBTLE & WARM",     1,  "Rate=0.30 Depth=25 PrDly=7.0 LFO=SIN Phase=180 E.Lvl=65 HigGn=-2.0 HigFq=5.00k LowGn=+1.5" },

    // 224-230: Stereo 3D Chorus
    { "RICH 3D CHORUS",    2,  "Rate=0.40 Depth=70 PrDly=12.0 LFO=SIN Phase=180 AzmtL=L150 AzmtR=R150" },
    { "SLAP 3D CHORUS",    2,  "Rate=0.60 Depth=40 PrDly=80.0 LFO=TRI Phase=180 AzmtL=L100 AzmtR=R100 E.Lvl=75" },
    { "SMOOTH 3D CHORUS",  2,  "Rate=0.14 Depth=60 PrDly=10.0 LFO=SIN Phase=180 AzmtL=L120 AzmtR=R120" },
    { "LIFTED CHORUS",     2,  "Rate=0.55 Depth=45 PrDly=9.0 LFO=SIN Phase=180 AzmtL=L60 ElevL=+45 AzmtR=R60 ElevR=+45" },
    { "WARM 3D CHORUS 1",  2,  "Rate=0.30 Depth=50 PrDly=11.0 LFO=SIN Phase=180 AzmtL=L90 AzmtR=R90 HigGn=-3.0 HigFq=5.00k" },
    { "WARM 3D CHORUS 2",  2,  "Rate=0.25 Depth=65 PrDly=15.0 LFO=TRI Phase=180 AzmtL=L135 ElevL=-15 AzmtR=R135 ElevR=-15 HigGn=-4.0 HigFq=4.50k" },
    { "SLOW & WIDE",       2,  "Rate=0.08 Depth=80 PrDly=18.0 LFO=SIN Phase=180 AzmtL=L170 AzmtR=R170" },

    // 231-232: 3D Panner
    { "TWINING AROUND",    3,  "Speed=4 Dir=CCW Start=R90 PrDly=20.0 Dirct=ON E.Lvl=80" },
    { "SPLASHING VIBES",   3,  "Speed=8 Dir=CW Start=L180 PrDly=0.0 Dirct=OFF E.Lvl=100" },

    // 233-234: Space Chorus
    { "SPACE CHORUS",      4,  "Mode=3 Dirct=ON T.Lvl=90" },
    { "3&4 PUSHED SDD",    4,  "Mode=3+4 Dirct=ON T.Lvl=95" },

    // 235-242: 2 Band Chorus
    { "SLOW&FAST 2BAND",   5,  "Lo-Rt=0.20 Lo-Dp=50 Lo-PD=8.0 LoLFO=SIN LoMix=100 Hi-Rt=2.50 Hi-Dp=30 Hi-PD=5.0 HiLFO=SIN HiMix=100 Xover=800" },
    { "HIGH VELO CHORUS",  5,  "Lo-Dp=0 LoMix=0 Hi-Rt=4.00 Hi-Dp=40 Hi-PD=4.0 HiLFO=TRI Hi-Ph=180 HiMix=110 Xover=1.50k" },
    { "CROSSOVER BASS",    5,  "Lo-Dp=0 LoMix=0 Hi-Rt=0.60 Hi-Dp=60 Hi-PD=9.0 HiLFO=TRI Hi-Ph=180 HiMix=120 Xover=400" },
    { "LOW SMOOTH 2BAND",  5,  "Lo-Rt=0.25 Lo-Dp=40 Lo-PD=10.0 LoLFO=SIN LoMix=100 Hi-Rt=0.50 Hi-Dp=20 Hi-PD=6.0 HiLFO=SIN HiMix=60 Xover=600" },
    { "LOW DEEP 2BAND",    5,  "Lo-Rt=0.30 Lo-Dp=90 Lo-PD=12.0 LoLFO=TRI LoMix=120 Hi-Rt=0.50 Hi-Dp=25 Hi-PD=6.0 HiLFO=TRI HiMix=50 Xover=700" },
    { "EXPAND BASS 2BAND", 5,  "Lo-Dp=0 LoMix=0 Hi-Rt=0.45 Hi-Dp=70 Hi-PD=15.0 HiLFO=SIN Hi-Ph=180 HiMix=110 Xover=450 LowGn=+2.0 LowFq=80" },
    { "CLEAN STRAT",       5,  "Lo-Rt=0.40 Lo-Dp=15 Lo-PD=5.0 LoMix=40 Hi-Rt=0.80 Hi-Dp=55 Hi-PD=8.0 HiLFO=SIN Hi-Ph=180 HiMix=110 Xover=1.20k HigGn=+2.0 HigFq=5.00k" },
    { "RICH AND BRIGHT",   5,  "Lo-Rt=0.35 Lo-Dp=35 Lo-PD=10.0 LoMix=80 Hi-Rt=0.90 Hi-Dp=60 Hi-PD=12.0 HiLFO=SIN Hi-Ph=180 HiMix=120 Xover=2.00k HigGn=+4.0 HigFq=9.00k" },

    // 243-248: 3 Band Chorus
    { "MID DEEP 3BAND",    6,  "Lo-Dp=10 LoMix=40 Md-Rt=0.45 Md-Dp=90 Md-PD=12.0 Md-Ph=180 MdMix=120 Hi-Rt=0.70 Hi-Dp=30 HiMix=60 LMXov=250 MHXov=2.50k" },
    { "LOW DEEP 3BAND",    6,  "Lo-Rt=0.25 Lo-Dp=90 Lo-PD=14.0 LoLFO=TRI LoMix=120 Md-Rt=0.50 Md-Dp=35 MdMix=70 Hi-Rt=0.80 Hi-Dp=20 HiMix=50 LMXov=200 MHXov=2.00k" },
    { "WIDE Strings3BAND", 6,  "Lo-Rt=0.30 Lo-Dp=30 LoMix=70 Md-Rt=0.55 Md-Dp=70 Md-PD=15.0 Md-Ph=180 MdMix=110 Hi-Rt=5.50 Hi-Dp=20 Hi-PD=8.0 Hi-Ph=120 HiMix=90 LMXov=300 MHXov=3.00k" },
    { "FAST ON TOP",       6,  "Lo-Dp=0 LoMix=0 Md-Rt=0.50 Md-Dp=40 MdMix=80 Hi-Rt=6.00 Hi-Dp=35 Hi-PD=3.0 HiLFO=SIN Hi-Ph=180 HiMix=110 LMXov=200 MHXov=2.50k" },
    { "LO END BEEF",       6,  "Lo-Rt=0.20 Lo-Dp=60 Lo-PD=18.0 LoLFO=SIN LoMix=120 Md-Rt=0.40 Md-Dp=30 MdMix=60 Hi-Dp=10 HiMix=30 LMXov=180 MHXov=1.50k LowGn=+3.0 LowFq=90" },
    { "3BAND SHIMMER",     6,  "Lo-Dp=10 LoMix=40 Md-Rt=0.90 Md-Dp=40 MdMix=80 Hi-Rt=1.80 Hi-Dp=55 Hi-PD=6.0 HiLFO=SIN Hi-Ph=90 HiMix=120 LMXov=300 MHXov=4.00k HigGn=+3.0 HigFq=9.00k" },

    // 249-255: 4 Band Chorus
    { "SLAPPIN'CHORUS 1",  7,  "Lo-Dp=10 Lo-PD=40.0 LoMix=50 LM-Rt=0.50 LM-Dp=40 LM-PD=60.0 LMMix=90 HM-Rt=0.70 HM-Dp=50 HM-PD=75.0 HMMix=90 Hi-Rt=0.90 Hi-Dp=50 Hi-PD=90.0 HiMix=80" },
    { "SLAPPIN'CHORUS 2",  7,  "Lo-Dp=10 Lo-PD=60.0 LoMix=40 LM-Rt=0.40 LM-Dp=45 LM-PD=90.0 LMMix=90 HM-Rt=0.60 HM-Dp=50 HM-PD=110.0 HMMix=90 Hi-Rt=0.80 Hi-Dp=55 Hi-PD=130.0 HiMix=80" },
    { "SLAPPIN'CHORUS 3",  7,  "Lo-Dp=20 Lo-PD=100.0 LoMix=60 LM-Rt=0.35 LM-Dp=40 LM-PD=120.0 LMMix=80 HM-Rt=0.55 HM-Dp=45 HM-PD=140.0 HMMix=80 Hi-Rt=0.75 Hi-Dp=50 Hi-PD=160.0 HiMix=70" },
    { "SLAPPIN'CHORUS 4",  7,  "Lo-Dp=20 Lo-PD=150.0 LoMix=50 LM-Rt=0.30 LM-Dp=35 LM-PD=170.0 LMMix=80 HM-Rt=0.50 HM-Dp=40 HM-PD=185.0 HMMix=80 Hi-Rt=0.70 Hi-Dp=45 Hi-PD=200.0 HiMix=70 E.Lvl=80" },
    { "RICH & MOTION",     7,  "Lo-Rt=0.20 Lo-Dp=30 LoMix=70 LM-Rt=0.70 LM-Dp=60 LM-PD=9.0 LM-Ph=90 LMMix=100 HM-Rt=1.20 HM-Dp=55 HM-PD=12.0 HM-Ph=180 HMMix=100 Hi-Rt=2.00 Hi-Dp=45 Hi-PD=6.0 Hi-Ph=270 HiMix=100" },
    { "BASS CHORUS 1",     7,  "Lo-Dp=0 LoMix=0 LM-Rt=0.45 LM-Dp=55 LM-PD=10.0 LMMix=100 HM-Rt=0.60 HM-Dp=60 HM-PD=12.0 HMMix=110 Hi-Rt=0.80 Hi-Dp=50 HiMix=90 LoxLM=150 LMxHM=600 HMxHi=2.00k Dirct=ON" },
    { "BASS CHORUS 2",     7,  "Lo-Dp=0 LoMix=0 LM-Dp=10 LMMix=40 HM-Rt=0.70 HM-Dp=70 HM-PD=14.0 HMMix=120 Hi-Rt=1.00 Hi-Dp=60 HiMix=110 LoxLM=200 LMxHM=500 HMxHi=1.50k LowGn=+2.0 LowFq=60" },

    // 256-263: Stereo 8 Phase Chorus
    { "CREAM & SUGAR 1",   8,  "Rate=0.45 LFO=SIN Dpt-1=50 Dpt-2=55 Dpt-3=60 Dpt-4=50 Dpt-5=45 Dpt-6=55 Mix-1=80 Mix-2=80 Mix-3=70 Mix-4=70 Mix-5=60 Mix-6=60 Mix-7=0 Mix-8=0" },
    { "CREAM & SUGAR 2",   8,  "Rate=0.30 LFO=SIN Dpt-1=65 Dpt-2=70 Dpt-3=60 Dpt-4=75 Dpt-5=65 Dpt-6=70 Dpt-7=60 Dpt-8=75 Mix-1=70 Mix-2=70 Mix-3=70 Mix-4=70 Mix-5=60 Mix-6=60 Mix-7=60 Mix-8=60 HigGn=-2.0 HigFq=6.00k" },
    { "PHASE SCREW",       8,  "Rate=2.20 LFO=TRI Dpt-1=80 Dpt-2=80 Dpt-3=80 Dpt-4=80 Ph-1=0 Ph-2=10 Ph-3=20 Ph-4=30 Pol-1=NORML Pol-2=INV Pol-3=NORML Pol-4=INV PD-1=0.5 PD-2=0.8 PD-3=1.1 PD-4=1.4 Mix-1=100 Mix-2=100 Mix-3=100 Mix-4=100 Mix-5=0 Mix-6=0 Mix-7=0 Mix-8=0" },
    { "WATER SURFACE",     8,  "Rate=1.40 LFO=SIN Dpt-1=35 Dpt-2=40 Dpt-3=45 Dpt-4=50 Dpt-5=35 Dpt-6=40 Dpt-7=45 Dpt-8=50 Mix-1=60 Mix-2=60 Mix-3=60 Mix-4=60 Mix-5=60 Mix-6=60 Mix-7=60 Mix-8=60 HigGn=+3.0 HigFq=8.00k" },
    { "LIKE BUTTAH!",      8,  "Rate=0.22 LFO=SIN Dpt-1=55 Dpt-2=55 Dpt-3=55 Dpt-4=55 Mix-1=90 Mix-2=90 Mix-3=90 Mix-4=90 Mix-5=0 Mix-6=0 Mix-7=0 Mix-8=0 HigGn=-3.0 HigFq=5.00k LowGn=+1.0" },
    { "ROLL AROUND",       8,  "Rate=0.80 LFO=TRI Dpt-1=70 Dpt-2=70 Dpt-3=70 Dpt-4=70 Ph-1=0 Ph-2=90 Ph-3=180 Ph-4=270 Pol-1=INV Pol-2=INV Pol-3=INV Pol-4=INV PD-1=10.0 PD-2=20.0 PD-3=30.0 PD-4=40.0 Mix-1=90 Mix-2=90 Mix-3=90 Mix-4=90 Mix-5=0 Mix-6=0 Mix-7=0 Mix-8=0" },
    { "SLOW DEEP 8PHASE",  8,  "Rate=0.12 LFO=SIN Dpt-1=95 Dpt-2=95 Dpt-3=95 Dpt-4=95 Dpt-5=95 Dpt-6=95 Dpt-7=95 Dpt-8=95 Mix-1=70 Mix-2=70 Mix-3=70 Mix-4=70 Mix-5=70 Mix-6=70 Mix-7=70 Mix-8=70" },
    { "3PHASE CHORUS",     8,  "Rate=0.60 LFO=SIN Dpt-1=60 Dpt-2=60 Dpt-3=60 Ph-1=0 Ph-2=120 Ph-3=240 PD-1=6.0 PD-2=6.0 PD-3=6.0 Mix-1=100 Mix-2=100 Mix-3=100 Mix-4=0 Mix-5=0 Mix-6=0 Mix-7=0 Mix-8=0" },

    // 264-269: 16 Phase Chorus
    { "THICK & RICH",      9,  "Mode=3 Rate=0.40 Depth=60 PrDly=8.0 LFO=SIN" },
    { "SLOW & RICH",       9,  "Mode=3 Rate=0.15 Depth=75 PrDly=10.0 LFO=SIN" },
    { "THICK SPREAD",      9,  "Mode=6 Rate=0.35 Depth=55 PrDly=4.0 LFO=TRI" },
    { "SMOOTH SPREAD",     9,  "Mode=6 Rate=0.20 Depth=45 PrDly=6.0 LFO=SIN" },
    { "TREMOLOTION",       9,  "Mode=4 Rate=4.50 Depth=40 PrDly=0.0 LFO=SIN" },
    { "SHINING CHORUS",    9,  "Mode=5 Rate=0.90 Depth=35 PrDly=5.0 LFO=SIN HigGn=+3.0 HigFq=8.00k" },

    // 270-273: Detune Chorus
    { "HARMOCHORUS",      10,  "L1Pit=-12 R1Pit=+12 L2Pit=+7 R2Pit=-7 L3Pit=-20 R3Pit=+20 L1Lvl=80 R1Lvl=80 L2Lvl=60 R2Lvl=60 L3Lvl=40 R3Lvl=40 PrDly=15.0" },
    { "DETUNE SHIMM.",    10,  "L1Pit=-6 R1Pit=+6 L2Pit=+11 R2Pit=-11 L1Lvl=75 R1Lvl=75 L2Lvl=50 R2Lvl=50 L3Lvl=0 R3Lvl=0 PrDly=8.0 HigGn=+3.0 HigFq=8.00k" },
    { "LIGHT DETUNE",     10,  "L1Pit=-5 R1Pit=+5 L1Lvl=60 R1Lvl=60 L2Lvl=0 R2Lvl=0 L3Lvl=0 R3Lvl=0 PrDly=10.0" },
    { "FAT PIANO DETUNE", 10,  "L1Pit=-8 R1Pit=+8 L2Pit=+4 R2Pit=-4 L3Pit=-15 R3Pit=+15 L1Lvl=70 R1Lvl=70 L2Lvl=55 R2Lvl=55 L3Lvl=35 R3Lvl=35 PrDly=20.0 LowGn=+1.5" },

    // 274-278: Ensemble
    { "STRINGS ENSEMBLE", 11,  "Mode=1" },
    { "E.PIANO ENSEMBLE", 11,  "Mode=2" },
    { "ORGAN ENSEMBLE",   11,  "Mode=5" },
    { "RHYTHM CUTTING",   11,  "Mode=3 HigGn=+2.0 HigFq=5.00k" },
    { "LUCY'S VIBE.",     11,  "Mode=6" },

    // 279-281: Wave Chorus
    { "WAVEY CHORUS",     12,  "LFO=OCW1 Rate=0.70 Depth=60 PrDly=8.0 Phase=180 XMix=20" },
    { "RIPPLES",          12,  "LFO=OCW3 Rate=1.30 Depth=45 PrDly=5.0 Phase=90 XMix=10" },
    { "SLOW TIDES",       12,  "LFO=EXP Rate=0.15 Depth=80 PrDly=12.0 Phase=180 XMix=30" },

    // 282-283: Vintage Chorus
    { "VINTAGE CE-1",     13,  "Intns=55 Mode=4 E.Lvl=100" },
    { "VINTAGE CE-3",     13,  "Intns=40 Mode=3 E.Lvl=100" },

    // 284-289: Stereo Flanger
    { "ST.HI-FLANGER 1",  14,  "Rate=0.25 Depth=70 Manu=60 Resnc=70 LFO=TRI Phase=180" },
    { "ST.HI-FLANGER 2",  14,  "Rate=0.45 Depth=60 Manu=70 Resnc=80 LFO=SIN Phase=90" },
    { "ST.DEEP FLANGER",  14,  "Rate=0.10 Depth=100 Manu=50 Resnc=60 LFO=TRI Phase=180" },
    { "VELOCITY RESONATE",14,  "Rate=0.05 Depth=20 Manu=35 Resnc=-85 LFO=SIN Phase=0" },
    { "MODWHEEL FLANGER", 14,  "Rate=0.01 Depth=0 Manu=40 Resnc=75 LFO=TRI Phase=0" },
    { "GONNA GO MY WAY",  14,  "Rate=0.20 Depth=90 Manu=55 Resnc=-60 LFO=OFW1 Phase=180" },

    // 290-294: Rotary
    { "MODEL 147",        15,  "Speed=SLOW HFast=6.80 RFast=5.90 HSlow=0.80 RSlow=0.66 H-Rls=30 R-Rls=60 H-Fal=35 R-Fal=70 R:H=50:50 Mode=OnMic H-Dpt=60 R-Dpt=50 H-Trm=60 R-Trm=50 OD-SW=OFF Difsn=50" },
    { "FAST CHURCH MONO", 15,  "Speed=FAST HFast=7.20 RFast=6.30 Mode=OfMic H-Dpt=55 R-Dpt=45 H-Trm=45 R-Trm=40 Difsn=0 R:H=55:45" },
    { "SLOW OVERDRIVE",   15,  "Speed=SLOW HSlow=0.75 RSlow=0.60 Mode=OnMic H-Dpt=60 R-Dpt=50 H-Trm=65 R-Trm=50 OD-SW=ON Gain=70 Drive=45 OD-Lv=65 Difsn=60" },
    { "FAST CHURCHEESE",  15,  "Speed=FAST HFast=7.60 RFast=6.80 Mode=OfMic H-Dpt=70 R-Dpt=55 H-Trm=55 R-Trm=45 Difsn=90 R:H=45:55 HigGn=+2.0 HigFq=4.00k" },
    { "FAT DRIVE",        15,  "Speed=FAST HFast=6.80 RFast=5.90 Mode=OnMic H-Dpt=65 R-Dpt=55 H-Trm=70 R-Trm=55 OD-SW=ON Gain=90 Drive=80 OD-Lv=55 Difsn=55 R:H=60:40" },

    // 295-300: Ambience Chorus
    { "DRY AMB CHORUS",   16,  "Rate=0.55 Depth=45 PrDly=8.0 LFO=SIN Phase=180 ChLvl=100 AmDcy=4 AmDmp=-40 AmDns=60 AmLvl=30" },
    { "WET AMB CHORUS",   16,  "Rate=0.45 Depth=55 PrDly=12.0 LFO=SIN Phase=180 ChLvl=90 AmDcy=15 AmDmp=-25 AmDns=80 AmLvl=70" },
    { "LIGHT AMB CHORUS", 16,  "Rate=0.70 Depth=30 PrDly=6.0 LFO=SIN Phase=180 ChFil=HPF ChFrq=400 ChLvl=80 AmDcy=6 AmDmp=-20 AmDns=50 AmFil=HPF AmFrq=600 AmLvl=40" },
    { "DEEP AMB CHORUS",  16,  "Rate=0.25 Depth=80 PrDly=15.0 LFO=TRI Phase=180 ChLvl=100 AmDcy=22 AmDmp=-45 AmDns=90 AmFil=LPF AmFrq=5.00k AmLvl=65" },
    { "SYMPHONIC BOOTH",  16,  "Rate=0.35 Depth=60 PrDly=20.0 LFO=SIN Phase=180 ChFil=LPF ChFrq=7.00k ChLvl=90 AmDcy=12 AmDmp=-55 AmDns=95 AmLvl=60" },
    { "LIGHT WEIGHT AMB", 16,  "Rate=0.90 Depth=25 PrDly=4.0 LFO=SIN Phase=120 ChFil=HPF ChFrq=800 ChLvl=70 AmDcy=3 AmDmp=-10 AmDns=40 AmFil=HPF AmFrq=1.20k AmLvl=35" }
};

//==============================================================================
int findParam (int algo, const char* lcd)
{
    int list[kMaxAlgoParams + 16];
    int n = algoParams (algo, list);
    for (int k = 0; k < 9; ++k) list[n++] = kEqParams[k];
    char name[16];
    for (int k = 0; k < n; ++k)
    {
        lcdNameFor (list[k], algo, name, sizeof (name));
        if (! std::strcmp (name, lcd)) return list[k];
    }
    return -1;
}

bool parseValue (int p, const char* t, double& r)
{
    const ParamInfo& info = kInfo[p];
    if (info.unit == uChoice)
    {
        for (int k = 0; k <= (int) info.hi; ++k)
            if (! std::strcmp (info.choices[k], t)) { r = k; return true; }
        return false;
    }
    if (info.unit == uPan || info.unit == uBal)
    {
        int a = 0, b = 0;
        if (std::sscanf (t, "%d:%d", &a, &b) != 2) return false;
        r = info.unit == uPan ? (double) b : (b - 50.0);    // pan: right share; R:H: horn - 50
        return true;
    }
    if (info.unit == uAzim)
    {
        if (t[0] == 'L') { r = -std::atof (t + 1); return true; }
        if (t[0] == 'R') { r = std::atof (t + 1); return true; }
        r = std::atof (t);
        return true;
    }
    char* end = 0;
    double v = std::strtod (t, &end);
    if (end == t) return false;
    if (*end == 'k') v *= 1000.0;
    r = v;
    return true;
}

int factoryValues (int index, float values[kNumParams])
{
    if (index < 0 || index >= kNumFactory) return 1;
    const FactoryProgram& f = kFactory[index];
    for (int p = kAlgo; p < kNumParams; ++p) values[p] = defaultNorm (p);
    const int algo = f.algo - 1;
    values[kAlgo] = algoValue (algo);
    int bad = 0;
    const char* s = f.settings;
    char tok[64];
    while (*s)
    {
        while (*s == ' ') ++s;
        if (! *s) break;
        int len = 0;
        while (s[len] && s[len] != ' ' && len < 63) { tok[len] = s[len]; ++len; }
        tok[len] = 0;
        s += len;
        char* eq = std::strchr (tok, '=');
        if (! eq) { ++bad; continue; }
        *eq = 0;
        const int p = findParam (algo, tok);
        double r;
        if (p == kMode && algo != kSpaceChorus)
        {
            const int m = std::atoi (eq + 1);
            if (m < 1 || m > modeCount (algo)) { ++bad; continue; }
            r = m - 1;
        }
        else if (p < 0 || ! parseValue (p, eq + 1, r)) { ++bad; continue; }
        values[p] = (float) toNorm (p, r);
    }
    return bad;
}

} // namespace tbsdx
