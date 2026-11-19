// program_test.cpp
// The program memory: user programs 1-200 written and named, presets 201-300 read only,
// unwritten user programs copying the presets (as the unit's factory initialization does).

#include "../Source/VST2/Programs.h"
#include "../Source/DSP/Factory.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace tbsdx;

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("  %s  %s\n", ok ? "ok  " : "FAIL", what);
    if (! ok) ++failures;
}

int main()
{
    const std::string path = "/tmp/tbsdx_program_test/sub/Programs.txt";
    std::remove (path.c_str());
    std::printf ("Program store\n");
    ProgramStore a;
    a.setPath (path);
    check (! a.load(), "no file yet: nothing written");
    check (a.name (201) == "STEREO CHORUS" && a.name (300) == "LIGHT WEIGHT AMB", "preset names 201 and 300");
    check (a.name (1) == "STEREO CHORUS" && a.name (101) == "STEREO CHORUS" && a.name (26) == "SMOOTH 3D CHORUS",
           "unwritten user programs copy the presets (1-100 and 101-200)");
    float v[kNumParams], w[kNumParams];
    for (int i = 0; i < kNumParams; ++i) v[i] = w[i] = 0.0f;
    a.get (226, v);
    float f[kNumParams];
    factoryValues (25, f);
    bool same = true;
    for (int i = kAlgo; i < kNumParams; ++i) if (v[i] != f[i]) same = false;
    check (same, "a preset loads its factory values");
    check (! a.put (201, v), "presets can't be written");

    for (int i = kAlgo; i < kNumParams; ++i) v[i] = (float) ((i * 37) % 101) / 100.0f;
    check (a.put (5, v), "write user program 5");
    check (a.name (5) == "LO END WIGGLE", "a first write keeps the copied preset's name");
    check (a.setName (5, "  My\tWide\nChorus  "), "rename it");
    check (a.name (5) == "My Wide Chorus", "names are cleaned to one trimmed line");
    check (a.setName (7, "x") == false, "an unwritten program can't be renamed");
    a.setName (5, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    check (a.name (5).size() == 17 && a.name (5) == "ABCDEFGHIJKLMNOPQ", "names are cut to the LCD's 17 characters");
    a.setName (5, "Caf\xc3\xa9 chorus");
    check (a.name (5) == "Caf chorus", "only the LCD's ASCII characters are kept");
    a.setName (5, "WIDE ONE");
    check (a.save(), "save creates the folders and the file");

    ProgramStore b;
    b.setPath (path);
    check (b.load(), "a second instance reads the file");
    check (b.written (5) && ! b.written (6), "it sees which programs are written");
    b.get (5, w);
    bool round = true;
    for (int i = kAlgo; i < kNumParams; ++i) if (std::fabs (w[i] - v[i]) > 1e-5f) round = false;
    check (round, "values round-trip");
    check (b.name (5) == "WIDE ONE", "the name round-trips");
    for (int i = kAlgo; i < kNumParams; ++i) v[i] = 0.25f;
    b.put (5, v);
    check (b.name (5) == "WIDE ONE", "re-writing keeps the name");
    b.setName (5, "");
    check (b.name (5).empty(), "a name can be cleared");

    // An older, shorter line (fewer parameters) still loads; the rest take defaults
    FILE* fp = std::fopen (path.c_str(), "w");
    std::fprintf (fp, "TBSDX330-PROGRAMS 1\nprog 9: 0.5 0.25\nname 9: SHORT\n");
    std::fclose (fp);
    ProgramStore c;
    c.setPath (path);
    c.load();
    c.get (9, w);
    check (c.written (9) && c.name (9) == "SHORT" && w[kAlgo] == 0.5f && w[kELvl] == 0.25f
           && w[kRate] == defaultNorm (kRate), "short lines load, missing values take defaults");

    std::printf ("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL PROGRAM CHECKS PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
