#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include "ChorusDSP.h"

namespace flchorus::param
{
inline constexpr const char* delay     = "delay";      // ms, 0..30
inline constexpr const char* depth     = "depth";      // ms
inline constexpr const char* stereo    = "stereo";     // degrees, 0..360
inline constexpr const char* lfoFreq[] = { "lfo1_freq", "lfo2_freq", "lfo3_freq" };
inline constexpr const char* lfoWave[] = { "lfo1_wave", "lfo2_wave", "lfo3_wave" };
inline constexpr const char* crossType = "cross_type"; // Process HF / Process LF
inline constexpr const char* cutoff    = "cross_cutoff";
inline constexpr const char* mix       = "mix";        // 0..100 %, 50 % = FL Studio default, 100 % = FL "Wet Only"
}

class FLChorusProcessor : public juce::AudioProcessor
{
public:
    FLChorusProcessor();
    ~FLChorusProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // For the editor: output peaks (L, R) and the left-channel LFO phases
    std::atomic<float> outputPeak[2] { { 0.0f }, { 0.0f } };
    std::atomic<float> lfoPhase[flchorus::numVoices] { { 0.0f }, { 0.0f }, { 0.0f } };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float>* pDelay = nullptr;
    std::atomic<float>* pDepth = nullptr;
    std::atomic<float>* pStereo = nullptr;
    std::atomic<float>* pFreq[flchorus::numVoices] {};
    std::atomic<float>* pWave[flchorus::numVoices] {};
    std::atomic<float>* pCross = nullptr;
    std::atomic<float>* pCutoff = nullptr;
    std::atomic<float>* pMix = nullptr;

    double sr = 44100.0;
    flchorus::Ring ring[2];
    flchorus::OnePole band[2];
    double phase[flchorus::numVoices] { 0.0, 0.0, 0.0 }; // radians
    bool wasPlaying = false;

    juce::SmoothedValue<double> delayMs, depthMs;
    juce::SmoothedValue<float> dryGain, wetScale;
    static float dryFor (float mix01) { return mix01 <= 0.5f ? 1.0f - 0.6f * mix01 : 1.4f * (1.0f - mix01); }
    static float wetFor (float mix01) { return mix01 <= 0.5f ? 2.0f * mix01 : 1.0f; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FLChorusProcessor)
};
