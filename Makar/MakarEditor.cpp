#include "MakarEditor.h"
#include "MakarLayout.h"

using namespace juce;

MakarEditor::MakarEditor(MakarAudioProcessor& p)
    : AudioProcessorEditor(p), proc(p)
{
    setLookAndFeel(&laf);
    addAndMakeVisible(background);   // en premier : derrière tout
    addAndMakeVisible(curve);
    addAndMakeVisible(meter);

    struct K { Slider* s; const char* id; const char* kind; bool centre; double def; const char* title; Rectangle<int> area; };
    const K sliders[] = {
        { &drive,   MakarIDs::drive,   "drive", false, 0.3,     "Drive",    MkLayout::drive },
        { &lowCut,  MakarIDs::lowCut,  "small", false, 20.0,    "Low Cut",  MkLayout::knobCol[0] },
        { &tone,    MakarIDs::tone,    "small", true,  0.0,     "Tone",     MkLayout::knobCol[1] },
        { &highCut, MakarIDs::highCut, "small", false, 20000.0, "High Cut", MkLayout::knobCol[2] },
        { &mix,     MakarIDs::mix,     "bar",   false, 1.0,     "Mix",      { 104, 416, 260, 36 } },
        { &output,  MakarIDs::output,  "bar",   false, 0.0,     "Output",   { 502, 416, 270, 36 } },
    };
    for (const auto& k : sliders) {
        Slider& s = *k.s;
        const bool bar = String(k.kind) == "bar";
        s.setSliderStyle(bar ? Slider::LinearHorizontal : Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
        if (!bar) {
            s.setRotaryParameters(MakarLookAndFeel::startAngle, MakarLookAndFeel::endAngle, true);
            s.setMouseDragSensitivity(String(k.kind) == "drive" ? 400 : 300);
        }
        s.getProperties().set(MkStyle::kind, k.kind);
        s.getProperties().set(MkStyle::centre, k.centre);
        s.setTitle(k.title);
        s.setBufferedToImage(true);   // redessiné seulement quand sa valeur change
        const auto area = k.area;     // libellés de valeur peints par l'éditeur
        s.onValueChange = [this, area] { repaintDesign(area); syncStates(); };
        addAndMakeVisible(s);
        sliderAtts.push_back(std::make_unique<SA>(proc.apvts, k.id, s));
        s.setDoubleClickReturnValue(true, k.def);   // après l'attache : valeur par défaut au double-clic
    }

    // circuits : 5 boutons exclusifs reliés au paramètre à choix
    const char* names[] = { "BANDE", "TRANSFO", "GERMANIUM", "TRIODE", "PENTODE" };
    for (int i = 0; i < 5; ++i) {
        auto b = std::make_unique<TextButton>(names[i]);
        b->getProperties().set(MkStyle::kind, "circuit");
        b->getProperties().set(MkStyle::code, "0" + String(i + 1));
        b->setTitle(String("Circuit ") + names[i]);
        b->setRadioGroupId(1, dontSendNotification);
        b->setClickingTogglesState(false);
        b->onClick = [this, i] { if (circuitAtt) circuitAtt->setValueAsCompleteGesture((float) i); };
        addAndMakeVisible(*b);
        circuits.push_back(std::move(b));
    }
    // le paramètre pilote l'état des boutons (y compris automation et rappel de preset)
    circuitAtt = std::make_unique<ParameterAttachment>(*proc.apvts.getParameter(MakarIDs::style), [this](float v) {
        circuitIndex = jlimit(0, 4, roundToInt(v));
        for (int i = 0; i < 5; ++i)
            circuits[(size_t) i]->setToggleState(i == circuitIndex, dontSendNotification);
        syncStates();
    });
    circuitAtt->sendInitialUpdate();

    struct B { TextButton* b; const char* id; const char* kind; const char* text; };
    const B buttons[] = {
        { &punish,   MakarIDs::punish,   "punish", "PUNISH" },
        { &thump,    MakarIDs::thump,    "toggle", "THUMP" },
        { &steep,    MakarIDs::steep,    "toggle", "STEEP" },
        { &autoGain, MakarIDs::autoGain, "auto",   "AUTO" },
    };
    for (const auto& k : buttons) {
        k.b->setButtonText(k.text);
        k.b->setTitle(k.text);
        k.b->getProperties().set(MkStyle::kind, k.kind);
        k.b->setClickingTogglesState(true);
        k.b->onStateChange = [this] { syncStates(); };
        addAndMakeVisible(*k.b);
        buttonAtts.push_back(std::make_unique<BA>(proc.apvts, k.id, *k.b));
    }
    syncStates();

    // fenêtre redimensionnable de 75 % à 200 %, proportions fixes ; taille gardée dans le projet
    using namespace MkLayout;
    // largeur lue avant setResizeLimits, qui redimensionne déjà la fenêtre (et l'enregistrerait)
    const int savedW = (int) proc.apvts.state.getProperty("uiWidth", W);
    setResizable(true, true);
    setResizeLimits(W * 3 / 4, H * 3 / 4, W * 2, H * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio((double) W / (double) H);
    const int w = jlimit(W * 3 / 4, W * 2, savedW);
    setSize(w, roundToInt((float) w * (float) H / (float) W));
    startTimerHz(25);
}

MakarEditor::~MakarEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void MakarEditor::timerCallback()
{
    meter.setDb(proc.attitudeDb.load());
    syncStates();
}

void MakarEditor::syncStates()
{
    const bool hot = punish.getToggleState();
    if ((bool) drive.getProperties()[MkStyle::hot] != hot) {
        drive.getProperties().set(MkStyle::hot, hot);
        drive.repaint();
    }
    background.setHalos(circuitIndex, hot);
    curve.setSettings({ (float) lowCut.getValue(), (float) tone.getValue(), (float) highCut.getValue(),
                        thump.getToggleState(), steep.getToggleState() });
}

void MakarEditor::repaintDesign(Rectangle<int> a)
{
    repaint(a.toFloat().transformedBy(AffineTransform::scale(uiScale)).getSmallestIntegerContainer().expanded(1));
}

void MakarEditor::resized()
{
    using namespace MkLayout;
    background.setBounds(0, 0, W, H);
    curve.setBounds(MkLayout::curve);
    meter.setBounds(MkLayout::meter.expanded(AttitudeMeter::margin));
    drive.setBounds(MkLayout::drive);
    lowCut.setBounds(knob(0));
    tone.setBounds(knob(1));
    highCut.setBounds(knob(2));
    mix.setBounds(mixBar);
    output.setBounds(outBar);
    for (int i = 0; i < 5; ++i) circuits[(size_t) i]->setBounds(circuit(i));
    punish.setBounds(MkLayout::punish);
    thump.setBounds(MkLayout::thump);
    steep.setBounds(MkLayout::steep);
    autoGain.setBounds(autoBtn);

    // tout est placé dans le repère de la maquette puis mis à l'échelle (dessin vectoriel net)
    uiScale = (float) getWidth() / (float) W;
    const auto t = AffineTransform::scale(uiScale);
    for (auto* c : designChildren())
        c->setTransform(t);
    proc.apvts.state.setProperty("uiWidth", getWidth(), nullptr);
}

std::vector<Component*> MakarEditor::designChildren()
{
    std::vector<Component*> v { &background, &curve, &meter, &drive, &lowCut, &tone, &highCut, &mix, &output,
                                &punish, &thump, &steep, &autoGain };
    for (auto& b : circuits) v.push_back(b.get());
    return v;
}

void MakarEditor::paint(Graphics& g)
{
    g.fillAll(MkColours::bg);
}

void MakarEditor::paintOverChildren(Graphics& g)
{
    using namespace MkLayout;
    g.addTransform(AffineTransform::scale(uiScale));   // textes dans le repère de la maquette
    auto spaced = [](Font f, float px) { f.setExtraKerningFactor(px / f.getHeight()); return f; };

    auto hz = [](double f) { return f >= 1000.0 ? String(f / 1000.0, 1) + " kHz" : String(roundToInt(f)) + " Hz"; };
    const int tn = roundToInt(tone.getValue() * 100.0);
    const String values[3] = {
        lowCut.getValue() <= 20.5 ? String("OFF") : hz(lowCut.getValue()),
        tn == 0 ? String("NEUTRE") : (tn > 0 ? "+" + String(tn) : String(tn)),
        highCut.getValue() >= 19999.0 ? String("OFF") : hz(highCut.getValue()),
    };
    const char* names[3] = { "LOW CUT", "TONE", "HIGH CUT" };
    for (int i = 0; i < 3; ++i) {
        const auto col = knobCol[i];
        g.setFont(spaced(laf.mono(10.0f), 1.5f));
        g.setColour(MkColours::label);
        g.drawText(names[i], col.getX() - 4, col.getY() + 58, col.getWidth() + 8, 13, Justification::centred, false);
        g.setFont(laf.mono(11.0f));
        g.setColour(MkColours::text);
        g.drawText(values[i], col.getX() - 4, col.getY() + 73, col.getWidth() + 8, 14, Justification::centred, false);
    }

    // valeurs de la barre du bas
    const double out = output.getValue();
    g.setFont(laf.mono(13.0f));
    g.setColour(MkColours::text);
    g.drawText(String(roundToInt(mix.getValue() * 100.0)) + " %", 296, 422, 70, 24, Justification::centredLeft, false);
    g.drawText((out > 0.05 ? "+" : "") + String(out, 1) + " dB", 694, 422, 70, 24, Justification::centredLeft, false);
}
