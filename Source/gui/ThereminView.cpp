#include "ThereminView.h"
#include "ThereminLookAndFeel.h"
#include "../PluginProcessor.h"

namespace theremin
{

namespace
{
    const char* const noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    juce::String noteName (int midiNote)
    {
        const int octave = midiNote / 12 - 1;
        return juce::String (noteNames[((midiNote % 12) + 12) % 12]) + juce::String (octave);
    }
}

ThereminView::ThereminView (AetherThereminAudioProcessor& p) : processor (p)
{
    setOpaque (false);
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

//==============================================================================
void ThereminView::resized()
{
    auto bounds = getLocalBounds().toFloat();

    stageArea = bounds.removeFromLeft (bounds.getWidth() * 0.63f);
    bounds.removeFromLeft (10.0f);
    tunerArea = bounds.removeFromTop (bounds.getHeight() * 0.5f - 5.0f);
    bounds.removeFromTop (10.0f);
    scopeArea = bounds;

    // instrument geometry: keeps the design proportions (stage 640 x 280)
    // when a resized window stretches the stage wider or taller
    const auto s = stageArea;
    rodX = s.getRight() - 46.0f;
    const auto cabW = juce::jmin (s.getWidth() * 0.40f, s.getHeight() * 0.915f);
    const auto cabH = juce::jmin (s.getHeight() * 0.27f, s.getWidth() * 0.118f);
    const auto legH = s.getHeight() * 0.14f;
    cabinet = { rodX - cabW + 16.0f, s.getBottom() - legH - cabH - 12.0f, cabW, cabH };

    rodBottom = cabinet.getY() + 4.0f;
    rodTop = s.getY() + 16.0f;

    loopRadiusX = juce::jmin (s.getWidth() * 0.085f, s.getHeight() * 0.194f);
    loopRadiusY = loopRadiusX * 0.32f;
    loopCentre = { cabinet.getX() - loopRadiusX - 12.0f, cabinet.getY() + cabH * 0.32f };

    playLeft = s.getX() + 22.0f;
    playRight = rodX - 18.0f;
}

float ThereminView::pitchForX (float x) const noexcept
{
    const auto t = juce::jlimit (0.0f, 1.0f, (x - playLeft) / juce::jmax (1.0f, playRight - playLeft));
    return lowPitch + t * (highPitch - lowPitch);
}

float ThereminView::xForPitch (float pitch) const noexcept
{
    const auto t = juce::jlimit (0.0f, 1.0f, (pitch - lowPitch) / (highPitch - lowPitch));
    return playLeft + t * (playRight - playLeft);
}

//==============================================================================
void ThereminView::playFromMouse (juce::Point<float> pos)
{
    const auto pitch = pitchForX (pos.x);
    const auto yNorm = juce::jlimit (0.0f, 1.0f, (pos.y - stageArea.getY()) / stageArea.getHeight());
    const auto level = std::pow (1.0f - yNorm, 1.2f);
    processor.setAirPlay (true, pitch, level);
}

void ThereminView::mouseDown (const juce::MouseEvent& e)
{
    if (stageArea.contains (e.position))
    {
        mousePlaying = true;
        playFromMouse (e.position);
    }
}

void ThereminView::mouseDrag (const juce::MouseEvent& e)
{
    if (mousePlaying)
        playFromMouse (e.position);
}

void ThereminView::mouseUp (const juce::MouseEvent&)
{
    if (mousePlaying)
    {
        mousePlaying = false;
        processor.setAirPlay (false, shownHandPitch, 0.0f);
    }
}

void ThereminView::mouseMove (const juce::MouseEvent& e)
{
    const bool inside = stageArea.contains (e.position);
    if (inside != hovering)
    {
        hovering = inside;
        repaint();
    }
}

//==============================================================================
void ThereminView::refresh()
{
    auto& t = processor.getTelemetry();
    const auto hand = t.handPitch.load (std::memory_order_relaxed);
    const auto full = t.fullPitch.load (std::memory_order_relaxed);
    const auto level = t.level.load (std::memory_order_relaxed);
    const auto freq = t.frequency.load (std::memory_order_relaxed);

    shownHandPitch += (hand - shownHandPitch) * 0.6f;
    shownFullPitch += (full - shownFullPitch) * 0.6f;
    shownLevel += (level - shownLevel) * 0.5f;
    shownFreq = freq;
    shownBendRange = t.bendRange.load (std::memory_order_relaxed);
    animPhase += 0.04f + 0.12f * shownLevel;
    if (animPhase > 1000.0f)
        animPhase -= 1000.0f;

    processor.getScope().copyLatest (scopeData.data(), scopeSamples);
    repaint();
}

//==============================================================================
void ThereminView::paint (juce::Graphics& g)
{
    drawStage (g);
    drawInstrument (g);
    drawHands (g);
    drawTuner (g, tunerArea);
    drawScope (g, scopeArea);
}

void ThereminView::drawStage (juce::Graphics& g)
{
    using namespace palette;
    const auto s = stageArea;

    g.setColour (stage);
    g.fillRoundedRectangle (s, 8.0f);

    // spotlight from above
    juce::ColourGradient spot (juce::Colour (0xff3a3326).withAlpha (0.85f), s.getCentreX() + s.getWidth() * 0.12f, s.getY(),
                               stage.withAlpha (0.0f), s.getCentreX(), s.getBottom(), true);
    g.setGradientFill (spot);
    g.fillRoundedRectangle (s, 8.0f);

    // floor shadow
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (cabinet.getX() - 30.0f, s.getBottom() - 26.0f, cabinet.getWidth() + 60.0f, 16.0f);

    g.setColour (brassDark.withAlpha (0.6f));
    g.drawRoundedRectangle (s.reduced (0.5f), 8.0f, 1.0f);

    if (! mousePlaying && shownLevel < 0.02f)
    {
        g.setColour (creamDim.withAlpha (hovering ? 0.85f : 0.5f));
        g.setFont (ThereminLookAndFeel::labelFont (12.5f));
        g.drawFittedText ("Click and drag in the air to play - nearer the antenna = higher pitch, higher up = louder",
                          s.reduced (16.0f, 10.0f).removeFromTop (20.0f).toNearestInt(), juce::Justification::centredLeft, 1);
    }
}

void ThereminView::drawInstrument (juce::Graphics& g)
{
    using namespace palette;
    const auto s = stageArea;

    // legs
    const auto legTop = cabinet.getBottom() - 2.0f;
    const auto legBottom = s.getBottom() - 18.0f;
    for (float fx : { 0.08f, 0.92f })
    {
        const auto lx = cabinet.getX() + cabinet.getWidth() * fx;
        juce::Path leg;
        leg.startNewSubPath (lx - 6.0f, legTop);
        leg.lineTo (lx + 6.0f, legTop);
        leg.lineTo (lx + 3.0f, legBottom);
        leg.lineTo (lx - 3.0f, legBottom);
        leg.closeSubPath();
        g.setGradientFill (juce::ColourGradient (walnutLight, lx - 6.0f, legTop, walnutDark, lx + 6.0f, legTop, false));
        g.fillPath (leg);
    }

    // stretcher between the legs
    g.setColour (walnut.darker (0.3f));
    g.fillRect (cabinet.getX() + cabinet.getWidth() * 0.08f, legTop + (legBottom - legTop) * 0.6f,
                cabinet.getWidth() * 0.84f, 4.0f);

    // cabinet body
    {
        juce::ColourGradient wood (walnutLight.brighter (0.15f), cabinet.getX(), cabinet.getY(),
                                   walnutDark, cabinet.getRight(), cabinet.getBottom(), false);
        wood.addColour (0.45, walnut);
        g.setGradientFill (wood);
        g.fillRoundedRectangle (cabinet, 5.0f);

        // grain
        juce::Random rng (7);
        g.saveState();
        g.reduceClipRegion (cabinet.toNearestInt());
        for (int i = 0; i < 18; ++i)
        {
            const auto y = cabinet.getY() + rng.nextFloat() * cabinet.getHeight();
            juce::Path grain;
            grain.startNewSubPath (cabinet.getX(), y);
            for (float x = cabinet.getX(); x <= cabinet.getRight(); x += 12.0f)
                grain.lineTo (x, y + 2.0f * std::sin ((x + (float) i * 31.0f) * 0.03f));
            g.setColour (juce::Colours::black.withAlpha (0.10f + 0.08f * rng.nextFloat()));
            g.strokePath (grain, juce::PathStrokeType (0.8f));
        }
        g.restoreState();

        // top lid highlight
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillRoundedRectangle (cabinet.withHeight (5.0f), 3.0f);

        // front panel with two knobs and a nameplate
        const auto panelR = cabinet.reduced (cabinet.getWidth() * 0.12f, cabinet.getHeight() * 0.26f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (panelR, 3.0f);
        g.setColour (brassDark);
        g.drawRoundedRectangle (panelR, 3.0f, 1.0f);

        for (float fx : { 0.2f, 0.8f })
        {
            const auto kc = juce::Point<float> (panelR.getX() + panelR.getWidth() * fx, panelR.getCentreY());
            const auto kr = panelR.getHeight() * 0.28f;
            g.setGradientFill (juce::ColourGradient (brass.brighter (0.3f), kc.x - kr, kc.y - kr, brassDark, kc.x + kr, kc.y + kr, false));
            g.fillEllipse (kc.x - kr, kc.y - kr, kr * 2.0f, kr * 2.0f);
        }

        g.setColour (brass);
        g.setFont (ThereminLookAndFeel::labelFont (juce::jmax (9.0f, panelR.getHeight() * 0.28f), true));
        g.drawText ("AETHER", panelR, juce::Justification::centred);
    }

    // pitch antenna (vertical rod)
    {
        const auto level = shownLevel;
        const juce::Rectangle<float> rod (rodX - 3.0f, rodTop, 6.0f, rodBottom - rodTop);

        // capacitance field rings
        for (int k = 0; k < 6; ++k)
        {
            const auto frac = std::fmod (animPhase * 0.35f + (float) k / 6.0f, 1.0f);
            const auto rx = 10.0f + frac * 120.0f;
            const auto alpha = level * 0.28f * (1.0f - frac);
            g.setColour (amber.withAlpha (alpha));
            g.drawEllipse (rodX - rx, (rodTop + rodBottom) * 0.5f - rx * 1.6f, rx * 2.0f, rx * 3.2f, 1.2f);
        }

        // glow
        if (level > 0.001f)
        {
            juce::ColourGradient glow (amber.withAlpha (0.35f * level), rodX, (rodTop + rodBottom) * 0.5f,
                                       amber.withAlpha (0.0f), rodX + 40.0f, (rodTop + rodBottom) * 0.5f, true);
            g.setGradientFill (glow);
            g.fillRect (rod.expanded (40.0f, 0.0f));
        }

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffeeeeee), rod.getX(), 0.0f,
                                                 juce::Colour (0xff6c6c70), rod.getRight(), 0.0f, false));
        g.fillRoundedRectangle (rod, 3.0f);
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.fillEllipse (rodX - 4.0f, rodTop - 4.0f, 8.0f, 8.0f);

        // brass mount
        g.setGradientFill (juce::ColourGradient (brass.brighter (0.3f), rodX - 9.0f, 0.0f, brassDark, rodX + 9.0f, 0.0f, false));
        g.fillRoundedRectangle (rodX - 9.0f, rodBottom - 12.0f, 18.0f, 12.0f, 2.0f);
    }

    // volume antenna (horizontal loop)
    {
        juce::Path stem;
        stem.startNewSubPath (cabinet.getX(), loopCentre.y);
        stem.lineTo (loopCentre.x + loopRadiusX, loopCentre.y);
        g.setColour (juce::Colour (0xffb0b0b5));
        g.strokePath (stem, juce::PathStrokeType (4.0f));

        g.setColour (juce::Colour (0xffd8d8dc));
        g.drawEllipse (loopCentre.x - loopRadiusX, loopCentre.y - loopRadiusY, loopRadiusX * 2.0f, loopRadiusY * 2.0f, 4.0f);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawEllipse (loopCentre.x - loopRadiusX + 1.0f, loopCentre.y - loopRadiusY - 1.0f, loopRadiusX * 2.0f - 2.0f, loopRadiusY * 2.0f, 1.0f);

        g.setGradientFill (juce::ColourGradient (brass.brighter (0.3f), 0.0f, loopCentre.y - 7.0f, brassDark, 0.0f, loopCentre.y + 7.0f, false));
        g.fillRoundedRectangle (cabinet.getX() - 10.0f, loopCentre.y - 7.0f, 12.0f, 14.0f, 2.0f);
    }
}

void ThereminView::drawHands (juce::Graphics& g)
{
    using namespace palette;
    const auto level = shownLevel;
    const bool active = level > 0.003f || mousePlaying;

    // right (pitch) hand: its distance to the rod is the pitch
    {
        const auto x = xForPitch (shownFullPitch);
        const auto y = rodTop + (rodBottom - rodTop) * 0.42f;
        const auto alpha = active ? 0.35f + 0.65f * juce::jmin (1.0f, level * 2.0f) : 0.18f;

        juce::Path dash;
        dash.startNewSubPath (x + 16.0f, y);
        dash.lineTo (rodX - 6.0f, y);
        const float pattern[] = { 3.0f, 5.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dash, dash, pattern, 2);
        g.setColour (amber.withAlpha (alpha * 0.6f));
        g.fillPath (dash);

        juce::ColourGradient halo (amber.withAlpha (0.45f * alpha), x, y, amber.withAlpha (0.0f), x + 34.0f, y, true);
        g.setGradientFill (halo);
        g.fillEllipse (x - 34.0f, y - 34.0f, 68.0f, 68.0f);

        g.setColour (cream.withAlpha (alpha));
        g.fillEllipse (x - 9.0f, y - 12.0f, 18.0f, 24.0f);          // palm
        for (int f = 0; f < 4; ++f)                                    // fingers towards the rod
            g.fillRoundedRectangle (x + 6.0f, y - 11.0f + (float) f * 6.0f, 12.0f - std::abs ((float) f - 1.5f) * 2.0f, 4.0f, 2.0f);
    }

    // left (volume) hand: height above the loop is the loudness
    {
        const auto maxRise = loopCentre.y - stageArea.getY() - 40.0f;
        const auto y = loopCentre.y - 18.0f - level * maxRise;
        const auto x = loopCentre.x;
        const auto alpha = active ? 0.35f + 0.65f * juce::jmin (1.0f, level * 2.0f) : 0.18f;

        juce::ColourGradient halo (phosphor.withAlpha (0.35f * alpha), x, y, phosphor.withAlpha (0.0f), x + 30.0f, y, true);
        g.setGradientFill (halo);
        g.fillEllipse (x - 30.0f, y - 30.0f, 60.0f, 60.0f);

        g.setColour (cream.withAlpha (alpha));
        g.fillEllipse (x - 13.0f, y - 6.0f, 26.0f, 13.0f);             // palm, facing the loop
        for (int f = 0; f < 4; ++f)
            g.fillRoundedRectangle (x - 13.0f + (float) f * 7.0f, y - 2.0f, 5.0f, 12.0f, 2.0f);
    }
}

void ThereminView::drawTuner (juce::Graphics& g, juce::Rectangle<float> area)
{
    using namespace palette;

    g.setColour (juce::Colour (0xff120d09));
    g.fillRoundedRectangle (area, 8.0f);
    g.setColour (brassDark.withAlpha (0.7f));
    g.drawRoundedRectangle (area.reduced (0.5f), 8.0f, 1.0f);

    auto face = area.reduced (10.0f);
    g.setGradientFill (juce::ColourGradient (cream, face.getCentreX(), face.getY(),
                                             juce::Colour (0xffcdbd98), face.getCentreX(), face.getBottom(), false));
    g.fillRoundedRectangle (face, 5.0f);

    const auto active = shownLevel > 0.003f;
    const int nearest = (int) std::lround (shownHandPitch);
    const auto cents = (shownFullPitch - (float) nearest) * 100.0f;

    // meter arc
    const auto pivot = juce::Point<float> (face.getCentreX(), face.getBottom() - 8.0f);
    const auto radius = juce::jmin (face.getWidth() * 0.46f, face.getHeight() * 0.85f);
    const auto maxAngle = 0.85f;

    for (int c = -50; c <= 50; c += 10)
    {
        const auto a = (float) c / 50.0f * maxAngle;
        const auto r1 = radius, r2 = radius - (c % 50 == 0 || c == 0 ? 12.0f : 7.0f);
        g.setColour (c == 0 ? juce::Colour (0xff8a1c10) : juce::Colour (0xff3a2a1a));
        g.drawLine (pivot.x + r1 * std::sin (a), pivot.y - r1 * std::cos (a),
                    pivot.x + r2 * std::sin (a), pivot.y - r2 * std::cos (a), c == 0 ? 2.0f : 1.2f);
    }

    g.setFont (ThereminLookAndFeel::labelFont (10.0f));
    g.setColour (juce::Colour (0xff3a2a1a));
    for (int c : { -50, 50 })
    {
        const auto a = (float) c / 50.0f * maxAngle;
        const auto p = juce::Point<float> (pivot.x + (radius - 22.0f) * std::sin (a), pivot.y - (radius - 22.0f) * std::cos (a));
        g.drawText (c > 0 ? "+50" : "-50", juce::Rectangle<float> (40.0f, 12.0f).withCentre (p), juce::Justification::centred);
    }

    // note name + frequency
    g.setColour (juce::Colour (0xff2a1c10).withAlpha (active ? 1.0f : 0.45f));
    g.setFont (ThereminLookAndFeel::labelFont (juce::jmin (34.0f, face.getHeight() * 0.32f), true));
    g.drawText (noteName (nearest), face.withTrimmedBottom (face.getHeight() * 0.18f), juce::Justification::centred);

    g.setFont (ThereminLookAndFeel::labelFont (11.5f));
    const auto info = juce::String (shownFreq, 1) + " Hz   " + (cents >= 0.0f ? "+" : "") + juce::String (juce::roundToInt (cents)) + " ct";
    g.drawText (info, face.removeFromBottom (face.getHeight() * 0.36f).withTrimmedBottom (14.0f), juce::Justification::centred);

    // needle
    const auto a = juce::jlimit (-1.0f, 1.0f, cents / 50.0f) * maxAngle;
    g.setColour (juce::Colour (0xff8a1c10).withAlpha (active ? 1.0f : 0.35f));
    g.drawLine (pivot.x, pivot.y, pivot.x + (radius - 2.0f) * std::sin (a), pivot.y - (radius - 2.0f) * std::cos (a), 1.6f);
    g.setColour (juce::Colour (0xff2a1c10));
    g.fillEllipse (pivot.x - 4.0f, pivot.y - 4.0f, 8.0f, 8.0f);

    g.setFont (ThereminLookAndFeel::labelFont (9.5f));
    g.setColour (juce::Colour (0xff5a4630));
    g.drawText ("BEND +/-" + juce::String (juce::roundToInt (shownBendRange)) + " st",
                area.reduced (16.0f, 14.0f), juce::Justification::topRight);
    g.drawText ("PITCH", area.reduced (16.0f, 14.0f), juce::Justification::topLeft);
}

void ThereminView::drawScope (juce::Graphics& g, juce::Rectangle<float> area)
{
    using namespace palette;

    g.setColour (juce::Colour (0xff120d09));
    g.fillRoundedRectangle (area, 8.0f);
    g.setColour (brassDark.withAlpha (0.7f));
    g.drawRoundedRectangle (area.reduced (0.5f), 8.0f, 1.0f);

    auto screen = area.reduced (10.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0f2a1d), screen.getCentreX(), screen.getCentreY(),
                                             juce::Colour (0xff04100a), screen.getX(), screen.getY(), true));
    g.fillRoundedRectangle (screen, 10.0f);

    // graticule
    g.setColour (phosphor.withAlpha (0.08f));
    for (int i = 1; i < 8; ++i)
        g.drawVerticalLine ((int) (screen.getX() + screen.getWidth() * (float) i / 8.0f), screen.getY() + 4.0f, screen.getBottom() - 4.0f);
    for (int i = 1; i < 4; ++i)
        g.drawHorizontalLine ((int) (screen.getY() + screen.getHeight() * (float) i / 4.0f), screen.getX() + 4.0f, screen.getRight() - 4.0f);

    // trigger on a rising zero crossing so the waveform stands still
    const auto sr = (float) processor.getSampleRateForDisplay();
    const auto period = juce::jlimit (16.0f, 900.0f, sr / juce::jmax (20.0f, shownFreq));
    const int window = juce::jlimit (64, scopeSamples / 2, (int) (period * 2.5f));

    int start = scopeSamples - window - 1;
    for (int i = start; i > juce::jmax (1, start - (int) period * 2); --i)
    {
        if (scopeData[(size_t) i - 1] < 0.0f && scopeData[(size_t) i] >= 0.0f)
        {
            start = i;
            break;
        }
    }

    float peak = 0.05f;
    for (int i = 0; i < window; ++i)
        peak = juce::jmax (peak, std::abs (scopeData[(size_t) (start + i)]));

    const auto gain = 0.42f * screen.getHeight() / peak;
    scopePath.clear();
    const auto points = juce::jmin (window, 400);
    for (int p = 0; p < points; ++p)
    {
        const auto idx = start + (int) ((float) p / (float) (points - 1) * (float) (window - 1));
        const auto x = screen.getX() + 6.0f + (screen.getWidth() - 12.0f) * (float) p / (float) (points - 1);
        const auto y = screen.getCentreY() - scopeData[(size_t) idx] * gain;
        if (p == 0) scopePath.startNewSubPath (x, y);
        else        scopePath.lineTo (x, y);
    }

    g.setColour (phosphor.withAlpha (0.18f));
    g.strokePath (scopePath, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (phosphor.withAlpha (shownLevel > 0.003f ? 0.95f : 0.35f));
    g.strokePath (scopePath, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setFont (ThereminLookAndFeel::labelFont (9.5f));
    g.setColour (phosphor.withAlpha (0.5f));
    g.drawText ("HETERODYNE OUTPUT", screen.reduced (8.0f, 6.0f), juce::Justification::topLeft);
}

} // namespace theremin
