#include "PluginEditor.h"

using namespace mavkaui;

namespace
{
    juce::Font uiFont (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
    }

    bool isBipolar (const juce::String& id)
    {
        return id == "ink" || id == "slant" || id == "bias" || id == "tone" || id == "preTilt" || id == "output";
    }
}

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, colours::track);
    setColour (juce::ComboBox::outlineColourId, colours::edge);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::arrowColourId, colours::ink);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff0c1312));
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::ink.withAlpha (0.22f));
    setColour (juce::PopupMenu::highlightedTextColourId, colours::text);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff0c1312));
    setColour (juce::TooltipWindow::textColourId, colours::text);
    setColour (juce::TooltipWindow::outlineColourId, colours::edge);
    setColour (juce::ToggleButton::textColourId, colours::text);
    setColour (juce::ToggleButton::tickColourId, colours::ink);
    setColour (juce::ToggleButton::tickDisabledColourId, colours::dim);
    setColour (juce::TextButton::buttonColourId, colours::track);
    setColour (juce::TextButton::buttonOnColourId, colours::ink.withAlpha (0.25f));
    setColour (juce::TextButton::textColourOffId, colours::dim);
    setColour (juce::TextButton::textColourOnId, colours::text);
    setColour (juce::Label::textColourId, colours::dim);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                    float startA, float endA, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float lineW = juce::jmax (2.5f, radius * 0.12f);
    const float arcR = radius - lineW * 0.5f;
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
    const juce::PathStrokeType strokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startA, endA, true);
    g.setColour (colours::track);
    g.strokePath (track, strokeType);

    const bool bipolar = (bool) s.getProperties()["bipolar"];
    const float angle = startA + pos * (endA - startA);
    const float from = bipolar ? 0.5f * (startA + endA) : startA;
    if (std::abs (angle - from) > 0.01f)
    {
        juce::Path value;
        value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (accent.withAlpha (s.isEnabled() ? 1.0f : 0.4f));
        g.strokePath (value, strokeType);
    }

    const float bodyR = arcR - lineW * 1.25f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff22302c), c.x, c.y - bodyR,
                                             juce::Colour (0xff0c1311), c.x, c.y + bodyR, false));
    g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (colours::edge);
    g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

    // pointer: an ink drop sliding toward the rim
    const float inner = bodyR * 0.25f, outer = bodyR * 0.8f;
    const juce::Point<float> p0 (c.x + inner * std::sin (angle), c.y - inner * std::cos (angle));
    const juce::Point<float> p1 (c.x + outer * std::sin (angle), c.y - outer * std::cos (angle));
    g.setColour (accent);
    g.drawLine ({ p0, p1 }, juce::jmax (2.0f, bodyR * 0.09f));
    const float dot = juce::jmax (2.5f, bodyR * 0.11f);
    g.fillEllipse (p1.x - dot, p1.y - dot, dot * 2.0f, dot * 2.0f);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (box.hasKeyboardFocus (false) ? colours::ink.withAlpha (0.6f) : colours::edge);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    juce::Path arrow;
    const float ax = (float) width - 14.0f, ay = (float) height * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow);
}

juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox&) { return uiFont (13.0f); }

void LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font LookAndFeel::getPopupMenuFont() { return uiFont (14.0f); }

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? colours::ink.withAlpha (0.16f) : (over || down ? colours::track.brighter (0.15f) : colours::track));
    g.fillRoundedRectangle (r, 5.0f);
    if (on)
    {
        g.setColour (colours::ink);
        g.fillRoundedRectangle (r.withTop (r.getBottom() - 2.5f).reduced (8.0f, 0.0f), 1.2f);
    }
}

juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int) { return uiFont (12.5f, true); }

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float minPos,
                                    float maxPos, juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minPos, maxPos, style, s);
        return;
    }
    const auto accent = s.findColour (juce::Slider::trackColourId);
    const float cx = (float) x + (float) width * 0.5f;
    const float top = (float) y + 8.0f, bottom = (float) (y + height) - 8.0f;

    // scale ticks
    g.setColour (colours::edge);
    for (int i = 0; i <= 10; ++i)
    {
        const float ty = top + (bottom - top) * (float) i / 10.0f;
        const float len = (i % 5 == 0) ? 9.0f : 5.0f;
        g.drawLine (cx - 14.0f - len, ty, cx - 14.0f, ty, 1.0f);
        g.drawLine (cx + 14.0f, ty, cx + 14.0f + len, ty, 1.0f);
    }
    // groove and ink fill
    const juce::Rectangle<float> groove (cx - 3.0f, top, 6.0f, bottom - top);
    g.setColour (colours::track.darker (0.3f));
    g.fillRoundedRectangle (groove, 3.0f);
    const float pos = juce::jlimit (top, bottom, sliderPos);
    g.setColour (accent.withAlpha (0.85f));
    g.fillRoundedRectangle (juce::Rectangle<float> (cx - 3.0f, pos, 6.0f, bottom - pos), 3.0f);

    // fader cap
    const juce::Rectangle<float> cap (cx - 17.0f, pos - 9.0f, 34.0f, 18.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b3a36), cap.getX(), cap.getY(),
                                             juce::Colour (0xff121a18), cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 4.0f);
    g.setColour (colours::edge.brighter (0.3f));
    g.drawRoundedRectangle (cap, 4.0f, 1.0f);
    g.setColour (accent);
    g.fillRect (cap.getX() + 5.0f, pos - 1.0f, cap.getWidth() - 10.0f, 2.0f);
}

//==============================================================================
Fader::Fader (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& t,
              juce::Colour accent, const juce::String& tooltip)
    : title (t)
{
    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::trackColourId, accent);
    slider.setTooltip (tooltip);
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
    if (auto* p = state.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, (double) p->convertFrom0to1 (p->getDefaultValue()));
    slider.onValueChange = [this] { repaint (valueArea); };
}

void Fader::resized()
{
    auto b = getLocalBounds();
    titleArea = b.removeFromTop (17);
    valueArea = b.removeFromBottom (16);
    slider.setBounds (b);
}

void Fader::paint (juce::Graphics& g)
{
    g.setColour (colours::text);
    g.setFont (uiFont (12.5f, true));
    g.drawText (title.toUpperCase(), titleArea, juce::Justification::centred, false);
    g.setColour (colours::dim);
    g.setFont (uiFont (11.0f));
    g.drawText (slider.getTextFromValue (slider.getValue()), valueArea, juce::Justification::centred, false);
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& t,
            juce::Colour accent, bool isLarge, const juce::String& tooltip)
    : title (t), large (isLarge)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.getProperties().set ("bipolar", isBipolar (paramId));
    slider.setMouseDragSensitivity (large ? 260 : 200);
    slider.setTooltip (tooltip);
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
    if (auto* p = state.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, (double) p->convertFrom0to1 (p->getDefaultValue()));
    slider.onValueChange = [this] { repaint (valueArea); };
}

void Knob::resized()
{
    auto b = getLocalBounds();
    titleArea = b.removeFromTop (large ? 18 : 16);
    valueArea = b.removeFromBottom (large ? 18 : 16);
    slider.setBounds (b);
}

void Knob::paint (juce::Graphics& g)
{
    g.setColour (colours::text);
    g.setFont (uiFont (large ? 13.0f : 11.0f, true));
    g.drawText (title.toUpperCase(), titleArea, juce::Justification::centred, false);
    g.setColour (colours::dim);
    g.setFont (uiFont (large ? 12.5f : 11.0f));
    g.drawText (slider.getTextFromValue (slider.getValue()), valueArea, juce::Justification::centred, false);
}

//==============================================================================
Choice::Choice (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& t,
                const juce::String& tooltip)
    : title (t)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramId)))
        box.addItemList (p->choices, 1);
    box.setTooltip (tooltip);
    addAndMakeVisible (box);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, paramId, box);
}

void Choice::resized()
{
    auto b = getLocalBounds();
    b.removeFromTop (16);
    box.setBounds (b.removeFromTop (26));
}

void Choice::paint (juce::Graphics& g)
{
    g.setColour (colours::dim);
    g.setFont (uiFont (11.0f, true));
    g.drawText (title.toUpperCase(), getLocalBounds().removeFromTop (16), juce::Justification::centredLeft, false);
}

//==============================================================================
void StrokeView::push (float stroke, float slantMs, float levelDb, float ink)
{
    const float lvl = mavka::smoothstep (-36.0f, -3.0f, levelDb);   // relative to the voice's recent loudness
    float shape;
    if (ink >= 0.0f) shape = 1.0f - 0.85f * ink * stroke;                  // thin at the start, then blooms
    else             shape = juce::jmin (1.0f, 0.55f - 0.6f * ink * stroke); // pressed at the start
    thick[(size_t) head] = 1.0f + 17.0f * lvl * shape;
    offset[(size_t) head] = juce::jlimit (-1.0f, 1.0f, slantMs / 2.5f) * 16.0f;
    head = (head + 1) % N;
}

void StrokeView::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0b1211));
    g.fillRoundedRectangle (r, 6.0f);

    // ruled paper
    g.setColour (colours::edge.withAlpha (0.55f));
    for (int i = 1; i < 4; ++i)
        g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * (float) i / 4.0f), r.getX() + 8.0f, r.getRight() - 8.0f);

    const float mid = r.getCentreY();
    juce::Path ribbon;
    for (int i = 0; i < N; ++i)
    {
        const int k = (head + i) % N;
        const float x = r.getX() + 6.0f + (r.getWidth() - 12.0f) * (float) i / (float) (N - 1);
        const float y = mid + offset[(size_t) k] - thick[(size_t) k];
        if (i == 0) ribbon.startNewSubPath (x, y);
        else        ribbon.lineTo (x, y);
    }
    for (int i = N - 1; i >= 0; --i)
    {
        const int k = (head + i) % N;
        const float x = r.getX() + 6.0f + (r.getWidth() - 12.0f) * (float) i / (float) (N - 1);
        ribbon.lineTo (x, mid + offset[(size_t) k] + thick[(size_t) k]);
    }
    ribbon.closeSubPath();
    g.setGradientFill (juce::ColourGradient (colours::ink.withAlpha (0.0f), r.getX(), mid,
                                             colours::ink.withAlpha (0.95f), r.getRight(), mid, false));
    g.fillPath (ribbon);

    // the nib
    const int last = (head + N - 1) % N;
    const float nx = r.getRight() - 6.0f, ny = mid + offset[(size_t) last];
    g.setColour (colours::text);
    g.fillEllipse (nx - 3.0f, ny - 3.0f, 6.0f, 6.0f);
}

//==============================================================================
void ClockView::setState (const float* phases, const juce::String& chordName, const juce::String& rateName)
{
    for (int i = 0; i < 4; ++i)
        ph[i] = phases[i];
    chord = chordName;
    rate = rateName;
}

void ClockView::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (6.0f);
    const float cx = r.getCentreX(), cy = r.getCentreY();
    const float R = 0.5f * juce::jmin (r.getWidth(), r.getHeight());
    const juce::Colour dots[4] = { colours::ink, colours::sky, colours::lilac, colours::amber };

    for (int i = 0; i < 4; ++i)
    {
        const float rad = R * (0.40f + 0.19f * (float) i);
        g.setColour (colours::edge);
        g.drawEllipse (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f, 1.0f);

        const float a = juce::MathConstants<float>::twoPi * ph[i];
        juce::Path trail;
        trail.addCentredArc (cx, cy, rad, rad, 0.0f, a - 1.1f, a, true);
        g.setColour (dots[i].withAlpha (0.45f));
        g.strokePath (trail, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float x = cx + rad * std::sin (a), y = cy - rad * std::cos (a);
        g.setColour (dots[i]);
        g.fillEllipse (x - 5.0f, y - 5.0f, 10.0f, 10.0f);
    }

    g.setColour (colours::text);
    g.setFont (uiFont (13.0f, true));
    g.drawText (chord, juce::Rectangle<float> (cx - 60.0f, cy - 16.0f, 120.0f, 18.0f), juce::Justification::centred, false);
    g.setColour (colours::dim);
    g.setFont (uiFont (11.0f));
    g.drawText ("cycle " + rate, juce::Rectangle<float> (cx - 60.0f, cy + 2.0f, 120.0f, 16.0f), juce::Justification::centred, false);
}

//==============================================================================
void DuckMap::setGains (const float* g)
{
    for (int b = 0; b < 16; ++b)
        gains[b] = g[b];
}

void DuckMap::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0b1211));
    g.fillRoundedRectangle (r, 5.0f);
    const auto inner = r.reduced (5.0f, 6.0f).withTrimmedBottom (12.0f);
    const float bw = inner.getWidth() / 16.0f;
    for (int b = 0; b < 16; ++b)
    {
        const float db = juce::jlimit (0.0f, 18.0f, -20.0f * std::log10 (juce::jmax (gains[b], 1.0e-3f)));
        const float h = juce::jmax (1.0f, inner.getHeight() * db / 18.0f);
        g.setColour (colours::ink.withAlpha (0.2f + 0.8f * db / 18.0f));
        g.fillRect (juce::Rectangle<float> (inner.getX() + (float) b * bw + 0.5f, inner.getY(), bw - 1.0f, h));
    }
    g.setColour (colours::dim);
    g.setFont (uiFont (9.5f));
    g.drawText ("low  -  high", r.withTop (r.getBottom() - 15.0f), juce::Justification::centred, false);
}

//==============================================================================
void Page::paint (juce::Graphics& g)
{
    g.setColour (colours::dim);
    g.setFont (uiFont (12.0f));
    g.drawFittedText (description, getLocalBounds().removeFromTop (34).reduced (6, 2), juce::Justification::topLeft, 2);
}

void Surface::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (colours::bgTop, 0.0f, 0.0f, colours::bgBottom, 0.0f, b.getBottom(), false));
    g.fillAll();

    // calligraphic flourish behind the title
    juce::Path swash;
    swash.startNewSubPath (150.0f, 46.0f);
    swash.cubicTo (215.0f, 6.0f, 300.0f, 60.0f, 400.0f, 18.0f);
    swash.cubicTo (300.0f, 66.0f, 212.0f, 13.0f, 150.0f, 46.0f);
    swash.closeSubPath();
    g.setGradientFill (juce::ColourGradient (colours::ink.withAlpha (0.0f), 150.0f, 40.0f, colours::ink.withAlpha (0.35f), 400.0f, 20.0f, false));
    g.fillPath (swash);

    g.setColour (colours::text);
    g.setFont (juce::Font (juce::FontOptions (34.0f, juce::Font::bold)).withExtraKerningFactor (0.2f));
    g.drawText ("MAVKA", 22, 6, 220, 38, juce::Justification::centredLeft, false);
    g.setColour (colours::dim);
    g.setFont (uiFont (12.0f));
    g.drawText ("vocal handwriting distortion", 24, 40, 260, 16, juce::Justification::centredLeft, false);
    g.setFont (uiFont (11.0f, true));
    g.drawText ("PRESET", 470, 4, 120, 14, juce::Justification::centredLeft, false);

    struct Panel { juce::Rectangle<float> r; const char* title; const char* sub; juce::Colour accent; };
    const Panel panels[] =
    {
        { { 12.0f, 64.0f, 430.0f, 250.0f },  "HANDWRITING", "how MAVKA writes every word",          colours::ink },
        { { 452.0f, 64.0f, 596.0f, 250.0f }, "CORE",        "macros",                    colours::amber },
        { { 12.0f, 322.0f, 250.0f, 256.0f }, "CHORD CLOCK", "",                           colours::sky },
        { { 272.0f, 322.0f, 776.0f, 256.0f }, "",           "",                                     colours::lilac },
        { { 12.0f, 586.0f, 1036.0f, 202.0f }, "AURA",       "nonlinear send effects",               colours::ink },
    };
    for (const auto& p : panels)
    {
        g.setColour (colours::panel);
        g.fillRoundedRectangle (p.r, 8.0f);
        g.setColour (colours::edge);
        g.drawRoundedRectangle (p.r, 8.0f, 1.0f);
        if (*p.title != 0)
        {
            g.setColour (p.accent);
            g.fillRoundedRectangle (p.r.getX() + 12.0f, p.r.getY() + 12.0f, 3.0f, 14.0f, 1.5f);
            g.setColour (colours::text);
            g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)).withExtraKerningFactor (0.12f));
            g.drawText (p.title, (int) p.r.getX() + 20, (int) p.r.getY() + 10, 160, 18, juce::Justification::centredLeft, false);
            g.setColour (colours::dim);
            g.setFont (uiFont (11.5f));
            g.drawText (p.sub, (int) p.r.getX() + 160, (int) p.r.getY() + 10, (int) p.r.getWidth() - 172, 18,
                        juce::Justification::centredRight, false);
        }
    }
    // AURA strip separators
    g.setColour (colours::edge);
    for (float sx : { 214.0f, 562.0f, 770.0f, 858.0f })
        g.drawLine (sx, 620.0f, sx, 778.0f, 1.0f);
}

//==============================================================================
MavkaAudioProcessorEditor::MavkaAudioProcessorEditor (MavkaAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (surface);
    tooltips = std::make_unique<juce::TooltipWindow> (this, 700);

    // ---- header
    int count = 0;
    const mavka::Preset* presets = mavka::factoryPresets (count);
    for (int i = 0; i < count; ++i)
        presetBox.addItem (presets[i].name, i + 1);
    presetBox.setTooltip ("Factory presets. Each one keeps MAVKA's handwriting - from silk to dirt.");
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id > 0 && id - 1 != proc.getFactoryPresetIndex())
            proc.loadFactoryPreset (id - 1);
        lastPreset = id - 1;
    };
    presetBox.setBounds (470, 20, 210, 26);
    surface.addAndMakeVisible (presetBox);

    addChoice (surface, "os", "Oversampling", { 700, 4, 120, 44 },
               "Auto picks the internal rate from the distortion amount and your project's sample rate (and goes one step higher while rendering).");
    osRunning.setBounds (826, 22, 90, 26);
    osRunning.setFont (uiFont (12.0f));
    osRunning.setColour (juce::Label::textColourId, colours::ink);
    surface.addAndMakeVisible (osRunning);

    autoGain.setTooltip ("Keeps the wet signal as loud as the dry one, so you judge character, not volume.");
    autoGain.setBounds (930, 20, 116, 26);
    surface.addAndMakeVisible (autoGain);
    autoGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "autoGain", autoGain);

    // ---- handwriting
    stroke.setBounds (24, 94, 406, 86);
    surface.addAndMakeVisible (stroke);
    addKnob (surface, "ink", "Ink", colours::ink, true, { 22, 186, 104, 122 },
             "Ink: every word starts clean and the distortion spreads over it. Below zero = Press: a hard hit that lifts off.");
    addKnob (surface, "slant", "Slant", colours::ink, true, { 128, 186, 104, 122 },
             "Slant: on every syllable the doubles draw a small pitch stroke. Scoop = dip and rise, Fall = up and settle.");
    addKnob (surface, "ligature", "Ligature", colours::ink, true, { 234, 186, 104, 122 },
             "Ligature: a dirty echo in time with the song that only speaks between words, joining them.");
    addKnob (surface, "inkTime", "Ink time", colours::ink, false, { 342, 196, 90, 106 },
             "Length of one pen stroke (Ink and Slant).");

    // ---- core macros
    struct M { const char* id; const char* t; juce::Colour c; const char* tip; };
    const M macros[] =
    {
        { "drive",  "Drive",  colours::amber, "How hard the three impossible machines are pushed." },
        { "grit",   "Grit",   colours::rose,  "From silk to dirt: harder clipping, deeper folding." },
        { "crush",  "Crush",  colours::pixel, "Harmonic crusher: the sample-and-hold clock follows the pitch of your voice, "
                                              "so the crushed voice still sings in tune. Mode in the HARDWARE tab." },
        { "melody", "Melody", colours::lilac, "How loud the harp in the lamp rings." },
        { "motion", "Motion", colours::sky,   "Depth of the whole chord clock." },
        { "mix",    "Mix",    colours::text,  "Dry / wet. Use 100% on a parallel bus." },
        { "output", "Output", colours::dim,   "Output level." },
    };
    for (int i = 0; i < 7; ++i)
        addKnob (surface, macros[i].id, macros[i].t, macros[i].c, true, { 460 + i * 84, 104, 84, 160 }, macros[i].tip);

    // ---- chord clock visual
    clock.setBounds (32, 352, 210, 210);
    surface.addAndMakeVisible (clock);

    // ---- tabs
    const char* tabNames[4] = { "HARDWARE", "CLOCK", "HARP", "VOICE" };
    for (int i = 0; i < 4; ++i)
    {
        auto& t = tabs[(size_t) i];
        t.setButtonText (tabNames[i]);
        t.setClickingTogglesState (true);
        t.setRadioGroupId (4242);
        t.setBounds (284 + i * 124, 332, 120, 28);
        t.onClick = [this, i] { showPage (i); };
        surface.addAndMakeVisible (t);

        auto& pg = pages[(size_t) i];
        pg.setBounds (284, 366, 756, 204);
        surface.addChildComponent (pg);
    }
    pages[0].description = "Three machines that cannot exist: a lamp whose bias breathes, a speaker cone with a 20 cm throw "
                           "(glassy self-modulation), a folding diode, and a crusher that samples in tune with your pitch. The ghosts feed octaves INTO them.";
    pages[1].description = "Every modulator runs at a rate taken from a chord (4:5:6 = major, 10:12:15 = minor...), locked to your bars. "
                           "The movement repeats like a groove instead of drifting.";
    pages[2].description = "Strings tuned to your song, struck by the distorted voice: a melodic halo even on rap. Set the key of your track.";
    pages[3].description = "How MAVKA listens: the same saturation for whispers and shouts, vowels sing, consonants stay clear.";

    {
        auto& pg = pages[0];
        struct K { const char* id; const char* t; juce::Colour c; const char* tip; };
        const K ks[] =
        {
            { "lamp",      "Lamp",     colours::amber, "The lead: a warm asymmetric tube. Stays in tune and on time." },
            { "glass",     "Glass",    colours::sky,   "Doppler glass double: the voice moves its own speaker cone." },
            { "fold",      "Fold",     colours::rose,  "Folding diode double: metallic dirt." },
            { "doppler",   "Doppler",  colours::sky,   "Throw of the impossible speaker cone: more = glassier, singing harmonics." },
            { "bias",      "Bias",     colours::amber, "Lamp asymmetry: even harmonics, warmth. Breathes with the clock." },
            { "ghostDown", "Ghost dn", colours::lilac, "Octave below (with its natural fifth), fed into the machines." },
            { "ghostUp",   "Ghost up", colours::lilac, "Octave above, fed into the machines." },
        };
        for (int i = 0; i < 7; ++i)
            addKnob (pg, ks[i].id, ks[i].t, ks[i].c, false, { 2 + i * 92, 56, 90, 132 }, ks[i].tip);
        addChoice (pg, "crushMode", "Crush mode", { 652, 80, 100, 44 },
                   "Harmonic: steps locked to your pitch (in tune). Digital: classic fixed-rate lo-fi. "
                   "Clock: the resolution jumps in octaves with the chord clock.");
    }
    {
        auto& pg = pages[1];
        addChoice (pg, "rate", "Cycle", { 6, 46, 130, 44 }, "Length of the base cycle, in bars or notes.");
        addChoice (pg, "ratio", "Chord", { 6, 104, 130, 44 }, "The chord the four modulators play: their rates relate like the notes of this chord.");
        struct K { const char* id; const char* t; const char* tip; };
        const K ks[] =
        {
            { "width",    "Width",    "Stereo spread of the doubles, clock and harp." },
            { "vibrato",  "Vibrato",  "Micro pitch motion of the lead." },
            { "ensemble", "Ensemble", "Chorus motion of the doubles." },
            { "breath",   "Breath",   "Lamp bias breathing (even harmonics come and go)." },
            { "pulse",    "Pulse",    "The two doubles trade places in rhythm." },
            { "orbit",    "Orbit",    "The doubles circle around the lead in the stereo field." },
            { "warp",     "Warp",     "Phase warp before the machines: the timbre turns like a phaser inside the tube." },
        };
        for (int i = 0; i < 7; ++i)
            addKnob (pg, ks[i].id, ks[i].t, colours::sky, false, { 146 + i * 87, 56, 84, 132 }, ks[i].tip);
    }
    {
        auto& pg = pages[2];
        addChoice (pg, "key", "Key", { 6, 80, 110, 44 }, "Key of your song.");
        addChoice (pg, "chord", "Chord", { 126, 80, 130, 44 }, "Which notes the strings are tuned to.");
        addChoice (pg, "register", "Register", { 266, 80, 110, 44 }, "Octave of the harp.");
        addKnob (pg, "decay", "Decay", colours::lilac, false, { 410, 56, 100, 132 }, "How long the strings ring.");
        addKnob (pg, "bright", "Bright", colours::lilac, false, { 520, 56, 100, 132 }, "String brightness.");
        addKnob (pg, "harpDrive", "Harp drive", colours::lilac, false, { 630, 56, 100, 132 }, "Overdrive inside the strings.");
    }
    {
        auto& pg = pages[3];
        struct K { const char* id; const char* t; const char* tip; };
        const K ks[] =
        {
            { "consist",  "Consistency", "Same saturation for whispers and shouts (dynamics are restored afterwards)." },
            { "focus",    "Vowel focus", "The distortion sings on vowels; consonants and S's stay clean." },
            { "smooth",   "Smooth",      "Tames harsh upper mids in the wet signal." },
            { "lowCut",   "Low cut",     "Low cut before the machines." },
            { "highCut",  "High cut",    "High cut before the machines (less fizz)." },
            { "preTilt",  "Pre tilt",    "Tilts what the machines hear: dark / bright bite." },
            { "tone",     "Tone",        "Tone of the wet signal." },
        };
        for (int i = 0; i < 7; ++i)
            addKnob (pg, ks[i].id, ks[i].t, colours::ink, false, { 4 + i * 107, 56, 100, 132 }, ks[i].tip);
    }

    // ---- AURA: nonlinear send effects
    auto addFader = [this] (const char* id, const char* title, juce::Colour c, juce::Rectangle<int> r, const char* tip)
    {
        faders.push_back (std::make_unique<mavkaui::Fader> (proc.apvts, id, title, c, tip));
        faders.back()->setBounds (r);
        surface.addAndMakeVisible (*faders.back());
    };
    addFader ("echo", "Echo", colours::sky, { 26, 620, 64, 164 },
              "Tape echo in time with the song. Every repeat is re-saturated and narrowed, so the echoes get grainier as they fade. Ducks while you sing.");
    addChoice (surface, "echoTime", "Time", { 98, 622, 104, 44 }, "Echo time (Slap or note values at the project tempo).");
    addKnob (surface, "echoFb", "Feedback", colours::sky, false, { 98, 670, 100, 114 }, "How many repeats.");

    addFader ("forest", "Forest", colours::ink, { 224, 620, 64, 164 },
              "Reverb with saturation inside its loop: loud parts compress, tails bloom in the gaps.");
    addChoice (surface, "forestType", "Type", { 296, 622, 118, 44 }, "Room, Plate, Hall or Cathedral.");
    addKnob (surface, "forestDecay", "Decay", colours::ink, false, { 296, 670, 86, 114 }, "Reverb time.");
    addKnob (surface, "forestTune", "Tune", colours::ink, false, { 384, 670, 86, 114 },
             "Pulls the room onto the harp's key and chord: the reverb sings in tune.");
    addKnob (surface, "forestBloom", "Bloom", colours::ink, false, { 472, 670, 86, 114 },
             "Holds the reverb back while you sing and lets it bloom in the pauses.");

    addFader ("twin", "Twin", colours::lilac, { 572, 620, 64, 164 },
              "Detuned doubles whose spread breathes with your voice and the clock; each twin has its own throat.");
    addChoice (surface, "twinType", "Type", { 644, 622, 114, 44 }, "Tight, Wide, Lush, Choir (4 twins) or Octave (a shadow below).");

    addFader ("spin", "Spin", colours::amber, { 780, 620, 64, 164 },
              "The whole aura turns around your head in time with the chord clock (also wobbles echo, forest and twins).");

    addChoice (surface, "duck", "Duck", { 870, 622, 168, 44 },
               "Spectral: the echo and forest step aside only in the frequencies your voice is using right now. "
               "Classic: the whole return ducks. Off: no ducking.");
    duckMap.setBounds (868, 674, 54, 106);
    duckMap.setTooltip ("Duck map: how far the echo / forest steps aside in each band right now (low to high).");
    surface.addAndMakeVisible (duckMap);
    addKnob (surface, "duckDepth", "Depth", colours::ink, false, { 924, 670, 62, 112 },
             "Spectral: how far the echo / forest must stay under your voice in the bands it uses (6..24 dB). "
             "Classic: how deep the whole return dips.");
    addKnob (surface, "feed", "Feed", colours::text, false, { 986, 670, 62, 112 },
             "What goes into the aura: 0% = your clean voice, 100% = MAVKA's distorted voice.");

    tabs[0].setToggleState (true, juce::dontSendNotification);
    showPage (0);

    setResizable (true, true);
    setResizeLimits ((int) (baseW * 0.7), (int) (baseH * 0.7), (int) (baseW * 1.6), (int) (baseH * 1.6));
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) baseW / (double) baseH);
    setSize (baseW, baseH);

    timerCallback();
    startTimerHz (30);
}

MavkaAudioProcessorEditor::~MavkaAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

mavkaui::Knob& MavkaAudioProcessorEditor::addKnob (juce::Component& parent, const juce::String& id, const juce::String& title,
                                                   juce::Colour accent, bool large, juce::Rectangle<int> bounds, const juce::String& tip)
{
    knobs.push_back (std::make_unique<mavkaui::Knob> (proc.apvts, id, title, accent, large, tip));
    auto& k = *knobs.back();
    k.setBounds (bounds);
    parent.addAndMakeVisible (k);
    return k;
}

mavkaui::Choice& MavkaAudioProcessorEditor::addChoice (juce::Component& parent, const juce::String& id, const juce::String& title,
                                                       juce::Rectangle<int> bounds, const juce::String& tip)
{
    choices.push_back (std::make_unique<mavkaui::Choice> (proc.apvts, id, title, tip));
    auto& c = *choices.back();
    c.setBounds (bounds);
    parent.addAndMakeVisible (c);
    return c;
}

void MavkaAudioProcessorEditor::selectPage (int index)
{
    tabs[(size_t) juce::jlimit (0, 3, index)].setToggleState (true, juce::dontSendNotification);
    showPage (juce::jlimit (0, 3, index));
}

void MavkaAudioProcessorEditor::showPage (int index)
{
    for (int i = 0; i < 4; ++i)
        pages[(size_t) i].setVisible (i == index);
}

void MavkaAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::bgBottom);
}

void MavkaAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) baseW;
    surface.setBounds (0, 0, baseW, baseH);
    surface.setTransform (juce::AffineTransform::scale (scale));
}

void MavkaAudioProcessorEditor::timerCallback()
{
    auto& e = proc.engine;
    const float ink = proc.apvts.getRawParameterValue ("ink")->load();
    stroke.push (e.getInkStroke(), e.getSlantMs(), e.getRelativeDb(), ink);
    stroke.repaint();

    float ph[4];
    for (int i = 0; i < 4; ++i)
        ph[i] = e.getClockPhase (i);
    const int ratio = juce::jlimit (0, 7, (int) proc.apvts.getRawParameterValue ("ratio")->load());
    const int rate = juce::jlimit (0, 6, (int) proc.apvts.getRawParameterValue ("rate")->load());
    clock.setState (ph, mavka::kRatioChoices[ratio], mavka::kRateChoices[rate]);
    clock.repaint();

    osRunning.setText ("running " + juce::String (e.getActiveFactor()) + "x", juce::dontSendNotification);

    float dg[16];
    for (int b = 0; b < 16; ++b)
        dg[b] = e.getDuckGain (b);
    duckMap.setGains (dg);
    duckMap.repaint();

    const int preset = proc.getFactoryPresetIndex();
    if (preset != lastPreset)
    {
        presetBox.setSelectedId (preset + 1, juce::dontSendNotification);
        lastPreset = preset;
    }
}
