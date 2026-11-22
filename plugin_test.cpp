// Headless check of the JUCE wrapper: instantiate, process, presets, state round-trip, editor snapshot.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>

#include <cstdio>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

static int fails = 0;
static void check (bool ok, const char* what)
{
    std::printf ("  [%s] %s\n", ok ? " OK " : "FAIL", what);
    if (! ok) ++fails;
}

static float runAudio (juce::AudioProcessor& p, double sr, int block, double seconds, bool& finite)
{
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    float peak = 0.0f;
    finite = true;
    const int total = (int) (sr * seconds);
    for (int pos = 0; pos < total; pos += block)
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < block; ++i)
            {
                const double t = (pos + i) / sr;
                buf.setSample (c, i, (float) (0.3 * std::sin (2.0 * juce::MathConstants<double>::pi * 150.0 * t)
                                              * (0.5 + 0.5 * std::sin (2.0 * juce::MathConstants<double>::pi * 3.0 * t))));
            }
        p.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < block; ++i)
            {
                const float x = buf.getSample (c, i);
                if (! std::isfinite (x)) finite = false;
                peak = juce::jmax (peak, std::abs (x));
            }
    }
    return peak;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    std::unique_ptr<juce::AudioProcessor> proc (createPluginFilter());
    auto& p = dynamic_cast<MavkaAudioProcessor&> (*proc);

    std::printf ("MAVKA plugin test\n");
    check (p.getParameters().size() == mavka::P_COUNT, "parameter count matches the table");

    juce::AudioProcessor::BusesLayout stereo;
    stereo.inputBuses.add (juce::AudioChannelSet::stereo());
    stereo.outputBuses.add (juce::AudioChannelSet::stereo());
    check (p.setBusesLayout (stereo), "stereo layout accepted");

    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        p.setRateAndBufferSizeDetails (sr, 512);
        p.prepareToPlay (sr, 512);
        bool finite = true;
        const float peak = runAudio (p, sr, 512, 2.0, finite);
        char msg[160];
        std::snprintf (msg, sizeof (msg), "%.0f Hz: audio finite, peak %.2f, latency %d samples, running %dx",
                       sr, peak, p.getLatencySamples(), p.engine.getActiveFactor());
        check (finite && peak > 0.01f && peak < 4.0f, msg);
    }

    // choice parameters must reach the engine as indices
    auto* ratio = p.apvts.getParameter ("ratio");
    ratio->setValueNotifyingHost (ratio->convertTo0to1 (4.0f));
    check (std::abs (p.apvts.getRawParameterValue ("ratio")->load() - 4.0f) < 1.0e-4f, "choice parameter raw value is the index");

    // presets
    int count = 0;
    const mavka::Preset* presets = mavka::factoryPresets (count);
    for (int i = 0; i < count; ++i)
    {
        p.loadFactoryPreset (i);
        float expected[mavka::P_COUNT];
        mavka::presetValues (i, expected);
        bool ok = true;
        for (int k = 0; k < mavka::P_COUNT; ++k)
        {
            if (k == mavka::P_OS || k == mavka::P_OUTPUT) continue;
            const float v = p.apvts.getRawParameterValue (mavka::paramDefs()[k].id)->load();
            if (std::abs (v - expected[k]) > 1.0e-3f * (1.0f + std::abs (expected[k]))) ok = false;
        }
        bool finite = true;
        const float peak = runAudio (p, 48000.0, 256, 1.0, finite);
        char msg[160];
        std::snprintf (msg, sizeof (msg), "preset %-14s loads exactly, audio finite (peak %.2f)", presets[i].name, peak);
        check (ok && finite, msg);
    }

    // state round trip
    p.loadFactoryPreset (6);
    p.apvts.getParameter ("drive")->setValueNotifyingHost (0.123f);
    juce::MemoryBlock state;
    p.getStateInformation (state);
    std::unique_ptr<juce::AudioProcessor> proc2 (createPluginFilter());
    auto& p2 = dynamic_cast<MavkaAudioProcessor&> (*proc2);
    p2.setStateInformation (state.getData(), (int) state.getSize());
    bool same = true;
    for (int k = 0; k < mavka::P_COUNT; ++k)
    {
        const char* id = mavka::paramDefs()[k].id;
        if (std::abs (p.apvts.getRawParameterValue (id)->load() - p2.apvts.getRawParameterValue (id)->load()) > 1.0e-5f)
            same = false;
    }
    check (same && p2.getFactoryPresetIndex() == 6, "state save / load round trip (incl. preset name)");

    // text <-> value
    auto* ink = p.apvts.getParameter ("ink");
    check (ink->getText (ink->convertTo0to1 (-0.4f), 32) == "Press 40%", "Ink shows Press/Ink text");
    check (std::abs (ink->convertFrom0to1 (ink->getValueForText ("Press 40%")) + 0.4f) < 1.0e-3f, "Ink parses typed text");

    // editor snapshots (software renderer, no display needed), driven by a voice file if given
    {
        p.loadFactoryPreset (0);
        if (auto* echo = p.apvts.getParameter ("echo"))
            echo->setValueNotifyingHost (0.5f);
        p.setRateAndBufferSizeDetails (48000.0, 1600);
        p.prepareToPlay (48000.0, 1600);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto& editor = dynamic_cast<MavkaAudioProcessorEditor&> (*ed);

        juce::AudioBuffer<float> voice;
        if (argc > 2)
        {
            juce::AudioFormatManager fm;
            fm.registerBasicFormats();
            if (std::unique_ptr<juce::AudioFormatReader> r { fm.createReaderFor (juce::File (argv[2])) })
            {
                voice.setSize (2, (int) r->lengthInSamples);
                r->read (&voice, 0, (int) r->lengthInSamples, 0, true, true);
            }
        }
        juce::AudioBuffer<float> buf (2, 1600);
        juce::MidiBuffer midi;
        const int ticks = 205;   // ~6.8 s: the rap section
        for (int t = 0; t < ticks; ++t)
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 1600; ++i)
                {
                    const int idx = t * 1600 + i;
                    buf.setSample (c, i, voice.getNumSamples() > idx ? voice.getSample (juce::jmin (c, voice.getNumChannels() - 1), idx)
                                                                     : 0.1f * (float) std::sin (idx * 0.02));
                }
            p.processBlock (buf, midi);
            editor.refreshDisplays();
        }
        bool ok = true;
        for (int page = 0; page < 4; ++page)
        {
            editor.selectPage (page);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            const juce::File out (juce::String (argc > 1 ? argv[1] : "mavka_ui") + "_" + juce::String (page) + ".png");
            out.deleteFile();
            juce::FileOutputStream os (out);
            juce::PNGImageFormat png;
            ok = ok && os.openedOk() && png.writeImageToStream (img, os);
        }
        check (ok, "editor renders all four pages to images");
    }

    std::printf (fails ? "PLUGIN TEST: %d FAILED\n" : "PLUGIN TEST: ALL OK\n", fails);
    return fails ? 1 : 0;
}
