#include "PluginEditor.h"

using namespace freekmod::ui;

namespace
{
    // black print on brushed silver, safety orange for anything that lights up, and a green LCD
    const juce::Colour kAccent (226, 86, 18);
    const juce::Colour kLcd (150, 170, 78), kLcdLit (176, 198, 92), kLcdInk (26, 34, 16);

    manners::Theme theme()
    {
        manners::Theme t;
        t.name = "FREEKMOD";
        t.ink = juce::Colour (20, 20, 22);
        t.accent = kAccent;
        t.base[0] = 0.99f; t.base[1] = 1.0f; t.base[2] = 1.02f;   // a cool, clean silver
        t.leak[0] = 1.0f; t.leak[1] = 0.95f; t.leak[2] = 0.88f;   // the light leaks barely warm it
        return t;
    }

    const juce::Identifier kScaleProp { "uiScale" };

    // the LCD and the SPANNER's window, in face coordinates
    const juce::Rectangle<float> kAlgWin { 60.0f, 165.0f, 290.0f, 250.0f };
    const juce::Rectangle<float> kMoodWin { 1095.0f, 592.0f, 282.0f, 118.0f };

    juce::String aboutLine()
    {
        const juce::String date (__DATE__);
        const auto parts = juce::StringArray::fromTokens (date, " ", "");
        const auto when = parts.size() == 3 ? parts[1] + " " + parts[0].toUpperCase() + " " + parts[2] : date;
        return "FREEKMOD " + juce::String (FREEKMOD_VERSION) + ", BUILT " + when;
    }

    manners::Face* faceOf (juce::Component& c) { return c.findParentComponentOfClass<manners::Face>(); }

    int paramIndex (FreekmodProcessor& p, const char* id)
    {
        auto* prm = p.apvts.getParameter (id);
        return prm != nullptr ? juce::roundToInt (prm->convertFrom0to1 (prm->getValue())) : 0;
    }

    float paramReal (FreekmodProcessor& p, const char* id)
    {
        auto* prm = p.apvts.getParameter (id);
        return prm != nullptr ? prm->convertFrom0to1 (prm->getValue()) : 0.0f;
    }

    float voicesLit (FreekmodProcessor& p)
    {
        float lit = 0.0f;
        for (auto& m : p.synth.meterLevel) lit = juce::jmax (lit, m.load());
        return lit;
    }

    // the glass of an LCD: green, a touch brighter while notes sound, with a sheen across it
    void lcdGlass (juce::Graphics& g, juce::Rectangle<float> r, float lit)
    {
        juce::Path glass;
        glass.addRoundedRectangle (r, 10.0f);
        g.setGradientFill (juce::ColourGradient (kLcd.interpolatedWith (kLcdLit, lit), r.getCentreX(), r.getY(),
                                                 kLcd.darker (0.25f), r.getCentreX(), r.getBottom(), false));
        g.fillPath (glass);
    }

    void lcdSheen (juce::Graphics& g, juce::Rectangle<float> r)
    {
        juce::Path glass;
        glass.addRoundedRectangle (r, 10.0f);
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (glass);
        juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.0f), r.getX(), r.getY(),
                                    juce::Colours::white.withAlpha (0.0f), r.getRight(), r.getBottom(), false);
        sheen.addColour (0.30, juce::Colours::white.withAlpha (0.0f));
        sheen.addColour (0.38, juce::Colours::white.withAlpha (0.10f));
        sheen.addColour (0.46, juce::Colours::white.withAlpha (0.0f));
        g.setGradientFill (sheen);
        g.fillRect (r);
        // an inner shadow from the bezel
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.35f), 0.0f, r.getY(),
                                                 juce::Colours::transparentBlack, 0.0f, r.getY() + 14.0f, false));
        g.fillRect (r);
    }

    const char* moodWord (float s)
    {
        if (s < 0.01f) return "BEHAVING";
        if (s < 0.22f) return "ITCHY";
        if (s < 0.45f) return "SPANNERED";
        if (s < 0.72f) return "UNHINGED";
        return "HELLSCAPE";
    }
}

//==============================================================================
Legend::Legend (FreekmodProcessor& p, const char* paramId, juce::StringArray w, juce::Point<float> centre, float radius)
    : words (std::move (w)), r (radius)
{
    param = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (paramId));
    setInterceptsMouseClicks (false, false);
    const float half = r + 40.0f;
    setBounds (juce::Rectangle<float> (centre.x - half, centre.y - half, 2.0f * half, 2.0f * half).getSmallestIntegerContainer());
    c = centre - getPosition().toFloat();
}

void Legend::update()
{
    if (param == nullptr) return;
    const int now = juce::roundToInt (param->convertFrom0to1 (param->getValue()));
    if (now != shown) { shown = now; repaint(); }
}

void Legend::paint (juce::Graphics& g)
{
    auto* face = faceOf (*this);
    if (face == nullptr || words.isEmpty()) return;
    const auto ink = face->theme().ink;
    const int n = words.size();
    const float size = 15.0f;
    manners::paintWorn (g, *this, [&] (juce::Graphics& lg)
    {
        for (int i = 0; i < n; ++i)
        {
            const float t = n > 1 ? (float) i / (float) (n - 1) : 0.5f;
            const float a = juce::degreesToRadians (-135.0f + 270.0f * t);
            const float sx = std::sin (a), sy = -std::cos (a);
            const float R = r + 14.0f;
            lg.setColour (i == shown ? kAccent : ink.withAlpha (0.8f));
            manners::text (lg, words[i], c.x + sx * R, c.y + sy * R, size, 0.04f);
        }
    });
}

//==============================================================================
AlgDisplay::AlgDisplay (FreekmodProcessor& p) : proc (p)
{
    setTitle ("Algorithm: click for the next one");
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void AlgDisplay::update()
{
    const int a = paramIndex (proc, ParamIDs::alg);
    const int sd = paramIndex (proc, ParamIDs::seed);
    const float sp = paramReal (proc, ParamIDs::spanner);
    const bool lo = paramReal (proc, ParamIDs::loose) > 0.5f;
    const float l = voicesLit (proc);
    if (a != alg || sd != seed || std::abs (sp - spanner) > 0.004f || lo != loose || std::abs (l - lit) > 0.02f)
    {
        alg = a; seed = sd; spanner = sp; loose = lo; lit = l;
        repaint();
    }
}

void AlgDisplay::mouseUp (const juce::MouseEvent& e)
{
    if (! contains (e.getPosition())) return;
    if (auto* prm = proc.apvts.getParameter (ParamIDs::alg))
    {
        proc.presets->beginGesture ("algorithm");
        const int next = (paramIndex (proc, ParamIDs::alg) + (e.mods.isShiftDown() ? fm::kAlgs - 1 : 1)) % fm::kAlgs;
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) next));
        prm->endChangeGesture();
    }
}

void AlgDisplay::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (8.0f);
    const int a = juce::jlimit (0, fm::kAlgs - 1, alg);
    const auto& A = fm::alg (a);

    // where each operator sits, in columns and rows; carriers along the bottom row
    static const float pos[fm::kAlgs][4][2] = {
        { { 0, 3 }, { 0, 2 }, { 0, 1 }, { 0, 0 } },
        { { 0, 3 }, { 0, 2 }, { -0.6f, 1 }, { 0.6f, 1 } },
        { { 0, 3 }, { 0, 2 }, { 0, 1 }, { 1, 2 } },
        { { 0, 3 }, { 1, 2 }, { 0, 2 }, { 0, 1 } },
        { { -0.7f, 3 }, { -0.7f, 2 }, { 0.7f, 3 }, { 0.7f, 2 } },
        { { -1, 3 }, { 0, 3 }, { 1, 3 }, { 0, 2 } },
        { { -1, 3 }, { 0, 3 }, { 1, 3 }, { 1, 2 } },
        { { -1.5f, 3 }, { -0.5f, 3 }, { 0.5f, 3 }, { 1.5f, 3 } },
    };
    const float colW = 64.0f, rowH = 44.0f, boxW = 46.0f, boxH = 32.0f;
    // centre the drawing in the glass, whatever rows this algorithm uses
    float minRow = 3.0f;
    for (int i = 0; i < fm::kOps; ++i) minRow = juce::jmin (minRow, pos[a][i][1]);
    const float drawH = (3.0f - minRow) * rowH + boxH + 34.0f;   // boxes, then the output bar under them
    const float cx = r.getCentreX() - 10.0f;
    const float top = r.getY() + 38.0f + (r.getHeight() - 38.0f - drawH) * 0.5f + boxH * 0.5f - minRow * rowH;
    auto box = [&] (int op)
    {
        return juce::Rectangle<float> (boxW, boxH).withCentre ({ cx + pos[a][op][0] * colW, top + pos[a][op][1] * rowH });
    };

    lcdGlass (g, r, lit);
    manners::paintWorn (g, *this, [&] (juce::Graphics& lg)
    {
        lg.setColour (kLcdInk);
        manners::text (lg, "ALGORITHM " + juce::String (a + 1), r.getX() + 14.0f, r.getY() + 20.0f, 19.0f, 0.10f,
                       juce::Justification::centredLeft, 300.0f);

        const juce::PathStrokeType wire (2.2f);
        // the wires: from the bottom of each modulator to the top of what it modulates
        for (int i = 0; i < fm::kOps; ++i)
            for (int j = 0; j < fm::kOps; ++j)
                if ((A.mod[i] >> j) & 1)
                {
                    const auto from = box (j), to = box (i);
                    juce::Path p;
                    p.startNewSubPath (from.getCentreX(), from.getBottom());
                    p.lineTo (to.getCentreX(), to.getY());
                    lg.strokePath (p, wire);
                }

        // OP 4's feedback loop: out of its side and back in at the top
        {
            const auto b = box (3);
            juce::Path p;
            p.startNewSubPath (b.getRight(), b.getCentreY());
            p.cubicTo (b.getRight() + 16.0f, b.getCentreY(), b.getRight() + 14.0f, b.getY() - 14.0f,
                       b.getCentreX() + 6.0f, b.getY() - 12.0f);
            p.quadraticTo (b.getCentreX(), b.getY() - 11.0f, b.getCentreX(), b.getY());
            lg.strokePath (p, juce::PathStrokeType (1.8f));
            juce::Path head;
            head.addTriangle (b.getCentreX() - 4.5f, b.getY() - 6.0f, b.getCentreX() + 4.5f, b.getY() - 6.0f, b.getCentreX(), b.getY());
            lg.fillPath (head);
        }

        // the carriers down to the output bar
        const float outY = top + 3.0f * rowH + boxH * 0.5f + 16.0f;
        float x0 = 1.0e9f, x1 = -1.0e9f;
        for (int i = 0; i < fm::kOps; ++i)
            if ((A.carriers >> i) & 1)
            {
                const auto b = box (i);
                lg.drawLine (b.getCentreX(), b.getBottom(), b.getCentreX(), outY, 2.2f);
                x0 = juce::jmin (x0, b.getCentreX());
                x1 = juce::jmax (x1, b.getCentreX());
            }
        lg.fillRect (juce::Rectangle<float> (x0 - 4.0f, outY - 1.5f, x1 - x0 + 8.0f, 3.0f));
        manners::text (lg, "OUT", x1 + 30.0f, outY, 15.0f, 0.12f);

        // the boxes: carriers solid, modulators outlined
        for (int i = 0; i < fm::kOps; ++i)
        {
            const auto b = box (i);
            const bool car = (A.carriers >> i) & 1;
            lg.setColour (car ? kLcdInk : kLcd.interpolatedWith (kLcdLit, lit));
            lg.fillRoundedRectangle (b, 3.0f);
            lg.setColour (kLcdInk);
            lg.drawRoundedRectangle (b, 3.0f, 2.0f);
            lg.setColour (car ? kLcd.interpolatedWith (kLcdLit, lit) : kLcdInk);
            manners::text (lg, juce::String (i + 1), b.getCentreX(), b.getCentreY(), 19.0f, 0.0f);
        }

        // the SPANNER's stray wires, dotted, on top of everything, getting bolder as it goes in
        const float stray = fm::smooth (0.22f, 0.75f, spanner);
        if (stray > 0.0f)
        {
            const auto c = fm::Chaos::make (fm::chaosSeed (seed, false, 60, 0));
            for (int i = 0; i < fm::kOps; ++i)
                for (int j = 0; j < fm::kOps; ++j)
                    if (c.extra[(size_t) i][(size_t) j] != 0.0f)
                    {
                        const auto from = box (j).getCentre(), to = box (i).getCentre();
                        auto dir = to - from;
                        const float len = juce::jmax (1.0f, dir.getDistanceFromOrigin());
                        if (len < 2.0f) continue;
                        const juce::Point<float> perp (-dir.y / len, dir.x / len);
                        const float side = j > i ? 1.0f : -1.0f;
                        const float bow = side * (34.0f + 7.0f * (float) (i + j));
                        juce::Path p, dashed;
                        p.startNewSubPath (from + perp * side * (boxW * 0.5f));
                        p.quadraticTo ((from + to) * 0.5f + perp * bow, to + perp * side * (boxW * 0.5f));
                        const float dash[] = { 4.0f, 4.0f };
                        juce::PathStrokeType (2.2f).createDashedStroke (dashed, p, dash, 2);
                        lg.setColour (kLcdInk.withAlpha (stray * (0.45f + 0.55f * std::abs (c.extra[(size_t) i][(size_t) j]))));
                        lg.fillPath (dashed);
                    }
        }

        if (loose && stray > 0.0f)
        {
            lg.setColour (kLcdInk);
            manners::text (lg, "LOOSE", r.getRight() - 14.0f, r.getY() + 20.0f, 15.0f, 0.12f, juce::Justification::centredRight, 200.0f);
        }
    }, manners::Weather::screen);

    lcdSheen (g, r);
}

//==============================================================================
OpBadge::OpBadge (FreekmodProcessor& p, int o) : proc (p), op (o)
{
    setInterceptsMouseClicks (false, false);
}

void OpBadge::update()
{
    const int w = paramIndex (proc, ParamIDs::wave[op]);
    const int a = paramIndex (proc, ParamIDs::alg);
    if (w != wave || a != alg) { wave = w; alg = a; repaint(); }
}

void OpBadge::paint (juce::Graphics& g)
{
    auto* face = faceOf (*this);
    if (face == nullptr) return;
    const auto ink = face->theme().ink;
    const bool car = fm::isCarrier (alg, op);
    const float w = (float) getWidth();
    manners::paintWorn (g, *this, [&] (juce::Graphics& lg)
    {
        // one cycle of its wave, in a little frame
        const auto frame = juce::Rectangle<float> (w * 0.5f - 42.0f, 4.0f, 84.0f, 40.0f);
        lg.setColour (ink.withAlpha (0.5f));
        lg.drawRoundedRectangle (frame, 4.0f, 1.2f);
        juce::Path p;
        for (int i = 0; i <= 80; ++i)
        {
            const double ph = (double) i / 80.0;
            const float x = frame.getX() + 6.0f + (frame.getWidth() - 12.0f) * (float) ph;
            const float y = frame.getCentreY() - 14.0f * (float) fm::waveShape (wave, std::min (ph, 0.9999));
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        lg.setColour (ink);
        lg.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        lg.setColour (car ? kAccent : ink.withAlpha (0.75f));
        manners::text (lg, car ? "CARRIER" : "MODULATOR", w * 0.5f, 64.0f, 16.0f, 0.12f);
    });
}

//==============================================================================
Mood::Mood (FreekmodProcessor& p) : proc (p)
{
    setTitle ("Spanner: hold to hear a chord");
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void Mood::update()
{
    const float sp = paramReal (proc, ParamIDs::spanner);
    const int sd = paramIndex (proc, ParamIDs::seed);
    const bool lo = paramReal (proc, ParamIDs::loose) > 0.5f;
    const float l = voicesLit (proc);
    if (std::abs (sp - spanner) > 0.004f || sd != seed || lo != loose || std::abs (l - lit) > 0.02f)
    {
        spanner = sp; seed = sd; loose = lo; lit = l;
        repaint();
    }
}

void Mood::mouseDown (const juce::MouseEvent&) { proc.faceChord.store (FreekmodProcessor::press); sounding = true; }
void Mood::mouseUp (const juce::MouseEvent&) { if (sounding) proc.faceChord.store (FreekmodProcessor::letGo); sounding = false; }

void Mood::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (8.0f);
    // the further gone, the more the display itself jitters
    const float shake = fm::smooth (0.72f, 1.0f, spanner) * 2.5f;
    juce::Random rnd ((juce::int64) (seed * 31 + (int) (spanner * 100.0f)));
    const float dx = shake * (rnd.nextFloat() - 0.5f) * 2.0f, dy = shake * (rnd.nextFloat() - 0.5f) * 2.0f;

    lcdGlass (g, r, lit);
    manners::paintWorn (g, *this, [&] (juce::Graphics& lg)
    {
        lg.setColour (kLcdInk);
        manners::text (lg, moodWord (spanner), r.getCentreX() + dx, r.getY() + 38.0f + dy, 34.0f, 0.10f);
        manners::text (lg, "SEED " + juce::String (seed).paddedLeft ('0', 4), r.getX() + 14.0f, r.getBottom() - 20.0f, 16.0f, 0.12f,
                       juce::Justification::centredLeft, 200.0f);
        manners::text (lg, loose ? "EVERY NOTE NEW" : "SAME EVERY TIME", r.getRight() - 14.0f, r.getBottom() - 20.0f, 16.0f, 0.08f,
                       juce::Justification::centredRight, 200.0f);
    }, manners::Weather::screen);

    lcdSheen (g, r);
}

//==============================================================================
FreekmodEditor::FreekmodEditor (FreekmodProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p), face (theme()), algDisplay (p), mood (p),
      menu (*p.presets, { [this] (float s) { setScale (s); }, [this] { return scale(); }, aboutLine() })
{
    namespace P = ParamIDs;
    addAndMakeVisible (face);
    face.setBounds (0, 0, manners::kBaseW, manners::kBaseH);

    constexpr float kHeadA = 140.0f, kRowA = 215.0f, kRowB = 362.0f;
    constexpr float kHeadC = 455.0f, kRowC = 530.0f;
    constexpr float kHeadD = 620.0f, kRowD = 695.0f;
    static constexpr float kR = 30.0f, kLabel = 20.0f;

    face.addHeading ("ALGORITHM", 50.0f, 470.0f, kHeadA);
    for (int k = 0; k < 4; ++k)
    {
        const float x0 = 500.0f + 222.0f * (float) k;
        face.addHeading ("OP " + juce::String (k + 1), x0, x0 + 200.0f, kHeadA);
    }
    face.addHeading ("TIMBRE", 50.0f, 570.0f, kHeadC);
    face.addHeading ("CHARACTER", 600.0f, 830.0f, kHeadC);
    face.addHeading ("SPANNER", 860.0f, 1382.0f, kHeadC);
    face.addHeading ("AMPLIFIER", 50.0f, 570.0f, kHeadD);
    face.addHeading ("OUTPUT", 600.0f, 830.0f, kHeadD);
    face.addWindow (kAlgWin);
    face.addWindow (kMoodWin);

    face.addAndMakeVisible (algDisplay);
    algDisplay.setBounds (kAlgWin.toNearestInt());
    face.addAndMakeVisible (mood);
    mood.setBounds (kMoodWin.toNearestInt());

    auto dial = [this] (const char* id, const char* label, float cx, float cy, float r = 30.0f, float labelSize = 20.0f)
    {
        auto d = std::make_unique<manners::Dial> (proc.apvts, id, label, r, labelSize);
        face.addAndMakeVisible (*d);
        d->place (cx, cy);
        dials.push_back (std::move (d));
    };

    {
        auto l = std::make_unique<Legend> (proc, P::alg, juce::StringArray { "1", "2", "3", "4", "5", "6", "7", "8" },
                                           juce::Point<float> { 412.0f, 290.0f }, kR);
        face.addAndMakeVisible (*l);
        legends.push_back (std::move (l));
    }
    dial (P::alg, "ALG", 412.0f, 290.0f);

    for (int k = 0; k < 4; ++k)
    {
        const float x0 = 500.0f + 222.0f * (float) k;
        dial (P::ratio[k], "RATIO", x0 + 50.0f, kRowA);
        dial (P::level[k], "LEVEL", x0 + 150.0f, kRowA);
        dial (P::wave[k], "WAVE", x0 + 50.0f, kRowB);
        auto b = std::make_unique<OpBadge> (proc, k);
        face.addAndMakeVisible (*b);
        b->setBounds (juce::Rectangle<float> (x0 + 95.0f, kRowB - 32.0f, 110.0f, 80.0f).toNearestInt());
        badges.push_back (std::move (b));
    }

    // TIMBRE: the four big ideas
    dial (P::bright, "BRIGHT", 100.0f, kRowC);
    dial (P::bite, "BITE", 205.0f, kRowC);
    dial (P::tail, "TAIL", 310.0f, kRowC);
    dial (P::feedback, "FEEDBACK", 415.0f, kRowC);
    dial (P::touch, "TOUCH", 520.0f, kRowC);
    // CHARACTER
    dial (P::detune, "DETUNE", 660.0f, kRowC);
    dial (P::grit, "GRIT", 770.0f, kRowC);
    // AMPLIFIER
    dial (P::attack, "ATTACK", 100.0f, kRowD);
    dial (P::decay, "DECAY", 205.0f, kRowD);
    dial (P::sustain, "SUSTAIN", 310.0f, kRowD);
    dial (P::release, "RELEASE", 415.0f, kRowD);
    dial (P::ampVel, "VEL", 520.0f, kRowD);
    // OUTPUT
    dial (P::vibrato, "VIBRATO", 660.0f, kRowD);
    dial (P::volume, "VOLUME", 770.0f, kRowD);
    // the SPANNER
    dial (P::spanner, "SPANNER", 975.0f, 612.0f, 70.0f, 26.0f);

    {
        auto t = std::make_unique<manners::Lamp> ("THROW", 24.0f);
        face.addAndMakeVisible (*t);
        t->place (1110.0f, 512.0f);
        t->setTitle ("Throw: a new spanner seed");
        t->onClick = [this] { proc.throwSpanner(); };
        lamps.push_back (std::move (t));

        auto l = std::make_unique<manners::Lamp> ("LOOSE", 24.0f);
        l->setClickingTogglesState (true);
        face.addAndMakeVisible (*l);
        l->place (1110.0f, 554.0f);
        buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, P::loose, *l));
        l->onClick = [this] { proc.presets->beginGesture ("loose"); };
        lamps.push_back (std::move (l));
    }

    face.addAndMakeVisible (strip);
    strip.setBounds (36, 798, manners::kBaseW - 72, 62);
    strip.onStep = [this] (int d) { proc.presets->step (d); };
    face.onReadout = [this] (const juce::String& s) { strip.show (s); };
    face.onGesture = [this] { proc.presets->beginGesture ("Turn knob"); };

    face.addAndMakeVisible (menuIcon);
    menuIcon.setBounds (1312, 44, 74, 52);
    menuIcon.onClick = [this] { if (menu.isOpen()) menu.close(); else menu.open(); };
    face.addChildComponent (menu);

    setResizable (true, true);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) manners::kBaseW / (double) manners::kBaseH);
    setResizeLimits (manners::kBaseW / 2, manners::kBaseH / 2, (int) (manners::kBaseW * 1.6), (int) (manners::kBaseH * 1.6));
    const float s = juce::jlimit (0.5f, 1.6f, (float) (double) proc.apvts.state.getProperty (kScaleProp, 0.75));
    setSize (juce::roundToInt ((float) manners::kBaseW * s), juce::roundToInt ((float) manners::kBaseH * s));

    timerCallback();
    startTimerHz (30);
}

FreekmodEditor::~FreekmodEditor()
{
    stopTimer();
}

void FreekmodEditor::setScale (float s)
{
    setSize (juce::roundToInt ((float) manners::kBaseW * s), juce::roundToInt ((float) manners::kBaseH * s));
}

void FreekmodEditor::resized()
{
    face.setTransform (juce::AffineTransform::scale (scale()));
    proc.apvts.state.setProperty (kScaleProp, scale(), nullptr);
}

void FreekmodEditor::openMenu (manners::MenuCard::Page page)
{
    menu.open();
    if (page != manners::MenuCard::Page::main)
        menu.show (page);
}

void FreekmodEditor::timerCallback()
{
    algDisplay.update();
    mood.update();
    for (auto& b : badges) b->update();
    for (auto& l : legends) l->update();

    const auto name = proc.presets->currentName().toUpperCase();
    if (name != shownPreset)
    {
        shownPreset = name;
        strip.setPreset (name);
    }
}
