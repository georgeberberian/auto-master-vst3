#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>

//==============================================================================
// LoudnessMeter
void LoudnessMeter::prepare (double sr)
{
    sampleRate = sr;
    blockLen = juce::jmax (1, (int) std::round (0.1 * sr)); // 100 ms blocks
    reset();
}

void LoudnessMeter::reset()
{
    blockFill = 0; blockAccum = 0.0;
    peakDb = -100.0f; peakHold = 0.0f;
    blocks.clear(); shortTermHistory.clear();
    kFilters.clear();
}

void LoudnessMeter::ensureFilters (int numChannels)
{
    if ((int) kFilters.size() == numChannels)
        return;

    kFilters.resize (numChannels);
    auto shelf = juce::dsp::IIR::Coefficients<double>::makeHighShelf (sampleRate, 1681.97, 0.7071, 1.584893192);
    auto rlbf  = juce::dsp::IIR::Coefficients<double>::makeHighPass  (sampleRate, 38.135, 0.5003);

    for (auto& ch : kFilters)
    {
        ch[0].coefficients = shelf;
        ch[1].coefficients = rlbf;
    }
}

void LoudnessMeter::process (const juce::AudioBuffer<float>& buffer)
{
    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    if (numCh == 0) return;

    ensureFilters (numCh);
    const int n = buffer.getNumSamples();

    // Peak tracking (~6 dB/s decay for display)
    for (int ch = 0; ch < numCh; ++ch)
    {
        const float* d = buffer.getReadPointer (ch);
        for (int i = 0; i < n; ++i)
        {
            auto a = std::abs (d[i]);
            if (a > peakHold) peakHold = a;
        }
    }
    peakDb   = juce::Decibels::gainToDecibels (peakHold, -100.0f);
    peakHold *= (float) std::pow (0.5, (double) n / (0.5 * sampleRate));

    // K-weighted energy accumulation -> 100 ms blocks
    for (int i = 0; i < n; ++i)
    {
        double s = 0.0;
        for (int ch = 0; ch < numCh; ++ch)
        {
            double x = kFilters[ch][0].processSample ((double) buffer.getSample (ch, i));
            x = kFilters[ch][1].processSample (x);
            s += x * x;
        }
        blockAccum += s;

        if (++blockFill >= blockLen)
        {
            pushBlock (blockAccum / (double) blockLen);
            blockAccum = 0.0;
            blockFill = 0;
        }
    }
}

void LoudnessMeter::pushBlock (double power)
{
    blocks.push_back (power);
    while (blocks.size() > 6000) blocks.pop_front(); // 10 min

    if (blocks.size() >= 30)
    {
        double sum = 0.0;
        for (size_t i = blocks.size() - 30; i < blocks.size(); ++i)
            sum += blocks[i];
        shortTermHistory.push_back (sum / 30.0);
        while (shortTermHistory.size() > 1800) shortTermHistory.pop_front(); // 3 min
    }
}

float LoudnessMeter::momentaryLUFS() const
{
    if (blocks.size() < 4) return -100.0f;
    double sum = 0.0;
    for (size_t i = blocks.size() - 4; i < blocks.size(); ++i) sum += blocks[i];
    return (float) powerToLufs (sum / 4.0);
}

float LoudnessMeter::shortTermLUFS() const
{
    if (blocks.size() < 30) return -100.0f;
    double sum = 0.0;
    for (size_t i = blocks.size() - 30; i < blocks.size(); ++i) sum += blocks[i];
    return (float) powerToLufs (sum / 30.0);
}

float LoudnessMeter::integratedLUFS() const
{
    if (blocks.empty()) return -100.0f;

    double sum = 0.0; size_t cnt = 0;
    for (double p : blocks)
        if (powerToLufs (p) > -70.0) { sum += p; ++cnt; }
    if (cnt == 0) return -100.0f;

    const double rel = powerToLufs (sum / cnt) - 10.0; // relative gate
    sum = 0.0; cnt = 0;
    for (double p : blocks)
        if (powerToLufs (p) > rel) { sum += p; ++cnt; }

    return cnt ? (float) powerToLufs (sum / cnt) : -100.0f;
}

float LoudnessMeter::lra() const
{
    const double I = integratedLUFS();
    if (I < -70.0 || shortTermHistory.empty()) return 0.0f;

    std::vector<float> vals;
    for (double p : shortTermHistory)
    {
        double L = powerToLufs (p);
        if (L > -70.0 && L > I - 20.0)
            vals.push_back ((float) L);
    }
    if (vals.size() < 10) return 0.0f;

    std::sort (vals.begin(), vals.end());
    auto pct = [&] (double q)
    {
        return vals[(size_t) juce::jlimit (0, (int) vals.size() - 1, (int) std::floor (q * (double) vals.size()))];
    };
    return juce::jlimit (0.0f, 30.0f, pct (0.95) - pct (0.10));
}

//==============================================================================
// Parameters
juce::AudioProcessorValueTreeState::ParameterLayout AutoMasterAudioProcessor::createLayout()
{
    using juce::AudioParameterFloat;
    using juce::AudioParameterBool;
    using juce::ParameterID;

    juce::AudioProcessorValueTreeState::ParameterLayout l;

    l.add (std::make_unique<AudioParameterBool>   (ParameterID{ "master",    1 }, "Mastering On", true));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID{ "target",    1 }, "Target LUFS",
              juce::NormalisableRange<float> (-16.0f, -8.0f, 0.1f), -14.0f));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID{ "amount",    1 }, "Amount",        0.0f, 1.0f, 0.01f, 0.5f));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID{ "glue",      1 }, "Glue Comp",     0.0f, 1.0f, 0.01f, 0.4f));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID{ "harsh",     1 }, "De-Harsh",      0.0f, 1.0f, 0.01f, 0.3f));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID{ "harshFreq", 1 }, "Harsh Freq",
              juce::NormalisableRange<float> (2000.0f, 6000.0f, 0.0f, 0.3f), 3500.0f));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID{ "warmth",    1 }, "Warmth (dB)",  -6.0f, 6.0f, 0.1f, 0.0f));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID{ "air",       1 }, "Air (dB)",      0.0f, 6.0f, 0.1f, 0.0f));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID{ "width",     1 }, "Width",         0.0f, 2.0f, 0.01f, 1.0f));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID{ "ceiling",   1 }, "Ceiling (dB)", -3.0f, 0.0f, 0.05f, -0.3f));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID{ "agc",       1 }, "Auto Gain", true));

    return l;
}

AutoMasterAudioProcessor::AutoMasterAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

bool AutoMasterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& main = layouts.getMainOutputChannelSet();
    return layouts.getMainInputChannelSet() == main
        && (main == juce::AudioChannelSet::stereo() || main == juce::AudioChannelSet::mono());
}

void AutoMasterAudioProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    sampleRate = sr;
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) samplesPerBlock, 2 };

    glue.prepare (spec);
    limiter.prepare (spec);
    makeup.prepare (spec);
    makeup.setRampDurationSeconds (0.15);

    meter.prepare (sr);

    for (auto& s : harshGain)
    {
        s.reset ((float) sr, 0.05);
        s.setCurrentAndTargetValue (1.0f);
    }

    cachedWarmth = 999.0f;
    cachedAir    = 999.0f;
    cachedHarshFreq = -1.0f;
    agcGainDb.store (0.0f);
}

void AutoMasterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();
    if (numCh == 0 || numSamples == 0) return;

    const bool  masterOn = apvts.getRawParameterValue ("master")->load() > 0.5f;
    const float amount   = apvts.getRawParameterValue ("amount")->load();
    const float target   = apvts.getRawParameterValue ("target")->load();
    const bool  agcOn    = apvts.getRawParameterValue ("agc")->load() > 0.5f;
    const float amtScale = 0.35f + 0.65f * amount; // Amount scales processing intensity

    float* L = buffer.getWritePointer (0);
    float* R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    if (masterOn)
    {
        // ---------- Tone: warmth + air ----------
        const float warmth = apvts.getRawParameterValue ("warmth")->load() * amtScale;
        const float air    = apvts.getRawParameterValue ("air")->load()    * amtScale;

        if (std::abs (warmth - cachedWarmth) > 0.001f)
        {
            cachedWarmth = warmth;
            auto c = juce::dsp::IIR::Coefficients<float>::makeLowShelf (sampleRate, 140.0, 0.7f,
                          juce::Decibels::decibelsToGain (warmth));
            for (auto& f : warmthShelf) f.coefficients = c;
        }
        if (std::abs (air - cachedAir) > 0.001f)
        {
            cachedAir = air;
            auto c = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, 8000.0, 0.7f,
                          juce::Decibels::decibelsToGain (air));
            for (auto& f : airShelf) f.coefficients = c;
        }

        for (int i = 0; i < numSamples; ++i)
        {
            L[i] = warmthShelf[0].processSample (L[i]);
            L[i] = airShelf[0].processSample (L[i]);
            if (R != nullptr)
            {
                R[i] = warmthShelf[1].processSample (R[i]);
                R[i] = airShelf[1].processSample (R[i]);
            }
        }

        // ---------- De-harsh: dynamic EQ (parallel band cut) ----------
        const float harshAmt  = apvts.getRawParameterValue ("harsh")->load() * amtScale;
        const float harshFreq = apvts.getRawParameterValue ("harshFreq")->load();

        if (std::abs (harshFreq - cachedHarshFreq) > 1.0f)
        {
            cachedHarshFreq = harshFreq;
            auto c = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, (double) harshFreq, 1.4f);
            for (auto& f : harshBand) f.coefficients = c;
        }

        if (harshAmt > 0.001f)
        {
            double bandSq = 0.0, fullSq = 0.0;
            juce::int64 count = 0;

            for (int i = 0; i < numSamples; ++i)
            {
                for (int ch = 0; ch < numCh; ++ch)
                {
                    float* d = (ch == 0) ? &L[i] : &R[i];
                    const float dry = *d;
                    const float b = harshBand[ch].processSample (dry);
                    bandSq += (double) b * b;
                    fullSq += (double) dry * dry;
                    ++count;
                    *d = dry - b * (1.0f - harshGain[ch].getNextValue());
                }
            }

            float reduction = 0.0f;
            if (count > 0)
            {
                const float bandDb = 10.0f * std::log10 ((float) (bandSq / (double) count) + 1e-12f);
                const float fullDb = 10.0f * std::log10 ((float) (fullSq / (double) count) + 1e-12f);
                const float excess = bandDb - (fullDb - 6.0f); // harsh if band dominates
                reduction = juce::jlimit (0.0f, 12.0f, excess * (0.5f + 1.5f * harshAmt)) * harshAmt;
            }
            harshReductionDb.store (reduction);

            const float g = juce::Decibels::decibelsToGain (-reduction);
            for (auto& s : harshGain) s.setTargetValue (g);
        }
        else
        {
            harshReductionDb.store (0.0f);
            for (auto& s : harshGain) s.setTargetValue (1.0f);
        }

        // ---------- Glue compression ----------
        const float glueAmt = apvts.getRawParameterValue ("glue")->load() * amtScale;
        if (glueAmt > 0.01f)
        {
            auto& c = glue.get<0>();
            c.setThreshold (-9.0f - 15.0f * glueAmt);
            c.setRatio (1.5f + 2.5f * glueAmt);
            c.setAttack (25.0f);
            c.setRelease (180.0f);

            juce::dsp::AudioBlock<float> block (buffer);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            glue.process (ctx);
        }

        // ---------- Stereo width (mid/side) ----------
        const float width = apvts.getRawParameterValue ("width")->load();
        if (numCh == 2 && std::abs (width - 1.0f) > 0.001f)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                const float m = 0.5f * (L[i] + R[i]);
                const float s = 0.5f * (L[i] - R[i]);
                L[i] = m + width * s;
                R[i] = m - width * s;
            }
        }

        // ---------- Auto gain: steer short-term LUFS to target ----------
        if (agcOn && meter.ready())
        {
            const float st = meter.shortTermLUFS();
            if (st > -70.0f)
            {
                const float dt = (float) numSamples / (float) sampleRate;
                const float next = agcGainDb.load() + (target - st) * dt * (0.5f + 1.5f * amount);
                agcGainDb.store (juce::jlimit (-12.0f, 15.0f, next));
            }
        }
        makeup.setGainDecibels (agcOn ? agcGainDb.load() : 0.0f);
        {
            juce::dsp::AudioBlock<float> block (buffer);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            makeup.process (ctx);
        }

        // ---------- Ceiling limiter ----------
        limiter.get<0>().setThreshold (apvts.getRawParameterValue ("ceiling")->load());
        limiter.get<0>().setRelease (80.0f);
        {
            juce::dsp::AudioBlock<float> block (buffer);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            limiter.process (ctx);
        }
    }
    else
    {
        makeup.setGainDecibels (0.0f);
    }

    // Meter the final output
    meter.process (buffer);
}

juce::AudioProcessorEditor* AutoMasterAudioProcessor::createEditor()
{
    return new AutoMasterAudioProcessorEditor (*this);
}

void AutoMasterAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::MemoryOutputStream stream (dest, false);
    apvts.state.writeToStream (stream);
}

void AutoMasterAudioProcessor::setStateInformation (const void* data, int size)
{
    auto tree = juce::ValueTree::readFromData (data, (size_t) size);
    if (tree.isValid())
        apvts.replaceState (tree);
}
