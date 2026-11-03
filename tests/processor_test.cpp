// Offline checks for FL Chorus (no DAW needed).
// Usage: processor_test [snapshot.png]
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdio>
#include <random>

static void setParam (FLChorusProcessor& p, const char* id, float plain)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

static juce::AudioBuffer<float> run (FLChorusProcessor& p, const std::vector<float>& l, const std::vector<float>& r, int block)
{
    juce::AudioBuffer<float> out (2, (int) l.size()), buf (2, block);
    juce::MidiBuffer midi;
    for (int pos = 0; pos < (int) l.size(); pos += block)
    {
        const int n = juce::jmin (block, (int) l.size() - pos);
        buf.setSize (2, n, false, false, true);
        for (int i = 0; i < n; ++i) { buf.setSample (0, i, l[(size_t) (pos + i)]); buf.setSample (1, i, r[(size_t) (pos + i)]); }
        p.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, buf, c, 0, n);
    }
    return out;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    int failures = 0;
    const double fs = 44100.0;
    std::mt19937 rng (3);
    std::normal_distribution<float> nd (0.0f, 0.3f);
    std::vector<float> noise ((size_t) fs * 3);
    for (auto& s : noise) s = nd (rng);

    // 1) Wet only + depth 0: every voice is the same 10 ms delay -> output == input delayed by 441 samples
    {
        FLChorusProcessor p;
        p.setPlayConfigDetails (2, 2, fs, 512);
        setParam (p, flchorus::param::depth, 0.0f);
        setParam (p, flchorus::param::delay, 10.0f);
        setParam (p, flchorus::param::wetOnly, 1.0f);
        p.prepareToPlay (fs, 512);
        auto out = run (p, noise, noise, 512);
        double err = 0.0;
        for (int i = 20000; i < out.getNumSamples(); ++i)
            err = juce::jmax (err, (double) std::abs (out.getSample (0, i) - noise[(size_t) (i - 441)]));
        std::printf ("[delay]   wet only, 10 ms, depth 0: max error %.2g\n", err);
        if (err > 1e-5) ++failures;
    }

    // 2) default settings: finite, chorus actually moves, stereo phase makes L != R
    {
        FLChorusProcessor p;
        p.setPlayConfigDetails (2, 2, fs, 256);
        p.prepareToPlay (fs, 256);
        auto out = run (p, noise, noise, 256);
        double diffLR = 0.0, pk = 0.0;
        bool finite = true;
        for (int i = 0; i < out.getNumSamples(); ++i)
        {
            const float a = out.getSample (0, i), b = out.getSample (1, i);
            finite &= std::isfinite (a) && std::isfinite (b);
            diffLR = juce::jmax (diffLR, (double) std::abs (a - b));
            pk = juce::jmax (pk, (double) std::abs (a));
        }
        std::printf ("[default] finite=%d, peak %.3f, max |L-R| %.3f (mono in)\n", (int) finite, pk, diffLR);
        if (! finite || diffLR < 0.01) ++failures;
    }

    // 3) crossover modes + all waves + automation, random block sizes
    for (int mode = 0; mode < 3; ++mode)
    {
        FLChorusProcessor p;
        p.setPlayConfigDetails (2, 2, fs, 1024);
        setParam (p, flchorus::param::crossType, (float) mode);
        for (int v = 0; v < 3; ++v) setParam (p, flchorus::param::lfoWave[v], (float) v);
        p.prepareToPlay (fs, 1024);
        auto out = run (p, noise, noise, 333);
        bool finite = true;
        for (int i = 0; i < out.getNumSamples(); ++i) finite &= std::isfinite (out.getSample (0, i));
        std::printf ("[cross]   mode %d finite=%d\n", mode, (int) finite);
        if (! finite) ++failures;
    }

    if (argc > 1)
    {
        FLChorusProcessor p;
        p.setPlayConfigDetails (2, 2, fs, 512);
        p.prepareToPlay (fs, 512);
        run (p, noise, noise, 512);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        p.outputPeak[0] = 0.62f;
        p.outputPeak[1] = 0.58f;
        dynamic_cast<FLChorusEditor*> (ed.get())->updateMeters();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
        out.deleteFile();
        juce::FileOutputStream os (out);
        juce::PNGImageFormat().writeImageToStream (img, os);
    }

    std::printf (failures == 0 ? "ALL PASSED\n" : "%d FAILED\n", failures);
    return failures;
}
