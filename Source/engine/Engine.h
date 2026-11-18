#pragma once
// Step Grid engine: a tempo-synced 32-step sequencer that fires one effect per step on the
// incoming audio. No JUCE dependency, so it is unit tested on its own (tests/EngineTests.cpp).
//
// Signal flow per sample:
//   input -> always-on ring buffer (20 s)
//   step clock (host ppq, or free-running BPM) -> step changes land on the exact sample
//   two voices (current + previous) render their effect from the input / ring buffer and
//   crossfade over 4 ms whenever the effect changes, so step changes never click
//   + Echo Throw send -> synced ping-pong delay whose tail outlives the step
//   mix (dry / wet) -> output gain

#include "Dsp.h"
#include "Pattern.h"
#include <array>
#include <atomic>

namespace sg
{
struct Params
{
    float mix = 1.0f;           // 0..1
    float outputDb = 0.0f;
    bool sync = true;           // follow the host transport when it plays
    float bpm = 120.0f;         // own tempo (host stopped, no host tempo, or sync off)
    bool latch = false;         // pads toggle instead of momentary
    int chainEnd = 0;           // 0 = no chain; n = play A -> slot n (1 = A-B ... 7 = A-H) in order
    std::array<Fx, kNumPads> padFx { Fx::Stutter, Fx::Stutter, Fx::Reverse, Fx::TapeStop,
                                     Fx::Bitcrush, Fx::LpSweep, Fx::Gate, Fx::EchoThrow };
    std::array<float, kNumPads> padAmount { 0.5f, 0.75f, 0.9f, 0.5f, 0.7f, 0.7f, 0.6f, 0.7f };
};

struct Transport
{
    bool hasInfo = false;       // host gave us tempo / position
    bool playing = false;
    double ppq = 0.0;           // quarter-note position at the first sample of the block
    double bpm = 120.0;
};

struct Event
{
    enum Type : uint8_t { SelectPattern, PadOn, PadOff, AllPadsOff };
    int sample = 0;             // offset in the block
    Type type = SelectPattern;
    int index = 0;
};

class Engine
{
public:
    Engine();

    void prepare (double sampleRate, int maxBlock);
    void reset();
    void setParams (const Params& p);
    void setPattern (int slot, const Pattern& p);
    const Pattern& getPattern (int slot) const { return patterns[(size_t) clampSlot (slot)]; }
    void seed (uint32_t s) { rng.seed (s); }

    // Stereo, in place. Events must be sorted by sample offset.
    void process (float* left, float* right, int numSamples, const Transport& t,
                  const Event* events = nullptr, int numEvents = 0);

    // ---- status (safe to read from any thread)
    int currentPattern() const { return stCurPattern.load(); }
    int queuedPattern() const { return stQueued.load(); }        // -1 = none
    int currentStep() const { return stCurStep.load(); }
    int activePad() const { return stActivePad.load(); }         // -1 = grid is playing
    uint32_t padMask() const { return stPadMask.load(); }        // pads held or latched
    bool hostLocked() const { return stHostLocked.load(); }
    float bpmInUse() const { return stBpm.load(); }
    Fx soundingFx() const { return (Fx) stFx.load(); }
    uint32_t triggerCount() const { return stTriggers.load(); }
    int lastTriggeredStep() const { return stLastTrig.load(); }

    // Absolute sample counter (since prepare/reset) and the sample on which the latest step began.
    int64_t sampleCount() const { return samplesDone; }
    int64_t lastStepStartSample() const { return lastStepStart; }

    double sampleRate() const { return sr; }

private:
    struct Voice
    {
        Fx fx = Fx::Off;
        float amount = 0;
        int64_t start = 0;      // ring index of the sample where the voice began
        int64_t el = 0;         // samples since start
        double dur = 1;         // nominal length in samples (step length x tie)
        double readPos = 0;     // tape
        float speed = 1;
        float pan = 0, panTarget = 0;
        float gateG = 1;
        float pitchPhase = 0;
        float crushL = 0, crushR = 0;
        int crushCnt = 0;
        Svf f[2];
    };

    static int clampSlot (int s) { return s < 0 ? 0 : (s >= kNumPatterns ? kNumPatterns - 1 : s); }
    int64_t absStep (double ppq, const Pattern& p) const;
    void onNewStep (double ppq, int64_t abs, bool jump);
    void triggerGridStep();
    void trigger (Fx fx, float amount, double durSamples);
    void handleEvent (const Event& e);
    void updatePads();
    void render (Voice& v, float xL, float xR, float& oL, float& oR, float& send);

    double sr = 48000.0;
    Params params;
    std::array<Pattern, kNumPatterns> patterns {};
    Ring ring;
    Rng rng;

    // clock
    double bps = 0;                  // beats per sample in use
    double freePpq = 0, lastPpq = 0;
    bool wasHost = false;
    bool haveAbs = false;
    int64_t lastAbs = 0;
    int curPattern = 0, queued = -1, patStep = 0;
    int gridOwner = -1;              // step whose effect the current voice is playing (-1 = none)
    int64_t samplesDone = 0, lastStepStart = 0;

    // voices
    Voice cur, prev;
    bool prevActive = false;
    int xfadePos = 0, xfadeLen = 192;
    int panFlip = 1;

    // pads
    std::array<bool, kNumPads> padHeld {}, padLatched {};
    std::array<uint32_t, kNumPads> padOrder {};
    uint32_t padCounter = 0;
    int activePadIdx = -1;

    // echo
    DelayLine echoL, echoR;
    OnePole echoDampL, echoDampR;
    float echoFb = 0.5f;
    Smoother sendSm, outGain, mixSm;

    std::atomic<int> stCurPattern { 0 }, stQueued { -1 }, stCurStep { 0 }, stActivePad { -1 }, stFx { 0 }, stLastTrig { -1 };
    std::atomic<uint32_t> stPadMask { 0 }, stTriggers { 0 };
    std::atomic<bool> stHostLocked { false };
    std::atomic<float> stBpm { 120.0f };
};
} // namespace sg
