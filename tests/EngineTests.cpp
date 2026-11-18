// Engine tests: plain C++17, no JUCE. Run with ctest or ./stepgrid_tests.
#include "Engine.h"
#include <cmath>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

using namespace sg;

static int failures = 0, checks = 0;
#define CHECK(cond) do { ++checks; if (! (cond)) { ++failures; std::printf ("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

namespace
{
constexpr double kSr = 48000.0;
constexpr double kBpm = 120.0;
const double kBps = kBpm / 60.0 / kSr;   // beats per sample

void setup (Engine& e, Params p = {})
{
    e.setParams (p);
    e.prepare (kSr, 512);
    e.seed (99);
}

Transport hostAt (int64_t sample)
{
    Transport t;
    t.hasInfo = true;
    t.playing = true;
    t.bpm = kBpm;
    t.ppq = (double) sample * kBps;
    return t;
}

// Runs one sample at a time and returns the absolute samples where the step changed.
std::vector<int64_t> stepStarts (Engine& e, int64_t total, bool host)
{
    std::vector<int64_t> starts;
    int64_t last = -1;
    Transport free;
    for (int64_t s = 0; s < total; ++s)
    {
        float l = 0, r = 0;
        e.process (&l, &r, 1, host ? hostAt (s) : free);
        if (e.lastStepStartSample() != last) { last = e.lastStepStartSample(); starts.push_back (last); }
    }
    return starts;
}

void runBlocks (Engine& e, std::vector<float>& L, std::vector<float>& R, int64_t offset = 0, int block = 512,
                const std::vector<Event>& evs = {}, bool host = true)
{
    const int n = (int) L.size();
    for (int pos = 0; pos < n; pos += block)
    {
        const int len = std::min (block, n - pos);
        std::vector<Event> local;
        for (auto ev : evs) if (ev.sample >= pos && ev.sample < pos + len) { ev.sample -= pos; local.push_back (ev); }
        Transport t;
        if (host) t = hostAt (offset + pos);
        e.process (L.data() + pos, R.data() + pos, len, t, local.data(), (int) local.size());
    }
}

Pattern single (Fx fx, int at, float amount = 1.0f, int length = 1, Rate rate = Rate::Sixteenth, int patLen = 16)
{
    Pattern p;
    p.rate = rate;
    p.length = patLen;
    p.steps[(size_t) at] = { fx, amount, 1.0f, length };
    return p;
}

float maxJump (const std::vector<float>& x)
{
    float m = 0;
    for (size_t i = 1; i < x.size(); ++i) m = std::max (m, std::fabs (x[i] - x[i - 1]));
    return m;
}
}

static void testStepBoundaries()
{
    std::puts ("step boundaries land on the exact sample (1/16 at 120 BPM = 6000 samples)");
    for (bool host : { true, false })
    {
        Params p;
        p.bpm = 120.0f;
        Engine e; setup (e, p);
        auto starts = stepStarts (e, 6000 * 40 + 10, host);
        CHECK (starts.size() == 41);
        bool exact = true;
        for (size_t i = 0; i < starts.size(); ++i) exact = exact && starts[i] == (int64_t) i * 6000;
        CHECK (exact);
    }
    // Same with 512-sample blocks: every step start is still a multiple of 6000.
    {
        Engine e; setup (e);
        int bad = 0, count = 0;
        int64_t last = -1;
        for (int64_t pos = 0; pos < 48000 * 8; pos += 512)
        {
            std::vector<float> l (512, 0.0f), r (512, 0.0f);
            e.process (l.data(), r.data(), 512, hostAt (pos));
            if (e.lastStepStartSample() != last) { last = e.lastStepStartSample(); ++count; if (last % 6000 != 0) ++bad; }
        }
        CHECK (bad == 0);
        CHECK (count >= 60);
    }
}

static void testSwingAndTriplets()
{
    std::puts ("swing and triplet grids");
    {
        Engine e; setup (e);
        Pattern p; p.swing = 0.5f;               // odd steps start a quarter step late
        e.setPattern (0, p);
        auto s = stepStarts (e, 24001, true);
        CHECK (s.size() == 5);
        if (s.size() == 5) { CHECK (s[1] == 7500); CHECK (s[2] == 12000); CHECK (s[3] == 19500); CHECK (s[4] == 24000); }
    }
    {
        Engine e; setup (e);
        Pattern p; p.rate = Rate::SixteenthT;     // 1/6 beat = 4000 samples
        e.setPattern (0, p);
        auto s = stepStarts (e, 4000 * 30 + 1, true);
        bool exact = s.size() == 31;
        for (size_t i = 0; exact && i < s.size(); ++i) exact = s[i] == (int64_t) i * 4000;
        CHECK (exact);
    }
}

static void testEffectStartsOnTheStep()
{
    std::puts ("an effect starts on the first sample of its step and not before");
    Engine e; setup (e);
    e.setPattern (0, single (Fx::Bitcrush, 1, 1.0f));
    std::vector<float> L (13000, 0.3f), R (13000, 0.3f);
    runBlocks (e, L, R, 0, 333);
    bool dryBefore = true;
    for (int i = 0; i < 6000; ++i) dryBefore = dryBefore && L[(size_t) i] == 0.3f;
    CHECK (dryBefore);
    CHECK (L[6000] != 0.3f);
    CHECK (std::fabs (L[6000 + 300] - 0.25f) < 1e-4f);   // 3-bit crush of 0.3 after the crossfade
}

static void testNoClicks()
{
    std::puts ("no clicks between steps (sine through every smooth effect, pattern switches and pads)");
    Engine e; setup (e);
    // every effect except Bitcrush (a crusher is a deliberate staircase)
    e.setPattern (0, makePattern ("A", Rate::Sixteenth, 16, 0.0f, "s.tu-r.lhgpfe-s.", "5 5 5 9 9 9 99 9"));
    e.setPattern (1, makePattern ("B", Rate::ThirtySecond, 32, 0.3f, "ss.r-g.p.f.e.h.l.s-s-ssst---u---", "36 9 9 9 9 9 9 9 3 6 9999      "));
    const int n = 48000 * 12;
    std::vector<float> L ((size_t) n), R ((size_t) n);
    for (int i = 0; i < n; ++i) L[(size_t) i] = R[(size_t) i] = 0.5f * std::sin (kTwoPi * 220.0f * (float) i / (float) kSr);
    std::vector<Event> ev = { { 48000 * 4 + 100, Event::SelectPattern, 1 }, { 48000 * 8 + 777, Event::PadOn, 0 },
                              { 48000 * 9, Event::PadOn, 3 }, { 48000 * 9 + 9000, Event::PadOff, 3 },
                              { 48000 * 10 + 5, Event::PadOff, 0 }, { 48000 * 10 + 20000, Event::SelectPattern, 0 } };
    runBlocks (e, L, R, 0, 256, ev);
    const float dryMax = 0.5f * kTwoPi * 220.0f / (float) kSr;   // ~0.0144
    std::printf ("    largest step %.4f (left), %.4f (right); dry sine %.4f\n", maxJump (L), maxJump (R), dryMax);
    CHECK (maxJump (L) < 0.06f);
    CHECK (maxJump (R) < 0.06f);
    CHECK (e.triggerCount() > 40);
}

static void testProbabilityAndTies()
{
    std::puts ("probability 0 never fires; a tied step fires once");
    {
        Engine e; setup (e);
        auto p = single (Fx::Stutter, 0);
        p.steps[0].prob = 0.0f;
        p.steps[4] = { Fx::Gate, 0.5f, 0.0f, 1 };
        e.setPattern (0, p);
        std::vector<float> L (48000 * 4, 0.1f), R = L;
        runBlocks (e, L, R);
        CHECK (e.triggerCount() == 0);
    }
    {
        Engine e; setup (e);
        e.setPattern (0, single (Fx::Stutter, 0, 0.5f, 4));
        std::vector<float> L (96000 * 2, 0.1f), R = L;   // 2 passes of 16 steps (4 s at 1/16)
        runBlocks (e, L, R);
        CHECK (e.triggerCount() == 2);
    }
    {
        Engine e; setup (e);
        auto p = single (Fx::Gate, 0);
        p.steps[0].prob = 0.5f;
        p.length = 1;                         // fires on every step with probability 1/2
        e.setPattern (0, p);
        std::vector<float> L (6000 * 400, 0.1f), R = L;
        runBlocks (e, L, R);
        CHECK (e.triggerCount() > 150 && e.triggerCount() < 250);
    }
}

static void testPatternSwitchAndChain()
{
    std::puts ("pattern switch waits for the next step; chain plays A -> B");
    {
        Engine e; setup (e);
        std::vector<float> L (9000, 0.0f), R = L;
        runBlocks (e, L, R, 0, 9000, { { 1000, Event::SelectPattern, 3 } });
        CHECK (e.currentPattern() == 3);    // switched at sample 6000
        Engine e2; setup (e2);
        std::vector<float> L2 (5000, 0.0f), R2 = L2;
        e2.process (L2.data(), R2.data(), 10, hostAt (0));
        Event ev { 0, Event::SelectPattern, 3 };
        e2.process (L2.data(), R2.data(), 4000, hostAt (10), &ev, 1);
        CHECK (e2.currentPattern() == 0);   // not yet
        CHECK (e2.queuedPattern() == 3);
    }
    {
        Params prm; prm.chainEnd = 1;
        Engine e; setup (e, prm);
        Pattern a; a.length = 4;
        Pattern b; b.length = 2;
        e.setPattern (0, a);
        e.setPattern (1, b);
        std::vector<int> seen;
        for (int s = 0; s < 6000 * 12; s += 6000)
        {
            std::vector<float> L (6000, 0.0f), R = L;
            e.process (L.data(), R.data(), 6000, hostAt (s));
            seen.push_back (e.currentPattern() * 10 + e.currentStep());
        }
        const std::vector<int> want = { 0, 1, 2, 3, 10, 11, 0, 1, 2, 3, 10, 11 };
        CHECK (seen == want);
    }
}

static void testHostJump()
{
    std::puts ("host loop / jump resyncs the step to the song position");
    Engine e; setup (e);
    std::vector<float> L (6000 * 7, 0.0f), R = L;
    runBlocks (e, L, R);
    CHECK (e.currentStep() == 6);
    std::vector<float> l2 (100, 0.0f), r2 = l2;
    runBlocks (e, l2, r2, 6000 * 3);   // jump back to step 3
    CHECK (e.currentStep() == 3);
    CHECK (e.lastStepStartSample() == 6000 * 7);
}

static void testPads()
{
    std::puts ("perform pads override the grid while held");
    Params prm;
    prm.padFx[2] = Fx::Reverse;
    Engine e; setup (e, prm);
    e.setPattern (0, makePattern ("busy", Rate::Sixteenth, 16, 0, "gggggggggggggggg"));
    std::vector<float> L (6000 * 2, 0.1f), R = L;
    runBlocks (e, L, R);
    const auto before = e.triggerCount();
    std::vector<float> L2 (6000 * 4, 0.1f), R2 = L2;
    runBlocks (e, L2, R2, 6000 * 2, 512, { { 0, Event::PadOn, 2 } });
    CHECK (e.activePad() == 2);
    CHECK (e.soundingFx() == Fx::Reverse);
    CHECK (e.triggerCount() == before);   // grid did not fire while the pad was held
    std::vector<float> L3 (6000 * 2, 0.1f), R3 = L3;
    runBlocks (e, L3, R3, 6000 * 6, 512, { { 10, Event::PadOff, 2 } });
    CHECK (e.activePad() == -1);
    CHECK (e.triggerCount() > before);

    // latch: a tap keeps the pad on until tapped again
    prm.latch = true;
    Engine e2; setup (e2, prm);
    std::vector<float> L4 (1000, 0.0f), R4 = L4;
    runBlocks (e2, L4, R4, 0, 1000, { { 10, Event::PadOn, 5 }, { 20, Event::PadOff, 5 } });
    CHECK (e2.activePad() == 5);
    runBlocks (e2, L4, R4, 1000, 1000, { { 10, Event::PadOn, 5 } });
    CHECK (e2.activePad() == -1);
}

static void testEffects()
{
    std::puts ("stutter repeats the slice, reverse plays backwards, tape stop ends silent");
    const int n = 6000 * 4;
    auto ramp = [] (int len) { std::vector<float> v ((size_t) len); for (int i = 0; i < len; ++i) v[(size_t) i] = (float) i * 1e-5f; return v; };
    {
        Engine e; setup (e);
        e.setPattern (0, single (Fx::Stutter, 0, 0.5f, 4));   // 1/16 slice = 6000 samples
        auto L = ramp (n), R = L;
        runBlocks (e, L, R);
        bool repeats = true;
        for (int k = 200; k < 5800; k += 37) repeats = repeats && std::fabs (L[(size_t) (6000 + k)] - (float) k * 1e-5f) < 1e-6f
                                                                && std::fabs (L[(size_t) (18000 + k)] - (float) k * 1e-5f) < 1e-6f;
        CHECK (repeats);
    }
    {
        Engine e; setup (e);
        e.setPattern (0, single (Fx::Reverse, 2, 1.0f, 2));
        auto L = ramp (n), R = L;
        runBlocks (e, L, R);
        bool backwards = true;
        for (int k = 400; k < 11000; k += 53) backwards = backwards && std::fabs (L[(size_t) (12000 + k)] - (float) (12000 - 1 - k) * 1e-5f) < 1e-6f;
        CHECK (backwards);
    }
    {
        Engine e; setup (e);
        e.setPattern (0, single (Fx::TapeStop, 0, 0.5f, 4));
        std::vector<float> L ((size_t) n), R;
        for (int i = 0; i < n; ++i) L[(size_t) i] = 0.5f * std::sin ((float) i * 0.05f);
        R = L;
        runBlocks (e, L, R);
        float tail = 0;
        for (int i = n - 3000; i < n - 100; ++i) tail = std::max (tail, std::fabs (L[(size_t) i]));
        CHECK (tail < 1e-3f);
    }
    {
        // echo throw keeps ringing after its step ends
        Engine e; setup (e);
        e.setPattern (0, single (Fx::EchoThrow, 0, 0.8f));
        std::vector<float> L (48000, 0.0f), R;
        for (int i = 0; i < 3000; ++i) L[(size_t) i] = 0.5f * std::sin ((float) i * 0.07f);
        R = L;
        runBlocks (e, L, R);
        float late = 0;
        for (int i = 30000; i < 48000; ++i) late = std::max (late, std::fabs (L[(size_t) i]) + std::fabs (R[(size_t) i]));
        CHECK (late > 0.01f);
    }
}

static void testPatternData()
{
    std::puts ("pattern text round trip, presets and randomise");
    auto p = makePattern ("x", Rate::ThirtySecondT, 24, 0.4f, "s-.tb..ruue", "9 3 7", "  4");
    p.steps[31] = { Fx::Gate, 0.25f, 0.5f, 1 };
    const auto q = deserialize (serialize (p));
    CHECK (q == p);
    CHECK (p.steps[0].length == 2);
    CHECK (p.ownerOf (1) == 0);
    CHECK (p.ownerOf (2) == -1);
    CHECK (deserialize ("garbage").length == 16);

    const auto& f = factoryPatterns();
    CHECK (f.size() >= 40);
    std::set<std::string> names;
    for (auto& fp : f)
    {
        names.insert (fp.pattern.name);
        CHECK (! fp.pattern.isEmpty());
    }
    CHECK (names.size() == f.size());

    for (uint32_t s = 1; s < 50; ++s)
    {
        Pattern r;
        r.length = 8 + (int) (s % 25);
        randomise (r, s);
        bool inside = true;
        for (int i = 0; i < kNumSteps; ++i)
        {
            const auto& st = r.steps[(size_t) i];
            if (st.fx != Fx::Off) inside = inside && i + st.length <= r.length;
        }
        CHECK (inside);
    }
}

int main()
{
    testStepBoundaries();
    testSwingAndTriplets();
    testEffectStartsOnTheStep();
    testNoClicks();
    testProbabilityAndTies();
    testPatternSwitchAndChain();
    testHostJump();
    testPads();
    testEffects();
    testPatternData();
    std::printf ("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
