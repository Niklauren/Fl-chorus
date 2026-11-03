#include "PluginEditor.h"
#include "BinaryData.h"

namespace flc::ui
{
namespace col
{
const juce::Colour bg (0xff0b120a);
const juce::Colour text (0xffe6efdc);
const juce::Colour title (0xffeaf2df);
const juce::Colour sub (0xffa9c98e);
const juce::Colour lime (0xffd7f25a);
const juce::Colour arc (0xff7dc22e);
const juce::Colour knob (0xff2a352c);
const juce::Colour red (0xffe8462a);
const juce::Colour on (0xffc9e84a);
const juce::Colour meterLo (0xff63b83e);
const juce::Colour meterMid (0xffd6ee45);
const juce::Colour voice[flchorus::numVoices] { juce::Colour (0xffd7f25a), juce::Colour (0xff9bd630), juce::Colour (0xff4fa83a) };
} // namespace col

static const juce::String kInstagram = "https://www.instagram.com/mikita_laurenau/";

//==============================================================================
Fonts& Fonts::get()
{
    static Fonts f = []
    {
        Fonts r;
        r.archivoExtraBold = juce::Typeface::createSystemTypefaceFor (BinaryData::ArchivoExtraBold_ttf, BinaryData::ArchivoExtraBold_ttfSize);
        r.archivoSemiBold  = juce::Typeface::createSystemTypefaceFor (BinaryData::ArchivoSemiBold_ttf, BinaryData::ArchivoSemiBold_ttfSize);
        r.monoBold         = juce::Typeface::createSystemTypefaceFor (BinaryData::JetBrainsMonoBold_ttf, BinaryData::JetBrainsMonoBold_ttfSize);
        return r;
    }();
    return f;
}

juce::Font Fonts::title (float px) const { return juce::Font (juce::FontOptions (archivoExtraBold).withPointHeight (px)); }
juce::Font Fonts::semi (float px) const  { return juce::Font (juce::FontOptions (archivoSemiBold).withPointHeight (px)); }
juce::Font Fonts::mono (float px) const  { return juce::Font (juce::FontOptions (monoBold).withPointHeight (px)); }

static float textWidth (const juce::Font& f, const juce::String& s) { return juce::GlyphArrangement::getStringWidth (f, s); }

static void glassBox (juce::Graphics& g, juce::Rectangle<float> r, float radius, float fillAlpha, float strokeAlpha)
{
    g.setColour (juce::Colours::white.withAlpha (fillAlpha));
    g.fillRoundedRectangle (r, radius);
    g.setColour (juce::Colours::white.withAlpha (strokeAlpha));
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
}

static void readoutBox (juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r, radius);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
}

//==============================================================================
KnobLookAndFeel::KnobLookAndFeel()
{
    setColour (juce::Label::textColourId, col::lime);
    setColour (juce::Label::textWhenEditingColourId, col::lime);
    setColour (juce::Label::backgroundWhenEditingColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::outlineWhenEditingColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::textColourId, col::lime);
    setColour (juce::TextEditor::highlightColourId, col::arc.withAlpha (0.45f));
    setColour (juce::TextEditor::highlightedTextColourId, juce::Colours::white);
    setColour (juce::CaretComponent::caretColourId, col::lime);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff1a2418));
    setColour (juce::TooltipWindow::textColourId, col::text);
    setColour (juce::TooltipWindow::outlineColourId, juce::Colours::white.withAlpha (0.12f));
}

void KnobLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider&)
{
    const float s = (float) juce::jmin (w, h) / 84.0f;
    const float cx = (float) x + (float) w * 0.5f, cy = (float) y + (float) h * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const juce::PathStrokeType stroke (4.0f * s, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc (cx, cy, 38.0f * s, 38.0f * s, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.strokePath (track, stroke);

    if (pos > 0.002f)
    {
        juce::Path value;
        value.addCentredArc (cx, cy, 38.0f * s, 38.0f * s, 0.0f, startAngle, angle, true);
        g.setColour (col::arc);
        g.strokePath (value, stroke);
    }

    const float r = 28.0f * s;
    g.setColour (col::knob);
    g.fillEllipse (cx - r, cy - r, 2.0f * r, 2.0f * r);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f * s, -22.0f * s, 3.0f * s, 14.0f * s, 1.5f * s);
    g.setColour (col::lime);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (cx, cy));
}

void KnobLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& l)
{
    if (l.isBeingEdited())
        return;
    g.setColour (l.findColour (juce::Label::textColourId));
    g.setFont (l.getFont());
    g.drawFittedText (l.getText(), l.getLocalBounds(), juce::Justification::centred, 1, 1.0f);
}

//==============================================================================
void PillButton::paintButton (juce::Graphics& g, bool isOver, bool)
{
    const auto on = getToggleState();
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float rad = r.getHeight() * 0.5f;

    g.setColour (on ? col::on : juce::Colours::white.withAlpha (isOver ? 0.12f : 0.08f));
    g.fillRoundedRectangle (r, rad);
    g.setColour (juce::Colours::white.withAlpha (0.16f));
    g.drawRoundedRectangle (r, rad, 1.0f);

    const float ledX = r.getX() + 10.0f, cy = r.getCentreY();
    g.setColour (on ? col::red : juce::Colours::white.withAlpha (0.25f));
    g.fillEllipse (ledX, cy - 3.0f, 6.0f, 6.0f);

    g.setColour (on ? col::bg : col::text);
    g.setFont (Fonts::get().title (11.0f).withExtraKerningFactor (0.04f));
    g.drawText (getButtonText(), juce::Rectangle<float> (ledX + 12.0f, r.getY(), r.getRight() - ledX - 12.0f, r.getHeight()),
                juce::Justification::centredLeft, false);
}

//==============================================================================
WaveChip::WaveChip (juce::RangedAudioParameter& p)
    : attach (p, [this] (float v) { index = juce::roundToInt (v); repaint(); })
{
    attach.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip ("LFO wave: click to switch Sine / Triangle / Multi-sine");
}

void WaveChip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float rad = r.getHeight() * 0.5f;
    g.setColour (juce::Colours::white.withAlpha (isMouseOver() ? 0.13f : 0.08f));
    g.fillRoundedRectangle (r, rad);
    g.setColour (juce::Colours::white.withAlpha (0.14f));
    g.drawRoundedRectangle (r, rad, 1.0f);

    // tiny wave icon
    const juce::Rectangle<float> icon (r.getX() + 9.0f, r.getCentreY() - 4.0f, 16.0f, 8.0f);
    juce::Path p;
    for (int i = 0; i <= 32; ++i)
    {
        const double ph = i / 32.0;
        const float v = (float) flchorus::lfoValue ((flchorus::Wave) index, ph);
        const float px = icon.getX() + icon.getWidth() * (float) ph, py = icon.getCentreY() - v * icon.getHeight() * 0.5f;
        if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
    }
    g.setColour (col::lime);
    g.strokePath (p, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    static const char* names[] = { "SINE", "TRIANGLE", "MULTI" };
    g.setColour (col::text);
    g.setFont (Fonts::get().title (9.0f).withExtraKerningFactor (0.04f));
    g.drawText (names[juce::jlimit (0, 2, index)], juce::Rectangle<float> (icon.getRight() + 5.0f, r.getY(), r.getRight() - icon.getRight() - 9.0f, r.getHeight()),
                juce::Justification::centredLeft, false);
}

void WaveChip::mouseUp (const juce::MouseEvent& e)
{
    if (! getLocalBounds().contains (e.getPosition()))
        return;
    attach.setValueAsCompleteGesture ((float) ((index + 1) % 3));
}

//==============================================================================
CrossSwitch::CrossSwitch (juce::RangedAudioParameter& p)
    : attach (p, [this] (float v) { index = juce::roundToInt (v); repaint(); })
{
    attach.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip ("ALL: chorus on the whole signal. LOW / HIGH: only below / above the cutoff");
}

void CrossSwitch::paint (juce::Graphics& g)
{
    static const char* names[] = { "ALL", "LOW", "HIGH" };
    const float h = ((float) getHeight() - 10.0f) / 3.0f;
    for (int i = 0; i < 3; ++i)
    {
        const juce::Rectangle<float> r (0.5f, 0.5f + (float) i * (h + 5.0f), (float) getWidth() - 1.0f, h - 1.0f);
        const bool sel = i == index;
        g.setColour (sel ? col::on : juce::Colours::white.withAlpha (0.08f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
        g.setColour (sel ? col::bg : col::text);
        g.setFont (Fonts::get().title (11.0f).withExtraKerningFactor (0.04f));
        g.drawText (names[i], r, juce::Justification::centred, false);
    }
}

void CrossSwitch::mouseUp (const juce::MouseEvent& e)
{
    if (! getLocalBounds().contains (e.getPosition()))
        return;
    const float h = ((float) getHeight() - 10.0f) / 3.0f;
    const int i = juce::jlimit (0, 2, (int) ((float) e.y / (h + 5.0f)));
    attach.setValueAsCompleteGesture ((float) i);
}

//==============================================================================
Content::Content (FLChorusProcessor& p)
    : proc (p), link ("recreated by mikita_laurenau", juce::URL (kInstagram))
{
    using namespace flchorus;
    // row A: display, Delay / Depth / Stereo, meter (like FL Soft Clipper)
    setupKnob (delay,  param::delay,  "Delay",  { 162, 72, 92, 115 });
    setupKnob (depth,  param::depth,  "Depth",  { 258, 72, 92, 115 });
    setupKnob (stereo, param::stereo, "Stereo", { 354, 72, 92, 115 });

    // row B: LFO 1..3 (+ wave chips), Cutoff, Cross
    for (int v = 0; v < numVoices; ++v)
    {
        setupKnob (lfo[v], param::lfoFreq[v], "LFO " + juce::String (v + 1), { 18 + v * 96, 206, 92, 115 });
        waves[v] = std::make_unique<WaveChip> (*proc.apvts.getParameter (param::lfoWave[v]));
        addAndMakeVisible (*waves[v]);
    }
    setupKnob (cutoff, param::cutoff, "Cutoff", { 18 + 3 * 96, 206, 92, 115 });

    crossBlock = { 18 + 4 * 96, 206, 92, 137 };
    cross = std::make_unique<CrossSwitch> (*proc.apvts.getParameter (param::crossType));
    addAndMakeVisible (*cross);

    wetOnly.setTooltip ("Only the chorus voices, no dry signal (for use on a send)");
    wetOnly.setClickingTogglesState (true);
    addAndMakeVisible (wetOnly);
    wetAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, param::wetOnly, wetOnly);

    link.setFont (Fonts::get().semi (10.0f), false, juce::Justification::centredLeft);
    link.setColour (juce::HyperlinkButton::textColourId, col::sub);
    link.setTooltip (kInstagram);
    addAndMakeVisible (link);
}

Content::~Content()
{
    for (auto* k : { &delay, &depth, &stereo, &lfo[0], &lfo[1], &lfo[2], &cutoff })
    {
        k->slider.setLookAndFeel (nullptr);
        k->readout.setLookAndFeel (nullptr);
    }
}

void Content::setupKnob (Knob& k, const char* paramId, const juce::String& name, juce::Rectangle<int> block)
{
    k.name = name;
    k.block = block;

    auto& s = k.slider;
    s.setLookAndFeel (&lnf);
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    s.setMouseDragSensitivity (220);
    addAndMakeVisible (s);
    k.attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, s);

    if (auto* prm = proc.apvts.getParameter (paramId))
        s.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));

    auto& r = k.readout;
    r.setLookAndFeel (&lnf);
    r.setFont (Fonts::get().mono (12.0f));
    r.setJustificationType (juce::Justification::centred);
    r.setEditable (false, true, false);
    r.setTooltip ("Double-click to type a value");
    r.setText (s.getTextFromValue (s.getValue()), juce::dontSendNotification);
    r.onTextChange = [&k] { k.slider.setValue (k.slider.getValueFromText (k.readout.getText()), juce::sendNotificationSync); };
    r.onEditorShow = [&r]
    {
        if (auto* ed = r.getCurrentTextEditor())
        {
            ed->setJustification (juce::Justification::centred);
            ed->setFont (Fonts::get().mono (12.0f));
            ed->setText (r.getText().retainCharacters ("-+.0123456789kK"), false);
            ed->selectAll();
        }
    };
    addAndMakeVisible (r);

    s.onValueChange = [this, &k]
    {
        k.readout.setText (k.slider.getTextFromValue (k.slider.getValue()), juce::dontSendNotification);
        repaint (displayBox.expanded (2));
    };
}

void Content::resized()
{
    for (auto* k : { &delay, &depth, &stereo, &lfo[0], &lfo[1], &lfo[2], &cutoff })
    {
        const auto b = k->block;
        k->slider.setBounds (b.getX() + 4, b.getY() + 18, 84, 84);
        k->readout.setBounds (b.getX() + 4, b.getY() + 92, 84, 23);
    }
    for (int v = 0; v < flchorus::numVoices; ++v)
        waves[v]->setBounds (lfo[v].block.getX() + 4, lfo[v].block.getY() + 119, 84, 18);

    cross->setBounds (crossBlock.getX() + 4, crossBlock.getY() + 20, 84, 96);

    const int pillW = 26 + (int) std::ceil (textWidth (Fonts::get().title (11.0f).withExtraKerningFactor (0.04f), wetOnly.getButtonText())) + 10;
    wetOnly.setBounds (486 - pillW, 21, pillW, 26);

    const auto titleW = textWidth (Fonts::get().title (22.0f).withExtraKerningFactor (-0.015f), "FL Chorus");
    const auto linkW = textWidth (Fonts::get().semi (10.0f), link.getButtonText());
    link.setBounds ((int) (28.0f + titleW + 9.0f), 27, (int) std::ceil (linkW) + 4, 17);
}

//==============================================================================
static void softBlob (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float a0, float a1, float mid, float blur)
{
    r = r.expanded (blur * 0.6f);
    const float rx = r.getWidth() * 0.5f, ry = r.getHeight() * 0.5f;
    juce::ColourGradient grad (c.withAlpha (a0), r.getCentreX(), r.getCentreY(),
                               c.withAlpha (0.0f), r.getCentreX() + rx, r.getCentreY(), true);
    grad.addColour (mid, c.withAlpha (a1));
    grad.addColour (0.85, c.withAlpha (a1 * 0.25f));

    juce::Graphics::ScopedSaveState ss (g);
    g.addTransform (juce::AffineTransform::scale (1.0f, ry / rx, r.getCentreX(), r.getCentreY()));
    g.setGradientFill (grad);
    g.fillEllipse (r.getCentreX() - rx, r.getCentreY() - rx, 2.0f * rx, 2.0f * rx);
}

static void blobs (juce::Graphics& g)
{
    softBlob (g, { 60.0f, 40.0f, 300.0f, 200.0f }, juce::Colour (0xff63b83e), 0.42f, 0.18f, 0.55f, 22.0f);
    softBlob (g, { 240.0f, 150.0f, 260.0f, 190.0f }, juce::Colour (0xffd6ee45), 0.24f, 0.09f, 0.55f, 20.0f);
    softBlob (g, { 170.0f, -20.0f, 240.0f, 150.0f }, juce::Colour (0xff3e7d2e), 0.40f, 0.15f, 0.60f, 24.0f);
    softBlob (g, { 0.0f, 230.0f, 280.0f, 160.0f }, juce::Colour (0xff3e7d2e), 0.32f, 0.12f, 0.60f, 24.0f);
}

void Content::drawBackground (juce::Graphics& g)
{
    g.fillAll (col::bg);
    blobs (g);

    // header glass
    const juce::Rectangle<float> header (14.0f, 14.0f, 484.0f, 40.0f);
    juce::Path hp;
    hp.addRoundedRectangle (header, 14.0f);
    juce::DropShadow (juce::Colours::black.withAlpha (0.35f), 18, { 0, 6 }).drawForPath (g, hp);
    g.setColour (col::bg);
    g.fillPath (hp);
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (hp);
        blobs (g);
    }
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.fillPath (hp);
    g.setColour (juce::Colours::white.withAlpha (0.14f));
    g.strokePath (hp, juce::PathStrokeType (1.0f));

    g.setColour (col::title);
    g.setFont (Fonts::get().title (22.0f).withExtraKerningFactor (-0.015f));
    g.drawText ("FL Chorus", juce::Rectangle<float> (28.0f, 14.0f, 260.0f, 40.0f), juce::Justification::centredLeft, false);

    glassBox (g, displayBox.toFloat(), 12.0f, 0.05f, 0.12f);

    for (auto* k : { &delay, &depth, &stereo, &lfo[0], &lfo[1], &lfo[2], &cutoff })
    {
        const auto b = k->block.toFloat();
        g.setColour (col::text);
        g.setFont (Fonts::get().title (13.0f).withExtraKerningFactor (-0.01f));
        g.drawText (k->name, juce::Rectangle<float> (b.getX(), b.getY(), b.getWidth(), 16.0f), juce::Justification::centred, false);
        readoutBox (g, { b.getX() + 4.0f, b.getY() + 92.0f, 84.0f, 23.0f }, 7.0f);
    }

    g.setColour (col::text);
    g.setFont (Fonts::get().title (13.0f).withExtraKerningFactor (-0.01f));
    g.drawText ("Cross", juce::Rectangle<float> ((float) crossBlock.getX(), (float) crossBlock.getY(), (float) crossBlock.getWidth(), 16.0f),
                juce::Justification::centred, false);

    // meter frame
    const auto m = meterBox.toFloat();
    g.setColour (col::text);
    g.setFont (Fonts::get().title (10.0f));
    g.drawText ("OUT", juce::Rectangle<float> (m.getX(), m.getY(), m.getWidth(), 13.0f), juce::Justification::centred, false);
    for (int c = 0; c < 2; ++c)
    {
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRoundedRectangle ({ m.getX() + 8.5f + (float) c * 11.0f, m.getY() + 18.0f, 8.0f, 90.0f }, 4.0f);
    }
    readoutBox (g, { m.getX(), m.getBottom() - 19.0f, m.getWidth(), 19.0f }, 5.0f);
}

void Content::drawLfoDisplay (juce::Graphics& g)
{
    using namespace flchorus;
    const auto box = displayBox.toFloat();
    const auto area = box.reduced (10.0f).withTrimmedTop (14.0f).withTrimmedBottom (12.0f);

    const float d = proc.apvts.getRawParameterValue (param::delay)->load();
    const float dp = proc.apvts.getRawParameterValue (param::depth)->load();

    g.setColour (col::text.withAlpha (0.6f));
    g.setFont (Fonts::get().mono (8.5f));
    g.drawText (juce::String (d + dp, 1) + " ms", juce::Rectangle<float> (box.getX() + 10.0f, box.getY() + 6.0f, 100.0f, 11.0f), juce::Justification::left, false);
    g.drawText (juce::String (d, 1) + " ms", juce::Rectangle<float> (box.getX() + 10.0f, box.getBottom() - 16.0f, 100.0f, 11.0f), juce::Justification::left, false);

    const float dash[] = { 3.0f, 3.0f };
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawDashedLine ({ area.getX(), area.getY(), area.getRight(), area.getY() }, dash, 2, 1.0f);
    g.drawDashedLine ({ area.getX(), area.getBottom(), area.getRight(), area.getBottom() }, dash, 2, 1.0f);

    const double window = 2.0; // seconds shown
    for (int v = numVoices - 1; v >= 0; --v)
    {
        const auto w = (Wave) juce::roundToInt (proc.apvts.getRawParameterValue (param::lfoWave[v])->load());
        const double f = proc.apvts.getRawParameterValue (param::lfoFreq[v])->load();
        juce::Path p;
        float endX = 0.0f, endY = 0.0f;
        const int steps = 120;
        for (int i = 0; i <= steps; ++i)
        {
            const double t = -window + window * i / steps;
            double ph = shownPhase[v] + f * t;
            ph -= std::floor (ph);
            const float val = (float) (0.5 + 0.5 * lfoValue (w, ph));
            endX = area.getX() + area.getWidth() * (float) i / (float) steps;
            endY = dp > 0.001f ? area.getBottom() - val * area.getHeight() : area.getBottom();
            if (i == 0) p.startNewSubPath (endX, endY); else p.lineTo (endX, endY);
        }
        g.setColour (col::voice[v]);
        g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.fillEllipse (endX - 3.5f, endY - 3.5f, 7.0f, 7.0f);
        g.setColour (col::bg);
        g.drawEllipse (endX - 3.5f, endY - 3.5f, 7.0f, 7.0f, 1.4f);
    }
}

void Content::drawMeter (juce::Graphics& g)
{
    const auto m = meterBox.toFloat();
    for (int c = 0; c < 2; ++c)
    {
        const auto inner = juce::Rectangle<float> (m.getX() + 8.5f + (float) c * 11.0f, m.getY() + 18.0f, 8.0f, 90.0f).reduced (2.0f);
        const float db = juce::Decibels::gainToDecibels (outDisp[c], -100.0f);
        const float h = inner.getHeight() * juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 54.0f);
        if (h <= 0.5f)
            continue;
        juce::ColourGradient grad (col::meterLo, 0.0f, inner.getBottom(), col::red, 0.0f, inner.getY(), false);
        grad.addColour (0.82, col::meterMid);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (inner.withTop (inner.getBottom() - h), 2.0f);
    }

    const juce::String minus = juce::String::fromUTF8 ("\xe2\x88\x92");
    juce::String txt = minus + juce::String::fromUTF8 ("\xe2\x88\x9e");
    if (outHold > 0.00001f)
    {
        const float db = juce::Decibels::gainToDecibels (outHold);
        txt = (db > 0.05f ? "+" : (db < -0.05f ? minus : juce::String())) + juce::String (std::abs (db), 1);
    }
    g.setColour (col::lime);
    g.setFont (Fonts::get().mono (9.0f));
    g.drawText (txt, juce::Rectangle<float> (m.getX(), m.getBottom() - 19.0f, m.getWidth(), 19.0f), juce::Justification::centred, false);
}

void Content::paint (juce::Graphics& g)
{
    const float total = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (bgCache.isNull() || std::abs (total - bgCacheScale) > 0.01f)
    {
        bgCacheScale = total;
        bgCache = juce::Image (juce::Image::ARGB, (int) std::ceil (designW * total), (int) std::ceil (designH * total), true);
        juce::Graphics ig (bgCache);
        ig.addTransform (juce::AffineTransform::scale (total));
        drawBackground (ig);
    }
    g.drawImage (bgCache, juce::Rectangle<float> (0.0f, 0.0f, (float) designW, (float) designH));

    drawLfoDisplay (g);
    drawMeter (g);
}

void Content::tick()
{
    float blockMax = 0.0f;
    for (int c = 0; c < 2; ++c)
    {
        const float o = proc.outputPeak[c].exchange (0.0f);
        outDisp[c] = o > outDisp[c] ? o : outDisp[c] * 0.85f;
        blockMax = juce::jmax (blockMax, o);
    }
    if (blockMax >= outHold) { outHold = blockMax; holdCounter = 30; }
    else if (--holdCounter <= 0) outHold *= 0.8f;

    for (int v = 0; v < flchorus::numVoices; ++v)
        shownPhase[v] = proc.lfoPhase[v].load (std::memory_order_relaxed);

    repaint (displayBox.expanded (2));
    repaint (meterBox.expanded (2));
}
} // namespace flc::ui

//==============================================================================
FLChorusEditor::FLChorusEditor (FLChorusProcessor& p)
    : AudioProcessorEditor (&p), content (p)
{
    addAndMakeVisible (content);
    constrainer.setFixedAspectRatio ((double) flc::ui::designW / (double) flc::ui::designH);
    constrainer.setSizeLimits (flc::ui::designW, flc::ui::designH, flc::ui::designW * 3, flc::ui::designH * 3);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize ((int) (flc::ui::designW * 1.5), (int) std::round (flc::ui::designH * 1.5));
    startTimerHz (30);
}

FLChorusEditor::~FLChorusEditor() = default;

void FLChorusEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff0b120a)); }

void FLChorusEditor::resized()
{
    const float s = (float) getWidth() / (float) flc::ui::designW;
    content.setBounds (0, 0, flc::ui::designW, flc::ui::designH);
    content.setTransform (juce::AffineTransform::scale (s));
}
