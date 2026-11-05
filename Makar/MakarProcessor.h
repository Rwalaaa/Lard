#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "MakarEngine.h"

namespace MakarIDs {
    inline constexpr const char* drive    = "drive";
    inline constexpr const char* punish   = "punish";
    inline constexpr const char* style    = "style";
    inline constexpr const char* lowCut   = "lowCut";
    inline constexpr const char* thump    = "thump";
    inline constexpr const char* tone     = "tone";
    inline constexpr const char* highCut  = "highCut";
    inline constexpr const char* steep    = "steep";
    inline constexpr const char* mix      = "mix";
    inline constexpr const char* output   = "output";
    inline constexpr const char* autoGain = "autoGain";
    inline constexpr int count = 11;
}

class MakarAudioProcessor final : public juce::AudioProcessor {
public:
    MakarAudioProcessor();
    ~MakarAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // attaque du circuit (dB), lue par l'interface pour le vu-mètre
    std::atomic<float> attitudeDb { -60.0f };

private:
    makar::Engine engine;
    std::atomic<float>* p[MakarIDs::count] = {};

    juce::AudioBuffer<float> bypassDelay;
    int bypassPos = 0;

    makar::Params readParams() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MakarAudioProcessor)
};
