#pragma once

#include "PluginProcessor.h"

// Same visual system as FL Soft Clipper: fixed design space, scaled to the window.
namespace flc::ui
{
inline constexpr int designW = 512, designH = 357;

struct Fonts
{
    juce::Typeface::Ptr archivoExtraBold, archivoSemiBold, monoBold;
    static Fonts& get();
    juce::Font title (float px) const;
    juce::Font semi (float px) const;
    juce::Font mono (float px) const;
};

class KnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    KnobLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
};

class PillButton : public juce::ToggleButton
{
public:
    explicit PillButton (const juce::String& label) : juce::ToggleButton (label) {}
    void paintButton (juce::Graphics&, bool isOver, bool isDown) override;
};

// Click to cycle Sine -> Triangle -> Multi-sine
class WaveChip : public juce::Component, public juce::SettableTooltipClient
{
public:
    WaveChip (juce::RangedAudioParameter&);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    int index = 0;
    juce::ParameterAttachment attach;
};

// All / Low / High selector for the crossover
class CrossSwitch : public juce::Component, public juce::SettableTooltipClient
{
public:
    CrossSwitch (juce::RangedAudioParameter&);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    int index = 0;
    juce::ParameterAttachment attach;
};

class Content : public juce::Component
{
public:
    explicit Content (FLChorusProcessor&);
    ~Content() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();

private:
    struct Knob
    {
        juce::String name;
        juce::Slider slider;
        juce::Label readout;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
        juce::Rectangle<int> block;
    };

    void setupKnob (Knob&, const char* paramId, const juce::String& name, juce::Rectangle<int> block);
    void drawBackground (juce::Graphics&);
    void drawLfoDisplay (juce::Graphics&);
    void drawMeter (juce::Graphics&);

    FLChorusProcessor& proc;
    KnobLookAndFeel lnf;
    Knob delay, depth, stereo, lfo[flchorus::numVoices], cutoff;
    std::unique_ptr<WaveChip> waves[flchorus::numVoices];
    std::unique_ptr<CrossSwitch> cross;
    juce::Rectangle<int> crossBlock;
    PillButton wetOnly { "WET ONLY" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> wetAttach;
    juce::HyperlinkButton link;

    juce::Image bgCache;
    float bgCacheScale = 0.0f;
    float outDisp[2] { 0.0f, 0.0f }, outHold = 0.0f;
    int holdCounter = 0;
    double shownPhase[flchorus::numVoices] { 0.0, 0.0, 0.0 };

    const juce::Rectangle<int> displayBox { 14, 64, 132, 132 };
    const juce::Rectangle<int> meterBox { 462, 64, 36, 132 };
};
} // namespace flc::ui

class FLChorusEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FLChorusEditor (FLChorusProcessor&);
    ~FLChorusEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void updateMeters() { content.tick(); }

private:
    void timerCallback() override { updateMeters(); }

    flc::ui::Content content;
    juce::ComponentBoundsConstrainer constrainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FLChorusEditor)
};
