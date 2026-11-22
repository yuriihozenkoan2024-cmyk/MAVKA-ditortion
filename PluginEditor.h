#pragma once

#include "PluginProcessor.h"

#include <array>
#include <memory>
#include <vector>

namespace mavkaui
{
namespace colours
{
    inline const juce::Colour bgTop    { 0xff101917 };
    inline const juce::Colour bgBottom { 0xff060a09 };
    inline const juce::Colour panel    { 0xff111b19 };
    inline const juce::Colour edge     { 0xff233430 };
    inline const juce::Colour track    { 0xff1b2724 };
    inline const juce::Colour ink      { 0xff7fe3c7 };
    inline const juce::Colour amber    { 0xfff0a856 };
    inline const juce::Colour lilac    { 0xffb79cff };
    inline const juce::Colour sky      { 0xff7db8ff };
    inline const juce::Colour rose     { 0xffff7a85 };
    inline const juce::Colour pixel    { 0xfff28fd3 };
    inline const juce::Colour text     { 0xffe6efea };
    inline const juce::Colour dim      { 0xff8fa39c };
}

class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown, int buttonX, int buttonY,
                       int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                           float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
};

// console-style vertical fader (the AURA sends)
class Fader final : public juce::Component
{
public:
    Fader (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& title,
           juce::Colour accent, const juce::String& tooltip);
    void paint (juce::Graphics&) override;
    void resized() override;

    juce::Slider slider;

private:
    juce::String title;
    juce::Rectangle<int> titleArea, valueArea;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class Knob final : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& title,
          juce::Colour accent, bool large, const juce::String& tooltip);
    void paint (juce::Graphics&) override;
    void resized() override;

    juce::Slider slider;

private:
    juce::String title;
    bool large;
    juce::Rectangle<int> titleArea, valueArea;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class Choice final : public juce::Component
{
public:
    Choice (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& title,
            const juce::String& tooltip);
    void paint (juce::Graphics&) override;
    void resized() override;

    juce::ComboBox box;

private:
    juce::String title;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

// the live handwriting: MAVKA draws the stroke it writes into the voice
class StrokeView final : public juce::Component
{
public:
    static constexpr int N = 200;
    void push (float stroke, float slantMs, float levelDb, float ink);
    void paint (juce::Graphics&) override;

private:
    std::array<float, N> thick {}, offset {};
    int head = 0;
};

// the chord clock: four modulators orbiting at chord ratios
class ClockView final : public juce::Component
{
public:
    void setState (const float* phases, const juce::String& chordName, const juce::String& rateName);
    void paint (juce::Graphics&) override;

private:
    float ph[4] {};
    juce::String chord, rate;
};

// live duck map: how far the echo / forest steps aside in each of the 16 bands right now
class DuckMap final : public juce::Component, public juce::SettableTooltipClient
{
public:
    void setGains (const float* g);
    void paint (juce::Graphics&) override;

private:
    float gains[16] { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
};

struct Page final : public juce::Component
{
    juce::String description;
    void paint (juce::Graphics&) override;
};

struct Surface final : public juce::Component
{
    void paint (juce::Graphics&) override;
};
} // namespace mavkaui

//==============================================================================
class MavkaAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    static constexpr int baseW = 1060, baseH = 800;

    explicit MavkaAudioProcessorEditor (MavkaAudioProcessor&);
    ~MavkaAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void refreshDisplays() { timerCallback(); }
    void selectPage (int index);

private:
    void timerCallback() override;
    void showPage (int index);
    mavkaui::Knob& addKnob (juce::Component& parent, const juce::String& id, const juce::String& title,
                            juce::Colour accent, bool large, juce::Rectangle<int> bounds, const juce::String& tip);
    mavkaui::Choice& addChoice (juce::Component& parent, const juce::String& id, const juce::String& title,
                                juce::Rectangle<int> bounds, const juce::String& tip);

    MavkaAudioProcessor& proc;
    mavkaui::LookAndFeel lnf;
    mavkaui::Surface surface;
    std::unique_ptr<juce::TooltipWindow> tooltips;

    juce::ComboBox presetBox;
    juce::Label osRunning;
    juce::ToggleButton autoGain { "Auto gain" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoGainAttachment;

    mavkaui::StrokeView stroke;
    mavkaui::ClockView clock;
    mavkaui::DuckMap duckMap;
    std::array<juce::TextButton, 4> tabs;
    std::array<mavkaui::Page, 4> pages;

    std::vector<std::unique_ptr<mavkaui::Knob>> knobs;
    std::vector<std::unique_ptr<mavkaui::Choice>> choices;
    std::vector<std::unique_ptr<mavkaui::Fader>> faders;
    int lastPreset = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MavkaAudioProcessorEditor)
};
