#include "PluginProcessor.h"

LardAudioProcessor::LardAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "LardState", createLayout())
{
    pFat   = apvts.getRawParameterValue(ParamIDs::fatness);
    pColor = apvts.getRawParameterValue(ParamIDs::color);
    pGain  = apvts.getRawParameterValue(ParamIDs::gain);
    pMix   = apvts.getRawParameterValue(ParamIDs::mix);
}

juce::AudioProcessorValueTreeState::ParameterLayout LardAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto percent = AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float v, int) { return String(juce::roundToInt(v * 100.0f)) + " %"; })
        .withValueFromStringFunction([](const String& s) { return s.getFloatValue() / 100.0f; });

    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID { ParamIDs::fatness, 1 }, "Fatness",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f, percent));

    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID { ParamIDs::color, 1 }, "Color",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f,
        AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float v, int) {
                const int c = juce::roundToInt((v - 0.5f) * 200.0f);   // -100..+100
                return c == 0 ? String("Neutre") : (c > 0 ? "+" : "") + String(c);
            })
            .withValueFromStringFunction([](const String& s) {
                return s.trim().startsWithIgnoreCase("n") ? 0.5f : s.getFloatValue() / 200.0f + 0.5f;
            })));

    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID { ParamIDs::gain, 1 }, "Gain",
        NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel("dB")));

    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID { ParamIDs::mix, 1 }, "Mix",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f, percent));

    return layout;
}

lard::Params LardAudioProcessor::readParams() const
{
    lard::Params p;
    p.fatness = pFat->load();
    p.color   = pColor->load();
    p.gainDb  = pGain->load();
    p.mix     = pMix->load();
    return p;
}

bool LardAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void LardAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    engine.setParams(readParams());   // pas de rampe au démarrage
    engine.prepare(sampleRate);
    setLatencySamples(engine.latencySamples());

    bypassDelay.setSize(juce::jmax(1, getTotalNumOutputChannels()), engine.latencySamples());
    bypassDelay.clear();
    bypassPos = 0;
}

void LardAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    for (int c = numIn; c < numOut; ++c)
        buffer.clear(c, 0, buffer.getNumSamples());

    engine.setParams(readParams());
    engine.process(buffer.getArrayOfWritePointers(),
                   juce::jmin(numOut, buffer.getNumChannels()),
                   buffer.getNumSamples());
    gainReductionDb.store(-engine.currentGainReductionDb());
}

void LardAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // retarde le signal de la latence du plugin pour qu'activer/désactiver
    // le bypass ne décale pas le son par rapport au reste du projet
    const int len = bypassDelay.getNumSamples();
    const int nch = juce::jmin(buffer.getNumChannels(), bypassDelay.getNumChannels());
    if (len == 0) return;
    int pos = bypassPos;
    for (int c = 0; c < nch; ++c) {
        float* io = buffer.getWritePointer(c);
        float* d  = bypassDelay.getWritePointer(c);
        pos = bypassPos;
        for (int i = 0; i < buffer.getNumSamples(); ++i) {
            const float in = io[i];
            io[i] = d[pos];
            d[pos] = in;
            if (++pos == len) pos = 0;
        }
    }
    bypassPos = pos;
}

juce::AudioProcessorEditor* LardAudioProcessor::createEditor()
{
    // interface provisoire (lot 4 : interface dédiée)
    return new juce::GenericAudioProcessorEditor(*this);
}

void LardAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void LardAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LardAudioProcessor();
}
