#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace theremin
{
    namespace palette
    {
        inline const juce::Colour walnutDark  { 0xff1c120b };
        inline const juce::Colour walnut      { 0xff3b2516 };
        inline const juce::Colour walnutLight { 0xff6b4429 };
        inline const juce::Colour panel       { 0xff22170f };
        inline const juce::Colour brass       { 0xffc9a45c };
        inline const juce::Colour brassDark   { 0xff7d6131 };
        inline const juce::Colour cream       { 0xffefe3c8 };
        inline const juce::Colour creamDim    { 0xffb5a588 };
        inline const juce::Colour amber       { 0xffffb347 };
        inline const juce::Colour phosphor    { 0xff8dffc0 };
        inline const juce::Colour stage       { 0xff0d0f14 };
    }

    class ThereminLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        ThereminLookAndFeel();

        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                               juce::Slider&) override;

        juce::Label* createSliderTextBox (juce::Slider&) override;

        void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                           int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;

        juce::Font getComboBoxFont (juce::ComboBox&) override;
        juce::Font getPopupMenuFont() override;
        void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

        void drawCornerResizer (juce::Graphics&, int w, int h, bool isMouseOver, bool isMouseDragging) override;

        static juce::Font labelFont (float height, bool bold = false);
    };
}
