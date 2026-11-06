#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// All parameter IDs live here so the processor, editor, presets and tests agree.
namespace theremin::ids
{
    // Pitch hand
    inline constexpr const char* glide       = "glide";
    inline constexpr const char* glideMode   = "glideMode";
    inline constexpr const char* noteRelease = "noteRelease";
    inline constexpr const char* overshoot   = "overshoot";
    inline constexpr const char* scoop       = "scoop";
    inline constexpr const char* bendRange   = "bendRange";
    inline constexpr const char* octave      = "octave";
    inline constexpr const char* fineTune    = "fineTune";

    // Vibrato / human hand
    inline constexpr const char* vibDepth    = "vibDepth";
    inline constexpr const char* vibRate     = "vibRate";
    inline constexpr const char* vibDelay    = "vibDelay";
    inline constexpr const char* humanize    = "humanize";

    // Heterodyne tone
    inline constexpr const char* timbre      = "timbre";
    inline constexpr const char* pull        = "pull";
    inline constexpr const char* warmth      = "warmth";
    inline constexpr const char* tone        = "tone";
    inline constexpr const char* cabinet     = "cabinet";
    inline constexpr const char* noise       = "noise";

    // Volume hand
    inline constexpr const char* attack      = "attack";
    inline constexpr const char* release     = "release";
    inline constexpr const char* velSens     = "velSens";

    // Space / output
    inline constexpr const char* reverbMix   = "reverbMix";
    inline constexpr const char* roomSize    = "roomSize";
    inline constexpr const char* output      = "output";
}

namespace theremin
{
    inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        using namespace juce;
        using Float  = AudioParameterFloat;
        using Choice = AudioParameterChoice;
        using Int    = AudioParameterInt;

        auto pct = [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; };
        auto ms  = [] (float v, int) { return v < 1000.0f ? String (roundToInt (v)) + " ms"
                                                           : String (v / 1000.0f, 2) + " s"; };

        // typed-in values: "45 %" -> 0.45, "1.2 s" -> 1200 ms, "+/-12 st" -> 12
        auto pctIn = [] (const String& t) { return t.retainCharacters ("0123456789.-").getFloatValue() / 100.0f; };
        auto msIn  = [] (const String& t)
        {
            const auto v = t.retainCharacters ("0123456789.").getFloatValue();
            const auto lower = t.trim().toLowerCase();
            return lower.endsWith ("s") && ! lower.endsWith ("ms") ? v * 1000.0f : v;
        };
        auto numIn = [] (const String& t) { return t.retainCharacters ("0123456789.-").getFloatValue(); };

        AudioProcessorValueTreeState::ParameterLayout layout;

        // --- Pitch hand -----------------------------------------------------
        layout.add (std::make_unique<Float> (ParameterID { ids::glide, 1 }, "Glide",
                                             NormalisableRange<float> (0.0f, 2000.0f, 0.0f, 0.35f), 90.0f,
                                             AudioParameterFloatAttributes().withStringFromValueFunction (ms).withValueFromStringFunction (msIn)));

        layout.add (std::make_unique<Choice> (ParameterID { ids::glideMode, 1 }, "Glide Mode",
                                              StringArray { "Legato (overlapping notes)", "Always" }, 0));

        layout.add (std::make_unique<Choice> (ParameterID { ids::noteRelease, 1 }, "Note Release",
                                              StringArray { "Stay (FL slide style)", "Return to held note" }, 0));

        layout.add (std::make_unique<Float> (ParameterID { ids::overshoot, 1 }, "Hand Overshoot",
                                             NormalisableRange<float> (0.0f, 1.0f), 0.3f,
                                             AudioParameterFloatAttributes().withStringFromValueFunction (pct).withValueFromStringFunction (pctIn)));

        layout.add (std::make_unique<Float> (ParameterID { ids::scoop, 1 }, "Scoop",
                                             NormalisableRange<float> (0.0f, 1.0f), 0.2f,
                                             AudioParameterFloatAttributes().withStringFromValueFunction (pct).withValueFromStringFunction (pctIn)));

        layout.add (std::make_unique<Float> (ParameterID { ids::bendRange, 1 }, "Pitch Bend Range",
                                             NormalisableRange<float> (1.0f, 48.0f, 1.0f), 12.0f,
                                             AudioParameterFloatAttributes().withStringFromValueFunction (
                                                 [] (float v, int) { return "+/-" + String (roundToInt (v)) + " st"; })
                                                 .withValueFromStringFunction ([] (const String& t) { return t.retainCharacters ("0123456789.").getFloatValue(); })));

        layout.add (std::make_unique<Int> (ParameterID { ids::octave, 1 }, "Octave", -3, 3, 0,
                                           AudioParameterIntAttributes().withStringFromValueFunction (
                                               [] (int v, int) { return v > 0 ? "+" + String (v) : String (v); })));

        layout.add (std::make_unique<Float> (ParameterID { ids::fineTune, 1 }, "Fine Tune",
                                             NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f,
                                             AudioParameterFloatAttributes().withStringFromValueFunction (
                                                 [] (float v, int) { return (v > 0.0f ? "+" : "") + String (v, 1) + " ct"; })
                                                 .withValueFromStringFunction (numIn)));

        // --- Vibrato ---------------------------------------------------------
        layout.add (std::make_unique<Float> (ParameterID { ids::vibDepth, 1 }, "Vibrato Depth",
                                             NormalisableRange<float> (0.0f, 100.0f, 0.0f, 0.6f), 18.0f,
                                             AudioParameterFloatAttributes().withStringFromValueFunction (
                                                 [] (float v, int) { return String (v, v < 10.0f ? 1 : 0) + " ct"; })
                                                 .withValueFromStringFunction (numIn)));

        layout.add (std::make_unique<Float> (ParameterID { ids::vibRate, 1 }, "Vibrato Rate",
                                             NormalisableRange<float> (1.0f, 10.0f, 0.0f, 0.8f), 5.6f,
                                             AudioParameterFloatAttributes().withStringFromValueFunction (
                                                 [] (float v, int) { return String (v, 2) + " Hz"; })
                                                 .withValueFromStringFunction (numIn)));

        layout.add (std::make_unique<Float> (ParameterID { ids::vibDelay, 1 }, "Vibrato Delay",
                                             NormalisableRange<float> (0.0f, 2000.0f, 0.0f, 0.5f), 220.0f,
                                             AudioParameterFloatAttributes().withStringFromValueFunction (ms).withValueFromStringFunction (msIn)));

        layout.add (std::make_unique<Float> (ParameterID { ids::humanize, 1 }, "Humanize",
                                             NormalisableRange<float> (0.0f, 1.0f), 0.35f,
                                             AudioParameterFloatAttributes().withStringFromValueFunction (pct).withValueFromStringFunction (pctIn)));

        // --- Heterodyne tone -------------------------------------------------
        layout.add (std::make_unique<Float> (ParameterID { ids::timbre, 1 }, "Timbre",
                                             NormalisableRange<float> (0.0f, 1.0f), 0.45f,
                                             AudioParameterFloatAttributes().withStringFromValueFunction (pct).withValueFromStringFunction (pctIn)));

        layout.add (std::make_unique<Float> (ParameterID { ids::pull, 1 }, "Oscillator Pull",
                                             NormalisableRange<float> (0.0f, 1.0f), 0.3f,
                                             AudioParameterFloatAttributes().withStringFromValueFunction (pct).withValueFromStringFunction (pctIn)));

        layout.add (std::make_unique<Float> (ParameterID { ids::warmth, 1 }, "Tube Warmth",
                                             NormalisableRange<float> (0.0f, 1.0f), 0.3f,
                                             AudioParameterFloatAttributes().withStringFromValueFunction (pct).withValueFromStringFunction (pctIn)));

        layout.add (std::make_unique<Float> (ParameterID { ids::tone, 1 }, "Tone",
                                             NormalisableRange<float> (0.0f, 1.0f), 0.7f,
                                             AudioParameterFloatAttributes().withStringFromValueFunction (pct).withValueFromStringFunction (pctIn)));

        layout.add (std::make_unique<Float> (ParameterID { ids::cabinet, 1 }, "Cabinet",
                                             NormalisableRange<float> (0.0f, 1.0f), 0.6f,
                                             AudioParameterFloatAttributes().withStringFromValueFunction (pct).withValueFromStringFunction (pctIn)));

        layout.add (std::make_unique<Float> (ParameterID { ids::noise, 1 }, "Vintage Hum",
                                             NormalisableRange<float> (0.0f, 1.0f), 0.0f,
                                             AudioParameterFloatAttributes().withStringFromValueFunction (pct).withValueFromStringFunction (pctIn)));

        // --- Volume hand -----------------------------------------------------
        layout.add (std::make_unique<Float> (ParameterID { ids::attack, 1 }, "Attack",
                                             NormalisableRange<float> (1.0f, 2000.0f, 0.0f, 0.35f), 35.0f,
                                             AudioParameterFloatAttributes().withStringFromValueFunction (ms).withValueFromStringFunction (msIn)));

        layout.add (std::make_unique<Float> (ParameterID { ids::release, 1 }, "Release",
                                             NormalisableRange<float> (5.0f, 4000.0f, 0.0f, 0.35f), 220.0f,
                                             AudioParameterFloatAttributes().withStringFromValueFunction (ms).withValueFromStringFunction (msIn)));

        layout.add (std::make_unique<Float> (ParameterID { ids::velSens, 1 }, "Velocity Sensitivity",
                                             NormalisableRange<float> (0.0f, 1.0f), 0.4f,
                                             AudioParameterFloatAttributes().withStringFromValueFunction (pct).withValueFromStringFunction (pctIn)));

        // --- Space / output --------------------------------------------------
        layout.add (std::make_unique<Float> (ParameterID { ids::reverbMix, 1 }, "Reverb",
                                             NormalisableRange<float> (0.0f, 1.0f), 0.18f,
                                             AudioParameterFloatAttributes().withStringFromValueFunction (pct).withValueFromStringFunction (pctIn)));

        layout.add (std::make_unique<Float> (ParameterID { ids::roomSize, 1 }, "Room Size",
                                             NormalisableRange<float> (0.0f, 1.0f), 0.62f,
                                             AudioParameterFloatAttributes().withStringFromValueFunction (pct).withValueFromStringFunction (pctIn)));

        layout.add (std::make_unique<Float> (ParameterID { ids::output, 1 }, "Output",
                                             NormalisableRange<float> (-36.0f, 6.0f, 0.1f), -4.0f,
                                             AudioParameterFloatAttributes().withStringFromValueFunction (
                                                 [] (float v, int) { return String (v, 1) + " dB"; })
                                                 .withValueFromStringFunction (numIn)));

        return layout;
    }
}
