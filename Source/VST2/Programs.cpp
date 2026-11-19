// Programs.cpp

#include "Programs.h"
#include "../DSP/Factory.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>

namespace tbsdx
{

namespace
{
    const char* const kHeader = "TBSDX330-PROGRAMS 1";

    void makeParentDirs (const std::string& file)
    {
        const std::string::size_type last = file.rfind ('/');
        if (last == std::string::npos || last == 0) return;
        const std::string dir = file.substr (0, last);
        for (std::string::size_type i = 1; i <= dir.size(); ++i)
            if (i == dir.size() || dir[i] == '/')
                mkdir (dir.substr (0, i).c_str(), 0755);
    }
}

// The LCD's characters: printable ASCII, one line, trimmed, at most 17
std::string ProgramStore::cleanName (const std::string& in)
{
    std::string out;
    for (std::string::size_type i = 0; i < in.size(); ++i)
    {
        const unsigned char c = (unsigned char) in[i];
        if (c == '\n' || c == '\r' || c == '\t') out += ' ';
        else if (c >= 32 && c < 127) out += (char) c;
    }
    std::string::size_type a = out.find_first_not_of (' ');
    if (a == std::string::npos) return std::string();
    std::string::size_type b = out.find_last_not_of (' ');
    out = out.substr (a, b - a + 1);
    if (out.size() > (std::string::size_type) kMaxProgramName) out = out.substr (0, kMaxProgramName);
    return out;
}

ProgramStore::ProgramStore()
    : path (defaultPath())
{
    for (int s = 0; s < kNumUser; ++s)
    {
        used[s] = false;
        for (int i = 0; i < kNumProgramParams; ++i) vals[s][i] = defaultNorm (kFirstProgramParam + i);
    }
}

std::string ProgramStore::defaultPath()
{
    const char* over = std::getenv ("TBSDX_PROGRAM_FILE");
    if (over && *over) return over;
    const char* home = std::getenv ("HOME");
    std::string base = (home && *home) ? home : "/tmp";
#if defined(__APPLE__)
    return base + "/Library/Application Support/TB/TB SDX-330/Programs.txt";
#else
    return base + "/.config/TB/TB SDX-330/Programs.txt";
#endif
}

bool ProgramStore::load()
{
    for (int s = 0; s < kNumUser; ++s) { used[s] = false; names[s].clear(); }
    FILE* f = std::fopen (path.c_str(), "r");
    if (! f) return false;
    static char line[8192];
    while (std::fgets (line, sizeof (line), f))
    {
        int num = -1, n0 = 0;
        if (std::sscanf (line, "name %d:%n", &num, &n0) == 1 && isUser (num) && n0 > 0)
        {
            const char* t = line + n0;
            if (*t == ' ') ++t;
            names[num - 1] = cleanName (t);
            continue;
        }
        num = -1; n0 = 0;
        if (std::sscanf (line, "prog %d:%n", &num, &n0) != 1 || ! isUser (num) || n0 <= 0) continue;
        float v[kNumProgramParams];
        for (int i = 0; i < kNumProgramParams; ++i) v[i] = defaultNorm (kFirstProgramParam + i);
        const char* s = line + n0;
        for (int i = 0; i < kNumProgramParams; ++i)
        {
            char* end = 0;
            const double d = std::strtod (s, &end);
            if (end == s) break;                    // older, shorter lines: the rest stay default
            v[i] = (float) clamp01 (d);
            s = end;
        }
        for (int i = 0; i < kNumProgramParams; ++i) vals[num - 1][i] = v[i];
        used[num - 1] = true;
    }
    std::fclose (f);
    return true;
}

bool ProgramStore::save() const
{
    makeParentDirs (path);
    const std::string tmp = path + ".tmp";
    FILE* f = std::fopen (tmp.c_str(), "w");
    if (! f) return false;
    std::fprintf (f, "%s\n", kHeader);
    std::fprintf (f, "# TB SDX-330 user programs 1-200. One line per written program: values 0..1 in parameter order from Algo; then its name.\n");
    for (int s = 0; s < kNumUser; ++s)
    {
        if (! used[s]) continue;
        std::fprintf (f, "prog %d:", s + 1);
        for (int i = 0; i < kNumProgramParams; ++i) std::fprintf (f, " %.6f", vals[s][i]);
        std::fprintf (f, "\n");
        if (! names[s].empty()) std::fprintf (f, "name %d: %s\n", s + 1, names[s].c_str());
    }
    std::fclose (f);
    return std::rename (tmp.c_str(), path.c_str()) == 0;
}

bool ProgramStore::written (int number) const
{
    return isUser (number) && used[number - 1];
}

void ProgramStore::get (int number, float values[kNumParams]) const
{
    if (written (number))
    {
        for (int i = 0; i < kNumProgramParams; ++i) values[kFirstProgramParam + i] = vals[number - 1][i];
        return;
    }
    // Presets, and user programs never written (a copy of a preset)
    int idx = (number >= 201 && number <= 300) ? number - 201 : (number - 1) % kNumFactory;
    if (idx < 0) idx = 0;
    float v[kNumParams];
    factoryValues (idx, v);
    for (int i = kFirstProgramParam; i < kNumParams; ++i) values[i] = v[i];
}

std::string ProgramStore::name (int number) const
{
    if (written (number)) return names[number - 1];
    int idx = (number >= 201 && number <= 300) ? number - 201 : (number - 1) % kNumFactory;
    if (idx < 0) idx = 0;
    return kFactory[idx].name;
}

bool ProgramStore::put (int number, const float values[kNumParams])
{
    if (! isUser (number)) return false;
    if (! used[number - 1]) names[number - 1] = name (number);      // keeps the copied preset's name
    for (int i = 0; i < kNumProgramParams; ++i) vals[number - 1][i] = (float) clamp01 (values[kFirstProgramParam + i]);
    used[number - 1] = true;
    return true;
}

bool ProgramStore::setName (int number, const std::string& n)
{
    if (! written (number)) return false;
    names[number - 1] = cleanName (n);
    return true;
}

} // namespace tbsdx
