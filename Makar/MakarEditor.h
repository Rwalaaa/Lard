#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "MakarProcessor.h"
#include "MakarLookAndFeel.h"
#include "HudBackground.h"
#include "HudWidgets.h"

class MakarEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit MakarEditor(MakarAudioProcessor&);
    ~MakarEditor() override;

    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void syncStates();                                  // halos, couronne Punish, courbe des filtres
    void repaintDesign(juce::Rectangle<int> designArea);
    std::vector<juce::Component*> designChildren();     // composants placés dans le repère de la maquette

    MakarAudioProcessor& proc;
    MakarLookAndFeel laf;
    HudBackground background { laf };
    FilterCurve curve;
    AttitudeMeter meter { laf };
    float uiScale = 1.0f;   // taille de la fenêtre / taille de référence (800 x 470)

    juce::Slider drive, lowCut, tone, highCut, mix, output;
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SA>> sliderAtts;

    std::vector<std::unique_ptr<juce::TextButton>> circuits;
    std::unique_ptr<juce::ParameterAttachment> circuitAtt;
    int circuitIndex = 0;

    juce::TextButton punish, thump, steep, autoGain;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::vector<std::unique_ptr<BA>> buttonAtts;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MakarEditor)
};
