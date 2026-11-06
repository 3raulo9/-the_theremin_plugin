#include "PluginEditor.h"

using namespace theremin;

namespace
{
    constexpr int margin = 12;
    constexpr int headerHeight = 58;
    constexpr int viewHeight = 280;
    constexpr int rowGap = 10;
    constexpr int rowHeight = 128;
    constexpr int sectionTitleHeight = 22;
}

//==============================================================================
AetherThereminAudioProcessorEditor::AetherThereminAudioProcessorEditor (AetherThereminAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), view (p)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);
    addAndMakeVisible (content);
    content.addAndMakeVisible (view);

    // --- Pitch hand ----------------------------------------------------------
    Section pitch { "PITCH HAND" };
    pitch.cells.push_back (&addKnob (ids::glide,     "GLIDE",      "How long the pitch hand takes to travel to a new note when notes overlap (slides).").slider);
    pitch.cells.push_back (&addKnob (ids::overshoot, "OVERSHOOT",  "Human hand inertia: big leaps land slightly past the note and settle back.").slider);
    pitch.cells.push_back (&addKnob (ids::scoop,     "SCOOP",      "Fresh notes are approached from slightly below, like a thereminist finding the pitch.").slider);
    pitch.cells.push_back (&addKnob (ids::bendRange, "BEND RANGE", "Pitch-bend range in semitones. Set it to the same value as FL Studio's wrapper pitch bend range (default 12). It follows the host automatically when the host sends RPN 0.").slider);
    pitch.cells.push_back (&addKnob (ids::octave,    "OCTAVE",     "Transpose in octaves.").slider);
    pitch.cells.push_back (&addKnob (ids::fineTune,  "FINE",       "Fine tuning in cents.").slider);
    pitch.numCells = 6;
    sections.push_back (pitch);

    Section vib { "VIBRATO" };
    vib.cells.push_back (&addKnob (ids::vibDepth, "DEPTH",    "Hand vibrato depth in cents. The mod wheel adds more.").slider);
    vib.cells.push_back (&addKnob (ids::vibRate,  "RATE",     "Hand vibrato speed.").slider);
    vib.cells.push_back (&addKnob (ids::vibDelay, "DELAY",    "Time before the vibrato starts on a new note. Vibrato also eases off while sliding.").slider);
    vib.cells.push_back (&addKnob (ids::humanize, "HUMANIZE", "Irregular vibrato, slow thermal drift and tiny hand tremor.").slider);
    vib.numCells = 4;
    sections.push_back (vib);

    Section slides { "SLIDES" };
    slides.cells.push_back (&addChooser (ids::glideMode,   "GLIDE MODE",   "Legato: only overlapping notes slide (FL Studio piano-roll slides). Always: every note glides from the previous pitch.").box);
    slides.cells.push_back (&addChooser (ids::noteRelease, "NOTE RELEASE", "When a sliding note ends while an older note is still held: Stay keeps the new pitch (FL Studio slide-note behaviour), Return glides back to the held note.").box);
    slides.numCells = 2;
    sections.push_back (slides);

    Section tone { "HETERODYNE TONE" };
    tone.cells.push_back (&addKnob (ids::timbre,  "TIMBRE",  "Balance of the two radio-frequency oscillators at the detector: pure sine to bright, nasal 'voice'.").slider);
    tone.cells.push_back (&addKnob (ids::pull,    "PULL",    "Coupling between the RF oscillators. Bends the waveform so low notes turn reedy, just like a real theremin.").slider);
    tone.cells.push_back (&addKnob (ids::warmth,  "WARMTH",  "Valve output stage saturation (adds even harmonics as you play louder).").slider);
    tone.cells.push_back (&addKnob (ids::tone,    "TONE",    "Treble control.").slider);
    tone.cells.push_back (&addKnob (ids::cabinet, "CABINET", "Voicing of the instrument's built-in loudspeaker cabinet.").slider);
    tone.cells.push_back (&addKnob (ids::noise,   "HUM",     "Vintage mains hum and valve hiss while playing.").slider);
    tone.numCells = 6;
    sections.push_back (tone);

    Section volume { "VOLUME HAND" };
    volume.cells.push_back (&addKnob (ids::attack,  "ATTACK",   "How fast the volume hand rises on a new phrase. Legato notes never re-attack.").slider);
    volume.cells.push_back (&addKnob (ids::release, "RELEASE",  "How fast the sound fades after the last note ends.").slider);
    volume.cells.push_back (&addKnob (ids::velSens, "VELOCITY", "How much note velocity changes loudness. CC11 (expression) also works as a volume hand.").slider);
    volume.numCells = 3;
    sections.push_back (volume);

    Section space { "SPACE" };
    space.cells.push_back (&addKnob (ids::reverbMix, "REVERB", "Room reverb amount.").slider);
    space.cells.push_back (&addKnob (ids::roomSize,  "ROOM",   "Room size.").slider);
    space.cells.push_back (&addKnob (ids::output,    "OUTPUT", "Output level.").slider);
    space.numCells = 3;
    sections.push_back (space);

    // --- Presets ---------------------------------------------------------------
    for (int i = 0; i < processor.getNumPrograms(); ++i)
        presetBox.addItem (processor.getProgramName (i), i + 1);

    presetBox.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const auto index = presetBox.getSelectedId() - 1;
        if (index >= 0)
            processor.loadFactoryPreset (index);
    };
    content.addAndMakeVisible (presetBox);

    presetLabel.setText ("PRESET", juce::dontSendNotification);
    presetLabel.setFont (ThereminLookAndFeel::labelFont (11.0f, true));
    presetLabel.setColour (juce::Label::textColourId, palette::brass);
    presetLabel.setJustificationType (juce::Justification::centredRight);
    content.addAndMakeVisible (presetLabel);

    // Freely resizable (corner grip, and the host window's own edges where the
    // host supports it). Opens at the size saved with the project, else 70 %.
    // Read the saved size first: setting the limits resizes (and records) the
    // still-empty editor.
    const auto saved = processor.getSavedEditorSize();
    const int minW = juce::roundToInt (designWidth * minScale), minH = juce::roundToInt (designHeight * minScale);
    const int maxW = juce::roundToInt (designWidth * maxScale), maxH = juce::roundToInt (designHeight * maxScale);

    setResizable (true, true);
    setResizeLimits (minW, minH, maxW, maxH);

    if (saved.x > 0 && saved.y > 0)
        setSize (juce::jlimit (minW, maxW, saved.x), juce::jlimit (minH, maxH, saved.y));
    else
        setSize (defaultWidth, defaultHeight);

    startTimerHz (30);
}

AetherThereminAudioProcessorEditor::~AetherThereminAudioProcessorEditor()
{
    stopTimer();
    processor.setAirPlay (false, 60.0f, 0.0f);
    setLookAndFeel (nullptr);
}

//==============================================================================
AetherThereminAudioProcessorEditor::Knob& AetherThereminAudioProcessorEditor::addKnob (const char* paramId,
                                                                                        const juce::String& labelText,
                                                                                        const juce::String& tooltip)
{
    auto knob = std::make_unique<Knob>();
    auto& s = knob->slider;

    // Parent first so the text box is created by our look-and-feel.
    content.addAndMakeVisible (s);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxTextColourId, palette::creamDim);
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.22f, juce::MathConstants<float>::pi * 2.78f, true);
    s.setTooltip (tooltip);
    s.setPopupDisplayEnabled (false, false, nullptr);

    knob->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, paramId, s);

    if (auto* param = processor.apvts.getParameter (paramId))
        s.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

    knob->label.setText (labelText, juce::dontSendNotification);
    knob->label.setFont (ThereminLookAndFeel::labelFont (10.5f, true));
    knob->label.setColour (juce::Label::textColourId, palette::cream);
    knob->label.setJustificationType (juce::Justification::centred);
    knob->label.setInterceptsMouseClicks (false, false);
    content.addAndMakeVisible (knob->label);

    knobs.push_back (std::move (knob));
    return *knobs.back();
}

AetherThereminAudioProcessorEditor::Chooser& AetherThereminAudioProcessorEditor::addChooser (const char* paramId,
                                                                                              const juce::String& labelText,
                                                                                              const juce::String& tooltip)
{
    auto chooser = std::make_unique<Chooser>();

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (paramId)))
        chooser->box.addItemList (choice->choices, 1);

    chooser->box.setTooltip (tooltip);
    content.addAndMakeVisible (chooser->box);
    chooser->attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, paramId, chooser->box);

    chooser->label.setText (labelText, juce::dontSendNotification);
    chooser->label.setFont (ThereminLookAndFeel::labelFont (10.5f, true));
    chooser->label.setColour (juce::Label::textColourId, palette::cream);
    chooser->label.setJustificationType (juce::Justification::centredLeft);
    content.addAndMakeVisible (chooser->label);

    choosers.push_back (std::move (chooser));
    return *choosers.back();
}

//==============================================================================
void AetherThereminAudioProcessorEditor::timerCallback()
{
    view.refresh();

    const auto programId = processor.getCurrentProgram() + 1;
    if (presetBox.getSelectedId() != programId)
        presetBox.setSelectedId (programId, juce::dontSendNotification);
}

void AetherThereminAudioProcessorEditor::rebuildBackground (int imageWidth, int imageHeight)
{
    using namespace palette;

    // Rendered at the screen's real resolution (not the design size) so the
    // wood stays sharp at any window size; drawn in design units.
    background = juce::Image (juce::Image::RGB, juce::jmax (1, imageWidth), juce::jmax (1, imageHeight), true);
    juce::Graphics g (background);

    const auto b = content.getLocalBounds().toFloat();
    g.addTransform (juce::AffineTransform::scale ((float) background.getWidth() / juce::jmax (1.0f, b.getWidth()),
                                                  (float) background.getHeight() / juce::jmax (1.0f, b.getHeight())));
    juce::ColourGradient wood (walnut, 0.0f, 0.0f, walnutDark, 0.0f, b.getBottom(), false);
    wood.addColour (0.5, walnut.darker (0.25f));
    g.setGradientFill (wood);
    g.fillAll();

    // procedural walnut grain
    juce::Random rng (1920);
    const int numGrains = juce::roundToInt (140.0f * b.getHeight() / (float) designHeight);
    for (int i = 0; i < numGrains; ++i)
    {
        const auto y0 = rng.nextFloat() * b.getHeight();
        const auto amp = 1.5f + rng.nextFloat() * 6.0f;
        const auto freq = 0.004f + rng.nextFloat() * 0.01f;
        const auto phase = rng.nextFloat() * 6.28f;
        juce::Path grain;
        grain.startNewSubPath (0.0f, y0);
        for (float x = 0.0f; x <= b.getWidth(); x += 10.0f)
            grain.lineTo (x, y0 + amp * std::sin (x * freq + phase) + 0.6f * amp * std::sin (x * freq * 3.1f + phase * 2.0f));

        g.setColour ((rng.nextBool() ? juce::Colours::black : walnutLight).withAlpha (0.05f + 0.07f * rng.nextFloat()));
        g.strokePath (grain, juce::PathStrokeType (0.6f + rng.nextFloat() * 1.4f));
    }

    // vignette
    juce::ColourGradient vignette (juce::Colours::transparentBlack, b.getCentreX(), b.getCentreY(),
                                   juce::Colours::black.withAlpha (0.45f), 0.0f, 0.0f, true);
    g.setGradientFill (vignette);
    g.fillAll();
}

void AetherThereminAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Only visible in the sub-pixel sliver the scaled content may leave.
    g.fillAll (palette::walnutDark);
}

void AetherThereminAudioProcessorEditor::paintContent (juce::Graphics& g)
{
    using namespace palette;

    const auto area = content.getLocalBounds();

    {
        // physical pixels per design unit, capped so huge windows stay cheap
        auto pixelScale = g.getInternalContext().getPhysicalPixelScaleFactor();
        const auto maxPixels = 16.0e6f;
        const auto designPixels = (float) area.getWidth() * (float) area.getHeight();
        pixelScale = juce::jlimit (0.25f, std::sqrt (maxPixels / juce::jmax (1.0f, designPixels)), pixelScale);

        const auto w = juce::roundToInt ((float) area.getWidth() * pixelScale);
        const auto h = juce::roundToInt ((float) area.getHeight() * pixelScale);
        if (background.isNull() || std::abs (background.getWidth() - w) > 1 || std::abs (background.getHeight() - h) > 1)
            rebuildBackground (w, h);
    }

    g.drawImage (background, area.toFloat());

    // header
    auto header = area.withHeight (headerHeight).reduced (margin + 4, 8);
    {
        auto titleArea = header.removeFromLeft (520);
        auto top = titleArea.removeFromTop (30);

        g.setFont (ThereminLookAndFeel::labelFont (27.0f, true));
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawText ("AETHER", top.translated (1, 2), juce::Justification::centredLeft);
        g.setColour (brass);
        g.drawText ("AETHER", top, juce::Justification::centredLeft);

        const auto aetherWidth = juce::GlyphArrangement::getStringWidthInt (ThereminLookAndFeel::labelFont (27.0f, true), "AETHER ");
        g.setColour (cream);
        g.setFont (ThereminLookAndFeel::labelFont (27.0f));
        g.drawText ("THEREMIN", top.withTrimmedLeft (aetherWidth), juce::Justification::centredLeft);

        g.setFont (ThereminLookAndFeel::labelFont (10.5f));
        g.setColour (creamDim);
        g.drawText ("HETERODYNE OSCILLATORS  -  CONTINUOUS PITCH  -  SLIDE-READY", titleArea, juce::Justification::centredLeft);
    }

    // section panels
    for (const auto& s : sections)
    {
        const auto r = s.bounds.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 7.0f);
        g.setColour (panel.withAlpha (0.92f));
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (brassDark.withAlpha (0.8f));
        g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);

        auto titleBar = s.bounds.withHeight (sectionTitleHeight).reduced (10, 0);
        g.setFont (ThereminLookAndFeel::labelFont (11.0f, true));
        g.setColour (brass);
        g.drawText (s.title, titleBar, juce::Justification::centredLeft);

        const auto textW = juce::GlyphArrangement::getStringWidthInt (ThereminLookAndFeel::labelFont (11.0f, true), s.title);
        g.setColour (brassDark.withAlpha (0.6f));
        g.drawHorizontalLine (titleBar.getCentreY(), (float) (titleBar.getX() + textW + 8), (float) titleBar.getRight());
    }

    // footer hint
    auto footer = area.withTrimmedTop (area.getHeight() - 24).reduced (margin + 4, 0);
    g.setFont (ThereminLookAndFeel::labelFont (10.5f));
    g.setColour (creamDim.withAlpha (0.8f));
    g.drawText ("Overlap notes in the piano roll to slide. Pitch-bend range follows FL Studio's wrapper setting (12 st by default). Mod wheel = vibrato, CC11 = volume hand.",
                footer, juce::Justification::centredLeft);
}

void AetherThereminAudioProcessorEditor::layoutSection (Section& section, juce::Rectangle<int> area)
{
    section.bounds = area;
    auto inner = area.reduced (6, 4);
    inner.removeFromTop (sectionTitleHeight - 2);

    if (section.title == "SLIDES")
    {
        inner.reduce (6, 4);
        const auto slot = juce::jmin (inner.getHeight() / 2, 50);
        inner = inner.withSizeKeepingCentre (inner.getWidth(), slot * 2);

        for (auto& chooser : choosers)
        {
            auto r = inner.removeFromTop (slot);
            chooser->label.setBounds (r.removeFromTop (16));
            chooser->box.setBounds (r.removeFromTop (26));
        }
        return;
    }

    const auto cellW = inner.getWidth() / juce::jmax (1, section.numCells);

    // label + square knob + value read-out stay together (centred) when a
    // tall window makes the row taller than the knobs need
    constexpr int labelH = 15, valueH = 16;
    inner = inner.withSizeKeepingCentre (inner.getWidth(), juce::jmin (inner.getHeight(), labelH + cellW + valueH));

    for (auto* c : section.cells)
    {
        auto cell = inner.removeFromLeft (cellW);

        for (auto& k : knobs)
        {
            if (&k->slider == c)
            {
                k->label.setBounds (cell.removeFromTop (labelH));
                k->slider.setBounds (cell);
                break;
            }
        }
    }
}

void AetherThereminAudioProcessorEditor::resized()
{
    const auto w = juce::jmax (1, getWidth());
    const auto h = juce::jmax (1, getHeight());

    // One uniform scale for everything (text and knobs never get squashed).
    // The design canvas grows in whichever direction has spare room, so the
    // layout fills any window shape instead of leaving empty bars.
    uiScale = juce::jmin ((float) w / (float) designWidth, (float) h / (float) designHeight);
    const auto canvasW = juce::jmax (designWidth,  (int) std::ceil ((float) w / uiScale - 0.01f));
    const auto canvasH = juce::jmax (designHeight, (int) std::ceil ((float) h / uiScale - 0.01f));

    content.setTransform ({});
    content.setBounds (0, 0, canvasW, canvasH);
    content.setTransform (juce::AffineTransform::scale (uiScale));

    processor.setSavedEditorSize (w, h);
}

void AetherThereminAudioProcessorEditor::layoutContent()
{
    auto bounds = content.getLocalBounds().reduced (margin, 0);

    // Spare height (windows taller than the design shape): the knob rows grow
    // only as far as their knobs can use it (knob size is limited by the cell
    // width), everything else goes to the instrument view.
    const int extra = juce::jmax (0, content.getHeight() - designHeight);
    const int usableW = content.getWidth() - 2 * margin - 2 * rowGap;
    const int knobRowNeed = juce::jmax (rowHeight, usableW / 12 + 60);
    const int rowH = juce::jmin (rowHeight + extra * 225 / 1000, knobRowNeed);
    const int viewH = viewHeight + extra - 2 * (rowH - rowHeight);

    auto header = bounds.removeFromTop (headerHeight);
    auto presetArea = header.reduced (4, 14).removeFromRight (300);
    presetBox.setBounds (presetArea.removeFromRight (230));
    presetLabel.setBounds (presetArea.withTrimmedRight (8));

    view.setBounds (bounds.removeFromTop (viewH));
    bounds.removeFromTop (rowGap);

    const int cellsPerRow = 12;
    auto layoutRow = [this, cellsPerRow] (juce::Rectangle<int> row, std::initializer_list<int> sectionIndices)
    {
        const int gaps = (int) sectionIndices.size() - 1;
        const int usable = row.getWidth() - gaps * rowGap;
        int x = row.getX();
        int i = 0;
        for (auto index : sectionIndices)
        {
            auto& s = sections[(size_t) index];
            const bool last = ++i == (int) sectionIndices.size();
            const int w = last ? row.getRight() - x : usable * s.numCells / cellsPerRow;
            layoutSection (s, { x, row.getY(), w, row.getHeight() });
            x += w + rowGap;
        }
    };

    layoutRow (bounds.removeFromTop (rowH), { 0, 1, 2 });
    bounds.removeFromTop (rowGap);
    layoutRow (bounds.removeFromTop (rowH), { 3, 4, 5 });

    background = {};
}
