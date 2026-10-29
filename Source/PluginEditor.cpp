#include "PluginEditor.h"

AutoMasterAudioProcessorEditor::AutoMasterAudioProcessorEditor (AutoMasterAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    masterButton.setButtonText ("AUTO-MASTER ON");
    addAndMakeVisible (masterButton);
    masterAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, "master", masterButton);

    struct Def { const char* id; const char* name; const char* suffix; };
    const Def defs[] = {
        { "target",    "Target LUFS", " LUFS" },
        { "amount",    "Amount",      ""      },
        { "glue",      "Glue Comp",   ""      },
        { "harsh",     "De-Harsh",    ""      },
        { "harshFreq", "Harsh Freq",  " Hz"   },
        { "warmth",    "Warmth",      " dB"   },
        { "air",       "Air",         " dB"   },
        { "width",     "Width",       ""      },
        { "ceiling",   "Ceiling",     " dB"   },
    };

    for (const auto& d : defs)
    {
        auto s = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
        s->setTextValueSuffix (d.suffix);
        addAndMakeVisible (*s);
        attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processor.apvts, d.id, *s));

        auto l = std::make_unique<juce::Label> ({}, d.name);
        l->setColour (juce::Label::textColourId, juce::Colours::grey.brighter());
        addAndMakeVisible (*l);

        sliders.push_back (std::move (s));
        labels.push_back (std::move (l));
    }

    for (auto* lbl : { &momLabel, &stLabel, &intLabel, &lraLabel, &peakLabel, &gainLabel })
    {
        lbl->setJustificationType (juce::Justification::centred);
        lbl->setFont (juce::FontOptions (14.0f));
        addAndMakeVisible (*lbl);
    }

    agcButton.setButtonText ("Auto Gain");
    addAndMakeVisible (agcButton);
    agcAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, "agc", agcButton);

    resetButton.onClick = [this] { processor.resetMeter(); };
    addAndMakeVisible (resetButton);

    setSize (800, 560);
    startTimerHz (24);
}

void AutoMasterAudioProcessorEditor::timerCallback()
{
    auto fmt = [] (float v, const char* unit)
    {
        if (v <= -99.0f)
            return juce::String ("--");
        return juce::String (v, 1) + " " + unit;
    };

    momLabel.setText ("Momentary\n"  + fmt (processor.meter.momentaryLUFS(), "LUFS"),  juce::dontSendNotification);
    stLabel.setText  ("Short-Term\n" + fmt (processor.meter.shortTermLUFS(),  "LUFS"),  juce::dontSendNotification);
    intLabel.setText ("Integrated\n" + fmt (processor.meter.integratedLUFS(),  "LUFS"),  juce::dontSendNotification);
    lraLabel.setText ("LRA\n"        + fmt (processor.meter.lra(),              "LU"),    juce::dontSendNotification);
    peakLabel.setText("Peak\n"       + fmt (processor.meter.peakDb(),           "dB"),    juce::dontSendNotification);
    gainLabel.setText ("AGC "  + juce::String (processor.agcGainDb.load(), 1) + " dB\n"
                       "Harsh " + juce::String (processor.harshReductionDb.load(), 1) + " dB\n"
                       "Glue "  + juce::String (processor.glueReductionDb.load(), 1) + " dB",
                       juce::dontSendNotification);
    repaint();
}

void AutoMasterAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    auto header = bounds.removeFromTop (46);
    masterButton.setBounds (header.removeFromLeft (170).reduced (8));

    meterArea = bounds.removeFromTop (220);
    meterArea = meterArea.withSizeKeepingCentre (720, 200);
    const int statsTop = meterArea.getBottom() + 4;
    const int statsW = meterArea.getWidth() / 6;
    for (auto* lbl : { &momLabel, &stLabel, &intLabel, &lraLabel, &peakLabel, &gainLabel })
        lbl->setBounds (meterArea.getX() + (lbl == &momLabel ? 0 : (lbl == &stLabel ? statsW : (lbl == &intLabel ? 2 * statsW : (lbl == &lraLabel ? 3 * statsW : (lbl == &peakLabel ? 4 * statsW : 5 * statsW))))),
                        statsTop, statsW, 54);

    auto controls = bounds;
    agcButton.setBounds (controls.removeFromRight (150).reduced (6));
    resetButton.setBounds (controls.removeFromRight (150).reduced (6));

    controls.reduce (10, 4);
    const int rowH = controls.getHeight() / (int) sliders.size();
    for (size_t i = 0; i < sliders.size(); ++i)
    {
        auto row = controls.removeFromTop (rowH);
        labels[i]->setBounds (row.removeFromLeft (100));
        sliders[i]->setBounds (row);
    }
}

void AutoMasterAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff16181d));

    auto top = getLocalBounds().removeFromTop (46);
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("AUTO-MASTER", top, juce::Justification::centred);

    if (meterArea.isEmpty())
        return;

    auto area = meterArea.toFloat();
    const float low = -60.0f, high = 0.0f;
    auto mapY = [&] (float lufs)
    {
        return area.getBottom() - (lufs - low) / (high - low) * area.getHeight();
    };

    // Scale grid
    g.setColour (juce::Colours::white.withAlpha (0.15f));
    g.setFont (juce::FontOptions (11.0f));
    for (float l : { -50.0f, -40.0f, -30.0f, -20.0f, -14.0f, -10.0f, -5.0f, 0.0f })
    {
        auto y = mapY (l);
        g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
        g.drawText (juce::String (l, 0), (int) area.getRight() + 4, (int) y - 7, 34, 14,
                    juce::Justification::centredLeft);
    }

    // Target LUFS line (yellow)
    const float target = processor.apvts.getRawParameterValue ("target")->load();
    g.setColour (juce::Colours::yellow.withAlpha (0.8f));
    g.drawHorizontalLine ((int) mapY (target), area.getX(), area.getRight());

    // Bars: momentary (green), short-term (blue)
    auto drawBar = [&] (float x, float w, float lufs, juce::Colour c)
    {
        if (lufs <= -99.0f) return;
        const float top = mapY (juce::jlimit (low, high, lufs));
        g.setColour (c.withAlpha (0.85f));
        g.fillRect (juce::Rectangle<float> (x, top, w, area.getBottom() - top));
    };
    const float bw = 34.0f;
    drawBar (area.getX() + 40,  bw, processor.meter.momentaryLUFS(), juce::Colours::limegreen);
    drawBar (area.getX() + 110, bw, processor.meter.shortTermLUFS(),  juce::Colours::cornflowerblue);

    // Integrated marker (white line)
    const float integ = processor.meter.integratedLUFS();
    if (integ > -99.0f)
    {
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawHorizontalLine ((int) mapY (integ), area.getX() + 160, area.getX() + 300);
    }

    g.setFont (juce::FontOptions (11.0f));
    g.setColour (juce::Colours::grey.brighter());
    g.drawText ("MOM",  (int) area.getX() + 40,  (int) area.getBottom() + 2, (int) bw, 14, juce::Justification::centred);
    g.drawText ("ST",   (int) area.getX() + 110, (int) area.getBottom() + 2, (int) bw, 14, juce::Justification::centred);
    g.drawText ("INTEG", (int) area.getX() + 160, (int) area.getBottom() + 2, 140, 14, juce::Justification::centred);
}
