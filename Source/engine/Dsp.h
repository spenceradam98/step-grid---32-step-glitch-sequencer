#pragma once
// Small scalar DSP building blocks (shared style with touchstorm). Plain C++17, no SIMD
// intrinsics, no -march flags: runs on any x86-64 (macOS 11 Intel included) and on arm64.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace sg
{
constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

inline float clampf (float v, float lo, float hi) { return std::min (hi, std::max (lo, v)); }
inline float lerpf (float a, float b, float t) { return a + (b - a) * t; }
inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }

struct Smoother
{
    float value = 0, target = 0, coeff = 0.001f;
    void setTime (double sr, float seconds) { coeff = 1.0f - std::exp (-1.0f / (float) (std::max (1e-4f, seconds) * sr)); }
    void snap (float v) { value = target = v; }
    float next() { value += (target - value) * coeff; return value; }
};

struct Rng
{
    uint32_t s = 0x1234567u;
    void seed (uint32_t v) { s = v ? v : 0x1234567u; }
    float next() { s = s * 1664525u + 1013904223u; return (float) (s >> 8) * (1.0f / 8388608.0f) - 1.0f; }   // -1..1
    float uni() { return 0.5f * (next() + 1.0f); }                                                             // 0..1
};

// Topology-preserving state variable filter (Simper / Zavalishin). One channel.
struct Svf
{
    float ic1 = 0, ic2 = 0, g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0;
    float lp = 0, bp = 0, hp = 0;
    void reset() { ic1 = ic2 = 0; lp = bp = hp = 0; }
    void set (double sr, float hz, float q)
    {
        hz = clampf (hz, 10.0f, (float) sr * 0.45f);
        g = std::tan (kPi * hz / (float) sr);
        k = 1.0f / std::max (0.05f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void tick (float x)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
    void copyCoeffs (const Svf& o) { g = o.g; k = o.k; a1 = o.a1; a2 = o.a2; a3 = o.a3; }
};

struct OnePole
{
    float z = 0, a = 0;
    void set (double sr, float hz) { a = std::exp (-kTwoPi * clampf (hz, 1.0f, (float) sr * 0.49f) / (float) sr); }
    float lpf (float x) { z = x + a * (z - x); return z; }
    void reset() { z = 0; }
};

// Stereo ring buffer addressed by an absolute, ever-increasing sample index.
// The input is always written here so Stutter, Reverse, Tape and Pitch can read the recent past.
struct Ring
{
    std::vector<float> l, r;
    int64_t mask = 0;
    int64_t written = 0;   // absolute index of the next sample to be written
    void allocate (int minSamples)
    {
        int64_t n = 1;
        while (n < (int64_t) minSamples + 8) n <<= 1;
        l.assign ((size_t) n, 0.0f);
        r.assign ((size_t) n, 0.0f);
        mask = n - 1;
        written = 0;
    }
    void clear() { std::fill (l.begin(), l.end(), 0.0f); std::fill (r.begin(), r.end(), 0.0f); }
    int64_t size() const { return mask + 1; }
    void push (float a, float b) { l[(size_t) (written & mask)] = a; r[(size_t) (written & mask)] = b; ++written; }
    // Clamp an absolute position into what the buffer still holds.
    double clampPos (double pos) const
    {
        const double newest = (double) (written - 1), oldest = (double) (written - size() + 4);
        return std::min (newest, std::max (oldest, pos));
    }
    float at (const std::vector<float>& b, int64_t i) const { return b[(size_t) (i & mask)]; }
    // Cubic (Catmull-Rom) read at a fractional absolute position.
    void read (double pos, float& outL, float& outR) const
    {
        pos = clampPos (pos);
        const int64_t i = (int64_t) std::floor (pos);
        const float f = (float) (pos - (double) i);
        const int64_t i0 = i - 1, i2 = std::min (i + 1, written - 1), i3 = std::min (i + 2, written - 1);
        auto cubic = [f] (float y0, float y1, float y2, float y3)
        {
            const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3,
                        c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * f + c2) * f + c1) * f + y1;
        };
        outL = cubic (at (l, i0), at (l, i), at (l, i2), at (l, i3));
        outR = cubic (at (r, i0), at (r, i), at (r, i2), at (r, i3));
    }
    void readInt (int64_t i, float& outL, float& outR) const
    {
        i = std::min (written - 1, std::max (written - size() + 1, i));
        outL = at (l, i);
        outR = at (r, i);
    }
};

// Mono delay line for the echo send (integer + linear read is enough for a fixed synced time).
struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;
    void allocate (int maxSamples)
    {
        int n = 1;
        while (n < maxSamples + 4) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1;
        w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    float read (float d) const
    {
        d = clampf (d, 1.0f, (float) (mask - 2));
        const int i = (int) d;
        const float f = d - (float) i;
        const float a = buf[(size_t) ((w - i) & mask)], b = buf[(size_t) ((w - i - 1) & mask)];
        return a + (b - a) * f;
    }
};
} // namespace sg
