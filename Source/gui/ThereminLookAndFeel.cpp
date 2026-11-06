#include "ThereminLookAndFeel.h"

namespace theremin
{

ThereminLookAndFeel::ThereminLookAndFeel()
{
    using namespace palette;

    setColour (juce::Slider::textBoxTextColourId,       cream);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId,  brass.withAlpha (0.4f));
    setColour (juce::Slider::rotarySliderFillColourId,  amber);
    setColour (juce::Slider::rotarySliderOutlineColourId, walnutDark);

    setColour (juce::Label::textColourId, cream);

    setColour (juce::ComboBox::backgroundColourId, panel);
    setColour (juce::ComboBox::outlineColourId,    brassDark);
    setColour (juce::ComboBox::textColourId,       cream);
    setColour (juce::ComboBox::arrowColourId,      brass);

    setColour (juce::PopupMenu::backgroundColourId,            walnutDark);
    setColour (juce::PopupMenu::textColourId,                  cream);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, brassDark);
    setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);

    setColour (juce::TextEditor::backgroundColourId, panel);
    setColour (juce::TextEditor::textColourId, cream);
    setColour (juce::TextEditor::highlightColourId, brass.withAlpha (0.4f));
    setColour (juce::CaretComponent::caretColourId, amber);

    setColour (juce::TooltipWindow::backgroundColourId, walnutDark);
    setColour (juce::TooltipWindow::textColourId, cream);
    setColour (juce::TooltipWindow::outlineColourId, brassDark);
}

juce::Font ThereminLookAndFeel::labelFont (float height, bool bold)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

void ThereminLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float startAngle, float endAngle,
                                            juce::Slider& slider)
{
    using namespace palette;

    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();
    const auto outerR = size * 0.5f - 2.0f;
    const auto knobR = outerR * 0.72f;
    const auto angle = startAngle + sliderPos * (endAngle - startAngle);
    const bool enabled = slider.isEnabled();

    // tick marks
    for (int i = 0; i <= 10; ++i)
    {
        const auto a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const auto r1 = outerR - 1.0f, r2 = outerR - (i % 5 == 0 ? 6.0f : 4.0f);
        const juce::Point<float> p1 (centre.x + r1 * std::sin (a), centre.y - r1 * std::cos (a));
        const juce::Point<float> p2 (centre.x + r2 * std::sin (a), centre.y - r2 * std::cos (a));
        g.setColour (creamDim.withAlpha (0.55f));
        g.drawLine ({ p1, p2 }, i % 5 == 0 ? 1.4f : 1.0f);
    }

    // value arc
    {
        const auto arcR = outerR - 8.0f;
        juce::Path track, value;
        track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.strokePath (track, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
        const auto zeroPos = bipolar ? (float) slider.valueToProportionOfLength (0.0) : 0.0f;
        const auto zeroAngle = startAngle + zeroPos * (endAngle - startAngle);

        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, juce::jmin (zeroAngle, angle), juce::jmax (zeroAngle, angle), true);
        g.setColour ((enabled ? amber : creamDim).withAlpha (0.9f));
        g.strokePath (value, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // drop shadow
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (centre.x - knobR + 1.5f, centre.y - knobR + 3.0f, knobR * 2.0f, knobR * 2.0f);

    // brass skirt
    {
        juce::ColourGradient skirt (brass.brighter (0.35f), centre.x - knobR, centre.y - knobR,
                                    brassDark.darker (0.3f), centre.x + knobR, centre.y + knobR, false);
        g.setGradientFill (skirt);
        g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);
    }

    // bakelite cap
    {
        const auto capR = knobR * 0.8f;
        juce::ColourGradient cap (juce::Colour (0xff5a3a26), centre.x - capR * 0.5f, centre.y - capR * 0.6f,
                                  juce::Colour (0xff150c07), centre.x + capR * 0.6f, centre.y + capR, true);
        g.setGradientFill (cap);
        g.fillEllipse (centre.x - capR, centre.y - capR, capR * 2.0f, capR * 2.0f);

        // glossy highlight
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillEllipse (centre.x - capR * 0.65f, centre.y - capR * 0.85f, capR * 1.1f, capR * 0.7f);

        // pointer
        const juce::Point<float> tip (centre.x + capR * 0.88f * std::sin (angle), centre.y - capR * 0.88f * std::cos (angle));
        const juce::Point<float> tail (centre.x + capR * 0.25f * std::sin (angle), centre.y - capR * 0.25f * std::cos (angle));
        g.setColour (enabled ? cream : creamDim);
        g.drawLine ({ tail, tip }, 2.2f);
    }
}

juce::Label* ThereminLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (labelFont (12.0f));
    label->setColour (juce::Label::textColourId, palette::creamDim);
    label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::textWhenEditingColourId, palette::cream);
    label->setColour (juce::Label::backgroundWhenEditingColourId, palette::panel);
    label->setColour (juce::Label::outlineWhenEditingColourId, palette::brassDark);
    return label;
}

void ThereminLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                        int, int, int, int, juce::ComboBox& box)
{
    using namespace palette;
    const auto r = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);

    g.setColour (panel.darker (0.3f));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.hasKeyboardFocus (true) ? brass : brassDark);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    const auto arrowZone = juce::Rectangle<float> ((float) width - 20.0f, 0.0f, 16.0f, (float) height);
    juce::Path arrow;
    arrow.addTriangle (arrowZone.getCentreX() - 4.0f, arrowZone.getCentreY() - 2.0f,
                       arrowZone.getCentreX() + 4.0f, arrowZone.getCentreY() - 2.0f,
                       arrowZone.getCentreX(),        arrowZone.getCentreY() + 3.0f);
    g.setColour (brass);
    g.fillPath (arrow);
}

juce::Font ThereminLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return labelFont (13.0f);
}

juce::Font ThereminLookAndFeel::getPopupMenuFont()
{
    return labelFont (14.0f);
}

void ThereminLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setMinimumHorizontalScale (0.7f);
}

void ThereminLookAndFeel::drawCornerResizer (juce::Graphics& g, int w, int h, bool isMouseOver, bool isMouseDragging)
{
    // three engraved brass grip lines in the bottom-right corner
    const auto size = (float) juce::jmin (w, h);
    const auto colour = palette::brass.withAlpha (isMouseDragging ? 1.0f : (isMouseOver ? 0.85f : 0.45f));

    for (int i = 1; i <= 3; ++i)
    {
        const auto d = size * 0.28f * (float) i;
        const auto x1 = (float) w - d, y1 = (float) h - 2.0f;
        const auto x2 = (float) w - 2.0f, y2 = (float) h - d;
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawLine (x1 + 1.0f, y1 + 1.0f, x2 + 1.0f, y2 + 1.0f, 1.4f);
        g.setColour (colour);
        g.drawLine (x1, y1, x2, y2, 1.4f);
    }
}

} // namespace theremin
