#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "MavkaEngine.h"

#include <array>

class MavkaAudioProcessor final : public juce::AudioProcessor
{
public:
    MavkaAudioProcessor();
    ~MavkaAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 14.0; }

    // factory presets live in the editor's preset menu (host program lists stay untouched)
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "MAVKA"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void loadFactoryPreset (int index);
    int  getFactoryPresetIndex() const noexcept { return presetIndex.load(); }

    static juce::String formatValue (mavka::Unit unit, float v);

    juce::AudioProcessorValueTreeState apvts;
    mavka::Engine engine;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void pullParams() noexcept;

    std::array<std::atomic<float>*, mavka::P_COUNT> raw {};
    std::atomic<int> presetIndex { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MavkaAudioProcessor)
};
