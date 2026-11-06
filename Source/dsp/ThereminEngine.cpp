#include "ThereminEngine.h"

#include <cmath>

namespace theremin
{

namespace
{
    constexpr double twoPi = juce::MathConstants<double>::twoPi;

    /** Two cascaded one-pole stages reach ~90 % of a step after 3.89 time constants. */
    constexpr double cascadeTo90 = 3.89;

    /** Per-sample coefficient of a one-pole smoother with the given time constant. */
    inline float onePole (double timeConstantSeconds, double sampleRate) noexcept
    {
        if (timeConstantSeconds <= 0.0)
            return 1.0f;

        return (float) (1.0 - std::exp (-1.0 / (timeConstantSeconds * sampleRate)));
    }

    inline bool isNoteOffBytes (const juce::uint8* d, int n) noexcept
    {
        if (n < 3) return false;
        const auto type = d[0] & 0xF0;
        return type == 0x80 || (type == 0x90 && d[2] == 0);
    }

    inline bool isNoteOnBytes (const juce::uint8* d, int n) noexcept
    {
        return n >= 3 && (d[0] & 0xF0) == 0x90 && d[2] != 0;
    }

    /** Mean of sqrt(1 + r^2 + 2r cos x) over a period (series of the elliptic integral). */
    inline float detectorMean (float r) noexcept
    {
        const auto r2 = r * r;
        return 1.0f + r2 * (0.25f + r2 * (1.0f / 64.0f + r2 * (1.0f / 256.0f)));
    }
}

//==============================================================================
ThereminEngine::ThereminEngine()
{
    resetControllers();
}

void ThereminEngine::prepare (double newSampleRate, int /*maximumBlockSize*/)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    dt = 1.0 / sampleRate;

    // Oversample the oscillator 4x (2x at very high host rates) to keep the
    // bright detector waveforms alias-free right up the top of the range.
    const int stages = sampleRate > 100000.0 ? 1 : 2;
    oversamplingFactor = 1 << stages;
    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        1, (size_t) stages, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampler->initProcessing ((size_t) maxChunk);
    scratch.setSize (1, maxChunk, false, true, false);

    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxChunk, 1 };

    toneFilter.prepare (spec);
    toneFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    toneFilter.setResonance (0.62f);

    // Loudspeaker-cabinet voicing: no deep bass, a woody body bump, a vocal
    // presence region, a cone-breakup dip and a gentle top roll-off.
    using Coefs = juce::dsp::IIR::Coefficients<float>;
    const auto sr = sampleRate;
    cabinetFilters[0].coefficients = Coefs::makeHighPass  (sr, 72.0f, 0.707f);
    cabinetFilters[1].coefficients = Coefs::makePeakFilter (sr, 210.0f, 0.9f, juce::Decibels::decibelsToGain (3.0f));
    cabinetFilters[2].coefficients = Coefs::makePeakFilter (sr, 1350.0f, 1.3f, juce::Decibels::decibelsToGain (3.0f));
    cabinetFilters[3].coefficients = Coefs::makePeakFilter (sr, 3300.0f, 2.0f, juce::Decibels::decibelsToGain (-2.5f));
    cabinetFilters[4].coefficients = Coefs::makeLowPass   (sr, (float) juce::jmin (7500.0, sr * 0.45), 0.6f);

    for (auto& f : cabinetFilters)
        f.prepare (spec);

    dcCoeff = (float) std::exp (-twoPi * 12.0 / sampleRate);

    toneCutoff.reset (sampleRate, 0.03);
    outputGain.reset (sampleRate, 0.03);
    cabinetMix.reset (sampleRate, 0.03);
    warmthSmoothed.reset (sampleRate, 0.03);
    timbreSmoothed.reset (sampleRate, 0.03);
    pullSmoothed.reset (sampleRate, 0.03);

    reverb.setSampleRate (sampleRate);

    reset();
}

void ThereminEngine::reset()
{
    stackSize = 0;
    gate = false;
    killFade = false;
    guiWasActive = false;
    handVel = 0.0;
    handOmega = 0.0;
    handPos = handTarget = (double) currentNote;
    bendSmoothed = 0.0f;
    bendSnapSamples = 0;
    ampEnv1 = ampEnv2 = 0.0f;
    presence = 0.0f;
    vibEnv = 0.0f;
    vibPhase = 0.0;
    noteTime = 0.0;
    theta = 0.0;
    dc1x = dc1y = dc2x = dc2y = 0.0f;
    hissState = 0.0f;
    humPhase = 0.0;

    if (oversampler != nullptr)
        oversampler->reset();

    toneFilter.reset();
    for (auto& f : cabinetFilters)
        f.reset();

    reverb.reset();

    toneCutoff.setCurrentAndTargetValue (juce::jlimit (200.0f, (float) (sampleRate * 0.45), 600.0f * std::pow (2.0f, settings.tone * 5.0f)));
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (settings.outputDb));
    cabinetMix.setCurrentAndTargetValue (settings.cabinet);
    warmthSmoothed.setCurrentAndTargetValue (settings.warmth);
    timbreSmoothed.setCurrentAndTargetValue (settings.timbre);
    pullSmoothed.setCurrentAndTargetValue (settings.pull);
}

void ThereminEngine::resetControllers()
{
    bendNorm.fill (0.0f);
    rpn.fill ({});
    modWheel = 0.0f;
    ccVolume = 1.0f;
    ccExpression = 1.0f;
}

void ThereminEngine::setGuiPerformance (bool active, float pitchSemitones, float level) noexcept
{
    guiActive = active;
    guiPitch = pitchSemitones;
    guiLevel = juce::jlimit (0.0f, 1.0f, level);
}

float ThereminEngine::takeReceivedBendRange() noexcept
{
    return receivedBendRange.exchange (-1.0f);
}

float ThereminEngine::effectiveBendRange() const noexcept
{
    return bendRangeOverride > 0.0f ? bendRangeOverride : settings.bendRange;
}

float ThereminEngine::velocityToLevel (float velocity) const noexcept
{
    const auto v = juce::jlimit (0.0f, 1.0f, velocity);
    return juce::Decibels::decibelsToGain (-30.0f * settings.velSens * (1.0f - v));
}

//==============================================================================
void ThereminEngine::startMotion (double target, double motionTimeSeconds)
{
    handTarget = target;

    if (motionTimeSeconds < 0.0005)
    {
        handOmega = 0.0;
        handPos = target;
        handVel = 0.0;
        return;
    }

    handOmega = cascadeTo90 / motionTimeSeconds;
    handZeta  = 1.0 - 0.45 * (double) juce::jlimit (0.0f, 1.0f, settings.overshoot);
}

void ThereminEngine::noteOn (int note, int channel, float velocity)
{
    // remove a duplicate of this key (e.g. a retrigger without note-off)
    for (int i = 0; i < stackSize; ++i)
    {
        if (stack[(size_t) i].note == note && stack[(size_t) i].channel == channel)
        {
            for (int j = i; j < stackSize - 1; ++j)
                stack[(size_t) j] = stack[(size_t) j + 1];
            --stackSize;
            break;
        }
    }

    const bool legato = stackSize > 0;

    if (stackSize < (int) stack.size())
        stack[(size_t) stackSize++] = { note, channel, velocity };

    currentNote = note;
    currentChannel = channel;
    noteLevel = velocityToLevel (velocity);
    killFade = false;

    if (legato)
    {
        // Overlapping notes: the pitch hand travels to the new note while the
        // volume hand stays where it is -> a true slide, no re-articulation.
        startMotion ((double) note, settings.glideMs * 0.001);
    }
    else
    {
        const bool glideFromLast = settings.glideAlways && hasPlayedBefore && settings.glideMs > 0.5f;

        if (glideFromLast)
        {
            startMotion ((double) note, settings.glideMs * 0.001);
        }
        else
        {
            // A thereminist lands on a fresh note from slightly below.
            const double scoopSemis = 0.8 * (double) settings.scoop;
            handPos = (double) note - scoopSemis;
            handVel = 0.0;
            startMotion ((double) note, scoopSemis > 0.001 ? 0.07 + 0.06 * (double) settings.scoop : 0.0);
        }

        // A new phrase must start exactly on pitch: jump the bend smoother and
        // keep snapping for a few ms so a host bend-reset sent alongside the
        // note (in either order) never produces an audible swoop.
        bendSmoothed = bendNorm[(size_t) channel] * effectiveBendRange();
        bendSnapSamples = juce::jmax (1, (int) (0.004 * sampleRate));
        noteTime = 0.0;

        if (ampEnv2 < 1.0e-3f)
            vibEnv = 0.0f;
    }

    gate = true;
    hasPlayedBefore = true;
}

void ThereminEngine::noteOff (int note, int channel)
{
    int index = -1;

    for (int i = stackSize; --i >= 0;)
        if (stack[(size_t) i].note == note && stack[(size_t) i].channel == channel) { index = i; break; }

    if (index < 0) // tolerate hosts that send the note-off on another channel
        for (int i = stackSize; --i >= 0;)
            if (stack[(size_t) i].note == note) { index = i; break; }

    if (index < 0)
        return;

    const bool wasCurrent = stack[(size_t) index].note == currentNote
                         && stack[(size_t) index].channel == currentChannel;

    for (int j = index; j < stackSize - 1; ++j)
        stack[(size_t) j] = stack[(size_t) j + 1];
    --stackSize;

    if (stackSize == 0)
    {
        gate = guiActive;
        return;
    }

    if (wasCurrent && settings.returnToHeld)
    {
        const auto& top = stack[(size_t) stackSize - 1];
        currentNote = top.note;
        currentChannel = top.channel;
        noteLevel = velocityToLevel (top.velocity);
        startMotion ((double) top.note, settings.glideMs * 0.001);
    }
    // otherwise "stay": the pitch remains on the released note, exactly like
    // FL Studio's native slide notes, until the remaining notes end.
}

void ThereminEngine::allNotesOff (bool killImmediately)
{
    stackSize = 0;
    gate = guiActive;
    if (killImmediately)
        killFade = true;
}

void ThereminEngine::setReceivedBendRange (float semitones)
{
    const auto range = juce::jlimit (1.0f, 48.0f, semitones);
    bendRangeOverride = range;
    receivedBendRange.store (range);
}

void ThereminEngine::handleController (int channel, int cc, int value)
{
    auto& r = rpn[(size_t) channel];

    switch (cc)
    {
        case 1:   modWheel = (float) value / 127.0f; break;
        case 11:  ccExpression = std::pow ((float) value / 127.0f, 1.66f); break;

        // RPN 0 = pitch-bend sensitivity (sent by FL Studio when the wrapper's
        // "Send pitch bend range" option is enabled, and by MPE controllers).
        case 101: r.msb = value; r.nrpn = false; break;
        case 100: r.lsb = value; r.nrpn = false; break;
        case 99:
        case 98:  r.nrpn = true; break;
        case 6:
            if (! r.nrpn && r.msb == 0 && r.lsb == 0 && value > 0)
            {
                r.dataMsb = value;
                setReceivedBendRange ((float) value);
            }
            break;
        case 38:
            if (! r.nrpn && r.msb == 0 && r.lsb == 0 && r.dataMsb > 0)
                setReceivedBendRange ((float) r.dataMsb + (float) value / 100.0f);
            break;

        case 120: allNotesOff (true); break;
        case 121: resetControllers(); break;
        case 123:
        case 124: case 125: case 126: case 127:
                  allNotesOff (false); break;
        default:  break;
    }
}

void ThereminEngine::handleMidiEvent (const juce::MidiMessage& m)
{
    const auto* d = m.getRawData();
    const auto n = m.getRawDataSize();

    if (n < 2 || d[0] < 0x80 || d[0] >= 0xF0)
        return;

    const int type = d[0] & 0xF0;
    const int channel = d[0] & 0x0F;

    switch (type)
    {
        case 0x90:
            if (n >= 3 && d[2] > 0) noteOn (d[1], channel, (float) d[2] / 127.0f);
            else if (n >= 3)        noteOff (d[1], channel);
            break;

        case 0x80:
            noteOff (d[1], channel);
            break;

        case 0xB0:
            if (n >= 3) handleController (channel, d[1], d[2]);
            break;

        case 0xE0:
            if (n >= 3)
            {
                const int value = (int) d[1] | ((int) d[2] << 7);
                // asymmetric scaling so that 0 -> -1 and 16383 -> +1 exactly
                bendNorm[(size_t) channel] = value >= 8192 ? (float) (value - 8192) / 8191.0f
                                                           : (float) (value - 8192) / 8192.0f;
            }
            break;

        default:
            break;
    }
}

//==============================================================================
void ThereminEngine::process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi)
{
    const int numSamples = buffer.getNumSamples();

    // --- per-block parameter targets ----------------------------------------
    toneCutoff.setTargetValue (juce::jlimit (200.0f, (float) (sampleRate * 0.45), 600.0f * std::pow (2.0f, settings.tone * 5.0f)));
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (settings.outputDb));
    cabinetMix.setTargetValue (settings.cabinet);
    warmthSmoothed.setTargetValue (settings.warmth);
    timbreSmoothed.setTargetValue (settings.timbre);
    pullSmoothed.setTargetValue (settings.pull);

    juce::Reverb::Parameters rp;
    // A sustained, nearly sinusoidal tone builds up strongly in a long room,
    // so the wet gain is kept modest and the dry level dips a little with mix.
    rp.roomSize   = 0.3f + 0.6f * settings.roomSize;
    rp.damping    = 0.5f;
    rp.wetLevel   = 0.24f * settings.reverbMix;
    rp.dryLevel   = 0.5f * (1.0f - 0.35f * settings.reverbMix);
    rp.width      = 0.9f;
    rp.freezeMode = 0.0f;
    reverb.setParameters (rp);

    telemetry.bendRange.store (effectiveBendRange(), std::memory_order_relaxed);

    // --- mouse-played "air" performance --------------------------------------
    if (guiActive)
    {
        if (! guiWasActive && ampEnv2 < 1.0e-3f)
        {
            handPos = guiPitch;
            handVel = 0.0;
            noteTime = 0.0;
            vibEnv = 0.0f;
            bendSnapSamples = juce::jmax (1, (int) (0.004 * sampleRate));
        }

        handTarget = guiPitch;
        handOmega = cascadeTo90 / 0.035;
        handZeta = 1.0;
        gate = true;
        killFade = false;
    }
    else if (guiWasActive)
    {
        if (stackSize > 0)
            startMotion ((double) currentNote, settings.glideMs * 0.001);
        else
            gate = false;
    }

    guiWasActive = guiActive;

    // --- walk the MIDI, rendering sample-accurately between event groups ----
    int position = 0;
    auto it = midi.cbegin();
    const auto end = midi.cend();

    constexpr int maxGroup = 64;
    std::array<juce::MidiMessageMetadata, maxGroup> group;

    while (it != end)
    {
        const int eventPos = juce::jlimit (0, numSamples, (*it).samplePosition);

        if (eventPos > position)
        {
            renderSegment (buffer, position, eventPos - position);
            position = eventPos;
        }

        // Gather every event at this exact sample position.
        int groupSize = 0;
        while (it != end && juce::jlimit (0, numSamples, (*it).samplePosition) == eventPos && groupSize < maxGroup)
        {
            group[(size_t) groupSize++] = *it;
            ++it;
        }

        // Hosts disagree on the order of a note-off and a note-on that share a
        // timestamp. Releasing notes first means back-to-back notes articulate
        // while genuinely overlapping notes slide - independent of host order.
        std::array<bool, maxGroup> done {};

        for (int i = 0; i < groupSize; ++i)
        {
            const auto& e = group[(size_t) i];

            if (! isNoteOffBytes (e.data, e.numBytes))
                continue;

            bool startedInGroup = false;
            for (int j = 0; j < i; ++j)
            {
                const auto& o = group[(size_t) j];
                if (isNoteOnBytes (o.data, o.numBytes) && o.data[1] == e.data[1] && (o.data[0] & 0x0F) == (e.data[0] & 0x0F))
                    startedInGroup = true;
            }

            if (! startedInGroup)
            {
                handleMidiEvent (e.getMessage());
                done[(size_t) i] = true;
            }
        }

        for (int i = 0; i < groupSize; ++i)
            if (! done[(size_t) i])
                handleMidiEvent (group[(size_t) i].getMessage());
    }

    if (position < numSamples)
        renderSegment (buffer, position, numSamples - position);
}

void ThereminEngine::renderSegment (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    const int numChannels = buffer.getNumChannels();

    if (numChannels <= 0)
        return;

    auto* left  = buffer.getWritePointer (0);
    auto* right = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    int done = 0;
    while (done < numSamples)
    {
        const int n = juce::jmin (maxChunk, numSamples - done);
        renderChunk (left + startSample + done, right != nullptr ? right + startSample + done : nullptr, n);
        done += n;
    }

    for (int ch = 2; ch < numChannels; ++ch)
        buffer.copyFrom (ch, startSample, buffer, 0, startSample, numSamples);
}

void ThereminEngine::renderChunk (float* left, float* right, int n)
{
    // ------------------------------------------------------------------------
    // 1) control rate (one value per output sample): hands, vibrato, envelopes
    // ------------------------------------------------------------------------
    const float bendRange   = effectiveBendRange();
    const float bendCoeff   = onePole (0.003, sampleRate);
    const float attackCoeff = onePole (juce::jmax (1.0f, settings.attackMs) * 0.001 / cascadeTo90, sampleRate);
    const float releaseCoeff = killFade ? onePole (0.003 / cascadeTo90, sampleRate)
                                        : onePole (juce::jmax (5.0f, settings.releaseMs) * 0.001 / cascadeTo90, sampleRate);
    const float vibFadeCoeff = onePole (0.22, sampleRate);
    const float ccCoeff      = onePole (0.012, sampleRate);
    const float presenceUp   = onePole (0.05, sampleRate);
    const float presenceDown = onePole (1.5, sampleRate);
    const double vibDelay    = settings.vibDelayMs * 0.001;
    const float human        = juce::jlimit (0.0f, 1.0f, settings.humanize);

    // Ornstein-Uhlenbeck random processes for thermal drift, hand tremor and
    // vibrato irregularity (a human never holds a perfectly steady LFO).
    auto ou = [this] (float& state, double tau, float sigma)
    {
        const auto a = (float) (dt / tau);
        const auto s = (float) std::sqrt (2.0 * dt / tau);
        state += -state * a + sigma * s * gaussian();
    };

    const double baseOffset = (double) settings.octave * 12.0 + (double) settings.fineCents * 0.01;
    const auto nyquistGuard = (float) (sampleRate * 0.45);
    double handSemis = handPos;

    for (int i = 0; i < n; ++i)
    {
        // pitch hand: damped 2nd-order system -> human S-curve glides
        if (handOmega <= 0.0)
        {
            handPos = handTarget;
            handVel = 0.0;
        }
        else
        {
            const double accel = handOmega * handOmega * (handTarget - handPos) - 2.0 * handZeta * handOmega * handVel;
            handVel += accel * dt;
            handPos += handVel * dt;

            if (std::abs (handTarget - handPos) < 1.0e-7 && std::abs (handVel) < 1.0e-6)
            {
                handPos = handTarget;
                handVel = 0.0;
            }
        }

        // pitch bend (per MIDI channel)
        const float bendTarget = bendNorm[(size_t) currentChannel] * bendRange;
        if (bendSnapSamples > 0 || ampEnv2 < 1.0e-5f)
        {
            bendSmoothed = bendTarget;
            if (bendSnapSamples > 0)
                --bendSnapSamples;
        }
        else
        {
            bendSmoothed += (bendTarget - bendSmoothed) * bendCoeff;
        }

        // humanisation
        ou (driftCents,  2.5,  5.0f  * human);
        ou (tremorCents, 0.06, 1.2f  * human);
        ou (rateJitter,  1.2,  0.07f * human);
        ou (depthJitter, 0.9,  0.22f * human);

        // vibrato: delayed onset, faded in, eased off while the hand travels
        noteTime += dt;
        const auto speed = (float) std::abs (handVel);       // semitones / second
        const float suppress = 1.0f / (1.0f + (speed * 0.1f) * (speed * 0.1f));
        const float vibTarget = (noteTime >= vibDelay ? 1.0f : 0.0f) * suppress;
        vibEnv += (vibTarget - vibEnv) * vibFadeCoeff;

        vibPhase += twoPi * (double) settings.vibRateHz * (1.0 + (double) rateJitter) * dt;
        if (vibPhase >= twoPi)
            vibPhase -= twoPi;

        const float depth = (settings.vibDepthCents + modWheel * 60.0f) * juce::jmax (0.0f, 1.0f + depthJitter);
        const float vibCents = (float) std::sin (vibPhase) * depth * vibEnv;

        handSemis = handPos + (double) bendSmoothed + baseOffset;
        const double pitch = handSemis + (double) (vibCents + driftCents + tremorCents) * 0.01;

        chunkFreq[(size_t) i] = juce::jlimit (4.0f, nyquistGuard, (float) (440.0 * std::exp2 ((pitch - 69.0) / 12.0)));

        // volume hand
        const float levelTarget = gate ? (guiActive ? guiLevel : noteLevel) : 0.0f;
        const float coeff = levelTarget > ampEnv2 ? attackCoeff : releaseCoeff;
        ampEnv1 += (levelTarget - ampEnv1) * coeff;
        ampEnv2 += (ampEnv1 - ampEnv2) * coeff;

        if (killFade && ! gate && ampEnv2 < 1.0e-6f)
            killFade = false;

        ccGainSmoothed += (ccVolume * ccExpression - ccGainSmoothed) * ccCoeff;
        chunkAmp[(size_t) i] = ampEnv2 * ccGainSmoothed;

        const float presenceTarget = (gate || ampEnv2 > 1.0e-4f) ? 1.0f : 0.0f;
        presence += (presenceTarget - presence) * (presenceTarget > presence ? presenceUp : presenceDown);
    }

    // ------------------------------------------------------------------------
    // 2) heterodyne oscillator at the oversampled rate
    // ------------------------------------------------------------------------
    auto* base = scratch.getWritePointer (0);
    juce::FloatVectorOperations::clear (base, n);

    float* channels[] = { base };
    juce::dsp::AudioBlock<float> baseBlock (channels, 1, (size_t) n);
    auto upBlock = oversampler->processSamplesUp (baseBlock);
    auto* up = upBlock.getChannelPointer (0);

    timbreSmoothed.skip (n);
    pullSmoothed.skip (n);

    // detector level ratio r: ~0 = pure sine, ->1 = bright rectified "voice"
    const float timbre = timbreSmoothed.getCurrentValue();
    const auto r = (double) (0.02f + 0.955f * std::pow (juce::jlimit (0.0f, 1.0f, timbre), 0.85f));
    const auto dcEst = (double) detectorMean ((float) r);
    const auto rInv = 1.0 / r;
    const auto rr = 1.0 + r * r;

    // coupling strength of the two RF oscillators (Adler's K), in rad/s
    const double K = twoPi * 80.0 * (double) pullSmoothed.getCurrentValue();
    const double K2 = K * K;

    const int os = oversamplingFactor;
    const double dtOs = dt / (double) os;
    const double invOs = 1.0 / (double) os;
    double fPrev = (double) lastFreq;

    for (int i = 0; i < n; ++i)
    {
        const double fNext = (double) chunkFreq[(size_t) i];

        for (int k = 0; k < os; ++k)
        {
            const double f = fPrev + (fNext - fPrev) * (double) (k + 1) * invOs;
            const double omega = twoPi * f;

            // Uniform phase of the beat; the actual RF phase difference is the
            // exact solution of Adler's equation with detuning sqrt(w^2 + K^2),
            // which keeps the average beat frequency exactly on pitch.
            theta += omega * dtOs;
            if (theta >= twoPi)
                theta -= twoPi;

            const double half = 0.5 * theta;
            const double c = std::cos (half);
            const double s = std::sin (half);
            const double detune = std::sqrt (omega * omega + K2);
            const double X = detune * c;
            const double Y = K * c + omega * s;
            const double X2 = X * X, Y2 = Y * Y;
            const double cosPhi = (X2 - Y2) / (X2 + Y2 + 1.0e-30);

            // envelope detector on the summed RF signal
            const double env = std::sqrt (juce::jmax (0.0, rr + 2.0 * r * cosPhi));
            up[(size_t) (i * os + k)] = (float) ((env - dcEst) * rInv);
        }

        fPrev = fNext;
    }

    lastFreq = (float) fPrev;
    oversampler->processSamplesDown (baseBlock);

    // ------------------------------------------------------------------------
    // 3) volume antenna VCA -> tube output stage -> tone -> cabinet -> space
    // ------------------------------------------------------------------------
    warmthSmoothed.skip (n);
    const float warmth = juce::jlimit (0.0f, 1.0f, warmthSmoothed.getCurrentValue());
    const float tubeGain = 1.0f + 3.0f * warmth;
    const float tubeBias = 0.25f * warmth;
    const float tubeBiasT = std::tanh (tubeBias);
    const float tubeNorm = 1.0f / (std::tanh (tubeGain + tubeBias) - tubeBiasT);

    toneFilter.setCutoffFrequency (toneCutoff.skip (n));

    const float noiseLevel = 0.012f * settings.noise * settings.noise;
    const double humInc = twoPi * 50.0 * dt;

    for (int i = 0; i < n; ++i)
    {
        float x = base[i];

        // DC blocker (detector output carries a level-dependent DC component)
        float y = x - dc1x + dcCoeff * dc1y;
        dc1x = x;
        dc1y = y;

        // volume antenna
        float v = y * chunkAmp[(size_t) i];

        // tube stage: biased tanh -> even harmonics that grow with loudness
        const float sat = (std::tanh (tubeGain * v + tubeBias) - tubeBiasT) * tubeNorm;
        v += warmth * (sat - v);

        y = v - dc2x + dcCoeff * dc2y;
        dc2x = v;
        dc2y = y;
        v = y;

        v = toneFilter.processSample (0, v);

        float cab = v;
        for (auto& f : cabinetFilters)
            cab = f.processSample (cab);

        const float cabAmt = cabinetMix.getNextValue();
        v += cabAmt * (cab - v);

        if (noiseLevel > 0.0f)
        {
            humPhase += humInc;
            if (humPhase >= twoPi)
                humPhase -= twoPi;

            hissState += (uniform() * 2.0f - 1.0f - hissState) * 0.35f;
            const auto hum = 0.65f * (float) std::sin (humPhase) + 0.35f * (float) std::sin (2.0 * humPhase);
            v += noiseLevel * presence * (hum + 0.5f * hissState);
        }

        v *= outputGain.getNextValue();

        if (! std::isfinite (v))
        {
            v = 0.0f;
            toneFilter.reset();
            for (auto& f : cabinetFilters)
                f.reset();
            dc1x = dc1y = dc2x = dc2y = 0.0f;
        }

        chunkMono[(size_t) i] = v;
    }

    std::copy (chunkMono.begin(), chunkMono.begin() + n, left);

    if (right != nullptr)
    {
        std::copy (chunkMono.begin(), chunkMono.begin() + n, right);
        reverb.processStereo (left, right, n);
    }
    else
    {
        reverb.processMono (left, n);
    }

    // ------------------------------------------------------------------------
    // 4) telemetry for the editor
    // ------------------------------------------------------------------------
    telemetry.handPitch.store ((float) handSemis, std::memory_order_relaxed);
    telemetry.fullPitch.store ((float) (12.0 * std::log2 (juce::jmax (1.0f, lastFreq) / 440.0) + 69.0), std::memory_order_relaxed);
    telemetry.frequency.store (lastFreq, std::memory_order_relaxed);
    telemetry.level.store (chunkAmp[(size_t) (n - 1)], std::memory_order_relaxed);
}

//==============================================================================
float ThereminEngine::uniform() noexcept
{
    // xorshift32
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return (float) (rngState >> 8) * (1.0f / 16777216.0f);
}

float ThereminEngine::gaussian() noexcept
{
    // Irwin-Hall approximation (sum of 4 uniforms), unit variance
    const float sum = uniform() + uniform() + uniform() + uniform();
    return (sum - 2.0f) * 1.7320508f;
}

} // namespace theremin
