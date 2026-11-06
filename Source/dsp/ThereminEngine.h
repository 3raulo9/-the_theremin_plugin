#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cstdint>

namespace theremin
{

//==============================================================================
/** Plain snapshot of every user-facing setting, filled once per audio block. */
struct EngineSettings
{
    // pitch hand
    float glideMs        = 90.0f;
    bool  glideAlways    = false;   // false = glide only between overlapping (legato) notes
    bool  returnToHeld   = false;   // false = "stay" on the last pitch (FL Studio slide semantics)
    float overshoot      = 0.3f;
    float scoop          = 0.2f;
    float bendRange      = 12.0f;   // semitones, must match the host's wrapper setting
    int   octave         = 0;
    float fineCents      = 0.0f;

    // vibrato / human hand
    float vibDepthCents  = 18.0f;
    float vibRateHz      = 5.6f;
    float vibDelayMs     = 220.0f;
    float humanize       = 0.35f;

    // heterodyne tone
    float timbre         = 0.45f;
    float pull           = 0.3f;
    float warmth         = 0.3f;
    float tone           = 0.7f;
    float cabinet        = 0.6f;
    float noise          = 0.0f;

    // volume hand
    float attackMs       = 35.0f;
    float releaseMs      = 220.0f;
    float velSens        = 0.4f;

    // space / output
    float reverbMix      = 0.18f;
    float roomSize       = 0.62f;
    float outputDb       = -4.0f;
};

//==============================================================================
/** Values the audio thread publishes for the editor (all relaxed atomics). */
struct EngineTelemetry
{
    std::atomic<float> handPitch { 69.0f };  // semitones, without vibrato / drift
    std::atomic<float> fullPitch { 69.0f };  // semitones, everything included
    std::atomic<float> frequency { 440.0f };
    std::atomic<float> level     { 0.0f };   // 0..1 amplitude of the volume hand
    std::atomic<float> bendRange { 12.0f };  // the bend range currently in effect
};

//==============================================================================
/**
    A monophonic, continuously-gliding theremin voice.

    Sound generation follows the real instrument:
      * Two radio-frequency oscillators are mixed; the audible tone is their
        difference ("beat") frequency.
      * The oscillators are weakly coupled, so their phase difference obeys
        Adler's equation  dphi/dt = dw - K sin(phi).  We use its exact closed-form
        solution, which reproduces the characteristic waveform skew ("pulling")
        that makes low theremin notes reedy and high notes pure.
      * The beat is recovered by an envelope detector on the summed RF signal:
        sqrt(1 + r^2 + 2 r cos(phi)). The level ratio r morphs the tone from a
        pure sine (r -> 0) towards the bright, nasal rectified shape (r -> 1).
      * A tube output stage, tone control and loudspeaker-cabinet voicing follow.

    Performance modelling: a 2nd-order "hand" model for pitch glides
    (natural S-curves, optional overshoot), delayed and humanised vibrato,
    thermal drift, and a smooth volume-hand envelope that never retriggers
    during legato.

    MIDI: last-note priority note stack, legato slides, per-channel 14-bit pitch
    bend (works with plain MIDI, FL Studio's channel pitch / note-colour channels
    and MPE), RPN 0 bend-range detection, mod wheel -> vibrato, CC7/CC11 volume,
    all-notes-off / all-sound-off / reset-controllers.
*/
class ThereminEngine
{
public:
    ThereminEngine();

    void prepare (double sampleRate, int maximumBlockSize);
    void reset();

    void setSettings (const EngineSettings& newSettings) noexcept { settings = newSettings; }

    /** Mouse-played "air" performance from the editor. Pitch in semitones (float). */
    void setGuiPerformance (bool active, float pitchSemitones, float level) noexcept;

    /** Renders into all channels of the buffer (the buffer is overwritten). */
    void process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi);

    /** Returns a bend range received via RPN 0 since the last call, or -1. */
    float takeReceivedBendRange() noexcept;

    /** Clears an RPN bend-range override (called when the user moves the parameter). */
    void clearBendRangeOverride() noexcept { bendRangeOverride = -1.0f; }

    EngineTelemetry& getTelemetry() noexcept { return telemetry; }

    bool isSounding() const noexcept { return gate || ampEnv2 > 1.0e-5f; }

private:
    //==========================================================================
    struct HeldNote { int note = 0; int channel = 0; float velocity = 0.0f; };
    struct RpnState { int msb = 0; int lsb = 0; bool nrpn = false; int dataMsb = -1; };

    void handleMidiEvent (const juce::MidiMessage& m);
    void noteOn (int note, int channel, float velocity);
    void noteOff (int note, int channel);
    void allNotesOff (bool killImmediately);
    void resetControllers();
    void handleController (int channel, int cc, int value);
    void setReceivedBendRange (float semitones);

    void startMotion (double targetSemitones, double motionTimeSeconds);
    float velocityToLevel (float velocity) const noexcept;
    float effectiveBendRange() const noexcept;

    void renderSegment (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void renderChunk (float* left, float* right, int numSamples);

    float gaussian() noexcept;
    float uniform() noexcept;

    //==========================================================================
    EngineSettings settings;
    EngineTelemetry telemetry;

    double sampleRate = 44100.0;
    double dt = 1.0 / 44100.0;
    int oversamplingFactor = 4;

    static constexpr int maxChunk = 128;

    // note stack (last-note priority)
    std::array<HeldNote, 128> stack {};
    int stackSize = 0;
    int currentNote = 69;
    int currentChannel = 0;
    bool gate = false;
    bool hasPlayedBefore = false;
    float noteLevel = 0.0f;

    // GUI "air" performance
    bool guiActive = false, guiWasActive = false;
    float guiPitch = 60.0f, guiLevel = 0.0f;

    // pitch hand dynamics (2nd-order system, semitones)
    double handPos = 69.0, handVel = 0.0, handTarget = 69.0;
    double handOmega = 0.0, handZeta = 1.0;

    // pitch bend, per MIDI channel, normalised -1..+1
    std::array<float, 16> bendNorm {};
    float bendSmoothed = 0.0f;
    int bendSnapSamples = 0;
    float bendRangeOverride = -1.0f;
    std::atomic<float> receivedBendRange { -1.0f };
    std::array<RpnState, 16> rpn {};

    // controllers
    float modWheel = 0.0f;
    float ccVolume = 1.0f, ccExpression = 1.0f;
    float ccGainSmoothed = 1.0f;

    // vibrato & humanisation
    double vibPhase = 0.0;
    float vibEnv = 0.0f;
    double noteTime = 0.0;
    float driftCents = 0.0f, tremorCents = 0.0f, tremorFast = 0.0f;
    float rateJitter = 0.0f, depthJitter = 0.0f;
    uint32_t rngState = 0x9E3779B9u;

    // volume hand
    float ampEnv1 = 0.0f, ampEnv2 = 0.0f;
    bool killFade = false;
    float presence = 0.0f;

    // oscillator
    double theta = 0.0;            // uniform beat phase
    float lastFreq = 440.0f;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::AudioBuffer<float> scratch;

    // output chain
    float dc1x = 0.0f, dc1y = 0.0f, dc2x = 0.0f, dc2y = 0.0f, dcCoeff = 0.999f;
    juce::dsp::StateVariableTPTFilter<float> toneFilter;
    std::array<juce::dsp::IIR::Filter<float>, 5> cabinetFilters;
    juce::SmoothedValue<float> toneCutoff, outputGain, cabinetMix, warmthSmoothed, timbreSmoothed, pullSmoothed;
    double humPhase = 0.0;
    float hissState = 0.0f;
    juce::Reverb reverb;

    // per-chunk control-rate scratch
    std::array<float, maxChunk> chunkFreq {}, chunkAmp {}, chunkMono {};
};

} // namespace theremin
