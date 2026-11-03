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
    pWetOnly = apvts.getRawParameterValue (param::wetOnly);
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

    NormalisableRange<float> delayRange (0.0f, 30.0f, 0.01f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { param::delay, 1 }, "Delay", delayRange, 10.0f,
                                                       attr ([] (float v) { return num (v, 1) + " ms"; }).withLabel ("ms")));

    NormalisableRange<float> depthRange (0.0f, 20.0f, 0.01f);
    depthRange.setSkewForCentre (5.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { param::depth, 1 }, "Depth", depthRange, 3.0f,
                                                       attr ([] (float v) { return num (v, 1) + " ms"; }).withLabel ("ms")));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { param::stereo, 1 }, "Stereo",
                                                       NormalisableRange<float> (0.0f, 360.0f, 1.0f), 90.0f,
                                                       attr ([] (float v) { return String (roundToInt (v)) + juce::String::fromUTF8 ("\xc2\xb0"); }).withLabel ("deg")));

    const float defFreq[numVoices] { 0.42f, 0.71f, 1.13f };
    for (int v = 0; v < numVoices; ++v)
    {
        NormalisableRange<float> fr (0.01f, 10.0f, 0.001f);
        fr.setSkewForCentre (1.0f);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { param::lfoFreq[v], 1 }, "LFO " + String (v + 1) + " Freq", fr, defFreq[v],
                                                           attr ([] (float f) { return num (f, f < 1.0f ? 2 : 1) + " Hz"; }).withLabel ("Hz")));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { param::lfoWave[v], 1 }, "LFO " + String (v + 1) + " Wave",
                                                            StringArray { "Sine", "Triangle", "Multi-sine" }, 0));
    }

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { param::crossType, 1 }, "Cross Type",
                                                        StringArray { "All", "Process LP", "Process HP" }, 0));

    NormalisableRange<float> cutRange (20.0f, 20000.0f, 1.0f);
    cutRange.setSkewForCentre (1000.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { param::cutoff, 1 }, "Cross Cutoff", cutRange, 500.0f,
                                                       attr ([] (float f) { return f >= 1000.0f ? num (f / 1000.0f, f >= 10000.0f ? 1 : 2) + " kHz"
                                                                                                : String (roundToInt (f)) + " Hz"; }).withLabel ("Hz")));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { param::wetOnly, 1 }, "Wet Only", false));
    return layout;
}

bool FLChorusProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void FLChorusProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    for (auto& r : ring)
        r.prepare ((int) std::ceil (0.055 * sr) + 8);

    crossover.prepare ({ sr, (juce::uint32) juce::jmax (1, samplesPerBlock), 2 });
    crossover.reset();
    lastCutoff = -1.0f;

    const double ramp = 0.03;
    for (auto* s : { &delayMs, &depthMs, &stereoTurns, &wetGain, &dryGain })
        s->reset (sr, ramp);

    const bool wo = pWetOnly->load() > 0.5f;
    delayMs.setCurrentAndTargetValue (pDelay->load());
    depthMs.setCurrentAndTargetValue (pDepth->load());
    stereoTurns.setCurrentAndTargetValue (pStereo->load() / 360.0f);
    wetGain.setCurrentAndTargetValue (wo ? 1.0f : 0.5f);
    dryGain.setCurrentAndTargetValue (wo ? 0.0f : 1.0f);

    phase[0] = 0.0; phase[1] = 1.0 / 3.0; phase[2] = 2.0 / 3.0;
}

void FLChorusProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (2, getTotalNumOutputChannels());
    const int n = buffer.getNumSamples();

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);

    const bool wo = pWetOnly->load() > 0.5f;
    delayMs.setTargetValue (pDelay->load());
    depthMs.setTargetValue (pDepth->load());
    stereoTurns.setTargetValue (pStereo->load() / 360.0f);
    wetGain.setTargetValue (wo ? 1.0f : 0.5f);
    dryGain.setTargetValue (wo ? 0.0f : 1.0f);

    const int cross = juce::roundToInt (pCross->load()); // 0 all, 1 process LP, 2 process HP
    const float cut = pCutoff->load();
    if (std::abs (cut - lastCutoff) > 0.01f)
    {
        crossover.setCutoffFrequency (juce::jlimit (20.0f, (float) (0.45 * sr), cut));
        lastCutoff = cut;
    }

    Wave wave[numVoices];
    double inc[numVoices];
    for (int v = 0; v < numVoices; ++v)
    {
        wave[v] = (Wave) juce::roundToInt (pWave[v]->load());
        inc[v] = (double) pFreq[v]->load() / sr;
    }

    const double msToSamples = sr / 1000.0;
    float pk[2] = { 0.0f, 0.0f };
    float* data[2] = { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };

    for (int i = 0; i < n; ++i)
    {
        const double d = delayMs.getNextValue();
        const double dp = depthMs.getNextValue();
        const double st = stereoTurns.getNextValue();
        const float wg = wetGain.getNextValue();
        const float dg = dryGain.getNextValue();

        for (int c = 0; c < numCh; ++c)
        {
            const float x = data[c][i];
            float proc = x, rest = 0.0f;
            if (cross != 0)
            {
                float lo = 0.0f, hi = 0.0f;
                crossover.processSample (c, x, lo, hi);
                proc = cross == 1 ? lo : hi;
                rest = cross == 1 ? hi : lo;
            }

            ring[c].push (proc);

            float wet = 0.0f;
            for (int v = 0; v < numVoices; ++v)
            {
                double ph = phase[v] + (c == 1 ? st : 0.0);
                ph -= std::floor (ph);
                const double lfo = lfoValue (wave[v], ph);
                const double dl = juce::jmax (2.0, (d + dp * (0.5 + 0.5 * lfo)) * msToSamples);
                wet += ring[c].read (dl);
            }
            wet *= 1.0f / (float) numVoices;

            const float y = dg * (rest + 0.5f * proc) + wg * wet;
            data[c][i] = y;
            pk[c] = juce::jmax (pk[c], std::abs (y));
        }

        for (int v = 0; v < numVoices; ++v)
        {
            phase[v] += inc[v];
            if (phase[v] >= 1.0) phase[v] -= 1.0;
        }
    }

    if (numCh == 1) pk[1] = pk[0];
    for (int c = 0; c < 2; ++c)
        if (pk[c] > outputPeak[c].load (std::memory_order_relaxed))
            outputPeak[c].store (pk[c], std::memory_order_relaxed);
    for (int v = 0; v < numVoices; ++v)
        lfoPhase[v].store ((float) phase[v], std::memory_order_relaxed);
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
