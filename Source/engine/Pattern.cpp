#include "Pattern.h"
#include "Dsp.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace sg
{
namespace
{
struct FxInfo { const char* name; const char* shortName; char ch; };
const FxInfo kFx[kNumFx] = {
    { "Off", "OFF", '.' },        { "Stutter", "STUT", 's' },   { "Tape Stop", "STOP", 't' },
    { "Tape Start", "START", 'u' }, { "Reverse", "REV", 'r' },  { "Bitcrush", "CRUSH", 'b' },
    { "LP Sweep", "LP", 'l' },    { "HP Sweep", "HP", 'h' },    { "Gate", "GATE", 'g' },
    { "Pitch Down", "PITCH", 'p' }, { "Pan Flick", "PAN", 'f' }, { "Echo Throw", "ECHO", 'e' },
};
int fxIndex (Fx f) { const int i = (int) f; return i >= 0 && i < kNumFx ? i : 0; }
}

const char* fxName (Fx f) { return kFx[fxIndex (f)].name; }
const char* fxShortName (Fx f) { return kFx[fxIndex (f)].shortName; }
char fxChar (Fx f) { return kFx[fxIndex (f)].ch; }
Fx fxFromChar (char c)
{
    for (int i = 0; i < kNumFx; ++i) if (kFx[i].ch == c) return (Fx) i;
    return Fx::Off;
}

double stutterBeats (float a)
{
    if (a < 0.3f) return 0.5;
    if (a < 0.65f) return 0.25;
    if (a < 0.85f) return 0.125;
    return 0.0625;
}

std::string amountText (Fx f, float a)
{
    char buf[48];
    a = clampf (a, 0.0f, 1.0f);
    switch (f)
    {
        case Fx::Stutter:
        {
            const double b = stutterBeats (a);
            std::snprintf (buf, sizeof buf, "1/%d", (int) std::lround (4.0 / b));
            break;
        }
        case Fx::Bitcrush: std::snprintf (buf, sizeof buf, "%.0f bit", std::round (lerpf (10.0f, 3.0f, a))); break;
        case Fx::PitchDown: std::snprintf (buf, sizeof buf, "-%.0f st", std::round (a * 12.0f)); break;
        case Fx::Off: std::snprintf (buf, sizeof buf, "-"); break;
        default: std::snprintf (buf, sizeof buf, "%.0f%%", a * 100.0f); break;
    }
    return buf;
}

const char* rateName (Rate r)
{
    static const char* n[] = { "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/32T" };
    const int i = (int) r;
    return i >= 0 && i < kNumRates ? n[i] : "1/16";
}

double rateBeats (Rate r)
{
    switch (r)
    {
        case Rate::Eighth: return 0.5;
        case Rate::EighthT: return 1.0 / 3.0;
        case Rate::Sixteenth: return 0.25;
        case Rate::SixteenthT: return 1.0 / 6.0;
        case Rate::ThirtySecond: return 0.125;
        case Rate::ThirtySecondT: return 1.0 / 12.0;
        default: return 0.25;
    }
}

bool Pattern::isEmpty() const
{
    for (auto& s : steps) if (s.fx != Fx::Off) return false;
    return true;
}

int Pattern::ownerOf (int i) const
{
    if (i < 0 || i >= kNumSteps) return -1;
    for (int j = i; j >= 0; --j)
    {
        const auto& s = steps[(size_t) j];
        if (s.fx != Fx::Off && j + s.length > i) return j;
        if (j < i && s.fx != Fx::Off) return -1;   // an earlier active step that ends before i
    }
    return -1;
}

void Pattern::clear()
{
    steps = {};
}

std::string serialize (const Pattern& p)
{
    std::ostringstream o;
    o << "v1 " << p.length << ' ' << (int) p.rate << ' ' << p.swing;
    for (int i = 0; i < kNumSteps; ++i)
    {
        const auto& s = p.steps[(size_t) i];
        if (s.fx == Fx::Off && s.length == 1) continue;
        o << ' ' << i << ':' << (int) s.fx << ':' << s.amount << ':' << s.prob << ':' << s.length;
    }
    return o.str();
}

Pattern deserialize (const std::string& text)
{
    Pattern p;
    std::istringstream in (text);
    std::string tag;
    if (! (in >> tag) || tag != "v1") return p;
    int len = 16, rate = 2;
    float swing = 0;
    if (! (in >> len >> rate >> swing)) return p;
    p.length = std::max (1, std::min (kNumSteps, len));
    p.rate = (Rate) std::max (0, std::min (kNumRates - 1, rate));
    p.swing = clampf (swing, 0.0f, 1.0f);
    std::string tok;
    while (in >> tok)
    {
        int i = 0, fx = 0, l = 1;
        float a = 0.6f, pr = 1.0f;
        if (std::sscanf (tok.c_str(), "%d:%d:%f:%f:%d", &i, &fx, &a, &pr, &l) != 5) continue;
        if (i < 0 || i >= kNumSteps) continue;
        auto& s = p.steps[(size_t) i];
        s.fx = (Fx) std::max (0, std::min (kNumFx - 1, fx));
        s.amount = clampf (a, 0.0f, 1.0f);
        s.prob = clampf (pr, 0.0f, 1.0f);
        s.length = std::max (1, std::min (16, l));
    }
    return p;
}

void randomise (Pattern& p, uint32_t seed)
{
    Rng rng;
    rng.seed (seed * 2654435761u + 17u);
    p.clear();
    static const Fx pool[] = { Fx::Stutter, Fx::Stutter, Fx::Reverse, Fx::Bitcrush, Fx::LpSweep, Fx::HpSweep,
                               Fx::Gate, Fx::PitchDown, Fx::PanFlick, Fx::EchoThrow, Fx::TapeStop, Fx::TapeStart };
    const int n = p.length;
    int i = 0;
    while (i < n)
    {
        const float pos = (float) i / (float) std::max (1, n);
        const bool downbeat = (i % 4) == 0;
        const float chance = 0.18f + 0.45f * pos - (downbeat ? 0.12f : 0.0f);
        if (rng.uni() < chance)
        {
            auto& s = p.steps[(size_t) i];
            s.fx = pool[(int) (rng.uni() * 11.999f)];
            if ((s.fx == Fx::TapeStop) && pos < 0.6f) s.fx = Fx::Stutter;   // stops belong at the end
            s.amount = 0.25f + 0.7f * rng.uni();
            s.prob = rng.uni() < 0.25f ? 0.5f + 0.4f * rng.uni() : 1.0f;
            s.length = rng.uni() < 0.3f ? 2 : 1;
            if (s.fx == Fx::TapeStop) s.length = std::max (2, n - i);
            s.length = std::min (s.length, n - i);
            i += s.length;
        }
        else
            ++i;
    }
}

Pattern makePattern (const char* name, Rate rate, int length, float swing, const char* steps, const char* amounts, const char* probs)
{
    Pattern p;
    p.name = name;
    p.rate = rate;
    p.length = std::max (1, std::min (kNumSteps, length));
    p.swing = swing;
    int last = -1;
    const size_t na = std::strlen (amounts), np = std::strlen (probs);
    for (int i = 0; steps[i] != 0 && i < kNumSteps; ++i)
    {
        const char c = steps[i];
        if (c == '-')
        {
            if (last >= 0) p.steps[(size_t) last].length = std::min (16, i - last + 1);
            continue;
        }
        auto& s = p.steps[(size_t) i];
        s.fx = fxFromChar (c);
        if (s.fx == Fx::Off) { last = -1; continue; }
        last = i;
        if ((size_t) i < na && amounts[i] >= '0' && amounts[i] <= '9') s.amount = (float) (amounts[i] - '0' + 1) / 10.0f;
        if ((size_t) i < np && probs[i] >= '0' && probs[i] <= '9') s.prob = (float) (probs[i] - '0' + 1) / 10.0f;
    }
    return p;
}
} // namespace sg
