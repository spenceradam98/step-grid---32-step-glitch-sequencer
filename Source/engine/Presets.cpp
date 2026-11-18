// Factory patterns. Each loads into the selected slot. Step letters (see Pattern.h):
//  . off  s stutter  t tape stop  u tape start  r reverse  b bitcrush  l LP sweep  h HP sweep
//  g gate  p pitch down  f pan flick  e echo throw  - tie to the previous step
// The amount and probability rows are digits d -> (d + 1) / 10, aligned with the steps.
#include "Pattern.h"

namespace sg
{
const std::vector<FactoryPattern>& factoryPatterns()
{
    using R = Rate;
    static const std::vector<FactoryPattern> list = {
        // ---- Fills (end-of-bar ideas, mostly dry at the start)
        { "Fills", makePattern ("Trap Fill", R::Sixteenth, 16, 0.0f,
                                "........s-.ss-ss", "        5 88 99 9") },
        { "Fills", makePattern ("Stutter Fill", R::Sixteenth, 16, 0.0f,
                                "............s-s-", "            3 6 ") },
        { "Fills", makePattern ("Snare Roll Fill", R::Sixteenth, 16, 0.0f,
                                "........s-s-ssss", "        4 6 7799") },
        { "Fills", makePattern ("Triplet Roll", R::SixteenthT, 24, 0.0f,
                                "............ssssss-sss--", "            556677 899  ") },
        { "Fills", makePattern ("Reverse Turnaround", R::Sixteenth, 16, 0.0f,
                                "............r---", "            9   ") },
        { "Fills", makePattern ("Crunch Fill", R::Sixteenth, 16, 0.0f,
                                "..........b.b-bb", "          5 7 89") },
        { "Fills", makePattern ("Pitch Dive Fill", R::Sixteenth, 16, 0.0f,
                                "............p-p-", "            4 9 ") },
        { "Fills", makePattern ("Half-Bar Fill", R::Sixteenth, 8, 0.0f,
                                "....s-ss", "    4 79") },

        // ---- Endings
        { "Endings", makePattern ("Tape-Stop Ending", R::Sixteenth, 16, 0.0f,
                                  "........t-------", "        4       ") },
        { "Endings", makePattern ("Slow Tape Stop", R::Eighth, 16, 0.0f,
                                  "........t-------", "        1       ") },
        { "Endings", makePattern ("DJ Echo Out", R::Sixteenth, 16, 0.0f,
                                  "............e---", "            8   ") },
        { "Endings", makePattern ("Filter Out", R::Sixteenth, 32, 0.0f,
                                  "................l---------------", "                9               ") },
        { "Endings", makePattern ("Reverse Exit", R::Sixteenth, 16, 0.0f,
                                  "........r-------", "        9       ") },
        { "Endings", makePattern ("Spin Back", R::Sixteenth, 16, 0.0f,
                                  "............r-t-", "            9 7 ") },

        // ---- Builds
        { "Builds", makePattern ("Stutter Build", R::Sixteenth, 32, 0.0f,
                                 "s---s---s-s-s-s-ssssssssssssssss", "1   1   3 3 3 3 6666666688888888") },
        { "Builds", makePattern ("HP Riser", R::Sixteenth, 32, 0.0f,
                                 "h-------------------------------", "8                               ") },
        { "Builds", makePattern ("Gate Build", R::Sixteenth, 32, 0.0f,
                                 "................g---g---g-g-gggg", "                3   5   7 7 9999") },
        { "Builds", makePattern ("Crush Build", R::Eighth, 16, 0.0f,
                                 "........b-b-bbbb", "        2 4 6789") },
        { "Builds", makePattern ("Snare Roll Riser", R::Sixteenth, 32, 0.0f,
                                 "s-s-s-s-s-s-s-s-ssssssssssssssss", "2 2 2 2 4 4 4 4 6666666699999999") },

        // ---- Breakdowns
        { "Breakdowns", makePattern ("Breakdown Filter", R::Sixteenth, 16, 0.0f,
                                     "l---------------", "6               ") },
        { "Breakdowns", makePattern ("Underwater", R::Eighth, 8, 0.0f,
                                     "l-------", "9       ") },
        { "Breakdowns", makePattern ("Telephone", R::Sixteenth, 16, 0.0f,
                                     "h---------------", "9               ") },
        { "Breakdowns", makePattern ("Half Speed Dream", R::Sixteenth, 16, 0.0f,
                                     "p-------p-------", "9       9       ") },

        // ---- Glitch
        { "Glitch", makePattern ("Glitch Hop", R::Sixteenth, 16, 0.25f,
                                 "..s...b...s.r-.s", "  5   6   8 9  9") },
        { "Glitch", makePattern ("Random Crunch", R::Sixteenth, 16, 0.0f,
                                 ".b..b.b..b.bb.b.", " 5  8 3  9 76 4 ", " 6  4 5  6 45 7 ") },
        { "Glitch", makePattern ("Broken Machine", R::ThirtySecond, 32, 0.0f,
                                 "s...b..r-..s.s..g...b..s.t-..ss.", "9   7  9   9 8  4   9  9 6   99 ") },
        { "Glitch", makePattern ("Data Bend", R::Sixteenth, 16, 0.0f,
                                 "b.s.p.b.r.s.b.p.", "9 6 3 5 9 8 7 9 ", "7 6 5 6 5 6 5 6 ") },
        { "Glitch", makePattern ("Chance Stutter", R::Sixteenth, 16, 0.0f,
                                 "..s...s...s...ss", "  6   8   6   89", "  4   3   4   55") },
        { "Glitch", makePattern ("Micro Cuts", R::ThirtySecond, 32, 0.0f,
                                 "s.s...s.s.s...s.s.s...s.s.s.ssss", "9 9   9 9 9   9 9 9   9 9 9 9999") },
        { "Glitch", makePattern ("Reverse Hits", R::Eighth, 8, 0.0f,
                                 "..r...r-", "  9   9 ") },

        // ---- Grooves (good to leave running)
        { "Grooves", makePattern ("Offbeat Gate", R::Sixteenth, 16, 0.0f,
                                  "..g...g...g...g.", "  6   6   6   6 ") },
        { "Grooves", makePattern ("Pan Flicks", R::Sixteenth, 16, 0.0f,
                                  "..f...f...f.f.f.", "  8   8   8 6 9 ") },
        { "Grooves", makePattern ("Dub Throws", R::Sixteenth, 16, 0.0f,
                                  "......e.......e.", "      6       8 ") },
        { "Grooves", makePattern ("Swing Chops", R::Sixteenth, 16, 0.6f,
                                  "g.g.g.g.s-g.g.s-", "5 5 5 5 4 5 5 7 ") },
        { "Grooves", makePattern ("Shuffle Stutter", R::Sixteenth, 16, 0.5f,
                                  "...s...s...s..ss", "   4   4   6  79") },
        { "Grooves", makePattern ("Triplet Pans", R::EighthT, 12, 0.0f,
                                  "f.ff.ff.ff.f", "9 99 99 99 9") },
        { "Grooves", makePattern ("Sweep Pump", R::Eighth, 8, 0.0f,
                                  "h-h-h-h-", "5 5 5 5 ") },

        // ---- Transitions
        { "Transitions", makePattern ("Spin Up Drop", R::Sixteenth, 16, 0.0f,
                                      "u---............", "6               ") },
        { "Transitions", makePattern ("Stop And Start", R::Sixteenth, 32, 0.0f,
                                      "........................t---u---", "                        7   7   ") },
        { "Transitions", makePattern ("Rewind", R::Sixteenth, 16, 0.0f,
                                      "........r-r-r-rr", "        9 9 9 99") },
        { "Transitions", makePattern ("Drop Prep", R::Sixteenth, 32, 0.0f,
                                      "h---------------ssssssssss..t---", "7               6666888899  4   ") },
    };
    return list;
}
} // namespace sg
