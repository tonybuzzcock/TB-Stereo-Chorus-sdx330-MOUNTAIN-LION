// Factory.h
// The SDX-330's 100 preset programs (201-300). Their names and algorithms are the
// manual's Preset Name Table; their settings aren't published, so each one is voiced
// here from its name and algorithm, written in the LCD's own terms ("Rate=0.65").
// Anything a program doesn't set takes the parameter's default (Params.h).

#ifndef TBSDX_FACTORY_H
#define TBSDX_FACTORY_H

#include "../VST2/Params.h"

namespace tbsdx
{

struct FactoryProgram
{
    const char* name;       // as in the Preset Name Table (up to 17 characters)
    int algo;               // 1..16
    const char* settings;   // "LCDName=value ..." in the unit's terms
};

extern const FactoryProgram kFactory[kNumFactory];

// Fill every parameter for factory program 0..99 (program 201..300). The front-panel
// knobs, Bypass, Program and Direct Mix are left out (the caller keeps its own values).
// Returns the number of settings it couldn't read (0 when the table is right).
int factoryValues (int index, float values[kNumParams]);

// Parse one "Name=value" for an algorithm; -1 when not found
int  findParam (int algo, const char* lcd);
bool parseValue (int param, const char* text, double& real);

// Parameters a program holds (everything but the panel knobs, Bypass, Program, Direct Mix)
inline bool isProgramParam (int p) { return p >= kAlgo; }

} // namespace tbsdx

#endif
