#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

class AetherThereminAudioProcessor;

namespace theremin
{
    /**
        The instrument itself: a drawing of the cabinet, pitch rod and volume loop
        that you can play with the mouse (horizontal distance to the rod = pitch,
        height = volume), a tuner needle and a phosphor oscilloscope.
    */
    class ThereminView final : public juce::Component
    {
    public:
        explicit ThereminView (AetherThereminAudioProcessor&);

        void paint (juce::Graphics&) override;
        void resized() override;

        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;

        /** Called by the editor's timer (~30 Hz). */
        void refresh();

        static constexpr float lowPitch  = 36.0f;  // C2 at the far edge
        static constexpr float highPitch = 84.0f;  // C6 right next to the rod

    private:
        void playFromMouse (juce::Point<float> position);
        float pitchForX (float x) const noexcept;
        float xForPitch (float pitch) const noexcept;

        void drawStage (juce::Graphics&);
        void drawInstrument (juce::Graphics&);
        void drawHands (juce::Graphics&);
        void drawTuner (juce::Graphics&, juce::Rectangle<float>);
        void drawScope (juce::Graphics&, juce::Rectangle<float>);

        AetherThereminAudioProcessor& processor;

        juce::Rectangle<float> stageArea, tunerArea, scopeArea, cabinet;
        float rodX = 0.0f, rodTop = 0.0f, rodBottom = 0.0f;
        float playLeft = 0.0f, playRight = 0.0f;
        juce::Point<float> loopCentre;
        float loopRadiusX = 0.0f, loopRadiusY = 0.0f;

        float shownHandPitch = 60.0f, shownFullPitch = 60.0f, shownLevel = 0.0f, shownFreq = 261.6f;
        float shownBendRange = 12.0f;
        float animPhase = 0.0f;
        bool mousePlaying = false, hovering = false;

        static constexpr int scopeSamples = 2048;
        std::array<float, scopeSamples> scopeData {};
        juce::Path scopePath;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ThereminView)
    };
}
