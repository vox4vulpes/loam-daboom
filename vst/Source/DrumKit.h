#pragma once

#include <array>
#include <cstdint>
#include <vector>

// Everything the web version (loam-daboom/index.html) defines as data: the
// eight voices and their knobs, the factory kits, and the pattern library.
// Kept free of JUCE GUI types so the engine test can use it too.
namespace daboom
{
    constexpr int numSlots = 8, numSteps = 16, numPatterns = 4, maxKnobs = 4;

    enum class Unit { hz, ms, pct, none };

    struct KnobDef
    {
        const char* id;
        const char* label;
        float min, max, def;
        Unit unit;
        bool log;
    };

    enum Type { kick, snare, clap, closedHat, openHat, tom, rim, cowbell };

    // One slot per voice: pad, computer key, General MIDI note and sequencer row.
    struct VoiceDef
    {
        const char* id;
        const char* name;
        char key;                    // computer key that plays it
        int gm;                      // General MIDI drum note
        std::uint32_t colour;        // ARGB, the web's --accent for the strip
        int numKnobs;                // the last one is always LEVEL
        std::array<KnobDef, maxKnobs> knobs;
    };

    inline const std::array<VoiceDef, numSlots>& voices()
    {
        constexpr std::uint32_t coral = 0xffb85540, mustard = 0xffc08f17, driveRed = 0xffb8322a,
                                teal = 0xff336f6b, lavender = 0xff8a78a3, sage = 0xff5f7344;
        constexpr KnobDef level { "level", "LEVEL", 0, 100, 80, Unit::pct, false };

        static const std::array<VoiceDef, numSlots> list {{
            { "kick",  "KICK",    'Z', 36, coral, 4, {{ { "tune", "TUNE", 30, 90, 50, Unit::hz, false },
                                                        { "decay", "DECAY", 80, 1500, 450, Unit::ms, true },
                                                        { "punch", "PUNCH", 0, 100, 50, Unit::pct, false }, level }} },
            { "snare", "SNARE",   'X', 38, mustard, 4, {{ { "tune", "TUNE", 120, 320, 190, Unit::hz, false },
                                                          { "decay", "DECAY", 60, 600, 200, Unit::ms, true },
                                                          { "snappy", "SNAPPY", 0, 100, 60, Unit::pct, false }, level }} },
            { "clap",  "CLAP",    'C', 39, driveRed, 4, {{ { "tone", "TONE", 600, 3000, 1200, Unit::hz, true },
                                                           { "decay", "DECAY", 60, 800, 250, Unit::ms, true },
                                                           { "spread", "SPREAD", 4, 25, 10, Unit::ms, false }, level }} },
            { "ch",    "C-HAT",   'V', 42, teal, 3, {{ { "tone", "TONE", 4000, 12000, 8000, Unit::hz, true },
                                                       { "decay", "DECAY", 20, 200, 60, Unit::ms, true }, level, {} }} },
            { "oh",    "O-HAT",   'B', 46, teal, 3, {{ { "tone", "TONE", 4000, 12000, 7500, Unit::hz, true },
                                                       { "decay", "DECAY", 100, 1200, 400, Unit::ms, true }, level, {} }} },
            { "tom",   "TOM",     'N', 45, lavender, 3, {{ { "tune", "TUNE", 60, 300, 120, Unit::hz, true },
                                                           { "decay", "DECAY", 80, 1200, 350, Unit::ms, true }, level, {} }} },
            { "rim",   "RIM",     'M', 37, sage, 3, {{ { "tune", "TUNE", 800, 3000, 1700, Unit::hz, true },
                                                       { "decay", "DECAY", 10, 120, 35, Unit::ms, true }, level, {} }} },
            { "cow",   "COWBELL", ',', 56, mustard, 3, {{ { "tune", "TUNE", 400, 900, 540, Unit::hz, true },
                                                          { "decay", "DECAY", 60, 900, 280, Unit::ms, true }, level, {} }} },
        }};
        return list;
    }

    // Other General MIDI drum notes that land on the nearest voice.
    inline int slotForNote (int note)
    {
        for (int i = 0; i < numSlots; ++i)
            if (voices()[(size_t) i].gm == note)
                return i;
        switch (note)
        {
            case 35: return kick;
            case 40: return snare;
            case 44: return closedHat;
            case 41: case 43: case 47: case 48: case 50: return tom;
            default: return -1;
        }
    }

    // ---- factory kits: 808 (the defaults above), 909, Lo-Fi
    constexpr int numFactoryKits = 3;
    inline const char* factoryKitName (int kit)
    {
        static const char* names[] = { "808", "909", "Lo-Fi" };
        return names[kit];
    }

    // A factory kit's value for one knob of one slot.
    inline float factoryValue (int slot, int knob, int kit)
    {
        // Overrides of TUNE/TONE, DECAY and the third knob; 0 means "keep the default".
        static const float over[numFactoryKits][numSlots][3] = {
            { {}, {}, {}, {}, {}, {}, {}, {} },
            { { 56, 300, 85 }, { 220, 160, 80 }, { 1500, 200, 8 }, { 9500, 45 }, { 9000, 300 }, { 140, 250 }, { 2000, 25 }, { 560, 200 } },
            { { 45, 700, 25 }, { 170, 260, 40 }, { 900, 350, 14 }, { 5500, 80 }, { 5000, 600 }, { 100, 450 }, { 1300, 50 }, { 480, 350 } },
        };
        const auto& v = voices()[(size_t) slot];
        const auto def = v.knobs[(size_t) knob].def;
        if (knob >= v.numKnobs - 1)                     // LEVEL is the same in every kit
            return def;
        const auto o = over[kit][slot][knob];
        return o != 0.0f ? o : def;
    }

    // ---- pattern library. Rows are '.' off, 'x' hit, 'X' accent; empty rows are silent.
    struct Groove
    {
        const char* name;
        std::array<const char*, numSlots> rows;   // in slot order
    };

    inline const std::vector<Groove>& factoryGrooves()
    {
        static const std::vector<Groove> list {
            //                kick                 snare                clap                 c-hat                o-hat                tom                  rim                  cowbell
            { "House",       { "X...x...X...x...", "",                  "....X.......X...", "x...x...x...x...", "..x...x...x...x.", "",                  "......x.......x.", "" } },
            { "Boom Bap",    { "X......x..x.....", "....X.......X...", "",                  "x.x.x.x.x.x.x.x.", "..............x.", "",                  "",                  "" } },
            { "Breakbeat",   { "X.x.......X.x...", "....X......x.X..", "",                  "x.x.x.x.x.x.x.x.", "",                  "...............x", "",                  "" } },
            { "Trap",        { "X......x..x.....", "",                  "........X.......", "x.x.xxx.x.x.x.xx", "",                  "",                  "...x.......x....", "..............x." } },
            { "Techno",      { "X...X...X...X...", "",                  "....X.......X...", "x.x.x.x.x.x.x.x.", "..x...x...x...x.", "",                  "",                  "" } },
            { "Garage",      { "X.........X.....", "....X.......X...", "",                  "..x...x.x.x...x.", "......x.......x.", "",                  "",                  "" } },
            { "Lazy Groove", { "X.....x...x..x..", "....X.......X..x", "",                  "x..xx.x.x..xx.x.", "",                  "",                  "..x.......x.....", "" } },
            { "Half-Time",   { "X.........x.....", "........X.......", "",                  "x.x.x.x.x.x.x.x.", "..............x.", "",                  "",                  "" } },
        };
        return list;
    }

    // Patterns A-D start as the first four grooves, as in the web version.
    inline int stepValue (const char* row, int step)
    {
        if (row == nullptr)
            return 0;
        for (int i = 0; i <= step; ++i)
            if (row[i] == 0)
                return 0;
        return row[step] == 'X' ? 2 : row[step] == 'x' ? 1 : 0;
    }
}
