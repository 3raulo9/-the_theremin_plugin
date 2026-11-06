#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include <functional>
#include <iostream>
#include <vector>

namespace testutils
{
    //==========================================================================
    struct Checker
    {
        int passed = 0, failed = 0;

        bool check (bool ok, const juce::String& name, const juce::String& detail = {})
        {
            (ok ? passed : failed)++;
            std::cout << (ok ? "  [PASS] " : "  [FAIL] ") << name;
            if (detail.isNotEmpty())
                std::cout << "  (" << detail << ")";
            std::cout << std::endl;
            return ok;
        }

        int summary() const
        {
            std::cout << "\n" << passed << " passed, " << failed << " failed" << std::endl;
            return failed == 0 ? 0 : 1;
        }
    };

    //==========================================================================
    struct TimedEvent
    {
        double timeSeconds;
        juce::MidiMessage message;
    };

    /** Renders events through a processor, using either a fixed block size or a
        callback that picks each block's size (to emulate hosts like FL Studio
        that vary the buffer length). Returns channel 0 (and 1) of the output. */
    inline juce::AudioBuffer<float> render (juce::AudioProcessor& proc, double sampleRate,
                                            std::vector<TimedEvent> events, double seconds,
                                            std::function<int (int)> blockSizeFor = nullptr,
                                            int fixedBlock = 512,
                                            std::function<void (int sampleOffset)> beforeBlock = nullptr)
    {
        const int total = (int) std::ceil (seconds * sampleRate);
        const int outChannels = juce::jmax (1, proc.getTotalNumOutputChannels());
        juce::AudioBuffer<float> out (outChannels, total);
        out.clear();

        std::stable_sort (events.begin(), events.end(), [] (const auto& a, const auto& b) { return a.timeSeconds < b.timeSeconds; });

        size_t next = 0;
        int pos = 0, blockIndex = 0;
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> block;

        while (pos < total)
        {
            int n = blockSizeFor ? blockSizeFor (blockIndex) : fixedBlock;
            n = juce::jlimit (1, total - pos, n);
            ++blockIndex;

            midi.clear();
            while (next < events.size())
            {
                const auto samplePos = (int) std::llround (events[next].timeSeconds * sampleRate);
                if (samplePos >= pos + n)
                    break;
                midi.addEvent (events[next].message, juce::jmax (0, samplePos - pos));
                ++next;
            }

            if (beforeBlock)
                beforeBlock (pos);

            block.setSize (juce::jmax (outChannels, proc.getTotalNumInputChannels()), n, false, false, true);
            block.clear();
            proc.processBlock (block, midi);

            for (int ch = 0; ch < outChannels; ++ch)
                out.copyFrom (ch, pos, block, ch, 0, n);

            pos += n;
        }

        return out;
    }

    //==========================================================================
    /** YIN-style fundamental frequency estimate of channel 0 over [start, start+length). */
    inline double estimatePitch (const juce::AudioBuffer<float>& buf, double sampleRate, double startSeconds, double lengthSeconds,
                                 double minHz = 40.0, double maxHz = 2500.0)
    {
        const int start = (int) (startSeconds * sampleRate);
        const int len = juce::jmin ((int) (lengthSeconds * sampleRate), buf.getNumSamples() - start);
        if (start < 0 || len < 64)
            return 0.0;

        const auto* x = buf.getReadPointer (0) + start;
        const int maxLag = juce::jmin (len / 2, (int) (sampleRate / minHz));
        const int minLag = juce::jmax (2, (int) (sampleRate / maxHz));
        const int w = len - maxLag;
        if (w < 32)
            return 0.0;

        std::vector<double> d ((size_t) maxLag + 2, 0.0);
        for (int tau = 1; tau <= maxLag + 1; ++tau)
        {
            double sum = 0.0;
            for (int i = 0; i < w; ++i)
            {
                const double diff = (double) x[i] - (double) x[i + tau];
                sum += diff * diff;
            }
            d[(size_t) tau] = sum;
        }

        // cumulative mean normalised difference
        std::vector<double> cmnd (d.size(), 1.0);
        double running = 0.0;
        for (int tau = 1; tau <= maxLag + 1; ++tau)
        {
            running += d[(size_t) tau];
            cmnd[(size_t) tau] = running > 0.0 ? d[(size_t) tau] * tau / running : 1.0;
        }

        int best = -1;
        for (int tau = minLag; tau <= maxLag; ++tau)
        {
            if (cmnd[(size_t) tau] < 0.12)
            {
                while (tau + 1 <= maxLag && cmnd[(size_t) tau + 1] < cmnd[(size_t) tau])
                    ++tau;
                best = tau;
                break;
            }
        }

        if (best < 0)
        {
            double m = 1e9;
            for (int tau = minLag; tau <= maxLag; ++tau)
                if (cmnd[(size_t) tau] < m) { m = cmnd[(size_t) tau]; best = tau; }
        }

        // parabolic interpolation on the raw difference function
        double tauRefined = best;
        if (best > 1 && best < maxLag)
        {
            const double a = d[(size_t) best - 1], b = d[(size_t) best], c = d[(size_t) best + 1];
            const double denom = a - 2.0 * b + c;
            if (std::abs (denom) > 1e-20)
                tauRefined = best + 0.5 * (a - c) / denom;
        }

        return sampleRate / tauRefined;
    }

    inline double centsBetween (double hz, double referenceHz)
    {
        return 1200.0 * std::log2 (hz / referenceHz);
    }

    inline double midiToHz (double note) { return 440.0 * std::pow (2.0, (note - 69.0) / 12.0); }

    inline float rmsDb (const juce::AudioBuffer<float>& buf, double sampleRate, double startSeconds, double lengthSeconds)
    {
        const int start = (int) (startSeconds * sampleRate);
        const int len = juce::jmin ((int) (lengthSeconds * sampleRate), buf.getNumSamples() - start);
        return juce::Decibels::gainToDecibels (buf.getRMSLevel (0, start, len), -200.0f);
    }

    inline bool allFinite (const juce::AudioBuffer<float>& buf)
    {
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            for (int i = 0; i < buf.getNumSamples(); ++i)
                if (! std::isfinite (buf.getSample (ch, i)))
                    return false;
        return true;
    }

    inline void writeWav (const juce::AudioBuffer<float>& buf, double sampleRate, const juce::File& file)
    {
        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream().release());
        if (stream == nullptr)
            return;

        const auto options = juce::AudioFormatWriterOptions{}.withSampleRate (sampleRate)
                                                             .withNumChannels (buf.getNumChannels())
                                                             .withBitsPerSample (24);
        if (auto writer = wav.createWriterFor (stream, options))
            writer->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
    }

    inline juce::MidiMessage on (int note, int velocity = 100, int channel = 1)  { return juce::MidiMessage::noteOn (channel, note, (juce::uint8) velocity); }
    inline juce::MidiMessage off (int note, int channel = 1)                     { return juce::MidiMessage::noteOff (channel, note); }
    inline juce::MidiMessage bend (int value, int channel = 1)                   { return juce::MidiMessage::pitchWheel (channel, value); }
    inline juce::MidiMessage cc (int number, int value, int channel = 1)         { return juce::MidiMessage::controllerEvent (channel, number, value); }
}
