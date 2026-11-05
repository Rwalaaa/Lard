#include "MakarProcessor.h"

using namespace juce;

static const char* const kIds[MakarIDs::count] = {
    MakarIDs::drive, MakarIDs::punish, MakarIDs::style, MakarIDs::lowCut, MakarIDs::thump, MakarIDs::tone,
    MakarIDs::highCut, MakarIDs::steep, MakarIDs::mix, MakarIDs::output, MakarIDs::autoGain
};

MakarAudioProcessor::MakarAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput ("Input",  AudioChannelSet::stereo(), true)
                         .withOutput("Output", AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "MakarState", createLayout())
{
    for (int i = 0; i < MakarIDs::count; ++i)
        p[i] = apvts.getRawParameterValue(kIds[i]);
}

AudioProcessorValueTreeState::ParameterLayout MakarAudioProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto percent = AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float v, int) { return String(roundToInt(v * 100.0f)) + " %"; })
        .withValueFromStringFunction([](const String& s) { return s.getFloatValue() / 100.0f; });
    auto hertz = [](const char* offText, bool offBelow) {
        return AudioParameterFloatAttributes()
            .withStringFromValueFunction([offText, offBelow](float v, int) {
                if (offBelow ? v <= 20.5f : v >= 19999.0f) return String(offText);
                return v >= 1000.0f ? String(v / 1000.0f, 1) + " kHz" : String(roundToInt(v)) + " Hz";
            })
            .withValueFromStringFunction([](const String& s) {
                const float v = s.getFloatValue();
                return s.containsIgnoreCase("k") ? v * 1000.0f : v;
            });
    };
    // plages en fréquence : réparties de façon logarithmique sur la course
    auto logRange = [](float lo, float hi) {
        return NormalisableRange<float>(lo, hi,
            [](float a, float b, float t) { return a * std::pow(b / a, t); },
            [](float a, float b, float v) { return std::log(v / a) / std::log(b / a); },
            [](float, float, float v) { return v; });
    };

    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { MakarIDs::drive, 1 }, "Drive",
               NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.3f, percent));
    layout.add(std::make_unique<AudioParameterBool>(ParameterID { MakarIDs::punish, 1 }, "Punish", false));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID { MakarIDs::style, 1 }, "Style",
               StringArray { "Bande", "Transfo", "Germanium", "Triode", "Pentode" }, 0));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { MakarIDs::lowCut, 1 }, "Low Cut",
               logRange(20.0f, 1000.0f), 20.0f, hertz("Off", true)));
    layout.add(std::make_unique<AudioParameterBool>(ParameterID { MakarIDs::thump, 1 }, "Thump", false));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { MakarIDs::tone, 1 }, "Tone",
               NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.0f,
               AudioParameterFloatAttributes()
                   .withStringFromValueFunction([](float v, int) {
                       const int t = roundToInt(v * 100.0f);
                       return t == 0 ? String("Neutre") : (t < 0 ? "Sombre " + String(-t) : "Brillant " + String(t));
                   })));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { MakarIDs::highCut, 1 }, "High Cut",
               logRange(1000.0f, 20000.0f), 20000.0f, hertz("Off", false)));
    layout.add(std::make_unique<AudioParameterBool>(ParameterID { MakarIDs::steep, 1 }, "Steep", false));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { MakarIDs::mix, 1 }, "Mix",
               NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f, percent));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { MakarIDs::output, 1 }, "Output",
               NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f,
               AudioParameterFloatAttributes().withLabel("dB")));
    layout.add(std::make_unique<AudioParameterBool>(ParameterID { MakarIDs::autoGain, 1 }, "Auto", true));
    return layout;
}

makar::Params MakarAudioProcessor::readParams() const
{
    makar::Params q;
    q.drive     = p[0]->load();
    q.punish    = p[1]->load() > 0.5f;
    q.style     = roundToInt(p[2]->load());
    q.lowCutHz  = p[3]->load();
    q.thump     = p[4]->load() > 0.5f;
    q.tone      = p[5]->load();
    q.highCutHz = p[6]->load() >= 19999.0f ? 22000.0f : p[6]->load();   // butée haute = coupé
    q.steep     = p[7]->load() > 0.5f;
    q.mix       = p[8]->load();
    q.outputDb  = p[9]->load();
    q.autoGain  = p[10]->load() > 0.5f;
    return q;
}

bool MakarAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void MakarAudioProcessor::prepareToPlay(double sampleRate, int)
{
    engine.setParams(readParams());
    engine.prepare(sampleRate);
    setLatencySamples(engine.latencySamples());
    bypassDelay.setSize(jmax(1, getTotalNumOutputChannels()), engine.latencySamples());
    bypassDelay.clear();
    bypassPos = 0;
}

void MakarAudioProcessor::processBlock(AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    for (int c = numIn; c < numOut; ++c)
        buffer.clear(c, 0, buffer.getNumSamples());

    engine.setParams(readParams());
    engine.process(buffer.getArrayOfWritePointers(), jmin(numOut, buffer.getNumChannels()), buffer.getNumSamples());
    attitudeDb.store(engine.attitudeDb());
}

void MakarAudioProcessor::processBlockBypassed(AudioBuffer<float>& buffer, MidiBuffer&)
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

AudioProcessorEditor* MakarAudioProcessor::createEditor()
{
    // interface provisoire : la vraie arrive après la maquette (M3/M4)
    return new GenericAudioProcessorEditor(*this);
}

void MakarAudioProcessor::getStateInformation(MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void MakarAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(ValueTree::fromXml(*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MakarAudioProcessor();
}
