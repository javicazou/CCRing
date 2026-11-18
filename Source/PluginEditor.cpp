#include "PluginEditor.h"

using namespace mm::gui;

namespace
{
    // Botón de modelo del motor C: color de su banco, nombre arriba y la forma de onda abajo
    struct ModelButton : public juce::TextButton
    {
        ModelButton (const char* name, int m) : juce::TextButton (name), model (m)
        {
            const auto c = mm::gui::cModelColour (m);
            setColour (juce::TextButton::buttonColourId, c.interpolatedWith (juce::Colour (0xff303237), 0.82f));
            setColour (juce::TextButton::buttonOnColourId, c);
            setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        }
        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            auto& lf = getLookAndFeel();
            const bool on = getToggleState();
            lf.drawButtonBackground (g, *this, on ? findColour (juce::TextButton::buttonOnColourId) : findColour (juce::TextButton::buttonColourId), over, down);
            auto r = getLocalBounds().toFloat().reduced (8.0f, 6.0f);
            auto icon = r.removeFromBottom (r.getHeight() * 0.36f).reduced (r.getWidth() * 0.12f, 2.0f);
            g.setColour (findColour (on ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId));
            g.setFont (lf.getTextButtonFont (*this, 26));
            g.drawFittedText (getButtonText(), r.toNearestInt(), juce::Justification::centred, 1);
            mm::gui::drawCModelIcon (g, icon, model, on ? juce::Colours::black.withAlpha (0.7f)
                                                         : mm::gui::cModelColour (model).withAlpha (over ? 1.0f : 0.75f));
        }
        const int model;
    };

    constexpr int pageY = 70;
    constexpr int pageH = URingAudioProcessorEditor::baseH - pageY - 6;
}

URingAudioProcessorEditor::URingAudioProcessorEditor (URingAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      seqGrid (p), keyboard (p.keyboardState)
{
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf); // menús y ventanas emergentes también en Montserrat

    canvas.painter = [this] (juce::Graphics& g)
    {
        g.fillAll (Palette::bg);
        drawModulePanel (g, { 6.0f, 4.0f, (float) baseW - 12.0f, 56.0f }, {});
        // LED del logo: con el secuenciador en marcha titila en cada tiempo
        g.setColour (Palette::accent.withAlpha (0.35f + 0.5f * ledFlash));
        g.fillEllipse (19.0f - 3.0f * ledFlash, 22.0f - 3.0f * ledFlash, 14.0f + 6.0f * ledFlash, 14.0f + 6.0f * ledFlash);
        g.setColour (Palette::accent.interpolatedWith (juce::Colours::white, 0.45f * ledFlash));
        g.fillEllipse (22.0f, 25.0f, 8.0f, 8.0f);
        g.setColour (Palette::text);
        g.setFont (uiFont (20.0f).withExtraKerningFactor (0.25f));
        g.drawText ("URING", 40, 9, 130, 26, juce::Justification::centredLeft);
        g.setColour (Palette::dim.withAlpha (0.8f));
        g.setFont (uiFont (7.5f).withExtraKerningFactor (0.3f));
        g.drawText ("MONSTRI AUDIO", 41, 34, 130, 12, juce::Justification::centredLeft);
        g.setColour (Palette::accent.withAlpha (0.8f));
        g.drawText (juce::String ("v") + URING_VERSION, 120, 34, 40, 12, juce::Justification::centredLeft);
    };
    addAndMakeVisible (canvas);
    for (auto* c : std::initializer_list<juce::Component*> { &synthPage, &fxPage, &seqPage, &livePage, &macroPage, &texturePage, &lcd, &keyboard,
                                                               &synthTab, &fxTab, &seqTab, &liveTab, &macroTab, &textureTab, &prevPreset, &nextPreset, &presetBox, &saveBtn,
                                                               &recBtn, &recLenBox, &miniMeter, &logoHotspot, &undoBtn, &redoBtn, &browseBtn, &cablesBtn })
        canvas.addAndMakeVisible (c);
    addMouseListener (this, true);   // cualquier clic en el plugin = el usuario tocó algo (para deshacer)
    setWantsKeyboardFocus (true);
    undoBtn.setTooltip ("Undo (Cmd+Z)");
    redoBtn.setTooltip ("Redo (Cmd+Shift+Z)");
    browseBtn.setTooltip ("Preset browser: search, categories, favorites");
    undoBtn.onClick = [this] { doUndo (false); };
    redoBtn.onClick = [this] { doUndo (true); };
    browseBtn.onClick = [this] { if (browser.isVisible()) browser.hide(); else browser.show(); };
    cablesBtn.setTooltip ("Cables: shown / faded / hidden (they keep working)");
    cablesBtn.onClick = [this] { setCableView ((synthPage.cableView + 1) % 3, true); };
    browser.onPreview = [this] (const juce::String& category) { playPreview (category); };
    browser.favouritesFile = URingAudioProcessor::userPresetFolder().getParentDirectory().getChildFile ("Favorites.txt");
    browser.getUserPresets = [this] { return proc.getUserPresetNames(); };
    browser.getCurrentName = [this] { return proc.getCurrentPresetName(); };
    browser.onLoadFactory = [this] (int i) { loadPreset (i); };
    browser.onLoadUser = [this] (const juce::String& name)
    {
        quietUntil = juce::Time::getMillisecondCounter() + 500;
        userActed = true;
        if (proc.loadUserPreset (name)) lcd.setText ("USER PRESET", name);
    };
    logoHotspot.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    logoHotspot.setHelpText ("About URing");
    logoHotspot.onClick = [this] { about.setVisible (true); about.toFront (false); };
    miniMeter.setInterceptsMouseClicks (false, false);
    fxPage.addAndMakeVisible (fxAnalyzer);
    livePage.addAndMakeVisible (liveAnalyzer);
    macroPage.addAndMakeVisible (macroAnalyzer);
    texturePage.addAndMakeVisible (texAnalyzer);

    // ---- REC de automatización
    recBtn.setButtonText (juce::String::fromUTF8 ("\xe2\x97\x8f REC"));
    recBtn.setClickingTogglesState (true);
    recBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffff3030));
    recBtn.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    recBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffff5a4a));
    recBtn.setTooltip ("REC: while the song plays, every knob you move is recorded in a loop (LEN). Right-click: recorded knobs / clear");
    recAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::autoRec, recBtn);
    recBtn.onClick = [this]
    {
        lcd.setText ("AUTOMATION REC", recBtn.getToggleState() ? "ARMED - STARTS ON STEP 1" : "OFF");
    };
    recBtn.onMenu = [this] { showAutomationMenu(); };
    recLenBox.addItemList ({ "SEQ", "1 BAR", "2 BARS", "4 BARS", "8 BARS" }, 1);
    recLenBox.setTooltip ("Automation loop: SEQ = one full cycle of SEQ 1 (8, 16, 32, 64 steps...), or 1-8 bars. REC records exactly one loop");
    recLenAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, IDs::autoLen, recLenBox);

    // ================================================================ SYNTH
    {
        Section a { "ENGINE A  /  RESONATOR" }, mix { "MIX" }, b { "ENGINE B  /  IMPACT" }, out { "OUTPUT" },
                flt { "FILTER" }, env { "AMP ENV" }, lfo1 { "LFO 1" }, lfo2 { "LFO 2" };
        auto& sp = synthPage;
        using P = URingAudioProcessor;
        // Entradas de modulación (destinos) y salidas (fuentes, id 100 + n)
        a.light = true;    a.tag = Palette::engineA; a.jacks = { "STRUC", "BRIGHT", "DAMP", "POS" };
        a.jackIds = { P::DAStructure, P::DABright, P::DADamping, P::DAPosition };
        mix.tag = Palette::text.withAlpha (0.7f); mix.jacks = { "A/B", "FEED", "PITCH" };
        mix.jackIds = { P::DABMix, P::DXFeed, P::DPitch };
        b.light = true;    b.tag = Palette::engineB; b.jacks = { "MATER", "DECAY", "TONE", "SPREAD" };
        b.jackIds = { P::DBMaterial, P::DBDecay, P::DBTone, P::DBSpread };
        flt.jacks = { "CUT", "RESO" };
        flt.jackIds = { P::DCutoff, P::DResonance };
        env.jacks = { "ENV" };
        env.jackIds = { 100 + P::SrcEnv };
        lfo1.jacks = { "OUT" };
        lfo1.jackIds = { 100 + P::SrcLfo1 };
        lfo2.jacks = { "OUT" };
        lfo2.jackIds = { 100 + P::SrcLfo2 };
        out.light = true;
        addChoice (sp, a, IDs::aModel, "MODEL");
        addChoice (sp, a, IDs::aExciter, "EXCITER");
        addKnob   (sp, a, IDs::aStructure, "STRUCTURE");
        addKnob   (sp, a, IDs::aBright, "BRIGHTNESS");
        addKnob   (sp, a, IDs::aDamping, "DAMPING");
        addKnob   (sp, a, IDs::aPosition, "POSITION");
        addKnob   (sp, a, IDs::aPitch, "PITCH", true);
        addKnob   (sp, mix, IDs::abMix, "A / B", true);
        addKnob   (sp, mix, IDs::xfeed, "A > B FEED");
        addKnob   (sp, mix, IDs::tune, "TUNE", true);
        addKnob   (sp, mix, IDs::variation, "VARIATION");
        addKnob   (sp, mix, IDs::width, "WIDTH");
        addKnob   (sp, mix, IDs::cMix, "ENGINE C");   // el motor C (24 modelos) también se sube desde acá

        addChoice (sp, b, IDs::bModel, "MODEL");
        addChoice (sp, b, IDs::bExciter, "EXCITER");
        addKnob   (sp, b, IDs::bMaterial, "MATERIAL");
        addKnob   (sp, b, IDs::bDecay, "DECAY");
        addKnob   (sp, b, IDs::bTone, "TONE");
        addKnob   (sp, b, IDs::bPosition, "POSITION");
        addKnob   (sp, b, IDs::bSpread, "SPREAD");
        addKnob   (sp, b, IDs::bSweep, "SWEEP");
        addKnob   (sp, b, IDs::bPitch, "PITCH", true);
        addKnob   (sp, out, IDs::velSens, "VELOCITY");
        addKnob   (sp, out, IDs::volume, "VOLUME");
        addKnob   (sp, out, IDs::body, "BODY");
        addPad    (sp, out, IDs::freeze, "FREEZE", "DRONE");

        addChoice (sp, flt, IDs::fltType, "TYPE");
        addKnob   (sp, flt, IDs::cutoff, "CUTOFF");
        addKnob   (sp, flt, IDs::resonance, "RESO");
        addKnob   (sp, flt, IDs::fltEnv, "ENV AMT", true);
        addKnob   (sp, flt, IDs::fltDecay, "ENV DECAY");
        addKnob   (sp, flt, IDs::keyTrack, "KEYTRK");
        addKnob   (sp, flt, IDs::drive, "DRIVE");
        addKnob   (sp, env, IDs::attack, "ATTACK");
        addKnob   (sp, env, IDs::decay, "DECAY");
        addKnob   (sp, env, IDs::sustain, "SUSTAIN");
        addKnob   (sp, env, IDs::release, "RELEASE");

        for (int i = 0; i < URingAudioProcessor::kNumLfos; ++i)
        {
            auto& s = i == 0 ? lfo1 : lfo2;
            addChoice (sp, s, IDs::lfo (i, "Shape"), "SHAPE");
            addKnob   (sp, s, IDs::lfo (i, "Rate"), "RATE");
            addPad    (sp, s, IDs::lfo (i, "Sync"), "SYNC", "TEMPO");
            addChoice (sp, s, IDs::lfo (i, "Div"), "DIVISION");
            addChoice (sp, s, IDs::lfo (i, "Dest"), "DESTINATION");
            addKnob   (sp, s, IDs::lfo (i, "Depth"), "DEPTH", true);
        }

        sp.rows = { { a, mix, b }, { flt, env, lfo1, lfo2, out } };
        sp.rowTop = { 0, 352 };
        sp.rowHeight = { 330, 330 };
        sp.controlRows = 3;
        // Cables funcionales: se arrastran de una salida (LFO / ENV) a una entrada
        sp.proc = &proc;
        sp.onFeedback = [this] (const juce::String& a, const juce::String& b) { lcd.setText (a, b); };
    }

    // ================================================================ FX
    {
        Section ch { "CHORUS" }, gl { "GLITCH" }, dl { "TAPE DELAY" }, sr { "SPRING REVERB" };
        auto& fp = fxPage;
        ch.jacks = { "IN", "OUT" };
        addKnob   (fp, ch, IDs::chorusMix, "MIX");
        addKnob   (fp, ch, IDs::chorusRate, "RATE");
        addKnob   (fp, ch, IDs::chorusDepth, "DEPTH");
        gl.jacks = { "IN", "OUT" };
        dl.jacks = { "IN", "FB", "OUT" };
        sr.jacks = { "IN", "OUT" };
        addPad    (fp, gl, IDs::glitchOn, "ON", "GLITCH");
        addKnob   (fp, gl, IDs::glitchProb, "CHANCE");
        addChoice (fp, gl, IDs::glitchLen, "LENGTH");
        addKnob   (fp, gl, IDs::glitchVar, "VARIATION");
        addKnob   (fp, gl, IDs::glitchCrush, "CRUSH");
        addKnob   (fp, gl, IDs::glitchMix, "MIX");

        addPad    (fp, dl, IDs::delayOn, "ON", "DELAY");
        addPad    (fp, dl, IDs::delaySync, "SYNC", "TEMPO");
        addChoice (fp, dl, IDs::delayDiv, "DIVISION");
        addKnob   (fp, dl, IDs::delayTime, "TIME");
        addKnob   (fp, dl, IDs::delayFb, "FEEDBACK");
        addKnob   (fp, dl, IDs::delayWow, "WOW");
        addKnob   (fp, dl, IDs::delayDrive, "DRIVE");
        addKnob   (fp, dl, IDs::delayHp, "LOW CUT");
        addKnob   (fp, dl, IDs::delayLp, "HIGH CUT");
        addPad    (fp, dl, IDs::delayPing, "PING", "PONG");
        addKnob   (fp, dl, IDs::delayDuck, "DUCK");
        addKnob   (fp, dl, IDs::delayMix, "MIX");

        addPad    (fp, sr, IDs::springOn, "ON", "SPRING");
        addKnob   (fp, sr, IDs::springPre, "PRE-DELAY");
        addKnob   (fp, sr, IDs::springTension, "TENSION");
        addKnob   (fp, sr, IDs::springDecay, "DECAY");
        addKnob   (fp, sr, IDs::springDamp, "DAMP");
        addKnob   (fp, sr, IDs::springTone, "TONE");
        addKnob   (fp, sr, IDs::springDrip, "DRIP");
        addKnob   (fp, sr, IDs::springMix, "MIX");

        fp.rows = { { ch, gl, dl, sr } };
        fp.rowTop = { 0 };
        fp.rowHeight = { 420 };   // 1.5: módulos más altos, analizador más chico
        fp.controlRows = 3;
        fp.cables = { { 0, 0, "OUT", 0, 1, "IN", juce::Colour (0xff3fd8ff) },
                      { 0, 1, "OUT", 0, 2, "IN", Palette::accent },
                      { 0, 2, "OUT", 0, 3, "IN", juce::Colour (0xffd8d8d8) } };
    }

    // ================================================================ SEQ
    {
        Section sq { "EUCLIDEAN SEQUENCER" }, nt { "NOTES" }, rn { "RANDOM" };
        auto& qp = seqPage;
        sq.tag = Palette::engineA; sq.jacks = { "CLK", "GATE", "CV" };
        nt.jacks = { "CV" };
        rn.jacks = { "S&H", "OUT" };
        addPad    (qp, sq, IDs::seqOn, "RUN", "HOST SYNC");
        addPad    (qp, sq, IDs::midiOut, "MIDI", "MIDI OUT");
        addPad    (qp, sq, IDs::seqFill, "FILL", "FILL");
        addChoice (qp, sq, IDs::seqDiv, "DIVISION");
        addChoice (qp, sq, IDs::seqDir, "DIRECTION");
        addKnob   (qp, sq, IDs::seqLength, "STEPS");
        addKnob   (qp, sq, IDs::seqPulses, "PULSES");
        addKnob   (qp, sq, IDs::seqRotate, "ROTATE");
        addKnob   (qp, sq, IDs::seqSwing, "SWING");
        addKnob   (qp, sq, IDs::tempo, "BPM (NO HOST)");
        addChoice (qp, nt, IDs::seqRoot, "ROOT");
        addChoice (qp, nt, IDs::seqScale, "SCALE");
        addKnob   (qp, nt, IDs::seqOctave, "OCTAVE");
        addKnob   (qp, nt, IDs::seqRange, "RANGE");
        addKnob   (qp, nt, IDs::seqGate, "GATE");
        addKnob   (qp, nt, IDs::seqProb, "PROB");
        addKnob   (qp, rn, IDs::seqDensity, "DENSITY");
        addKnob   (qp, rn, IDs::seqRandom, "AMOUNT");
        addChoice (qp, rn, IDs::seqRndRate, "RATE");
        addKnob   (qp, rn, IDs::seqRndTime, "TIME");
        addKnob   (qp, rn, IDs::seqLongShort, "LONG / SHORT");
        // MARBLES (Mutable Instruments): las notas salen de un bucle aleatorio con memoria
        Section mb { "MARBLES" };
        mb.tag = juce::Colour (0xff8fe3c8);
        addPad    (qp, mb, IDs::seqMarbles, "MARBLES", "ON");
        addKnob   (qp, mb, IDs::seqDejaVu, "DEJA VU");
        addKnob   (qp, mb, IDs::seqMarbLen, "LENGTH");
        addKnob   (qp, mb, IDs::seqSpread, "SPREAD");
        addKnob   (qp, mb, IDs::seqBias, "BIAS");
        // SEQ 2: otro dibujo para el motor B (STEPS / PULSES / ROTATE / DIRECTION y la grilla editan
        // el secuenciador elegido en EDIT)
        Section s2 { "SEQ 2  /  ENGINE B" };
        s2.tag = Palette::engineB;
        {
            auto sel = std::make_unique<SeqSelect>();
            sel->setSelected (proc.getEditSequencer());
            sel->onSelect = [this] (int seq)
            {
                proc.setEditSequencer (seq);
                lcd.setText ("EDITING", seq == 0 ? "SEQ 1  >  ENGINE A" : "SEQ 2  >  ENGINE B");
            };
            seqSelect = sel.get();
            qp.addAndMakeVisible (*sel);
            s2.items.push_back (sel.get());
            controls.push_back (std::move (sel));
        }
        addPad    (qp, s2, IDs::seq2On, "SEQ 2", "ON");
        addChoice (qp, s2, IDs::seq2Div, "DIVISION");
        addKnob   (qp, s2, IDs::seq2Oct, "OCTAVE", true);
        qp.rows = { { sq, nt, rn, mb, s2 } };
        qp.rowTop = { 0 };
        qp.rowHeight = { 262 };
        qp.controlRows = 2;
        qp.cables = { { 0, 2, "OUT", 0, 1, "CV", Palette::accent } };
        qp.addAndMakeVisible (seqGrid);
        seqGrid.onFeedback = [this] (const juce::String& a, const juce::String& b) { lcd.setText (a, b); };
    }

    // ================================================================ LIVE
    {
        Section mc { "MACROS" }, mo { "MORPH  A / B" }, pn { "PIANO  /  LIFE" }, sq { "SEQUENCE" };
        auto& lp = livePage;
        mc.tag = Palette::accent;
        mo.spanLast = pn.spanLast = sq.spanLast = true;
        for (int i = 0; i < 4; ++i)
            addKnob (lp, mc, IDs::macro[i], URingAudioProcessor::macroName (i), true);
        for (int slot = 0; slot < 2; ++slot)
        {
            auto b = std::make_unique<juce::TextButton> (mm::lang::tr (slot == 0 ? "STORE A" : "STORE B"));
            b->setName (slot == 0 ? "STORE A" : "STORE B");
            b->setTooltip (mm::lang::spanish() ? juce::String::fromUTF8 ("Guarda el sonido actual como morph ") + (slot == 0 ? "A" : "B") + juce::String::fromUTF8 (". Despu\xc3\xa9s gir\xc3\xa1 MORPH para pasar de A a B")
                                               : "Saves the current sound as morph " + juce::String (slot == 0 ? "A" : "B") + ". Then turn MORPH to blend A into B");
            b->setColour (juce::TextButton::textColourOffId, slot == 0 ? Palette::accent : juce::Colour (0xff3fd8ff));
            b->onClick = [this, slot]
            {
                proc.storeMorph (slot);
                lcd.setText ("MORPH", juce::String (slot == 0 ? "A" : "B") + " = " + proc.getCurrentPresetName().toUpperCase());
            };
            lp.addAndMakeVisible (*b);
            mo.items.push_back (b.get());
            controls.push_back (std::move (b));
        }
        addKnob   (lp, mo, IDs::morph, "MORPH");
        addPad    (lp, pn, IDs::pedal, "PEDAL", "SUSTAIN");
        addKnob   (lp, pn, IDs::sympathy, "SYMPATHY");
        addKnob   (lp, pn, IDs::life, "LIFE");
        addChoice (lp, sq, IDs::lockDest, "LOCK DEST");
        addChoice (lp, sq, IDs::seq2Follow, "SEQ 2 FOLLOW");
        addKnob   (lp, sq, IDs::seqMutate, "MUTATE");
        lp.rows = { { mc, mo, pn, sq } };
        lp.rowTop = { 0 };
        lp.rowHeight = { 300 };
        // Pad X/Y: X = MORPH (A -> B), Y = MOTION
        xyPad = std::make_unique<mm::gui::XYPad> (proc.apvts, IDs::morph, IDs::macro[1], "MORPH", URingAudioProcessor::macroName (1));
        xyPad->onFeedback = [this] (const juce::String& a, const juce::String& b) { lcd.setText (a, b); };
        lp.addAndMakeVisible (*xyPad);
        lp.controlRows = 2;
        lp.onFeedback = [this] (const juce::String& a, const juce::String& b) { lcd.setText (a, b); };
    }

    // ================================================================ MACRO (motor C: Plaits)
    {
        Section md { "ENGINE C  /  MACRO  -  24 MODELS" }, mc { "MACRO" }, lg { "LOW PASS GATE" }, rt { "ROUTING" };
        auto& mp = macroPage;
        md.tag = Palette::engineC; mc.tag = Palette::engineC; rt.tag = Palette::text.withAlpha (0.7f);
        static const char* shortNames[] { "VA+VCF", "PHASE", "6-OP I", "6-OP II", "6-OP III", "TERRAIN", "STRINGS", "CHIP",
                                          "VA", "WAVESHP", "FM", "GRAIN", "HARMONIC", "WAVETBL", "CHORDS", "SPEECH",
                                          "SWARM", "NOISE", "PARTICLE", "STRING", "MODAL", "KICK", "SNARE", "HI-HAT" };
        modelButtons.resize (24);
        for (int k = 0; k < 24; ++k)
        {
            const int m = (k % 4) * 6 + k / 4;   // la sección llena por columnas: así los modelos quedan en filas de 6
            auto b = std::make_unique<ModelButton> (shortNames[m], m);
            b->setClickingTogglesState (true);
            b->setRadioGroupId (24024);
            b->setTooltip (juce::String (m + 1) + "  " + cz::macroModelName (m)
                           + (m < 8 ? "  -  BANK 1" : m < 16 ? "  -  BANK 2" : "  -  BANK 3"));
            b->onClick = [this, m]
            {
                auto* prm = proc.apvts.getParameter (IDs::cModel);
                prm->setValueNotifyingHost (prm->convertTo0to1 ((float) m));
                // Si el motor C está en silencio (A/B - C en 0), al elegir un modelo se prende a la mitad
                auto* mix = proc.apvts.getParameter (IDs::cMix);
                if (mix->convertFrom0to1 (mix->getValue()) < 0.02f)
                {
                    mix->beginChangeGesture(); mix->setValueNotifyingHost (mix->convertTo0to1 (0.5f)); mix->endChangeGesture();
                    lcd.setText ("ENGINE C ON  (A/B - C 50%)", juce::String (cz::macroModelName (m)).toUpperCase());
                    return;
                }
                lcd.setText ("ENGINE C  MODEL " + juce::String (m + 1), juce::String (cz::macroModelName (m)).toUpperCase());
            };
            mp.addAndMakeVisible (*b);
            md.items.push_back (b.get());
            modelButtons[(size_t) m] = b.get();
            controls.push_back (std::move (b));
        }
        mc.light = true;
        addKnob   (mp, mc, IDs::cHarm, "HARMONICS");
        addKnob   (mp, mc, IDs::cTimbre, "TIMBRE");
        addKnob   (mp, mc, IDs::cMorph, "MORPH");
        addKnob   (mp, mc, IDs::cPitch, "PITCH", true);
        addKnob   (mp, mc, IDs::cFm, "FM", true);
        addKnob   (mp, mc, IDs::cTimbreMod, "TIMBRE MOD", true);
        addKnob   (mp, mc, IDs::cMorphMod, "MORPH MOD", true);
        addChoice (mp, mc, IDs::cOut, "OUTPUT");
        addPad    (mp, lg, IDs::cLpg, "LPG", "ON / DRONE");
        addKnob   (mp, lg, IDs::cDecay, "DECAY");
        addKnob   (mp, lg, IDs::cColour, "COLOUR");
        addKnob   (mp, rt, IDs::cMix, "A/B  -  C");
        addKnob   (mp, rt, IDs::cFeed, "C > A FEED");
        addKnob   (mp, rt, IDs::cFeedB, "C > B FEED");
        addChoice (mp, rt, IDs::seq2Target, "SEQ 2 PLAYS");
        mp.rows = { { md, mc, lg, rt } };
        mp.rowTop = { 0 };
        mp.rowHeight = { 520 };
        mp.controlRows = 4;
        mp.onFeedback = [this] (const juce::String& a, const juce::String& b) { lcd.setText (a, b); };
    }

    // ================================================================ TEXTURE
    {
        Section tx { "TEXTURE  /  GRANULAR" }, pl { "PLATE REVERB" }, ou { "OUTPUT" };
        auto& tp = texturePage;
        tx.tag = juce::Colour (0xff8fe3c8);
        tx.jacks = { "IN", "OUT" };
        pl.jacks = { "IN", "OUT" };
        ou.jacks = { "IN" };
        ou.light = true;
        addPad    (tp, tx, IDs::texOn, "ON", "TEXTURE");
        addChoice (tp, tx, IDs::texMode, "MODE");
        addKnob   (tp, tx, IDs::texPosition, "POSITION");
        addKnob   (tp, tx, IDs::texSize, "SIZE");
        addKnob   (tp, tx, IDs::texPitch, "PITCH", true);
        addKnob   (tp, tx, IDs::texDensity, "DENSITY");
        addKnob   (tp, tx, IDs::texTexture, "TEXTURE");
        addKnob   (tp, tx, IDs::texSpread, "SPREAD");
        addKnob   (tp, tx, IDs::texFeedback, "FEEDBACK");
        addKnob   (tp, tx, IDs::texReverb, "REVERB");
        addPad    (tp, tx, IDs::texFreeze, "FREEZE", "HOLD");
        addKnob   (tp, tx, IDs::texMix, "MIX");
        addPad    (tp, pl, IDs::plateOn, "ON", "PLATE");
        addKnob   (tp, pl, IDs::platePre, "PRE-DELAY");
        addKnob   (tp, pl, IDs::plateSize, "SIZE");
        addKnob   (tp, pl, IDs::plateDecay, "DECAY");
        addKnob   (tp, pl, IDs::plateDamp, "DAMP");
        addKnob   (tp, pl, IDs::plateMod, "MOTION");
        addKnob   (tp, pl, IDs::plateMix, "MIX");
        addKnob   (tp, ou, IDs::eqLow, "LOW", true);
        addKnob   (tp, ou, IDs::eqMid, "MID", true);
        addKnob   (tp, ou, IDs::eqMidHz, "MID FREQ");
        addKnob   (tp, ou, IDs::eqHigh, "HIGH", true);
        addKnob   (tp, ou, IDs::masterDrive, "DRIVE");
        addPad    (tp, ou, IDs::limiterOn, "LIMITER", "-0.3 dB");
        addChoice (tp, ou, IDs::quality, "QUALITY");
        tp.rows = { { tx, pl, ou } };
        tp.rowTop = { 0 };
        tp.rowHeight = { 420 };
        tp.controlRows = 3;
        tp.cables = { { 0, 0, "OUT", 0, 1, "IN", juce::Colour (0xff8fe3c8) },
                      { 0, 1, "OUT", 0, 2, "IN", Palette::accent } };
    }

    // ---- Pestañas
    for (auto* t : { &synthTab, &fxTab, &seqTab, &liveTab, &macroTab, &textureTab })
    {
        t->setClickingTogglesState (true);
        t->setRadioGroupId (777);
        t->setColour (juce::TextButton::buttonOnColourId, Palette::accent);
    }
    macroTab.setColour (juce::TextButton::buttonOnColourId, Palette::engineC);
    macroTab.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    synthTab.onClick = [this] { if (synthTab.getToggleState()) showTab (0); };
    fxTab.onClick    = [this] { if (fxTab.getToggleState()) showTab (1); };
    seqTab.onClick   = [this] { if (seqTab.getToggleState()) showTab (2); };
    liveTab.onClick  = [this] { if (liveTab.getToggleState()) showTab (3); };
    macroTab.onClick = [this] { if (macroTab.getToggleState()) showTab (4); };
    textureTab.onClick = [this] { if (textureTab.getToggleState()) showTab (5); };
    textureTab.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff8fe3c8));
    textureTab.setColour (juce::TextButton::textColourOnId, juce::Colours::black);

    // ---- Presets
    populatePresetMenu();
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id >= 1000)
        {
            const auto name = presetBox.getItemText (presetBox.indexOfItemId (id));
            quietUntil = juce::Time::getMillisecondCounter() + 500;
            if (proc.loadUserPreset (name))
                lcd.setText ("USER PRESET", name);
        }
        else if (id > 0)
        {
            loadPreset (id - 1);
        }
    };
    const int numPresets = (int) presets::factory().size();
    prevPreset.onClick = [this, numPresets] { loadPreset ((juce::jmax (0, proc.getPresetIndex()) - 1 + numPresets) % numPresets); };
    nextPreset.onClick = [this, numPresets] { loadPreset ((juce::jmax (-1, proc.getPresetIndex()) + 1) % numPresets); };
    saveBtn.onClick = [this] { saveUserPresetDialog(); };
    saveBtn.setTooltip ("Save the current sound as a user preset. Factory presets never change; tweaks (*) are lost unless you save");
    for (auto* b : { &prevPreset, &nextPreset, &saveBtn })
        b->setColour (juce::TextButton::textColourOffId, Palette::text);

    // ---- Teclado en pantalla (minimal)
    keyboard.setKeyPressBaseOctave (keyboard.octave);
    keyboard.setVelocity (keyboard.velocity, false);
    keyboard.setWantsKeyboardFocus (true);
    keyboard.onFeedback = [this] (const juce::String& a, const juce::String& b) { lcd.setText (a, b); };
    keyboard.setAvailableRange (24, 108); // el piano no se ve: solo recibe las teclas de la computadora
    keyboard.setLowestVisibleKey (36);
    keyboard.setKeyWidth (26.0f);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffe4e4e2));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff151517));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff8a8a8a));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, Palette::accent.withAlpha (0.15f));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, Palette::accent);
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, Palette::bg);
    keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonArrowColourId, Palette::dim);
    keyboard.setColour (juce::MidiKeyboardComponent::textLabelColourId, juce::Colour (0xff6a6a6a));

    // ---- Layout a tamaño base; el escalado lo hace el transform del canvas
    canvas.setBounds (0, 0, baseW, baseH);
    // Pestañas compactas: el menú de presets gana ancho (los nombres largos se leen enteros)
    synthTab.setBounds (168, 17, 52, 28);
    fxTab.setBounds (222, 17, 34, 28);
    seqTab.setBounds (258, 17, 40, 28);
    liveTab.setBounds (300, 17, 42, 28);
    macroTab.setBounds (344, 17, 62, 28);
    textureTab.setBounds (408, 17, 66, 28);
    browseBtn.setBounds (480, 16, 30, 30);
    prevPreset.setBounds (514, 16, 24, 30);
    presetBox.setBounds (540, 16, 150, 30);
    nextPreset.setBounds (692, 16, 24, 30);
    undoBtn.setBounds (720, 16, 28, 30);
    redoBtn.setBounds (750, 16, 28, 30);
    saveBtn.setBounds (784, 16, 56, 30);
    recBtn.setBounds (846, 16, 58, 30);
    recLenBox.setBounds (908, 16, 66, 30);
    miniMeter.setBounds (980, 12, 22, 40);
    cablesBtn.setBounds (1008, 16, 30, 30);
    lcd.setBounds (1044, 12, baseW - 22 - 1044, 40);
    logoHotspot.setBounds (14, 8, 150, 44);
    canvas.addChildComponent (browser);
    browser.setBounds (16, pageY - 4, baseW - 32, pageH + 4);
    canvas.addChildComponent (about);
    about.setBounds (0, 0, baseW, baseH);
    for (auto* page : { &synthPage, &fxPage, &seqPage, &livePage, &macroPage, &texturePage })
    {
        page->setBounds (16, pageY, baseW - 32, pageH);
        page->layout (page->getWidth());
    }
    seqGrid.setBounds (0, 276, seqPage.getWidth(), seqPage.getHeight() - 280);
    // Zonas de abajo: analizador de salida (FX, MACRO) y pad X/Y + analizador (LIVE)
    fxAnalyzer.setBounds (0, 432, fxPage.getWidth(), fxPage.getHeight() - 436);
    macroAnalyzer.setBounds (0, 532, macroPage.getWidth(), macroPage.getHeight() - 536);
    texAnalyzer.setBounds (0, 432, texturePage.getWidth(), texturePage.getHeight() - 436);
    xyPad->setBounds (0, 312, 700, livePage.getHeight() - 316);   // 1.5: pad X/Y más grande, analizador más chico
    liveAnalyzer.setBounds (708, 312, livePage.getWidth() - 708, livePage.getHeight() - 316);
    keyboard.setBounds (0, 0, 1, 1);

    // Perillas → parámetro, para mostrar en cian lo que está automatizado
    for (auto& c : controls)
        if (auto* k = dynamic_cast<mm::gui::Knob*> (c.get()))
            if (const int idx = proc.indexOf (k->paramId); idx >= 0) knobParams.emplace_back (&k->slider, idx);

    synthTab.setToggleState (true, juce::dontSendNotification);
    showTab (0);
    setCableView (mm::lang::getSetting ("cableView", 0), false);
    retranslateUi();
    startTimerHz (20);

    setResizable (true, true);
    setResizeLimits (baseW / 2, baseH / 2, baseW * 2, baseH * 2);
    getConstrainer()->setFixedAspectRatio ((double) baseW / (double) baseH);
    setSize (baseW * 85 / 100, baseH * 85 / 100);
}

URingAudioProcessorEditor::~URingAudioProcessorEditor()
{
    if (previewNote >= 0) proc.keyboardState.noteOff (1, previewNote, 0.0f);   // que no quede sonando
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

//==============================================================================
void URingAudioProcessorEditor::showTab (int index)
{
    browser.hide();   // tocar una pestaña con la lista de presets abierta va directo a esa página
    synthPage.setVisible (index == 0);
    fxPage.setVisible (index == 1);
    seqPage.setVisible (index == 2);
    livePage.setVisible (index == 3);
    macroPage.setVisible (index == 4);
    texturePage.setVisible (index == 5);
    macroTab.setToggleState (index == 4, juce::dontSendNotification);
    textureTab.setToggleState (index == 5, juce::dontSendNotification);
    synthTab.setToggleState (index == 0, juce::dontSendNotification);
    fxTab.setToggleState (index == 1, juce::dontSendNotification);
    seqTab.setToggleState (index == 2, juce::dontSendNotification);
    liveTab.setToggleState (index == 3, juce::dontSendNotification);
}

void URingAudioProcessorEditor::populatePresetMenu()
{
    presetBox.clear (juce::dontSendNotification);
    const auto& list = presets::factory();
    juce::String lastCategory;
    for (int i = 0; i < (int) list.size(); ++i)
    {
        if (lastCategory != list[(size_t) i].category)
        {
            lastCategory = list[(size_t) i].category;
            presetBox.addSectionHeading (lastCategory.toUpperCase());
        }
        presetBox.addItem (list[(size_t) i].name, i + 1);
    }

    const auto user = proc.getUserPresetNames();
    if (! user.isEmpty())
    {
        presetBox.addSectionHeading ("USER");
        for (int i = 0; i < user.size(); ++i)
            presetBox.addItem (user[i], 1000 + i);
    }

    if (proc.getPresetIndex() >= 0)
        presetBox.setSelectedId (proc.getPresetIndex() + 1, juce::dontSendNotification);
    else if (const int idx = user.indexOf (proc.getUserPresetName()); idx >= 0)
        presetBox.setSelectedId (1000 + idx, juce::dontSendNotification);
}

void URingAudioProcessorEditor::saveUserPresetDialog()
{
    saveDialog = std::make_unique<juce::AlertWindow> (mm::lang::tr ("Save preset"), mm::lang::tr ("Name for your preset:"), juce::MessageBoxIconType::NoIcon);
    saveDialog->addTextEditor ("name", proc.getUserPresetName().isNotEmpty() ? proc.getUserPresetName() : juce::String (mm::lang::tr ("My Sound")));
    saveDialog->addButton (mm::lang::tr ("Save"), 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton (mm::lang::tr ("Cancel"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int result)
    {
        if (result == 1 && saveDialog != nullptr)
        {
            const auto name = saveDialog->getTextEditorContents ("name");
            if (proc.saveUserPreset (name))
            {
                populatePresetMenu();
                const int idx = proc.getUserPresetNames().indexOf (proc.getUserPresetName());
                if (idx >= 0) presetBox.setSelectedId (1000 + idx, juce::dontSendNotification);
                lcd.setText ("PRESET SAVED", proc.getUserPresetName());
            }
            else
            {
                lcd.setText ("PRESET", "COULD NOT SAVE");
            }
        }
        saveDialog.reset();
    }));
}

void URingAudioProcessorEditor::loadPreset (int index)
{
    if (index < 0)
        return;
    quietUntil = juce::Time::getMillisecondCounter() + 500;
    userActed = true;
    proc.loadPreset (index);
    presetBox.setSelectedId (index + 1, juce::dontSendNotification);
    const auto& p = presets::factory()[(size_t) index];
    lcd.setText (juce::String ("PRESET ") + p.category, p.name);
}

void URingAudioProcessorEditor::retranslateUi()
{
    using mm::lang::tr;
    seenLanguage = mm::lang::version().load();
    synthTab.setButtonText (tr ("SYNTH")); fxTab.setButtonText (tr ("FX")); seqTab.setButtonText (tr ("SEQ"));
    liveTab.setButtonText (tr ("LIVE")); macroTab.setButtonText (tr ("ENGINE C")); textureTab.setButtonText (tr ("TEXTURE"));
    macroTab.setTooltip (tr ("Engine C: 24 synthesis models. Raise ENGINE C (SYNTH > MIX) or pick a model to hear it"));
    saveBtn.setButtonText (tr ("SAVE"));
    undoBtn.setTooltip (tr ("Undo (Cmd+Z)"));
    redoBtn.setTooltip (tr ("Redo (Cmd+Shift+Z)"));
    browseBtn.setTooltip (tr ("Preset browser: search, categories, favorites"));
    cablesBtn.setTooltip (tr ("Cables: shown / faded / hidden (they keep working)"));
    logoHotspot.setHelpText (tr ("About URing"));
    recBtn.setTooltip (tr ("REC: while the song plays, every knob you move is recorded in a loop (LEN). Right-click: recorded knobs / clear"));
    recLenBox.setTooltip (tr ("Automation loop: SEQ = one full cycle of SEQ 1 (8, 16, 32, 64 steps...), or 1-8 bars. REC records exactly one loop"));
    saveBtn.setTooltip (tr ("Save the current sound as a user preset. Factory presets never change; tweaks (*) are lost unless you save"));
    {
        const char* lens[] { "SEQ", "1 BAR", "2 BARS", "4 BARS", "8 BARS" };
        for (int i = 0; i < 5; ++i) recLenBox.changeItemText (i + 1, tr (lens[i]));
        recLenBox.setText (recLenBox.getItemText (recLenBox.getSelectedItemIndex()), juce::dontSendNotification);
    }
    for (auto& c : controls)
    {
        if (auto* k = dynamic_cast<mm::gui::Knob*> (c.get()))           k->retranslate();
        else if (auto* ch = dynamic_cast<mm::gui::ChoiceBox*> (c.get())) ch->retranslate();
        else if (auto* pd = dynamic_cast<mm::gui::Pad*> (c.get()))       pd->retranslate();
        else if (auto* tb = dynamic_cast<juce::TextButton*> (c.get()); tb != nullptr && tb->getName().startsWith ("STORE"))
            tb->setButtonText (tr (tb->getName()));
    }
    seqGrid.retranslate();
    browser.retranslate();
    canvas.repaint();
}

void URingAudioProcessorEditor::timerCallback()
{
    if (mm::lang::version().load() != seenLanguage) retranslateUi();

    if (const auto learned = proc.popLearnEvent(); learned.isNotEmpty())
        lcd.setText ("MIDI MAPPED", learned);

    // Si la ventana del plugin está activa, el teclado recupera el foco (p. ej. después de
    // usar un menú) para poder seguir tocando con la computadora. Nunca roba el foco a otra app.
    if (keyboard.isShowing() && ! keyboard.hasKeyboardFocus (false) && ! browser.isVisible())
        if (auto* peer = getPeer(); peer != nullptr && peer->isFocused()
                                    && dynamic_cast<juce::TextEditor*> (juce::Component::getCurrentlyFocusedComponent()) == nullptr)
            keyboard.grabKeyboardFocus();

    // Nombre del preset; con * si se tocó algo desde que se cargó (el preset guardado no cambia hasta SAVE)
    if (proc.isPresetModified())
    {
        const auto shown = proc.getCurrentPresetName() + "  *";
        if (presetBox.getText() != shown) presetBox.setText (shown, juce::dontSendNotification);
    }
    else if (proc.getPresetIndex() >= 0 && presetBox.getSelectedId() != proc.getPresetIndex() + 1)
        presetBox.setSelectedId (proc.getPresetIndex() + 1, juce::dontSendNotification);

    // Botones de modelo del motor C: siguen al parámetro (presets, automatización, morph)
    if (macroPage.isVisible())
    {
        const int cm = juce::jlimit (0, 23, (int) proc.apvts.getRawParameterValue (IDs::cModel)->load());
        if (cm < (int) modelButtons.size() && ! modelButtons[(size_t) cm]->getToggleState())
            modelButtons[(size_t) cm]->setToggleState (true, juce::dontSendNotification);
    }

    // Automatización: perillas en cian (punto = tiene grabación, arco = valor que suena ahora)
    for (auto& [slider, idx] : knobParams)
    {
        const double v = proc.hasAutomation (idx) ? (double) juce::jmax (-0.5f, proc.getAutoDisplay01 (idx)) : -1.0;
        const double old = slider->getProperties().getWithDefault ("auto", -1.0);
        if (std::abs (v - old) > 1.0e-4) { slider->getProperties().set ("auto", v); slider->repaint(); }
    }
    // REC: armado = titila lento (espera el paso 1), grabando = titila rápido; al terminar el ciclo se apaga solo
    ++blink;
    const int rs = proc.isRecordingAutomation() ? 2 : proc.isAutomationArmed() ? 1 : 0;
    if (rs != lastRecState)
    {
        if (rs == 2) lcd.setText (juce::String::fromUTF8 ("\xe2\x97\x8f REC AUTOMATION"), "RECORDING ONE LOOP - MOVE KNOBS");
        if (rs == 0 && lastRecState == 2) lcd.setText ("AUTOMATION", "LOOP RECORDED - REC OFF");
        lastRecState = rs;
    }
    if (rs == 1) recBtn.setAlpha ((blink / 10) % 2 ? 1.0f : 0.4f);
    if (seqSelect != nullptr) seqSelect->setSelected (proc.getEditSequencer());
    if (rs == 2)
    {
        recBtn.setAlpha ((blink / 3) % 2 ? 1.0f : 0.55f);
        if (const int r = proc.lastRecorded.exchange (-1); r >= 0 && r != lastRecShown)
        {
            lastRecShown = r;
            lcd.setText (juce::String::fromUTF8 ("\xe2\x97\x8f REC AUTOMATION"), proc.getParamDisplayName (r));
        }
    }
    else { if (rs == 0) recBtn.setAlpha (1.0f); lastRecShown = -1; }

    // Deshacer: cada ~250 ms, si no se está arrastrando nada, se registra lo que cambió el usuario
    if (++undoCounter % 5 == 0 && ! juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
    {
        proc.undoTick (userActed);
        userActed = false;
        undoBtn.setEnabled (proc.canUndo());
        redoBtn.setEnabled (proc.canRedo());
    }

    // LCD en vivo: preset y forma de onda cuando no hay mensaje
    proc.readScope (lcdWave.data(), (int) lcdWave.size());
    lcd.setIdle (proc.getCurrentPresetName() + (proc.isPresetModified() ? " *" : ""), lcdWave.data(), (int) lcdWave.size());
    if (livePage.isVisible() && xyPad != nullptr) xyPad->refresh();

    // LED del logo al tempo (cada 4 pasos del SEQ 1 = un tiempo en 1/16)
    {
        const int step = proc.sequencer.getCurrentStep();
        const float old = ledFlash;
        if (step >= 0 && step != lastLedStep && step % 4 == 0) ledFlash = 1.0f;
        else ledFlash *= 0.6f;
        if (ledFlash < 0.02f) ledFlash = 0.0f;
        lastLedStep = step;
        if (std::abs (ledFlash - old) > 1.0e-3f) canvas.repaint (12, 14, 30, 30);
    }

    // Luces de las salidas (LFO / ENV) y cables
    if (synthPage.isVisible())
        synthPage.repaint();
}

URingAudioProcessorEditor::SeqSelect::SeqSelect()
{
    for (auto* b : { &one, &two })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (4711);
        addAndMakeVisible (*b);
    }
    one.setColour (juce::TextButton::buttonOnColourId, Palette::accent);
    two.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff3fd8ff));
    two.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    one.onClick = [this] { if (one.getToggleState() && onSelect) onSelect (0); };
    two.onClick = [this] { if (two.getToggleState() && onSelect) onSelect (1); };
    label.setText ("EDIT", juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (mm::gui::uiFont (10.0f));
    label.setColour (juce::Label::textColourId, Palette::dim);
    addAndMakeVisible (label);
}

void URingAudioProcessorEditor::SeqSelect::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromBottom (16));
    const int h = juce::jmin (26, r.getHeight() / 2 - 2);
    one.setBounds (r.removeFromTop (r.getHeight() / 2).withSizeKeepingCentre (r.getWidth(), h));
    two.setBounds (r.withSizeKeepingCentre (r.getWidth(), h));
}

void URingAudioProcessorEditor::SeqSelect::setSelected (int seq)
{
    (seq == 1 ? two : one).setToggleState (true, juce::dontSendNotification);
}

void URingAudioProcessorEditor::showAutomationMenu()
{
    juce::PopupMenu m;
    const auto list = proc.getAutomatedParams();
    m.addSectionHeader (mm::lang::ui ("AUTOMATION  -  PATTERN ") + juce::String::charToString ((juce::juce_wchar) ('A' + proc.getActivePattern())));
    if (list.isEmpty())
        m.addItem (1, mm::lang::tr ("Nothing recorded: press REC and move knobs while it plays"), false, false);
    else
    {
        m.addSectionHeader (mm::lang::tr ("RECORDED  (click = clear)"));
        for (int i : list) m.addItem (100 + i, proc.getParamDisplayName (i));
        m.addSeparator();
        m.addItem (2, mm::lang::tr ("Clear this pattern"));
    }
    m.addItem (3, mm::lang::tr ("Clear all patterns"));
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&recBtn), [this] (int r)
    {
        if (r >= 100) { lcd.setText ("AUTOMATION CLEARED", proc.getParamDisplayName (r - 100)); proc.clearAutomation (r - 100); }
        if (r == 2) { proc.clearAutomation (-1); lcd.setText ("AUTOMATION", "PATTERN CLEARED"); }
        if (r == 3) { proc.clearAllAutomation(); lcd.setText ("AUTOMATION", "ALL CLEARED"); }
    });
}

void URingAudioProcessorEditor::addKnob (Page& page, Section& s, const juce::String& id, const char* title, bool bipolar)
{
    auto k = std::make_unique<Knob> (proc.apvts, id, title, bipolar);
    k->slider.getProperties().set ("light", s.light);
    if (s.light) k->label.setColour (juce::Label::textColourId, juce::Colour (0xff1a1b1e));
    k->slider.onValueChange = [this, id] { showParam (id); };
    k->slider.onRightClick  = [this, id] { showMidiMenu (id); };
    page.addAndMakeVisible (*k);
    s.items.push_back (k.get());
    controls.push_back (std::move (k));
}

void URingAudioProcessorEditor::addChoice (Page& page, Section& s, const juce::String& id, const char* title)
{
    auto c = std::make_unique<ChoiceBox> (proc.apvts, id, title);
    if (s.light) c->label.setColour (juce::Label::textColourId, juce::Colour (0xff1a1b1e));
    c->box.onChange = [this, id] { showParam (id); };
    page.addAndMakeVisible (*c);
    s.items.push_back (c.get());
    controls.push_back (std::move (c));
}

void URingAudioProcessorEditor::addPad (Page& page, Section& s, const juce::String& id, const char* text, const char* title)
{
    auto p = std::make_unique<Pad> (proc.apvts, id, text, title);
    if (s.light) p->label.setColour (juce::Label::textColourId, juce::Colour (0xff1a1b1e));
    p->button.onClick = [this, id] { showParam (id); };
    page.addAndMakeVisible (*p);
    s.items.push_back (p.get());
    controls.push_back (std::move (p));
}

void URingAudioProcessorEditor::showParam (const juce::String& id)
{
    if (juce::Time::getMillisecondCounter() < quietUntil)
        return;
    if (auto* p = proc.apvts.getParameter (id))
    {
        const int cc = proc.getMappedCc (id);
        // En la pantalla, el nombre de la perilla (traducido) en vez del nombre técnico del parámetro
        juce::String name = p->getName (24);
        for (auto& c : controls)
            if (auto* k = dynamic_cast<mm::gui::Knob*> (c.get()); k != nullptr && k->paramId == id) { name = mm::lang::tr (k->getTitle()); break; }
        lcd.setText (name, p->getCurrentValueAsText() + (cc >= 0 ? "  CC " + juce::String (cc) : juce::String()));
    }
}

// Clic derecho en una perilla: MIDI Learn / borrar asignación
void URingAudioProcessorEditor::showMidiMenu (const juce::String& id)
{
    auto* p = proc.apvts.getParameter (id);
    if (p == nullptr)
        return;

    const int cc = proc.getMappedCc (id);
    juce::PopupMenu menu;
    menu.addSectionHeader (p->getName (32));
    menu.addItem (1, mm::lang::tr ("MIDI Learn"));
    menu.addItem (2, cc >= 0 ? mm::lang::tr ("Clear MIDI") + " (CC " + juce::String (cc) + ")" : mm::lang::tr ("Clear MIDI"), cc >= 0);
    menu.addSeparator();
    menu.addItem (3, mm::lang::tr ("Clear all MIDI mappings"));
    const int pIdx = proc.indexOf (id);
    menu.addSeparator();
    menu.addItem (4, mm::lang::tr ("Clear recorded automation"), proc.hasAutomation (pIdx));

    menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [this, id, name = p->getName (24)] (int result)
    {
        if (result == 1)
        {
            proc.startMidiLearn (id);
            lcd.setText ("MIDI LEARN  " + name, "MOVE A CONTROLLER");
        }
        else if (result == 2)
        {
            proc.clearMidiMapping (id);
            lcd.setText ("MIDI CLEARED", name);
        }
        else if (result == 3)
        {
            proc.clearAllMidiMappings();
            lcd.setText ("MIDI", "ALL MAPPINGS CLEARED");
        }
        else if (result == 4)
        {
            proc.clearAutomation (proc.indexOf (id));
            lcd.setText ("AUTOMATION CLEARED", name);
        }
    });
}

void URingAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::bg);
}

void URingAudioProcessorEditor::resized()
{
    canvas.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) baseW));
}

//==============================================================================
bool URingAudioProcessorEditor::QwertyKeyboard::keyPressed (const juce::KeyPress& key)
{
    if (key.getModifiers().isCommandDown())
        return false;   // Cmd+Z / Cmd+F: los maneja el editor (deshacer, buscador)
    const auto c = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());
    if (c == 'z' || c == 'x')
    {
        octave = juce::jlimit (1, 8, octave + (c == 'x' ? 1 : -1));
        kbState.allNotesOff (1); // evita notas colgadas al cambiar de octava
        setKeyPressBaseOctave (octave);
        setLowestVisibleKey (juce::jlimit (24, 72, (octave - 1) * 12));
        if (onFeedback) onFeedback ("KEYBOARD OCTAVE", "C" + juce::String (octave - 2) + " - C" + juce::String (octave - 1));
        return true;
    }
    if (c == 'c' || c == 'v')
    {
        velocity = juce::jlimit (0.1f, 1.0f, velocity + (c == 'v' ? 0.1f : -0.1f));
        setVelocity (velocity, false);
        if (onFeedback) onFeedback ("KEYBOARD VELOCITY", juce::String (juce::roundToInt (velocity * 127)));
        return true;
    }
    return juce::MidiKeyboardComponent::keyPressed (key);
}

//==============================================================================
void URingAudioProcessorEditor::Page::layout (int width)
{
    constexpr int gap = 8, margin = 12;
    const bool tall = controlRows > 1;

    for (size_t r = 0; r < rows.size(); ++r)
    {
        auto& row = rows[r];
        int totalCols = 0;
        for (const auto& s : row)
            totalCols += tall ? ((int) s.items.size() + controlRows - 1) / controlRows : (int) s.items.size();

        const int fixed = gap * ((int) row.size() - 1) + 2 * margin * (int) row.size();
        const float colW = juce::jmin (tall ? 150.0f : 104.0f, (float) (width - fixed) / (float) totalCols);
        const int top = rowTop[r], height = rowHeight[r];
        int x = 0;

        for (auto& s : row)
        {
            const int n = (int) s.items.size();
            const int cols = tall ? (n + controlRows - 1) / controlRows : n;
            const int w = juce::roundToInt (colW * (float) cols) + 2 * margin;
            s.bounds = { x, top, w, height };
            x += w + gap;

            // Zona de controles: debajo del título y (en módulos altos) encima de la fila de jacks
            auto area = s.bounds.reduced (margin, 0).withTrimmedTop (28).withTrimmedBottom (tall ? 62 : 8);
            const int rowsHere = tall ? controlRows : 1;
            const float cw = (float) area.getWidth() / (float) cols;
            const float ch = (float) area.getHeight() / (float) rowsHere;
            for (int i = 0; i < n; ++i)
            {
                const int c = tall ? i / controlRows : i;   // en Eurorack se llena por columnas
                const int rr = tall ? i % controlRows : 0;
                const bool alone = tall && s.spanLast && i == n - 1 && rr == 0;   // solo en su columna: la ocupa entera
                const float hh = alone ? ch * (float) controlRows : ch;
                auto cell = juce::Rectangle<float> ((float) area.getX() + cw * (float) c, (float) area.getY() + ch * (float) rr, cw, hh);
                if (alone) cell = cell.withSizeKeepingCentre (cw, juce::jmin (hh, cw * 1.25f));
                s.items[(size_t) i]->setBounds (cell.reduced (3.0f).toNearestInt());
            }
        }
    }
}

juce::Point<float> URingAudioProcessorEditor::Page::jackCentre (const Section& s, int index) const
{
    const auto b = s.bounds.toFloat();
    if (controlRows > 1) // fila de jacks abajo, repartida a lo ancho
    {
        const float spacing = juce::jmin (52.0f, (b.getWidth() - 40.0f) / (float) juce::jmax (1, s.jacks.size()));
        const float startX = b.getCentreX() - spacing * (float) (s.jacks.size() - 1) * 0.5f;
        return { startX + spacing * (float) index, b.getBottom() - 46.0f };
    }
    return { b.getRight() - 30.0f - (float) index * 46.0f, b.getY() + 13.0f }; // arriba a la derecha
}

void URingAudioProcessorEditor::Page::paint (juce::Graphics& g)
{
    // Rieles del rack arriba y abajo de cada fila
    for (size_t r = 0; r < rows.size(); ++r)
    {
        drawRail (g, { 0.0f, (float) rowTop[r] - 3.0f, (float) getWidth(), 12.0f });
        drawRail (g, { 0.0f, (float) (rowTop[r] + rowHeight[r]) - 9.0f, (float) getWidth(), 12.0f });
    }

    for (const auto& row : rows)
        for (const auto& s : row)
        {
            drawModulePanel (g, s.bounds.toFloat(), mm::lang::tr (s.title), s.light, {}, s.tag);
            for (int j = 0; j < s.jacks.size(); ++j)
            {
                const bool isOut = j < (int) s.jackIds.size() && s.jackIds[(size_t) j] >= 100;
                const auto c = jackCentre (s, j);
                drawJack (g, c, s.jacks[j], s.light, controlRows > 1, isOut);
                if (isOut && proc != nullptr) // LED que sigue a la fuente
                {
                    const float v = proc->getSourceValue (s.jackIds[(size_t) j] - 100);
                    const auto led = sourceColour (s.jackIds[(size_t) j] - 100);
                    g.setColour (led.withAlpha (0.25f + 0.75f * juce::jlimit (0.0f, 1.0f, std::abs (v))));
                    g.fillEllipse (c.x + 10.0f, c.y - 11.0f, 5.0f, 5.0f);
                }
            }

            // Logo serigrafiado al pie de los módulos altos
            if (controlRows > 1)
            {
                g.setColour (s.light ? juce::Colour (0xff3a3b3f) : Palette::dim.withAlpha (0.7f));
                g.setFont (uiFont (8.5f, true).withExtraKerningFactor (0.3f));
                g.drawText ("URING", s.bounds.toFloat().removeFromBottom (18.0f).translated (0.0f, -2.0f),
                            juce::Justification::centred);
            }
        }
}

void URingAudioProcessorEditor::Page::paintOverChildren (juce::Graphics& g)
{
    paintCables (g);
    paintHover (g);
    // Rótulos de jacks y títulos encima de los cables: siempre se leen
    for (const auto& row : rows)
        for (const auto& s : row)
        {
            if (cables.empty() && (proc == nullptr || proc->getPatches().empty())) break;
            drawPanelTitle (g, s.bounds.toFloat(), mm::lang::tr (s.title), s.light, s.tag);
            for (int j = 0; j < s.jacks.size(); ++j)
            {
                const bool isOut = j < (int) s.jackIds.size() && s.jackIds[(size_t) j] >= 100;
                drawJackLabel (g, jackCentre (s, j), s.jacks[j], s.light, controlRows > 1, isOut);
            }
        }
}

void URingAudioProcessorEditor::Page::paintCables (juce::Graphics& g)
{
    auto find = [this] (int row, int sec, const juce::String& label, juce::Point<float>& out)
    {
        if (row >= (int) rows.size() || sec >= (int) rows[(size_t) row].size()) return false;
        const auto& s = rows[(size_t) row][(size_t) sec];
        const int idx = s.jacks.indexOf (label);
        if (idx < 0) return false;
        out = jackCentre (s, idx);
        return true;
    };

    // Vista de cables: transparentes = se ven las perillas de abajo; ocultos = solo el cable en la mano
    const bool hidden = cableView == 2;
    if (cableView == 1) g.beginTransparencyLayer (0.3f);
    for (const auto& c : cables)
    {
        if (hidden) break;
        juce::Point<float> a, b;
        if (find (c.rowA, c.secA, c.jackA, a) && find (c.rowB, c.secB, c.jackB, b))
            drawCable (g, a, b, c.colour, juce::jmin (70.0f, 26.0f + a.getDistanceFrom (b) * 0.08f));
    }

    if (proc == nullptr)
    {
        if (cableView == 1) g.endTransparencyLayer();
        return;
    }

    // Cables de modulación reales
    for (const auto& cab : proc->getPatches())
    {
        if (hidden) break;
        const auto a = jackPositionForId (100 + cab.source), b = jackPositionForId (cab.dest);
        if (a.isOrigin() || b.isOrigin()) continue;
        auto col = sourceColour (cab.source);
        if (cab.amount < 0.0f) col = col.darker (0.5f); // invertido: más oscuro
        drawCable (g, a, b, col, juce::jmin (70.0f, 26.0f + a.getDistanceFrom (b) * 0.08f));
    }
    if (cableView == 1) g.endTransparencyLayer();

    // Cable en la mano (siempre visible, aunque los cables estén ocultos)
    if (dragSource >= 0 || dragDest >= 0)
    {
        const auto from = dragSource >= 0 ? jackPositionForId (100 + dragSource) : jackPositionForId (dragDest);
        drawCable (g, from, dragPos, dragSource >= 0 ? sourceColour (dragSource) : Palette::dim, 30.0f);
    }
}

//==============================================================================
// Patcheo: arrastrar desde una salida (LFO / ENV) hasta una entrada, o tomar un plug para
// desconectarlo. Rueda del mouse = cantidad; clic derecho = cantidad / invertir / quitar.
juce::String URingAudioProcessorEditor::Page::jackName (int jackId)
{
    static const char* sources[] { "LFO 1", "LFO 2", "ENV" };
    static const char* dests[] { "", "PITCH", "A STRUCTURE", "A BRIGHT", "A DAMPING", "A POSITION", "B MATERIAL",
                                 "B DECAY", "B TONE", "B SPREAD", "CUTOFF", "RESO", "A/B MIX", "DELAY TIME",
                                 "GLITCH", "SPRING", "A>B FEED" };
    if (jackId >= 100) return sources[juce::jlimit (0, 2, jackId - 100)];
    return dests[juce::jlimit (0, (int) URingAudioProcessor::NumDests - 1, jackId)];
}

juce::Colour URingAudioProcessorEditor::Page::sourceColour (int source)
{
    switch (source)
    {
        case URingAudioProcessor::SrcLfo1: return Palette::accent;
        case URingAudioProcessor::SrcLfo2: return juce::Colour (0xff3c7dd9);
        default:                           return juce::Colour (0xffd8d8d8);
    }
}

bool URingAudioProcessorEditor::Page::findJack (juce::Point<float> p, int& jackId, juce::Point<float>& centre) const
{
    for (const auto& row : rows)
        for (const auto& s : row)
            for (int j = 0; j < (int) s.jackIds.size(); ++j)
            {
                const auto c = jackCentre (s, j);
                if (c.getDistanceFrom (p) <= 11.0f)
                {
                    jackId = s.jackIds[(size_t) j];
                    centre = c;
                    return true;
                }
            }
    return false;
}

juce::Point<float> URingAudioProcessorEditor::Page::jackPositionForId (int jackId) const
{
    for (const auto& row : rows)
        for (const auto& s : row)
            for (int j = 0; j < (int) s.jackIds.size(); ++j)
                if (s.jackIds[(size_t) j] == jackId)
                    return jackCentre (s, j);
    return {};
}

void URingAudioProcessorEditor::Page::mouseDown (const juce::MouseEvent& e)
{
    if (proc == nullptr) return;
    int id; juce::Point<float> c;
    if (! findJack (e.position, id, c)) return;

    if (id < 100) // entrada
    {
        std::vector<URingAudioProcessor::PatchCable> here;
        for (const auto& cab : proc->getPatches())
            if (cab.dest == id) here.push_back (cab);

        if (e.mods.isPopupMenu())
        {
            if (here.empty()) return;
            juce::PopupMenu menu;
            for (int k = 0; k < (int) here.size(); ++k)
            {
                menu.addSectionHeader (jackName (100 + here[(size_t) k].source) + " > " + jackName (id)
                                       + "  " + juce::String (juce::roundToInt (here[(size_t) k].amount * 100)) + "%");
                static const int amounts[] { 100, 75, 50, 25, 10, -25, -50, -100 };
                for (int a = 0; a < 8; ++a)
                    menu.addItem (k * 100 + a + 1, juce::String (amounts[a]) + " %");
                menu.addItem (k * 100 + 50, mm::lang::tr ("Remove cable"));
            }
            menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [this, here, id] (int r)
            {
                if (r <= 0) return;
                const int k = (r - 1) / 100, opt = r - k * 100;
                if (k >= (int) here.size()) return;
                static const int amounts[] { 100, 75, 50, 25, 10, -25, -50, -100 };
                if (opt == 50) proc->removePatch (here[(size_t) k].source, id);
                else if (opt >= 1 && opt <= 8) proc->setPatchAmount (here[(size_t) k].source, id, amounts[opt - 1] / 100.0f);
                if (onFeedback) onFeedback ("PATCH " + jackName (id), opt == 50 ? "REMOVED" : juce::String (amounts[juce::jlimit (0, 7, opt - 1)]) + " %");
                repaint();
            });
            return;
        }

        if (! here.empty()) // tomar el último cable conectado: se desconecta y queda en la mano
        {
            const auto cab = here.back();
            proc->removePatch (cab.source, id);
            dragSource = cab.source; dragDest = -1; dragAmount = cab.amount;
        }
        else
        {
            dragDest = id; dragSource = -1; dragAmount = 0.5f;
        }
    }
    else
    {
        dragSource = id - 100; dragDest = -1; dragAmount = 0.5f;
    }
    dragPos = e.position;
    repaint();
}

void URingAudioProcessorEditor::Page::mouseDrag (const juce::MouseEvent& e)
{
    if (dragSource < 0 && dragDest < 0) return;
    dragPos = e.position;
    repaint();
}

void URingAudioProcessorEditor::Page::mouseUp (const juce::MouseEvent& e)
{
    if (proc == nullptr || (dragSource < 0 && dragDest < 0)) return;
    int id; juce::Point<float> c;
    if (findJack (e.position, id, c))
    {
        const int src = dragSource >= 0 ? dragSource : (id >= 100 ? id - 100 : -1);
        const int dst = dragDest >= 0 ? dragDest : (id < 100 ? id : -1);
        if (src >= 0 && dst > 0)
        {
            proc->addPatch (src, dst, dragAmount);
            if (onFeedback) onFeedback ("PATCH " + jackName (100 + src) + " > " + jackName (dst),
                                        juce::String (juce::roundToInt (dragAmount * 100)) + " %  (WHEEL = AMOUNT)");
        }
    }
    else if (dragSource >= 0 && onFeedback)
    {
        onFeedback ("PATCH", "CABLE UNPLUGGED");
    }
    dragSource = dragDest = -1;
    repaint();
}

void URingAudioProcessorEditor::Page::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (proc == nullptr) return;
    int id; juce::Point<float> c;
    if (! findJack (e.position, id, c) || id >= 100) return;
    for (const auto& cab : proc->getPatches())
        if (cab.dest == id)
        {
            const float amt = juce::jlimit (-1.0f, 1.0f, cab.amount + (w.deltaY > 0 ? 0.05f : -0.05f));
            proc->setPatchAmount (cab.source, id, amt);
            if (onFeedback) onFeedback (jackName (100 + cab.source) + " > " + jackName (id), juce::String (juce::roundToInt (amt * 100)) + " %");
            repaint();
            return;
        }
}

// Globo sobre el jack: qué tiene conectado y con cuánta cantidad (útil sobre todo con los cables ocultos)
juce::String URingAudioProcessorEditor::Page::hoverText() const
{
    if (hoverJack < 0 || proc == nullptr) return {};
    const auto patches = proc->getPatches();
    juce::StringArray lines;
    if (hoverJack >= 100)
    {
        for (const auto& cab : patches)
            if (cab.source == hoverJack - 100)
                lines.add (juce::String::fromUTF8 ("\xe2\x86\x92 ") + jackName (cab.dest) + "  " + juce::String (juce::roundToInt (cab.amount * 100)) + "%");
        lines.insert (0, jackName (hoverJack) + (lines.isEmpty() ? "  -  " + juce::String (mm::lang::tr ("NOT PATCHED")) : juce::String()));
    }
    else
    {
        for (const auto& cab : patches)
            if (cab.dest == hoverJack)
                lines.add (jackName (100 + cab.source) + juce::String::fromUTF8 (" \xe2\x86\x92  ") + juce::String (juce::roundToInt (cab.amount * 100)) + "%");
        lines.insert (0, jackName (hoverJack) + (lines.isEmpty() ? "  -  " + juce::String (mm::lang::tr ("NOT PATCHED")) : juce::String()));
    }
    return lines.joinIntoString ("\n");
}

void URingAudioProcessorEditor::Page::paintHover (juce::Graphics& g)
{
    if (dragSource >= 0 || dragDest >= 0) return;
    const auto text = hoverText();
    if (text.isEmpty()) return;
    const auto lines = juce::StringArray::fromLines (text);
    const auto font = uiFont (11.0f);
    float w = 0.0f;
    for (const auto& l : lines) w = juce::jmax (w, juce::GlyphArrangement::getStringWidth (font, l));
    const float h = 8.0f + 15.0f * (float) lines.size();
    auto box = juce::Rectangle<float> (w + 20.0f, h).withCentre (hoverPos).translated (0.0f, -h * 0.5f - 18.0f);
    box = box.constrainedWithin (getLocalBounds().toFloat().reduced (4.0f));
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (box.translated (0.0f, 2.0f), 5.0f);
    g.setColour (Palette::lcdBg.withAlpha (0.96f));
    g.fillRoundedRectangle (box, 5.0f);
    g.setColour ((hoverJack >= 100 ? sourceColour (hoverJack - 100) : Palette::accent).withAlpha (0.8f));
    g.drawRoundedRectangle (box, 5.0f, 1.2f);
    g.setFont (font);
    for (int i = 0; i < lines.size(); ++i)
    {
        g.setColour (i == 0 ? Palette::text : Palette::dim.brighter (0.4f));
        g.drawText (lines[i], box.reduced (10.0f, 4.0f).withHeight (15.0f).translated (0.0f, 15.0f * (float) i), juce::Justification::centredLeft);
    }
}

void URingAudioProcessorEditor::Page::mouseMove (const juce::MouseEvent& e)
{
    if (proc == nullptr) return;
    int id = -1; juce::Point<float> c;
    if (! findJack (e.position, id, c)) id = -1;
    if (id != hoverJack) { hoverJack = id; hoverPos = c; repaint(); }
}

void URingAudioProcessorEditor::Page::mouseExit (const juce::MouseEvent&)
{
    if (hoverJack >= 0) { hoverJack = -1; repaint(); }
}

void URingAudioProcessorEditor::setCableView (int view, bool announce)
{
    view = juce::jlimit (0, 2, view);
    for (auto* page : { &synthPage, &fxPage, &seqPage, &livePage, &macroPage, &texturePage })
    {
        page->cableView = view;
        page->repaint();
    }
    cablesBtn.level = view;
    cablesBtn.repaint();
    mm::lang::setSetting ("cableView", view);
    if (announce)
    {
        static const char* names[] { "CABLES: SHOWN", "CABLES: FADED", "CABLES: HIDDEN" };
        lcd.setText ("CABLES", mm::lang::tr (names[view]));
    }
}

// ESCUCHAR en el buscador: una nota corta de cada preset que se elige (graves más abajo)
void URingAudioProcessorEditor::playPreview (const juce::String& category)
{
    if (previewNote >= 0) proc.keyboardState.noteOff (1, previewNote, 0.0f);
    const auto c = category.toUpperCase();
    previewNote = c == "BASS" ? 36 : (c == "PAD" || c == "DRONE") ? 48 : 60;
    proc.keyboardState.noteOn (1, previewNote, 0.8f);
    const int id = ++previewId;
    juce::Component::SafePointer<URingAudioProcessorEditor> safe (this);
    juce::Timer::callAfterDelay (c == "PAD" || c == "DRONE" ? 1400 : 700, [safe, id]
    {
        if (safe == nullptr || safe->previewId != id || safe->previewNote < 0) return;
        safe->proc.keyboardState.noteOff (1, safe->previewNote, 0.0f);
        safe->previewNote = -1;
    });
}

//==============================================================================
void URingAudioProcessorEditor::doUndo (bool redo)
{
    quietUntil = juce::Time::getMillisecondCounter() + 400;
    const bool ok = redo ? proc.redo() : proc.undo();
    lcd.setText (redo ? "REDO" : "UNDO", ok ? juce::String (redo ? "CHANGE RESTORED" : "LAST CHANGE UNDONE") : juce::String ("NOTHING TO " + juce::String (redo ? "REDO" : "UNDO")));
    undoBtn.setEnabled (proc.canUndo());
    redoBtn.setEnabled (proc.canRedo());
    userActed = false;
}

bool URingAudioProcessorEditor::keyPressed (const juce::KeyPress& k)
{
    if (k.getModifiers().isCommandDown() && (k.getKeyCode() == 'Z' || k.getKeyCode() == 'z'))
    {
        doUndo (k.getModifiers().isShiftDown());
        return true;
    }
    if (k.getModifiers().isCommandDown() && (k.getKeyCode() == 'F' || k.getKeyCode() == 'f'))
    {
        browser.show();
        return true;
    }
    return false;
}
