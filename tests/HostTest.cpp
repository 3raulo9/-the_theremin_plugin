// Loads the built VST3 exactly like a DAW does (through the VST3 interfaces,
// including JUCE's IMidiMapping path used for pitch bend and CCs) and checks
// that notes, slides, pitch bend, RPN, presets and state all work end to end.
//
// Usage: ThereminHostTest "<path to Aether Theremin.vst3>"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "TestUtils.h"

using namespace testutils;

namespace
{
    juce::AudioProcessorParameter* findParam (juce::AudioProcessor& proc, const juce::String& name)
    {
        for (auto* p : proc.getParameters())
            if (p->getName (100) == name)
                return p;
        return nullptr;
    }

    bool setParamText (juce::AudioProcessor& proc, const juce::String& name, float plainValue, float minV, float maxV)
    {
        if (auto* p = findParam (proc, name))
        {
            p->setValueNotifyingHost ((plainValue - minV) / (maxV - minV));
            return true;
        }
        std::cout << "  !! parameter not found: " << name << std::endl;
        return false;
    }

    juce::String fmt (double v, int dp = 2) { return juce::String (v, dp); }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    Checker t;

    if (argc < 2)
    {
        std::cout << "usage: ThereminHostTest <plugin.vst3>" << std::endl;
        return 2;
    }

    const juce::String pluginPath (argv[1]);
    juce::AudioPluginFormatManager formats;
    formats.addFormat (std::make_unique<juce::VST3PluginFormat>());

    juce::OwnedArray<juce::PluginDescription> found;
    juce::VST3PluginFormat vst3;
    vst3.findAllTypesForFile (found, pluginPath);

    t.check (found.size() == 1, "VST3 scanned", juce::String (found.size()) + " plugin(s) in " + pluginPath);
    if (found.isEmpty())
        return t.summary();

    const auto& desc = *found[0];
    std::cout << "  name='" << desc.name << "' vendor='" << desc.manufacturerName << "' category='" << desc.category
              << "' version=" << desc.version << std::endl;
    t.check (desc.isInstrument, "registers as an instrument (FL Studio lists it under generators)");
    t.check (desc.name == "Aether Theremin", "plugin name");

    for (double sr : { 44100.0, 48000.0 })
    {
        std::cout << "\n== Hosted at " << sr << " Hz ==" << std::endl;

        juce::String error;
        auto instance = formats.createPluginInstance (desc, sr, 512, error);
        t.check (instance != nullptr, "instantiates", error);
        if (instance == nullptr)
            continue;

        instance->enableAllBuses();
        t.check (instance->getTotalNumOutputChannels() == 2 && instance->getTotalNumInputChannels() == 0, "stereo out, no audio in",
                 juce::String (instance->getTotalNumInputChannels()) + " in / " + juce::String (instance->getTotalNumOutputChannels()) + " out");
        t.check (instance->acceptsMidi(), "accepts MIDI");

        instance->setPlayConfigDetails (0, 2, sr, 512);
        instance->prepareToPlay (sr, 512);

        // exact-pitch configuration
        setParamText (*instance, "Vibrato Depth", 0.0f, 0.0f, 100.0f);
        if (auto* p = findParam (*instance, "Humanize")) p->setValueNotifyingHost (0.0f);
        if (auto* p = findParam (*instance, "Scoop"))    p->setValueNotifyingHost (0.0f);
        if (auto* p = findParam (*instance, "Reverb"))   p->setValueNotifyingHost (0.0f);
        if (auto* p = findParam (*instance, "Hand Overshoot")) p->setValueNotifyingHost (0.0f);

        // let the parameter changes reach the processor
        render (*instance, sr, {}, 0.05);

        {
            auto out = render (*instance, sr, { { 0.0, on (69) }, { 0.8, off (69) } }, 1.0);
            const auto hz = estimatePitch (out, sr, 0.4, 0.3);
            t.check (std::abs (centsBetween (hz, 440.0)) < 2.0, "A4 in tune through VST3", fmt (hz) + " Hz");
            t.check (allFinite (out), "finite output");
        }

        {
            auto out = render (*instance, sr, { { 0.0, on (60) }, { 0.4, on (67) }, { 0.8, off (60) }, { 1.2, off (67) } }, 2.0);
            const auto hz = estimatePitch (out, sr, 0.75, 0.15);
            t.check (std::abs (centsBetween (hz, midiToHz (67))) < 3.0, "legato slide through VST3", fmt (hz) + " Hz");
        }

        {
            // Pitch bend travels through IMidiMapping as a parameter in VST3.
            auto out = render (*instance, sr, { { 0.0, bend (8192) }, { 0.01, on (69) }, { 0.3, bend (16383) }, { 0.8, bend (8192) }, { 1.0, off (69) } }, 2.0);
            const auto hz = estimatePitch (out, sr, 0.5, 0.25);
            t.check (std::abs (centsBetween (hz, 880.0)) < 4.0, "pitch bend +12 st through VST3 IMidiMapping", fmt (hz) + " Hz");
        }

        {
            // A smooth channel-pitch slide like FL Studio sends.
            std::vector<TimedEvent> ev { { 0.0, bend (8192) }, { 0.01, on (57) } };
            for (int i = 1; i <= 40; ++i)
                ev.push_back ({ 0.3 + i * 0.005, bend (8192 + (int) std::lround (8191.0 * 5.0 / 12.0 * i / 40.0)) });
            ev.push_back ({ 1.0, off (57) });
            ev.push_back ({ 1.01, bend (8192) });
            auto out = render (*instance, sr, ev, 2.0);
            const auto hz = estimatePitch (out, sr, 0.65, 0.25);
            t.check (std::abs (centsBetween (hz, midiToHz (62))) < 4.0, "bend ramp (+5 st) through VST3", fmt (hz) + " Hz");
        }

        {
            // RPN 0 via CCs (FL "Send pitch bend range"), then a full bend.
            auto out = render (*instance, sr, { { 0.0, cc (101, 0) }, { 0.0, cc (100, 0) }, { 0.0, cc (6, 7) }, { 0.0, cc (38, 0) },
                                                { 0.02, on (69) }, { 0.3, bend (16383) }, { 0.9, bend (8192) }, { 1.0, off (69) } }, 1.6);
            const auto hz = estimatePitch (out, sr, 0.55, 0.25);
            t.check (std::abs (centsBetween (hz, midiToHz (76))) < 4.0, "RPN bend range (7 st) through VST3", fmt (hz) + " Hz");

            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            setParamText (*instance, "Pitch Bend Range", 12.0f, 1.0f, 48.0f);
            render (*instance, sr, {}, 0.05);
        }

        {
            // random buffer sizes like FL Studio delivers
            juce::Random rng (7);
            auto out = render (*instance, sr, { { 0.0, on (64) }, { 0.3, on (66) }, { 0.5, off (64) }, { 0.9, off (66) } }, 2.0,
                               [&rng] (int) { return 1 + rng.nextInt (512); });
            const auto hz = estimatePitch (out, sr, 0.75, 0.12);
            t.check (allFinite (out) && std::abs (centsBetween (hz, midiToHz (66))) < 4.0, "variable buffer sizes", fmt (hz) + " Hz");
        }

        {
            const auto programs = instance->getNumPrograms();
            t.check (programs >= 8, "factory presets exposed to host", juce::String (programs) + " programs");
            for (int i = 0; i < programs; ++i)
                std::cout << "      " << i << ": " << instance->getProgramName (i) << std::endl;

            instance->setCurrentProgram (3);
            render (*instance, sr, {}, 0.05);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            if (auto* vd = findParam (*instance, "Vibrato Depth"))
                t.check (vd->getText (vd->getValue(), 32).startsWith ("48"), "selecting a preset from the host applies it",
                         "Vibrato Depth = " + vd->getText (vd->getValue(), 32));
            juce::MemoryBlock state;
            instance->getStateInformation (state);

            juce::String err2;
            auto other = formats.createPluginInstance (desc, sr, 512, err2);
            if (other != nullptr)
            {
                other->setStateInformation (state.getData(), (int) state.getSize());
                auto* a = findParam (*instance, "Vibrato Depth");
                auto* b = findParam (*other, "Vibrato Depth");
                t.check (a != nullptr && b != nullptr && std::abs (a->getValue() - b->getValue()) < 1.0e-4f,
                         "state restores in a fresh instance (project reload)");
            }
        }

        {
            std::unique_ptr<juce::AudioProcessorEditor> editor (instance->createEditorAndMakeActive());
            t.check (editor != nullptr, "editor opens through VST3",
                     editor != nullptr ? juce::String (editor->getWidth()) + "x" + juce::String (editor->getHeight()) : juce::String());

            // The host-side window is resizable only if the plugin's IPlugView says canResize().
            t.check (editor != nullptr && editor->isResizable(), "VST3 view reports itself as resizable");
            t.check (editor != nullptr && editor->getWidth() == 728 && editor->getHeight() == 448,
                     "VST3 view opens at 70% of the design size");
            editor.reset();
        }

        // ---- stress tests in the spirit of pluginval ------------------------
        {
            // every parameter randomly automated every block while playing
            juce::Random rng (99);
            std::vector<TimedEvent> ev;
            for (int i = 0; i < 24; ++i)
            {
                ev.push_back ({ i * 0.25, on (36 + rng.nextInt (60), 1 + rng.nextInt (127)) });
                ev.push_back ({ i * 0.25 + 0.1 + rng.nextDouble() * 0.4, off (36 + rng.nextInt (60)) });
                ev.push_back ({ i * 0.25 + 0.05, bend (rng.nextInt (16384)) });
                ev.push_back ({ i * 0.25 + 0.07, cc (1, rng.nextInt (128)) });
            }

            auto& ps = instance->getParameters();
            auto out = render (*instance, sr, ev, 6.5, [&rng] (int) { return 1 + rng.nextInt (512); }, 512,
                               [&ps, &rng] (int)
                               {
                                   for (auto* p : ps)
                                       if (p->isAutomatable() && rng.nextFloat() < 0.3f)
                                           p->setValueNotifyingHost (rng.nextFloat());
                               });
            const auto peak = out.getMagnitude (0, 0, out.getNumSamples());
            t.check (allFinite (out) && peak < 8.0f, "random automation of every parameter stays stable", "peak " + fmt (peak, 2));

            // back to a clean state for the following checks
            render (*instance, sr, { { 0.0, cc (123, 0) }, { 0.0, bend (8192) }, { 0.0, cc (1, 0) } }, 0.1);
            for (auto* p : ps)
                if (p->isAutomatable())
                    p->setValueNotifyingHost (p->getDefaultValue());
            render (*instance, sr, {}, 0.1);
        }

        {
            // repeated re-preparation with changing rates and block sizes
            bool ok = true;
            for (int round = 0; round < 6; ++round)
            {
                const double rate = (round % 3 == 0) ? 44100.0 : (round % 3 == 1 ? 96000.0 : 48000.0);
                const int block = 32 << round;
                instance->releaseResources();
                instance->setPlayConfigDetails (0, 2, rate, block);
                instance->prepareToPlay (rate, block);
                auto out = render (*instance, rate, { { 0.0, on (60) }, { 0.2, off (60) } }, 0.4, nullptr, block);
                const auto mag = out.getMagnitude (0, 0, out.getNumSamples());
                if (! allFinite (out) || mag < 0.01f)
                {
                    std::cout << "      round " << round << " (" << rate << " Hz, " << block << " samples): peak " << mag << std::endl;
                    ok = false;
                }
            }
            instance->releaseResources();
            instance->setPlayConfigDetails (0, 2, sr, 512);
            instance->prepareToPlay (sr, 512);
            t.check (ok, "re-prepare at different rates / block sizes");
        }

        {
            // zero-length and single-sample blocks
            juce::AudioBuffer<float> empty (2, 0);
            juce::MidiBuffer midi;
            midi.addEvent (on (64), 0);
            instance->processBlock (empty, midi);
            auto out = render (*instance, sr, { { 0.0, off (64) }, { 0.01, on (67) }, { 0.3, off (67) } }, 0.5, [] (int) { return 1; });
            t.check (allFinite (out), "zero-length and 1-sample blocks");
        }

        {
            // open / close the editor repeatedly while audio runs
            bool ok = true;
            for (int i = 0; i < 4; ++i)
            {
                std::unique_ptr<juce::AudioProcessorEditor> ed (instance->createEditorAndMakeActive());
                ok = ok && ed != nullptr;
                render (*instance, sr, { { 0.0, on (62) }, { 0.05, off (62) } }, 0.1);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (30);
            }
            t.check (ok, "editor open / close cycles");
        }

        instance->releaseResources();
    }

    return t.summary();
}
