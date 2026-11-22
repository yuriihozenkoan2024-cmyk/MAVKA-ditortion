#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace mavka;

//==============================================================================
juce::String MavkaAudioProcessor::formatValue (Unit unit, float v)
{
    const auto pct = [] (float x) { return juce::String (juce::roundToInt (x * 100.0f)) + "%"; };
    switch (unit)
    {
        case Unit::Percent: return pct (v);
        case Unit::Bipolar: return (v > 0.004f ? "+" : "") + pct (v);
        case Unit::Ink:     return v > 0.004f ? "Ink " + pct (v) : (v < -0.004f ? "Press " + pct (-v) : juce::String ("Off"));
        case Unit::Slant:   return v > 0.004f ? "Scoop " + pct (v) : (v < -0.004f ? "Fall " + pct (-v) : juce::String ("Off"));
        case Unit::Tone:    return v > 0.004f ? "Bright " + pct (v) : (v < -0.004f ? "Dark " + pct (-v) : juce::String ("Neutral"));
        case Unit::Db:      return (v > 0.04f ? "+" : "") + juce::String (v, 1) + " dB";
        case Unit::Hz:      return v < 1000.0f ? juce::String (juce::roundToInt (v)) + " Hz" : juce::String (v / 1000.0f, 1) + " kHz";
        case Unit::Ms:      return juce::String (juce::roundToInt (v)) + " ms";
        case Unit::Sec:     return juce::String (v, v < 10.0f ? 2 : 1) + " s";
        case Unit::None:    break;
    }
    return juce::String (v, 2);
}

static float parseValue (Unit unit, const juce::String& text)
{
    const auto t = text.trim().toLowerCase();
    float x = t.retainCharacters ("0123456789.-+").getFloatValue();
    switch (unit)
    {
        case Unit::Percent:
        case Unit::Bipolar: return x / 100.0f;
        case Unit::Ink:     return (t.contains ("press") ? -std::abs (x) : x) / 100.0f;
        case Unit::Slant:   return (t.contains ("fall") ? -std::abs (x) : x) / 100.0f;
        case Unit::Tone:    return (t.contains ("dark") ? -std::abs (x) : x) / 100.0f;
        case Unit::Hz:      return t.contains ("k") ? x * 1000.0f : x;
        case Unit::Db:
        case Unit::Ms:
        case Unit::Sec:
        case Unit::None:    break;
    }
    return x;
}

juce::AudioProcessorValueTreeState::ParameterLayout MavkaAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const ParamDef* defs = paramDefs();

    for (int i = 0; i < P_COUNT; ++i)
    {
        const ParamDef& p = defs[i];
        const juce::ParameterID pid { p.id, 1 };

        if (p.type == PType::Choice)
        {
            juce::StringArray items;
            for (int k = 0; k < p.numChoices; ++k)
                items.add (p.choices[k]);
            layout.add (std::make_unique<juce::AudioParameterChoice> (pid, p.name, items, (int) p.defV));
        }
        else if (p.type == PType::Bool)
        {
            layout.add (std::make_unique<juce::AudioParameterBool> (pid, p.name, p.defV > 0.5f));
        }
        else
        {
            juce::NormalisableRange<float> range (p.minV, p.maxV);
            if (p.centre > p.minV && p.centre < p.maxV)
                range.setSkewForCentre (p.centre);

            const Unit unit = p.unit;
            auto attributes = juce::AudioParameterFloatAttributes()
                                  .withStringFromValueFunction ([unit] (float v, int) { return formatValue (unit, v); })
                                  .withValueFromStringFunction ([unit] (const juce::String& s) { return parseValue (unit, s); });
            layout.add (std::make_unique<juce::AudioParameterFloat> (pid, p.name, range, p.defV, attributes));
        }
    }
    return layout;
}

//==============================================================================
MavkaAudioProcessor::MavkaAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MAVKA", createLayout())
{
    const ParamDef* defs = paramDefs();
    for (int i = 0; i < P_COUNT; ++i)
    {
        raw[(size_t) i] = apvts.getRawParameterValue (defs[i].id);
        jassert (raw[(size_t) i] != nullptr);
    }
}

bool MavkaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void MavkaAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    engine.prepare (sampleRate, getTotalNumOutputChannels());
    pullParams();
    setLatencySamples (engine.getLatency());
}

void MavkaAudioProcessor::pullParams() noexcept
{
    float v[P_COUNT];
    for (int i = 0; i < P_COUNT; ++i)
        v[i] = raw[(size_t) i]->load (std::memory_order_relaxed);
    engine.setParams (v);
}

void MavkaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);

    pullParams();

    bool playing = false;
    double bpm = 120.0, ppq = 0.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            bpm = pos->getBpm().orFallback (120.0);
            ppq = pos->getPpqPosition().orFallback (0.0);
        }
    engine.setTransport (playing, bpm, ppq);
    engine.setNonRealtime (isNonRealtime());

    const int numCh = juce::jmin (2, buffer.getNumChannels());
    if (numCh > 0 && n > 0)
        engine.process (buffer.getArrayOfWritePointers(), numCh, n);
}

void MavkaAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // keep the dry signal latency-aligned while bypassed
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    if (numCh > 0)
        engine.processBypassed (buffer.getArrayOfWritePointers(), numCh, buffer.getNumSamples());
}

//==============================================================================
void MavkaAudioProcessor::loadFactoryPreset (int index)
{
    float v[P_COUNT];
    presetValues (index, v);
    const ParamDef* defs = paramDefs();
    for (int i = 0; i < P_COUNT; ++i)
    {
        if (i == P_OS || i == P_OUTPUT)
            continue;   // presets keep the user's oversampling and output level
        if (auto* p = apvts.getParameter (defs[i].id))
            p->setValueNotifyingHost (p->convertTo0to1 (v[i]));
    }
    presetIndex = index;
}

void MavkaAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", presetIndex.load(), nullptr);
    state.setProperty ("stateVersion", 1, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void MavkaAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            presetIndex = (int) tree.getProperty ("preset", 0);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* MavkaAudioProcessor::createEditor()
{
    return new MavkaAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MavkaAudioProcessor();
}
