#include "PluginEditor.h"

using namespace juce;

const Rectangle<int> LardEditor::fatBounds   { 92, 92, 220, 220 };
const Rectangle<int> LardEditor::colorBounds { 408, 92, 220, 220 };
const Rectangle<int> LardEditor::meterBounds { 540, 18, 160, 64 };

static String utf8(const char* s) { return String::fromUTF8(s); }

// rectangle aux coins biseautés (haut-gauche et bas-droite), comme la maquette
static Path bevelled(Rectangle<float> r, float cut)
{
    Path p;
    p.startNewSubPath(r.getX() + cut, r.getY());
    p.lineTo(r.getRight(), r.getY());
    p.lineTo(r.getRight(), r.getBottom() - cut);
    p.lineTo(r.getRight() - cut, r.getBottom());
    p.lineTo(r.getX(), r.getBottom());
    p.lineTo(r.getX(), r.getY() + cut);
    p.closeSubPath();
    return p;
}

LardEditor::LardEditor(LardAudioProcessor& p)
    : AudioProcessorEditor(p), proc(p)
{
    setLookAndFeel(&laf);

    auto setupRotary = [this](Slider& s, const char* style) {
        s.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters(LardLookAndFeel::startAngle, LardLookAndFeel::endAngle, true);
        s.setMouseDragSensitivity(300);
        s.setVelocityBasedMode(false);
        s.getProperties().set(LardStyle::key, style);
        s.onValueChange = [this] { repaint(); };
        addAndMakeVisible(s);
    };
    setupRotary(fatSlider, LardStyle::fat);
    setupRotary(colorSlider, LardStyle::color);

    auto setupBar = [this](Slider& s) {
        s.setSliderStyle(Slider::LinearHorizontal);
        s.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
        s.onValueChange = [this] { repaint(getLocalBounds().removeFromBottom(70)); };
        addAndMakeVisible(s);
    };
    setupBar(gainSlider);
    setupBar(mixSlider);

    fatAtt   = std::make_unique<Attachment>(proc.apvts, ParamIDs::fatness, fatSlider);
    colorAtt = std::make_unique<Attachment>(proc.apvts, ParamIDs::color,   colorSlider);
    gainAtt  = std::make_unique<Attachment>(proc.apvts, ParamIDs::gain,    gainSlider);
    mixAtt   = std::make_unique<Attachment>(proc.apvts, ParamIDs::mix,     mixSlider);

    // double-clic = valeur par défaut
    fatSlider.setDoubleClickReturnValue(true, 0.0);
    colorSlider.setDoubleClickReturnValue(true, 0.5);
    gainSlider.setDoubleClickReturnValue(true, 0.0);
    mixSlider.setDoubleClickReturnValue(true, 1.0);

    fatSlider.setTitle("Fatness");
    colorSlider.setTitle("Color");
    gainSlider.setTitle("Gain");
    mixSlider.setTitle("Mix");

    setSize(kWidth, kHeight);
    startTimerHz(30);
}

LardEditor::~LardEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void LardEditor::resized()
{
    fatSlider.setBounds(fatBounds);
    colorSlider.setBounds(colorBounds);
    // barres du bas : zone cliquable de 24 px de haut, barre dessinée de 6 px
    gainSlider.setBounds(84, 370, 120, 24);
    mixSlider.setBounds(500, 370, 120, 24);
}

void LardEditor::timerCallback()
{
    // vu-mètre : attaque immédiate, retombée douce
    const float target = jlimit(0.0f, 24.0f, proc.gainReductionDb.load());
    const float next = target > meterDb ? target : meterDb + (target - meterDb) * 0.25f;
    if (std::abs(next - meterDb) > 0.01f) {
        meterDb = next;
        repaint(meterBounds);
    }
}

// ---------------------------------------------------------------- textes
String LardEditor::fatText() const   { return String(roundToInt(fatSlider.getValue() * 100.0)) + " %"; }
String LardEditor::mixText() const   { return String(roundToInt(mixSlider.getValue() * 100.0)) + " %"; }
String LardEditor::colorText() const
{
    const int c = roundToInt((colorSlider.getValue() - 0.5) * 200.0);
    return c == 0 ? String("Neutre") : (c > 0 ? "+" : "") + String(c);
}
String LardEditor::gainText() const
{
    const double g = gainSlider.getValue();
    return (g > 0.05 ? "+" : "") + String(g, 1) + " dB";
}
String LardEditor::rankText() const
{
    static const char* ranks[] = { "E", "D", "C", "B", "A", "S" };
    return String("RANG ") + ranks[jmin(5, (int) std::floor(fatSlider.getValue() * 6.0))];
}

// ---------------------------------------------------------------- dessin
void LardEditor::paint(Graphics& g)
{
    g.fillAll(LardColours::bg);

    // aura derrière Fatness (le visage animé arrive au lot 4b)
    const float f = (float) fatSlider.getValue();
    const auto c = fatBounds.toFloat().getCentre();
    const float ar = 80.0f * (0.7f + 0.5f * f) * 1.35f;
    ColourGradient aura(LardColours::fire.withAlpha(0.1f + 0.6f * f), c.x, c.y,
                        LardColours::fire.withAlpha(0.0f), c.x + ar, c.y, true);
    g.setGradientFill(aura);
    g.fillEllipse(c.x - ar, c.y - ar, ar * 2.0f, ar * 2.0f);

    paintHeader(g);
    paintMeter(g);
    paintKnobLabels(g);
    paintBottomBar(g);
}

void LardEditor::paintHeader(Graphics& g)
{
    Font f = laf.display(40.0f);
    f.setExtraKerningFactor(0.15f);
    // la mesure n'inclut pas l'espacement ajouté : on le compte pour chaque lettre
    const float tw = GlyphArrangement::getStringWidth(f, "LARD")
                   + f.getExtraKerningFactor() * f.getHeight() * 4.0f;
    const auto panel = Rectangle<float>(24.0f, 18.0f, tw + 36.0f, 58.0f);
    const Path shape = bevelled(panel, 12.0f);

    // lueur rouge du cadre
    for (int i = 3; i >= 1; --i) {
        g.setColour(LardColours::fire.withAlpha(0.07f));
        g.strokePath(shape, PathStrokeType(2.0f + (float) i * 6.0f));
    }
    g.setColour(LardColours::panel);
    g.fillPath(shape);
    g.setColour(LardColours::ember);
    g.strokePath(shape, PathStrokeType(2.0f));

    g.setFont(f);
    const auto textArea = panel.withTrimmedLeft(18.0f).withTrimmedTop(2.0f).toNearestInt();
    g.setColour(LardColours::fire.withAlpha(0.45f));
    for (auto d : { Point<int>(-1, 0), Point<int>(1, 0), Point<int>(0, -1), Point<int>(0, 1) })
        g.drawText("LARD", textArea.translated(d.x, d.y), Justification::centredLeft, false);
    g.setColour(Colours::white);
    g.drawText("LARD", textArea, Justification::centredLeft, false);
}

void LardEditor::paintMeter(Graphics& g)
{
    const float right = (float) kWidth - 26.0f;
    Font lab = laf.body(11.0f);
    lab.setExtraKerningFactor(0.18f);
    g.setFont(lab);
    g.setColour(LardColours::label);
    g.drawText(utf8("R\xc3\x89" "DUCTION"), Rectangle<float>(right - 160.0f, 24.0f, 160.0f, 13.0f),
               Justification::centredRight, false);

    // 12 segments obliques, 1 dB chacun
    const int lit = jlimit(0, 12, roundToInt(meterDb));
    const float segW = 8.0f, gap = 4.0f, top = 41.0f, h = 18.0f;
    float x = right - (12.0f * segW + 11.0f * gap);
    for (int i = 0; i < 12; ++i) {
        Path seg;
        seg.startNewSubPath(x, top);
        seg.lineTo(x + segW, top + h * 0.2f);
        seg.lineTo(x + segW, top + h);
        seg.lineTo(x, top + h * 0.8f);
        seg.closeSubPath();
        g.setColour(i < lit ? (i > 8 ? LardColours::hot : LardColours::ember) : LardColours::off);
        g.fillPath(seg);
        x += segW + gap;
    }

    g.setFont(laf.body(12.0f));
    g.setColour(LardColours::text);
    const String grText = meterDb < 0.05f ? String("0.0 dB") : "-" + String(meterDb, 1) + " dB";
    g.drawText(grText, Rectangle<float>(right - 160.0f, 64.0f, 160.0f, 15.0f),
               Justification::centredRight, false);
}

void LardEditor::paintKnobLabels(Graphics& g)
{
    auto drawLabel = [&](Rectangle<int> knob, const String& name, const String& value, Colour valueColour) {
        Font title = laf.display(20.0f);
        title.setExtraKerningFactor(0.15f);
        const auto titleArea = Rectangle<int>(knob.getX(), knob.getY() + 196, knob.getWidth(), 26);
        const auto valueArea = Rectangle<int>(knob.getX(), knob.getY() + 222, knob.getWidth(), 18);

        // ombre portée pour rester lisible sur le visage
        g.setFont(title);
        g.setColour(Colours::black.withAlpha(0.7f));
        g.drawText(name, titleArea.translated(0, 2), Justification::centred, false);
        g.setColour(Colours::white);
        g.drawText(name, titleArea, Justification::centred, false);

        g.setFont(laf.body(14.0f));
        g.setColour(Colours::black.withAlpha(0.7f));
        g.drawText(value, valueArea.translated(0, 2), Justification::centred, false);
        g.setColour(valueColour);
        g.drawText(value, valueArea, Justification::centred, false);
    };
    drawLabel(fatBounds, "FATNESS", fatText(), LardColours::fatValue);
    drawLabel(colorBounds, "COLOR", colorText(), LardColours::amber);
}

void LardEditor::paintBottomBar(Graphics& g)
{
    const auto bar = Rectangle<float>(16.0f, (float) kHeight - 12.0f - 52.0f, (float) kWidth - 32.0f, 52.0f);
    const Path shape = bevelled(bar, 10.0f);
    g.setColour(LardColours::bar);
    g.fillPath(shape);
    g.setColour(LardColours::ember.withAlpha(0.53f));
    g.strokePath(shape, PathStrokeType(1.5f));

    Font lab = laf.body(11.0f);
    lab.setExtraKerningFactor(0.18f);
    const float cy = bar.getCentreY();

    g.setFont(lab);
    g.setColour(LardColours::label);
    g.drawText("GAIN", Rectangle<float>(36.0f, cy - 8.0f, 44.0f, 16.0f), Justification::centredLeft, false);
    g.drawText("MIX", Rectangle<float>(460.0f, cy - 8.0f, 36.0f, 16.0f), Justification::centredRight, false);

    g.setFont(laf.body(14.0f));
    g.setColour(LardColours::text);
    g.drawText(gainText(), Rectangle<float>(214.0f, cy - 9.0f, 70.0f, 18.0f), Justification::centredLeft, false);
    g.drawText(mixText(), Rectangle<float>(630.0f, cy - 9.0f, 54.0f, 18.0f), Justification::centredLeft, false);

    Font rk = laf.display(18.0f);
    rk.setExtraKerningFactor(0.11f);
    g.setFont(rk);
    g.setColour(LardColours::rank);
    g.drawText(rankText(), Rectangle<float>(290.0f, cy - 12.0f, 160.0f, 24.0f), Justification::centred, false);
}
