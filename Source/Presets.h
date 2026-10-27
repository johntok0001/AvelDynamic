#pragma once

#include "Params.h"

namespace avel::presets
{
    struct Preset
    {
        const char* category;
        const char* name;
        std::vector<std::pair<const char*, float>> values; // real (denormalised) values; choices by index
    };

    namespace p = avel::params::id;

    inline const std::vector<Preset>& factory()
    {
        static const std::vector<Preset> list {
            { "General", "Default", {} },
            { "General", "Gentle Leveler",   { { p::style, 2 }, { p::threshold, -22 }, { p::ratio, 2.5f }, { p::knee, 12 }, { p::attack, 25 }, { p::release, 300 }, { p::autoGain, 1 } } },
            { "General", "Transparent Peak Control", { { p::style, 0 }, { p::threshold, -10 }, { p::ratio, 8 }, { p::knee, 4 }, { p::attack, 0.5f }, { p::release, 60 }, { p::lookahead, 2 } } },
            { "General", "Brickwall-ish",    { { p::style, 0 }, { p::threshold, -6 }, { p::ratio, 100 }, { p::knee, 0 }, { p::attack, 0.005f }, { p::release, 40 }, { p::lookahead, 5 }, { p::oversampling, 1 } } },

            { "Vocals", "Vocal Rider",       { { p::style, 3 }, { p::threshold, -24 }, { p::ratio, 3 }, { p::knee, 6 }, { p::attack, 6 }, { p::release, 140 }, { p::autoRelease, 1 }, { p::scHpf, 100 }, { p::autoGain, 1 } } },
            { "Vocals", "Vocal Upfront",     { { p::style, 3 }, { p::threshold, -28 }, { p::ratio, 5 }, { p::knee, 4 }, { p::attack, 2.5f }, { p::release, 80 }, { p::character, 1 }, { p::drive, 25 }, { p::scHpf, 120 }, { p::autoGain, 1 } } },
            { "Vocals", "Smooth Opto Vocal", { { p::style, 2 }, { p::threshold, -26 }, { p::ratio, 4 }, { p::knee, 6 }, { p::attack, 10 }, { p::release, 250 }, { p::scHpf, 90 }, { p::autoGain, 1 } } },
            { "Vocals", "De-Harsh",          { { p::style, 0 }, { p::threshold, -30 }, { p::ratio, 4 }, { p::knee, 6 }, { p::attack, 1 }, { p::release, 50 }, { p::range, 6 }, { p::scHpf, 2000 }, { p::scLpf, 9000 } } },

            { "Drums", "Drum Bus Glue",      { { p::style, 5 }, { p::threshold, -16 }, { p::ratio, 4 }, { p::knee, 6 }, { p::attack, 10 }, { p::release, 100 }, { p::autoRelease, 1 }, { p::scHpf, 60 }, { p::autoGain, 1 } } },
            { "Drums", "Drum Smash Parallel",{ { p::style, 6 }, { p::threshold, -32 }, { p::ratio, 10 }, { p::knee, 2 }, { p::attack, 3 }, { p::release, 60 }, { p::character, 1 }, { p::drive, 45 }, { p::mix, 35 }, { p::autoGain, 1 } } },
            { "Drums", "Kick Punch",         { { p::style, 6 }, { p::threshold, -18 }, { p::ratio, 4 }, { p::knee, 3 }, { p::attack, 18 }, { p::release, 80 }, { p::hold, 10 } } },
            { "Drums", "Snare Crack",        { { p::style, 6 }, { p::threshold, -22 }, { p::ratio, 6 }, { p::knee, 3 }, { p::attack, 8 }, { p::release, 70 }, { p::character, 2 }, { p::drive, 25 } } },
            { "Drums", "Room Squash",        { { p::style, 1 }, { p::threshold, -30 }, { p::ratio, 20 }, { p::knee, 0 }, { p::attack, 0.3f }, { p::release, 200 }, { p::character, 1 }, { p::drive, 60 }, { p::autoGain, 1 } } },

            { "Instruments", "Bass Control", { { p::style, 2 }, { p::threshold, -20 }, { p::ratio, 4 }, { p::knee, 6 }, { p::attack, 15 }, { p::release, 180 }, { p::character, 1 }, { p::drive, 20 }, { p::autoGain, 1 } } },
            { "Instruments", "Acoustic Guitar", { { p::style, 1 }, { p::threshold, -20 }, { p::ratio, 3 }, { p::knee, 8 }, { p::attack, 12 }, { p::release, 150 }, { p::autoRelease, 1 }, { p::autoGain, 1 } } },
            { "Instruments", "Piano Smooth", { { p::style, 4 }, { p::threshold, -22 }, { p::ratio, 2 }, { p::knee, 12 }, { p::attack, 20 }, { p::release, 300 }, { p::autoRelease, 1 }, { p::autoGain, 1 } } },
            { "Instruments", "Synth Pump",   { { p::style, 7 }, { p::threshold, -30 }, { p::ratio, 8 }, { p::knee, 0 }, { p::attack, 1 }, { p::release, 220 }, { p::scExternal, 1 } } },

            { "Mix Bus", "Mix Bus Glue",     { { p::style, 5 }, { p::threshold, -14 }, { p::ratio, 2 }, { p::knee, 6 }, { p::attack, 30 }, { p::release, 200 }, { p::autoRelease, 1 }, { p::scHpf, 80 }, { p::autoGain, 1 } } },
            { "Mix Bus", "Mastering Gentle", { { p::style, 4 }, { p::threshold, -12 }, { p::ratio, 1.5f }, { p::knee, 12 }, { p::attack, 30 }, { p::release, 250 }, { p::autoRelease, 1 }, { p::scHpf, 60 }, { p::oversampling, 1 }, { p::autoGain, 1 } } },
            { "Mix Bus", "Mastering Density",{ { p::style, 4 }, { p::threshold, -16 }, { p::ratio, 2.5f }, { p::knee, 8 }, { p::attack, 15 }, { p::release, 150 }, { p::autoRelease, 1 }, { p::character, 1 }, { p::drive, 15 }, { p::oversampling, 2 }, { p::autoGain, 1 } } },
            { "Mix Bus", "Mid Focus (M/S)",  { { p::style, 4 }, { p::threshold, -16 }, { p::ratio, 2 }, { p::knee, 8 }, { p::attack, 20 }, { p::release, 200 }, { p::channelMode, 2 }, { p::autoGain, 1 } } },
            { "Mix Bus", "Wide Side Tamer",  { { p::style, 0 }, { p::threshold, -24 }, { p::ratio, 3 }, { p::knee, 6 }, { p::attack, 10 }, { p::release, 150 }, { p::channelMode, 3 }, { p::scHpf, 300 } } },
        };
        return list;
    }
}
