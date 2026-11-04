#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "LardLookAndFeel.h"
#include "DemonFace.h"

class LardEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    static constexpr int kWidth = 720, kHeight = 420;

    explicit LardEditor(LardAudioProcessor&);
    ~LardEditor() override;

    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintHeader(juce::Graphics&);
    void paintMeter(juce::Graphics&);
    void paintKnobLabels(juce::Graphics&);
    void paintBottomBar(juce::Graphics&);

    juce::String fatText() const;
    juce::String colorText() const;
    juce::String gainText() const;
    juce::String mixText() const;
    juce::String rankText() const;

    LardAudioProcessor& proc;
    LardLookAndFeel laf;
    DemonFace face;
    juce::Rectangle<float> headerPanel;

    juce::Slider fatSlider, colorSlider, gainSlider, mixSlider;
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> fatAtt, colorAtt, gainAtt, mixAtt;

    float meterDb = 0.0f;   // réduction affichée (dB, >= 0), lissée
    float uiScale = 1.0f;   // taille de la fenêtre / taille de référence (720 x 420)

    juce::AffineTransform uiTransform() const { return juce::AffineTransform::scale(uiScale); }
    void repaintDesign(juce::Rectangle<int> designArea);   // zone en coordonnées de la maquette

    static const juce::Rectangle<int> fatBounds, colorBounds, meterBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LardEditor)
};
