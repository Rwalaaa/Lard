#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "KoquilleEngine.h"

namespace KoqIDs {
    inline constexpr const char* input       = "input";
    inline constexpr const char* pentode     = "pentode";
    inline constexpr const char* triode      = "triode";
    inline constexpr const char* density     = "density";
    inline constexpr const char* satAmount   = "satAmount";
    inline constexpr const char* satType     = "satType";
    inline constexpr const char* satBand     = "satBand";
    inline constexpr const char* calibration = "calibration";
    inline constexpr const char* air         = "air";
    inline constexpr const char* mix         = "mix";
    inline constexpr const char* output      = "output";
    inline constexpr const char* autoGain    = "autoGain";
}

class KoquilleAudioProcessor final : public juce::AudioProcessor {
public:
    KoquilleAudioProcessor();
    ~KoquilleAudioProcessor() override = default;

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

    // « couleur » ajoutée par les lampes (dB), lue par l'interface pour l'aiguille
    std::atomic<float> colourDb { -120.0f };

private:
    koq::Engine engine;
    std::atomic<float>* p[12] = {};   // dans l'ordre de KoqIDs

    juce::AudioBuffer<float> bypassDelay;
    int bypassPos = 0;

    koq::Params readParams() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KoquilleAudioProcessor)
};
