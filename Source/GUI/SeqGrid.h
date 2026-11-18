#pragma once

#include "../PluginProcessor.h"
#include "LookAndFeel.h"
#include "Widgets.h"

namespace mm::gui
{

// Botón 🙂 dibujado en vectores: tira un patrón sorpresa
class SmileyButton : public juce::Button
{
public:
    SmileyButton() : juce::Button ("Surprise") { setTooltip ("Surprise pattern"); }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        const float d = jmin (r.getWidth(), r.getHeight()) - (down ? 2.0f : 0.0f);
        const auto face = Rectangle<float> (d, d).withCentre (r.getCentre());

        g.setColour (Colours::black.withAlpha (0.45f));
        g.fillEllipse (face.translated (0.0f, 1.5f));
        g.setGradientFill (ColourGradient (Palette::accent.brighter (over ? 0.35f : 0.15f), face.getX(), face.getY(),
                                           Palette::accent.darker (0.35f), face.getX(), face.getBottom(), false));
        g.fillEllipse (face);

        // Ojos y sonrisa
        g.setColour (Colours::black.withAlpha (0.85f));
        const float e = d * 0.11f;
        g.fillEllipse (face.getX() + d * 0.30f - e * 0.5f, face.getY() + d * 0.34f, e, e * 1.3f);
        g.fillEllipse (face.getX() + d * 0.70f - e * 0.5f, face.getY() + d * 0.34f, e, e * 1.3f);
        Path smile;
        smile.addCentredArc (face.getCentreX(), face.getY() + d * 0.52f, d * 0.26f, d * 0.22f, 0.0f,
                             MathConstants<float>::pi * 0.62f, MathConstants<float>::pi * 1.38f, true);
        g.strokePath (smile, PathStrokeType (d * 0.075f, PathStrokeType::curved, PathStrokeType::rounded));
    }
};

// Matriz del secuenciador melódico: 64 pasos, 32 visibles a la vez (páginas 1-32 y 33-64).
//  - Fila STEP: ritmo euclidiano (el punto marca el golpe base; clic invierte el paso)
//  - Carriles NOTE / OCT / VELO / GATE / PROB, cada uno con su propia longitud (LENGTH)
//  - Aleatorización del carril visible, del ritmo o de todo a la vez
class SeqGrid : public juce::Component, private juce::Timer
{
public:
    explicit SeqGrid (URingAudioProcessor& p) : proc (p)
    {
        for (int i = 0; i < cz::kNumValueLanes; ++i)
        {
            auto& b = laneButtons[(size_t) i];
            b.setButtonText (laneButtonText (i));
            b.setClickingTogglesState (true);
            b.setRadioGroupId (9191);
            b.setColour (juce::TextButton::buttonOnColourId, Palette::accent);
            b.onClick = [this, i] { if (laneButtons[(size_t) i].getToggleState()) selectLane (i); };
            addAndMakeVisible (b);
        }
        laneButtons[0].setToggleState (true, juce::dontSendNotification);
        laneButtons[(size_t) cz::Lane::Model].setTooltip (mm::lang::tr ("C MODEL: a different engine C model on each step"));

        auto styleText = [] (juce::TextButton& b, juce::Colour c) { b.setColour (juce::TextButton::textColourOffId, c); };
        styleText (rndLaneBtn, Palette::text);
        styleText (rndRhythmBtn, Palette::text);
        styleText (rndAllBtn, Palette::accent);
        styleText (generateBtn, Palette::accent);
        styleText (resetBtn, Palette::text);

        rndLaneBtn.onClick   = [this] { proc.randomizeLane ((cz::Lane) lane); feedback ("RANDOM", juce::String (names[lane]) + " LANE"); };
        rndRhythmBtn.onClick = [this] { proc.randomizeRhythm(); feedback ("RANDOM", "RHYTHM"); };
        rndAllBtn.onClick    = [this] { proc.randomizeAll(); feedback ("RANDOM", "ALL LANES + RHYTHM"); };
        smiley.onClick       = [this] { feedback ("SURPRISE!", proc.surprise()); };

        setupField (stepsField);
        setupField (lengthField);
        stepsField.onTextChange = [this]
        {
            const int v = stepsField.getText().trim().getIntValue();
            if (v > 0)
                nudgeSteps (v - param (IDs::seqLength));
        };
        lengthField.onTextChange = [this]
        {
            const auto t = lengthField.getText().trim();
            const int v = t.equalsIgnoreCase ("auto") ? 0 : juce::jlimit (0, cz::SeqPattern::kSteps, t.getIntValue());
            nudgeLength (v - param (IDs::seqLaneLen[lane]));
        };
        fitBtn.onClick       = [this]
        {
            proc.fitPatternToScale();
            feedback ("FIT SCALE", juce::String (proc.currentScale().name).toUpperCase());
        };
        styleText (fitBtn, Palette::accent);
        addAndMakeVisible (fitBtn);
        addAndMakeVisible (smiley);
        generateBtn.onClick  = [this]
        {
            proc.generatePattern();
            feedback ("SEQUENCE", "NEW MELODY IN " + juce::String (proc.currentScale().name).toUpperCase());
        };
        resetBtn.onClick = [this]
        {
            for (int i = 0; i < cz::SeqPattern::kSteps; ++i)
            {
                auto d = proc.pattern().get (i);
                d.flip = false;
                proc.pattern().set (i, d);
            }
            feedback ("SEQUENCE", "PURE EUCLID RHYTHM");
        };
        for (auto* b : { &rndLaneBtn, &rndRhythmBtn, &rndAllBtn, &generateBtn, &resetBtn, &lenMinus, &lenPlus,
                         &stepsMinus, &stepsPlus })
            addAndMakeVisible (b);
        styleText (stepsMinus, Palette::text);
        styleText (stepsPlus, Palette::text);
        stepsMinus.onClick = [this] { nudgeSteps (juce::ModifierKeys::currentModifiers.isShiftDown() ? -4 : -1); };
        stepsPlus.onClick  = [this] { nudgeSteps (juce::ModifierKeys::currentModifiers.isShiftDown() ? 4 : 1); };
        styleText (lenMinus, Palette::text);
        styleText (lenPlus, Palette::text);
        lenMinus.onClick = [this] { nudgeLength (juce::ModifierKeys::currentModifiers.isShiftDown() ? -4 : -1); };
        lenPlus.onClick  = [this] { nudgeLength (juce::ModifierKeys::currentModifiers.isShiftDown() ? 4 : 1); };

        // ---- Memorias de patrón A–H, cadena y copiar/pegar
        for (int i = 0; i < URingAudioProcessor::kNumPatterns; ++i)
        {
            auto& b = patternButtons[(size_t) i];
            b.setButtonText (juce::String::charToString ((juce::juce_wchar) ('A' + i)));
            b.setClickingTogglesState (true);
            b.setRadioGroupId (5150);
            b.onClick = [this, i]
            {
                proc.selectPattern (i);
                feedback ("PATTERN " + juce::String::charToString ((juce::juce_wchar) ('A' + i)),
                          proc.getQueuedPattern() == i ? "STARTS NEXT CYCLE" : "ACTIVE");
            };
            addAndMakeVisible (b);
        }
        chainBtn.setClickingTogglesState (true);
        chainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::seqChainOn, chainBtn);
        addAndMakeVisible (chainBtn);
        setupField (chainField);
        chainField.setFont (uiFont (11.5f, true));
        chainField.setText (proc.getChain().isEmpty() ? "A B" : proc.getChain(), juce::dontSendNotification);
        if (proc.getChain().isEmpty()) proc.setChain ("A B");
        chainField.onTextChange = [this]
        {
            proc.setChain (chainField.getText());
            chainField.setText (proc.getChain(), juce::dontSendNotification);
            feedback ("CHAIN", proc.getChain());
        };
        copyBtn.onClick  = [this] { proc.copyPattern(); feedback ("PATTERN", "COPIED"); };
        pasteBtn.onClick = [this] { feedback ("PATTERN", proc.pastePattern() ? "PASTED" : "NOTHING TO PASTE"); };
        for (auto* b : { &copyBtn, &pasteBtn })
        {
            styleText (*b, Palette::text);
            addAndMakeVisible (b);
        }

        selectLane (0);
        timerCallback(); // valores iniciales de los campos numéricos
        startTimerHz (30);
    }

    std::function<void (const juce::String&, const juce::String&)> onFeedback;
    // Botones de carril: rótulos cortos (entran 9 en la fila)
    static juce::String laneButtonText (int i)
    {
        static const char* en[] { "NOTE", "OCT", "VELO", "GATE", "PROB", "RTCH", "COND", "LOCK", "MOD" };
        static const char* es[] { "NOTA", "OCT", "VELO", "DUR", "PROB", "RTCH", "COND", "LOCK", "MOD" };
        return (mm::lang::spanish() ? es : en)[juce::jlimit (0, cz::kNumValueLanes - 1, i)];
    }
    void showLane (int l) { laneButtons[(size_t) l].setToggleState (true, juce::dontSendNotification); selectLane (l); }

    void retranslate()
    {
        using mm::lang::tr;
        for (int i = 0; i < cz::kNumValueLanes; ++i) laneButtons[(size_t) i].setButtonText (laneButtonText (i));
        rndLaneBtn.setButtonText (tr ("RND LANE")); rndRhythmBtn.setButtonText (tr ("RND RHYTHM")); rndAllBtn.setButtonText (tr ("RND ALL"));
        generateBtn.setButtonText (tr ("GENERATE")); resetBtn.setButtonText (tr ("RESET")); fitBtn.setButtonText (tr ("FIT SCALE"));
        chainBtn.setButtonText (tr ("CHAIN")); copyBtn.setButtonText (tr ("COPY")); pasteBtn.setButtonText (tr ("PASTE"));
        smiley.setTooltip (tr ("Surprise pattern"));
        stepsField.setTooltip (tr ("Click to type a number"));
        lengthField.setTooltip (tr ("Click to type a number"));
        repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (22, 12);
        auto header = r.removeFromTop (30);
        for (auto& b : laneButtons)
            b.setBounds (header.removeFromLeft (42).reduced (2, 0));
        header.removeFromLeft (8);
        stepsArea = header.removeFromLeft (132);
        stepsMinus.setBounds (stepsArea.getX() + 44, stepsArea.getY(), 28, stepsArea.getHeight());
        stepsPlus.setBounds (stepsArea.getRight() - 28, stepsArea.getY(), 28, stepsArea.getHeight());
        header.removeFromLeft (6);
        lengthArea = header.removeFromLeft (140);
        lenMinus.setBounds (lengthArea.getX() + 50, lengthArea.getY(), 28, lengthArea.getHeight());
        lenPlus.setBounds (lengthArea.getRight() - 28, lengthArea.getY(), 28, lengthArea.getHeight());
        stepsField.setBounds (stepsArea.withTrimmedLeft (74).withTrimmedRight (30).reduced (1, 3));
        lengthField.setBounds (lengthArea.withTrimmedLeft (78).withTrimmedRight (28).reduced (1, 3));
        header.removeFromLeft (6);
        for (auto* b : { &rndLaneBtn, &rndRhythmBtn, &rndAllBtn })
            b->setBounds (header.removeFromLeft (72).reduced (2, 0));
        fitBtn.setBounds (header.removeFromLeft (88).reduced (2, 0));
        resetBtn.setBounds (header.removeFromRight (76).reduced (2, 0));
        generateBtn.setBounds (header.removeFromRight (76).reduced (2, 0));
        smiley.setBounds (header.removeFromRight (36).withSizeKeepingCentre (32, 32));

        r.removeFromTop (12);
        labelCol = r.withWidth (64);
        r.removeFromLeft (68);
        content = r;
        pagesRow = r.removeFromTop (24);
        {
            auto pr = pagesRow.withTrimmedLeft (3 + kPages * 128 + 70); // tras las pestañas y la etiqueta PATTERN
            for (auto& b : patternButtons)
                b.setBounds (pr.removeFromLeft (32).reduced (1, 0));
            pr.removeFromLeft (12);
            chainBtn.setBounds (pr.removeFromLeft (76).reduced (1, 0));
            chainField.setBounds (pr.removeFromLeft (110).reduced (2, 1));
            pr.removeFromLeft (10);
            copyBtn.setBounds (pr.removeFromLeft (62).reduced (1, 0));
            pasteBtn.setBounds (pr.removeFromLeft (62).reduced (1, 0));
        }
        r.removeFromTop (8);
        padsRow = r.removeFromTop (40);
        r.removeFromTop (10);
        laneArea = r;
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        const auto seqAccent = proc.getEditSequencer() == 1 ? Colour (0xff3fd8ff) : Palette::accent; // SEQ 2 en cian
        drawModulePanel (g, getLocalBounds().toFloat(), {});
        const int len = param (IDs::seqLength);
        const int pulses = param (IDs::seqPulses);
        const int rotate = param (IDs::seqRotate);
        const int laneLen = effectiveLaneLength();
        const int cur = proc.editSequencer().getCurrentStep();
        const int laneCur = proc.editSequencer().getLaneStep ((cz::Lane) lane);
        const auto sc = proc.currentScale();

        g.setFont (uiFont (10.0f, true));
        g.setColour (Palette::dim);
        g.drawText (mm::lang::tr ("PAGE"), labelCol.withY (pagesRow.getY()).withHeight (pagesRow.getHeight()), Justification::centredLeft);
        g.drawText (mm::lang::tr ("STEP"), labelCol.withY (padsRow.getY()).withHeight (padsRow.getHeight()), Justification::centredLeft);
        g.drawText (mm::lang::tr ("PATTERN"), Rectangle<int> (content.getX() + 3 + kPages * 128 + 8, pagesRow.getY(), 62, pagesRow.getHeight()),
                    Justification::centredLeft);
        g.drawText (names[lane], labelCol.withY (laneArea.getY()).withHeight (20), Justification::centredLeft);

        // Páginas: blanca = la que se edita, gris = dentro de STEPS, naranja = la que suena
        const int numPages = (len + kCols - 1) / kCols;
        for (int pg = 0; pg < kPages; ++pg)
        {
            const auto pr = pageRect (pg).toFloat().reduced (3.0f, 1.0f);
            g.setColour (pg == page ? Palette::text : pg < numPages ? Palette::amber.withAlpha (0.6f) : Palette::padOff);
            g.fillRect (pr);
            if (cur >= 0 && cur / kCols == pg)
            {
                g.setColour (seqAccent);
                g.fillRect (pr.withHeight (3.0f).withY (pr.getBottom() - 3.0f));
            }
            g.setColour (pg == page ? Colours::black : Palette::dim);
            g.setFont (uiFont (10.0f, true));
            g.drawText (mm::lang::ui ("STEPS ") + String (pg * kCols + 1) + "-" + String ((pg + 1) * kCols), pr, Justification::centred);
        }

        if (lane == (int) cz::Lane::Octave || lane == (int) cz::Lane::Lock)
        {
            g.setColour (Palette::panelEdge);
            g.drawHorizontalLine ((int) valueToY (0.5f), (float) content.getX(), (float) content.getRight());
        }

        for (int c = 0; c < kCols; ++c)
        {
            const int i = page * kCols + c;
            const auto d = proc.pattern().get (i);

            // ---- Pads de ritmo
            const bool inLen = i < len;
            const bool euclid = inLen && cz::euclidHit (i, pulses, len, rotate);
            const bool on = inLen && (euclid != d.flip);
            const float alpha = inLen ? 1.0f : 0.3f;

            const auto pr = column (c, padsRow).toFloat().reduced (3.0f, 0.0f);
            g.setColour ((on ? Palette::text : Palette::padOff).withMultipliedAlpha (alpha));
            g.fillRect (pr);
            if (euclid)
            {
                g.setColour (on ? Colours::black.withAlpha (0.5f) : Palette::dim);
                g.fillEllipse (pr.getCentreX() - 2.0f, pr.getY() + 5.0f, 4.0f, 4.0f);
            }
            if (i == cur)
            {
                g.setColour (seqAccent);
                g.fillRect (pr.withHeight (3.0f).withY (pr.getBottom() - 3.0f));
            }
            g.setColour (on ? Colours::black.withAlpha (0.75f) : Palette::dim.withMultipliedAlpha (alpha));
            g.setFont (uiFont (9.5f));
            g.drawText (String (i + 1), pr.withTrimmedTop (8.0f), Justification::centred);

            // ---- Carril seleccionado (con su propia longitud)
            const bool inLane = i < laneLen;
            const float la = inLane ? 1.0f : 0.25f;
            const auto lr = column (c, laneArea).toFloat().reduced (3.0f, 0.0f);
            g.setColour (i == laneCur ? Palette::padOff.brighter (0.15f) : Palette::padOff);
            g.fillRect (lr);
            if (i == laneCur)
            {
                g.setColour (seqAccent);
                g.fillRect (lr.withHeight (2.0f));
            }
            g.setColour (seqAccent.withMultipliedAlpha (la));

            switch ((cz::Lane) lane)
            {
                case cz::Lane::Note:
                {
                    // Fuera de escala: bloque blanco con borde naranja (FIT SCALE lo corrige)
                    const bool inScale = quantizeToScale (sc, d.semis) == d.semis;
                    const float y = valueToY (norm (d.semis, cz::SeqPattern::kMinNote, cz::SeqPattern::kMaxNote));
                    const auto block = Rectangle<float> (lr.getX(), y - 7.0f, lr.getWidth(), 14.0f);
                    if (! inScale) g.setColour (Palette::text.withMultipliedAlpha (la));
                    g.fillRect (block);
                    if (! inScale) { g.setColour (seqAccent); g.drawRect (block, 1.5f); }
                    g.setColour (Colours::black.withAlpha (0.85f * la));
                    g.setFont (uiFont (8.5f, true));
                    g.drawText (noteName (d.semis), block, Justification::centred, false);
                    break;
                }
                case cz::Lane::Octave:
                {
                    const float mid = valueToY (0.5f);
                    const float y = valueToY (norm (d.octave, cz::SeqPattern::kMinOctave, cz::SeqPattern::kMaxOctave));
                    g.fillRect (Rectangle<float>::leftTopRightBottom (lr.getX(), jmin (mid, y) - 1.0f, lr.getRight(), jmax (mid, y) + 1.0f));
                    const auto label = Rectangle<float> (lr.getX(), y - 7.0f, lr.getWidth(), 14.0f);
                    g.fillRect (label);
                    g.setColour (Colours::black.withAlpha (0.85f * la));
                    g.setFont (uiFont (9.5f, true));
                    g.drawText ((d.octave > 0 ? "+" : "") + String (d.octave), label, Justification::centred);
                    break;
                }
                case cz::Lane::Ratchet: // 1-4 golpes por paso
                {
                    const float frac = (float) (d.ratchet - 1) / (float) (cz::SeqPattern::kMaxRatchet - 1);
                    const float top = valueToY (frac);
                    const float h = (lr.getBottom() - 8.0f - top) / (float) d.ratchet;
                    for (int k = 0; k < d.ratchet; ++k) // un bloque por golpe
                        g.fillRect (Rectangle<float> (lr.getX(), top + h * (float) k + 1.0f, lr.getWidth(), juce::jmax (2.0f, h - 3.0f)));
                    g.setColour (Colours::black.withAlpha (0.85f * la));
                    g.setFont (uiFont (9.5f, true));
                    g.drawText ("x" + String (d.ratchet), Rectangle<float> (lr.getX(), top, lr.getWidth(), 14.0f), Justification::centred);
                    break;
                }
                case cz::Lane::Cond:
                {
                    const float y = valueToY ((float) d.cond / (float) ((int) cz::Cond::NumConds - 1));
                    const auto block = Rectangle<float> (lr.getX(), y - 7.0f, lr.getWidth(), 14.0f);
                    if (d.cond == 0) g.setColour (Palette::dim.withMultipliedAlpha (la));
                    g.fillRect (block);
                    g.setColour (Colours::black.withAlpha (0.85f * la));
                    g.setFont (uiFont (8.5f, true));
                    g.drawText (cz::condName (d.cond), block, Justification::centred, false);
                    break;
                }
                case cz::Lane::Lock: // parameter lock bipolar: barra desde el centro (vacío = sin lock)
                {
                    if (d.lock != 0.0f)
                    {
                        const float y0 = valueToY (0.5f), y1 = valueToY (0.5f + d.lock * 0.5f);
                        g.fillRect (Rectangle<float>::leftTopRightBottom (lr.getX(), juce::jmin (y0, y1), lr.getRight(), juce::jmax (y0, y1)));
                    }
                    break;
                }
                case cz::Lane::Model: // modelo del motor C (vacío = el de la perilla)
                {
                    if (d.model >= 0)
                    {
                        const float y = valueToY (modelToValue (d.model));
                        const auto block = Rectangle<float> (lr.getX(), y - 7.0f, lr.getWidth(), 14.0f);
                        g.setColour (Palette::engineC.withMultipliedAlpha (la));
                        g.fillRect (Rectangle<float>::leftTopRightBottom (lr.getX() + lr.getWidth() * 0.4f, y, lr.getRight() - lr.getWidth() * 0.4f, lr.getBottom()));
                        g.fillRect (block);
                        g.setColour (Colours::black.withAlpha (0.85f * la));
                        g.setFont (uiFont (9.0f, true));
                        g.drawText (String (d.model + 1), block, Justification::centred, false);
                    }
                    break;
                }
                case cz::Lane::Gate: // hasta 4 pasos; marcas en 1, 2 y 3 pasos
                {
                    g.fillRect (Rectangle<float>::leftTopRightBottom (lr.getX(), valueToY (d.gate / kMaxGate), lr.getRight(), lr.getBottom()));
                    g.setColour (Colours::black.withAlpha (0.45f));
                    for (int k = 1; k < (int) kMaxGate; ++k)
                        g.drawHorizontalLine ((int) valueToY ((float) k / kMaxGate), lr.getX(), lr.getRight());
                    break;
                }
                default:
                {
                    const float v = lane == (int) cz::Lane::Velocity ? d.velocity : d.prob;
                    g.fillRect (Rectangle<float>::leftTopRightBottom (lr.getX(), valueToY (v), lr.getRight(), lr.getBottom()));
                    break;
                }
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (pagesRow.contains (e.getPosition()))
        {
            for (int pg = 0; pg < kPages; ++pg)
                if (pageRect (pg).contains (e.getPosition()))
                {
                    page = pg;
                    feedback ("PAGE " + juce::String (pg + 1), "STEPS " + juce::String (pg * kCols + 1) + "-" + juce::String ((pg + 1) * kCols));
                    repaint();
                }
            return;
        }

        const int col = stepAt (e.x);
        lastCol = col;
        if (col < 0) return;

        if (padsRow.contains (e.getPosition()))
        {
            dragPads = true;
            auto d = proc.pattern().get (col);
            d.flip = ! d.flip;
            paintFlip = d.flip;
            proc.pattern().set (col, d);
            feedback ("STEP " + juce::String (col + 1), d.flip ? "EDITED" : "EUCLID");
        }
        else if (laneArea.contains (e.getPosition()))
        {
            dragPads = false;
            editLane (col, e.y);
        }
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const int col = stepAt (e.x);
        if (col < 0) return;
        if (dragPads)
        {
            if (col != lastCol)
            {
                auto d = proc.pattern().get (col);
                d.flip = paintFlip;
                proc.pattern().set (col, d);
            }
        }
        else
        {
            editLane (col, e.y);
        }
        lastCol = col;
        repaint();
    }

private:
    static constexpr int kCols = 32; // pasos visibles a la vez
    static constexpr float kMaxGate = cz::StepSequencer::kMaxGateSteps;
    static constexpr int kPages = cz::SeqPattern::kSteps / kCols;
    static constexpr const char* names[] { "NOTE", "OCTAVE", "VELOCITY", "GATE", "PROBABILITY", "RATCHET", "CONDITION", "LOCK", "C MODEL" };

    void timerCallback() override
    {
        if (! chainField.isBeingEdited() && chainField.getText() != proc.getChain())
            chainField.setText (proc.getChain(), juce::dontSendNotification);
        const int active = proc.getActivePattern(), queued = proc.getQueuedPattern();
        for (int i = 0; i < (int) patternButtons.size(); ++i)
        {
            auto& b = patternButtons[(size_t) i];
            b.setToggleState (i == active, juce::dontSendNotification);
            b.setColour (juce::TextButton::textColourOffId, i == queued ? Palette::accent : Palette::dim);
        }
        // Los campos numéricos muestran el valor actual salvo mientras los estás escribiendo
        if (! stepsField.isBeingEdited())
            stepsField.setText (juce::String (param (IDs::seqLength)), juce::dontSendNotification);
        if (! lengthField.isBeingEdited())
        {
            const int ll = param (IDs::seqLaneLen[lane]);
            lengthField.setText (ll == 0 ? juce::String ("AUTO") : juce::String (ll), juce::dontSendNotification);
        }
        repaint();
    }

    // Campo numérico: clic para escribir, Enter para aplicar
    void setupField (juce::Label& f)
    {
        f.setEditable (true, true, false);
        f.setJustificationType (juce::Justification::centred);
        f.setFont (uiFont (13.0f, true));
        f.setBorderSize ({ 1, 2, 1, 2 });
        f.setColour (juce::Label::textColourId, Palette::text);
        f.setColour (juce::Label::backgroundColourId, Palette::lcdBg);
        f.setColour (juce::Label::outlineColourId, juce::Colours::black);
        f.setColour (juce::Label::textWhenEditingColourId, Palette::accent);
        f.setColour (juce::Label::backgroundWhenEditingColourId, Palette::lcdBg);
        f.setColour (juce::Label::outlineWhenEditingColourId, Palette::accent);
        f.setTooltip ("Click to type a number");
        addAndMakeVisible (f);
    }

    void feedback (const juce::String& a, const juce::String& b) { if (onFeedback) onFeedback (a, b); }

    int param (juce::StringRef id) const { return (int) proc.apvts.getRawParameterValue (id)->load(); }

    void selectLane (int l)
    {
        lane = l;
        repaint();
    }

    // Longitud del carril visible: 0 = AUTO (sigue a STEPS); shift = de a 4 pasos
    int effectiveLaneLength() const
    {
        const int v = param (IDs::seqLaneLen[lane]);
        return v > 0 ? v : param (IDs::seqLength);
    }

    void setParamValue (juce::StringRef id, int v)
    {
        auto* p = proc.apvts.getParameter (id);
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) v));
        p->endChangeGesture();
    }

    void nudgeSteps (int delta)
    {
        const int v = juce::jlimit (1, cz::SeqPattern::kSteps, param (IDs::seqLength) + delta);
        setParamValue (IDs::seqLength, v);
        if (v > (page + 1) * kCols) page = (v - 1) / kCols; // salta a la página donde terminan los pasos
        feedback ("STEPS", juce::String (v));
        repaint();
    }

    void nudgeLength (int delta)
    {
        auto* p = proc.apvts.getParameter (IDs::seqLaneLen[lane]);
        const int v = juce::jlimit (0, cz::SeqPattern::kSteps, param (IDs::seqLaneLen[lane]) + delta);
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) v));
        p->endChangeGesture();
        feedback (juce::String (names[lane]) + " LENGTH", v == 0 ? juce::String ("AUTO (= STEPS)") : juce::String (v) + " STEPS");
        repaint();
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        g.setColour (Palette::dim);
        g.setFont (uiFont (10.0f, true));
        g.drawText (mm::lang::tr ("STEPS"), stepsArea.withWidth (48), juce::Justification::centredLeft);
        g.drawText (mm::lang::tr ("LENGTH"), lengthArea.withWidth (54), juce::Justification::centredLeft);
    }

    juce::Rectangle<int> pageRect (int pg) const
    {
        // Pestañas compactas a la izquierda (no se alinean con columnas para no confundir)
        return { content.getX() + 3 + pg * 128, pagesRow.getY(), 122, pagesRow.getHeight() };
    }

    int stepAt (int x) const
    {
        const int c = columnAt (x);
        return c < 0 ? -1 : page * kCols + c;
    }

    juce::Rectangle<int> column (int i, juce::Rectangle<int> row) const
    {
        constexpr int groupGap = 8;
        const float w = (float) (content.getWidth() - (kCols / 4 - 1) * groupGap) / kCols;
        const int x = content.getX() + juce::roundToInt ((float) i * w) + (i / 4) * groupGap;
        return { x, row.getY(), juce::roundToInt (w), row.getHeight() };
    }

    int columnAt (int x) const
    {
        for (int i = 0; i < kCols; ++i)
        {
            const auto c = column (i, content);
            if (x >= c.getX() - 4 && x < c.getRight() + 4)
                return i;
        }
        return -1;
    }

    float valueToY (float n) const
    {
        const float top = (float) laneArea.getY() + 8.0f, bottom = (float) laneArea.getBottom() - 8.0f;
        return bottom - juce::jlimit (0.0f, 1.0f, n) * (bottom - top);
    }

    float yToValue (int y) const
    {
        const float top = (float) laneArea.getY() + 8.0f, bottom = (float) laneArea.getBottom() - 8.0f;
        return juce::jlimit (0.0f, 1.0f, (bottom - (float) y) / (bottom - top));
    }

    static float modelToValue (int m) { return 0.08f + 0.92f * (float) m / (float) (cz::kNumCModels - 1); }
    static float norm (int v, int lo, int hi) { return (float) (v - lo) / (float) (hi - lo); }
    static int denorm (float n, int lo, int hi) { return juce::roundToInt ((float) lo + n * (float) (hi - lo)); }

    juce::String noteName (int semis) const
    {
        return juce::MidiMessage::getMidiNoteName (proc.baseNote() + semis, true, true, 3);
    }

    void editLane (int col, int y)
    {
        auto d = proc.pattern().get (col);
        const float v = yToValue (y);
        juce::String text;
        switch ((cz::Lane) lane)
        {
            case cz::Lane::Note:
                d.semis = denorm (v, cz::SeqPattern::kMinNote, cz::SeqPattern::kMaxNote); // cromático libre
                text = noteName (d.semis) + (quantizeToScale (proc.currentScale(), d.semis) == d.semis ? "" : "  (OUT OF SCALE)");
                break;
            case cz::Lane::Octave:
                d.octave = denorm (v, cz::SeqPattern::kMinOctave, cz::SeqPattern::kMaxOctave);
                text = (d.octave > 0 ? "+" : "") + juce::String (d.octave) + " OCT";
                break;
            case cz::Lane::Velocity:
                d.velocity = juce::jmax (0.05f, v);
                text = juce::String (juce::roundToInt (d.velocity * 127));
                break;
            case cz::Lane::Ratchet:
                d.ratchet = juce::jlimit (1, cz::SeqPattern::kMaxRatchet, 1 + juce::roundToInt (v * (cz::SeqPattern::kMaxRatchet - 1)));
                text = d.ratchet == 1 ? juce::String ("1 HIT") : juce::String (d.ratchet) + " HITS (RATCHET)";
                break;
            case cz::Lane::Cond:
                d.cond = juce::jlimit (0, (int) cz::Cond::NumConds - 1, juce::roundToInt (v * ((int) cz::Cond::NumConds - 1)));
                text = cz::condName (d.cond);
                break;
            case cz::Lane::Lock:
            {
                d.lock = v * 2.0f - 1.0f;
                if (std::abs (d.lock) < 0.08f) d.lock = 0.0f; // cerca del centro = sin lock
                const auto dest = proc.apvts.getParameter (IDs::lockDest)->getCurrentValueAsText().toUpperCase();
                text = d.lock == 0.0f ? juce::String ("NO LOCK") : (d.lock > 0 ? "+" : "") + juce::String (juce::roundToInt (d.lock * 100)) + " %  " + dest;
                break;
            }
            case cz::Lane::Model:
            {
                // abajo de todo = sin cambio (usa el modelo de la perilla); arriba, los 24 modelos
                d.model = v < 0.06f ? -1 : juce::jlimit (0, cz::kNumCModels - 1, juce::roundToInt ((v - 0.08f) / 0.92f * (float) (cz::kNumCModels - 1)));
                if (d.model < 0) text = mm::lang::tr ("KNOB MODEL");
                else if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (IDs::cModel)))
                    text = juce::String (d.model + 1) + "  " + mm::lang::tr (ch->choices[d.model]).toUpperCase();
                break;
            }
            case cz::Lane::Gate:
                d.gate = juce::jmax (0.05f, v * kMaxGate);
                text = juce::String (d.gate, 2) + " STEPS" + (d.gate > 1.0f ? "  (LONG)" : d.gate < 0.2f ? "  (SHORT)" : "");
                break;
            default:
                d.prob = v;
                text = juce::String (juce::roundToInt (v * 100)) + " %";
                break;
        }
        proc.pattern().set (col, d);
        feedback ("STEP " + juce::String (col + 1) + " " + names[lane], text);
    }

    URingAudioProcessor& proc;
    std::array<juce::TextButton, cz::kNumValueLanes> laneButtons;
    juce::TextButton rndLaneBtn { "RND LANE" }, rndRhythmBtn { "RND RHYTHM" }, rndAllBtn { "RND ALL" },
                     generateBtn { "GENERATE" }, resetBtn { "RESET" };
    juce::TextButton lenMinus { "-" }, lenPlus { "+" }, stepsMinus { "-" }, stepsPlus { "+" };
    SmileyButton smiley;
    juce::Label stepsField, lengthField;
    juce::TextButton fitBtn { "FIT SCALE" };
    std::array<juce::TextButton, URingAudioProcessor::kNumPatterns> patternButtons;
    juce::TextButton chainBtn { "CHAIN" }, copyBtn { "COPY" }, pasteBtn { "PASTE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> chainAttachment;
    juce::Label chainField;
    juce::Rectangle<int> labelCol, content, pagesRow, padsRow, laneArea, lengthArea, stepsArea;
    int lane = 0, lastCol = -1, page = 0;
    bool dragPads = false, paintFlip = false;
};

} // namespace mm::gui
