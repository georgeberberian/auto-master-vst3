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
    row-placeholder-marker
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
