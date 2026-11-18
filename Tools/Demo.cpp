// URingDemo: renderiza un WAV de demostración (varios presets tocando una frase)
// y capturas PNG de las pestañas SYNTH y FX, sin instalar el plugin.
//   URingDemo <carpeta-salida>
#include <juce_audio_utils/juce_audio_utils.h>
#include <map>
#include <set>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"
#include "Tutorial.h"
#include "Promo.h"

namespace
{
struct DemoPlayHead : juce::AudioPlayHead
{
    double ppq = 0.0, bpm = 110.0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setIsPlaying (true);
        p.setBpm (bpm);
        p.setPpqPosition (ppq);
        return p;
    }
};

int findPreset (const char* name)
{
    const auto& list = presets::factory();
    for (int i = 0; i < (int) list.size(); ++i)
        if (juce::String (list[(size_t) i].name) == name)
            return i;
    return 0;
}

void writePng (juce::Component& c, const juce::File& f)
{
    f.deleteFile();
    juce::FileOutputStream os (f);
    const float scale = std::getenv ("URING_SHOT_SCALE") != nullptr ? (float) std::atof (std::getenv ("URING_SHOT_SCALE")) : 1.0f;   // capturas en alta: URING_SHOT_SCALE=2
    juce::PNGImageFormat().writeImageToStream (c.createComponentSnapshot (c.getLocalBounds(), true, scale), os);
    std::cout << "GUI: " << f.getFullPathName() << std::endl;
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    const juce::File outDir = argc > 1 ? juce::File (juce::String (argv[1]))
                                       : juce::File::getCurrentWorkingDirectory();
    outDir.createDirectory();

    URingAudioProcessor proc;
    // Idioma de las capturas sin tocar la preferencia guardada: --en / --es al final de la línea de comandos
    for (int i = 1; i < argc; ++i)
    {
        if (juce::String (argv[i]) == "--es") mm::lang::currentRef() = mm::lang::Lang::Spanish;
        if (juce::String (argv[i]) == "--en") mm::lang::currentRef() = mm::lang::Lang::English;
    }
    if (argc > 2 && juce::String (argv[2]) == "stuck") // notas colgadas al cambiar SEQ 2 / FOLLOW en vivo
    {
        DemoPlayHead ph; ph.bpm = 120.0;
        proc.setPlayHead (&ph);
        proc.setPlayConfigDetails (0, 2, 48000.0, 256);
        auto setP = [&] (const char* id, float v) { auto* prm = proc.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };
        auto run = [&] (const char* label, std::function<void (int)> atBlock)
        {
            proc.loadPreset (findPreset ("Fifths Machine"));
            setP (IDs::midiOut, 1.0f); setP (IDs::seqGate, 0.98f); setP (IDs::seqLongShort, 1.0f);
            proc.prepareToPlay (48000.0, 256);
            ph.ppq = 0.0;
            std::map<std::pair<int, int>, int> held; // (canal, nota) -> notas sonando
            juce::AudioBuffer<float> b (2, 256);
            for (int n = 0; n < 1500; ++n)
            {
                atBlock (n);
                juce::MidiBuffer m; proc.processBlock (b, m);
                ph.ppq += 256 * ph.bpm / (60.0 * 48000.0);
                for (const auto ev : m)
                {
                    const auto mm2 = ev.getMessage();
                    if (mm2.isNoteOn()) ++held[{ mm2.getChannel(), mm2.getNoteNumber() }];
                    else if (mm2.isNoteOff()) { auto& h = held[{ mm2.getChannel(), mm2.getNoteNumber() }]; if (h > 0) --h; }
                }
            }
            setP (IDs::seqOn, 0.0f); // al parar todo tiene que soltarse
            for (int n = 0; n < 4; ++n) { juce::MidiBuffer m; proc.processBlock (b, m);
                for (const auto ev : m) { const auto mm2 = ev.getMessage(); if (mm2.isNoteOff()) { auto& h = held[{ mm2.getChannel(), mm2.getNoteNumber() }]; if (h > 0) --h; } } }
            int stuck = 0; juce::String which;
            for (auto& [k, v] : held) if (v > 0) { stuck += v; which << "ch" << k.first << ":" << k.second << " "; }
            std::cout << "STUCK " << juce::String (label).paddedRight (' ', 34) << stuck << " notas colgadas " << which << std::endl;
        };
        run ("normal", [] (int) {});
        run ("SEQ 2 on/off cada 200 bloques", [&] (int n) { if (n % 200 == 100) setP (IDs::seq2On, (n / 200) % 2 ? 1.0f : 0.0f); });
        run ("FOLLOW cambia cada 150 bloques", [&] (int n) { if (n % 150 == 75) setP (IDs::seq2Follow, (float) ((n / 150) % 4 == 3 ? 0 : (n / 150) % 8)); });
        run ("patron A->B->C en vivo", [&] (int n) { if (n % 300 == 150) proc.selectPattern ((n / 300) % 3); });
        return 0;
    }
    if (argc > 2 && juce::String (argv[2]) == "host") // lo que hace un DAW: guardar/abrir, proyecto viejo, frecuencias, buffers, instancias
    {
        auto setP = [] (URingAudioProcessor& p, const char* id, float v) { auto* prm = p.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };
        auto render = [] (URingAudioProcessor& p, double sr, int block, double secs, double bpm = 122.0)
        {
            DemoPlayHead ph; ph.bpm = bpm;
            p.setPlayHead (&ph);
            p.setPlayConfigDetails (0, 2, sr, block);
            p.prepareToPlay (sr, block);
            juce::AudioBuffer<float> b (2, block);
            double pk = 0, s2 = 0; long n = 0; bool bad = false;
            for (int pos = 0; pos < (int) (secs * sr); pos += block)
            {
                juce::MidiBuffer m; p.processBlock (b, m);
                ph.ppq += block * bpm / (60.0 * sr);
                for (int c = 0; c < 2; ++c) for (int i = 0; i < block; ++i)
                { const float v = b.getSample (c, i); if (! std::isfinite (v)) bad = true; pk = juce::jmax (pk, (double) std::abs (v)); s2 += v * v; ++n; }
            }
            p.setPlayHead (nullptr);
            return std::tuple<double, double, bool> { 20 * std::log10 (juce::jmax (1e-9, pk)), 10 * std::log10 (juce::jmax (1e-18, s2 / (double) n)), bad };
        };
        auto stateXml = [] (URingAudioProcessor& p) { juce::MemoryBlock mb; p.getStateInformation (mb); return juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize()); };

        // 1. Guardar el proyecto y volver a abrirlo (otra instancia)
        proc.loadPreset (findPreset ("Kick Into Modal")); { auto* so = proc.apvts.getParameter (IDs::seqOn); so->setValueNotifyingHost (1.0f); }
        setP (proc, IDs::cTimbre, 0.77f); setP (proc, IDs::cutoff, 3210.0f);
        proc.addPatch (URingAudioProcessor::SrcLfo2, URingAudioProcessor::DCMorph, 0.4f);
        { auto d = proc.pattern().get (5); d.semis = 7; d.ratchet = 2; proc.pattern().set (5, d); }
        proc.storeMorph (0);
        juce::MemoryBlock saved; proc.getStateInformation (saved);
        URingAudioProcessor second;
        second.setStateInformation (saved.getData(), (int) saved.getSize());
        auto a = stateXml (proc), b = stateXml (second);
        const bool same = a != nullptr && b != nullptr && a->isEquivalentTo (b.get(), true);
        std::cout << "TEST project save/reopen: state identical = " << (same ? "yes" : "NO")
                  << "  (C timbre " << second.apvts.getParameter (IDs::cTimbre)->getValue() << ", cables " << second.getPatches().size()
                  << ", step 6 note " << second.pattern().get (5).semis << " x" << second.pattern().get (5).ratchet << ")" << std::endl;
        if (! same && a && b)
        {
            for (auto* e : a->getChildIterator()) {}
            for (int i = 0; i < a->getNumAttributes(); ++i)
                if (a->getAttributeValue (i) != b->getStringAttribute (a->getAttributeName (i)))
                    std::cout << "   diff attr " << a->getAttributeName (i) << std::endl;
        }

        // 2. Proyecto viejo (antes del Motor C): sin parámetros C -> el C queda apagado
        {
            auto x = stateXml (proc);
            juce::Array<juce::XmlElement*> drop;
            for (auto* e : x->getChildIterator())
                if (e->getStringAttribute ("id").startsWith ("c") && e->getStringAttribute ("id") != "cutoff" && e->getStringAttribute ("id") != "chorusMix"
                    && ! e->getStringAttribute ("id").startsWith ("chorus"))
                    drop.add (e);
            drop.add (x->getChildByAttribute ("id", IDs::seq2Target));
            for (auto* e : drop) if (e != nullptr) x->removeChildElement (e, true);
            juce::MemoryBlock mb; juce::AudioProcessor::copyXmlToBinary (*x, mb);
            URingAudioProcessor old;
            old.setStateInformation (mb.getData(), (int) mb.getSize());
            auto get = [&] (const char* id) { return old.apvts.getParameter (id)->convertFrom0to1 (old.apvts.getParameter (id)->getValue()); };
            std::cout << "TEST old project (no engine C params, removed " << drop.size() << "): C mix " << get (IDs::cMix) << ", C feed " << get (IDs::cFeed)
                      << ", seq2 target " << get (IDs::seq2Target) << " (expect 0 0 0), cutoff " << juce::roundToInt (get (IDs::cutoff)) << " (expect 3210)" << std::endl;
        }

        // 3. Frecuencias de muestreo y tamaños de buffer
        for (auto [sr, block] : { std::pair<double, int> { 44100.0, 512 }, { 48000.0, 64 }, { 88200.0, 1024 }, { 96000.0, 333 }, { 192000.0, 2048 }, { 44100.0, 1 } })
        {
            proc.loadPreset (findPreset ("Dub Chord Stab")); { auto* so = proc.apvts.getParameter (IDs::seqOn); so->setValueNotifyingHost (1.0f); }
            auto [pk, rms, bad] = render (proc, sr, block, block == 1 ? 1.0 : 4.0);
            std::cout << "TEST " << juce::String (sr / 1000.0, 1) << " kHz / buffer " << block << ": peak " << juce::String (pk, 1)
                      << " dB, rms " << juce::String (rms, 1) << " dB" << (bad ? "  ** NaN/Inf **" : "") << std::endl;
        }
        // afinación del motor C a 96 kHz (VA, C4)
        {
            proc.loadPreset (0); { auto* so = proc.apvts.getParameter (IDs::seqOn); so->setValueNotifyingHost (1.0f); }
            setP (proc, IDs::seqOn, 0.0f); setP (proc, IDs::cModel, 8.0f); setP (proc, IDs::cMix, 1.0f); setP (proc, IDs::cLpg, 0.0f);
            for (auto* id : { IDs::delayOn, IDs::springOn, IDs::chorusMix, IDs::variation, IDs::life }) setP (proc, id, 0.0f);
            setP (proc, "lfo1Depth", 0.0f); setP (proc, "lfo2Depth", 0.0f); proc.clearPatches(); setP (proc, IDs::cutoff, 20000.0f);
            for (double sr : { 44100.0, 96000.0 })
            {
                proc.setPlayConfigDetails (0, 2, sr, 512); proc.prepareToPlay (sr, 512);
                juce::AudioBuffer<float> b (2, 512); std::vector<float> x;
                for (int pos = 0; pos < (int) (0.5 * sr); pos += 512)
                { juce::MidiBuffer m; if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0); proc.processBlock (b, m); for (int i = 0; i < 512; ++i) x.push_back (b.getSample (0, i)); }
                int zc = 0, first = -1, last = -1;
                for (int i = (int) (0.1 * sr); i < (int) x.size() - 1; ++i) if (x[(size_t) i] < 0 && x[(size_t) i + 1] >= 0) { if (first < 0) first = i; last = i; ++zc; }
                const double f = (zc - 1) * sr / (double) (last - first);
                std::cout << "TEST engine C pitch at " << juce::String (sr / 1000.0, 1) << " kHz: C4 = " << juce::String (f, 2) << " Hz (expect 261.63, "
                          << juce::String (1200.0 * std::log2 (f / 261.6256), 1) << " cents)" << std::endl;
            }
        }
        // 4. Varias instancias a la vez (como 4 pistas de Ableton)
        {
            std::vector<std::unique_ptr<URingAudioProcessor>> inst;
            const char* names[] { "Rubber Blip", "Deep Kick 7/16", "Formant Drone", "Kick Into Modal" };
            for (auto* n : names) { inst.push_back (std::make_unique<URingAudioProcessor>()); inst.back()->loadPreset (findPreset (n)); inst.back()->apvts.getParameter (IDs::seqOn)->setValueNotifyingHost (1.0f); }
            bool bad = false;
            DemoPlayHead ph;
            for (auto& p : inst) { p->setPlayHead (&ph); p->setPlayConfigDetails (0, 2, 48000.0, 256); p->prepareToPlay (48000.0, 256); }
            juce::AudioBuffer<float> b (2, 256);
            const auto t0 = juce::Time::getHighResolutionTicks();
            for (int n = 0; n < (int) (10.0 * 48000 / 256); ++n)
            {
                for (auto& p : inst) { juce::MidiBuffer m; p->processBlock (b, m); for (int i = 0; i < 256; ++i) if (! std::isfinite (b.getSample (0, i))) bad = true; }
                ph.ppq += 256 * ph.bpm / (60.0 * 48000.0);
            }
            const double secs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
            std::cout << "TEST 4 instances, 10 s: " << (bad ? "NaN!" : "ok") << ", CPU " << juce::String (100.0 * secs / 10.0, 1) << "% of one core" << std::endl;
            for (auto& p : inst) p->setPlayHead (nullptr);
        }
        // 5. Exportar dos veces (render offline): mismo resultado
        {
            auto bounce = [&] (const char* preset)
            {
                URingAudioProcessor p; p.loadPreset (findPreset (preset)); { auto* so = p.apvts.getParameter (IDs::seqOn); so->setValueNotifyingHost (1.0f); }
                DemoPlayHead ph; ph.bpm = 124.0; p.setPlayHead (&ph); p.setNonRealtime (true);
                p.setPlayConfigDetails (0, 2, 48000.0, 512); p.prepareToPlay (48000.0, 512);
                juce::AudioBuffer<float> b (2, 512); std::vector<float> out; std::vector<int> notes;
                for (int n = 0; n < 400; ++n)
                {
                    juce::MidiBuffer m; p.processBlock (b, m); ph.ppq += 512 * 124.0 / (60.0 * 48000.0);
                    for (int i = 0; i < 512; ++i) out.push_back (b.getSample (0, i));
                }
                p.setPlayHead (nullptr);
                return out;
            };
            for (auto* pr : { "Click Bass", "Resonant Arp", "Init Ring" })
            {
                const auto r1 = bounce (pr), r2 = bounce (pr);
                double md = 0, pk = 1e-9; int firstDiff = -1;
                for (size_t i = 0; i < r1.size(); ++i) { const double d = std::abs (r1[i] - r2[i]); if (d > 1e-6 && firstDiff < 0) firstDiff = (int) i; md = juce::jmax (md, d); pk = juce::jmax (pk, (double) std::abs (r1[i])); }
                std::cout << "TEST offline bounce twice (" << pr << "): max difference " << juce::String (20 * std::log10 (juce::jmax (1e-12, md / pk)), 1)
                          << " dB rel. peak, first at " << juce::String (firstDiff / 48.0, 1) << " ms" << std::endl;
            }
        }
        return 0;
    }
    if (argc > 2 && juce::String (argv[2]) == "undo") // deshacer / rehacer
    {
        DemoPlayHead ph; ph.bpm = 122.0;
        proc.setPlayHead (&ph);
        proc.setPlayConfigDetails (0, 2, 48000.0, 512);
        proc.prepareToPlay (48000.0, 512);
        auto setP = [&] (const char* id, float v) { auto* prm = proc.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };
        auto get = [&] (const char* id) { return proc.apvts.getParameter (id)->convertFrom0to1 (proc.apvts.getParameter (id)->getValue()); };
        proc.loadPreset (findPreset ("Resonant Arp"));
        proc.undoTick (false);
        const float cut0 = get (IDs::cutoff);
        const int note0 = proc.pattern().get (3).semis;
        setP (IDs::cutoff, 1234.0f);                                   proc.undoTick (true);
        { auto d = proc.pattern().get (3); d.semis = note0 + 5; proc.pattern().set (3, d); } proc.undoTick (true);
        proc.addPatch (URingAudioProcessor::SrcLfo2, URingAudioProcessor::DCutoff, 0.7f); proc.undoTick (true);
        const auto patches3 = proc.getPatches().size();
        proc.undo(); const auto pAfter1 = proc.getPatches().size();
        proc.undo(); const int noteAfter2 = proc.pattern().get (3).semis;
        proc.undo(); const float cutAfter3 = get (IDs::cutoff);
        proc.redo(); const float cutRedo = get (IDs::cutoff);
        std::cout << "TEST undo: cables " << (int) patches3 << " -> " << (int) pAfter1 << " | step 4 note " << note0 + 5 << " -> " << noteAfter2
                  << " (expect " << note0 << ") | cutoff 1234 -> " << juce::roundToInt (cutAfter3) << " (expect " << juce::roundToInt (cut0)
                  << ") | redo -> " << juce::roundToInt (cutRedo) << " (expect 1234)" << std::endl;
        // MUTATE sonando: no crea pasos de deshacer
        proc.undo(); proc.undo(); proc.undo();   // vaciar hacia atrás
        while (proc.canUndo()) proc.undo();
        setP (IDs::seqMutate, 1.0f); setP (IDs::seqOn, 1.0f); proc.undoTick (true);   // 1 paso del usuario
        int before = 0; { auto tmp = proc.canUndo(); before = tmp ? 1 : 0; }
        juce::AudioBuffer<float> b (2, 512);
        int changes = 0; auto pat0 = proc.pattern().get (0).semis;
        for (int n = 0; n < 2000; ++n)
        {
            juce::MidiBuffer m; proc.processBlock (b, m); ph.ppq += 512 * ph.bpm / (60.0 * 48000.0);
            if (n % 10 == 0) proc.undoTick (false);
            if (proc.pattern().get (0).semis != pat0) { ++changes; pat0 = proc.pattern().get (0).semis; }
        }
        int steps = 0; while (proc.canUndo()) { proc.undo(); ++steps; }
        std::cout << "TEST undo + MUTATE: pattern changed " << changes << " times while playing, undo steps = " << steps
                  << " (expect 1, the MUTATE knob; had " << before << ")" << std::endl;
        return 0;
    }
    if (argc > 2 && juce::String (argv[2]) == "look") // capturas de todas las pestañas con el plugin sonando
    {
        constexpr double tsr = 48000.0;
        DemoPlayHead ph; ph.bpm = 122.0;
        proc.setPlayHead (&ph);
        proc.setPlayConfigDetails (0, 2, tsr, 512);
        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        editor->setSize (URingAudioProcessorEditor::baseW, URingAudioProcessorEditor::baseH);
        auto* ed = dynamic_cast<URingAudioProcessorEditor*> (editor.get());
        auto runFor = [&] (double secs)
        {
            juce::AudioBuffer<float> b (2, 512);
            const int blocks = (int) (secs * tsr / 512);
            for (int n = 0; n < blocks; ++n)
            {
                juce::MidiBuffer m; proc.processBlock (b, m);
                ph.ppq += 512 * ph.bpm / (60.0 * tsr);
                if (n % 4 == 3) juce::MessageManager::getInstance()->runDispatchLoopUntil (40);   // ~tiempo real
            }
        };
        struct Shot { int tab; const char* preset; const char* file; };
        for (auto sh : { Shot { 0, "Mbira Ghost", "look_synth.png" }, Shot { 1, "Mutating Plonks", "look_fx.png" },
                         Shot { 2, "Resonant Arp", "look_seq.png" }, Shot { 3, "Fifths Machine", "look_live.png" },
                         Shot { 4, "Dub Chord Stab", "look_macro.png" }, Shot { 5, "Glass Machine", "look_texture.png" } })
        {
            proc.loadPreset (findPreset (sh.preset));
            auto* prm = proc.apvts.getParameter (IDs::seqOn); prm->setValueNotifyingHost (1.0f);
            if (sh.tab == 5)
                for (auto* id : { IDs::texOn, IDs::plateOn })
                    proc.apvts.getParameter (id)->setValueNotifyingHost (1.0f);
            if (sh.tab == 0) { proc.clearPatches(); proc.addPatch (URingAudioProcessor::SrcLfo1, URingAudioProcessor::DAPosition, 0.5f); }
            proc.prepareToPlay (tsr, 512); ph.ppq = 0.0;
            ed->showTab (sh.tab);
            runFor (2.5);
            writePng (*editor, outDir.getChildFile (sh.file));
        }
        ed->openPresetBrowser(); juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
        writePng (*editor, outDir.getChildFile ("look_browser.png"));
        ed->closePresetBrowser();
        ed->showAbout (true); juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        writePng (*editor, outDir.getChildFile ("look_about.png"));
        return 0;
    }
    if (argc > 2 && juce::String (argv[2]) == "dice") // RANDOM (dado): niveles, que nada se rompa, deshacer, audio de ejemplo
    {
        constexpr double tsr = 48000.0;
        constexpr int bs = 512;
        proc.setPlayConfigDetails (0, 2, tsr, bs);
        int fails = 0;
        auto check = [&] (bool ok, const juce::String& what) { std::cout << (ok ? "OK   " : "FAIL ") << what << std::endl; if (! ok) ++fails; };
        const int notes[] { 48, 55, 60, 63, 67 };
        auto render = [&] (juce::AudioBuffer<float>& out)
        {
            proc.prepareToPlay (tsr, bs);
            juce::AudioBuffer<float> b (2, bs);
            out.setSize (2, (int) (4.0 * tsr)); out.clear();
            for (int pos = 0; pos + bs <= out.getNumSamples(); pos += bs)
            {
                juce::MidiBuffer m;
                for (int k = 0; k < 5; ++k)
                {
                    const int on = (int) ((k < 3 ? k * 0.35 : 1.2) * tsr), off = on + (int) ((k < 3 ? 0.3 : 2.0) * tsr);
                    if (on >= pos && on < pos + bs) m.addEvent (juce::MidiMessage::noteOn (1, notes[k], 0.8f), on - pos);
                    if (off >= pos && off < pos + bs) m.addEvent (juce::MidiMessage::noteOff (1, notes[k]), off - pos);
                }
                proc.processBlock (b, m);
                for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, b, c, 0, bs);
            }
        };
        auto loud = [&] (const juce::AudioBuffer<float>& out)
        {
            const int win = (int) (0.4 * tsr); double best = 0.0;
            for (int s0 = 0; s0 + win <= out.getNumSamples(); s0 += win / 4)
            { double sum = 0; for (int c = 0; c < 2; ++c) { auto* d = out.getReadPointer (c, s0); for (int i = 0; i < win; ++i) sum += d[i] * d[i]; }
              best = std::max (best, std::sqrt (sum / (2.0 * win))); }
            return juce::Decibels::gainToDecibels ((float) best, -120.0f);
        };
        std::vector<float> levels; int silent = 0, broken = 0, clipped = 0;
        juce::AudioBuffer<float> wav (2, 0), out;
        for (int k = 0; k < 40; ++k)
        {
            proc.loadPreset (findPreset ("Init Ring"));
            const auto what = proc.randomizeSound (URingAudioProcessor::RandAll, false);
            render (out);
            bool nan = false;
            for (int c = 0; c < 2; ++c) for (int i = 0; i < out.getNumSamples(); ++i) if (! std::isfinite (out.getSample (c, i))) nan = true;
            const float pk = out.getMagnitude (0, out.getNumSamples()), l = loud (out);
            levels.push_back (l);
            silent += l < -60.0f; broken += nan; clipped += pk > 0.99f;
            std::cout << "DICE " << juce::String (k).paddedLeft (' ', 2) << "  level " << juce::String (l, 1).paddedLeft (' ', 6) << " dB  peak "
                      << juce::String (juce::Decibels::gainToDecibels (pk), 1).paddedLeft (' ', 6) << "  " << what << std::endl;
            if (k < 10)
            {
                const int old = wav.getNumSamples(); wav.setSize (2, old + out.getNumSamples(), true, true);
                for (int c = 0; c < 2; ++c) wav.copyFrom (c, old, out, c, 0, out.getNumSamples());
            }
        }
        auto sorted = levels; std::sort (sorted.begin(), sorted.end());
        const float median = sorted[sorted.size() / 2];
        std::cout << "DICE median level " << median << " dB (factory presets ~ -23 dB), min " << sorted.front() << ", max " << sorted.back() << std::endl;
        check (broken == 0, "no NaN / broken audio in 40 random sounds");
        check (silent <= 1, "random sounds are audible (" + juce::String (silent) + " too quiet)");
        check (clipped <= 2, "random sounds rarely hit the limiter (" + juce::String (clipped) + ")");
        check (median > -28.0f && median < -18.0f, "median level close to the factory presets");
        // Variación: cambia poco
        proc.loadPreset (findPreset ("Marimba Rosewood"));
        const float before = proc.apvts.getRawParameterValue (IDs::bMaterial)->load();
        const int model = (int) proc.apvts.getRawParameterValue (IDs::bModel)->load();
        proc.randomizeSound (URingAudioProcessor::RandAll, true);
        const float after = proc.apvts.getRawParameterValue (IDs::bMaterial)->load();
        check (std::abs (after - before) <= 0.151f && (int) proc.apvts.getRawParameterValue (IDs::bModel)->load() == model, "VARY moves knobs a little and keeps the models");
        // Solo motor C: no toca el motor A
        proc.loadPreset (findPreset ("Felt Upright"));
        const float aModel = proc.apvts.getRawParameterValue (IDs::aModel)->load();
        proc.randomizeSound (URingAudioProcessor::RandEngineC, false);
        check (proc.apvts.getRawParameterValue (IDs::aModel)->load() == aModel && proc.apvts.getRawParameterValue (IDs::cMix)->load() > 0.4f, "ENGINE C ONLY leaves engine A alone and makes C audible");
        // Deshacer
        proc.loadPreset (findPreset ("Felt Upright"));
        proc.undoTick (false);
        proc.randomizeSound (URingAudioProcessor::RandAll, false);
        proc.undoTick (true);
        proc.undo();
        check (proc.getCurrentPresetName() == "Felt Upright" && proc.apvts.getRawParameterValue (IDs::aModel)->load() == aModel, "undo brings back the previous sound");
        // secuenciador apagado después de un random
        check (proc.apvts.getRawParameterValue (IDs::seqOn)->load() < 0.5f, "sequencer stays off");
        {   // captura con un sonido al azar cargado
            proc.randomizeSound (URingAudioProcessor::RandAll, false);
            std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
            editor->setSize (URingAudioProcessorEditor::baseW, URingAudioProcessorEditor::baseH);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
            writePng (*editor, outDir.getChildFile ("dado.png"));
        }
        wav.applyGain (juce::Decibels::decibelsToGain (14.0f));
        { auto f = outDir.getChildFile ("dado_10_sonidos.wav"); f.deleteFile(); juce::WavAudioFormat fmt;
          std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (new juce::FileOutputStream (f), tsr, 2, 24, {}, 0)); w->writeFromAudioSampleBuffer (wav, 0, wav.getNumSamples()); }
        std::cout << (fails == 0 ? "DICE: all checks passed" : "DICE: " + juce::String (fails) + " FAILED") << std::endl;
        return fails == 0 ? 0 : 1;
    }
    if (argc > 2 && juce::String (argv[2]) == "c15") // 1.5: carril MODEL, motor C como excitador de A y C > B (niveles y nulos)
    {
        constexpr double tsr = 48000.0;
        constexpr int bs = 256;
        struct Head : juce::AudioPlayHead
        {
            double ppq = 0.0, bpm = 120.0; bool playing = true;
            juce::Optional<PositionInfo> getPosition() const override
            { PositionInfo p; p.setIsPlaying (playing); p.setBpm (bpm); p.setPpqPosition (ppq); return p; }
        } ph;
        // un bloque con el transporte parado antes de cada render: el azar arranca igual (como en el DAW)
        auto restart = [&] { proc.prepareToPlay (tsr, bs); ph.ppq = 0.0; ph.playing = false;
                             juce::AudioBuffer<float> b (2, bs); juce::MidiBuffer m; proc.processBlock (b, m); ph.playing = true; cz::VoiceMeter::reset(); };
        proc.setPlayHead (&ph);
        proc.setPlayConfigDetails (0, 2, tsr, bs);
        auto setP = [&] (const char* id, float v) { auto* prm = proc.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };
        int fails = 0;
        auto check = [&] (bool ok, const juce::String& what) { std::cout << (ok ? "OK   " : "FAIL ") << what << std::endl; if (! ok) ++fails; };
        auto dB = [] (float x) { return juce::String (juce::Decibels::gainToDecibels (x, -120.0f), 1); };

        auto diffDb = [] (const juce::AudioBuffer<float>& x, const juce::AudioBuffer<float>& y)
        {
            float d = 0.0f, pk = 1.0e-9f;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < x.getNumSamples(); ++i)
                {
                    d = juce::jmax (d, std::abs (x.getSample (ch, i) - y.getSample (ch, i)));
                    pk = juce::jmax (pk, std::abs (x.getSample (ch, i)));
                }
            return juce::Decibels::gainToDecibels (d / pk, -240.0f);
        };

        // ---- Carril MODEL (cada render con un procesador nuevo: sin colas del render anterior)
        auto renderModels = [&] (std::function<int (int)> modelOf, double secs, bool keep = false)
        {
            auto pp = std::make_unique<URingAudioProcessor>();
            auto& P = *pp;
            P.setPlayHead (&ph); P.setPlayConfigDetails (0, 2, tsr, bs);
            auto set = [&] (const char* id, float v) { auto* prm = P.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };
            P.loadPreset (findPreset ("Init Ring"));
            set (IDs::cMix, 1.0f); set (IDs::cModel, 8.0f); set (IDs::seqLength, 8.0f); set (IDs::seqPulses, 8.0f); set (IDs::seqOn, 1.0f);
            for (int k = 0; k < cz::SeqPattern::kSteps; ++k) { auto d = P.pattern().get (k); d.model = modelOf (k); P.pattern().set (k, d); }
            P.prepareToPlay (tsr, bs); ph.ppq = 0.0; ph.playing = true;
            juce::AudioBuffer<float> all (2, (int) (secs * tsr)); all.clear();
            juce::AudioBuffer<float> b (2, bs);
            for (int pos = 0; pos + bs <= all.getNumSamples(); pos += bs)
            {
                juce::MidiBuffer m; b.clear(); P.processBlock (b, m);
                ph.ppq += bs * ph.bpm / (60.0 * tsr);
                for (int ch = 0; ch < 2; ++ch) all.copyFrom (ch, pos, b, ch, 0, bs);
            }
            if (keep)
            {
                juce::MemoryBlock mb; P.getStateInformation (mb);
                URingAudioProcessor other; other.setStateInformation (mb.getData(), (int) mb.getSize());
                check (other.pattern().get (3).model == 14 && other.pattern().get (1).model == -1, "MODEL lane saved with the project");
            }
            return all;
        };
        const auto knob  = renderModels ([] (int) { return -1; }, 4.0);
        const auto knob2 = renderModels ([] (int) { return -1; }, 4.0);
        const auto same  = renderModels ([] (int) { return 8; }, 4.0);
        const auto hats  = renderModels ([] (int) { return 23; }, 4.0);
        static const int seqModels[] { 21, -1, 23, 14, 21, 22, 23, 15 };
        const auto mixed = renderModels ([] (int k) { return seqModels[k % 8]; }, 8.0, true);
        std::cout << "MODEL lane: same render twice " << diffDb (knob, knob2) << " dB, knob vs lane=knob model " << diffDb (knob, same)
                  << " dB, knob vs lane=Hi-Hat " << diffDb (knob, hats) << " dB" << std::endl;
        check (diffDb (knob, same) < -100.0f, "MODEL lane set to the knob's model sounds identical (null test)");
        check (diffDb (knob, hats) > -20.0f, "MODEL lane changes the sound (Hi-Hat on every step)");
        check (mixed.getMagnitude (0, mixed.getNumSamples()) > 1.0e-3f && mixed.getMagnitude (0, mixed.getNumSamples()) < 1.0f, "mixed models per step: peak " + dB (mixed.getMagnitude (0, mixed.getNumSamples())) + " dB");
        {
            juce::WavAudioFormat fmt; const auto f = outDir.getChildFile ("c15_model_lane.wav"); f.deleteFile();
            if (std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (new juce::FileOutputStream (f), tsr, 2, 24, {}, 0)); w) w->writeFromAudioSampleBuffer (mixed, 0, mixed.getNumSamples());
        }
        setP (IDs::seqOn, 0.0f);

        // ---- Motor C como excitador del resonador A (nivel comparado con el arco)
        auto renderNote = [&] (int note, double secs)
        {
            restart();
            float pk = 0.0f; double sum = 0.0; int n = 0;
            juce::AudioBuffer<float> b (2, bs);
            for (int pos = 0; pos < (int) (secs * tsr); pos += bs)
            {
                juce::MidiBuffer m; b.clear();
                if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);
                if (pos >= (int) (1.5 * tsr) && pos < (int) (1.5 * tsr) + bs) m.addEvent (juce::MidiMessage::noteOff (1, note), 0);
                proc.processBlock (b, m);
                pk = juce::jmax (pk, b.getMagnitude (0, bs)); sum += b.getRMSLevel (0, 0, bs); ++n;
            }
            return std::pair<float, float> { cz::VoiceMeter::peak.load(), (float) (sum / n) };   // pico a la entrada de la saturación de la voz
        };
        const char* aModels[] { "Modal", "String", "Sympathetic", "Metallic", "Piano", "Tine", "Reed" };
        const int cModels[] { 8, 2, 13, 17 };   // VA, FM, wavetable, ruido filtrado
        float worst = 0.0f, best = 1.0e9f;
        for (int am = 0; am < 7; ++am)
        {
            proc.loadPreset (findPreset ("Init Ring"));
            setP (IDs::abMix, 0.0f); setP (IDs::aModel, (float) am); setP (IDs::aExciter, 2.0f); setP (IDs::volume, -6.0f);
            const auto bow = renderNote (48, 2.5);
            juce::String line = juce::String ("EXCITE ") + juce::String (aModels[am]).paddedRight (' ', 12) + " bow peak " + dB (bow.first);
            for (int cm : cModels)
            {
                setP (IDs::aExciter, 5.0f); setP (IDs::cModel, (float) cm);
                const auto ce = renderNote (48, 2.5);
                const float rel = juce::Decibels::gainToDecibels (ce.first / juce::jmax (1.0e-6f, bow.first));
                worst = juce::jmax (worst, rel); best = juce::jmin (best, rel);
                line << "  C" << (cm + 1) << " " << dB (ce.first) << " (" << juce::String (rel, 1) << ")";
            }
            std::cout << line << std::endl;
        }
        check (worst < 10.0f && best > -18.0f, "ENGINE C exciter level close to the bow (range " + juce::String (best, 1) + " .. " + juce::String (worst, 1) + " dB)");

        // ---- C > B FEED: el motor C hace vibrar al objeto del motor B
        proc.loadPreset (findPreset ("Init Ring"));
        setP (IDs::abMix, 1.0f); setP (IDs::bModel, 1.0f); setP (IDs::cModel, 8.0f); setP (IDs::volume, -6.0f);
        const auto b0 = renderNote (48, 2.5);
        setP (IDs::cFeedB, 1.0f);
        const auto b1 = renderNote (48, 2.5);
        std::cout << "C > B FEED: off rms " << dB (b0.second) << " dB, 100% rms " << dB (b1.second) << " dB, peak " << dB (b1.first) << " dB" << std::endl;
        check (b1.second > b0.second * 1.2f && b1.first < 1.0f, "C > B FEED adds energy without clipping");
        setP (IDs::cFeedB, 0.0f);

        // presets viejos no cambian: el excitador por defecto sigue siendo PLUCK y C > B arranca en 0
        URingAudioProcessor fresh;
        check ((int) fresh.apvts.getRawParameterValue (IDs::aExciter)->load() == 0 && fresh.apvts.getRawParameterValue (IDs::cFeedB)->load() == 0.0f, "defaults unchanged (old projects sound the same)");
        std::cout << (fails == 0 ? "C15: all checks passed" : "C15: " + juce::String (fails) + " FAILED") << std::endl;
        return fails == 0 ? 0 : 1;
    }
    if (argc > 2 && juce::String (argv[2]) == "v15") // 1.5: vista de cables, globo de los jacks, ESCUCHAR en el buscador
    {
        constexpr double tsr = 48000.0;
        proc.setPlayConfigDetails (0, 2, tsr, 512);
        proc.prepareToPlay (tsr, 512);
        const int savedView = mm::lang::getSetting ("cableView", 0);
        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        editor->setSize (URingAudioProcessorEditor::baseW, URingAudioProcessorEditor::baseH);
        auto* ed = dynamic_cast<URingAudioProcessorEditor*> (editor.get());
        proc.loadPreset (findPreset ("Mbira Ghost"));
        proc.clearPatches();
        proc.addPatch (URingAudioProcessor::SrcLfo1, URingAudioProcessor::DAPosition, 0.5f);
        proc.addPatch (URingAudioProcessor::SrcLfo2, URingAudioProcessor::DCutoff, -0.25f);
        proc.addPatch (URingAudioProcessor::SrcEnv, URingAudioProcessor::DCutoff, 0.75f);
        ed->showTab (0);
        int fails = 0;
        auto check = [&] (bool ok, const juce::String& what) { std::cout << (ok ? "OK   " : "FAIL ") << what << std::endl; if (! ok) ++fails; };

        const char* files[] { "v15_cables_shown.png", "v15_cables_faded.png", "v15_cables_hidden.png" };
        for (int v = 0; v < 3; ++v)
        {
            ed->testSetCableView (v);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
            writePng (*editor, outDir.getChildFile (files[v]));
            check (ed->testCableView() == v && mm::lang::getSetting ("cableView", -1) == v, "cable view " + juce::String (v) + " applied and remembered");
        }
        check (proc.getPatches().size() == 3, "hidden cables keep modulating (3 patches)");
        {   // una ventana nueva arranca con la vista elegida
            std::unique_ptr<juce::AudioProcessorEditor> second (proc.createEditor());
            check (dynamic_cast<URingAudioProcessorEditor*> (second.get())->testCableView() == 2, "new window opens with the remembered view");
        }

        ed->testHoverJack (URingAudioProcessor::DCutoff);
        const auto hover = ed->testHoverText();
        std::cout << "HOVER CUTOFF: " << hover.replace ("\n", " | ") << std::endl;
        check (hover.contains ("LFO 2") && hover.contains ("-25%") && hover.contains ("ENV") && hover.contains ("75%"), "hover lists both cables into CUTOFF with amounts");
        ed->testHoverJack (100 + URingAudioProcessor::SrcLfo1);
        check (ed->testHoverText().contains ("A POSITION"), "hover on LFO 1 output lists its destination");
        ed->testHoverJack (URingAudioProcessor::DPitch);
        check (ed->testHoverText().contains (mm::lang::tr ("NOT PATCHED")), "hover on a free input says not patched");
        ed->testSetCableView (2);
        ed->testHoverJack (URingAudioProcessor::DCutoff);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
        writePng (*editor, outDir.getChildFile ("v15_hover_hidden.png"));
        ed->testHoverJack (-1);

        // ESCUCHAR: suena una nota y se suelta sola
        proc.loadPreset (findPreset ("Mbira Ghost"));
        auto level = [&] (double secs)
        {
            float pk = 0.0f;
            juce::AudioBuffer<float> b (2, 512);
            for (int n = 0; n < (int) (secs * tsr / 512); ++n)
            {
                juce::MidiBuffer m; proc.processBlock (b, m);
                pk = juce::jmax (pk, b.getMagnitude (0, 512));
                juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
            }
            return pk;
        };
        level (0.5);
        ed->testPreview ("Perc");
        const float during = level (0.3);
        check (during > 1.0e-3f, "preview plays a note (peak " + juce::String (juce::Decibels::gainToDecibels (during), 1) + " dB)");
        juce::MessageManager::getInstance()->runDispatchLoopUntil (900);
        check (! ed->testPreviewSounding(), "preview note released by itself");
        ed->testPreview ("Bass"); ed->testPreview ("Keys");   // flechas rápidas: nunca dos notas colgadas
        juce::MessageManager::getInstance()->runDispatchLoopUntil (900);
        level (6.0);
        check (level (0.5) < 1.0e-4f, "after fast browsing everything goes quiet (no stuck notes)");

        // RUN arranca apagado: instancia nueva, preset de fábrica y preset de usuario guardado con RUN prendido
        {
            URingAudioProcessor fresh;
            check (fresh.apvts.getRawParameterValue (IDs::seqOn)->load() < 0.5f, "new instance: sequencer off");
            proc.loadPreset (findPreset ("Euclid Plonks"));
            check (proc.apvts.getRawParameterValue (IDs::seqOn)->load() < 0.5f, "factory preset: sequencer off");
            proc.apvts.getParameter (IDs::seqOn)->setValueNotifyingHost (1.0f);
            const juce::String tmpName ("zz test run v15");
            proc.saveUserPreset (tmpName);
            proc.loadPreset (findPreset ("Init Ring"));
            const bool loaded = proc.loadUserPreset (tmpName);
            check (loaded && proc.apvts.getRawParameterValue (IDs::seqOn)->load() < 0.5f, "user preset saved with RUN on: loads with sequencer off");
            URingAudioProcessor::userPresetFolder().getChildFile (tmpName + ".uring").deleteFile();
            proc.apvts.getParameter (IDs::seqOn)->setValueNotifyingHost (1.0f);
            check (proc.apvts.getRawParameterValue (IDs::seqOn)->load() > 0.5f, "RUN button still turns it on");
        }

        ed->openPresetBrowser(); juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
        writePng (*editor, outDir.getChildFile ("v15_browser.png"));
        ed->closePresetBrowser();
        // capturas de lo nuevo de la 1.5: carril MODEL, motor C como excitador, C > B
        proc.loadPreset (findPreset ("Model Hopper"));
        ed->showTab (2); ed->testShowSeqLane ((int) cz::Lane::Model);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
        writePng (*editor, outDir.getChildFile ("v15_model_lane.png"));
        proc.loadPreset (findPreset ("FM Struck String"));
        ed->showTab (0); juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
        writePng (*editor, outDir.getChildFile ("v15_exciter_c.png"));
        proc.loadPreset (findPreset ("Gong Driven"));
        ed->showTab (4); juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
        writePng (*editor, outDir.getChildFile ("v15_engine_c.png"));
        mm::lang::setSetting ("cableView", savedView);
        std::cout << (fails == 0 ? "V15: all checks passed" : "V15: " + juce::String (fails) + " FAILED") << std::endl;
        return fails == 0 ? 0 : 1;
    }
    if (argc > 2 && juce::String (argv[2]) == "macro") // motor C: 24 modelos, afinación, FEED, ruteo SEQ 2, CPU, presets, captura
    {
        constexpr double tsr = 48000.0;
        DemoPlayHead ph; ph.bpm = 122.0;
        proc.setPlayHead (&ph);
        auto setP = [&] (const char* id, float v) { auto* prm = proc.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };
        proc.setPlayConfigDetails (0, 2, tsr, 512);
        auto dry = [&]
        {
            proc.loadPreset (0);
            for (auto* id : { IDs::seqOn, IDs::delayOn, IDs::springOn, IDs::glitchOn }) setP (id, 0.0f);
            for (auto* id : { IDs::variation, IDs::width, IDs::life, IDs::body, IDs::chorusMix, IDs::xfeed }) setP (id, 0.0f);
            setP ("lfo1Depth", 0.0f); setP ("lfo2Depth", 0.0f); proc.clearPatches();
            setP (IDs::cutoff, 20000.0f); setP (IDs::keyTrack, 0.0f);
        };
        // toca notas y devuelve el canal izquierdo (o estéreo intercalado si stereo)
        auto play = [&] (std::vector<std::pair<int, double>> notes, double secs, int ch, juce::AudioBuffer<float>* keep)
        {
            proc.prepareToPlay (tsr, 512);
            ph.ppq = 0.0;
            juce::AudioBuffer<float> b (2, 512);
            std::vector<float> x;
            const int total = (int) (secs * tsr);
            if (keep) keep->setSize (2, total);
            for (int pos = 0; pos < total; pos += 512)
            {
                juce::MidiBuffer m;
                for (auto [n, t] : notes)
                {
                    const int on = (int) (t * tsr), off = on + (int) (0.35 * tsr);
                    if (on >= pos && on < pos + 512) m.addEvent (juce::MidiMessage::noteOn (ch, n, 0.8f), on - pos);
                    if (off >= pos && off < pos + 512) m.addEvent (juce::MidiMessage::noteOff (ch, n), off - pos);
                }
                proc.processBlock (b, m);
                ph.ppq += 512 * ph.bpm / (60.0 * tsr);
                const int n = juce::jmin (512, total - pos);
                for (int i = 0; i < n; ++i) x.push_back (b.getSample (0, i));
                if (keep) for (int c = 0; c < 2; ++c) keep->copyFrom (c, pos, b, c, 0, n);
            }
            return x;
        };
        auto dB = [] (double v) { return 20.0 * std::log10 (juce::jmax (1.0e-9, v)); };
        auto stats = [&] (const std::vector<float>& x, double& peak, double& rms)
        {
            peak = 0; double s2 = 0;
            for (auto v : x) { peak = juce::jmax (peak, (double) std::abs (v)); s2 += (double) v * v; }
            rms = std::sqrt (s2 / juce::jmax ((size_t) 1, x.size()));
        };
        auto yinCents = [&] (const std::vector<float>& x, int note, int offMs)
        {
            const double f0 = 440.0 * std::pow (2.0, (note - 69) / 12.0);
            const int lo = (int) (tsr / (f0 * 1.25)), hi = (int) (tsr / (f0 / 1.25)) + 2, W = 2048, off = (int) (offMs * tsr / 1000);
            std::vector<double> d ((size_t) hi + 2, 0.0);
            for (int tau = 1; tau <= hi + 1; ++tau)
                for (int i = 0; i < W; ++i) { const double df = x[(size_t) (off + i)] - x[(size_t) (off + i + tau)]; d[(size_t) tau] += df * df; }
            std::vector<double> cm (d.size(), 1.0); double run = 0.0;
            for (int tau = 1; tau < (int) d.size(); ++tau) { run += d[(size_t) tau]; cm[(size_t) tau] = d[(size_t) tau] * tau / juce::jmax (1.0e-12, run); }
            int best = lo; for (int tau = lo; tau <= hi; ++tau) if (cm[(size_t) tau] < cm[(size_t) best]) best = tau;
            double t = best;
            if (best > 1 && best + 1 < (int) cm.size()) { const double a = cm[(size_t) best - 1], bb = cm[(size_t) best], c = cm[(size_t) best + 1];
                const double den = a - 2 * bb + c; if (std::abs (den) > 1.0e-12) t += 0.5 * (a - c) / den; }
            return 1200.0 * std::log2 ((tsr / t) / f0);
        };
        juce::WavAudioFormat fmt;
        auto writeWav = [&] (const juce::AudioBuffer<float>& b, const juce::String& name)
        {
            auto f = outDir.getChildFile (name); f.deleteFile();
            std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (new juce::FileOutputStream (f), tsr, 2, 24, {}, 0));
            w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
            std::cout << "WAV: " << f.getFullPathName() << std::endl;
        };

        // 1. Los 24 modelos: nivel y afinación (los que tienen altura definida)
        juce::AudioBuffer<float> all (2, 0);
        const std::vector<std::pair<int, double>> phrase { { 48, 0.0 }, { 55, 0.5 }, { 60, 1.0 }, { 63, 1.25 } };
        for (auto [label, eng, model] : { std::tuple<const char*, int, int> { "A String", 0, 1 }, { "A Modal", 0, 0 }, { "B Marimba", 1, 6 }, { "B Bar", 1, 1 } })
        {
            dry(); setP (eng == 0 ? IDs::aModel : IDs::bModel, (float) model); setP (IDs::abMix, (float) eng);
            auto x = play (phrase, 2.0, 1, nullptr);
            double pk, rms; stats (x, pk, rms);
            std::cout << "REF   " << juce::String (label).paddedRight (' ', 21) << "pico " << juce::String (dB (pk), 1).paddedLeft (' ', 6) << " dB  rms " << juce::String (dB (rms), 1).paddedLeft (' ', 6) << " dB" << std::endl;
        }
        for (int m = 0; m < 24; ++m)
        {
            dry(); setP (IDs::cModel, (float) m); setP (IDs::cMix, 1.0f);
            juce::AudioBuffer<float> keep;
            auto x = play (phrase, 2.0, 1, &keep);
            double pk, rms; stats (x, pk, rms);
            const int old = all.getNumSamples();
            all.setSize (2, old + keep.getNumSamples(), true, true);
            for (int c = 0; c < 2; ++c) all.copyFrom (c, old, keep, c, 0, keep.getNumSamples());
            juce::String line; line << "MACRO " << juce::String (m).paddedLeft (' ', 2) << " " << juce::String (cz::macroModelName (m)).paddedRight (' ', 18)
                                   << "pico " << juce::String (dB (pk), 1).paddedLeft (' ', 6) << " dB  rms " << juce::String (dB (rms), 1).paddedLeft (' ', 6) << " dB";
            const bool pitched = (m <= 14 && m != 5) || m == 19 || m == 20;
            if (pitched)
            {
                line << "  afinación:";
                for (int note : { 36, 60, 84 })
                {
                    dry(); setP (IDs::cModel, (float) m); setP (IDs::cMix, 1.0f); setP (IDs::cLpg, 0.0f);
                    if (m == 14) setP (IDs::cHarm, 0.0f);   // acordes: octavas, fundamental = la nota
                    auto y = play ({ { note, 0.0 } }, 0.4, 1, nullptr);
                    const double c = yinCents (y, note, 120);
                    line << " " << note << ":" << (c >= 0 ? "+" : "") << juce::String (juce::roundToInt (c)) << "c";
                }
            }
            bool bad = false; for (auto v : x) if (! std::isfinite (v)) bad = true;
            std::cout << line << (bad ? "  ** NaN **" : "") << std::endl;
        }
        all.applyGain (proc.apvts.getRawParameterValue (IDs::volume) != nullptr ? 1.0f : 1.0f);
        writeWav (all, "uring_macro_24_modelos.wav");

        // 2. C > A FEED: el resonador A suena con Plaits como excitador (sin mezcla directa de C)
        for (float feed : { 0.0f, 0.7f })
        {
            dry(); setP (IDs::cModel, 20); setP (IDs::cMix, 0.0f); setP (IDs::cFeed, feed); setP (IDs::aModel, 1); setP (IDs::abMix, 0.0f);
            auto x = play ({ { 48, 0.0 } }, 1.5, 1, nullptr);
            double pk, rms; stats (x, pk, rms);
            std::cout << "FEED C>A " << feed << "  rms " << juce::String (dB (rms), 1) << " dB" << std::endl;
        }
        // 3. Ruteo: SEQ 2 (canal 16) toca B, C o B+C — C solo con cMix 0 igual se escucha si el SEQ 2 lo toca
        for (int tgt : { 0, 1, 2 })
        {
            dry(); setP (IDs::cModel, 21); setP (IDs::cMix, 0.0f); setP (IDs::seq2Target, (float) tgt); setP (IDs::bModel, 1); setP (IDs::abMix, 1.0f);
            for (float bOn : { 1.0f }) juce::ignoreUnused (bOn);
            auto x16 = play ({ { 36, 0.0 } }, 0.8, 16, nullptr);
            double pk, rms; stats (x16, pk, rms);
            // con B apagado (sin volumen en B) se aísla C
            dry(); setP (IDs::cModel, 21); setP (IDs::cMix, 0.0f); setP (IDs::seq2Target, (float) tgt); setP (IDs::abMix, 0.0f);
            auto x15 = play ({ { 36, 0.0 } }, 0.8, 15, nullptr);   // canal 15 = SEQ 2 motor A: C nunca debe sonar acá si tgt>=1
            double pk2, rms2; stats (x15, pk2, rms2);
            std::cout << "SEQ2 target " << tgt << "  canal16 rms " << juce::String (dB (rms), 1) << " dB   canal15 rms " << juce::String (dB (rms2), 1) << " dB" << std::endl;
        }
        // 4. CPU: 10 voces sostenidas, peor caso
        for (int m : { 2, 14, 16, 8 })
        {
            dry(); setP (IDs::cModel, (float) m); setP (IDs::cMix, 0.5f); setP (IDs::cFeed, 0.5f); setP (IDs::cLpg, 0.0f);
            proc.prepareToPlay (tsr, 512);
            juce::AudioBuffer<float> b (2, 512);
            juce::MidiBuffer on; for (int k = 0; k < 10; ++k) on.addEvent (juce::MidiMessage::noteOn (1, 40 + k * 3, 0.8f), 0);
            proc.processBlock (b, on);
            const auto t0 = juce::Time::getHighResolutionTicks();
            const int blocks = (int) (5.0 * tsr / 512);
            for (int n = 0; n < blocks; ++n) { juce::MidiBuffer mm; proc.processBlock (b, mm); }
            const double secs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
            std::cout << "CPU 10 voces A+B+C (" << cz::macroModelName (m) << "): " << juce::String (100.0 * secs / 5.0, 1) << "% de un núcleo" << std::endl;
        }
        // 5. Presets MACRO con su secuencia: niveles y WAV
        juce::AudioBuffer<float> pre (2, 0);
        std::vector<juce::String> names { "Mutating Plonks" };
        for (const auto& fp : presets::factory()) if (juce::String (fp.category) == "Macro") names.push_back (fp.name);
        for (const auto& nm : names)
        {
            const char* name = nm.toRawUTF8();
            proc.loadPreset (findPreset (name));
            setP (IDs::seqOn, 1.0f);
            juce::AudioBuffer<float> keep;
            auto x = play ({}, 6.0, 1, &keep);
            double pk, rms; stats (x, pk, rms);
            std::cout << "PRESET " << juce::String (name).paddedRight (' ', 22) << "pico " << juce::String (dB (pk), 1) << " dB  rms " << juce::String (dB (rms), 1) << " dB" << std::endl;
            const int old = pre.getNumSamples();
            pre.setSize (2, old + keep.getNumSamples(), true, true);
            for (int c = 0; c < 2; ++c) pre.copyFrom (c, old, keep, c, 0, keep.getNumSamples());
        }
        pre.applyGain (juce::Decibels::decibelsToGain (14.0f)); // los presets salen a -20 dB: se sube para escucharlos
        writeWav (pre, "uring_macro_presets.wav");

        // 6. Captura de la pestaña MACRO
        proc.loadPreset (findPreset ("Dub Chord Stab"));
        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        editor->setSize (URingAudioProcessorEditor::baseW, URingAudioProcessorEditor::baseH);
        auto* ed = dynamic_cast<URingAudioProcessorEditor*> (editor.get());
        ed->showTab (4); juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        writePng (*editor, outDir.getChildFile ("uring_macro.png"));
        return 0;
    }
    if (argc > 2 && juce::String (argv[2]) == "tuning") // afinación de cada modelo en 6 octavas (cents de error)
    {
        constexpr double tsr = 48000.0;
        auto setP = [&] (const char* id, float v) { auto* prm = proc.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };
        proc.setPlayConfigDetails (0, 2, tsr, 512);
        struct M { const char* name; const char* modelId; int model; int exciter; bool engineA; };
        const M models[] { { "A Modal", IDs::aModel, 0, 1, true }, { "A String", IDs::aModel, 1, 0, true }, { "A Metallic", IDs::aModel, 3, 1, true },
                           { "A Piano", IDs::aModel, 4, 4, true }, { "A Tine", IDs::aModel, 5, 4, true }, { "A Reed", IDs::aModel, 6, 4, true },
                           { "B Bar", IDs::bModel, 1, 0, false }, { "B Marimba", IDs::bModel, 6, 0, false }, { "B Vibes", IDs::bModel, 7, 0, false },
                           { "B Mbira", IDs::bModel, 8, 0, false }, { "B Steel Pan", IDs::bModel, 9, 0, false }, { "B Tube", IDs::bModel, 4, 0, false } };
        for (const auto& md : models)
        {
            juce::String line = juce::String (md.name).paddedRight (' ', 12);
            for (int note : { 36, 48, 60, 72, 84, 96 })
            {
                proc.loadPreset (0);
                for (auto* id : { IDs::seqOn, IDs::delayOn, IDs::springOn, IDs::glitchOn }) setP (id, 0.0f);
                for (auto* id : { IDs::variation, IDs::width, IDs::life, IDs::body, IDs::chorusMix, IDs::xfeed }) setP (id, 0.0f);
                setP ("lfo1Depth", 0.0f); setP ("lfo2Depth", 0.0f); proc.clearPatches();
                setP (md.modelId, (float) md.model);
                setP (md.engineA ? IDs::aExciter : IDs::bExciter, (float) md.exciter);
                setP (IDs::abMix, md.engineA ? 0.0f : 1.0f);
                setP (IDs::bSweep, 0.0f); setP (IDs::bSpread, 0.0f); setP (IDs::cutoff, 20000.0f); setP (IDs::keyTrack, 0.0f);
                setP (IDs::aDamping, 0.2f); setP (IDs::bDecay, 0.7f);
                if (argc > 3) setP (IDs::quality, juce::String (argv[3]) == "eco" ? 0.0f : 1.0f);
                proc.prepareToPlay (tsr, 512);
                juce::AudioBuffer<float> b (2, 512);
                std::vector<float> x;
                for (int pos = 0; pos < (int) (0.6 * tsr); pos += 512)
                {
                    juce::MidiBuffer m;
                    if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);
                    proc.processBlock (b, m);
                    for (int i = 0; i < 512; ++i) x.push_back (b.getSample (0, i));
                }
                // YIN alrededor de la nota esperada (evita errores de octava)
                const double f0 = 440.0 * std::pow (2.0, (note - 69) / 12.0);
                const int lo = (int) (tsr / (f0 * 1.25)), hi = (int) (tsr / (f0 / 1.25)) + 2, W = 2048, off = (int) (0.04 * tsr);
                std::vector<double> d ((size_t) hi + 2, 0.0);
                for (int tau = 1; tau <= hi + 1; ++tau)
                    for (int i = 0; i < W; ++i) { const double df = x[(size_t) (off + i)] - x[(size_t) (off + i + tau)]; d[(size_t) tau] += df * df; }
                // cuánto tarda en caer 40 dB desde el pico (ms)
                float pk = 0.0f; int pkAt = 0;
                for (int i = 0; i < (int) x.size(); ++i) if (std::abs (x[(size_t) i]) > pk) { pk = std::abs (x[(size_t) i]); pkAt = i; }
                int decayAt = (int) x.size();
                for (int i = (int) x.size() - 1; i > pkAt; --i) if (std::abs (x[(size_t) i]) > pk * 0.01f) { decayAt = i; break; }
                const int decayMs = (int) ((decayAt - pkAt) * 1000.0 / tsr);
                std::vector<double> cm (d.size(), 1.0);
                double run = 0.0;
                for (int tau = 1; tau < (int) d.size(); ++tau) { run += d[(size_t) tau]; cm[(size_t) tau] = d[(size_t) tau] * tau / juce::jmax (1.0e-12, run); }
                int best = lo;
                for (int tau = lo; tau <= hi; ++tau) if (cm[(size_t) tau] < cm[(size_t) best]) best = tau;
                double t = best;
                if (best > 1 && best + 1 < (int) cm.size())
                {
                    const double a = cm[(size_t) best - 1], bb = cm[(size_t) best], c = cm[(size_t) best + 1];
                    const double den = a - 2 * bb + c; if (std::abs (den) > 1.0e-12) t += 0.5 * (a - c) / den;
                }
                const double cents = 1200.0 * std::log2 ((tsr / t) / f0);
                line << juce::String (note).paddedLeft (' ', 4) << ":" << (cents >= 0 ? "+" : "") << juce::String (juce::roundToInt (cents)) << "c/" << decayMs << "ms ";
            }
            std::cout << "TUNING " << line << std::endl;
        }
        return 0;
    }
    if (argc > 2 && juce::String (argv[2]) == "tour") // recorrido de presets para escuchar: URingDemo <salida> tour "Preset 1" "Preset 2" ...
    {
        constexpr double tsr = 48000.0;
        constexpr int bs = 512;
        DemoPlayHead ph; ph.bpm = 122.0;
        proc.setPlayHead (&ph);
        proc.setPlayConfigDetails (0, 2, tsr, bs);
        juce::AudioBuffer<float> all (2, 0);
        for (int a = 3; a < argc; ++a)
        {
            const juce::String name (argv[a]);
            if (name.startsWith ("--")) continue;
            const int k = findPreset (name.toRawUTF8());
            const juce::String cat (presets::factory()[(size_t) k].category);
            const bool seq = cat == "Sequence" || cat == "Macro" || cat == "Glitch";
            proc.loadPreset (k);
            proc.apvts.getParameter (IDs::seqOn)->setValueNotifyingHost (seq ? 1.0f : 0.0f);
            proc.prepareToPlay (tsr, bs);
            ph.ppq = 0.0;
            const int base = cat == "Bass" ? 36 : 60;
            struct Ev { double t; int note; double dur; float vel; };
            std::vector<Ev> ev;
            if (! seq) ev = { { 0.0, base, 0.4, 0.9f }, { 0.5, base + 7, 0.4, 0.7f }, { 1.0, base + 12, 0.8, 1.0f },
                              { 2.0, base - 12, 2.0, 0.8f }, { 2.0, base - 5, 2.0, 0.8f }, { 2.0, base + 3, 2.0, 0.8f } };
            const int total = (int) (6.0 * tsr);
            juce::AudioBuffer<float> out (2, total), b (2, bs);
            out.clear();
            for (int pos = 0; pos + bs <= total; pos += bs)
            {
                juce::MidiBuffer m;
                for (const auto& e : ev)
                {
                    const int on = (int) (e.t * tsr), off = (int) ((e.t + e.dur) * tsr);
                    if (on >= pos && on < pos + bs) m.addEvent (juce::MidiMessage::noteOn (1, e.note, e.vel), on - pos);
                    if (off >= pos && off < pos + bs) m.addEvent (juce::MidiMessage::noteOff (1, e.note), off - pos);
                }
                proc.processBlock (b, m);
                ph.ppq += bs * ph.bpm / (60.0 * tsr);
                for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, b, c, 0, bs);
            }
            out.applyGainRamp (total - 4800, 4800, 1.0f, 0.0f);
            const int old = all.getNumSamples();
            all.setSize (2, old + total, true, true);
            for (int c = 0; c < 2; ++c) all.copyFrom (c, old, out, c, 0, total);
            std::cout << "TOUR " << name << std::endl;
        }
        all.applyGain (juce::Decibels::decibelsToGain (14.0f));
        for (int c = 0; c < 2; ++c) for (int i = 0; i < all.getNumSamples(); ++i) all.setSample (c, i, juce::jlimit (-0.98f, 0.98f, all.getSample (c, i)));
        auto f = outDir.getChildFile ("uring_tour.wav"); f.deleteFile();
        juce::WavAudioFormat fmt;
        std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (new juce::FileOutputStream (f), tsr, 2, 24, {}, 0));
        w->writeFromAudioSampleBuffer (all, 0, all.getNumSamples());
        std::cout << "WAV: " << f.getFullPathName() << std::endl;
        return 0;
    }
    if (argc > 2 && juce::String (argv[2]) == "calib") // nivel de cada modelo / excitador antes de la saturación de la voz
    {
        constexpr double tsr = 48000.0;
        constexpr int bs = 256;
        proc.setPlayConfigDetails (0, 2, tsr, bs);
        auto setP = [&] (const char* id, float v) { auto* prm = proc.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };
        auto measure = [&] (bool engineA, int model, int exc)
        {
            float worst = 0.0f;
            for (int note : { 36, 48, 60, 72, 84 })
                for (float settings : { 0.0f, 1.0f })   // perillas por defecto, y largo / brillante
                {
                    proc.loadPreset (0);
                    for (auto* id : { IDs::delayOn, IDs::springOn, IDs::glitchOn }) setP (id, 0.0f);
                    for (auto* id : { IDs::variation, IDs::width, IDs::life, IDs::body, IDs::chorusMix, IDs::xfeed, IDs::drive }) setP (id, 0.0f);
                    setP ("lfo1Depth", 0.0f); setP ("lfo2Depth", 0.0f); proc.clearPatches();
                    setP (IDs::abMix, engineA ? 0.0f : 1.0f);
                    setP (engineA ? IDs::aModel : IDs::bModel, (float) model);
                    setP (engineA ? IDs::aExciter : IDs::bExciter, (float) exc);
                    if (settings > 0.5f)
                    {
                        setP (IDs::aDamping, 0.15f); setP (IDs::aBright, 0.9f); setP (IDs::aStructure, 0.6f);
                        setP (IDs::bDecay, 0.85f); setP (IDs::bMaterial, 0.95f); setP (IDs::bTone, 0.9f);
                    }
                    proc.prepareToPlay (tsr, bs);
                    cz::VoiceMeter::reset();
                    juce::AudioBuffer<float> b (2, bs);
                    for (int pos = 0; pos < (int) (1.5 * tsr); pos += bs)
                    {
                        juce::MidiBuffer m;
                        if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, note, 1.0f), 0);
                        if (pos == (int) (1.0 * tsr) / bs * bs) m.addEvent (juce::MidiMessage::noteOff (1, note), 0);
                        proc.processBlock (b, m);
                    }
                    worst = juce::jmax (worst, cz::VoiceMeter::peak.load());
                }
            return worst;
        };
        const char* aNames[] { "Modal", "String", "Sympathetic", "Metallic", "Piano", "Tine", "Reed" };
        const char* bNames[] { "Membrane", "Bar", "Plate", "Bell", "Tube", "Glass", "Marimba", "Vibes", "Mbira", "SteelPan", "HandDrum", "Gong" };
        std::cout << "inline float resonator[7][5] {";
        for (int m = 0; m < 7; ++m)
        {
            std::cout << (m ? ", " : " ") << "{ ";
            for (int e = 0; e < 5; ++e)
            {
                const float pk = measure (true, m, e);
                std::cerr << "A " << aNames[m] << " exc " << e << ": peak " << juce::Decibels::gainToDecibels (pk) << " dB" << std::endl;
                std::cout << (e ? ", " : "") << juce::String (cz::calib::resonator[m][e] * 0.5f / juce::jmax (1.0e-6f, pk), 6) << "f";
            }
            std::cout << " }";
        }
        std::cout << " };" << std::endl << "inline float impact[12][4] {";
        for (int m = 0; m < 12; ++m)
        {
            std::cout << (m ? ", " : " ") << "{ ";
            for (int e = 0; e < 4; ++e)
            {
                const float pk = measure (false, m, e);
                std::cerr << "B " << bNames[m] << " exc " << e << ": peak " << juce::Decibels::gainToDecibels (pk) << " dB" << std::endl;
                std::cout << (e ? ", " : "") << juce::String (cz::calib::impact[m][e] * 0.5f / juce::jmax (1.0e-6f, pk), 6) << "f";
            }
            std::cout << " }";
        }
        std::cout << " };" << std::endl;
        return 0;
    }
    if (argc > 2 && juce::String (argv[2]) == "cpu") // tiempo de cada bloque: picos y si son periódicos
    {
        const double sr = argc > 4 ? juce::String (argv[4]).getDoubleValue() : 96000.0;
        const int bs = argc > 5 ? juce::String (argv[5]).getIntValue() : 64;
        DemoPlayHead ph; ph.bpm = 124.0;
        proc.setPlayHead (&ph);
        proc.setPlayConfigDetails (0, 2, sr, bs);
        proc.loadPreset (findPreset (argc > 3 ? argv[3] : "Init Ring"));
        proc.apvts.getParameter (IDs::midiOut)->setValueNotifyingHost (1.0f);
        proc.prepareToPlay (sr, bs);
        juce::AudioBuffer<float> b (2, bs);
        int spikesWithNote = 0, blocksWithNote = 0;
        const double budget = 1000.0 * bs / sr;
        std::vector<double> t;
        std::vector<int> spikes;
        for (int k = 0; k < (int) (20.0 * sr / bs); ++k)
        {
            juce::MidiBuffer m;
            if (k == 10) for (int n : { 48, 55, 60, 63 }) m.addEvent (juce::MidiMessage::noteOn (1, n, 0.8f), 0);
            const auto t0 = juce::Time::getHighResolutionTicks();
            proc.processBlock (b, m);
            const double ms = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0) * 1000.0;
            ph.ppq += bs * ph.bpm / (60.0 * sr);
            t.push_back (ms);
            bool noteHere = false; for (const auto meta : m) if (meta.getMessage().isNoteOn()) noteHere = true;
            if (noteHere) ++blocksWithNote;
            if (ms > budget * 0.5) { spikes.push_back (k); if (noteHere) ++spikesWithNote; }
        }
        auto sorted = t; std::sort (sorted.begin(), sorted.end());
        std::cout << "CPU " << sr << " Hz / " << bs << ": budget " << juce::String (budget, 3) << " ms, median " << juce::String (sorted[sorted.size() / 2], 3)
                  << ", p99 " << juce::String (sorted[sorted.size() * 99 / 100], 3) << ", max " << juce::String (sorted.back(), 3)
                  << ", blocks over 80%: " << spikes.size() << std::endl;
        juce::String sp; for (size_t i = 0; i < spikes.size() && i < 20; ++i) sp << spikes[i] << " ";
        std::cout << "  spike blocks: " << sp << "  | slow blocks (>50%) with a note start: " << spikesWithNote << " of " << spikes.size()
                  << " (notes start in " << juce::String (100.0 * blocksWithNote / (double) t.size(), 1) << "% of blocks)" << std::endl;
        return 0;
    }
    if (argc > 2 && juce::String (argv[2]) == "fuzz") // estrés: parámetros al azar, presets, notas a mansalva, calidad y frecuencia
    {
        juce::Random rnd (argc > 3 ? juce::String (argv[3]).getIntValue() : 7);
        DemoPlayHead ph; ph.bpm = 128.0;
        proc.setPlayHead (&ph);
        const double rates[] { 44100.0, 48000.0, 96000.0 };
        const int blocks[] { 64, 256, 512, 1000 };
        int problems = 0; double worstMs = 0.0; float worstPeak = 0.0f, worstLimOn = 0.0f; juce::String worstWhat, worstMsWhat;
        const auto& params = proc.getParameters();
        for (int round = 0; round < 24; ++round)
        {
            const double sr = rates[round % 3]; const int bs = blocks[(round / 3) % 4];
            proc.setPlayConfigDetails (0, 2, sr, bs);
            proc.loadPreset (rnd.nextInt ((int) presets::factory().size()));
            if (auto* q = proc.apvts.getParameter (IDs::quality)) q->setValueNotifyingHost (round % 2 ? 1.0f : 0.0f);
            proc.prepareToPlay (sr, bs);
            juce::AudioBuffer<float> b (2, bs);
            const int nBlocks = (int) (3.0 * sr / bs);
            for (int k = 0; k < nBlocks; ++k)
            {
                juce::MidiBuffer m;
                juce::String changed;
                // cada ~50 ms: 3 parámetros al azar (cualquier valor, incluso extremos)
                if (k % juce::jmax (1, (int) (0.05 * sr / bs)) == 0)
                    for (int j = 0; j < 3; ++j)
                    {
                        auto* prm = params[rnd.nextInt (params.size())];
                        if (prm->getName (32).containsIgnoreCase ("quality")) continue;
                        const float v = rnd.nextInt (4) == 0 ? (float) rnd.nextInt (2) : rnd.nextFloat();
                        prm->setValueNotifyingHost (v);
                        changed << prm->getName (20) << "=" << juce::String (v, 2) << " ";
                    }
                // ráfagas de notas, acordes de hasta 20 notas, notas extremas, pitch bend y CC
                if (rnd.nextInt (3) == 0)
                    for (int j = 0, c = 1 + rnd.nextInt (rnd.nextInt (10) == 0 ? 20 : 3); j < c; ++j)
                        m.addEvent (juce::MidiMessage::noteOn (1 + rnd.nextInt (16), rnd.nextInt (128), (juce::uint8) (1 + rnd.nextInt (127))), rnd.nextInt (bs));
                if (rnd.nextInt (3) == 0) m.addEvent (juce::MidiMessage::noteOff (1 + rnd.nextInt (16), rnd.nextInt (128)), rnd.nextInt (bs));
                if (rnd.nextInt (20) == 0) m.addEvent (juce::MidiMessage::pitchWheel (1, rnd.nextInt (16384)), rnd.nextInt (bs));
                if (rnd.nextInt (20) == 0) m.addEvent (juce::MidiMessage::controllerEvent (1, rnd.nextInt (2) ? 1 : 64, rnd.nextInt (128)), rnd.nextInt (bs));
                if (rnd.nextInt (400) == 0) m.addEvent (juce::MidiMessage::allNotesOff (1), 0);
                if (rnd.nextInt (300) == 0 && argc > 4) proc.loadPreset (rnd.nextInt ((int) presets::factory().size()));   // cambio de preset sonando
                if (rnd.nextInt (500) == 0) { proc.undo(); }
                const auto t0 = juce::Time::getHighResolutionTicks();
                proc.processBlock (b, m);
                const double ms = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0) * 1000.0;
                const double budget = 1000.0 * bs / sr;
                if (ms / budget > worstMs) { worstMs = ms / budget; worstMsWhat = juce::String (sr) + " Hz / " + juce::String (bs) + " block " + juce::String (k) + " " + proc.getCurrentPresetName() + " ms " + juce::String (ms, 2) + " notes " + juce::String (m.getNumEvents()) + " changed: " + changed; }
                ph.ppq += bs * ph.bpm / (60.0 * sr);
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < bs; ++i)
                    {
                        const float v = b.getSample (c, i);
                        if (! std::isfinite (v)) { if (problems++ < 5) std::cout << "  NaN/Inf round " << round << " block " << k << " (" << proc.getCurrentPresetName() << ")" << std::endl; c = 2; break; }
                        if (std::abs (v) > worstPeak) { worstPeak = std::abs (v); worstWhat = proc.getCurrentPresetName(); }
                        if (proc.apvts.getRawParameterValue (IDs::limiterOn)->load() > 0.5f) worstLimOn = juce::jmax (worstLimOn, std::abs (v));
                    }
            }
            std::cout << "FUZZ round " << round << "  " << sr << " Hz / " << bs << (round % 2 ? " HIGH" : " ECO") << "  ok" << std::endl;
        }
        std::cout << "FUZZ RESULT: problems " << problems << ", worst block load " << juce::String (worstMs * 100.0, 1)
                  << "% of real time (" << worstMsWhat << "), max sample " << worstPeak << " (" << worstWhat << "), with limiter on " << worstLimOn << std::endl;
        return 0;
    }
    if (argc > 2 && juce::String (argv[2]) == "audit") // busca defectos de audio en todos los presets
    {                                                   // URingDemo <salida> audit [nombre de preset]
        constexpr double tsr = 48000.0;
        constexpr int bs = 512;
        DemoPlayHead ph; ph.bpm = 122.0;
        proc.setPlayHead (&ph);
        proc.setPlayConfigDetails (0, 2, tsr, bs);
        const auto& list = presets::factory();
        const juce::String only = argc > 3 && ! juce::String (argv[3]).startsWith ("--") ? juce::String (argv[3]) : juce::String();
        struct Ev { double t; int note; double dur; float vel; };
        int flaggedTotal = 0;
        for (int k = 0; k < (int) list.size(); ++k)
        {
            const auto& pr = list[(size_t) k];
            if (only.isNotEmpty() && only != pr.name) continue;
            const juce::String cat (pr.category);
            const bool seq = cat == "Sequence" || cat == "Macro" || juce::String (pr.name) == "Mutating Plonks";
            proc.loadPreset (k);
            proc.apvts.getParameter (IDs::seqOn)->setValueNotifyingHost (seq ? 1.0f : 0.0f);
            // aislar etapas: URingDemo <out> audit "<preset>" nospring / nodelay / nofx / eco
            for (int ai = 4; ai < argc; ++ai)
            {
                const juce::String opt (argv[ai]);
                auto off = [&] (const char* id) { proc.apvts.getParameter (id)->setValueNotifyingHost (0.0f); };
                if (opt == "nospring" || opt == "nofx") off (IDs::springOn);
                if (opt == "nodelay" || opt == "nofx") off (IDs::delayOn);
                if (opt == "nofx") { off (IDs::chorusMix); off (IDs::glitchOn); off (IDs::body); off (IDs::sympathy); }
                if (opt == "eco") off (IDs::quality);
                if (opt.contains ("="))   // id=valor (valor real del parámetro)
                    if (auto* prm = proc.apvts.getParameter (opt.upToFirstOccurrenceOf ("=", false, false)))
                        prm->setValueNotifyingHost (prm->convertTo0to1 (opt.fromFirstOccurrenceOf ("=", false, false).getFloatValue()));
            }
            proc.apvts.getParameter (IDs::midiOut)->setValueNotifyingHost (1.0f);   // para saber dónde empieza cada nota del secuenciador
            proc.prepareToPlay (tsr, bs);
            ph.ppq = 0.0;
            cz::VoiceMeter::reset();
            std::vector<int> onsets;   // muestras donde arranca una nota (el ataque no es un click)
            // frase: notas sueltas que se sueltan (para oír el release), un acorde y una nota grave y otra aguda
            std::vector<Ev> ev;
            if (! seq)
            {
                const int base = cat == "Bass" ? 36 : 60;
                const Ev ph1[] { { 0.0, base, 0.25, 0.9f }, { 0.5, base + 7, 0.25, 0.6f }, { 1.0, base + 12, 0.6, 1.0f },
                                 { 2.0, base - 12, 1.2, 0.8f }, { 2.0, base - 5, 1.2, 0.8f }, { 2.0, base + 3, 1.2, 0.8f },
                                 { 3.6, base + 24, 0.3, 0.9f }, { 4.2, base - 24, 0.5, 0.9f } };
                ev.assign (std::begin (ph1), std::end (ph1));
                for (int ai = 4; ai < argc; ++ai)
                    if (juce::String (argv[ai]) == "single") ev = { { 0.0, base, 3.0, 0.9f } };
                for (int ai = 4; ai < argc; ++ai)   // nota sola en un momento dado: at=0.5
                    if (juce::String (argv[ai]).startsWith ("at:")) ev = { { juce::String (argv[ai]).substring (3).getDoubleValue(), base + 7, 0.25, 0.6f } };
            }
            const double secs = 7.0;
            const int total = (int) (secs * tsr);
            juce::AudioBuffer<float> out (2, total), b (2, bs);
            out.clear();
            for (int pos = 0; pos + bs <= total; pos += bs)
            {
                juce::MidiBuffer m;
                for (const auto& e : ev)
                {
                    const int on = (int) (e.t * tsr), off = (int) ((e.t + e.dur) * tsr);
                    if (on >= pos && on < pos + bs) { m.addEvent (juce::MidiMessage::noteOn (1, e.note, e.vel), on - pos); onsets.push_back (on); }
                    if (off >= pos && off < pos + bs) m.addEvent (juce::MidiMessage::noteOff (1, e.note), off - pos);
                }
                proc.processBlock (b, m);
                for (const auto meta : m) if (meta.getMessage().isNoteOn()) onsets.push_back (pos + meta.samplePosition);
                ph.ppq += bs * ph.bpm / (60.0 * tsr);
                for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, b, c, 0, bs);
            }
            for (int ai = 4; ai < argc; ++ai)
                if (juce::String (argv[ai]) == "wav")
                {
                    auto f = outDir.getChildFile ("audit_" + juce::String (pr.name).replaceCharacter (' ', '_') + ".wav"); f.deleteFile();
                    juce::WavAudioFormat wf;
                    std::unique_ptr<juce::AudioFormatWriter> w (wf.createWriterFor (new juce::FileOutputStream (f), tsr, 2, 32, {}, 0));
                    w->writeFromAudioSampleBuffer (out, 0, total);
                }
            juce::StringArray flags;
            // NaN / inf
            bool bad = false;
            for (int c = 0; c < 2 && ! bad; ++c) for (int i = 0; i < total; ++i) if (! std::isfinite (out.getSample (c, i))) { bad = true; break; }
            if (bad) flags.add ("NaN");
            const float peak = out.getMagnitude (0, total);
            // DC: promedio de cada canal
            double dc = 0.0;
            for (int c = 0; c < 2; ++c) { double s = 0.0; for (int i = 0; i < total; ++i) s += out.getSample (c, i); dc = juce::jmax (dc, std::abs (s / total)); }
            if (dc > 0.003) flags.add ("DC " + juce::String (juce::Decibels::gainToDecibels ((float) dc), 0) + " dB");
            // Clics: la 2.ª diferencia salta muy por encima de su nivel local (ventana de 5 ms)
            int clicks = 0;
            {
                const int w = 240;
                std::vector<float> d2 ((size_t) total, 0.0f);
                for (int i = 2; i < total; ++i)
                {
                    const float* x = out.getReadPointer (0);
                    d2[(size_t) i] = std::abs (x[i] - 2.0f * x[i - 1] + x[i - 2]);
                }
                // nivel local = RMS de la 2.ª diferencia en los 5 ms ANTERIORES (sumas acumuladas)
                std::vector<double> acc ((size_t) total + 1, 0.0);
                for (int i = 0; i < total; ++i) acc[(size_t) i + 1] = acc[(size_t) i] + (double) d2[(size_t) i] * d2[(size_t) i];
                for (int i = w + 2; i < total; ++i)
                {
                    const float rmsLocal = (float) std::sqrt ((acc[(size_t) i] - acc[(size_t) (i - w)]) / w);
                    bool atOnset = false;   // ataque de una nota (hasta 4 ms después; algo antes por el humanizado / swing)
                    for (int o : onsets) if (i >= o - 96 && i <= o + 1200) { atOnset = true; break; }   // cuerdas graves: el golpe sale un período después
                    if (! atOnset && d2[(size_t) i] > 0.02f && d2[(size_t) i] > rmsLocal * 9.0f + 1.0e-4f)
                    {
                        ++clicks;
                        if (only.isNotEmpty() && clicks <= 40)
                            std::cout << "  click at " << juce::String (i / tsr, 4) << " s  (sample " << i << ", mod32 " << i % 32 << ", mod512 " << i % 512
                                      << ")  d2 " << d2[(size_t) i] << " local " << rmsLocal << std::endl;
                        i += w;
                    }
                }
            }
            if (clicks > 0) flags.add (juce::String (clicks) + " clicks");
            // Recorte: muestras pegadas al techo del limitador
            int clip = 0;
            for (int c = 0; c < 2; ++c) for (int i = 0; i < total; ++i) if (std::abs (out.getSample (c, i)) > 0.95f) ++clip;
            if (clip > 20) flags.add ("limiter/clip " + juce::String (clip));
            // Estéreo: la suma mono pierde mucho (fase invertida)
            {
                double sl = 0, ss = 0;
                for (int i = 0; i < total; ++i) { const float a = out.getSample (0, i), b2 = out.getSample (1, i); sl += 0.5 * (a * a + b2 * b2); ss += 0.25 * (a + b2) * (a + b2); }
                if (sl > 1.0e-9 && 10.0 * std::log10 (ss / sl) < -6.0) flags.add ("mono loss " + juce::String (10.0 * std::log10 (ss / sl), 1) + " dB");
            }
            // Graves de menos de 25 Hz (retumbe / continua lenta)
            {
                float lp = 0.0f; double sub = 0.0, all = 0.0;
                const float a = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 25.0f / (float) tsr);
                for (int i = 0; i < total; ++i) { const float x = out.getSample (0, i); lp += (x - lp) * a; sub += lp * lp; all += x * x; }
                if (all > 1.0e-9 && 10.0 * std::log10 (sub / all) > -12.0) flags.add ("sub<25Hz " + juce::String (10.0 * std::log10 (sub / all), 1) + " dB");
            }
            // Corte brusco: el nivel cae más de 30 dB de un milisegundo al siguiente estando fuerte
            int cuts = 0;
            {
                const int w = 480;   // picos cada 10 ms (así una onda grave no parece un corte)
                float prev = 0.0f;
                for (int s = 0; s + w <= total; s += w)
                {
                    const float r = out.getMagnitude (0, s, w);
                    if (prev > 0.01f && r < prev * 0.03f)
                    {
                        ++cuts;
                        if (only.isNotEmpty() && cuts <= 10) std::cout << "  cut at " << juce::String (s / tsr, 4) << " s  " << prev << " -> " << r << std::endl;
                    }
                    prev = r;
                }
            }
            if (cuts > 0) flags.add (juce::String (cuts) + " abrupt cuts");
            if (peak < 1.0e-4f) flags.add ("SILENT");
            {
                const float vp = cz::VoiceMeter::peak.load();
                const int sat = cz::VoiceMeter::saturated.load();
                if (vp > 1.0f) flags.add ("VOICE PEAK " + juce::String (juce::Decibels::gainToDecibels (vp), 1) + " dB, " + juce::String (sat) + " saturated");
            }
            if (! flags.isEmpty()) ++flaggedTotal;
            std::cout << (flags.isEmpty() ? "OK   " : "FLAG ") << juce::String (pr.category).paddedRight (' ', 10) << juce::String (pr.name).paddedRight (' ', 22)
                      << "peak " << juce::String (juce::Decibels::gainToDecibels (peak), 1).paddedLeft (' ', 6) << "  " << flags.joinIntoString (" | ") << std::endl;
            if (only.isNotEmpty())
            {
                auto f = outDir.getChildFile ("audit_" + juce::String (pr.name).replaceCharacter (' ', '_') + ".wav"); f.deleteFile();
                juce::WavAudioFormat fmt;
                std::unique_ptr<juce::AudioFormatWriter> wr (fmt.createWriterFor (new juce::FileOutputStream (f), tsr, 2, 24, {}, 0));
                wr->writeFromAudioSampleBuffer (out, 0, total);
            }
        }
        std::cout << "AUDIT: " << flaggedTotal << " presets with something to check" << std::endl;
        return 0;
    }
    if (argc > 2 && juce::String (argv[2]) == "ux") // secuenciador apagado, flechas en la lista, pestañas cierran la lista
    {
        URingAudioProcessor fresh;
        std::cout << "UX seq RUN at startup: " << (fresh.apvts.getRawParameterValue (IDs::seqOn)->load() > 0.5f ? "ON" : "off") << std::endl;
        proc.apvts.getParameter (IDs::seqOn)->setValueNotifyingHost (1.0f);
        proc.loadPreset (findPreset ("Euclid Plonks"));
        std::cout << "UX seq RUN after loading a Sequence preset: " << (proc.apvts.getRawParameterValue (IDs::seqOn)->load() > 0.5f ? "ON" : "off") << std::endl;

        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        auto* ed = dynamic_cast<URingAudioProcessorEditor*> (editor.get());
        editor->setSize (URingAudioProcessorEditor::baseW, URingAudioProcessorEditor::baseH);
        editor->addToDesktop (juce::ComponentPeer::windowHasTitleBar);
        editor->setVisible (true);
        editor->toFront (true);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        ed->openPresetBrowser();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        auto press = [&] (int key)
        {
            if (auto* peer = editor->getPeer()) peer->handleKeyPress (juce::KeyPress (key));
            juce::MessageManager::getInstance()->runDispatchLoopUntil (150);   // el timer del editor también corre
            return proc.getCurrentPresetName();
        };
        const auto start = proc.getCurrentPresetName();
        const auto d1 = press (juce::KeyPress::downKey);
        const auto d2 = press (juce::KeyPress::downKey);
        const auto u1 = press (juce::KeyPress::upKey);
        std::cout << "UX arrows in the list: " << start << " -> " << d1 << " -> " << d2 << " -> (up) " << u1 << std::endl;
        std::cout << "UX list still open after arrows: " << (ed->isPresetBrowserOpen() ? "yes" : "no") << std::endl;
        ed->showTab (1);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        std::cout << "UX after clicking FX: list open = " << (ed->isPresetBrowserOpen() ? "yes" : "no") << std::endl;
        editor->removeFromDesktop();
        return 0;
    }
    if (argc > 2 && juce::String (argv[2]) == "eqbench")
    {
        for (float gdb : { 0.0f, 12.0f, -12.0f })
        {
            cz::MasterBus mb; mb.prepare (48000.0);
            cz::MasterParams mp; mp.low = gdb; mp.limiter = false;
            std::vector<float> l (4800), r (4800);
            for (int i = 0; i < 4800; ++i) l[(size_t) i] = r[(size_t) i] = 0.1f * std::sin (juce::MathConstants<float>::twoPi * 65.0f * (float) i / 48000.0f);
            mb.process (l.data(), r.data(), 4800, mp);
            float mx = 0.0f; for (int i = 2400; i < 4800; ++i) mx = juce::jmax (mx, std::abs (l[(size_t) i]));
            std::cout << "EQBENCH low " << gdb << " dB: 65 Hz sine 0.1 -> " << mx << std::endl;
        }
        return 0;
    }
    if (argc > 2 && juce::String (argv[2]) == "showcase") // audio de las novedades de la 1.4
    {
        constexpr double tsr = 48000.0;
        constexpr int bs = 512;
        DemoPlayHead ph; ph.bpm = 122.0;
        proc.setPlayHead (&ph);
        proc.setPlayConfigDetails (0, 2, tsr, bs);
        auto setP = [&] (const char* id, float v) { auto* prm = proc.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };
        struct Scene { const char* preset; std::function<void()> tweak; bool seq; };
        const std::vector<Scene> scenes {
            { "Glass Machine", [&] { setP (IDs::texOn, 1); setP (IDs::texMix, 0.6f); setP (IDs::texDensity, 0.75f); setP (IDs::texPitch, 12); setP (IDs::plateOn, 1); setP (IDs::plateMix, 0.35f); }, true },
            { "Bassline 7/16", [&] { setP (IDs::fltType, 4); setP (IDs::cutoff, 700); setP (IDs::resonance, 0.75f); setP (IDs::fltEnv, 0.6f); setP (IDs::fltDecay, 180); setP (IDs::masterDrive, 0.4f); }, true },
            { "Euclid Plonks", [&] { setP (IDs::fltType, 5); setP (IDs::cutoff, 220); setP (IDs::resonance, 0.85f); setP ("lfo1Dest", 10); setP ("lfo1Depth", 0.5f); setP ("lfo1Rate", 0.25f); }, true },
            { "Swarm Strings", [&] { setP (IDs::fltType, 6); setP (IDs::resonance, 0.6f); setP ("lfo1Dest", 10); setP ("lfo1Depth", 0.8f); setP ("lfo1Rate", 0.3f); setP ("lfo1Shape", 1); }, false },
            { "Mbira Cycle", [&] { setP (IDs::seqMarbles, 1); setP (IDs::seqDejaVu, 0.45f); setP (IDs::seqMarbLen, 6); setP (IDs::seqSpread, 0.6f); setP (IDs::plateOn, 1); setP (IDs::plateMix, 0.3f); }, true },
            { "Bell Shimmer", [&] { setP (IDs::texOn, 1); setP (IDs::texMode, 3); setP (IDs::texMix, 0.7f); setP (IDs::plateOn, 1); setP (IDs::plateMix, 0.4f); setP (IDs::plateDecay, 0.85f); }, false } };
        juce::AudioBuffer<float> all (2, 0);
        for (const auto& sc : scenes)
        {
            proc.loadPreset (findPreset (sc.preset));
            sc.tweak();
            setP (IDs::seqOn, sc.seq ? 1.0f : 0.0f);
            proc.prepareToPlay (tsr, bs);
            ph.ppq = 0.0;
            const int total = (int) (7.0 * tsr);
            juce::AudioBuffer<float> out (2, total), b (2, bs);
            out.clear();
            for (int pos = 0; pos + bs <= total; pos += bs)
            {
                juce::MidiBuffer m;
                if (! sc.seq)
                {
                    if (pos == 0) for (int n : { 48, 55, 60, 63 }) m.addEvent (juce::MidiMessage::noteOn (1, n, 0.8f), 0);
                    if (pos == (int) (4.5 * tsr) / bs * bs) for (int n : { 48, 55, 60, 63 }) m.addEvent (juce::MidiMessage::noteOff (1, n), 0);
                }
                proc.processBlock (b, m);
                ph.ppq += bs * ph.bpm / (60.0 * tsr);
                for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, b, c, 0, bs);
            }
            out.applyGainRamp (total - 9600, 9600, 1.0f, 0.0f);
            const int old = all.getNumSamples();
            all.setSize (2, old + total, true, true);
            for (int c = 0; c < 2; ++c) all.copyFrom (c, old, out, c, 0, total);
            std::cout << "SCENE " << sc.preset << " peak " << juce::Decibels::gainToDecibels (out.getMagnitude (0, total)) << " dB" << std::endl;
        }
        all.applyGain (juce::Decibels::decibelsToGain (12.0f));
        for (int c = 0; c < 2; ++c) for (int i = 0; i < all.getNumSamples(); ++i) all.setSample (c, i, juce::jlimit (-0.98f, 0.98f, all.getSample (c, i)));
        auto f = outDir.getChildFile ("uring_14_novedades.wav"); f.deleteFile();
        juce::WavAudioFormat fmt;
        std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (new juce::FileOutputStream (f), tsr, 2, 24, {}, 0));
        w->writeFromAudioSampleBuffer (all, 0, all.getNumSamples());
        std::cout << "WAV: " << f.getFullPathName() << std::endl;
        return 0;
    }
    if (argc > 2 && juce::String (argv[2]) == "texbench") // velocidad de TEXTURE por modo
    {
        cz::TextureFx tx;
        tx.prepare (48000.0);
        juce::AudioBuffer<float> b (2, 256);
        juce::Random rr (1);
        for (int mode : { 0, 1, 0, 1, 3, 1 })
        {
            cz::TextureParams tp; tp.mode = mode; tp.mix = 1.0f;
            if (argc > 3) tx.prepare (48000.0);
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            int blocks = 0;
            while (juce::Time::getMillisecondCounterHiRes() - t0 < 3000.0 && blocks < 188)
            {
                for (int c = 0; c < 2; ++c) for (int i = 0; i < 256; ++i) b.setSample (c, i, blocks < 40 ? 0.0f : std::sin (0.05f * (float) (blocks * 256 + i)) * 0.3f);
                tx.process (b.getWritePointer (0), b.getWritePointer (1), 256, tp);
                ++blocks;
            }
            const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
            std::cout << "TEXBENCH mode " << mode << ": " << blocks << " blocks (" << juce::String (blocks * 256 / 48.0, 0) << " ms audio) in "
                      << juce::String (ms, 0) << " ms" << std::endl;
        }
        return 0;
    }
    if (argc > 2 && juce::String (argv[2]) == "v14") // funciones nuevas de la 1.4
    {
        constexpr double tsr = 48000.0;
        constexpr int bs = 256;
        DemoPlayHead ph; ph.bpm = 120.0;
        proc.setPlayHead (&ph);
        proc.setPlayConfigDetails (0, 2, tsr, bs);
        auto setP = [&] (const char* id, float v) { auto* prm = proc.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };
        auto render = [&] (std::vector<std::tuple<double, int, double>> notes, double secs, bool seq = false)
        {
            if (! seq) setP (IDs::seqOn, 0.0f);
            proc.prepareToPlay (tsr, bs);
            ph.ppq = 0.0;
            const int total = (int) (secs * tsr);
            juce::AudioBuffer<float> out (2, total), b (2, bs);
            out.clear();
            for (int pos = 0; pos + bs <= total; pos += bs)
            {
                juce::MidiBuffer m;
                for (auto [t, n, d] : notes)
                {
                    const int on = (int) (t * tsr), off = (int) ((t + d) * tsr);
                    if (on >= pos && on < pos + bs) m.addEvent (juce::MidiMessage::noteOn (1, n, 0.9f), on - pos);
                    if (off >= pos && off < pos + bs) m.addEvent (juce::MidiMessage::noteOff (1, n), off - pos);
                }
                proc.processBlock (b, m);
                ph.ppq += bs * ph.bpm / (60.0 * tsr);
                for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, b, c, 0, bs);
            }
            return out;
        };
        auto finite = [] (const juce::AudioBuffer<float>& x)
        {
            for (int c = 0; c < x.getNumChannels(); ++c)
                for (int i = 0; i < x.getNumSamples(); ++i) if (! std::isfinite (x.getSample (c, i))) return false;
            return true;
        };
        auto db = [] (float g) { return juce::String (juce::Decibels::gainToDecibels (g, -150.0f), 1); };
        auto rms = [] (const juce::AudioBuffer<float>& x, double t0, double t1)
        {
            const int a = (int) (t0 * 48000.0), n = juce::jmin (x.getNumSamples() - a, (int) ((t1 - t0) * 48000.0));
            return std::sqrt (0.5f * (std::pow (x.getRMSLevel (0, a, n), 2.0f) + std::pow (x.getRMSLevel (1, a, n), 2.0f)));
        };

        // 1. Aliasing: nota aguda muy saturada; energía fuera de los armónicos (ECO vs HIGH)
        for (int q : { 0, 1 })
        {
            proc.loadPreset (0);
            setP (IDs::quality, (float) q);
            setP (IDs::aModel, 1); setP (IDs::aBright, 1.0f); setP (IDs::aDamping, 0.1f); setP (IDs::abMix, 0.0f);
            setP (IDs::drive, 1.0f); setP (IDs::delayOn, 0); setP (IDs::springOn, 0); setP (IDs::chorusMix, 0); setP (IDs::volume, 0.0f);
            setP (IDs::variation, 0); setP (IDs::width, 0); setP (IDs::tune, 0);
            proc.clearPatches();
            auto x = render ({ { 0.0, 98, 2.0 } }, 1.0);
            constexpr int order = 14, N = 1 << order;
            juce::dsp::FFT fft (order);
            std::vector<float> data ((size_t) N * 2, 0.0f);
            for (int i = 0; i < N; ++i) data[(size_t) i] = x.getSample (0, 12000 + i) * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) N));
            fft.performFrequencyOnlyForwardTransform (data.data());
            const double f0 = 440.0 * std::pow (2.0, (98 - 69) / 12.0), binHz = tsr / N;
            double harm = 0.0, other = 0.0;
            for (int k = 2; k < N / 2; ++k)
            {
                const double f = k * binHz, h = f / f0, dist = std::abs (h - std::round (h)) * f0;
                const double e = (double) data[(size_t) k] * data[(size_t) k];
                (dist < 4.0 * binHz ? harm : other) += e;
            }
            std::cout << "TEST aliasing " << (q ? "HIGH 2x" : "ECO 1x ") << ": energy off the harmonics " << juce::String (10.0 * std::log10 (other / harm), 1)
                      << " dB  finite " << (finite (x) ? "yes" : "NO") << std::endl;
        }
        setP (IDs::quality, 1);

        // 2. Limitador: preset fuerte con volumen al máximo
        {
            proc.loadPreset (findPreset ("Membrane Bass"));
            setP (IDs::volume, 6.0f); setP (IDs::masterDrive, 0.0f);
            auto x = render ({ { 0.0, 36, 0.4 }, { 0.0, 43, 0.4 }, { 0.5, 48, 0.4 }, { 0.5, 55, 0.4 } }, 1.5);
            int at = 0; float mx = 0.0f;
            for (int c = 0; c < 2; ++c) for (int i = 0; i < x.getNumSamples(); ++i) if (std::abs (x.getSample (c, i)) > mx) { mx = std::abs (x.getSample (c, i)); at = i; }
            std::cout << "TEST limiter ON, volume +6 dB: peak " << db (mx) << " dBFS at sample " << at << " (ceiling -0.3), gain reduction now " << db (proc.getLimiterReduction()) << " dB" << std::endl;
            setP (IDs::limiterOn, 0);
            auto y = render ({ { 0.0, 36, 0.4 }, { 0.0, 43, 0.4 }, { 0.5, 48, 0.4 }, { 0.5, 55, 0.4 } }, 1.5);
            std::cout << "TEST limiter OFF, volume +6 dB: peak " << db (y.getMagnitude (0, y.getNumSamples())) << " dBFS" << std::endl;
        }

        // 3. Filtros nuevos sobre un acorde brillante
        for (auto [name, type] : { std::pair<const char*, int> { "Low Pass", 0 }, { "Ladder 24", 4 }, { "Comb", 5 }, { "Formant", 6 } })
        {
            proc.loadPreset (findPreset ("Init Ring"));
            setP (IDs::fltType, (float) type); setP (IDs::cutoff, 800.0f); setP (IDs::resonance, 0.7f);
            auto x = render ({ { 0.0, 48, 1.0 }, { 0.0, 55, 1.0 }, { 0.0, 64, 1.0 } }, 1.2);
            std::cout << "TEST filter " << juce::String (name).paddedRight (' ', 10) << " rms " << db (rms (x, 0.05, 1.0)) << " dB  peak "
                      << db (x.getMagnitude (0, x.getNumSamples())) << " dB  finite " << (finite (x) ? "yes" : "NO") << std::endl;
        }
        // Barrido del cutoff con resonancia máxima (que no explote)
        {
            proc.loadPreset (findPreset ("Init Ring"));
            for (int type : { 4, 5, 6 })
            {
                setP (IDs::fltType, (float) type); setP (IDs::resonance, 1.0f); setP (IDs::drive, 1.0f);
                setP ("lfo1Dest", 10); setP ("lfo1Depth", 1.0f); setP ("lfo1Rate", 3.0f);
                auto x = render ({ { 0.0, 40, 2.0 }, { 0.3, 76, 1.5 } }, 2.5);
                std::cout << "TEST filter sweep type " << type << " reso 100% drive 100%: peak " << db (x.getMagnitude (0, x.getNumSamples()))
                          << " dB  finite " << (finite (x) ? "yes" : "NO") << std::endl;
            }
        }

        // 4. Plate: cola después de un golpe corto
        {
            proc.loadPreset (findPreset ("Wood Block"));
            setP (IDs::delayOn, 0); setP (IDs::springOn, 0);
            setP (IDs::plateOn, 1); setP (IDs::plateMix, 0.6f); setP (IDs::plateDecay, 0.8f);
            auto x = render ({ { 0.0, 60, 0.05 } }, 4.0);
            setP (IDs::plateOn, 0);
            auto y = render ({ { 0.0, 60, 0.05 } }, 4.0);
            std::cout << "TEST plate: level 1.5-2.0 s after the hit " << db (rms (x, 1.5, 2.0)) << " dB with plate, " << db (rms (y, 1.5, 2.0))
                      << " dB without  finite " << (finite (x) ? "yes" : "NO") << std::endl;
        }

        // 5. TEXTURE: los 4 modos y FREEZE
        for (int mode = 0; mode < 4; ++mode)
        {
            proc.loadPreset (findPreset ("Glass Harmonics"));
            setP (IDs::texOn, 1); setP (IDs::texMode, (float) mode); setP (IDs::texMix, 1.0f);
            auto x = render ({ { 0.0, 60, 0.8 }, { 0.4, 67, 0.8 }, { 0.8, 72, 0.8 } }, 3.0);
            std::cout << "TEST texture mode " << mode << ": wet level 0.5-2.5 s " << db (rms (x, 0.5, 2.5)) << " dB  finite " << (finite (x) ? "yes" : "NO") << std::endl;
        }
        {
            proc.loadPreset (findPreset ("Glass Harmonics"));
            setP (IDs::texOn, 1); setP (IDs::texMix, 1.0f); setP (IDs::delayOn, 0); setP (IDs::springOn, 0);
            setP (IDs::texFreeze, 0); setP (IDs::texPosition, 0.0f); setP (IDs::texSize, 0.3f); setP (IDs::seqOn, 0);
            proc.prepareToPlay (tsr, bs);
            juce::AudioBuffer<float> b (2, bs);
            float late = 0.0f;
            for (int pos = 0, k = 0; pos < (int) (6.0 * tsr); pos += bs, ++k)
            {
                juce::MidiBuffer m;
                if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, 60, 0.9f), 0);
                if (pos == (int) (0.5 * tsr) / bs * bs) m.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
                if (pos == (int) (0.4 * tsr) / bs * bs) setP (IDs::texFreeze, 1);
                proc.processBlock (b, m);
                if (pos > (int) (5.0 * tsr)) late = juce::jmax (late, b.getRMSLevel (0, 0, bs));
            }
            std::cout << "TEST texture FREEZE: level 4 s after the note ended " << db (late) << " dB (should still sound)" << std::endl;
            setP (IDs::texFreeze, 0);
        }

        // 6. MARBLES: con DEJA VU 0.5 el bucle se repite; con 0 cada vuelta cambia
        for (float dv : { 0.5f, 0.0f })
        {
            proc.loadPreset (findPreset ("Euclid Plonks"));
            setP (IDs::seqOn, 1); setP (IDs::seqMarbles, 1); setP (IDs::seqDejaVu, dv); setP (IDs::seqMarbLen, 4); setP (IDs::seqPulses, 16); setP (IDs::seqLength, 16);
            proc.prepareToPlay (tsr, bs);
            ph.ppq = 0.0;
            std::vector<int> notes;
            juce::AudioBuffer<float> b (2, bs);
            for (int pos = 0; pos < (int) (6.0 * tsr); pos += bs)
            {
                juce::MidiBuffer m;
                proc.processBlock (b, m);
                ph.ppq += bs * ph.bpm / (60.0 * tsr);
                for (const auto e : m) if (e.getMessage().isNoteOn()) notes.push_back (e.getMessage().getNoteNumber());
            }
            int repeats = 0, total = 0;
            for (size_t i = 4; i < notes.size(); ++i) { ++total; if (notes[i] == notes[i - 4]) ++repeats; }
            std::set<int> distinct (notes.begin(), notes.end());
            std::cout << "TEST marbles deja vu " << dv << ": " << notes.size() << " notes, " << distinct.size() << " distinct, repeats every 4 steps "
                      << repeats << "/" << total << std::endl;
        }

        // 7. EQ: +12 dB en graves
        {
            proc.loadPreset (findPreset ("Harmonic Low"));
            setP (IDs::volume, -30.0f);
            setP (IDs::eqLow, 0.0f);
            auto x = render ({ { 0.0, 36, 1.0 } }, 1.0);
            setP (IDs::eqLow, 12.0f);
            auto y = render ({ { 0.0, 36, 1.0 } }, 1.0);
            std::cout << "TEST EQ low +12 dB on a 65 Hz note: " << db (rms (x, 0.05, 0.9)) << " -> " << db (rms (y, 0.05, 0.9)) << " dB" << std::endl;
        }
        return 0;
    }
    if (argc > 2 && juce::String (argv[2]) == "levels") // nivel de cada preset con una frase según su categoría
    {                                                    // URingDemo <salida> levels [primer índice para el WAV]
        constexpr double tsr = 48000.0;
        constexpr int bs = 512;
        DemoPlayHead ph; ph.bpm = 122.0;
        proc.setPlayHead (&ph);
        proc.setPlayConfigDetails (0, 2, tsr, bs);
        const int wavFrom = argc > 3 ? juce::String (argv[3]).getIntValue() : 0;
        const auto& list = presets::factory();
        struct Ev { double t; int note; double dur; float vel; };
        auto phraseFor = [] (const juce::String& cat)
        {
            std::vector<Ev> ev;
            static const int bassNotes[] { 36, 36, 43, 39, 36, 41, 43, 34 };
            if (cat == "Bass")
                for (int i = 0; i < 8; ++i) ev.push_back ({ i * 0.5, bassNotes[i], 0.3, i % 2 ? 0.7f : 0.95f });
            else if (cat == "Perc")
                for (int i = 0; i < 12; ++i) ev.push_back ({ i * 0.25 + (i % 2) * 0.03, 48, 0.1, i % 4 == 0 ? 1.0f : 0.65f });
            else if (cat == "Pad" || cat == "Drone")
                for (int n : { 48, 55, 60, 63 }) ev.push_back ({ 0.0, n, 4.5, 0.8f });
            else
            {
                const Ev ph[] { { 0.0, 60, 0.35, 0.7f }, { 0.4, 63, 0.35, 0.75f }, { 0.8, 67, 0.35, 0.8f }, { 1.2, 70, 0.6, 0.9f },
                                { 2.0, 48, 1.4, 0.8f }, { 2.02, 55, 1.4, 0.8f }, { 2.04, 63, 1.4, 0.8f } };
                ev.assign (std::begin (ph), std::end (ph));
            }
            return ev;
        };
        juce::AudioBuffer<float> wav (2, 0);
        for (int k = 0; k < (int) list.size(); ++k)
        {
            const auto& pr = list[(size_t) k];
            for (const auto& [id, v] : pr.values)
                if (proc.apvts.getParameter (id) == nullptr && ! juce::String (id).startsWith ("@")) std::cout << "MISSING PARAM " << id << " in " << pr.name << std::endl;
            const juce::String cat (pr.category);
            const bool seq = cat == "Sequence" || cat == "Macro" || juce::String (pr.name) == "Mutating Plonks";
            proc.loadPreset (k);
            proc.apvts.getParameter (IDs::seqOn)->setValueNotifyingHost (seq ? 1.0f : 0.0f);
            proc.prepareToPlay (tsr, bs);
            ph.ppq = 0.0;
            const auto ev = seq ? std::vector<Ev> {} : phraseFor (cat);
            const double secs = 6.0;
            const int total = (int) (secs * tsr);
            juce::AudioBuffer<float> out (2, total), b (2, bs);
            for (int pos = 0; pos + bs <= total; pos += bs)
            {
                juce::MidiBuffer m;
                for (const auto& e : ev)
                {
                    const int on = (int) (e.t * tsr), off = (int) ((e.t + e.dur) * tsr);
                    if (on >= pos && on < pos + bs) m.addEvent (juce::MidiMessage::noteOn (1, e.note, e.vel), on - pos);
                    if (off >= pos && off < pos + bs) m.addEvent (juce::MidiMessage::noteOff (1, e.note), off - pos);
                }
                proc.processBlock (b, m);
                ph.ppq += bs * ph.bpm / (60.0 * tsr);
                for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, b, c, 0, bs);
            }
            // Nivel "momentáneo máximo": la ventana de 400 ms más fuerte (sirve igual para golpes y pads)
            const int win = (int) (0.4 * tsr);
            double best = 0.0;
            for (int s = 0; s + win <= total; s += win / 4)
            {
                double sum = 0.0;
                for (int c = 0; c < 2; ++c) { const float* d = out.getReadPointer (c, s); for (int i = 0; i < win; ++i) sum += d[i] * d[i]; }
                best = std::max (best, std::sqrt (sum / (2.0 * win)));
            }
            float vol = -6.0f;
            for (const auto& [id, v] : pr.values) if (juce::String (id) == IDs::volume) vol = v;
            const float pk = out.getMagnitude (0, total);
            std::cout << "LEVEL\t" << k << "\t" << pr.category << "\t" << pr.name << "\t" << juce::String (juce::Decibels::gainToDecibels ((float) best, -120.0f), 2)
                      << "\t" << juce::String (juce::Decibels::gainToDecibels (pk, -120.0f), 2) << "\t" << vol << std::endl;
            if (k >= wavFrom)
            {
                const int old = wav.getNumSamples(), len = (int) (5.0 * tsr);
                wav.setSize (2, old + len, true, true);
                for (int c = 0; c < 2; ++c) wav.copyFrom (c, old, out, c, 0, len);
                wav.applyGainRamp (old + len - 2400, 2400, 1.0f, 0.0f);
            }
        }
        wav.applyGain (juce::Decibels::decibelsToGain (14.0f)); // los presets salen a -20 dB: se sube para escucharlos
        auto f = outDir.getChildFile ("uring_presets_nuevos.wav"); f.deleteFile();
        juce::WavAudioFormat fmt;
        std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (new juce::FileOutputStream (f), tsr, 2, 24, {}, 0));
        w->writeFromAudioSampleBuffer (wav, 0, wav.getNumSamples());
        std::cout << "WAV: " << f.getFullPathName() << std::endl;
        return 0;
    }
    if (argc > 2 && juce::String (argv[2]) == "promo") // videos verticales para redes (--en = en inglés)
    {
        bool en = false;
        for (int i = 3; i < argc; ++i) if (juce::String (argv[i]) == "--en") en = true;
        return promo::run (proc, outDir, en);
    }
    if (argc > 2 && (juce::String (argv[2]) == "video15en" || juce::String (argv[2]) == "video15es")) // videos cortos de CCRing 1.5
        return tutorial::run (proc, outDir, true, juce::String (argv[2]) == "video15en", false, true);
    if (argc > 2 && juce::String (argv[2]).startsWith ("tutorial")) // diapositivas del video tutorial ("tutorial3" = versión corta)
        return tutorial::run (proc, outDir, juce::String (argv[2]).startsWith ("tutorial3"), juce::String (argv[2]) == "tutorial3en",
                              juce::String (argv[2]) == "tutorial3es");
    proc.loadPreset (findPreset ("Mbira Ghost"));

    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        editor->setSize (URingAudioProcessorEditor::baseW, URingAudioProcessorEditor::baseH);
        auto* ed = dynamic_cast<URingAudioProcessorEditor*> (editor.get());
        writePng (*editor, outDir.getChildFile ("uring_synth.png"));
        ed->showTab (1);
        writePng (*editor, outDir.getChildFile ("uring_fx.png"));
        proc.loadPreset (findPreset ("Resonant Arp"));
        { auto d = proc.pattern().get (2); d.semis = 1; proc.pattern().set (2, d); }  // una nota fuera de escala para la captura
        for (auto [id, v] : { std::pair<const char*, float> { IDs::seqLength, 32.0f }, { IDs::seqPulses, 13.0f } })
            proc.apvts.getParameter (id)->setValueNotifyingHost (proc.apvts.getParameter (id)->convertTo0to1 (v));
        ed->showTab (2);
        writePng (*editor, outDir.getChildFile ("uring_seq.png"));
    }

    // ---- Audio: 4.5 s por preset con una frase corta (o un acorde largo en los drones)
    struct Demo { const char* preset; bool hold; int steps = 0; float random = 0.0f; };
    const Demo demos[] {
        { "Resonant Arp", false, 64, 0.3f },
        { "Polymeter Steps", false }, { "Random Walker", false },
        { "Euclid Plonks", false }, { "Glass Machine", false }, { "Membrane Groove", false },
        { "Resonant Arp", false }, { "Glitch Euclid", false },
        { "Init Ring", false }, { "Glass Harmonics", false }, { "Sympathetic Sitar", false },
        { "Karplus Pluck", false }, { "Bowed Metal", true }, { "Wood Block", false },
        { "Tuned Membrane", false }, { "Bell Garden", false }, { "Glass Bounce", false },
        { "Ring Into Drum", false }, { "Mbira Ghost", false }, { "Crackle Box", false },
        { "Broken Machine", false }, { "Sympathetic Drone", true }, { "Tape Cathedral", true } };

    constexpr double sr = 48000.0;
    constexpr int block = 256;
    const int perPreset = (int) (4.5 * sr);
    const int numDemos = (int) std::size (demos);

    DemoPlayHead playHead;
    proc.setPlayHead (&playHead);
    proc.setPlayConfigDetails (0, 2, sr, block);
    proc.prepareToPlay (sr, block);

    // Frase: (inicio s, nota, duración s)
    struct NoteEv { double t; int note; double dur; };
    const NoteEv phrase[] { { 0.0, 48, 0.3 }, { 0.4, 55, 0.3 }, { 0.8, 60, 0.3 }, { 1.2, 63, 0.3 },
                            { 1.6, 58, 0.3 }, { 2.0, 51, 0.3 }, { 2.3, 67, 0.2 }, { 2.6, 60, 0.6 } };
    const NoteEv chord[]  { { 0.0, 48, 3.2 }, { 0.05, 55, 3.2 }, { 0.1, 63, 3.2 } };

    juce::AudioBuffer<float> out (2, perPreset * numDemos);
    juce::AudioBuffer<float> buf (2, block);

    for (int d = 0; d < numDemos; ++d)
    {
        proc.loadPreset (findPreset (demos[d].preset));
        auto setParam = [&] (const char* id, float v)
        {
            auto* prm = proc.apvts.getParameter (id);
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
        };
        if (demos[d].steps > 0)
        {
            setParam (IDs::seqLength, (float) demos[d].steps);
            setParam (IDs::seqPulses, (float) demos[d].steps * 9.0f / 16.0f);
            setParam (IDs::seqRandom, demos[d].random);
        }
        // Los presets de secuencia tocan solos: no se les manda la frase
        const bool sequenced = proc.apvts.getRawParameterValue (IDs::seqOn)->load() > 0.5f;
        const NoteEv* evs = demos[d].hold ? chord : phrase;
        const int numEvs = sequenced ? 0 : (demos[d].hold ? (int) std::size (chord) : (int) std::size (phrase));
        double diffSq = 0.0;
        float peak = 0.0f;
        double sumSq = 0.0;

        for (int pos = 0; pos < perPreset; pos += block)
        {
            const int n = std::min (block, perPreset - pos);
            buf.setSize (2, n, false, false, true);
            juce::MidiBuffer midi;
            for (int e = 0; e < numEvs; ++e)
            {
                const int on = (int) (evs[e].t * sr), off = (int) ((evs[e].t + evs[e].dur) * sr);
                if (on >= pos && on < pos + n)   midi.addEvent (juce::MidiMessage::noteOn (1, evs[e].note, (juce::uint8) 100), on - pos);
                if (off >= pos && off < pos + n) midi.addEvent (juce::MidiMessage::noteOff (1, evs[e].note), off - pos);
            }
            proc.processBlock (buf, midi);
            playHead.ppq += n * playHead.bpm / (60.0 * sr);

            for (int ch = 0; ch < 2; ++ch)
                out.copyFrom (ch, d * perPreset + pos, buf, ch, 0, n);
            peak = std::max (peak, buf.getMagnitude (0, 0, n));
            for (int i = 0; i < n; ++i)
            {
                sumSq += buf.getSample (0, i) * buf.getSample (0, i);
                const float side = buf.getSample (0, i) - buf.getSample (1, i);
                diffSq += side * side;
            }
        }

        std::cout << juce::String (demos[d].preset).paddedRight (' ', 18)
                  << " peak " << juce::String (juce::Decibels::gainToDecibels (peak), 1) << " dBFS"
                  << "  rms " << juce::String (juce::Decibels::gainToDecibels ((float) std::sqrt (sumSq / perPreset)), 1)
                  << " dBFS  side " << juce::String (juce::Decibels::gainToDecibels ((float) std::sqrt (diffSq / perPreset)), 1)
                  << " dB" << std::endl;
    }

    // ---- Test 1: MIDI Learn (CC 21 → Cutoff)
    {
        proc.loadPreset (findPreset ("Init Ring"));
        proc.startMidiLearn (IDs::cutoff);
        juce::AudioBuffer<float> b (2, block);
        juce::MidiBuffer m;
        m.addEvent (juce::MidiMessage::controllerEvent (1, 21, 64), 0);
        proc.processBlock (b, m);
        std::cout << "TEST midi learn: mapped CC = " << proc.getMappedCc (IDs::cutoff)
                  << "  event = '" << proc.popLearnEvent() << "'" << std::endl;

        m.clear();
        m.addEvent (juce::MidiMessage::controllerEvent (1, 21, 10), 0);
        proc.processBlock (b, m);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100); // deja correr el timer
        std::cout << "TEST midi learn: CC21=10 -> cutoff = "
                  << proc.apvts.getParameter (IDs::cutoff)->getCurrentValueAsText() << std::endl;
    }

    // ---- Test 3: botón 🙂 con DENSITY baja vs alta (cantidad de golpes)
    for (float dens : { 0.1f, 0.9f })
    {
        proc.loadPreset (findPreset ("Euclid Plonks"));
        proc.apvts.getParameter (IDs::seqOn)->setValueNotifyingHost (1.0f);   // los presets cargan con el SEQ apagado (RUN)
        auto* dp = proc.apvts.getParameter (IDs::seqDensity);
        dp->setValueNotifyingHost (dp->convertTo0to1 (dens));
        for (int k = 0; k < 3; ++k)
            std::cout << "TEST surprise density " << dens << ": " << proc.surprise() << std::endl;
    }

    // ---- Test 4: FIT SCALE (Minor Penta en C: 0 3 5 7 10). Notas fuera de escala → la más cercana
    {
        proc.loadPreset (findPreset ("Euclid Plonks"));
        proc.apvts.getParameter (IDs::seqOn)->setValueNotifyingHost (1.0f);   // los presets cargan con el SEQ apagado (RUN)
        const int raw[] { 1, 2, 4, 6, 8, 9, 11 };
        for (int i = 0; i < 7; ++i) { auto d = proc.pattern().get (i); d.semis = raw[i]; proc.pattern().set (i, d); }
        proc.fitPatternToScale();
        juce::String res;
        for (int i = 0; i < 7; ++i) res << raw[i] << "->" << proc.pattern().get (i).semis << "  ";
        std::cout << "TEST fit scale (minor penta): " << res << std::endl;
    }

    // ---- Test 5: salida MIDI del secuenciador (2 compases) y preset con secuenciador encendido
    {
        proc.loadPreset (findPreset ("Bell Garden"));
        std::cout << "TEST preset loads with SEQ off (RUN starts it): " << (proc.apvts.getRawParameterValue (IDs::seqOn)->load() > 0.5f ? "NO, it is on" : "yes") << std::endl;
        proc.apvts.getParameter (IDs::seqOn)->setValueNotifyingHost (1.0f);
        proc.prepareToPlay (sr, block);
        playHead.ppq = 0.0;
        int ons = 0, offs = 0;
        juce::String first;
        juce::AudioBuffer<float> b (2, block);
        for (int pos = 0; pos < (int) (8.0 * 60.0 / playHead.bpm * sr); pos += block)
        {
            juce::MidiBuffer m;
            proc.processBlock (b, m);
            playHead.ppq += block * playHead.bpm / (60.0 * sr);
            for (const auto meta : m)
            {
                const auto msg = meta.getMessage();
                if (msg.isNoteOn())  { ++ons; if (first.isEmpty()) first = msg.getDescription(); }
                if (msg.isNoteOff()) ++offs;
            }
        }
        std::cout << "TEST midi out: " << ons << " note-on, " << offs << " note-off; first: " << first << std::endl;
    }

    // ---- Test 6: LONG/SHORT y TIME al máximo → duraciones y retrasos de las notas emitidas
    {
        proc.loadPreset (findPreset ("Euclid Plonks"));
        proc.apvts.getParameter (IDs::seqOn)->setValueNotifyingHost (1.0f);   // los presets cargan con el SEQ apagado (RUN)
        for (auto [id, v] : { std::pair<const char*, float> { IDs::seqLongShort, 1.0f }, { IDs::seqRndTime, 1.0f },
                              { IDs::seqPulses, 9.0f } })
            proc.apvts.getParameter (id)->setValueNotifyingHost (proc.apvts.getParameter (id)->convertTo0to1 (v));
        proc.prepareToPlay (sr, block);
        playHead.ppq = 0.0;
        const double samplesPerStep = 0.25 * 60.0 / playHead.bpm * sr;
        std::map<int, double> onAt;
        int shortN = 0, longN = 0, normalN = 0, late = 0, total = 0;
        double maxLen = 0.0;
        juce::AudioBuffer<float> b (2, block);
        double t = 0.0;
        for (int pos = 0; pos < (int) (32.0 * 60.0 / playHead.bpm * sr); pos += block, t += block)
        {
            juce::MidiBuffer m;
            proc.processBlock (b, m);
            playHead.ppq += block * playHead.bpm / (60.0 * sr);
            for (const auto meta : m)
            {
                const auto msg = meta.getMessage();
                const double at = t + meta.samplePosition;
                if (msg.isNoteOn())
                {
                    onAt[msg.getNoteNumber()] = at; ++total;
                    const double frac = std::fmod (at, samplesPerStep) / samplesPerStep;
                    if (frac > 0.05 && frac < 0.95) ++late;
                }
                else if (msg.isNoteOff() && onAt.count (msg.getNoteNumber()))
                {
                    const double steps = (at - onAt[msg.getNoteNumber()]) / samplesPerStep;
                    maxLen = std::max (maxLen, steps);
                    if (steps > 1.2) ++longN; else if (steps < 0.25) ++shortN; else ++normalN;
                    onAt.erase (msg.getNoteNumber());
                }
            }
        }
        std::cout << "TEST long/short: " << longN << " long, " << shortN << " short, " << normalN
                  << " normal (max " << juce::String (maxLen, 2) << " steps); TIME: " << late << "/" << total
                  << " notes off the grid" << std::endl;
    }

    // Helper: corre el secuenciador N compases y devuelve las notas emitidas
    auto runBars = [&] (double bars, std::function<void (int bar)> perBar = {})
    {
        std::vector<std::pair<double, int>> ons; // (paso, nota)
        juce::AudioBuffer<float> b (2, block);
        playHead.ppq = 0.0;
        int lastBar = -1;
        const int total = (int) (bars * 4.0 * 60.0 / playHead.bpm * sr);
        for (int pos = 0; pos < total; pos += block)
        {
            const int bar = (int) (playHead.ppq / 4.0);
            if (bar != lastBar) { lastBar = bar; if (perBar) perBar (bar); }
            juce::MidiBuffer m;
            proc.processBlock (b, m);
            for (const auto meta : m)
                if (meta.getMessage().isNoteOn()
                    && (playHead.ppq + meta.samplePosition * playHead.bpm / (60.0 * sr)) < bars * 4.0 - 0.002)   // dentro de la ventana (el golpe del compás siguiente puede caer medio sample antes por redondeo)
                    ons.push_back ({ (playHead.ppq + meta.samplePosition * playHead.bpm / (60.0 * sr)) / 0.25,
                                     meta.getMessage().getNoteNumber() });
            playHead.ppq += block * playHead.bpm / (60.0 * sr);
        }
        return ons;
    };
    auto setP = [&] (const char* id, float v) { proc.apvts.getParameter (id)->setValueNotifyingHost (proc.apvts.getParameter (id)->convertTo0to1 (v)); };

    // ---- Test 7: ratchets (todos los pasos en x3) y condición 1:2
    {
        proc.loadPreset (findPreset ("Euclid Plonks"));
        proc.apvts.getParameter (IDs::seqOn)->setValueNotifyingHost (1.0f);   // los presets cargan con el SEQ apagado (RUN)
        proc.prepareToPlay (sr, block);
        setP (IDs::seqPulses, 4.0f); setP (IDs::seqRndTime, 0.0f); setP (IDs::seqLongShort, 0.0f);
        for (int i = 0; i < 64; ++i) { auto d = proc.pattern().get (i); d.ratchet = 3; d.cond = 0; d.prob = 1.0f; proc.pattern().set (i, d); }
        const auto r1 = runBars (1.0);
        for (int i = 0; i < 64; ++i) { auto d = proc.pattern().get (i); d.ratchet = 1; d.cond = 1; proc.pattern().set (i, d); }
        const auto r2 = runBars (2.0);
        std::cout << "TEST ratchet x3: " << r1.size() << " notes in 1 bar (4 hits -> expect 12); cond 1:2: "
                  << r2.size() << " notes in 2 bars (expect 4)" << std::endl;
    }

    // ---- Test 8: memorias de patrón — B en cola cambia al empezar el ciclo siguiente
    {
        proc.loadPreset (findPreset ("Euclid Plonks"));
        proc.apvts.getParameter (IDs::seqOn)->setValueNotifyingHost (1.0f);   // los presets cargan con el SEQ apagado (RUN)
        proc.prepareToPlay (sr, block);
        for (int i = 0; i < 64; ++i)
        {
            auto a = proc.pattern().get (i); a.semis = 0; a.octave = 0; a.cond = 0; a.ratchet = 1; a.prob = 1.0f; proc.pattern().set (i, a);
        }
        proc.selectPattern (1); // detenido: cambia ya a B
        for (int i = 0; i < 64; ++i) { auto b = proc.pattern().get (i); b.semis = 7; b.octave = 0; b.cond = 0; b.ratchet = 1; b.prob = 1.0f; proc.pattern().set (i, b); }
        proc.selectPattern (0);
        juce::String seen;
        const auto ons = runBars (3.0, [&] (int bar) { if (bar == 1) proc.selectPattern (1); });
        int lastNote = -1;
        for (const auto& [step, note] : ons)
            if (note != lastNote) { seen << "step " << juce::String (step, 1) << ": note " << note << "  "; lastNote = note; }
        std::cout << "TEST patterns A->B: " << seen << std::endl;
    }

    // ---- Test 9: FREEZE mantiene el sonido después de soltar la nota
    for (bool fr : { false, true })
    {
        proc.loadPreset (findPreset ("Wood Block"));
        setP (IDs::seqOn, 0.0f); setP (IDs::freeze, fr ? 1.0f : 0.0f); setP (IDs::delayOn, 0.0f); setP (IDs::springOn, 0.0f);
        proc.prepareToPlay (sr, block);
        juce::AudioBuffer<float> b (2, block);
        float lvl = 0.0f;
        for (int pos = 0; pos < (int) (3.0 * sr); pos += block)
        {
            juce::MidiBuffer m;
            if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 110), 0);
            if (pos == block * 8) m.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            proc.processBlock (b, m);
            if (pos >= (int) (2.5 * sr)) lvl = std::max (lvl, b.getMagnitude (0, 0, block));
        }
        std::cout << "TEST freeze " << (fr ? "ON " : "OFF") << ": level 2.5 s after release = "
                  << juce::String (juce::Decibels::gainToDecibels (lvl), 1) << " dB" << std::endl;
    }

    // ---- Test 10: cable LFO 1 → CUTOFF cambia el sonido; preset de usuario guarda/carga cables y patrones
    {
        proc.loadPreset (findPreset ("Init Ring"));
        proc.clearPatches();
        proc.addPatch (URingAudioProcessor::SrcLfo1, URingAudioProcessor::DCutoff, -1.0f);
        proc.selectPattern (5);
        proc.setChain ("A B C");
        proc.saveUserPreset ("URing Test");
        proc.loadPreset (findPreset ("Wood Block"));
        const bool loaded = proc.loadUserPreset ("URing Test");
        const auto patches = proc.getPatches();
        std::cout << "TEST user preset: loaded=" << loaded << " cables=" << patches.size()
                  << (patches.size() == 1 ? (patches[0].dest == URingAudioProcessor::DCutoff ? " (LFO1>CUTOFF ok)" : " (wrong)") : "")
                  << " pattern=" << (char) ('A' + proc.getActivePattern()) << " chain='" << proc.getChain() << "'" << std::endl;
        URingAudioProcessor::userPresetFolder().getChildFile ("URing Test.uring").deleteFile();
    }

    // ---- Test 2: cola del spring con DECAY 1 s vs 6 s (nivel 1.5 s después de un golpe)
    for (float decay : { 1.0f, 6.0f })
    {
        proc.loadPreset (findPreset ("Wood Block"));
        for (auto [id, v] : { std::pair<const char*, float> { IDs::springMix, 1.0f }, { IDs::springDecay, decay }, { IDs::delayOn, 0.0f } })
            proc.apvts.getParameter (id)->setValueNotifyingHost (proc.apvts.getParameter (id)->convertTo0to1 (v));
        proc.prepareToPlay (sr, block);
        juce::AudioBuffer<float> b (2, block);
        double tail = 0.0; int count = 0;
        for (int pos = 0; pos < (int) (2.0 * sr); pos += block)
        {
            juce::MidiBuffer m;
            if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 110), 0);
            if (pos == block * 4) m.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            proc.processBlock (b, m);
            if (pos >= (int) (1.5 * sr)) { tail += b.getRMSLevel (0, 0, block); ++count; }
        }
        std::cout << "TEST spring decay " << decay << " s: tail level at 1.5 s = "
                  << juce::String (juce::Decibels::gainToDecibels ((float) (tail / count)), 1) << " dB" << std::endl;
    }

    // ---- Prueba: REC de automatización. Se graba un barrido de CUTOFF y BRIGHTNESS durante el
    //      loop de 2 compases y después se escucha solo (sin tocar nada) + guardado en el estado.
    {
        proc.loadPreset (findPreset ("Mbira Ghost"));
        proc.prepareToPlay (sr, block);
        playHead.ppq = 0.0;
        auto* cutoff = proc.apvts.getParameter (IDs::cutoff);
        auto* bright = proc.apvts.getParameter (IDs::aBright);
        auto* rec = proc.apvts.getParameter (IDs::autoRec);
        auto* recLen = proc.apvts.getParameter (IDs::autoLen);
        recLen->setValueNotifyingHost (recLen->convertTo0to1 (2.0f)); // 2 compases
        rec->setValueNotifyingHost (1.0f);
        const double loopSamples = 8.0 * 60.0 / playHead.bpm * sr; // 2 compases
        juce::AudioBuffer<float> autoOut (2, (int) (loopSamples * 4) + block);
        autoOut.clear();
        juce::AudioBuffer<float> b (2, block);
        int pos = 0;
        auto run = [&] (double samples, std::function<void (double)> each)
        {
            for (double t = 0; t < samples && pos + block <= autoOut.getNumSamples(); t += block, pos += block)
            {
                if (each) each (t / samples);
                juce::MidiBuffer m;
                proc.processBlock (b, m);
                playHead.ppq += block * playHead.bpm / (60.0 * sr);
                for (int ch = 0; ch < 2; ++ch) autoOut.copyFrom (ch, pos, b, ch, 0, block);
            }
        };
        // Vuelta 1: grabar (barrido subiendo y bajando, como mover las perillas a mano)
        cutoff->beginChangeGesture(); bright->beginChangeGesture();
        run (loopSamples, [&] (double x)
        {
            const float tri = (float) (x < 0.5 ? x * 2.0 : 2.0 - x * 2.0);
            cutoff->setValueNotifyingHost (0.25f + 0.6f * tri);
            bright->setValueNotifyingHost (0.9f - 0.7f * tri);
        });
        cutoff->endChangeGesture(); bright->endChangeGesture();
        rec->setValueNotifyingHost (0.0f);
        cutoff->setValueNotifyingHost (0.25f); // la perilla queda abajo: lo que suena es la grabación
        // Vueltas 2-4: reproducción; se mide el valor automatizado a mitad del loop
        float atQuarter = -1.0f, atHalf = -1.0f, atEnd = -1.0f;
        const int ci = proc.indexOf (IDs::cutoff);
        run (loopSamples * 3, [&] (double x)
        {
            const double inLoop = std::fmod (x * 3.0, 1.0);
            if (atQuarter < 0 && x > 0.4 && inLoop > 0.25) atQuarter = proc.getAutoDisplay01 (ci);
            if (atHalf < 0 && x > 0.4 && inLoop > 0.5) atHalf = proc.getAutoDisplay01 (ci);
            if (atEnd < 0 && x > 0.4 && inLoop > 0.95) atEnd = proc.getAutoDisplay01 (ci);
        });
        juce::String names;
        for (int i : proc.getAutomatedParams()) names << proc.getParamDisplayName (i) << ", ";
        std::cout << "TEST automation: recorded [" << names << "] playback cutoff (0-1) at 1/4 " << juce::String (atQuarter, 2)
                  << " (expect ~0.55), 1/2 " << juce::String (atHalf, 2) << " (expect ~0.85), end " << juce::String (atEnd, 2)
                  << " (expect ~0.27); knob left at 0.25" << std::endl;

        juce::MemoryBlock mb;
        proc.getStateInformation (mb);
        proc.clearAllAutomation();
        proc.setStateInformation (mb.getData(), (int) mb.getSize());
        std::cout << "TEST automation saved with the project: " << proc.getAutomatedParams().size() << " knobs (expect 2)" << std::endl;

        const auto f = outDir.getChildFile ("uring_automation.wav");
        f.deleteFile();
        juce::WavAudioFormat fmt;
        if (std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0)); w != nullptr)
            w->writeFromAudioSampleBuffer (autoOut, 0, pos);
        std::cout << "WAV: " << f.getFullPathName() << "  (loop 1 = recording the knob moves, loops 2-4 = playback)" << std::endl;

        // Captura: REC encendido y perillas automatizadas en cian
        rec->setValueNotifyingHost (1.0f);
        run (block * 8, {});
        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        editor->setSize (URingAudioProcessorEditor::baseW, URingAudioProcessorEditor::baseH);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (400);
        writePng (*editor, outDir.getChildFile ("uring_rec.png"));
        rec->setValueNotifyingHost (0.0f);
    }

    // ---- Prueba: REC en modo SEQ. Se arma a mitad del ciclo: espera el paso 1, graba un ciclo
    //      completo (sea de 8 o de 32 pasos) y se apaga solo.
    for (int steps : { 8, 32 })
    {
        proc.loadPreset (findPreset ("Resonant Arp"));
        proc.prepareToPlay (sr, block);
        auto setP = [&] (const char* id, float v) { auto* prm = proc.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };
        setP (IDs::seqLength, (float) steps);
        setP (IDs::seqDiv, 1.0f);          // 1/16
        setP (IDs::autoLen, 0.0f);         // SEQ
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        const double cycle = steps * 0.25; // en negras
        playHead.ppq = cycle * 0.4;        // REC se aprieta a mitad del ciclo
        setP (IDs::autoRec, 1.0f);
        juce::AudioBuffer<float> b (2, block);
        auto* cut = proc.apvts.getParameter (IDs::cutoff);
        cut->beginChangeGesture();
        juce::String trace; int last = -1;
        for (int n = 0; n < (int) (cycle * 2.6 * 60.0 / playHead.bpm * sr / block); ++n)
        {
            cut->setValueNotifyingHost (0.3f + 0.5f * (float) ((n % 40) / 40.0));
            juce::MidiBuffer m;
            proc.processBlock (b, m);
            playHead.ppq += block * playHead.bpm / (60.0 * sr);
            const int st = proc.isRecordingAutomation() ? 2 : proc.isAutomationArmed() ? 1 : 0;
            if (st != last) { static const char* nm[] { "OFF", "ARMED", "RECORDING" }; trace << nm[st] << " @step " << juce::String (playHead.ppq * 4.0, 1) << "  "; last = st; }
            juce::MessageManager::getInstance()->runDispatchLoopUntil (1);
        }
        cut->endChangeGesture();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        std::cout << "TEST REC seq " << steps << " steps (pressed at step " << juce::String (steps * 0.4, 1) << "): " << trace
                  << "| REC button now " << (proc.apvts.getRawParameterValue (IDs::autoRec)->load() > 0.5f ? "ON" : "OFF") << std::endl;
        setP (IDs::autoRec, 0.0f);
    }

    // ---- Prueba: dos secuenciadores. SEQ 1 → motor A (canal 15), SEQ 2 → motor B (canal 16), cada uno su dibujo
    {
        proc.loadPreset (findPreset ("Resonant Arp"));
        proc.apvts.getParameter (IDs::seqOn)->setValueNotifyingHost (1.0f);   // los presets cargan con el SEQ apagado (RUN)
        proc.prepareToPlay (sr, block);
        auto setP = [&] (const char* id, float v) { auto* prm = proc.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };
        setP (IDs::seq2On, 1.0f);
        setP (IDs::midiOut, 1.0f);
        // Editar SEQ 2: 7 pasos con 3 golpes (las perillas STEPS/PULSES ahora son de SEQ 2)
        juce::AudioBuffer<float> b (2, block);
        { juce::MidiBuffer m; proc.processBlock (b, m); }
        proc.setEditSequencer (1);
        { juce::MidiBuffer m; proc.processBlock (b, m); }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
        setP (IDs::seqLength, 7.0f);
        setP (IDs::seqPulses, 3.0f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
        playHead.ppq = 0.0;
        juce::AudioBuffer<float> two (2, (int) (4 * 4 * 60.0 / playHead.bpm * sr));
        two.clear();
        int on15 = 0, on16 = 0;
        juce::String rhythm1, rhythm2;
        for (int pos = 0; pos + block <= two.getNumSamples(); pos += block)
        {
            juce::MidiBuffer m;
            proc.processBlock (b, m);
            playHead.ppq += block * playHead.bpm / (60.0 * sr);
            for (int ch = 0; ch < 2; ++ch) two.copyFrom (ch, pos, b, ch, 0, block);
            for (const auto meta : m)
                if (meta.getMessage().isNoteOn())
                {
                    const int st = juce::roundToInt ((playHead.ppq - block * playHead.bpm / (60.0 * sr)) * 4.0);
                    if (meta.getMessage().getChannel() == 15) { ++on15; if (st < 16) rhythm1 << st << " "; }
                    if (meta.getMessage().getChannel() == 16) { ++on16; if (st < 16) rhythm2 << st << " "; }
                }
        }
        const auto m1 = proc.patternOf (0).getMeta(), m2 = proc.patternOf (1).getMeta();
        std::cout << "TEST two sequencers (4 bars): SEQ 1 > engine A " << on15 << " notes (" << m1.length << " steps / " << m1.pulses
                  << " hits, first bar at steps " << rhythm1 << "), SEQ 2 > engine B " << on16 << " notes (" << m2.length << " steps / "
                  << m2.pulses << " hits, first bar at steps " << rhythm2 << ")" << std::endl;
        juce::MemoryBlock mb;
        proc.getStateInformation (mb);
        proc.loadPreset (0);
        proc.setStateInformation (mb.getData(), (int) mb.getSize());
        std::cout << "TEST two sequencers saved: SEQ 2 " << proc.patternOf (1).getMeta().length << " steps, editing SEQ "
                  << proc.getEditSequencer() + 1 << std::endl;

        const auto f = outDir.getChildFile ("uring_two_seqs.wav");
        f.deleteFile();
        juce::WavAudioFormat fmt;
        if (std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0)); w != nullptr)
            w->writeFromAudioSampleBuffer (two, 0, two.getNumSamples());
        std::cout << "WAV: " << f.getFullPathName() << std::endl;

        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        editor->setSize (URingAudioProcessorEditor::baseW, URingAudioProcessorEditor::baseH);
        if (auto* ed = dynamic_cast<URingAudioProcessorEditor*> (editor.get())) ed->showTab (2);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        writePng (*editor, outDir.getChildFile ("uring_seq2.png"));
        setP (IDs::seq2On, 0.0f); setP (IDs::midiOut, 0.0f);
        proc.setEditSequencer (0);
    }

    // ---- Audio: presets ACOUSTIC tocados como un teclado (secuenciador apagado), con dinámica:
    //      frase suave, frase fuerte y un acorde sostenido que se suelta (se oye el apagador)
    {
        const char* names[] { "Felt Upright", "Grand Room", "Broken Grand", "Tine Keys", "Tine Keys Bark", "Reed Keys",
                              "Marimba Rosewood", "Vibes", "Thumb Piano Box", "Steel Pan", "Nylon Harp", "Cello Body",
                              "Hand Drum", "Bronze Gongs" };
        struct Ev { double t; int note; double dur; float vel; };
        const Ev phrase[] { { 0.0, 57, 0.35, 0.35f }, { 0.4, 60, 0.35, 0.4f }, { 0.8, 64, 0.35, 0.45f }, { 1.2, 67, 0.6, 0.5f },
                            { 2.0, 57, 0.35, 0.95f }, { 2.4, 60, 0.35, 0.9f }, { 2.8, 64, 0.35, 0.95f }, { 3.2, 69, 0.6, 1.0f },
                            { 4.0, 45, 1.4, 0.7f }, { 4.02, 52, 1.4, 0.7f }, { 4.04, 60, 1.4, 0.7f }, { 4.06, 64, 1.4, 0.7f } };
        const double each = 6.5;
        const int nP = (int) std::size (names);
        juce::AudioBuffer<float> ac (2, (int) (each * sr) * nP);
        ac.clear();
        for (int k = 0; k < nP; ++k)
        {
            proc.loadPreset (findPreset (names[k]));
            proc.apvts.getParameter (IDs::seqOn)->setValueNotifyingHost (0.0f);
            proc.prepareToPlay (sr, block);
            juce::AudioBuffer<float> b (2, block);
            float peak = 0.0f; double sum = 0.0;
            const int total = (int) (each * sr);
            for (int pos = 0; pos + block <= total; pos += block)
            {
                juce::MidiBuffer m;
                for (const auto& e : phrase)
                {
                    const int on = (int) (e.t * sr), off = (int) ((e.t + e.dur) * sr);
                    if (on >= pos && on < pos + block) m.addEvent (juce::MidiMessage::noteOn (1, e.note, e.vel), on - pos);
                    if (off >= pos && off < pos + block) m.addEvent (juce::MidiMessage::noteOff (1, e.note), off - pos);
                }
                proc.processBlock (b, m);
                for (int ch = 0; ch < 2; ++ch) ac.copyFrom (ch, k * total + pos, b, ch, 0, block);
                peak = juce::jmax (peak, b.getMagnitude (0, block));
                sum += b.getRMSLevel (0, 0, block);
            }
            // Dinámica: nivel de la frase suave vs la fuerte
            auto rmsAt = [&] (double t0, double t1) { return juce::Decibels::gainToDecibels (ac.getRMSLevel (0, k * total + (int) (t0 * sr), (int) ((t1 - t0) * sr)), -100.0f); };
            std::cout << "ACOUSTIC " << juce::String (names[k]).paddedRight (' ', 17) << " peak " << juce::String (juce::Decibels::gainToDecibels (peak), 1)
                      << " dBFS | soft " << juce::String (rmsAt (0.0, 1.8), 1) << " dB, hard " << juce::String (rmsAt (2.0, 3.8), 1)
                      << " dB | 0.4 s after chord release " << juce::String (rmsAt (5.85, 6.3), 1) << " dB" << std::endl;
        }
        // Respuesta a la velocidad: una nota sola, suave y fuerte
        for (const char* nm : { "Felt Upright", "Grand Room", "Tine Keys", "Marimba Rosewood", "Vibes", "Thumb Piano Box", "Steel Pan", "Hand Drum", "Bronze Gongs", "Nylon Harp" })
        {
            juce::String line;
            for (float v : { 0.25f, 0.6f, 1.0f })
            {
                proc.loadPreset (findPreset (nm));
                proc.apvts.getParameter (IDs::seqOn)->setValueNotifyingHost (0.0f);
                proc.prepareToPlay (sr, block);
                juce::AudioBuffer<float> b (2, block);
                float pk = 0.0f; double hfSum = 0.0, sum = 0.0; float prev = 0.0f;
                for (int pos = 0; pos < (int) (0.6 * sr); pos += block)
                {
                    juce::MidiBuffer m;
                    if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, 60, v), 0);
                    proc.processBlock (b, m);
                    pk = juce::jmax (pk, b.getMagnitude (0, 0, block));
                    for (int i = 0; i < block; ++i) { const float x = b.getSample (0, i); sum += x * x; hfSum += (x - prev) * (x - prev); prev = x; }
                }
                line << "vel " << juce::String (v, 2) << ": " << juce::String (juce::Decibels::gainToDecibels (pk), 1) << " dB (bright "
                     << juce::String (std::sqrt (hfSum / juce::jmax (1.0e-12, sum)), 2) << ")  ";
            }
            std::cout << "VELOCITY " << juce::String (nm).paddedRight (' ', 17) << line << std::endl;
        }
        const auto f = outDir.getChildFile ("uring_acoustic.wav");
        f.deleteFile();
        juce::WavAudioFormat fmt;
        if (std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0)); w != nullptr)
            w->writeFromAudioSampleBuffer (ac, 0, ac.getNumSamples());
        std::cout << "WAV: " << f.getFullPathName() << std::endl;
    }

    // ---- Prueba: cambiar de preset deja VOLUME en -20 dB; los presets no guardan lo que se toca
    {
        auto vol = [&] { return proc.apvts.getRawParameterValue (IDs::volume)->load(); };
        auto cut = [&] { return proc.apvts.getRawParameterValue (IDs::cutoff)->load(); };
        proc.loadPreset (findPreset ("Felt Upright"));
        const float v1 = vol(), c1 = cut();
        const bool dirty0 = proc.isPresetModified();
        auto* prm = proc.apvts.getParameter (IDs::cutoff);
        prm->beginChangeGesture(); prm->setValueNotifyingHost (0.9f); prm->endChangeGesture();
        auto* pv = proc.apvts.getParameter (IDs::volume);
        pv->setValueNotifyingHost (pv->convertTo0to1 (0.0f));
        const bool dirty1 = proc.isPresetModified();
        {
            std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
            editor->setSize (URingAudioProcessorEditor::baseW, URingAudioProcessorEditor::baseH);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
            writePng (*editor, outDir.getChildFile ("uring_modified.png"));
        }
        proc.loadPreset (findPreset ("Marimba Rosewood"));
        const float v2 = vol();
        proc.loadPreset (findPreset ("Felt Upright"));
        std::cout << "TEST preset volume: Felt Upright " << v1 << " dB, Marimba " << v2 << " dB, back to Felt " << vol()
                  << " dB (expect -20 always)" << std::endl;
        std::cout << "TEST preset not saved: cutoff " << juce::String (c1, 0) << " Hz -> tweaked to " << juce::String (prm->convertFrom0to1 (0.9f), 0)
                  << " Hz -> reload: " << juce::String (cut(), 0) << " Hz; modified flag: after load " << (dirty0 ? "yes" : "no")
                  << ", after tweak " << (dirty1 ? "yes" : "no") << ", after reload " << (proc.isPresetModified() ? "yes" : "no") << std::endl;
        // Preset de usuario: guarda el volumen relativo y al cargar la perilla vuelve a -20
        pv->setValueNotifyingHost (pv->convertTo0to1 (-14.0f));
        proc.saveUserPreset ("URing Test Volume");
        proc.loadPreset (0);
        proc.loadUserPreset ("URing Test Volume");
        juce::MemoryBlock st; proc.getStateInformation (st);
        const auto trim = juce::ValueTree::fromXml (*juce::AudioProcessor::getXmlFromBinary (st.getData(), (int) st.getSize())).getProperty ("presetTrim").toString();
        float feltVol = -6.0f;
        for (const auto& fp : presets::factory()) if (juce::String (fp.name) == "Felt Upright") for (const auto& [id, v] : fp.values) if (juce::String (id) == IDs::volume) feltVol = v;
        std::cout << "TEST user preset: knob " << vol() << " dB (expect -20), preset trim " << trim << " dB (Felt Upright trim " << feltVol + 6.0f
                  << " + knob -14 over -20 -> expect " << feltVol + 12.0f << ")" << std::endl;
        URingAudioProcessor::userPresetFolder().getChildFile ("URing Test Volume.uring").deleteFile();
    }

    // ---- Pruebas de las funciones nuevas (LIVE)
    {
        auto setP = [&] (const char* id, float v) { auto* prm = proc.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };
        auto dB = [] (double x) { return juce::String (juce::Decibels::gainToDecibels ((float) x, -120.0f), 1); };
        // Toca una nota (o nada) y devuelve el RMS entre t0 y t1 s
        auto noteLevel = [&] (int note, float vel, double holdS, double t0, double t1, bool useSeq = false)
        {
            if (! useSeq) setP (IDs::seqOn, 0.0f);
            proc.prepareToPlay (sr, block);
            juce::AudioBuffer<float> b (2, block);
            double sum = 0.0; int cnt = 0;
            for (int pos = 0; pos < (int) (t1 * sr); pos += block)
            {
                juce::MidiBuffer m;
                if (note >= 0 && pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, note, vel), 0);
                if (note >= 0 && pos <= (int) (holdS * sr) && pos + block > (int) (holdS * sr)) m.addEvent (juce::MidiMessage::noteOff (1, note), 0);
                proc.processBlock (b, m);
                if (pos >= (int) (t0 * sr)) { sum += b.getRMSLevel (0, 0, block) + b.getRMSLevel (1, 0, block); cnt += 2; }
            }
            return cnt > 0 ? sum / cnt : 0.0;
        };

        // PEDAL: nota de piano soltada a los 0,3 s; nivel a 1,0-1,5 s
        proc.loadPreset (findPreset ("Felt Upright"));
        const auto noPed = noteLevel (48, 0.8f, 0.3, 1.0, 1.5);
        proc.loadPreset (findPreset ("Felt Upright"));
        setP (IDs::pedal, 1.0f);
        const auto ped = noteLevel (48, 0.8f, 0.3, 1.0, 1.5);
        setP (IDs::sympathy, 0.8f);
        const auto pedSym = noteLevel (48, 0.8f, 0.3, 1.0, 1.5);
        std::cout << "TEST pedal (note released at 0.3 s, level 1.0-1.5 s): no pedal " << dB (noPed) << " dB, pedal " << dB (ped)
                  << " dB, pedal + sympathy " << dB (pedSym) << " dB" << std::endl;

        // LIFE: cuerda frotada sostenida, nivel del "aire" entre golpes (sin notas: 0) y con nota a los 2-3 s
        proc.loadPreset (findPreset ("Nylon Harp"));
        setP (IDs::life, 0.0f);
        const auto l0 = noteLevel (60, 0.8f, 3.0, 2.0, 3.0);
        proc.loadPreset (findPreset ("Nylon Harp"));
        setP (IDs::life, 1.0f);
        const auto l1 = noteLevel (60, 0.8f, 3.0, 2.0, 3.0);
        std::cout << "TEST life (harp note held, level 2-3 s): life 0 " << dB (l0) << " dB, life 100% " << dB (l1) << " dB (the string keeps breathing)" << std::endl;

        // CHORUS: correlación L/R (1 = mono, menos = más ancho)
        auto corr = [&] (float mix)
        {
            proc.loadPreset (findPreset ("Tine Keys"));
            setP (IDs::seqOn, 0.0f); setP (IDs::width, 0.0f); setP (IDs::chorusMix, mix);
            proc.prepareToPlay (sr, block);
            juce::AudioBuffer<float> b (2, block);
            double lr = 0, ll = 0, rr = 0;
            for (int pos = 0; pos < (int) (1.5 * sr); pos += block)
            {
                juce::MidiBuffer m;
                if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
                proc.processBlock (b, m);
                for (int i = 0; i < block; ++i) { const double x = b.getSample (0, i), y = b.getSample (1, i); lr += x * y; ll += x * x; rr += y * y; }
            }
            return lr / std::sqrt (ll * rr + 1.0e-20);
        };
        std::cout << "TEST chorus: L/R correlation dry " << juce::String (corr (0.0f), 3) << ", chorus 100% " << juce::String (corr (1.0f), 3) << std::endl;

        // TRÉMOLO (LFO → Volume): profundidad de la modulación sobre una nota de vibráfono
        auto trem = [&] (float depth)
        {
            proc.loadPreset (findPreset ("Vibes Motor"));
            setP (IDs::seqOn, 0.0f); setP ("lfo1Depth", depth);
            setP (IDs::delayOn, 0.0f); setP (IDs::springOn, 0.0f); setP (IDs::variation, 0.0f); // sin ecos que muevan el nivel
            proc.prepareToPlay (sr, block);
            juce::AudioBuffer<float> b (2, block);
            std::vector<double> env;
            for (int pos = 0; pos < (int) (2.0 * sr); pos += block)
            {
                juce::MidiBuffer m;
                if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, 64, 0.8f), 0);
                proc.processBlock (b, m);
                if (pos > (int) (0.3 * sr) && pos < (int) (1.5 * sr)) env.push_back (juce::Decibels::gainToDecibels (b.getRMSLevel (0, 0, block) + 1.0e-9f));
            }
            // se quita la caída natural (recta en dB) y se mide cuánto oscila alrededor: pico a pico
            const double n = (double) env.size();
            double sx = 0, sy = 0, sxx = 0, sxy = 0;
            for (size_t i = 0; i < env.size(); ++i) { sx += i; sy += env[i]; sxx += (double) i * i; sxy += i * env[i]; }
            const double slope = (n * sxy - sx * sy) / (n * sxx - sx * sx), icpt = (sy - slope * sx) / n;
            double lo = 1e9, hi = -1e9;
            for (size_t i = 0; i < env.size(); ++i) { const double r = env[i] - (icpt + slope * i); lo = std::min (lo, r); hi = std::max (hi, r); }
            return (float) (hi - lo);
        };
        std::cout << "TEST tremolo: level swing around the decay (peak to peak), depth 0: " << juce::String (trem (0.0f), 1) << " dB, depth 35%: " << juce::String (trem (0.35f), 1) << " dB" << std::endl;

        // LOCK: un patrón con lock +100% contra -100% en B TONE (brillo de cada golpe)
        auto lockBright = [&] (float lockVal)
        {
            proc.loadPreset (findPreset ("Thumb Piano Box"));
            proc.apvts.getParameter (IDs::seqOn)->setValueNotifyingHost (1.0f);   // los presets cargan con el SEQ apagado (RUN)
            setP (IDs::lockDest, 5.0f); // B Tone
            for (int i = 0; i < cz::SeqPattern::kSteps; ++i) { auto d = proc.patternOf (0).get (i); d.lock = lockVal; proc.patternOf (0).set (i, d); }
            proc.prepareToPlay (sr, block);
            playHead.ppq = 0.0;
            juce::AudioBuffer<float> b (2, block);
            double hf = 0, sum = 0; float prev = 0;
            for (int pos = 0; pos < (int) (3.0 * sr); pos += block)
            {
                juce::MidiBuffer m; proc.processBlock (b, m);
                playHead.ppq += block * playHead.bpm / (60.0 * sr);
                for (int i = 0; i < block; ++i) { const float x = b.getSample (0, i); hf += (x - prev) * (x - prev); sum += x * x; prev = x; }
            }
            return std::sqrt (hf / (sum + 1.0e-20));
        };
        std::cout << "TEST step locks (B Tone): brightness with lock -100% " << juce::String (lockBright (-1.0f), 3)
                  << ", no lock " << juce::String (lockBright (0.0f), 3) << ", +100% " << juce::String (lockBright (1.0f), 3) << std::endl;

        // FOLLOW: SEQ 2 toca la última nota del SEQ 1 + quinta
        {
            proc.loadPreset (findPreset ("Fifths Machine"));
            proc.apvts.getParameter (IDs::seqOn)->setValueNotifyingHost (1.0f);   // los presets cargan con el SEQ apagado (RUN)
            setP (IDs::midiOut, 1.0f);
            proc.prepareToPlay (sr, block);
            playHead.ppq = 0.0;
            juce::AudioBuffer<float> b (2, block);
            int last1 = -1, ok = 0, total = 0;
            for (int pos = 0; pos < (int) (6.0 * sr); pos += block)
            {
                juce::MidiBuffer m; proc.processBlock (b, m);
                playHead.ppq += block * playHead.bpm / (60.0 * sr);
                for (const auto ev : m)
                {
                    const auto mm2 = ev.getMessage();
                    if (! mm2.isNoteOn()) continue;
                    if (mm2.getChannel() == 15) last1 = mm2.getNoteNumber();
                    if (mm2.getChannel() == 16 && last1 >= 0) { ++total; if (mm2.getNoteNumber() == last1 + 7) ++ok; }
                }
            }
            std::cout << "TEST seq 2 follow (+5th): " << ok << " of " << total << " SEQ 2 notes = last SEQ 1 note + 7 semitones" << std::endl;
            setP (IDs::midiOut, 0.0f);
        }

        // MUTATE: cuántas notas cambiaron en 16 vueltas
        {
            proc.loadPreset (findPreset ("Mutating Plonks"));
            proc.apvts.getParameter (IDs::seqOn)->setValueNotifyingHost (1.0f);   // los presets cargan con el SEQ apagado (RUN)
            std::array<int, 64> before {};
            for (int i = 0; i < 64; ++i) before[(size_t) i] = proc.patternOf (0).get (i).semis;
            proc.prepareToPlay (sr, block);
            playHead.ppq = 0.0;
            juce::AudioBuffer<float> b (2, block);
            const double cycleBeats = proc.patternOf (0).getMeta().length * 0.25;
            for (int pos = 0; pos < (int) (16 * cycleBeats * 60.0 / playHead.bpm * sr); pos += block)
            {
                juce::MidiBuffer m; proc.processBlock (b, m);
                playHead.ppq += block * playHead.bpm / (60.0 * sr);
            }
            int changed = 0;
            for (int i = 0; i < 64; ++i) changed += proc.patternOf (0).get (i).semis != before[(size_t) i] ? 1 : 0;
            std::cout << "TEST mutate 60%: " << changed << " notes changed after 16 cycles (random: a step can change twice or back)" << std::endl;
        }

        // MORPH: A = Felt Upright, B = Marimba Rosewood; MORPH 50%
        {
            proc.loadPreset (findPreset ("Felt Upright"));
            proc.storeMorph (0);
            const float abA = proc.apvts.getRawParameterValue (IDs::abMix)->load();
            proc.loadPreset (findPreset ("Marimba Rosewood"));
            proc.storeMorph (1);
            const float abB = proc.apvts.getRawParameterValue (IDs::abMix)->load();
            juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
            setP (IDs::morph, 0.5f);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
            const float abMid = proc.apvts.getRawParameterValue (IDs::abMix)->load();
            const float bodyMid = proc.apvts.getRawParameterValue (IDs::body)->load();
            setP (IDs::morph, 0.0f);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
            std::cout << "TEST morph: A/B mix  A=" << abA << "  B=" << abB << "  morph 50% -> " << abMid << " (body " << juce::String (bodyMid, 3)
                      << ");  back to 0% -> " << proc.apvts.getRawParameterValue (IDs::abMix)->load() << ", model " << proc.apvts.getParameter (IDs::aModel)->getCurrentValueAsText() << std::endl;
        }

        // MACROS: MATERIAL +100% y SPACE +100%
        {
            proc.loadPreset (findPreset ("Thumb Piano Box"));
            const float m0 = proc.getEffective (IDs::bMaterial), s0 = proc.getEffective (IDs::springMix);
            setP (IDs::macro[0], 1.0f); setP (IDs::macro[1], 1.0f);
            juce::AudioBuffer<float> b (2, block); juce::MidiBuffer m; proc.processBlock (b, m);
            std::cout << "TEST macros: MATERIAL +100% -> B material " << juce::String (m0, 2) << " -> " << juce::String (proc.getEffective (IDs::bMaterial), 2)
                      << "; SPACE +100% -> spring mix " << juce::String (s0, 2) << " -> " << juce::String (proc.getEffective (IDs::springMix), 2)
                      << " (knobs untouched: " << juce::String (proc.apvts.getRawParameterValue (IDs::bMaterial)->load(), 2) << ")" << std::endl;
            setP (IDs::macro[0], 0.0f); setP (IDs::macro[1], 0.0f);
        }

        // Audio: los presets LIVE con su secuenciador (8 s cada uno)
        const char* live[] { "Pedal Grand", "Tine Chorus", "Vibes Motor", "Bass Marimba", "Breathing Strings", "Fifths Machine", "Mutating Plonks" };
        const double each = 8.0;
        juce::AudioBuffer<float> lv (2, (int) (each * sr) * 7);
        lv.clear();
        for (int k = 0; k < 7; ++k)
        {
            proc.loadPreset (findPreset (live[k]));
            proc.apvts.getParameter (IDs::seqOn)->setValueNotifyingHost (1.0f);   // desde la 1.4 los presets cargan con RUN apagado
            proc.prepareToPlay (sr, block);
            playHead.ppq = 0.0;
            juce::AudioBuffer<float> b (2, block);
            float pk = 0; double sum = 0; int n = 0;
            for (int pos = 0; pos + block <= (int) (each * sr); pos += block)
            {
                juce::MidiBuffer m; proc.processBlock (b, m);
                playHead.ppq += block * playHead.bpm / (60.0 * sr);
                for (int ch = 0; ch < 2; ++ch) lv.copyFrom (ch, k * (int) (each * sr) + pos, b, ch, 0, block);
                pk = juce::jmax (pk, b.getMagnitude (0, block)); sum += b.getRMSLevel (0, 0, block); ++n;
            }
            std::cout << "LIVE " << juce::String (live[k]).paddedRight (' ', 18) << " peak " << dB (pk) << " dBFS, rms " << dB (sum / n) << " dB" << std::endl;
        }
        const auto f = outDir.getChildFile ("uring_live.wav");
        f.deleteFile();
        juce::WavAudioFormat fmt;
        if (std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0)); w != nullptr)
            w->writeFromAudioSampleBuffer (lv, 0, lv.getNumSamples());
        std::cout << "WAV: " << f.getFullPathName() << std::endl;

        // Capturas: LIVE, FX con CHORUS, SEQ con el carril LOCK
        proc.loadPreset (findPreset ("Mutating Plonks"));
        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        editor->setSize (URingAudioProcessorEditor::baseW, URingAudioProcessorEditor::baseH);
        auto* ed = dynamic_cast<URingAudioProcessorEditor*> (editor.get());
        ed->showTab (3); juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
        writePng (*editor, outDir.getChildFile ("uring_live.png"));
        ed->showTab (1); juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
        writePng (*editor, outDir.getChildFile ("uring_fx.png"));
        ed->showTab (2); juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
        writePng (*editor, outDir.getChildFile ("uring_seq_lock.png"));
    }

    const auto wav = outDir.getChildFile ("uring_demo.wav");
    wav.deleteFile();
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer (format.createWriterFor (new juce::FileOutputStream (wav), sr, 2, 24, {}, 0));
    if (writer == nullptr)
        return 1;
    writer->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
    std::cout << "WAV: " << wav.getFullPathName() << std::endl;
    return 0;
}
