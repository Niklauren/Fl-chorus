#pragma once

#include <array>
#include <cmath>
#include <vector>

// Chorus engine laid out like FL Studio's Fruity Chorus (per its manual):
//  - three chorus voices, each a delay line swept between Delay and Delay + Depth
//    by its own LFO (Sine, Triangle or Multi-sine = sine + sine at 3x speed)
//  - the right channel's LFOs run Stereo degrees ahead of the left ones
//  - an optional crossover limits the chorus to the low or the high band
// Without the original's internals this follows the documented behaviour;
// exact constants can be calibrated against FL Studio renders.
namespace flchorus
{
enum class Wave { sine = 0, triangle = 1, multiSine = 2 };

inline constexpr int numVoices = 3;
inline constexpr double twoPi = 6.283185307179586;

inline double lfoValue (Wave w, double phase) noexcept // phase in [0,1), result in [-1, 1]
{
    switch (w)
    {
        case Wave::triangle:
        {
            // in phase with the sine: 0 at 0, +1 at 0.25, -1 at 0.75
            const double p = phase - 0.25;
            return 1.0 - 4.0 * std::abs (p - std::floor (p + 0.5));
        }
        case Wave::multiSine:
        {
            // sin(x) + sin(3x), peak 1.5396 -> normalised to +-1
            const double x = twoPi * phase;
            return (std::sin (x) + std::sin (3.0 * x)) * (1.0 / 1.5396007178390022);
        }
        case Wave::sine:
        default:
            return std::sin (twoPi * phase);
    }
}

// Mono ring buffer with 4-point Hermite interpolation for fractional reads
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
    void push (float x) noexcept { buf[(size_t) w] = x; w = (w + 1) & mask; }

    // delay in samples behind the newest sample, >= 1
    float read (double delay) const noexcept
    {
        const double pos = (double) (w - 1) - delay;
        const double fl = std::floor (pos);
        const int i = (int) fl;
        const float t = (float) (pos - fl);
        const float xm1 = buf[(size_t) ((i - 1) & mask)];
        const float x0  = buf[(size_t) (i & mask)];
        const float x1  = buf[(size_t) ((i + 1) & mask)];
        const float x2  = buf[(size_t) ((i + 2) & mask)];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
    }

private:
    std::vector<float> buf;
    int mask = 0, w = 0;
};
} // namespace flchorus
