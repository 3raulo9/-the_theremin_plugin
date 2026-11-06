#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

using namespace theremin;

//==============================================================================
AetherThereminAudioProcessor::AetherThereminAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AetherTheremin", createParameterLayout())
{
    auto get = [this] (const char* id)
    {
        auto* p = apvts.getRawParameterValue (id);
        jassert (p != nullptr);
        return p;
    };

    params.glide       = get (ids::glide);
    params.glideMode   = get (ids::glideMode);
    params.noteRelease = get (ids::noteRelease);
    params.overshoot   = get (ids::overshoot);
    params.scoop       = get (ids::scoop);
    params.bendRange   = get (ids::bendRange);
    params.octave      = get (ids::octave);
    params.fineTune    = get (ids::fineTune);
    params.vibDepth    = get (ids::vibDepth);
    params.vibRate     = get (ids::vibRate);
    params.vibDelay    = get (ids::vibDelay);
    params.humanize    = get (ids::humanize);
    params.timbre      = get (ids::timbre);
    params.pull        = get (ids::pull);
    params.warmth      = get (ids::warmth);
    params.tone        = get (ids::tone);
    params.cabinet     = get (ids::cabinet);
    params.noise       = get (ids::noise);
    params.attack      = get (ids::attack);
    params.release     = get (ids::release);
    params.velSens     = get (ids::velSens);
    params.reverbMix   = get (ids::reverbMix);
    params.roomSize    = get (ids::roomSize);
    params.output      = get (ids::output);

    apvts.addParameterListener (ids::bendRange, this);
}

AetherThereminAudioProcessor::~AetherThereminAudioProcessor()
{
    apvts.removeParameterListener (ids::bendRange, this);
    cancelPendingUpdate();
}

void AetherThereminAudioProcessor::parameterChanged (const juce::String&, float)
{
    // The user (or host automation) moved the bend range: it now wins over a
    // range the host announced earlier via RPN 0.
    if (! applyingHostBendRange.load())
        bendRangeTouched.store (true);
}

//==============================================================================
bool AetherThereminAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.inputBuses.size() > 0 && ! layouts.getMainInputChannelSet().isDisabled())
        return false;

    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void AetherThereminAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    lastSampleRate.store (sampleRate);
    engine.setSettings (readSettings());
    engine.prepare (sampleRate, samplesPerBlock);
}

void AetherThereminAudioProcessor::reset()
{
    engine.reset();
}

EngineSettings AetherThereminAudioProcessor::readSettings()
{
    EngineSettings s;
    s.glideMs       = params.glide->load();
    s.glideAlways   = params.glideMode->load() > 0.5f;
    s.returnToHeld  = params.noteRelease->load() > 0.5f;
    s.overshoot     = params.overshoot->load();
    s.scoop         = params.scoop->load();
    s.bendRange     = params.bendRange->load();
    s.octave        = (int) std::lround (params.octave->load());
    s.fineCents     = params.fineTune->load();
    s.vibDepthCents = params.vibDepth->load();
    s.vibRateHz     = params.vibRate->load();
    s.vibDelayMs    = params.vibDelay->load();
    s.humanize      = params.humanize->load();
    s.timbre        = params.timbre->load();
    s.pull          = params.pull->load();
    s.warmth        = params.warmth->load();
    s.tone          = params.tone->load();
    s.cabinet       = params.cabinet->load();
    s.noise         = params.noise->load();
    s.attackMs      = params.attack->load();
    s.releaseMs     = params.release->load();
    s.velSens       = params.velSens->load();
    s.reverbMix     = params.reverbMix->load();
    s.roomSize      = params.roomSize->load();
    s.outputDb      = params.output->load();
    return s;
}

void AetherThereminAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    auto settings = readSettings();

    if (bendRangeTouched.exchange (false))
        engine.clearBendRangeOverride();

    engine.setSettings (settings);
    engine.setGuiPerformance (airActive.load(), airPitch.load(), airLevel.load());
    engine.process (buffer, midi);

    const auto received = engine.takeReceivedBendRange();
    if (received > 0.0f)
    {
        pendingBendRange.store (received);
        triggerAsyncUpdate();
    }

    if (buffer.getNumChannels() > 0)
        scope.push (buffer.getReadPointer (0), buffer.getNumSamples());
}

void AetherThereminAudioProcessor::handleAsyncUpdate()
{
    const auto range = pendingBendRange.exchange (-1.0f);

    if (range > 0.0f)
    {
        if (auto* p = apvts.getParameter (ids::bendRange))
        {
            applyingHostBendRange.store (true);
            p->setValueNotifyingHost (p->convertTo0to1 (range));
            applyingHostBendRange.store (false);
        }
    }
}

//==============================================================================
int AetherThereminAudioProcessor::getNumPrograms()
{
    return (int) getFactoryPresets().size();
}

const juce::String AetherThereminAudioProcessor::getProgramName (int index)
{
    const auto& presets = getFactoryPresets();
    if (juce::isPositiveAndBelow (index, (int) presets.size()))
        return (presets.begin() + index)->name;
    return {};
}

void AetherThereminAudioProcessor::setCurrentProgram (int index)
{
    // Some hosts re-send the current program after restoring a project; ignoring
    // a repeat keeps the user's tweaks intact.
    if (index == currentProgram)
        return;

    loadFactoryPreset (index);
}

void AetherThereminAudioProcessor::loadFactoryPreset (int index)
{
    const auto& presets = getFactoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    currentProgram = index;
    const auto& preset = *(presets.begin() + index);

    for (auto* p : getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            float value = ranged->convertFrom0to1 (ranged->getDefaultValue());

            for (const auto& [id, v] : preset.values)
                if (ranged->getParameterID() == id)
                    value = v;

            ranged->setValueNotifyingHost (ranged->convertTo0to1 (value));
        }
    }

    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

//==============================================================================
void AetherThereminAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("version", JucePlugin_VersionString, nullptr);

    const auto ui = getSavedEditorSize();
    if (ui.x > 0 && ui.y > 0)
    {
        state.setProperty ("uiWidth", ui.x, nullptr);
        state.setProperty ("uiHeight", ui.y, nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void AetherThereminAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentProgram = juce::jlimit (0, juce::jmax (0, getNumPrograms() - 1), (int) state.getProperty ("program", 0));

            const int uiW = state.getProperty ("uiWidth", 0), uiH = state.getProperty ("uiHeight", 0);
            if (uiW > 0 && uiH > 0)
                setSavedEditorSize (uiW, uiH);
            apvts.replaceState (state);
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* AetherThereminAudioProcessor::createEditor()
{
    return new AetherThereminAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AetherThereminAudioProcessor();
}
