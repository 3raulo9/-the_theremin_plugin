#pragma once

#include "Parameters.h"

#include <initializer_list>
#include <utility>

namespace theremin
{
    struct Preset
    {
        const char* name;
        std::initializer_list<std::pair<const char*, float>> values; // anything not listed = default
    };

    // Factory programs. Program 0 equals the parameter defaults.
    inline const std::initializer_list<Preset>& getFactoryPresets()
    {
        static const std::initializer_list<Preset> presets
        {
            { "Concert Theremin", {} },

            { "FL Slide Lead",
              { { ids::glide, 70.0f }, { ids::overshoot, 0.15f }, { ids::scoop, 0.0f },
                { ids::vibDepth, 9.0f }, { ids::vibDelay, 380.0f }, { ids::humanize, 0.15f },
                { ids::timbre, 0.55f }, { ids::attack, 12.0f }, { ids::release, 140.0f },
                { ids::reverbMix, 0.12f } } },

            { "Pure Sine",
              { { ids::timbre, 0.0f }, { ids::pull, 0.04f }, { ids::warmth, 0.05f },
                { ids::cabinet, 0.2f }, { ids::tone, 0.9f }, { ids::vibDepth, 14.0f },
                { ids::output, -2.7f } } },

            { "1950s Sci-Fi",
              { { ids::glide, 170.0f }, { ids::glideMode, 1.0f }, { ids::vibDepth, 48.0f },
                { ids::vibRate, 6.8f }, { ids::vibDelay, 0.0f }, { ids::humanize, 0.55f },
                { ids::timbre, 0.62f }, { ids::pull, 0.45f }, { ids::reverbMix, 0.42f },
                { ids::roomSize, 0.88f }, { ids::attack, 90.0f }, { ids::release, 600.0f },
                { ids::output, -4.8f } } },

            { "Bright Reed",
              { { ids::timbre, 0.9f }, { ids::pull, 0.42f }, { ids::warmth, 0.45f },
                { ids::tone, 0.84f }, { ids::cabinet, 0.72f } } },

            { "Vintage Radio",
              { { ids::noise, 0.5f }, { ids::warmth, 0.72f }, { ids::tone, 0.45f },
                { ids::cabinet, 1.0f }, { ids::timbre, 0.5f }, { ids::reverbMix, 0.1f },
                { ids::humanize, 0.6f }, { ids::output, -6.8f } } },

            { "Ghost Choir",
              { { ids::timbre, 0.25f }, { ids::vibDepth, 30.0f }, { ids::vibRate, 5.0f },
                { ids::attack, 420.0f }, { ids::release, 1600.0f }, { ids::reverbMix, 0.6f },
                { ids::roomSize, 0.95f }, { ids::glide, 260.0f }, { ids::output, -6.0f } } },

            { "Deep Bass Theremin",
              { { ids::octave, -1.0f }, { ids::timbre, 0.7f }, { ids::pull, 0.7f },
                { ids::warmth, 0.5f }, { ids::tone, 0.55f }, { ids::vibDepth, 10.0f },
                { ids::reverbMix, 0.1f } } },

            { "808 Glide Theremin",
              { { ids::octave, -2.0f }, { ids::glide, 110.0f }, { ids::scoop, 0.0f },
                { ids::vibDepth, 0.0f }, { ids::humanize, 0.05f }, { ids::timbre, 0.3f },
                { ids::pull, 0.5f }, { ids::warmth, 0.65f }, { ids::cabinet, 0.15f },
                { ids::attack, 4.0f }, { ids::release, 90.0f }, { ids::velSens, 0.2f },
                { ids::reverbMix, 0.0f } } },
        };

        return presets;
    }
}
