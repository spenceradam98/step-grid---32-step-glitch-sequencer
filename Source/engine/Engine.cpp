#include "Engine.h"

namespace sg
{
namespace
{
constexpr double kEps = 1e-7;    // in steps: keeps exact boundaries from rounding down
inline int64_t floorDiv (double x) { return (int64_t) std::floor (x); }
inline int wrapMod (int64_t a, int n) { const int64_t m = a % n; return (int) (m < 0 ? m + n : m); }
}

Engine::Engine()
{
    for (int i = 0; i < kNumPatterns; ++i) patterns[(size_t) i] = Pattern();
}

void Engine::prepare (double sampleRate, int)
{
    sr = sampleRate > 0 ? sampleRate : 48000.0;
    ring.allocate ((int) (sr * 20.0));
    echoL.allocate ((int) (sr * 2.5));
    echoR.allocate ((int) (sr * 2.5));
    echoDampL.set (sr, 4500.0f);
    echoDampR.set (sr, 4500.0f);
    xfadeLen = std::max (16, (int) std::lround (sr * 0.004));   // 4 ms between steps
    sendSm.setTime (sr, 0.003f);
    outGain.setTime (sr, 0.02f);
    mixSm.setTime (sr, 0.02f);
    reset();
}

void Engine::reset()
{
    ring.clear();
    ring.written = 0;
    echoL.clear(); echoR.clear();
    echoDampL.reset(); echoDampR.reset();
    cur = Voice(); prev = Voice();
    prevActive = false;
    xfadePos = 0;
    haveAbs = false;
    freePpq = lastPpq = 0;
    wasHost = false;
    patStep = 0;
    gridOwner = -1;
    samplesDone = lastStepStart = 0;
    sendSm.snap (0);
    outGain.snap (dbToGain (params.outputDb));
    mixSm.snap (params.mix);
    padHeld = {}; padLatched = {};
    activePadIdx = -1;
}

void Engine::setParams (const Params& p)
{
    const bool latchOff = params.latch && ! p.latch;
    params = p;
    params.chainEnd = std::max (0, std::min (kNumPatterns - 1, params.chainEnd));
    params.bpm = clampf (params.bpm, 20.0f, 300.0f);
    if (latchOff)
    {
        padLatched = {};
        updatePads();
    }
}

void Engine::setPattern (int slot, const Pattern& p)
{
    auto& dst = patterns[(size_t) clampSlot (slot)];
    dst = p;
    dst.length = std::max (1, std::min (kNumSteps, dst.length));
}

int64_t Engine::absStep (double ppq, const Pattern& p) const
{
    const double pos = ppq / rateBeats (p.rate) + kEps;
    const int64_t pair = floorDiv (pos * 0.5);
    const double within = pos - 2.0 * (double) pair;            // 0..2
    const double oddStart = 1.0 + 0.5 * clampf (p.swing, 0.0f, 1.0f);
    return 2 * pair + (within >= oddStart ? 1 : 0);
}

// ------------------------------------------------------------------ events and pads
void Engine::handleEvent (const Event& e)
{
    switch (e.type)
    {
        case Event::SelectPattern:
        {
            const int slot = clampSlot (e.index);
            if (! haveAbs) { curPattern = slot; queued = -1; stCurPattern.store (slot); }
            else queued = slot == curPattern ? -1 : slot;   // switches on the next step
            stQueued.store (queued);
            break;
        }
        case Event::PadOn:
            if (e.index < 0 || e.index >= kNumPads) return;
            if (params.latch)
            {
                padLatched[(size_t) e.index] = ! padLatched[(size_t) e.index];
                if (padLatched[(size_t) e.index]) padOrder[(size_t) e.index] = ++padCounter;
            }
            else
            {
                padHeld[(size_t) e.index] = true;
                padOrder[(size_t) e.index] = ++padCounter;
            }
            updatePads();
            break;
        case Event::PadOff:
            if (e.index < 0 || e.index >= kNumPads) return;
            padHeld[(size_t) e.index] = false;
            updatePads();
            break;
        case Event::AllPadsOff:
            padHeld = {}; padLatched = {};
            updatePads();
            break;
    }
}

void Engine::updatePads()
{
    int best = -1;
    uint32_t bestOrder = 0, mask = 0;
    for (int i = 0; i < kNumPads; ++i)
    {
        const bool on = padHeld[(size_t) i] || padLatched[(size_t) i];
        if (! on) continue;
        mask |= 1u << i;
        if (best < 0 || padOrder[(size_t) i] > bestOrder) { best = i; bestOrder = padOrder[(size_t) i]; }
    }
    stPadMask.store (mask);
    if (best == activePadIdx) return;
    activePadIdx = best;
    stActivePad.store (best);
    gridOwner = -1;
    if (best >= 0)
        trigger (params.padFx[(size_t) best], params.padAmount[(size_t) best], 1.0 / std::max (1e-9, bps));   // one beat
    else
        trigger (Fx::Off, 0, 1);   // back to dry; the grid takes over again on the next step
}

// ------------------------------------------------------------------ sequencing
void Engine::onNewStep (double ppq, int64_t abs, bool jump)
{
    lastStepStart = samplesDone;
    if (queued >= 0)
    {
        curPattern = queued;
        queued = -1;
        const auto& p = patterns[(size_t) curPattern];
        lastAbs = absStep (ppq, p);
        patStep = wrapMod (lastAbs, p.length);
    }
    else if (jump)
    {
        patStep = wrapMod (abs, patterns[(size_t) curPattern].length);
    }
    else
    {
        ++patStep;
        if (patStep >= patterns[(size_t) curPattern].length)
        {
            patStep = 0;
            if (params.chainEnd > 0)
            {
                const int next = (curPattern + 1 > params.chainEnd) ? 0 : curPattern + 1;
                if (next != curPattern)
                {
                    curPattern = next;
                    lastAbs = absStep (ppq, patterns[(size_t) curPattern]);   // the new pattern may use another rate
                }
            }
        }
    }
    stCurPattern.store (curPattern);
    stQueued.store (queued);
    stCurStep.store (patStep);
    triggerGridStep();
}

void Engine::triggerGridStep()
{
    if (activePadIdx >= 0) return;   // a perform pad overrides the grid
    const auto& p = patterns[(size_t) curPattern];
    const int owner = p.ownerOf (patStep);
    if (owner >= 0 && owner != patStep)
    {
        if (gridOwner == owner) return;          // tied step: keep the running effect (or keep dry if it did not fire)
        gridOwner = -1;
        trigger (Fx::Off, 0, 1);                 // landed in the middle of a tie (jump): stay dry
        return;
    }
    if (owner < 0)
    {
        gridOwner = -1;
        trigger (Fx::Off, 0, 1);
        return;
    }
    const auto& s = p.steps[(size_t) owner];
    gridOwner = owner;
    const bool fires = s.prob >= 1.0f || rng.uni() < s.prob;
    if (! fires)
    {
        trigger (Fx::Off, 0, 1);
        return;
    }
    const int len = std::max (1, std::min (s.length, p.length - owner));
    trigger (s.fx, s.amount, (double) len * rateBeats (p.rate) / std::max (1e-9, bps));
    stTriggers.fetch_add (1);
    stLastTrig.store (owner);
}

void Engine::trigger (Fx fx, float amount, double durSamples)
{
    if (fx == Fx::Off && cur.fx == Fx::Off && ! prevActive) return;   // already dry
    // Keep whichever voice is louder as the one we fade from.
    const bool midFade = prevActive && xfadePos < xfadeLen / 2;
    if (! midFade) prev = cur;
    prevActive = true;
    xfadePos = 0;

    Voice v;
    v.fx = fx;
    v.amount = clampf (amount, 0.0f, 1.0f);
    v.start = ring.written;          // this sample is written before the voices render
    v.dur = std::max (1.0, durSamples);
    v.readPos = (double) v.start;
    if (fx == Fx::PanFlick)
    {
        panFlip = -panFlip;
        v.panTarget = (float) panFlip * (0.4f + 0.6f * v.amount);
    }
    cur = v;
    stFx.store ((int) fx);
}

// ------------------------------------------------------------------ effects
void Engine::render (Voice& v, float xL, float xR, float& oL, float& oR, float& send)
{
    const float a = v.amount;
    const double t = std::min (1.0, (double) v.el / v.dur);
    send = 0;
    switch (v.fx)
    {
        case Fx::Off:
        case Fx::Count:
            oL = xL; oR = xR;
            break;

        case Fx::Stutter:
        {
            const int64_t slice = std::max<int64_t> (32, (int64_t) std::llround (stutterBeats (a) / bps));
            const int64_t pos = v.el % slice, rep = v.el / slice;
            ring.readInt (v.start + pos, oL, oR);      // first pass is live, then it repeats
            const float fade = (float) std::min<int64_t> (slice / 4, (int64_t) (sr * 0.001) + 1);
            float g = std::min (1.0f, (float) (slice - pos) / fade);
            if (rep > 0) g = std::min (g, (float) (pos + 1) / fade);
            oL *= g; oR *= g;
            break;
        }

        case Fx::Reverse:
        {
            float rl, rr;
            ring.readInt (v.start - 1 - v.el, rl, rr);
            const float w = 0.5f + 0.5f * a;
            oL = xL + (rl - xL) * w;
            oR = xR + (rr - xR) * w;
            break;
        }

        case Fx::TapeStop:
        case Fx::TapeStart:
        {
            const double span = std::max (1.0, v.dur * lerpf (1.0f, 0.3f, a));
            const float x = (float) std::min (1.0, (double) v.el / span);
            v.speed = v.fx == Fx::TapeStop ? std::pow (1.0f - x, 1.4f) : x * x;
            ring.read (v.readPos, oL, oR);
            v.readPos = std::min ((double) (ring.written - 1), v.readPos + (double) v.speed);
            const float g = clampf (v.speed * 4.0f, 0.0f, 1.0f);
            oL *= g; oR *= g;
            break;
        }

        case Fx::Bitcrush:
        {
            if (v.crushCnt-- <= 0)
            {
                const float levels = std::pow (2.0f, lerpf (10.0f, 3.0f, a) - 1.0f);
                v.crushCnt = (int) (a * a * 20.0f);
                v.crushL = std::round (xL * levels) / levels;
                v.crushR = std::round (xR * levels) / levels;
            }
            oL = v.crushL; oR = v.crushR;
            break;
        }

        case Fx::LpSweep:
        case Fx::HpSweep:
        {
            if ((v.el & 15) == 0)
            {
                float hz, q;
                if (v.fx == Fx::LpSweep) { const float target = lerpf (2500.0f, 180.0f, a); hz = 18000.0f * std::pow (target / 18000.0f, (float) t); q = 0.7f + 1.8f * a; }
                else { const float target = lerpf (600.0f, 5000.0f, a); hz = 20.0f * std::pow (target / 20.0f, (float) t); q = 0.7f + 1.2f * a; }
                v.f[0].set (sr, hz, q);
                v.f[1].copyCoeffs (v.f[0]);
            }
            v.f[0].tick (xL); v.f[1].tick (xR);
            if (v.fx == Fx::LpSweep) { oL = v.f[0].lp; oR = v.f[1].lp; }
            else { oL = v.f[0].hp; oR = v.f[1].hp; }
            break;
        }

        case Fx::Gate:
        {
            const double chunk = 0.125 / bps;                     // 1/32 note
            const bool open = std::fmod ((double) v.el, chunk) < chunk * 0.5;
            const float target = open ? 1.0f : 1.0f - (0.4f + 0.6f * a);
            v.gateG += (target - v.gateG) * (1.0f - std::exp (-1.0f / (float) (0.001 * sr)));
            oL = xL * v.gateG; oR = xR * v.gateG;
            break;
        }

        case Fx::PitchDown:
        {
            const float ratio = std::pow (2.0f, -std::round (a * 12.0f) / 12.0f);
            const float window = (float) (0.05 * sr);
            v.pitchPhase += (1.0f - ratio) / window;
            v.pitchPhase -= std::floor (v.pitchPhase);
            float p2 = v.pitchPhase + 0.5f;
            p2 -= std::floor (p2);
            const double now = (double) (ring.written - 1);
            float l1, r1, l2, r2;
            ring.read (now - 1.0 - v.pitchPhase * window, l1, r1);
            ring.read (now - 1.0 - p2 * window, l2, r2);
            const float w1 = std::sin (kPi * v.pitchPhase), w2 = std::sin (kPi * p2);
            oL = l1 * w1 * w1 + l2 * w2 * w2;
            oR = r1 * w1 * w1 + r2 * w2 * w2;
            break;
        }

        case Fx::PanFlick:
        {
            v.pan += (v.panTarget - v.pan) * (1.0f - std::exp (-1.0f / (float) (0.003 * sr)));
            const float p = std::fabs (v.pan);
            const float m = 0.5f * (xL + xR) * 1.41421356f;
            const float ang = (v.pan + 1.0f) * kPi * 0.25f;
            oL = xL + (m * std::cos (ang) - xL) * p;
            oR = xR + (m * std::sin (ang) - xR) * p;
            break;
        }

        case Fx::EchoThrow:
            oL = xL; oR = xR;
            send = 1.0f;
            echoFb = 0.35f + 0.45f * a;
            break;
    }
    ++v.el;
}

// ------------------------------------------------------------------ main loop
void Engine::process (float* left, float* right, int n, const Transport& tr, const Event* events, int numEvents)
{
    const bool host = params.sync && tr.hasInfo && tr.playing;
    const double bpm = (params.sync && tr.hasInfo && tr.bpm > 1.0) ? tr.bpm : (double) params.bpm;
    bps = bpm / (60.0 * sr);
    stBpm.store ((float) bpm);
    stHostLocked.store (host);
    if (! host && wasHost) freePpq = lastPpq;   // keep going from where the host stopped
    wasHost = host;

    outGain.target = dbToGain (params.outputDb);
    mixSm.target = clampf (params.mix, 0.0f, 1.0f);
    const float echoDelay = (float) std::min (0.75 / bps, (double) sr * 2.4);   // dotted 1/8

    int ev = 0;
    for (int i = 0; i < n; ++i)
    {
        while (ev < numEvents && events[ev].sample <= i) handleEvent (events[ev++]);

        const double ppq = host ? tr.ppq + (double) i * bps : freePpq;
        if (! host) freePpq += bps;
        lastPpq = ppq;

        const int64_t abs = absStep (ppq, patterns[(size_t) curPattern]);
        if (! haveAbs || abs != lastAbs)
        {
            const bool jump = ! haveAbs || abs != lastAbs + 1;
            haveAbs = true;
            lastAbs = abs;
            onNewStep (ppq, abs, jump);
        }

        const float xL = left[i], xR = right[i];
        ring.push (xL, xR);

        float cL, cR, cS;
        render (cur, xL, xR, cL, cR, cS);
        float wL = cL, wR = cR, send = cS;
        if (prevActive)
        {
            float pL, pR, pS;
            render (prev, xL, xR, pL, pR, pS);
            const float w = (float) (xfadePos + 1) / (float) xfadeLen;
            wL = pL + (cL - pL) * w;
            wR = pR + (cR - pR) * w;
            send = pS + (cS - pS) * w;
            if (++xfadePos >= xfadeLen) prevActive = false;
        }

        // Echo throw: ping-pong delay fed only while a throw step plays, tail rings on.
        sendSm.target = send;
        const float s = sendSm.next();
        const float eL = echoL.read (echoDelay), eR = echoR.read (echoDelay);
        echoL.push (echoDampL.lpf (0.5f * (xL + xR) * s + eR * echoFb));
        echoR.push (echoDampR.lpf (eL * echoFb));
        wL += eL * 0.8f;
        wR += eR * 0.8f;

        const float mix = mixSm.next(), g = outGain.next();
        left[i] = (xL + (wL - xL) * mix) * g;
        right[i] = (xR + (wR - xR) * mix) * g;
        ++samplesDone;
    }
    while (ev < numEvents) handleEvent (events[ev++]);
}
} // namespace sg
