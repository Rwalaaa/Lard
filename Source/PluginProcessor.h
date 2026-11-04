#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "LardEngine.h"

namespace ParamIDs {
    inline constexpr const char* fatness = "fatness";
    inline constexpr const char* color   = "color";
    inline constexpr const char* gain    = "gain";
    inline constexpr const char* mix     = "mix";
}

class LardAudioProcessor final : public juce::AudioProcessor {
public:
    LardAudioProcessor();
    ~LardAudioProcessor() override = default;

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

    // pour le futur vu-mètre de l'interface (dB de réduction, >= 0)
    std::atomic<float> gainReductionDb { 0.0f };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    lard::Engine engine;
    std::atomic<float>* pFat   = nullptr;
    std::atomic<float>* pColor = nullptr;
    std::atomic<float>* pGain  = nullptr;
    std::atomic<float>* pMix   = nullptr;

    // ligne à retard du bypass : garde le signal aligné avec la latence déclarée
    juce::AudioBuffer<float> bypassDelay;
    int bypassPos = 0;

    lard::Params readParams() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LardAudioProcessor)
};
