#include "KoquilleEditor.h"
#include "KoqLayout.h"

using namespace juce;

static String utf8(const char* s) { return String::fromUTF8(s); }

KoquilleEditor::KoquilleEditor(KoquilleAudioProcessor& p)
    : AudioProcessorEditor(p), proc(p)
{
    setLookAndFeel(&laf);
    addAndMakeVisible(background);   // en premier : derrière tout

    struct K { Slider* s; const char* id; const char* size; bool centre, plain, onBg; double def; const char* title; };
    const K knobs[] = {
        { &input,   KoqIDs::input,     "mid",   true,  false, false, 0.0, "Input" },
        { &output,  KoqIDs::output,    "mid",   true,  true,  false, 0.0, "Output" },
        { &pentode, KoqIDs::pentode,   "small", false, false, false, 0.2, "Pentode" },
        { &triode,  KoqIDs::triode,    "small", false, false, false, 0.3, "Triode" },
        { &density, KoqIDs::density,   "big",   false, false, false, 0.2, "Density" },
        { &sat,     KoqIDs::satAmount, "small", false, false, true,  0.0, "Saturation" },
        { &air,     KoqIDs::air,       "small", false, false, true,  0.0, "Air" },
        { &mix,     KoqIDs::mix,       "small", false, true,  true,  1.0, "Mix" },
    };
    for (const auto& k : knobs) {
        Slider& s = *k.s;
        s.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters(KoquilleLookAndFeel::startAngle, KoquilleLookAndFeel::endAngle, true);
        s.setMouseDragSensitivity(300);
        s.getProperties().set(KoqStyle::size, k.size);
        s.getProperties().set(KoqStyle::centre, k.centre);
        s.getProperties().set(KoqStyle::plain, k.plain);
        s.getProperties().set(KoqStyle::onBg, k.onBg);
        s.setTitle(k.title);
        s.setBufferedToImage(true);   // redessiné seulement quand sa valeur change
        s.onValueChange = [this] { updateBackground(); repaint(); };
        addAndMakeVisible(s);
        sliderAtts.push_back(std::make_unique<SA>(proc.apvts, k.id, s));
        s.setDoubleClickReturnValue(true, k.def);   // après l'attache : valeur par défaut au double-clic
    }

    setupChoice(types, KoqIDs::satType, { "Douce", "Agressive" }, 1);
    setupChoice(bands, KoqIDs::satBand, { "Grave", utf8("Bas-m\xc3\xa9" "d."), utf8("M\xc3\xa9" "dium"), "Aigu" }, 2);
    setupChoice(cals,  KoqIDs::calibration, { "Sombre", "Normal", "Brillant" }, 3);

    autoGain.getProperties().set(KoqStyle::pill, true);
    autoGain.setClickingTogglesState(true);
    autoGain.setTitle("Gain auto");
    addAndMakeVisible(autoGain);
    autoGainAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, KoqIDs::autoGain, autoGain);

    updateBackground();

    // fenêtre redimensionnable de 75 % à 200 %, proportions fixes ; taille gardée dans le projet
    // largeur lue avant setResizeLimits, qui redimensionne déjà la fenêtre (et l'enregistrerait)
    const int savedW = (int) proc.apvts.state.getProperty("uiWidth", KoqLayout::W);
    setResizable(true, true);
    setResizeLimits(KoqLayout::W * 3 / 4, KoqLayout::H * 3 / 4, KoqLayout::W * 2, KoqLayout::H * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio((double) KoqLayout::W / (double) KoqLayout::H);
    const int w = jlimit(KoqLayout::W * 3 / 4, KoqLayout::W * 2, savedW);
    setSize(w, roundToInt((float) w * (float) KoqLayout::H / (float) KoqLayout::W));
    startTimerHz(25);
}

KoquilleEditor::~KoquilleEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void KoquilleEditor::timerCallback()
{
    // aiguille : -40 dB (au repos) .. -6 dB (dans le rouge) ; montée rapide, retombée douce
    const float target = jlimit(0.0f, 1.0f, (proc.colourDb.load() + 40.0f) / 34.0f);
    const float k = target > needle ? 0.45f : 0.12f;
    needle += (target - needle) * k;
    background.setDynamics(needle, Time::getMillisecondCounterHiRes() * 0.001);
}

void KoquilleEditor::setupChoice(Choice& c, const char* paramId, const StringArray& labels, int radioGroup)
{
    auto* param = proc.apvts.getParameter(paramId);
    jassert(param != nullptr);
    for (int i = 0; i < labels.size(); ++i) {
        auto b = std::make_unique<TextButton>(labels[i]);
        b->getProperties().set(KoqStyle::seg, true);
        b->setRadioGroupId(radioGroup, dontSendNotification);
        b->setClickingTogglesState(false);
        b->onClick = [&c, i] { if (c.attachment) c.attachment->setValueAsCompleteGesture((float) i); };
        addAndMakeVisible(*b);
        c.buttons.push_back(std::move(b));
    }
    // le paramètre pilote l'état des boutons (y compris automation et rappel de preset)
    c.attachment = std::make_unique<ParameterAttachment>(*param, [&c](float v) {
        const int idx = roundToInt(v);
        for (int i = 0; i < (int) c.buttons.size(); ++i)
            c.buttons[(size_t) i]->setToggleState(i == idx, dontSendNotification);
    });
    c.attachment->sendInitialUpdate();
}

void KoquilleEditor::updateBackground()
{
    // lueur de base des lampes d'après leurs réglages (l'animation y ajoute la couleur mesurée)
    const float pe = (float) pentode.getValue(), tr = (float) triode.getValue(), de = (float) density.getValue();
    NacreBackground::State st;
    st.density = de;
    st.pentDrive = jlimit(0.0f, 1.0f, 0.15f + 0.6f * pe + 0.35f * de);
    st.triDrive  = jlimit(0.0f, 1.0f, 0.15f + 0.6f * tr + 0.35f * de);
    background.setState(st);
}

void KoquilleEditor::resized()
{
    using namespace KoqLayout;
    background.setBounds(0, 0, W, H);
    input.setBounds(KoqLayout::input);
    output.setBounds(KoqLayout::output);
    pentode.setBounds(pentKnob);
    triode.setBounds(triKnob);
    density.setBounds(KoqLayout::density);
    sat.setBounds(satKnob);
    air.setBounds(airKnob);
    mix.setBounds(mixKnob);
    for (int i = 0; i < 2; ++i) types.buttons[(size_t) i]->setBounds(KoqLayout::types[i]);
    for (int i = 0; i < 4; ++i) bands.buttons[(size_t) i]->setBounds(KoqLayout::bands[i]);
    for (int i = 0; i < 3; ++i) cals.buttons[(size_t) i]->setBounds(KoqLayout::cals[i]);
    autoGain.setBounds(KoqLayout::autoGain);

    // tout est placé dans le repère de la maquette puis mis à l'échelle (dessin vectoriel net)
    uiScale = (float) getWidth() / (float) W;
    const auto t = AffineTransform::scale(uiScale);
    for (auto* c : designChildren())
        c->setTransform(t);
    proc.apvts.state.setProperty("uiWidth", getWidth(), nullptr);
}

std::vector<Component*> KoquilleEditor::designChildren()
{
    std::vector<Component*> v { &background, &input, &output, &pentode, &triode, &density, &sat, &air, &mix, &autoGain };
    for (auto* c : { &types, &bands, &cals })
        for (auto& b : c->buttons)
            v.push_back(b.get());
    return v;
}

String KoquilleEditor::pct(const Slider& s) const { return String(roundToInt(s.getValue() * 100.0)) + " %"; }
String KoquilleEditor::dbText(const Slider& s) const
{
    const double v = s.getValue();
    return (v > 0.05 ? "+" : "") + String(v, 1) + " dB";
}

void KoquilleEditor::paint(Graphics& g)
{
    g.fillAll(KoqColours::bg);
}

void KoquilleEditor::paintOverChildren(Graphics& g)
{
    using namespace KoqLayout;
    g.addTransform(AffineTransform::scale(uiScale));   // textes dans le repère de la maquette
    auto text = [&](const String& s, Rectangle<int> r, const Font& f, Colour c, Justification j = Justification::centred) {
        g.setFont(f);
        g.setColour(c);
        g.drawText(s, r, j, false);
    };
    auto spaced = [&](Font f, float k) { f.setExtraKerningFactor(k); return f; };
    const Font label = spaced(laf.body(12.0f), 0.17f);
    const Font small = spaced(laf.body(11.0f), 0.18f);

    // en-tête
    text("KOQUILLE", { 32, 18, 300, 44 }, spaced(laf.display(42.0f), 0.17f), KoqColours::text, Justification::centredLeft);
    text(utf8("SATURATEUR \xc3\x80 LAMPES"), { 32, 62, 300, 14 }, spaced(laf.body(11.0f), 0.27f), KoqColours::label,
         Justification::centredLeft);
    text("COULEUR", { vu.getX() + 9, vu.getY() + 6, 70, 12 }, spaced(laf.body(9.0f), 0.22f), KoqColours::label,
         Justification::centredLeft);

    // rangée principale
    auto under = [&](Rectangle<int> knob, int dy, int h) { return Rectangle<int>(knob.getX() - 30, knob.getBottom() + dy, knob.getWidth() + 60, h); };
    text("INPUT",  under(KoqLayout::input, 4, 14),  label, KoqColours::label);
    text(dbText(input),  under(KoqLayout::input, 20, 16),  laf.mono(13.0f), KoqColours::text);
    text("OUTPUT", under(KoqLayout::output, 4, 14), label, KoqColours::label);
    text(dbText(output), under(KoqLayout::output, 20, 16), laf.mono(13.0f), KoqColours::text);
    text(utf8("PENTODE \xc2\xb7 ") + pct(pentode), under(pentKnob, 4, 14), label, KoqColours::label);
    text(utf8("TRIODE \xc2\xb7 ") + pct(triode),   under(triKnob, 4, 14),  label, KoqColours::label);
    text("DENSITY", under(KoqLayout::density, 4, 24), spaced(laf.display(20.0f), 0.2f), KoqColours::text);
    text(pct(density), under(KoqLayout::density, 28, 16), laf.mono(13.0f), KoqColours::acc);

    // panneau du bas
    text(utf8("SATURATION PARALL\xc3\x88LE"), { 104, 354, 240, 14 }, small, KoqColours::label, Justification::centredLeft);
    text(pct(sat), under(satKnob, 2, 14), laf.mono(11.0f), KoqColours::text);
    text("CALIBRATION", { 370, 372, 190, 14 }, small, KoqColours::label, Justification::centredLeft);
    text(utf8("AIR \xc2\xb7 ") + pct(air), under(airKnob, 2, 14), small, KoqColours::label);
    text(utf8("MIX \xc2\xb7 ") + pct(mix), under(mixKnob, 2, 14), small, KoqColours::label);
}
