#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Parameters.h"
#include "dsp/ThereminEngine.h"

#include <array>
#include <atomic>

//==============================================================================
/** Lock-free ring buffer the audio thread writes and the oscilloscope reads. */
class ScopeBuffer
{
public:
    static constexpr int size = 4096;

    void push (const float* samples, int numSamples) noexcept
    {
        auto w = writeIndex.load (std::memory_order_relaxed);
        for (int i = 0; i < numSamples; ++i)
        {
            data[(size_t) w].store (samples[i], std::memory_order_relaxed);
            w = (w + 1) & (size - 1);
        }
        writeIndex.store (w, std::memory_order_release);
    }

    /** Copies the most recent numSamples (<= size) into dest, oldest first. */
    void copyLatest (float* dest, int numSamples) const noexcept
    {
        numSamples = juce::jmin (numSamples, size);
        auto r = (writeIndex.load (std::memory_order_acquire) - numSamples) & (size - 1);
        for (int i = 0; i < numSamples; ++i)
        {
            dest[i] = data[(size_t) r].load (std::memory_order_relaxed);
            r = (r + 1) & (size - 1);
        }
    }

private:
    std::array<std::atomic<float>, size> data {};
    std::atomic<int> writeIndex { 0 };
};

//==============================================================================
class AetherThereminAudioProcessor final : public juce::AudioProcessor,
                                           private juce::AsyncUpdater,
                                           private juce::AudioProcessorValueTreeState::Listener
{
public:
    AetherThereminAudioProcessor();
    ~AetherThereminAudioProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==========================================================================
    const juce::String getName() const override { return "Aether Theremin"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    //==========================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    /** Applies a factory preset even if it is already the current one. */
    void loadFactoryPreset (int index);

    //==========================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;

    theremin::EngineTelemetry& getTelemetry() noexcept { return engine.getTelemetry(); }
    ScopeBuffer& getScope() noexcept { return scope; }
    double getSampleRateForDisplay() const noexcept { return lastSampleRate.load(); }

    /** Window size the user last chose; saved with the project. {0, 0} = never resized. */
    juce::Point<int> getSavedEditorSize() const noexcept { return { savedEditorWidth.load(), savedEditorHeight.load() }; }
    void setSavedEditorSize (int width, int height) noexcept
    {
        savedEditorWidth.store (width);
        savedEditorHeight.store (height);
    }

    /** Mouse "air" playing from the editor. */
    void setAirPlay (bool active, float pitchSemitones, float level) noexcept
    {
        airPitch.store (pitchSemitones);
        airLevel.store (level);
        airActive.store (active);
    }

private:
    void handleAsyncUpdate() override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    theremin::EngineSettings readSettings();

    struct ParamPointers
    {
        std::atomic<float>* glide {}; std::atomic<float>* glideMode {}; std::atomic<float>* noteRelease {};
        std::atomic<float>* overshoot {}; std::atomic<float>* scoop {}; std::atomic<float>* bendRange {};
        std::atomic<float>* octave {}; std::atomic<float>* fineTune {};
        std::atomic<float>* vibDepth {}; std::atomic<float>* vibRate {}; std::atomic<float>* vibDelay {};
        std::atomic<float>* humanize {};
        std::atomic<float>* timbre {}; std::atomic<float>* pull {}; std::atomic<float>* warmth {};
        std::atomic<float>* tone {}; std::atomic<float>* cabinet {}; std::atomic<float>* noise {};
        std::atomic<float>* attack {}; std::atomic<float>* release {}; std::atomic<float>* velSens {};
        std::atomic<float>* reverbMix {}; std::atomic<float>* roomSize {}; std::atomic<float>* output {};
    } params;

    theremin::ThereminEngine engine;
    ScopeBuffer scope;

    std::atomic<bool> airActive { false };
    std::atomic<float> airPitch { 60.0f }, airLevel { 0.0f };

    std::atomic<float> pendingBendRange { -1.0f };
    std::atomic<bool> bendRangeTouched { false }, applyingHostBendRange { false };

    std::atomic<double> lastSampleRate { 44100.0 };
    std::atomic<int> savedEditorWidth { 0 }, savedEditorHeight { 0 };
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AetherThereminAudioProcessor)
};
