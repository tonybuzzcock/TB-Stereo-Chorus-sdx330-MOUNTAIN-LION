// Blocks.h
// Building blocks for the SDX-330 algorithms: a modulated delay line with cubic
// interpolation, the LFO waveforms from the manual, RBJ biquads and a 4th-order
// Linkwitz-Riley crossover, smoothers, and the "3D" positioner used by the
// Stereo 3D Chorus and the 3D Panner.
//
// Plain C++98. Buffers are allocated in prepare(), never while processing.

#ifndef TBSDX_BLOCKS_H
#define TBSDX_BLOCKS_H

#include <cmath>
#include <vector>

namespace tbsdx
{

const double kPi = 3.14159265358979323846;
const double kTwoPi = 2.0 * kPi;

inline double clampd (double x, double lo, double hi) { return x < lo ? lo : (x > hi ? hi : x); }

//==============================================================================
// Delay line: power-of-two ring, read at a fractional delay in samples.
class DelayLine
{
public:
    DelayLine() : mask (0), pos (0) {}

    void allocate (int maxSamples)
    {
        int n = 16;
        while (n < maxSamples + 8) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1;
        pos = 0;
    }
    void clear() { for (size_t i = 0; i < buf.size(); ++i) buf[i] = 0.0f; }
    int  capacity() const { return mask - 4; }

    inline void write (double x) { buf[(size_t) pos] = (float) x; pos = (pos + 1) & mask; }

    // Delay of d samples, read after this sample's write(): d = 0 would be the sample
    // just written; the cubic needs one newer sample, so reads start at d = 1.
    inline double read (double d) const
    {
        if (d < 1.0) d = 1.0;               // y0 (one sample newer) must already exist
        if (d > mask - 4) d = mask - 4;
        const int di = (int) d;
        const double f = d - di;
        const int i0 = (pos - 1 - di) & mask;          // the sample at delay di
        const double y0 = buf[(size_t) ((i0 + 1) & mask)];   // delay di - 1
        const double y1 = buf[(size_t) i0];                  // delay di
        const double y2 = buf[(size_t) ((i0 - 1) & mask)];   // delay di + 1
        const double y3 = buf[(size_t) ((i0 - 2) & mask)];   // delay di + 2
        // Cubic Hermite between y1 (f = 0) and y2 (f = 1)
        const double c1 = 0.5 * (y2 - y0);
        const double c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
        const double c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }
    inline double tap (int d) const { return buf[(size_t) ((pos - 1 - d) & mask)]; }

private:
    std::vector<float> buf;
    int mask, pos;
};

//==============================================================================
// LFO waveforms (manual p.61), phase 0..1 -> 0..1 (the delay's sweep position).
// TRI and SIN are exact. EXP, OCW1-3 (Wave Chorus) and OFW1-2 (Flanger) are drawn
// from the little pictures in the manual: estimates of their shape.
enum Wave { kTri = 0, kSin, kExp, kOcw1, kOcw2, kOcw3, kOfw1, kOfw2 };

inline double triWave (double p) { p -= std::floor (p); return p < 0.5 ? 2.0 * p : 2.0 - 2.0 * p; }
inline double sinWave (double p) { return 0.5 - 0.5 * std::cos (kTwoPi * p); }

inline double lfoShape (int wave, double p)
{
    p -= std::floor (p);
    switch (wave)
    {
        default:
        case kTri:  return triWave (p);
        case kSin:  return sinWave (p);
        case kExp:
        {
            // Sharp peaks, long rounded troughs ("UU")
            const double t = triWave (p);
            return (std::exp (3.0 * t) - 1.0) / (std::exp (3.0) - 1.0);
        }
        case kOcw1:
        {
            // A sine with a second-harmonic kink on one side
            const double v = 0.5 - 0.5 * std::cos (kTwoPi * p) + 0.18 * std::sin (2.0 * kTwoPi * p);
            return clampd ((v + 0.12) / 1.24, 0.0, 1.0);
        }
        case kOcw2:
        {
            // Two humps of different heights per cycle
            const double a = sinWave (2.0 * p);
            return p < 0.5 ? a : 0.55 * a;
        }
        case kOcw3:
        {
            // Rounded steps: a sine with a slow ripple on top
            const double v = sinWave (p) * 0.8 + 0.2 * sinWave (3.0 * p);
            return clampd (v, 0.0, 1.0);
        }
        case kOfw1:
        {
            // Slow rise, quick rounded fall (flanger "up" sweep)
            if (p < 0.8) return sinWave (p / 1.6);
            return sinWave (0.5 + (p - 0.8) / 0.4);
        }
        case kOfw2:
        {
            // Quick rise, slow fall (flanger "down" sweep)
            if (p < 0.2) return sinWave (p / 0.4);
            return sinWave (0.5 + (p - 0.2) / 1.6);
        }
    }
}

struct Lfo
{
    double phase, inc;
    Lfo() : phase (0.0), inc (0.0) {}
    void setRate (double hz, double fs) { inc = hz / fs; }
    inline void tick() { phase += inc; if (phase >= 1.0) phase -= 1.0; }
};

//==============================================================================
// One-pole smoother
struct Smooth
{
    double y, k;
    Smooth() : y (0.0), k (1.0) {}
    void setTime (double seconds, double fs) { k = 1.0 - std::exp (-1.0 / (seconds * fs)); }
    inline double tick (double target) { y += (target - y) * k; return y; }
    void snap (double v) { y = v; }
};

//==============================================================================
// Biquad, transposed direct form II, RBJ cookbook designs
struct Biquad
{
    double b0, b1, b2, a1, a2, z1, z2;
    Biquad() : b0 (1), b1 (0), b2 (0), a1 (0), a2 (0), z1 (0), z2 (0) {}
    void reset() { z1 = z2 = 0.0; }
    void setIdentity() { b0 = 1; b1 = b2 = a1 = a2 = 0; }

    inline double tick (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    static double safeF (double f, double fs) { return clampd (f, 5.0, 0.45 * fs); }

    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2)
    {
        b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0; a1 = na1 / na0; a2 = na2 / na0;
    }
    void lowpass (double f, double q, double fs)
    {
        const double w = kTwoPi * safeF (f, fs) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void highpass (double f, double q, double fs)
    {
        const double w = kTwoPi * safeF (f, fs) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void peak (double f, double q, double dB, double fs)
    {
        const double A = std::pow (10.0, dB / 40.0);
        const double w = kTwoPi * safeF (f, fs) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowShelf (double f, double dB, double fs)
    {
        const double A = std::pow (10.0, dB / 40.0);
        const double w = kTwoPi * safeF (f, fs) / fs, c = std::cos (w), s = std::sin (w);
        const double al = s / 2.0 * std::sqrt (2.0);        // shelf slope 1
        const double sa = 2.0 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
             (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
    }
    void highShelf (double f, double dB, double fs)
    {
        const double A = std::pow (10.0, dB / 40.0);
        const double w = kTwoPi * safeF (f, fs) / fs, c = std::cos (w), s = std::sin (w);
        const double al = s / 2.0 * std::sqrt (2.0);
        const double sa = 2.0 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
             (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
    }
};

// One-pole low-pass / high-pass
struct OnePole
{
    double a, z;
    OnePole() : a (1.0), z (0.0) {}
    void reset() { z = 0.0; }
    void setLowpass (double f, double fs) { a = 1.0 - std::exp (-kTwoPi * clampd (f, 1.0, 0.49 * fs) / fs); }
    inline double lp (double x) { z += a * (x - z); return z; }
    inline double hp (double x) { z += a * (x - z); return x - z; }
};

//==============================================================================
// 4th-order Linkwitz-Riley crossover: low = LP2 x LP2, high = HP2 x HP2 (Q 0.707)
struct Crossover
{
    Biquad l1, l2, h1, h2;
    void set (double f, double fs)
    {
        l1.lowpass (f, 0.70710678, fs); l2 = l1;
        h1.highpass (f, 0.70710678, fs); h2 = h1;
        // keep state continuity: copying coefficients copied state too; that is fine
    }
    void setKeep (double f, double fs)
    {
        double s[8] = { l1.z1, l1.z2, l2.z1, l2.z2, h1.z1, h1.z2, h2.z1, h2.z2 };
        set (f, fs);
        l1.z1 = s[0]; l1.z2 = s[1]; l2.z1 = s[2]; l2.z2 = s[3];
        h1.z1 = s[4]; h1.z2 = s[5]; h2.z1 = s[6]; h2.z2 = s[7];
    }
    void reset() { l1.reset(); l2.reset(); h1.reset(); h2.reset(); }
    inline void split (double x, double& lo, double& hi)
    {
        lo = l2.tick (l1.tick (x));
        hi = h2.tick (h1.tick (x));
    }
};

//==============================================================================
// 3D positioner (an estimate of Roland's RSS processing, which isn't published):
// a mono source placed at an azimuth (-180 = L180 .. 0 front .. +180 = R180) and
// elevation (-60 .. +60 degrees) for two speakers.
//  - interaural time difference (up to about 0.65 ms) and head shadow on the far ear
//  - rear positions darker; elevation by a pinna-like peak/notch moving with height
//  - a crosstalk canceller (each ear's signal fed inverted and delayed to the other
//    speaker) so sounds to the side and behind go outside the speakers.
class Positioner
{
public:
    Positioner() : fs (44100.0), itdL (0), itdR (0), xtalk (0.0), gL (1.0), gR (1.0) {}

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        src.allocate ((int) (0.002 * fs) + 8);
        xl.allocate ((int) (0.001 * fs) + 8);
        xr.allocate ((int) (0.001 * fs) + 8);
        reset();
        setPosition (0.0, 0.0);
    }
    void reset()
    {
        src.clear(); xl.clear(); xr.clear();
        shL.reset(); shR.reset(); rearL.reset(); rearR.reset(); elL.reset(); elR.reset();
        xlLp.reset(); xrLp.reset();
    }

    void setPosition (double azimuthDeg, double elevationDeg)
    {
        const double az = azimuthDeg * kPi / 180.0;
        const double s = std::sin (az);              // + = right
        const double c = std::cos (az);              // + = front
        const double itd = 0.00065 * s;              // seconds, right ear leads when s > 0
        itdL = itd > 0 ? itd * fs : 0.0;
        itdR = itd < 0 ? -itd * fs : 0.0;
        // Head shadow: the far ear loses highs (down to about 1.5 kHz fully to one side)
        const double far = std::fabs (s);
        const double shadowF = 20000.0 * std::pow (1500.0 / 20000.0, far);
        if (s >= 0) { shL.highShelf (shadowF, -9.0 * far, fs); shR.highShelf (3000.0, 2.0 * far, fs); }
        else        { shR.highShelf (shadowF, -9.0 * far, fs); shL.highShelf (3000.0, 2.0 * far, fs); }
        gL = std::sqrt (0.5 * (1.0 - 0.8 * s));
        gR = std::sqrt (0.5 * (1.0 + 0.8 * s));
        // Behind: duller
        const double rear = c < 0 ? -c : 0.0;
        rearL.highShelf (4000.0, -7.0 * rear, fs); rearR = rearL;
        // Elevation: above brightens around 7-8 kHz, below dulls; a notch that rises with height
        const double e = clampd (elevationDeg / 60.0, -1.0, 1.0);
        elL.peak (6500.0 + 3000.0 * e, 1.2, e > 0 ? 7.0 * e : 5.0 * e, fs);
        elR = elL;
        // Crosstalk cancellation grows towards the sides and back
        xtalk = 0.55 * std::sqrt (far * 0.75 + rear * 0.25);
        xlLp.setLowpass (5000.0, fs); xrLp.setLowpass (5000.0, fs);
    }

    inline void process (double x, double& outL, double& outR)
    {
        src.write (x);
        double l = src.read (itdL), r = src.read (itdR);
        l = elL.tick (rearL.tick (shL.tick (l))) * gL;
        r = elR.tick (rearR.tick (shR.tick (r))) * gR;
        // Crosstalk path: about 0.25 ms (a speaker's extra distance to the far ear)
        xl.write (l); xr.write (r);
        const double dx = 0.00025 * fs;
        outL = l - xtalk * xrLp.lp (xr.read (dx));
        outR = r - xtalk * xlLp.lp (xl.read (dx));
    }

private:
    double fs, itdL, itdR, xtalk, gL, gR;
    DelayLine src, xl, xr;
    Biquad shL, shR, rearL, rearR, elL, elR;
    OnePole xlLp, xrLp;
};

} // namespace tbsdx

#endif
