#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class AutoMasterAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit AutoMasterAudioProcessorEditor (AutoMasterAudioProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    AutoMasterAudioProcessor& processor;

    juce::ToggleButton masterButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> masterAttach;

    std::vector<std::unique_ptr<juce::Slider>> sliders;
    std::vector<std::unique_ptr<juce::Label>>   labels;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;

    juce::Label momLabel, stLabel, intLabel, lraLabel, peakLabel, gainLabel;
    juce::TextButton resetButton { "Reset Meter" };
    juce::Rectangle<int> meterArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoMasterAudioProcessorEditor)
};
