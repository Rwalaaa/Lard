#include "KoquilleProcessor.h"

using namespace juce;

static String utf8(const char* s) { return String::fromUTF8(s); }

static const char* const kIds[12] = {
    KoqIDs::input, KoqIDs::pentode, KoqIDs::triode, KoqIDs::density, KoqIDs::satAmount, KoqIDs::satType,
    KoqIDs::satBand, KoqIDs::calibration, KoqIDs::air, KoqIDs::mix, KoqIDs::output, KoqIDs::autoGain
};

KoquilleAudioProcessor::KoquilleAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput ("Input",  AudioChannelSet::stereo(), true)
                         .withOutput("Output", AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "KoquilleState", createLayout())
{
    for (int i = 0; i < 12; ++i)
        p[i] = apvts.getRawParameterValue(kIds[i]);
}

AudioProcessorValueTreeState::ParameterLayout KoquilleAudioProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto percent = AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float v, int) { return String(roundToInt(v * 100.0f)) + " %"; })
        .withValueFromStringFunction([](const String& s) { return s.getFloatValue() / 100.0f; });
    auto decibels = AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float v, int) { return (v > 0.05f ? "+" : "") + String(v, 1) + " dB"; })
        .withValueFromStringFunction([](const String& s) { return s.getFloatValue(); });

    auto unit = [&](const char* id, const char* name, float def) {
        layout.add(std::make_unique<AudioParameterFloat>(ParameterID { id, 1 }, name,
                                                         NormalisableRange<float>(0.0f, 1.0f, 0.001f), def, percent));
    };
    auto dbParam = [&](const char* id, const char* name) {
        layout.add(std::make_unique<AudioParameterFloat>(ParameterID { id, 1 }, name,
                                                         NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f, decibels));
    };

    dbParam(KoqIDs::input, "Input");
    unit(KoqIDs::pentode, "Pentode", 0.2f);
    unit(KoqIDs::triode,  "Triode",  0.3f);
    unit(KoqIDs::density, "Density", 0.2f);
    unit(KoqIDs::satAmount, "Saturation", 0.0f);
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID { KoqIDs::satType, 1 }, utf8("Caract\xc3\xa8re"),
                                                      StringArray { "Douce", "Agressive" }, 0));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID { KoqIDs::satBand, 1 }, "Bande",
                                                      StringArray { "Grave", utf8("Bas-m\xc3\xa9" "dium"),
                                                                    utf8("M\xc3\xa9" "dium"), "Aigu" }, 1));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID { KoqIDs::calibration, 1 }, "Calibration",
                                                      StringArray { "Sombre", "Normal", "Brillant" }, 1));
    unit(KoqIDs::air, "Air", 0.0f);
    unit(KoqIDs::mix, "Mix", 1.0f);
    dbParam(KoqIDs::output, "Output");
    layout.add(std::make_unique<AudioParameterBool>(ParameterID { KoqIDs::autoGain, 1 }, "Gain auto", true));
    return layout;
}

koq::Params KoquilleAudioProcessor::readParams() const
{
    koq::Params q;
    q.inputDb     = p[0]->load();
    q.pentode     = p[1]->load();
    q.triode      = p[2]->load();
    q.density     = p[3]->load();
    q.satAmount   = p[4]->load();
    q.satType     = roundToInt(p[5]->load());
    q.satBand     = roundToInt(p[6]->load());
    q.calibration = roundToInt(p[7]->load());
    q.air         = p[8]->load();
    q.mix         = p[9]->load();
    q.outputDb    = p[10]->load();
    q.autoGain    = p[11]->load() > 0.5f;
    return q;
}

bool KoquilleAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void KoquilleAudioProcessor::prepareToPlay(double sampleRate, int)
{
    engine.setParams(readParams());
    engine.prepare(sampleRate);
    setLatencySamples(engine.latencySamples());
    bypassDelay.setSize(jmax(1, getTotalNumOutputChannels()), engine.latencySamples());
    bypassDelay.clear();
    bypassPos = 0;
}

void KoquilleAudioProcessor::processBlock(AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    for (int c = numIn; c < numOut; ++c)
        buffer.clear(c, 0, buffer.getNumSamples());

    engine.setParams(readParams());
    engine.process(buffer.getArrayOfWritePointers(), jmin(numOut, buffer.getNumChannels()), buffer.getNumSamples());
}

void KoquilleAudioProcessor::processBlockBypassed(AudioBuffer<float>& buffer, MidiBuffer&)
{
    // garde le signal aligné sur la latence déclarée quand le plugin est contourné
    const int len = bypassDelay.getNumSamples();
    const int nch = jmin(buffer.getNumChannels(), bypassDelay.getNumChannels());
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

AudioProcessorEditor* KoquilleAudioProcessor::createEditor()
{
    // interface provisoire : la vraie arrive aux lots K3/K4
    return new GenericAudioProcessorEditor(*this);
}

void KoquilleAudioProcessor::getStateInformation(MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void KoquilleAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(ValueTree::fromXml(*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KoquilleAudioProcessor();
}
