// Videos verticales para redes (1080x1920): cuadros con capturas reales de CCRing + su audio.
// URingDemo <carpeta> promo [--en]  →  promo1_presets / promo2_dado / promo3_mod (slides.txt, slideNN.png, audio.wav)
// Después: make_promo <carpeta del video> <audio.wav> <salida.mp4>
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace promo
{
using namespace juce;
inline String U (const char* s) { return String::fromUTF8 (s); }

struct Head : AudioPlayHead
{
    double ppq = 0.0, bpm = 122.0;
    Optional<PositionInfo> getPosition() const override
    { PositionInfo p; p.setIsPlaying (true); p.setBpm (bpm); p.setPpqPosition (ppq); return p; }
};

// Renderiza `secs` segundos del sonido actual: con secuenciador (si seq) o un acorde corto
inline AudioBuffer<float> renderSound (URingAudioProcessor& proc, double secs, bool seq, int baseNote = 48)
{
    constexpr double sr = 48000.0;
    constexpr int bs = 512;
    static Head ph; ph.ppq = 0.0;
    proc.setPlayHead (&ph);
    proc.setPlayConfigDetails (0, 2, sr, bs);
    proc.apvts.getParameter (IDs::seqOn)->setValueNotifyingHost (seq ? 1.0f : 0.0f);
    proc.prepareToPlay (sr, bs);
    AudioBuffer<float> out (2, (int) (secs * sr)), b (2, bs);
    out.clear();
    const int notes[] { baseNote, baseNote + 7, baseNote + 15 };
    for (int pos = 0; pos + bs <= out.getNumSamples(); pos += bs)
    {
        MidiBuffer m;
        if (! seq)
        {
            const int on = 0, off = (int) (secs * 0.55 * sr);
            for (int k = 0; k < 3; ++k)
            {
                const int t = on + k * (int) (0.06 * sr);
                if (t >= pos && t < pos + bs) m.addEvent (MidiMessage::noteOn (1, notes[k], 0.85f), t - pos);
                if (off >= pos && off < pos + bs) m.addEvent (MidiMessage::noteOff (1, notes[k]), off - pos);
            }
        }
        proc.processBlock (b, m);
        ph.ppq += bs * ph.bpm / (60.0 * sr);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, b, c, 0, bs);
    }
    if (! seq) for (int c = 0; c < 2; ++c) out.applyGainRamp (c, out.getNumSamples() - 2400, 2400, 1.0f, 0.0f);
    proc.apvts.getParameter (IDs::seqOn)->setValueNotifyingHost (0.0f);
    proc.setPlayHead (nullptr);
    return out;
}

inline void append (AudioBuffer<float>& all, const AudioBuffer<float>& x, float gainDb = 14.0f)
{
    const int old = all.getNumSamples();
    all.setSize (2, old + x.getNumSamples(), true, true);
    for (int c = 0; c < 2; ++c) all.copyFrom (c, old, x.getReadPointer (c), x.getNumSamples(), Decibels::decibelsToGain (gainDb));
}

inline AudioBuffer<float> silence (double secs) { AudioBuffer<float> b (2, (int) (secs * 48000.0)); b.clear(); return b; }

inline void writeWav (const AudioBuffer<float>& b, const File& f)
{
    f.deleteFile();
    WavAudioFormat fmt;
    std::unique_ptr<AudioFormatWriter> w (fmt.createWriterFor (new FileOutputStream (f), 48000.0, 2, 24, {}, 0));
    AudioBuffer<float> c (b);
    const float pk = c.getMagnitude (0, c.getNumSamples());
    if (pk > 0.89f) c.applyGain (0.89f / pk);   // nunca recorta
    w->writeFromAudioSampleBuffer (c, 0, c.getNumSamples());
}

inline Image snapshot (URingAudioProcessor& proc, int tab, int seqLane = -1)
{
    std::unique_ptr<AudioProcessorEditor> editor (proc.createEditor());
    editor->setSize (URingAudioProcessorEditor::baseW, URingAudioProcessorEditor::baseH);
    if (auto* ed = dynamic_cast<URingAudioProcessorEditor*> (editor.get()))
    {
        ed->showTab (tab);
        if (seqLane >= 0) ed->testShowSeqLane (seqLane);
    }
    MessageManager::getInstance()->runDispatchLoopUntil (200);
    return editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
}

// Cuadro vertical: logo, título grande, captura (o recorte) del plugin, texto y pie con la descarga
inline Image frame (const Image& shot, Rectangle<int> crop, const String& headline, const String& caption, bool english, Colour accent)
{
    Image img (Image::RGB, 1080, 1920, true);
    Graphics g (img);
    g.fillAll (Colour (0xff0b0c0e));
    static const auto logo = ImageCache::getFromMemory (URingFonts::ccr_logo_png, URingFonts::ccr_logo_pngSize);
    g.drawImage (logo, Rectangle<float> (60.0f, 70.0f, 220.0f, 127.0f), RectanglePlacement::centred);
    g.setColour (Colours::white);
    g.setFont (mm::gui::uiFont (64.0f).withExtraKerningFactor (0.18f));
    g.drawText ("CCRING", 300, 85, 720, 70, Justification::centredLeft);
    g.setColour (Colours::white.withAlpha (0.55f));
    g.setFont (mm::gui::uiFont (24.0f).withExtraKerningFactor (0.25f));
    g.drawText (english ? "FREE PHYSICAL-MODELLING SYNTH" : U ("SINTE DE MODELADO F\xc3\x8dSICO GRATIS"), 304, 150, 720, 34, Justification::centredLeft);

    g.setColour (accent);
    g.fillRect (60, 262, 12, 150);
    g.setColour (Colours::white);
    g.setFont (mm::gui::uiFont (66.0f));
    g.drawFittedText (headline, 96, 252, 930, 170, Justification::centredLeft, 2, 0.8f);

    // captura con borde
    const auto src = crop.isEmpty() ? shot.getBounds() : (crop.toFloat() * 2.0f).toNearestInt();
    const auto sub = shot.getClippedImage (src);
    const float k = jmin (1000.0f / (float) sub.getWidth(), 1000.0f / (float) sub.getHeight());
    const auto area = Rectangle<float> ((float) sub.getWidth() * k, (float) sub.getHeight() * k).withCentre ({ 540.0f, 1000.0f });
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (area.expanded (14.0f).translated (0.0f, 10.0f), 22.0f);
    g.drawImage (sub, area, RectanglePlacement::stretchToFit);
    g.setColour (accent.withAlpha (0.9f));
    g.drawRoundedRectangle (area.expanded (6.0f), 16.0f, 4.0f);

    g.setColour (accent);
    g.setFont (mm::gui::uiFont (54.0f));
    g.drawFittedText (caption, 60, (int) area.getBottom() + 50, 960, 140, Justification::centred, 2, 0.75f);

    g.setColour (Colours::white.withAlpha (0.85f));
    g.setFont (mm::gui::uiFont (30.0f).withExtraKerningFactor (0.12f));
    g.drawText (english ? U ("FREE  \xc2\xb7  MAC & WINDOWS  \xc2\xb7  VST3 / AU") : U ("GRATIS  \xc2\xb7  MAC Y WINDOWS  \xc2\xb7  VST3 / AU"), 0, 1740, 1080, 44, Justification::centred);
    g.setColour (Colours::white.withAlpha (0.55f));
    g.setFont (mm::gui::uiFont (28.0f));
    g.drawText ("github.com/javicazou/CCRing", 0, 1792, 1080, 40, Justification::centred);
    return img;
}

inline Image endCard (bool english)
{
    Image img (Image::RGB, 1080, 1920, true);
    Graphics g (img);
    g.fillAll (Colour (0xff0b0c0e));
    static const auto logo = ImageCache::getFromMemory (URingFonts::ccr_logo_png, URingFonts::ccr_logo_pngSize);
    g.drawImage (logo, Rectangle<float> (140.0f, 420.0f, 800.0f, 462.0f), RectanglePlacement::centred);
    g.setColour (Colours::white);
    g.setFont (mm::gui::uiFont (110.0f).withExtraKerningFactor (0.2f));
    g.drawText ("CCRING", 0, 960, 1080, 130, Justification::centred);
    g.setColour (Colour (0xffff6a2b));
    g.setFont (mm::gui::uiFont (52.0f));
    g.drawText (english ? "FREE DOWNLOAD" : U ("DESCARGA GRATIS"), 0, 1110, 1080, 70, Justification::centred);
    g.setColour (Colours::white.withAlpha (0.85f));
    g.setFont (mm::gui::uiFont (34.0f));
    g.drawText ("github.com/javicazou/CCRing", 0, 1210, 1080, 50, Justification::centred);
    g.setColour (Colours::white.withAlpha (0.55f));
    g.setFont (mm::gui::uiFont (28.0f));
    g.drawText (english ? U ("Mac (Intel & Apple Silicon)  \xc2\xb7  Windows  \xc2\xb7  VST3 / AU") : U ("Mac (Intel y Apple Silicon)  \xc2\xb7  Windows  \xc2\xb7  VST3 / AU"), 0, 1290, 1080, 44, Justification::centred);
    return img;
}

struct Video
{
    File dir; String index; int n = 0; AudioBuffer<float> audio { 2, 0 };
    explicit Video (const File& d) : dir (d) { dir.deleteRecursively(); dir.createDirectory(); }
    void add (const Image& img, double secs)
    {
        const auto name = String::formatted ("slide%02d", n++);
        FileOutputStream os (dir.getChildFile (name + ".png")); PNGImageFormat().writeImageToStream (img, os);
        index << name << "|" << String (secs, 2) << "\n";
    }
    void finish()
    {
        dir.getChildFile ("slides.txt").replaceWithText (index, false, false, "\n");
        writeWav (audio, dir.getChildFile ("audio.wav"));
    }
};

inline int findPresetIdx (const char* name)
{
    const auto& l = presets::factory();
    for (int k = 0; k < (int) l.size(); ++k) if (String (l[(size_t) k].name) == name) return k;
    return 0;
}

inline int run (URingAudioProcessor& proc, const File& outDir, bool english)
{
    mm::lang::currentRef() = english ? mm::lang::Lang::English : mm::lang::Lang::Spanish;
    const String sfx = english ? "_en" : "_es";
    const auto orange = Colour (0xffff6a2b), green = Colour (0xff7be3a5), blue = Colour (0xff7fb2ff);

    // 1. "165 presets": un preset distinto cada 1,6 s
    {
        Video v (outDir.getChildFile ("promo1_presets" + sfx));
        struct P { const char* name; int tab; bool seq; int note; };
        const P list[] { { "Felt Upright", 0, false, 48 }, { "Marimba Rosewood", 0, false, 55 }, { "Plaits Kit Walk", 2, true, 48 },
                         { "Gong Driven", 0, false, 43 }, { "Glass Machine", 2, true, 48 }, { "Chord Resonator", 0, false, 48 },
                         { "Dub Chord Steps", 4, true, 48 }, { "FM Struck String", 0, false, 52 }, { "Model Hopper", 2, true, 48 },
                         { "Granular Plate", 5, false, 48 } };
        for (const auto& p : list)
        {
            proc.loadPreset (findPresetIdx (p.name));
            const auto shot = snapshot (proc, p.tab);
            const auto& pr = presets::factory()[(size_t) findPresetIdx (p.name)];
            v.add (frame (shot, {}, english ? "165 FREE PRESETS" : "165 PRESETS GRATIS",
                          String (p.name).toUpperCase() + "\n" + mm::lang::tr (String (pr.category).toUpperCase()), english, orange), 1.6);
            append (v.audio, renderSound (proc, 1.6, p.seq, p.note));
        }
        v.add (endCard (english), 2.4);
        append (v.audio, silence (2.4));
        v.finish();
    }
    // 2. Dado: 8 sonidos al azar
    {
        Video v (outDir.getChildFile ("promo2_dado" + sfx));
        for (int k = 0; k < 8; ++k)
        {
            proc.loadPreset (findPresetIdx ("Init Ring"));
            const auto what = proc.randomizeSound (URingAudioProcessor::RandAll, false);
            const auto shot = snapshot (proc, 0);
            v.add (frame (shot, { 0, 0, 1240, 400 }, english ? "ONE CLICK = A NEW SOUND" : "UN CLIC = UN SONIDO NUEVO",
                          (english ? "ROLL " : "TIRADA ") + String (k + 1) + "\n" + what, english, blue), 2.0);
            append (v.audio, renderSound (proc, 2.0, false, 48 + (k % 3) * 5));
        }
        v.add (endCard (english), 2.4);
        append (v.audio, silence (2.4));
        v.finish();
    }
    // 3. Carril MOD: una batería entera desde un secuenciador
    {
        Video v (outDir.getChildFile ("promo3_mod" + sfx));
        proc.loadPreset (findPresetIdx ("Plaits Kit Walk"));
        proc.setEditSequencer (0);
        const auto seqShot = snapshot (proc, 2, (int) cz::Lane::Model);
        const auto cShot = snapshot (proc, 4);
        const String head = english ? "A WHOLE DRUM KIT FROM ONE SEQUENCER" : U ("UNA BATER\xc3\x8d" "A ENTERA DESDE UN SECUENCIADOR");
        v.add (frame (seqShot, { 16, 346, 1208, 450 }, head, english ? "MOD LANE: A DIFFERENT MODEL ON EVERY STEP" : "CARRIL MOD: OTRO MODELO EN CADA PASO", english, green), 4.0);
        v.add (frame (cShot, { 16, 70, 680, 520 }, head, english ? U ("KICK  \xc2\xb7  SNARE  \xc2\xb7  HI-HAT  \xc2\xb7  24 MODELS") : U ("BOMBO  \xc2\xb7  REDOBLANTE  \xc2\xb7  HI-HAT  \xc2\xb7  24 MODELOS"), english, green), 4.0);
        v.add (frame (seqShot, {}, head, english ? "PLAITS KIT WALK" : "PLAITS KIT WALK", english, green), 4.0);
        append (v.audio, renderSound (proc, 12.0, true));
        proc.loadPreset (findPresetIdx ("Model Hopper"));
        const auto hop = snapshot (proc, 2, (int) cz::Lane::Model);
        v.add (frame (hop, {}, head, "MODEL HOPPER", english, green), 4.0);
        append (v.audio, renderSound (proc, 4.0, true));
        v.add (endCard (english), 2.4);
        append (v.audio, silence (2.4));
        v.finish();
    }
    std::cout << "Promo: 3 videos in " << outDir.getFullPathName() << std::endl;
    return 0;
}
} // namespace promo
