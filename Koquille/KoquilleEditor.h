#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "KoquilleProcessor.h"
#include "KoquilleLookAndFeel.h"
#include "NacreBackground.h"

class KoquilleEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit KoquilleEditor(KoquilleAudioProcessor&);
    ~KoquilleEditor() override;

    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;

private:
    // groupe de boutons segmentés relié à un paramètre à choix
    struct Choice {
        std::vector<std::unique_ptr<juce::TextButton>> buttons;
        std::unique_ptr<juce::ParameterAttachment> attachment;
    };
    void timerCallback() override;
    void setupChoice(Choice&, const char* paramId, const juce::StringArray& labels, int radioGroup);
    float needle = 0.0f;   // aiguille lissée (0..1)
    void updateBackground();
    juce::String pct(const juce::Slider&) const;
    juce::String dbText(const juce::Slider&) const;

    KoquilleAudioProcessor& proc;
    KoquilleLookAndFeel laf;
    NacreBackground background;

    juce::Slider input, output, pentode, triode, density, sat, air, mix;
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SA>> sliderAtts;

    Choice types, bands, cals;
    juce::TextButton autoGain;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoGainAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KoquilleEditor)
};
