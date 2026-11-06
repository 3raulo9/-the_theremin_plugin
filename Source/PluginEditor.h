#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "gui/ThereminLookAndFeel.h"
#include "gui/ThereminView.h"

#include <memory>
#include <vector>

/**
    The editor is designed on a 1040 x 640 canvas. Everything lives inside one
    content component that is scaled to the window, so the window can be resized
    to any size and shape: the scale follows the tighter of the two dimensions
    and the layout stretches to use the spare space in the other one.
*/
class AetherThereminAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                 private juce::Timer
{
public:
    explicit AetherThereminAudioProcessorEditor (AetherThereminAudioProcessor&);
    ~AetherThereminAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Layout canvas size (scale 1.0). */
    static constexpr int designWidth  = 1040;
    static constexpr int designHeight = 640;

    /** Size of a freshly opened editor: 70 % of the design canvas. */
    static constexpr int defaultWidth  = designWidth  * 7 / 10;   // 728
    static constexpr int defaultHeight = designHeight * 7 / 10;   // 448

    /** Resize limits, as a fraction of the design canvas, per dimension. */
    static constexpr float minScale = 0.4f;
    static constexpr float maxScale = 5.0f;

    /** Current UI scale (window size relative to the design canvas). */
    float getUiScale() const noexcept { return uiScale; }

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Chooser
    {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };

    struct Section
    {
        juce::String title;
        std::vector<juce::Component*> cells;  // knobs (Knob::slider) or choosers
        int numCells = 0;
        juce::Rectangle<int> bounds;
    };

    /** Holds every control; laid out in design units and scaled by the editor. */
    struct Content final : public juce::Component
    {
        explicit Content (AetherThereminAudioProcessorEditor& e) : owner (e) { setOpaque (true); }
        void paint (juce::Graphics& g) override { owner.paintContent (g); }
        void resized() override { owner.layoutContent(); }

        AetherThereminAudioProcessorEditor& owner;
    };

    Knob& addKnob (const char* paramId, const juce::String& label, const juce::String& tooltip);
    Chooser& addChooser (const char* paramId, const juce::String& label, const juce::String& tooltip);
    void timerCallback() override;
    void paintContent (juce::Graphics&);
    void layoutContent();
    void rebuildBackground (int imageWidth, int imageHeight);
    void layoutSection (Section&, juce::Rectangle<int> area);

    AetherThereminAudioProcessor& processor;
    theremin::ThereminLookAndFeel lookAndFeel;
    Content content { *this };
    theremin::ThereminView view;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Chooser>> choosers;
    std::vector<Section> sections;

    juce::ComboBox presetBox;
    juce::Label presetLabel;
    juce::Image background;
    juce::TooltipWindow tooltipWindow { this, 700 };
    float uiScale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AetherThereminAudioProcessorEditor)
};
