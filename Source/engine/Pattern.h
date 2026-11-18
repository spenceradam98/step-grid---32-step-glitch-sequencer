#pragma once
// Step-grid pattern data: 8 slots of up to 32 steps, one effect per step. No JUCE dependency.

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace sg
{
constexpr int kNumSteps = 32;
constexpr int kNumPatterns = 8;
constexpr int kNumPads = 8;

enum class Fx : uint8_t
{
    Off, Stutter, TapeStop, TapeStart, Reverse, Bitcrush, LpSweep, HpSweep, Gate, PitchDown, PanFlick, EchoThrow,
    Count
};
constexpr int kNumFx = (int) Fx::Count;

const char* fxName (Fx f);        // "Stutter"
const char* fxShortName (Fx f);   // "STUT"
char fxChar (Fx f);               // pattern string letter, '.' for Off
Fx fxFromChar (char c);           // unknown letters give Off

// What the Amount knob means for this effect, as text (e.g. Stutter 0.7 -> "1/32").
std::string amountText (Fx f, float amount);
// Stutter repeat length in beats for an amount: 1/8, 1/16, 1/32 or 1/64 note.
double stutterBeats (float amount);

enum class Rate : uint8_t { Eighth, EighthT, Sixteenth, SixteenthT, ThirtySecond, ThirtySecondT, Count };
constexpr int kNumRates = (int) Rate::Count;
const char* rateName (Rate r);
double rateBeats (Rate r);        // step length in quarter-note beats

// Exact float compare without -Wfloat-equal noise (pattern equality is used for change detection).
inline bool sameValue (float a, float b) { return ! (a < b) && ! (b < a); }

struct Step
{
    Fx fx = Fx::Off;
    float amount = 0.6f;    // 0..1, meaning depends on the effect
    float prob = 1.0f;      // 0..1 chance the step fires each pass
    int length = 1;         // 1..16 steps (ties the following steps to this one)
    bool operator== (const Step& o) const { return fx == o.fx && sameValue (amount, o.amount) && sameValue (prob, o.prob) && length == o.length; }
};

struct Pattern
{
    std::array<Step, kNumSteps> steps {};
    int length = 16;                 // 1..32 steps
    Rate rate = Rate::Sixteenth;
    float swing = 0.0f;              // 0..1: odd steps start up to half a step late
    std::string name;                // display only

    bool isEmpty() const;
    // Which step owns position i (follows ties back); -1 if i is not covered by an active step.
    int ownerOf (int i) const;
    void clear();
    bool operator== (const Pattern& o) const { return steps == o.steps && length == o.length && rate == o.rate && sameValue (swing, o.swing); }
};

// Text round trip used for saved state. Never throws; bad input leaves defaults.
std::string serialize (const Pattern& p);
Pattern deserialize (const std::string& s);

// Musical random pattern (keeps downbeats mostly dry, busier toward the end of the bar).
void randomise (Pattern& p, uint32_t seed);

// Build a pattern from the compact preset text form:
//  steps: one char per step: . off, s stutter, t tape stop, u tape start, r reverse, b bitcrush,
//         l LP sweep, h HP sweep, g gate, p pitch down, f pan flick, e echo throw, '-' ties to the previous step.
//  amounts / probs (optional, aligned with steps): digit d -> (d + 1) / 10; space or missing = default.
Pattern makePattern (const char* name, Rate rate, int length, float swing, const char* steps,
                     const char* amounts = "", const char* probs = "");

struct FactoryPattern
{
    const char* category;
    Pattern pattern;
};
const std::vector<FactoryPattern>& factoryPatterns();
} // namespace sg
