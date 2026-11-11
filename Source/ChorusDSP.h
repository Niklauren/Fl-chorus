#pragma once

#include <cmath>
#include <vector>

// FL Studio Fruity Chorus, as measured from FL Studio 32-bit float renders:
//
//   out = 0.7 * dry  +  0.42 * (voice1 + voice2 + voice3)        (Wet Only: dry = 0)
//
//   voice_v = band(x) read from a delay line with LINEAR interpolation at
//             D = Delay + Depth * (0.5 + 0.5 * wave(phase_v))   [ms]
//   band    = 1st-order bilinear high-pass ("Process HF") or low-pass ("Process LF")
//             at Cross Cutoff; the dry signal is never filtered
//   phase_v = 2*pi * f_v * t, all LFOs start at 0 when playback starts;
//             the right channel runs Stereo degrees ahead
//   waves   : sin        = parabolic sine approximation
//             sin^3      = (parabolic sine)^3
//             multi-sin  = (sin x + sin 2x / 3 + sin 3x / 3) / 1.27614  (true sines, peak 1 at 45 deg)
//             multi-sin^3
//
// Static parts (dry/wet levels, filter, interpolation) null against FL at -109...-186 dB.
namespace flchorus
{
enum class Wave { sine = 0, sine3 = 1, multi = 2, multi3 = 3 };

inline constexpr int numVoices = 3;
inline constexpr double pi = 3.14159265358979323846;
inline constexpr double twoPi = 2.0 * pi;
inline constexpr float dryGain = 0.7f;
inline constexpr float voiceGain = 0.42f;

inline double parabolicSine (double x) noexcept // any x, radians
{
    x = x - twoPi * std::floor ((x + pi) / twoPi); // wrap to [-pi, pi)
    return (4.0 / pi) * x - (4.0 / (pi * pi)) * x * std::abs (x);
}

inline double multiSine (double x) noexcept
{
    constexpr double norm = 1.0 / (0.70710678118654752 + 1.0 / 3.0 + 0.70710678118654752 / 3.0);
    return (std::sin (x) + std::sin (2.0 * x) * (1.0 / 3.0) + std::sin (3.0 * x) * (1.0 / 3.0)) * norm;
}

inline double lfoValue (Wave w, double x) noexcept // x = phase in radians, result in [-1, 1]
{
    switch (w)
    {
        case Wave::sine3:  { const double s = parabolicSine (x); return s * s * s; }
        case Wave::multi:  return multiSine (x);
        case Wave::multi3: { const double s = multiSine (x); return s * s * s; }
        case Wave::sine:
        default:           return parabolicSine (x);
    }
}

// FL's LFOs run very slightly slower than the knob value (measured, ~0.1 % below 2 Hz)
inline double lfoRateCorrection (double hz) noexcept
{
    if (hz <= 1.25) return 0.998860;
    if (hz >= 2.45) return 0.999918;
    return 0.998860 + (hz - 1.25) / (2.45 - 1.25) * (0.999918 - 0.998860);
}

// 1st-order bilinear (tan-prewarped) high/low-pass, as in FL's crossover
struct OnePole
{
    double x1 = 0.0, y1 = 0.0;
    void reset() { x1 = y1 = 0.0; }
    static double coef (double fc, double fs) { const double t = std::tan (pi * fc / fs); return (1.0 - t) / (1.0 + t); }
    double highPass (double x, double p) noexcept { const double y = 0.5 * (1.0 + p) * (x - x1) + p * y1; x1 = x; y1 = y; return y; }
    double lowPass  (double x, double p) noexcept { const double y = 0.5 * (1.0 - p) * (x + x1) + p * y1; x1 = x; y1 = y; return y; }
};

// Mono delay line, linear interpolation: read (D) returns the signal D samples ago
class Ring
{
public:
    void prepare (int minSize)
    {
        int n = 1;
        while (n < minSize) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1;
        w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    void push (float x) noexcept { w = (w + 1) & mask; buf[(size_t) w] = x; } // w = newest sample

    float read (double delay) const noexcept
    {
        const double fl = std::floor (delay);
        const int k = (int) fl;
        const float frac = (float) (delay - fl);
        const float a = buf[(size_t) ((w - k) & mask)];
        const float b = buf[(size_t) ((w - k - 1) & mask)];
        return a + frac * (b - a);
    }

private:
    std::vector<float> buf;
    int mask = 0, w = 0;
};
} // namespace flchorus
