// Offline tests for the theremin processor (compiled directly from the plugin
// sources), plus an editor snapshot and a few demo renders.
//
// Usage: ThereminTests [output-directory]

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"
#include "TestUtils.h"

using namespace testutils;
namespace ids = theremin::ids;

namespace
{
    constexpr double sr = 48000.0;

    void setParam (juce::AudioProcessor& proc, const juce::String& id, float value)
    {
        for (auto* p : proc.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
                if (r->getParameterID() == id)
                {
                    r->setValueNotifyingHost (r->convertTo0to1 (value));
                    return;
                }
        std::cout << "  !! unknown parameter " << id << std::endl;
    }

    float getParam (juce::AudioProcessor& proc, const juce::String& id)
    {
        for (auto* p : proc.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
                if (r->getParameterID() == id)
                    return r->convertFrom0to1 (r->getValue());
        return -9999.0f;
    }

    /** A processor configured for precise pitch measurements. */
    std::unique_ptr<AetherThereminAudioProcessor> makeDry (double sampleRate = sr, int block = 512)
    {
        auto p = std::make_unique<AetherThereminAudioProcessor>();
        setParam (*p, ids::vibDepth, 0.0f);
        setParam (*p, ids::humanize, 0.0f);
        setParam (*p, ids::scoop, 0.0f);
        setParam (*p, ids::reverbMix, 0.0f);
        setParam (*p, ids::overshoot, 0.0f);
        p->setPlayConfigDetails (0, 2, sampleRate, block);
        p->prepareToPlay (sampleRate, block);
        return p;
    }

    juce::String fmt (double v, int dp = 2) { return juce::String (v, dp); }

    template <typename T>
    void collect (juce::Component& c, std::vector<T*>& out)
    {
        if (auto* t = dynamic_cast<T*> (&c))
            out.push_back (t);
        for (auto* child : c.getChildren())
            collect (*child, out);
    }

    void refreshViews (juce::Component& editor)
    {
        std::vector<theremin::ThereminView*> views;
        collect (editor, views);
        for (auto* v : views)
            for (int i = 0; i < 20; ++i)
                v->refresh();
    }

    /** Every knob and menu lies inside the window at a usable size, and no two knobs overlap. */
    juce::String checkControlsFit (juce::Component& editor)
    {
        const auto window = editor.getLocalBounds().toFloat().expanded (1.0f);
        std::vector<juce::Slider*> sliders;
        std::vector<juce::ComboBox*> boxes;
        collect (editor, sliders);
        collect (editor, boxes);

        std::vector<juce::Rectangle<float>> knobAreas;
        for (auto* s : sliders)
        {
            const auto r = editor.getLocalArea (s, s->getLocalBounds().toFloat());
            if (! window.contains (r))
                return "knob outside window: " + r.toString();
            if (r.getWidth() < 12.0f || r.getHeight() < 12.0f)
                return "knob too small: " + r.toString();
            knobAreas.push_back (r.reduced (1.0f));
        }
        for (auto* b : boxes)
        {
            const auto r = editor.getLocalArea (b, b->getLocalBounds().toFloat());
            if (! window.contains (r))
                return "menu outside window: " + r.toString();
        }
        for (size_t i = 0; i < knobAreas.size(); ++i)
            for (size_t j = i + 1; j < knobAreas.size(); ++j)
                if (knobAreas[i].intersects (knobAreas[j]))
                    return "knobs overlap: " + knobAreas[i].toString() + " / " + knobAreas[j].toString();

        return {};
    }

    bool writeSnapshot (juce::Component& editor, const juce::File& file)
    {
        auto image = editor.createComponentSnapshot (editor.getLocalBounds(), true, 1.0f);
        file.deleteFile();
        if (auto stream = file.createOutputStream())
        {
            juce::PNGImageFormat png;
            return png.writeImageToStream (image, *stream);
        }
        return false;
    }
}

//==============================================================================
int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    Checker t;

    const auto outDir = argc > 1 ? juce::File (juce::String (argv[1]))
                                 : juce::File::getCurrentWorkingDirectory().getChildFile ("test-output");
    outDir.createDirectory();

    //--------------------------------------------------------------------------
    std::cout << "\n== Tuning (default tone, pull compensation) ==" << std::endl;
    for (int note : { 33, 45, 57, 69, 81, 93 })
    {
        auto p = makeDry();
        auto out = render (*p, sr, { { 0.0, on (note) } }, 1.2);
        const auto hz = estimatePitch (out, sr, 0.6, 0.5, 30.0, 4000.0);
        const auto err = centsBetween (hz, midiToHz (note));
        t.check (std::abs (err) < 2.0, "note " + juce::String (note) + " in tune", fmt (hz) + " Hz, " + fmt (err) + " ct");
    }

    for (float timbre : { 0.0f, 1.0f })
        for (float pull : { 0.0f, 1.0f })
        {
            auto p = makeDry();
            setParam (*p, ids::timbre, timbre);
            setParam (*p, ids::pull, pull);
            auto out = render (*p, sr, { { 0.0, on (45) } }, 1.0);
            const auto hz = estimatePitch (out, sr, 0.5, 0.45);
            const auto err = centsBetween (hz, midiToHz (45));
            t.check (std::abs (err) < 2.0, "A2 in tune at timbre " + fmt (timbre, 1) + " pull " + fmt (pull, 1), fmt (err) + " ct");
        }

    for (double rate : { 44100.0, 96000.0, 192000.0 })
    {
        auto p = makeDry (rate, 256);
        auto out = render (*p, rate, { { 0.0, on (69) } }, 0.8, nullptr, 256);
        const auto hz = estimatePitch (out, rate, 0.4, 0.3);
        t.check (std::abs (centsBetween (hz, 440.0)) < 2.0, "A4 in tune at " + fmt (rate, 0) + " Hz", fmt (hz) + " Hz");
    }

    //--------------------------------------------------------------------------
    std::cout << "\n== Heterodyne physics (harmonic content) ==" << std::endl;
    {
        // Goertzel magnitude of harmonic k of a steady note
        auto harmonic = [] (const juce::AudioBuffer<float>& b, double f, double t0, double len)
        {
            const int s0 = (int) (t0 * sr), n = (int) (len * sr);
            const double w = juce::MathConstants<double>::twoPi * f / sr, c = 2.0 * std::cos (w);
            double q1 = 0.0, q2 = 0.0;
            for (int i = 0; i < n; ++i)
            {
                const double win = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * i / (n - 1));
                const double q0 = c * q1 - q2 + win * b.getSample (0, s0 + i);
                q2 = q1; q1 = q0;
            }
            return std::sqrt (q1 * q1 + q2 * q2 - c * q1 * q2);
        };

        auto thd = [&] (int note, float timbre, float pull)
        {
            auto p = makeDry();
            setParam (*p, ids::timbre, timbre);
            setParam (*p, ids::pull, pull);
            setParam (*p, ids::warmth, 0.0f);
            setParam (*p, ids::cabinet, 0.0f);
            setParam (*p, ids::tone, 1.0f);
            auto out = render (*p, sr, { { 0.0, on (note) } }, 1.3);
            const auto f = midiToHz (note);
            const auto h1 = harmonic (out, f, 0.5, 0.75);
            double rest = 0.0;
            for (int k = 2; k <= 8; ++k)
                if (f * k < sr * 0.45)
                    rest += juce::square (harmonic (out, f * k, 0.5, 0.75));
            return std::sqrt (rest) / h1;
        };

        const auto pure = thd (57, 0.0f, 0.0f);
        const auto lowPulled = thd (45, 0.0f, 1.0f);
        const auto highPulled = thd (81, 0.0f, 1.0f);
        const auto bright = thd (57, 1.0f, 0.0f);
        t.check (pure < 0.01, "timbre 0 + pull 0 is a pure sine", "THD " + fmt (pure * 100.0, 2) + " %");
        t.check (lowPulled > 4.0 * highPulled && lowPulled > 0.1,
                 "oscillator pull makes low notes reedy, high notes pure",
                 "THD A2 " + fmt (lowPulled * 100.0, 1) + " % vs A5 " + fmt (highPulled * 100.0, 1) + " %");
        t.check (bright > 0.15, "full timbre gives the bright detector waveform", "THD " + fmt (bright * 100.0, 1) + " %");
    }

    //--------------------------------------------------------------------------
    std::cout << "\n== Legato slides (overlapping notes) ==" << std::endl;
    {
        auto p = makeDry();
        setParam (*p, ids::glide, 120.0f);
        auto out = render (*p, sr, { { 0.0, on (60) }, { 0.5, on (64) }, { 1.0, off (60) }, { 1.5, off (64) } }, 1.8);

        const auto before = estimatePitch (out, sr, 0.3, 0.15);
        const auto mid = estimatePitch (out, sr, 0.52, 0.025);
        const auto after = estimatePitch (out, sr, 0.85, 0.12);
        t.check (std::abs (centsBetween (before, midiToHz (60))) < 3.0, "starts on C4", fmt (before) + " Hz");
        t.check (mid > midiToHz (60) * 1.01 && mid < midiToHz (64) * 0.995, "glides through intermediate pitches", fmt (mid) + " Hz");
        t.check (std::abs (centsBetween (after, midiToHz (64))) < 3.0, "arrives on E4", fmt (after) + " Hz");

        // the volume hand must not re-articulate during the slide
        float minDb = 0.0f, refDb = rmsDb (out, sr, 0.40, 0.05);
        minDb = refDb;
        for (double tt = 0.45; tt < 0.75; tt += 0.01)
            minDb = juce::jmin (minDb, rmsDb (out, sr, tt, 0.02));
        t.check (refDb - minDb < 1.5f, "no volume dip during slide", fmt (refDb - minDb) + " dB");
    }

    {
        // FL Studio slide-note pattern: long note, short slide note on top.
        auto p = makeDry();
        setParam (*p, ids::glide, 60.0f);
        auto stay = render (*p, sr, { { 0.0, on (57) }, { 0.5, on (60) }, { 0.8, off (60) }, { 2.0, off (57) } }, 2.1);
        const auto hzStay = estimatePitch (stay, sr, 1.4, 0.15);
        t.check (std::abs (centsBetween (hzStay, midiToHz (60))) < 3.0, "Stay mode keeps the slid-to pitch (FL slide semantics)", fmt (hzStay) + " Hz");

        auto p2 = makeDry();
        setParam (*p2, ids::glide, 60.0f);
        setParam (*p2, ids::noteRelease, 1.0f);
        auto ret = render (*p2, sr, { { 0.0, on (57) }, { 0.5, on (60) }, { 0.8, off (60) }, { 2.0, off (57) } }, 2.1);
        const auto hzRet = estimatePitch (ret, sr, 1.4, 0.15);
        t.check (std::abs (centsBetween (hzRet, midiToHz (57))) < 3.0, "Return mode glides back to the held note", fmt (hzRet) + " Hz");
    }

    {
        // Back-to-back notes sharing a timestamp, delivered note-on FIRST:
        // must articulate (no slide) regardless of the host's event order.
        auto p = makeDry();
        setParam (*p, ids::glide, 400.0f);
        auto out = render (*p, sr, { { 0.0, on (60) }, { 0.5, on (67) }, { 0.5, off (60) }, { 1.0, off (67) } }, 1.1);
        const auto hz = estimatePitch (out, sr, 0.505, 0.03);
        t.check (std::abs (centsBetween (hz, midiToHz (67))) < 5.0, "adjacent notes do not slide (on/off same sample)", fmt (hz) + " Hz");

        auto p2 = makeDry();
        setParam (*p2, ids::glide, 400.0f);
        auto out2 = render (*p2, sr, { { 0.0, on (60) }, { 0.5, on (67) }, { 0.501, off (60) }, { 1.0, off (67) } }, 1.1);
        const auto hz2 = estimatePitch (out2, sr, 0.505, 0.03);
        t.check (hz2 < midiToHz (67) * 0.97, "slightly overlapping notes DO slide", fmt (hz2) + " Hz");
    }

    {
        // Glide mode "Always": separate notes glide too.
        auto p = makeDry();
        setParam (*p, ids::glide, 300.0f);
        setParam (*p, ids::glideMode, 1.0f);
        auto out = render (*p, sr, { { 0.0, on (60) }, { 0.4, off (60) }, { 0.5, on (67) }, { 1.4, off (67) } }, 1.5);
        const auto hz = estimatePitch (out, sr, 0.51, 0.03);
        const auto hzEnd = estimatePitch (out, sr, 1.1, 0.15);
        t.check (hz < midiToHz (67) * 0.97, "Always mode glides between separate notes", fmt (hz) + " Hz");
        t.check (std::abs (centsBetween (hzEnd, midiToHz (67))) < 3.0, "Always mode arrives", fmt (hzEnd) + " Hz");
    }

    //--------------------------------------------------------------------------
    std::cout << "\n== Pitch bend ==" << std::endl;
    {
        auto p = makeDry();
        auto out = render (*p, sr, { { 0.0, on (69) }, { 0.4, bend (16383) }, { 0.8, bend (0) }, { 1.2, bend (8192) }, { 1.6, off (69) } }, 1.7);
        const auto up = estimatePitch (out, sr, 0.55, 0.2);
        const auto down = estimatePitch (out, sr, 0.95, 0.2);
        const auto centre = estimatePitch (out, sr, 1.35, 0.2);
        t.check (std::abs (centsBetween (up, 880.0)) < 3.0, "full bend up = +12 st (FL default range)", fmt (up) + " Hz");
        t.check (std::abs (centsBetween (down, 220.0)) < 3.0, "full bend down = -12 st", fmt (down) + " Hz");
        t.check (std::abs (centsBetween (centre, 440.0)) < 3.0, "bend centre = no offset", fmt (centre) + " Hz");
    }

    {
        // FL Studio-style slide via channel pitch: a smooth bend ramp of 7 st.
        auto p = makeDry();
        std::vector<TimedEvent> ev { { 0.0, on (57) } };
        for (int i = 0; i <= 50; ++i)
            ev.push_back ({ 0.3 + i * 0.004, bend (8192 + (int) std::lround (8191.0 * 7.0 / 12.0 * i / 50.0)) });
        ev.push_back ({ 1.0, off (57) });
        auto out = render (*p, sr, ev, 1.1);
        const auto hz = estimatePitch (out, sr, 0.7, 0.2);
        t.check (std::abs (centsBetween (hz, midiToHz (64))) < 3.0, "bend ramp lands on +7 st", fmt (hz) + " Hz");
    }

    {
        // Per-channel bends: FL note colours / MPE send each voice on its own channel.
        auto p = makeDry();
        auto out = render (*p, sr, { { 0.0, bend (16383, 2) }, { 0.0, on (69, 100, 3) }, { 0.6, off (69, 3) } }, 0.7);
        const auto hz = estimatePitch (out, sr, 0.3, 0.2);
        t.check (std::abs (centsBetween (hz, 440.0)) < 3.0, "bend on another channel does not affect the note", fmt (hz) + " Hz");

        auto p2 = makeDry();
        auto out2 = render (*p2, sr, { { 0.0, on (69, 100, 3) }, { 0.2, bend (16383, 3) }, { 0.8, off (69, 3) } }, 0.9);
        const auto hz2 = estimatePitch (out2, sr, 0.45, 0.2);
        t.check (std::abs (centsBetween (hz2, 880.0)) < 3.0, "bend on the note's own channel (MPE style)", fmt (hz2) + " Hz");
    }

    {
        // New phrase while a bend is reset at the very same sample (either order).
        for (bool bendFirst : { false, true })
        {
            auto p = makeDry();
            std::vector<TimedEvent> ev { { 0.0, on (69) }, { 0.1, bend (16383) }, { 0.5, off (69) } };
            if (bendFirst) { ev.push_back ({ 0.8, bend (8192) }); ev.push_back ({ 0.8, on (60) }); }
            else           { ev.push_back ({ 0.8, on (60) });     ev.push_back ({ 0.8, bend (8192) }); }
            ev.push_back ({ 1.2, off (60) });
            auto out = render (*p, sr, ev, 1.3);
            const auto hz = estimatePitch (out, sr, 0.802, 0.03);
            t.check (std::abs (centsBetween (hz, midiToHz (60))) < 5.0,
                     juce::String ("new note starts on pitch after bend reset (") + (bendFirst ? "bend first" : "note first") + ")", fmt (hz) + " Hz");
        }
    }

    {
        // RPN 0: host announces a +/-2 semitone range.
        auto p = makeDry();
        auto out = render (*p, sr, { { 0.0, cc (101, 0) }, { 0.0, cc (100, 0) }, { 0.0, cc (6, 2) }, { 0.0, cc (38, 0) },
                                     { 0.01, on (69) }, { 0.3, bend (16383) }, { 0.8, off (69) } }, 0.9);
        const auto hz = estimatePitch (out, sr, 0.5, 0.2);
        t.check (std::abs (centsBetween (hz, midiToHz (71))) < 3.0, "RPN 0 sets bend range to 2 st", fmt (hz) + " Hz");

        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        t.check (std::abs (getParam (*p, ids::bendRange) - 2.0f) < 0.01f, "bend range parameter follows RPN", fmt (getParam (*p, ids::bendRange)));

        // ...and a user change afterwards wins again
        setParam (*p, ids::bendRange, 12.0f);
        auto out2 = render (*p, sr, { { 0.0, bend (8192) }, { 0.01, on (69) }, { 0.2, bend (16383) }, { 0.7, off (69) } }, 0.8);
        const auto hz2 = estimatePitch (out2, sr, 0.4, 0.2);
        t.check (std::abs (centsBetween (hz2, 880.0)) < 3.0, "user bend range overrides an earlier RPN", fmt (hz2) + " Hz");
    }

    //--------------------------------------------------------------------------
    std::cout << "\n== Controllers, robustness ==" << std::endl;
    {
        auto p = makeDry();
        auto out = render (*p, sr, { { 0.0, on (69) }, { 0.5, cc (120, 0) } }, 0.7);
        t.check (rmsDb (out, sr, 0.53, 0.05) < -80.0f, "All Sound Off silences within 30 ms", fmt (rmsDb (out, sr, 0.53, 0.05)) + " dB");

        auto p2 = makeDry();
        auto out2 = render (*p2, sr, { { 0.0, on (69) }, { 0.5, cc (123, 0) } }, 1.8);
        t.check (rmsDb (out2, sr, 1.6, 0.1) < -80.0f, "All Notes Off releases", fmt (rmsDb (out2, sr, 1.6, 0.1)) + " dB");

        auto p3 = makeDry();
        auto out3 = render (*p3, sr, { { 0.0, on (69) }, { 0.3, cc (11, 0) }, { 0.6, cc (11, 127) } }, 0.9);
        t.check (rmsDb (out3, sr, 0.45, 0.1) < -80.0f && rmsDb (out3, sr, 0.75, 0.1) > -30.0f, "CC11 expression acts as volume hand");

        auto p4 = makeDry();
        auto out4 = render (*p4, sr, { { 0.0, on (69, 0) } }, 0.3);
        t.check (rmsDb (out4, sr, 0.0, 0.3) < -100.0f, "velocity-0 note-on is a note-off");

        auto p5 = makeDry();
        auto out5 = render (*p5, sr, { { 0.0, on (60) }, { 0.3, on (60) }, { 0.6, off (60) } }, 1.6);
        t.check (rmsDb (out5, sr, 1.4, 0.1) < -80.0f, "duplicate note-on does not hang");

        auto p6 = makeDry();
        auto out6 = render (*p6, sr, { { 0.0, on (60, 100, 1) }, { 0.5, off (60, 5) } }, 1.6);
        t.check (rmsDb (out6, sr, 1.4, 0.1) < -80.0f, "note-off on a different channel still releases");
    }

    {
        // Sample-accurate MIDI: FL Studio-like random buffer sizes must render
        // the same audio as fixed 512-sample blocks.
        std::vector<TimedEvent> ev { { 0.0, on (60) }, { 0.25, on (64) }, { 0.45, off (60) }, { 0.5, bend (12000) },
                                     { 0.7, bend (8192) }, { 0.8, off (64) }, { 0.9, on (55) }, { 1.3, off (55) } };
        auto a = makeDry();
        auto outA = render (*a, sr, ev, 1.6);

        auto b = makeDry (sr, 1024);
        juce::Random rng (42);
        auto outB = render (*b, sr, ev, 1.6, [&rng] (int) { return 1 + rng.nextInt (1024); });

        float maxDiff = 0.0f;
        for (int i = 0; i < outA.getNumSamples(); ++i)
            maxDiff = juce::jmax (maxDiff, std::abs (outA.getSample (0, i) - outB.getSample (0, i)));
        t.check (maxDiff < 1.0e-3f, "random host buffer sizes give identical audio", "max diff " + juce::String (maxDiff, 6));
        t.check (allFinite (outA) && allFinite (outB), "no NaN / inf");
    }

    {
        // clicks: the onset must not be sharper than the steady waveform
        auto p = std::make_unique<AetherThereminAudioProcessor>();
        p->setPlayConfigDetails (0, 2, sr, 512);
        p->prepareToPlay (sr, 512);
        auto out = render (*p, sr, { { 0.1, on (69) }, { 0.6, off (69) } }, 1.0);
        auto maxStep = [&out] (double t0, double t1)
        {
            float m = 0.0f;
            for (int i = (int) (t0 * sr) + 1; i < (int) (t1 * sr); ++i)
                m = juce::jmax (m, std::abs (out.getSample (0, i) - out.getSample (0, i - 1)));
            return m;
        };
        const auto onset = maxStep (0.1, 0.105), steady = maxStep (0.4, 0.5), releaseStep = maxStep (0.6, 0.605);
        t.check (onset <= steady * 1.05f && releaseStep <= steady * 1.05f, "click-free onset and release",
                 "onset " + fmt (onset, 4) + " release " + fmt (releaseStep, 4) + " steady " + fmt (steady, 4));
    }

    //--------------------------------------------------------------------------
    std::cout << "\n== Presets, state ==" << std::endl;
    {
        AetherThereminAudioProcessor p;
        p.setPlayConfigDetails (0, 2, sr, 512);
        p.prepareToPlay (sr, 512);

        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            p.loadFactoryPreset (i);
            p.reset();
            auto out = render (p, sr, { { 0.0, on (60, 127) }, { 0.6, on (67, 127) }, { 1.2, off (60) }, { 1.4, off (67) } }, 1.6);
            const auto peak = out.getMagnitude (0, 0, out.getNumSamples());
            const auto level = rmsDb (out, sr, 0.3, 0.6);
            t.check (allFinite (out) && peak < 1.6f && level > -40.0f,
                     "preset '" + p.getProgramName (i) + "' renders", "peak " + fmt (peak, 3) + ", rms " + fmt (level, 1) + " dB");
        }

        // state round trip
        p.loadFactoryPreset (3);
        setParam (p, ids::glide, 777.0f);
        setParam (p, ids::octave, 2.0f);
        setParam (p, ids::noteRelease, 1.0f);
        juce::MemoryBlock state;
        p.getStateInformation (state);

        AetherThereminAudioProcessor q;
        q.setStateInformation (state.getData(), (int) state.getSize());
        t.check (std::abs (getParam (q, ids::glide) - 777.0f) < 0.5f && std::abs (getParam (q, ids::octave) - 2.0f) < 0.01f
                     && getParam (q, ids::noteRelease) > 0.5f && q.getCurrentProgram() == 3,
                 "state save / restore round trip");

        // a host re-sending the same program after restoring must not wipe tweaks
        q.setCurrentProgram (3);
        t.check (std::abs (getParam (q, ids::glide) - 777.0f) < 0.5f, "repeated setCurrentProgram keeps user edits");
    }

    //--------------------------------------------------------------------------
    std::cout << "\n== Mouse 'air' playing ==" << std::endl;
    {
        auto p = makeDry();
        p->setAirPlay (true, 69.0f, 0.8f);
        auto out = render (*p, sr, {}, 0.6);
        const auto hz = estimatePitch (out, sr, 0.3, 0.2);
        t.check (std::abs (centsBetween (hz, 440.0)) < 3.0, "air play sounds at the hand's pitch", fmt (hz) + " Hz");
        p->setAirPlay (false, 69.0f, 0.0f);
        auto out2 = render (*p, sr, {}, 1.5);
        t.check (rmsDb (out2, sr, 1.3, 0.1) < -80.0f, "air play stops on mouse-up");
    }

    //--------------------------------------------------------------------------
    std::cout << "\n== Performance ==" << std::endl;
    {
        AetherThereminAudioProcessor p;
        p.setPlayConfigDetails (0, 2, sr, 512);
        p.prepareToPlay (sr, 512);
        std::vector<TimedEvent> ev;
        for (int i = 0; i < 40; ++i)
        {
            ev.push_back ({ i * 0.5, on (48 + (i * 5) % 24) });
            ev.push_back ({ i * 0.5 + 0.6, off (48 + (i * 5) % 24) });
        }
        const auto start = juce::Time::getMillisecondCounterHiRes();
        auto out = render (p, sr, ev, 20.0);
        const auto ms = juce::Time::getMillisecondCounterHiRes() - start;
        const auto realtime = 20000.0 / ms;
        t.check (realtime > 20.0, "renders much faster than real time", fmt (realtime, 1) + "x real time");
    }

    //--------------------------------------------------------------------------
    std::cout << "\n== Editor ==" << std::endl;
    {
        AetherThereminAudioProcessor p;
        p.setPlayConfigDetails (0, 2, sr, 512);
        p.prepareToPlay (sr, 512);
        auto out = render (p, sr, { { 0.0, on (64) } }, 0.8);

        using Editor = AetherThereminAudioProcessorEditor;
        std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditorAndMakeActive());
        t.check (editor != nullptr, "editor opens");

        if (editor != nullptr)
        {
            editor->setVisible (true);
            t.check (editor->getWidth() == 728 && editor->getHeight() == 448,
                     "opens 30% smaller than the design size (1040x640)",
                     juce::String (editor->getWidth()) + "x" + juce::String (editor->getHeight()));

            auto* constrainer = editor->getConstrainer();
            t.check (editor->isResizable() && constrainer != nullptr
                         && constrainer->getMinimumWidth() <= 420 && constrainer->getMaximumWidth() >= 5000
                         && constrainer->getFixedAspectRatio() == 0.0,
                     "freely resizable, any shape",
                     constrainer != nullptr ? juce::String (constrainer->getMinimumWidth()) + "x" + juce::String (constrainer->getMinimumHeight())
                                                  + " .. " + juce::String (constrainer->getMaximumWidth()) + "x" + juce::String (constrainer->getMaximumHeight())
                                            : juce::String());

            struct Size { int w, h; const char* name; };
            const Size sizes[] = { { 728, 448, "default" }, { 1040, 640, "design" }, { 416, 256, "minimum" },
                                   { 1700, 520, "wide" }, { 620, 980, "tall" }, { 2600, 1600, "huge" },
                                   { 1111, 677, "odd" } };

            for (const auto& sz : sizes)
            {
                editor->setSize (sz.w, sz.h);
                refreshViews (*editor);
                const auto problem = checkControlsFit (*editor);

                // the scaled content must cover the whole window
                auto* content = editor->getChildComponent (0);
                const auto covered = content != nullptr
                    ? editor->getLocalArea (content, content->getLocalBounds().toFloat()) : juce::Rectangle<float>();
                const bool fills = covered.getWidth() >= (float) sz.w - 0.5f && covered.getHeight() >= (float) sz.h - 0.5f;

                const auto saved = p.getSavedEditorSize();
                t.check (problem.isEmpty() && fills && saved.x == sz.w && saved.y == sz.h,
                         juce::String ("layout at ") + sz.name + " size " + juce::String (sz.w) + "x" + juce::String (sz.h),
                         problem.isNotEmpty() ? problem : (fills ? juce::String() : juce::String ("content does not fill window: ") + covered.toString()));

                writeSnapshot (*editor, outDir.getChildFile (juce::String ("editor_") + sz.name + ".png"));
            }

            // README screenshot at the design size
            editor->setSize (Editor::designWidth, Editor::designHeight);
            refreshViews (*editor);
            const auto file = outDir.getChildFile ("editor.png");
            t.check (writeSnapshot (*editor, file) && file.existsAsFile(), "editor snapshot written", file.getFullPathName());

            // the chosen size is remembered by the instance and by the project
            // (a host tells the processor before it deletes the editor, as FL Studio's wrapper does)
            editor->setSize (1300, 700);
            p.editorBeingDeleted (editor.get());
            editor.reset();
            editor.reset (p.createEditorAndMakeActive());
            t.check (editor != nullptr && editor->getWidth() == 1300 && editor->getHeight() == 700,
                     "reopens at the size the user chose",
                     editor != nullptr ? juce::String (editor->getWidth()) + "x" + juce::String (editor->getHeight()) : juce::String ("no editor"));
            p.editorBeingDeleted (editor.get());
            editor.reset();

            juce::MemoryBlock state;
            p.getStateInformation (state);
            AetherThereminAudioProcessor q;
            q.setStateInformation (state.getData(), (int) state.getSize());
            std::unique_ptr<juce::AudioProcessorEditor> restored (q.createEditorAndMakeActive());
            t.check (restored != nullptr && restored->getWidth() == 1300 && restored->getHeight() == 700,
                     "window size is saved with the project",
                     restored != nullptr ? juce::String (restored->getWidth()) + "x" + juce::String (restored->getHeight()) : juce::String ("no editor"));
            q.editorBeingDeleted (restored.get());
            restored.reset();
        }
    }

    //--------------------------------------------------------------------------
    std::cout << "\n== Demo renders ==" << std::endl;
    {
        // A short phrase with overlapping-note slides, written the way it would
        // be drawn in FL Studio's piano roll.
        const double beat = 60.0 / 92.0;
        std::vector<TimedEvent> phrase;
        auto note = [&phrase, beat] (double startBeat, double lenBeats, int n, int vel = 100)
        {
            phrase.push_back ({ startBeat * beat, on (n, vel) });
            phrase.push_back ({ (startBeat + lenBeats) * beat, off (n) });
        };
        note (0.0, 2.1, 64);  note (2.0, 1.1, 69); note (3.0, 1.1, 71); note (4.0, 3.0, 72);
        note (7.5, 0.6, 71);  note (8.0, 0.6, 69); note (8.5, 3.5, 76);
        note (12.5, 1.0, 74); note (13.4, 1.1, 72); note (14.4, 3.6, 69);

        for (int preset : { 0, 1, 3, 4, 8 })
        {
            AetherThereminAudioProcessor p;
            p.setPlayConfigDetails (0, 2, sr, 512);
            p.prepareToPlay (sr, 512);
            p.loadFactoryPreset (preset);
            p.reset();

            auto events = phrase;
            if (preset == 8)
                for (auto& e : events)
                    if (e.message.isNoteOnOrOff())
                        e.message.setNoteNumber (e.message.getNoteNumber() - 12);

            auto out = render (p, sr, events, 19.0 * beat + 2.0);
            auto file = outDir.getChildFile ("demo_" + p.getProgramName (preset).replaceCharacter (' ', '_').toLowerCase() + ".wav");
            writeWav (out, sr, file);
            t.check (file.existsAsFile() && allFinite (out), "rendered " + file.getFileName());
        }
    }

    return t.summary();
}
