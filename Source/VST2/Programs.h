// Programs.h
// The SDX-330's program memory: User Area 1-200 (written with MEMORY, each with a name
// of up to 17 characters, as on the unit's LCD) and Preset Area 201-300 (the factory
// programs, read only). A user program that has never been written holds a copy of a
// preset, as the real unit does after its factory initialization (1-100 and 101-200
// both copy 201-300).
//
// User programs live in one text file so every instance and session shares them:
//   Mac:   ~/Library/Application Support/TB/TB SDX-330/Programs.txt
//   other: ~/.config/TB/TB SDX-330/Programs.txt
// The TBSDX_PROGRAM_FILE environment variable overrides the location (tests).
// Plain C++98. File access only on the editor's thread, never in audio.

#ifndef TBSDX_PROGRAMS_H
#define TBSDX_PROGRAMS_H

#include "Params.h"

#include <string>

namespace tbsdx
{

enum { kMaxProgramName = 17, kFirstProgramParam = kAlgo, kNumProgramParams = kNumParams - kAlgo };

class ProgramStore
{
public:
    ProgramStore();

    void setPath (const std::string& p) { path = p; }
    const std::string& getPath() const { return path; }
    static std::string defaultPath();

    bool load();                    // re-reads the file; missing file = nothing written yet
    bool save() const;

    // Program numbers 1..300
    static bool isUser (int number) { return number >= 1 && number <= kNumUser; }
    bool written (int number) const;                       // a user program that has been written
    // Every program parameter (kAlgo..) for a program; other entries left alone
    void get (int number, float values[kNumParams]) const;
    std::string name (int number) const;
    // User programs only; put() keeps the name unless one is given
    bool put (int number, const float values[kNumParams]);
    bool setName (int number, const std::string& n);

    static std::string cleanName (const std::string& in);

private:
    std::string path;
    bool used[kNumUser];
    std::string names[kNumUser];
    float vals[kNumUser][kNumProgramParams];
};

} // namespace tbsdx

#endif
