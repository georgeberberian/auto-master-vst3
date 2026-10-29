#pragma once
#include <JuceHeader.h>
#include <array>
#include <deque>
#include <vector>

//==============================================================================
// ITU-R BS.1770 K-weighted loudness meter
class LoudnessMeter
{
public:
    void prepare (double sampleRate);
    void reset();

    void process (const juce::AudioBuffer<float>& buffer);

    float momentaryLUFS() const;   // 400 ms
    float shortTermLUFS() const;  // 3 s
    float integratedLUFS() const; // gated (abs -70 / rel -10)
    float lra() const;            // loudness range
    float peakDb() const { return peakDb; }
    bool  ready() const { return blocks.size() >= 30; } // ~3 s of audio

    static double powerToLufs (double p)
    {
        return -0.691 + 10.0 * std::log10 (std::max (p, 1e-12));
    }

private:
    void pushBlock (double power);
    void ensureFilters (int numChannels);

    double sampleRate = 48000.0;
    int blockLen = 4800, blockFill = 0;
    double blockAccum = 0.0;
    float peakDb = -100.0f, peakHold = 0.0f;

    std::deque<double> blocks;             // 100 ms block powers (10 min cap)
    std::deque<double> shortTermHistory;  // 3 s window powers (3 min cap)

    // K-weighting: high-shelf (+4 dB) then RLB high-pass, per channel
    std::vector<std::array<juce::dsp::IIR::Filter<double>, 2>> kFilters;
};

//==============================================================================
class AutoMasterAudioProcessor : public juce::AudioProcessor
{
public:
    AutoMasterAudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Auto-Master"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() const override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Public for the editor
    juce::AudioProcessorValueTreeState apvts;
    LoudnessMeter meter;
    std::atomic<float> agcGainDb         { 0.0f };
    std::atomic<float> harshReductionDb { 0.0f };
    void resetMeter() { meter.reset(); agcGainDb.store (0.0f); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    double sampleRate = 48000.0;

    std::array<juce::dsp::IIR::Filter<float>, 2> warmthShelf, airShelf, harshBand;
    std::array<juce::SmoothedValue<float>, 2> harshGain;
    juce::dsp::ProcessorDuplicator<juce::dsp::Compressor<float>> glue;
    juce::dsp::ProcessorDuplicator<juce::dsp::Limiter<float>>  limiter;
    juce::dsp::Gain<float> makeup;

    float cachedWarmth = 999.0f, cachedAir = 999.0f, cachedHarshFreq = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoMasterAudioProcessor)
};
