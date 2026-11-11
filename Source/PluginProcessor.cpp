#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace flchorus;

namespace
{
juce::String num (float v, int decimals) { return juce::String (v, decimals); }
} // namespace

FLChorusProcessor::FLChorusProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pDelay  = apvts.getRawParameterValue (param::delay);
    pDepth  = apvts.getRawParameterValue (param::depth);
    pStereo = apvts.getRawParameterValue (param::stereo);
    for (int v = 0; v < numVoices; ++v)
    {
        pFreq[v] = apvts.getRawParameterValue (param::lfoFreq[v]);
        pWave[v] = apvts.getRawParameterValue (param::lfoWave[v]);
    }
    pCross   = apvts.getRawParameterValue (param::crossType);
    pCutoff  = apvts.getRawParameterValue (param::cutoff);
    pMix     = apvts.getRawParameterValue (param::mix);
}

juce::AudioProcessorValueTreeState::ParameterLayout FLChorusProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto attr = [] (std::function<String (float)> toText)
    {
        return AudioParameterFloatAttributes()
            .withStringFromValueFunction ([toText] (float v, int) { return toText (v); })
            .withValueFromStringFunction ([] (const String& s)
            {
                const auto t = s.toLowerCase();
                float v = t.retainCharacters ("-+.0123456789").getFloatValue();
                if (t.contains ("k")) v *= 1000.0f;
                return v;
            });
    };

    // Ranges and defaults as in FL Studio's Fruity Chorus
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { param::delay, 1 }, "Delay",
                                                       NormalisableRange<float> (0.05f, 30.0f, 0.0f), 15.025f,
                                                       attr ([] (float v) { return num (v, 3) + " ms"; }).withLabel ("ms")));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { param::depth, 1 }, "Depth",
                                                       NormalisableRange<float> (0.0f, 5.0f, 0.0f), 2.25f,
                                                       attr ([] (float v) { return num (v, 2) + " ms"; }).withLabel ("ms")));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { param::stereo, 1 }, "Stereo",
                                                       NormalisableRange<float> (0.0f, 360.0f, 0.0f), 59.94f,
                                                       attr ([] (float v) { return String ((int) std::floor (v + 1.0e-4f)) + juce::String::fromUTF8 ("\xc2\xb0"); }).withLabel ("deg")));

    const float defFreq[numVoices] { 0.45f, 1.25f, 2.45f };
    for (int v = 0; v < numVoices; ++v)
    {
        NormalisableRange<float> fr (0.0f, 5.0f, 0.0f, 0.5f); // knob position squared, like FL
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { param::lfoFreq[v], 1 }, "LFO " + String (v + 1) + " Freq", fr, defFreq[v],
                                                           attr ([] (float f) { return num (f, 2) + " Hz"; }).withLabel ("Hz")));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { param::lfoWave[v], 1 }, "LFO " + String (v + 1) + " Wave",
                                                            StringArray { "sin", "sin^3", "multi-sin", "multi-sin^3" }, 0));
    }

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { param::crossType, 1 }, "Cross Type",
                                                        StringArray { "Process HF", "Process LF" }, 0));

    NormalisableRange<float> cutRange (20.0f, 5127.8f, 0.0f);
    cutRange.setSkewForCentre (320.24371f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { param::cutoff, 1 }, "Cross Cutoff", cutRange, 320.24371f,
                                                       attr ([] (float f) { return f >= 1000.0f ? num (f / 1000.0f, 2) + " kHz"
                                                                                                : String (roundToInt (f)) + " Hz"; }).withLabel ("Hz")));

    // Dry/Wet: 0 % = dry only, 50 % = FL Studio's fixed blend (dry 0.7 + voices 0.42), 100 % = FL "Wet Only"
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { param::mix, 1 }, "Dry/Wet",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.0f), 50.0f,
                                                       attr ([] (float v) { return String (roundToInt (v)) + " %"; }).withLabel ("%")));
    return layout;
}

bool FLChorusProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void FLChorusProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& r : ring)
        r.prepare ((int) std::ceil (0.036 * sr) + 8);
    for (auto& b : band)
        b.reset();

    for (auto* s : { &delayMs, &depthMs })
        s->reset (sr, 0.03);
    dryGain.reset (sr, 0.02);
    wetScale.reset (sr, 0.02);
    delayMs.setCurrentAndTargetValue (pDelay->load());
    depthMs.setCurrentAndTargetValue (pDepth->load());
    dryGain.setCurrentAndTargetValue (dryFor (pMix->load() * 0.01f));
    wetScale.setCurrentAndTargetValue (wetFor (pMix->load() * 0.01f));

    for (auto& p : phase)
        p = 0.0;
    wasPlaying = false;
}

void FLChorusProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (2, getTotalNumOutputChannels());
    const int n = buffer.getNumSamples();

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);

    // FL starts its LFOs from zero when playback starts
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            playing = pos->getIsPlaying();
    if (playing && ! wasPlaying)
        for (auto& p : phase)
            p = 0.0;
    wasPlaying = playing;

    delayMs.setTargetValue (pDelay->load());
    depthMs.setTargetValue (pDepth->load());
    dryGain.setTargetValue (dryFor (pMix->load() * 0.01f));
    wetScale.setTargetValue (wetFor (pMix->load() * 0.01f));

    const bool lowBand = juce::roundToInt (pCross->load()) == 1;
    const double p = OnePole::coef (juce::jlimit (1.0, 0.49 * sr, (double) pCutoff->load()), sr);
    const double stereoRad = (double) pStereo->load() * (twoPi / 360.0);

    Wave wave[numVoices];
    double inc[numVoices];
    for (int v = 0; v < numVoices; ++v)
    {
        wave[v] = (Wave) juce::jlimit (0, 3, juce::roundToInt (pWave[v]->load()));
        const double hz = pFreq[v]->load();
        inc[v] = twoPi * hz * lfoRateCorrection (hz) / sr;
    }

    const double msToSamples = sr / 1000.0;
    float pk[2] = { 0.0f, 0.0f };
    float* data[2] = { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };

    for (int i = 0; i < n; ++i)
    {
        const double d = delayMs.getNextValue();
        const double dp = depthMs.getNextValue();
        const float dg = dryGain.getNextValue();
        const float wg = voiceGain * wetScale.getNextValue();

        for (int c = 0; c < numCh; ++c)
        {
            const float x = data[c][i];
            const double banded = lowBand ? band[c].lowPass (x, p) : band[c].highPass (x, p);
            ring[c].push ((float) banded);

            float wet = 0.0f;
            const double off = c == 1 ? stereoRad : 0.0;
            for (int v = 0; v < numVoices; ++v)
            {
                const double lfo = lfoValue (wave[v], phase[v] + off);
                wet += ring[c].read ((d + dp * (0.5 + 0.5 * lfo)) * msToSamples);
            }

            const float y = dg * x + wg * wet;
            data[c][i] = y;
            pk[c] = juce::jmax (pk[c], std::abs (y));
        }

        for (int v = 0; v < numVoices; ++v)
        {
            phase[v] += inc[v];
            if (phase[v] >= twoPi) phase[v] -= twoPi;
        }
    }

    if (numCh == 1) pk[1] = pk[0];
    for (int c = 0; c < 2; ++c)
        if (pk[c] > outputPeak[c].load (std::memory_order_relaxed))
            outputPeak[c].store (pk[c], std::memory_order_relaxed);
    for (int v = 0; v < numVoices; ++v)
        lfoPhase[v].store ((float) (phase[v] / twoPi), std::memory_order_relaxed);
}

void FLChorusProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void FLChorusProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* FLChorusProcessor::createEditor()
{
    return new FLChorusEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FLChorusProcessor();
}
