#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "Engines.h"
#include "Modules.h"
#include "Envelope.h"
#include "MacroEngine.h"

namespace cz
{

// Valores ya modulados que el procesador entrega a todas las voces en cada sub-bloque
struct VoiceParams
{
    ResParams a;
    ImpParams b;
    float aPitch = 0.0f, bPitch = 0.0f, tune = 0.0f, pitchMod = 0.0f;
    float abMix = 0.5f, xfeed = 0.0f;
    float variation = 0.0f;   // cada golpe varía timbre, afinación y dinámica
    float width = 0.0f;       // reparto estéreo de voces y motores

    FilterType filterType = FilterType::LowPass;
    float cutoff = 8000.0f, resonance = 0.2f, fltEnvAmt = 0.0f, fltEnvDecayMs = 300.0f, keyTrack = 0.5f, drive = 0.0f;

    float attack = 1.0f, decay = 2000.0f, sustain = 1.0f, release = 600.0f;
    float velSens = 0.6f;
    bool freeze = false; // congela la resonancia: las notas no se apagan hasta soltar FREEZE

    // LIFE: el instrumento "respira": un soplo muy leve mantiene vivo el resonador y la afinación
    // deriva unos cents (cada voz por su lado)
    float life = 0.0f;

    // Parameter locks del secuenciador: valor -1..1 por nota (lo escribe el SEQ al tocarla) y a
    // qué parámetro va (LOCK DEST). Cada golpe conserva su timbre mientras suena.
    enum LockDest { LockABright = 0, LockADamping, LockAPosition, LockAStructure, LockBMaterial, LockBTone, LockBDecay, LockCutoff,
                    LockCHarmonics, LockCTimbre, LockCMorph };
    int lockDest = LockABright;
    std::array<std::array<float, 128>, 2> noteLock {}; // [SEQ 1 / SEQ 2][nota]
    // Carril MODEL: modelo del motor C de cada nota del secuenciador (-1 = el de la perilla)
    std::array<std::array<int, 128>, 2> noteModel = []
    {
        std::array<std::array<int, 128>, 2> t {};
        for (auto& row : t) row.fill (-1);
        return t;
    }();

    // MOTOR C (MACRO, Plaits). cMix: 0 = solo A/B, 1 = solo C. cFeed: C excita al resonador A.
    // seq2Target: con dos secuenciadores, el SEQ 2 toca 0 = B, 1 = C, 2 = B + C (C va con el SEQ 1 si no)
    MacroParams c;
    float cPitch = 0.0f, cMix = 0.0f, cFeed = 0.0f;
    float cFeedB = 0.0f;   // C > B FEED: el motor C hace vibrar al objeto del motor B
    int diceMode = 0;
    float diceTone = 0.0f, diceDecay = 0.0f;   // TONO (semitonos) y CAÍDA (-1..1) sobre los sonidos del dado, en vivo      // cara del dado: 0 sorpresa, 1 percusión, 2 melodía, 3 atmósfera, 4 contraste, 5 mutación
    bool diceNote = false; // DADO EN CADA NOTA: cada nota elige sus propios modelos y timbre al azar
    int seq2Target = 0;
};

// Qué motores toca una nota según su canal: 15 = solo A (SEQ 1 con SEQ 2 encendido),
// 16 = solo B (SEQ 2), cualquier otro = los dos (teclado, MIDI del DAW, SEQ 1 solo)
struct CzSound : public juce::SynthesiserSound
{
    explicit CzSound (int engineMask = 3) : mask (engineMask) {}
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int ch) override { return mask == 1 ? ch == 15 : mask == 2 ? ch == 16 : (ch != 15 && ch != 16); }
    const int mask;
};

// Medición (solo para las pruebas): cuánto entra a la saturación final de cada voz
struct VoiceMeter
{
    static inline std::atomic<float> peak { 0.0f };
    static inline std::atomic<int> saturated { 0 };   // muestras por encima de 1.5 (recorte audible)
    static void reset() { peak = 0.0f; saturated = 0; }
};

class CzVoice : public juce::SynthesiserVoice
{
public:
    explicit CzVoice (const VoiceParams& shared) : vp (shared) {}

    void prepare (double sr)
    {
        sampleRate = (float) sr;
        engA.prepare (sr);
        engB.prepare (sr);
        filterA.prepare (sr);
        filterB.prepare (sr);
        filterCL.prepare (sr);
        filterCR.prepare (sr);
        macro.prepare (sr);
        env.prepare (sr);
        dcL.prepare (sr); dcR.prepare (sr);
    }

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<CzSound*> (s) != nullptr; }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound* sound, int pitchWheel) override
    {
        note = (float) midiNote;
        engineMask = sound != nullptr ? static_cast<CzSound*> (sound)->mask : 3;
        // Lock del paso que disparó esta nota (canal 14/15 = SEQ 1, 16 = SEQ 2; el teclado no tiene)
        const int src = isPlayingChannel (16) ? 1 : (isPlayingChannel (15) || isPlayingChannel (14)) ? 0 : -1;
        lock = src >= 0 ? vp.noteLock[(size_t) src][(size_t) juce::jlimit (0, 127, midiNote)] : 0.0f;
        lockDest = vp.lockDest;
        modelOverride = src >= 0 ? vp.noteModel[(size_t) src][(size_t) juce::jlimit (0, 127, midiNote)] : -1;
        drift = 0.0f;
        airNoise.seed ((uint32_t) seedRng.nextInt() | 1u);
        macro.noteOn ((float) midiNote);
        heldByFreeze = false;
        excAgc = 1.0f; aFollow = 0.0f;

        // VARIATION: desvíos aleatorios fijos para este golpe (bipolares, -1..1)
        for (auto& o : offsets) o = seedRng.nextFloat() * 2.0f - 1.0f;
        // DADO EN CADA NOTA: esta nota sortea su propio instrumento (no toca a las que ya suenan)
        diced = vp.diceNote;
        if (diced)
        {
            // Azar de verdad (no se repite al darle Play). Cada nota sale de una FAMILIA de sonido
            // distinta a las dos anteriores (punteado, campana, pad, batería, sinte, madera, textura) y
            // la familia decide todo: qué motor suena, la envolvente, el filtro y la octava.
            auto& r = diceRng;
            auto& last = lastDice();
            auto u = [&r] (float lo, float hi) { return lo + (hi - lo) * r.nextFloat(); };
            auto pick = [&r] (std::initializer_list<int> l) { return *(l.begin() + r.nextInt ((int) l.size())); };
            // CARA DEL DADO: cada cara es otra lógica para elegir la familia de la nota
            // familias: 0 punteado, 1 campana, 2 pad, 3 batería, 4 sinte, 5 madera, 6 textura
            const int mode = vp.diceMode;
            auto fromSet = [&] (std::initializer_list<int> set)
            {
                // solo evita repetir la anterior: en grupos chicos evitar más armaría un ciclo fijo
                int f = pick (set);
                for (int k = 0; k < 8 && f == last.fam; ++k) f = pick (set);
                return f;
            };
            int fam = 0;
            bool mutate = false;
            switch (mode)
            {
                case 1:  fam = fromSet ({ 3, 5, 0, 3 }); break;                       // PERCUSIÓN: golpes cortos
                case 2:  fam = fromSet ({ 0, 1, 4, 5 }); break;                       // MELODÍA: todo con nota
                case 3:  fam = fromSet ({ 2, 6, 1 }); break;                          // ATMÓSFERA: largo y lento
                case 4:  last.flip = ! last.flip;                                     // CONTRASTE: corto, largo, corto...
                         fam = last.flip ? fromSet ({ 3, 0, 5 }) : fromSet ({ 2, 1, 6 }); break;
                case 5:  // MUTACIÓN: la misma familia va cambiando de a poco; cada tanto salta a otra
                         if (last.fam >= 0 && last.left > 0) { fam = last.fam; mutate = true; --last.left; }
                         else { fam = r.nextInt (kDiceFamilies); for (int k = 0; k < 8 && fam == last.fam; ++k) fam = r.nextInt (kDiceFamilies); last.left = 3 + r.nextInt (6); }
                         break;
                default: fam = r.nextInt (kDiceFamilies);                             // SORPRESA: cualquier cosa
                         for (int k = 0; k < 8 && (fam == last.fam || fam == last.fam2); ++k) fam = r.nextInt (kDiceFamilies);
                         break;
            }
            if (forcedFamily() >= 0) { fam = forcedFamily(); mutate = false; }
            Dice d;
            d.fam = fam;
            d.aExc = (int) ResExciter::Strike; d.bExc = (int) ImpExciter::Mallet;
            d.aModel = r.nextInt (7); d.bModel = r.nextInt (12); d.cModel = r.nextInt (24);
            d.aStruct = u (0.0f, 0.95f); d.aBright = u (0.3f, 0.95f); d.aDamp = u (0.2f, 0.7f); d.aPos = u (0.05f, 0.7f);
            d.bMat = u (0.0f, 1.0f); d.bDecay = u (0.3f, 0.7f); d.bTone = u (0.3f, 0.9f); d.bPos = u (0.05f, 0.7f);
            d.cHarm = u (0.1f, 0.9f); d.cTimbre = u (0.15f, 0.9f); d.cMorph = u (0.15f, 0.9f);
            switch (fam)
            {
                case 0:   // PUNTEADO: cuerda / piano / tine, corto y brillante
                    d.abMix = 0.0f; d.cMix = 0.0f;
                    d.aModel = pick ({ (int) ResModel::String, (int) ResModel::Piano, (int) ResModel::Tine, (int) ResModel::Modal });
                    d.aExc = pick ({ (int) ResExciter::Pluck, (int) ResExciter::Strike, (int) ResExciter::Hammer });
                    d.aDamp = u (0.45f, 0.85f); d.aBright = u (0.55f, 0.95f);
                    d.att = 1.0f; d.dec = u (90.0f, 350.0f); d.sus = 0.0f; d.rel = u (60.0f, 200.0f);
                    d.cutHz = u (3500.0f, 14000.0f); d.res = u (0.05f, 0.3f); d.fEnv = 0.0f;
                    d.oct = pick ({ 0, 0, 12 }); break;
                case 1:   // CAMPANA / METAL: largo, brillante, que queda sonando
                    d.abMix = 1.0f; d.cMix = 0.0f;
                    d.bModel = pick ({ (int) ImpModel::Bell, (int) ImpModel::Glass, (int) ImpModel::Gong, (int) ImpModel::Vibes, (int) ImpModel::SteelPan, (int) ImpModel::Plate });
                    d.bDecay = u (0.75f, 0.97f); d.bTone = u (0.55f, 0.95f);
                    d.att = 1.0f; d.dec = u (2500.0f, 6000.0f); d.sus = 0.0f; d.rel = u (2000.0f, 4000.0f);
                    d.cutHz = u (5000.0f, 16000.0f); d.res = 0.1f; d.fEnv = 0.0f;
                    d.oct = pick ({ 0, 12, 12 }); break;
                case 2:   // PAD: arco o soplido, entra lento, oscuro y largo
                    d.abMix = u (0.0f, 0.25f); d.cMix = 0.0f;
                    d.aModel = pick ({ (int) ResModel::Sympathetic, (int) ResModel::String, (int) ResModel::Reed, (int) ResModel::Metallic, (int) ResModel::Modal });
                    d.aExc = pick ({ (int) ResExciter::Bow, (int) ResExciter::Blow });
                    d.aDamp = u (0.05f, 0.35f); d.aBright = u (0.2f, 0.6f);
                    d.att = u (250.0f, 1200.0f); d.dec = 4000.0f; d.sus = u (0.8f, 1.0f); d.rel = u (1200.0f, 3000.0f);
                    d.cutHz = u (700.0f, 3000.0f); d.res = u (0.1f, 0.45f); d.fEnv = 0.0f;
                    d.oct = pick ({ -12, 0, 0 }); break;
                case 3:   // BATERÍA: bombo, redoblante, hi-hat, ruido o partículas del motor C
                    d.abMix = 0.5f; d.cMix = 1.0f;
                    d.cModel = pick ({ 21, 21, 22, 23, 17, 18 });
                    d.cLpg = true; d.cDecay = u (0.15f, 0.55f);
                    d.att = 1.0f; d.dec = u (80.0f, 400.0f); d.sus = 0.0f; d.rel = 80.0f;
                    d.cutHz = 18000.0f; d.res = 0.05f; d.fEnv = 0.0f;
                    d.oct = d.cModel == 21 ? -12 : 0; break;
                case 4:   // SINTE: osciladores del motor C con filtro resonante que se abre y cierra
                    d.abMix = 0.5f; d.cMix = 1.0f;
                    d.cModel = pick ({ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 13, 14 });
                    d.cLpg = false;
                    d.att = u (1.0f, 60.0f); d.dec = u (150.0f, 1500.0f); d.sus = u (0.0f, 0.6f); d.rel = u (100.0f, 900.0f);
                    d.cutHz = u (300.0f, 2500.0f); d.res = u (0.35f, 0.8f); d.fEnv = u (0.35f, 0.9f); d.fEnvMs = u (60.0f, 500.0f);
                    d.oct = pick ({ -12, -12, 0, 12 }); break;
                case 5:   // MADERA / PARCHE: marimba, barra, mbira, tambor de mano, golpe seco
                    d.abMix = 1.0f; d.cMix = 0.0f;
                    d.bModel = pick ({ (int) ImpModel::Marimba, (int) ImpModel::Bar, (int) ImpModel::Mbira, (int) ImpModel::HandDrum, (int) ImpModel::Membrane, (int) ImpModel::Tube });
                    d.bExc = pick ({ (int) ImpExciter::Mallet, (int) ImpExciter::Mallet, (int) ImpExciter::Bounce });
                    d.bDecay = u (0.15f, 0.5f); d.bTone = u (0.2f, 0.7f);
                    d.att = 1.0f; d.dec = u (150.0f, 700.0f); d.sus = 0.0f; d.rel = 150.0f;
                    d.cutHz = u (2500.0f, 9000.0f); d.res = 0.1f; d.fEnv = 0.0f;
                    d.oct = pick ({ -12, 0, 0, 12 }); break;
                default:  // TEXTURA: granos, enjambre, voz o cuerdas raras del motor C con el objeto B
                    d.abMix = 1.0f; d.cMix = u (0.5f, 0.8f);
                    d.cModel = pick ({ 11, 12, 15, 16, 19, 20 });
                    d.cLpg = false;
                    d.bModel = r.nextInt (12); d.bExc = (int) ImpExciter::Noise; d.bDecay = u (0.4f, 0.8f);
                    d.att = u (20.0f, 600.0f); d.dec = u (800.0f, 3000.0f); d.sus = u (0.3f, 0.8f); d.rel = u (400.0f, 2000.0f);
                    d.cutHz = u (1200.0f, 8000.0f); d.res = u (0.2f, 0.6f); d.fEnv = u (-0.4f, 0.4f); d.fEnvMs = u (200.0f, 1500.0f);
                    d.oct = pick ({ -12, 0, 12 }); break;
            }
            float extraGain = 1.0f;   // compensa lo que suena más bajo (golpes secos, octavas graves)
            // ALTURA: saltos de octava, quinta o cuarta al azar (siempre en tono con la nota tocada)
            {
                const int jump = mode == 3 ? pick ({ 0, 0, -12, 12, 7, -5 })
                               : mode == 1 ? pick ({ 0, -12, 12, 24, -24, 7 })
                                           : pick ({ 0, 0, 12, -12, 7, -5, 19, 24, -12 });
                d.oct = juce::jlimit (-24, 24, d.oct + jump);
                while (note + (float) d.oct > 108.0f) d.oct -= 12;
                while (note + (float) d.oct < 24.0f) d.oct += 12;
                extraGain *= d.oct < 0 ? std::pow (1.3f, (float) -d.oct / 12.0f) : d.oct > 12 ? 0.85f : 1.0f;
            }
            // DURACIÓN: cada nota con otro largo (del golpe seco al que queda sonando)
            {
                const bool shortGroup = fam == 3 || fam == 0 || fam == 5;
                float lo = -2.2f, hi = 1.8f, pStacc = 0.18f, pDrone = 0.15f, pSwell = 0.12f;
                if (mode == 1) { lo = -1.8f; hi = 1.0f; pStacc = 0.25f; pDrone = 0.0f; pSwell = 0.0f; }
                if (mode == 3) { lo = -0.8f; hi = 1.6f; pStacc = 0.0f; pDrone = 0.25f; pSwell = 0.3f; }
                if (mode == 4) { if (shortGroup) { lo = -2.0f; hi = 0.0f; pStacc = 0.4f; pDrone = 0.0f; pSwell = 0.0f; }
                                 else            { lo = 0.0f; hi = 1.6f; pStacc = 0.0f; pDrone = 0.3f; pSwell = 0.2f; } }
                float lg = u (lo, hi);
                const float roll = r.nextFloat();
                if (roll < pStacc)                         // seco: casi un clic con altura
                {
                    extraGain *= 1.9f;
                    lg = -3.0f; d.dec = u (25.0f, 90.0f); d.sus = 0.0f; d.rel = u (20.0f, 60.0f); d.att = 1.0f;
                }
                else if (roll < pStacc + pDrone)           // queda sonando mientras dure la nota
                {
                    lg = 1.8f; d.sus = u (0.6f, 1.0f); d.dec = juce::jmax (d.dec, 1500.0f); d.rel = u (1500.0f, 5000.0f);
                }
                else
                {
                    const float mul = std::exp2 (lg);
                    d.dec = juce::jlimit (25.0f, 12000.0f, d.dec * mul);
                    d.rel = juce::jlimit (20.0f, 8000.0f, d.rel * mul);
                }
                const bool sustained = fam == 2 || fam == 4 || fam == 6;
                if (sustained && r.nextFloat() < pSwell * 1.6f) { d.att = u (90.0f, 350.0f); if (d.sus < 0.5f) d.sus = u (0.5f, 0.9f); }   // entra de a poco
                // el cuerpo del instrumento acompaña: corto = más apagado, largo = resuena más
                d.aDamp = juce::jlimit (0.0f, 1.0f, d.aDamp - lg * 0.12f);
                d.bDecay = juce::jlimit (0.0f, 1.0f, d.bDecay + lg * 0.1f);
                d.cDecay = juce::jlimit (0.0f, 1.0f, d.cDecay + lg * 0.12f);
            }
            d.gain = kDiceFamilyGain[(size_t) fam] * extraGain;
            if (mutate && last.has)
            {
                // parte de la nota anterior y corre cada perilla un poco (a veces cambia el modelo)
                Dice m = last.prev;
                auto nudge = [&] (float& x, float amt) { x = juce::jlimit (0.0f, 1.0f, x + u (-amt, amt)); };
                for (float* x : { &m.aStruct, &m.aBright, &m.aDamp, &m.aPos, &m.bMat, &m.bDecay, &m.bTone, &m.bPos, &m.cHarm, &m.cTimbre, &m.cMorph })
                    nudge (*x, 0.12f);
                m.cutHz = juce::jlimit (200.0f, 18000.0f, m.cutHz * std::exp2 (u (-0.4f, 0.4f)));
                m.dec = juce::jlimit (60.0f, 8000.0f, m.dec * std::exp2 (u (-0.5f, 0.5f)));
                if (r.nextInt (4) == 0) { m.aModel = d.aModel; m.bModel = d.bModel; m.cModel = d.cModel; }
                if (r.nextInt (3) == 0) m.oct = d.oct;                                  // a veces salta de altura
                if (r.nextInt (3) == 0) { m.att = d.att; m.sus = d.sus; m.rel = d.rel; m.dec = d.dec; }   // o de largo
                d = m;
            }
            dice = d;
            last.prev = d; last.has = true;
            last.fam2 = last.fam; last.fam = fam;
        }
        vel = juce::jlimit (0.05f, 1.0f, velocity * (1.0f + offsets[OVel] * vp.variation * 0.35f));

        // WIDTH: posición estéreo de esta voz
        pan = (seedRng.nextFloat() * 2.0f - 1.0f) * vp.width;
        bend = (float) (pitchWheel - 8192) / 8192.0f * 2.0f;
        fenv = 1.0f;

        const auto seed = (uint32_t) seedRng.nextInt();
        updateEngines (32);
        dcL.reset(); dcR.reset();
        engA.trigger (0.3f + 0.7f * velocity, seed);
        engB.trigger (0.3f + 0.7f * velocity, seed ^ 0x5bd1e995u);
        env.noteOn();
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff && vp.freeze)
        {
            heldByFreeze = true; // sigue sonando mientras FREEZE esté activo
            return;
        }
        heldByFreeze = false;
        engA.release();
        macro.noteOff();
        env.noteOff();
        if (! allowTailOff)
        {
            env.reset();
            engA.reset();
            engB.reset();
            clearCurrentNote();
        }
    }

    void pitchWheelMoved (int v) override      { bend = (float) (v - 8192) / 8192.0f * 2.0f; }
    void controllerMoved (int, int) override  {}

    void renderNextBlock (juce::AudioBuffer<float>& out, int start, int num) override
    {
        if (! isVoiceActive())
            return;

        if (heldByFreeze && ! vp.freeze)
        {
            heldByFreeze = false;
            engA.release();
            macro.noteOff();
            env.noteOff();
        }
        const float dk = diced ? std::exp2 (vp.diceDecay * 3.0f) : 1.0f;   // CAÍDA: de 1/8 a 8 veces más largo
        if (diced) env.setParameters (dice.att, vp.freeze ? 1.0e6f : juce::jlimit (15.0f, 40000.0f, dice.dec * dk), vp.freeze ? 1.0f : dice.sus, juce::jlimit (15.0f, 20000.0f, dice.rel * dk));
        else       env.setParameters (vp.attack, vp.freeze ? 1.0e6f : vp.decay, vp.freeze ? 1.0f : vp.sustain, vp.release);
        const float fenvCoef = std::exp (-1.0f / (std::max (1.0f, diced ? dice.fEnvMs : vp.fltEnvDecayMs) * 0.001f * sampleRate));
        const float velGain = (1.0f - vp.velSens + vp.velSens * vel) * (diced ? dice.gain : 1.0f);
        const float cMixV = diced ? dice.cMix : vp.cMix;
        // Nota de un solo motor (dos secuenciadores): ese motor suena entero
        // Motor C: qué motores puede usar esta nota (con dos secuenciadores, según SEQ 2 → B / C / B+C)
        const bool cAllowed = engineMask == 3 || (engineMask == 1 && vp.seq2Target == 0) || (engineMask == 2 && vp.seq2Target >= 1);
        const bool abAllowed = engineMask != 2 || vp.seq2Target != 1;
        const float gAB = abAllowed ? (cAllowed ? 1.0f - cMixV : 1.0f) : 0.0f;
        const float gC  = cAllowed ? (abAllowed ? cMixV : 1.0f) : 0.0f;
        // EXCITER = ENGINE C: el motor C es el excitador del resonador A (se calcula aunque no se escuche)
        const bool cExcites = vp.a.exciter == ResExciter::EngineC;
        const bool cOn = (cAllowed && (gC > 0.0f || vp.cFeed > 0.0f || vp.cFeedB > 0.0f)) || cExcites;
        const float abMix = diced ? dice.abMix : vp.abMix;
        const float mixA = gAB * (engineMask == 1 ? 1.0f : engineMask == 2 ? 0.0f : std::cos (abMix * juce::MathConstants<float>::halfPi));
        const float mixB = gAB * (engineMask == 2 ? 1.0f : engineMask == 1 ? 0.0f : std::sin (abMix * juce::MathConstants<float>::halfPi));
        const bool runA = mixA > 1.0e-4f, runB = mixB > 1.0e-4f;
        const bool runEngA = runA || (runB && vp.xfeed > 0.0f);
        const float cGain = gC * 0.9f;            // Plaits llega a la voz a unos -6 dB (calibrado con la prueba "audit")
        const float cFeed = vp.cFeed * 0.6f + (cExcites ? kCExcite * vel : 0.0f);
        const float cFeedB = vp.cFeedB * kCFeedB;

        // Paneo equal-power: el motor A se abre hacia un lado y el B hacia el otro
        auto panGains = [] (float p, float& gl, float& gr)
        {
            const float a = (juce::jlimit (-1.0f, 1.0f, p) + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
            gl = std::cos (a) * 1.4142f;
            gr = std::sin (a) * 1.4142f;
        };
        float aL, aR, bL, bR;
        panGains (pan - vp.width * 0.35f, aL, aR);
        panGains (pan + vp.width * 0.35f, bL, bR);
        auto* l = out.getWritePointer (0);
        auto* r = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;

        for (int pos = 0; pos < num; pos += kChunk)
        {
            const int n = std::min (kChunk, num - pos);
            updateEngines (n);

            const float cutoff = juce::jmin (0.45f * sampleRate,
                                 (diced ? dice.cutHz : vp.cutoff)
                                 * std::exp2 ((diced ? dice.fEnv : vp.fltEnvAmt) * fenv * 6.0f + vp.keyTrack * (note - 60.0f) / 12.0f
                                              + (lockDest == VoiceParams::LockCutoff ? lock * 3.0f : 0.0f)));
            const float res = diced ? dice.res : vp.resonance;
            const float air = vp.life * 0.0025f; // soplo: excita el resonador muy suavemente mientras la nota vive
            if (runA) filterA.set (cutoff, res, vp.filterType);
            if (runB) filterB.set (cutoff, res, vp.filterType);
            if (cOn) { filterCL.set (cutoff, res, vp.filterType); filterCR.set (cutoff, res, vp.filterType); }

            for (int i = 0; i < n; ++i)
            {
                const float breath = air > 0.0f ? airNoise.next() * air : 0.0f;
                float cl = 0.0f, cr = 0.0f;
                if (cOn) macro.next (cl, cr);
                // C > A FEED: el motor MACRO excita al resonador (un oscilador macro golpeando un resonador)
                // Un motor que no se escucha ni alimenta al otro no se calcula (ahorra CPU)
                // Motor C como excitador: un oscilador afinado en la nota del resonador lo hace entrar en
                // resonancia y el nivel se dispara. Un control automático (rápido al subir, lento al bajar)
                // le baja la excitación para que el resonador quede al nivel del arco.
                const float a = runEngA ? engA.process (breath + (cl + cr) * 0.5f * cFeed * (cExcites ? excAgc : 1.0f)) : 0.0f;
                if (cExcites)
                {
                    aFollow = std::max (std::abs (a), aFollow * 0.9995f);
                    const float want = aFollow > kCExciteTarget ? kCExciteTarget / aFollow : 1.0f;
                    excAgc += (want - excAgc) * (want < excAgc ? 0.02f : 0.0004f);
                }
                const float b = runB ? engB.process (a * vp.xfeed * 2.0f + breath * 0.5f + (cl + cr) * 0.5f * cFeedB) : 0.0f;
                const float g = env.next() * velGain;
                const float ya = runA ? filterA.process (a * mixA, vp.filterType, vp.drive) * g : 0.0f;
                const float yb = runB ? filterB.process (b * mixB, vp.filterType, vp.drive) * g : 0.0f;
                const float ycl = cGain > 0.0f ? filterCL.process (cl * cGain, vp.filterType, vp.drive) * g : 0.0f;
                const float ycr = cGain > 0.0f ? filterCR.process (cr * cGain, vp.filterType, vp.drive) * g : 0.0f;
                fenv *= fenvCoef;

                if (r != nullptr)
                {
                    // sin continua ni retumbe por debajo de 18 Hz (los golpes de un solo signo y los
                    // modos graves los generan): más margen y la saturación suena limpia
                    const float xl = dcL.process (ya * aL + yb * bL + ycl), xr = dcR.process (ya * aR + yb * bR + ycr);
                   #if URING_VOICE_METER
                    const float m = std::max (std::abs (xl), std::abs (xr));
                    if (m > VoiceMeter::peak.load (std::memory_order_relaxed)) VoiceMeter::peak.store (m, std::memory_order_relaxed);
                    if (m > 1.5f) VoiceMeter::saturated.fetch_add (1, std::memory_order_relaxed);
                   #endif
                    l[start + pos + i] += fastTanh (xl);
                    r[start + pos + i] += fastTanh (xr);
                }
                else
                {
                    l[start + pos + i] += fastTanh (dcL.process (ya + yb + (ycl + ycr) * 0.5f));
                }
            }

            if (! env.isActive())
            {
                engA.reset();
                engB.reset();
                clearCurrentNote();
                return;
            }
        }
    }

private:
    static constexpr int kChunk = 32;
public:
    // Ganancias del motor C como excitador (calibradas con la prueba "v15": mismo nivel que el arco / el mazo)
    static inline float kCExcite = 0.03f, kCFeedB = 0.35f, kCExciteTarget = 0.15f;
private:

    void updateEngines (int chunk) noexcept
    {
        // Parámetros de esta voz = compartidos + desvío del golpe × VARIATION
        const float v = vp.variation;
        auto vary = [v] (float base, float off, float amount) { return juce::jlimit (0.0f, 1.0f, base + off * v * amount); };

        ResParams a = vp.a;
        ImpParams bDiced = vp.b;
        if (diced)   // DADO EN CADA NOTA: el instrumento que sorteó esta nota
        {
            a.model = (ResModel) dice.aModel;
            if (a.exciter != ResExciter::EngineC) a.exciter = (ResExciter) dice.aExc;
            a.structure = dice.aStruct; a.brightness = dice.aBright; a.position = dice.aPos;
            a.damping = juce::jlimit (0.0f, 1.0f, dice.aDamp - vp.diceDecay * 0.3f);   // CAÍDA también en el cuerpo
            bDiced.model = (ImpModel) dice.bModel; bDiced.exciter = (ImpExciter) dice.bExc;
            bDiced.material = dice.bMat; bDiced.decay = juce::jlimit (0.0f, 1.0f, dice.bDecay + vp.diceDecay * 0.25f); bDiced.tone = dice.bTone; bDiced.position = dice.bPos;
        }
        a.decayScale = vp.freeze ? 40.0f : 1.0f;
        a.position   = vary (a.position, offsets[OPosA], 0.35f);
        a.brightness = vary (a.brightness, offsets[OBright], 0.25f);
        a.damping    = vary (a.damping, offsets[ODampA], 0.15f);

        ImpParams b = bDiced;
        b.decayScale = vp.freeze ? 40.0f : 1.0f;
        b.position = vary (b.position, offsets[OPosB], 0.35f);
        b.tone     = vary (b.tone, offsets[OTone], 0.3f);
        b.material = vary (b.material, offsets[OMaterial], 0.2f);
        b.decay    = vary (b.decay, offsets[ODecayB], 0.15f);

        // Lock del paso (se suma al timbre de esta voz)
        if (lock != 0.0f)
        {
            auto add = [this] (float& x, float k) { x = juce::jlimit (0.0f, 1.0f, x + lock * k); };
            switch (lockDest)
            {
                case VoiceParams::LockABright:    add (a.brightness, 0.5f); break;
                case VoiceParams::LockADamping:   add (a.damping, 0.5f); break;
                case VoiceParams::LockAPosition:  add (a.position, 0.5f); break;
                case VoiceParams::LockAStructure: add (a.structure, 0.5f); break;
                case VoiceParams::LockBMaterial:  add (b.material, 0.5f); break;
                case VoiceParams::LockBTone:      add (b.tone, 0.5f); break;
                case VoiceParams::LockBDecay:     add (b.decay, 0.5f); break;
                default: break; // el cutoff se aplica en el filtro
            }
        }
        // LIFE: deriva lenta de afinación (paseo aleatorio, hasta ±10 cents)
        if (vp.life > 0.0f)
        {
            drift += (seedRng.nextFloat() - 0.5f) * 0.02f * vp.life;
            drift = juce::jlimit (-0.1f * vp.life, 0.1f * vp.life, drift * 0.998f);
        }
        const float base = note + (diced ? (float) dice.oct + vp.diceTone : 0.0f) + bend + vp.tune + vp.pitchMod + offsets[OPitch] * v * 0.12f + drift; // hasta ±12 cents
        if (! std::isfinite (base)) return;   // afinación inválida (NaN): no se le pasa a los motores
        if (diced) testPitchShift.store (base - note - (float) dice.oct, std::memory_order_relaxed);   // (pruebas) lo que corre TONO
        // Motor C: mismos locks por paso (HARMONICS / TIMBRE / MORPH)
        MacroParams cp = vp.c;
        if (lock != 0.0f)
        {
            auto addc = [this] (float& x) { x = juce::jlimit (0.0f, 1.0f, x + lock * 0.5f); };
            if (lockDest == VoiceParams::LockCHarmonics) addc (cp.harmonics);
            if (lockDest == VoiceParams::LockCTimbre)    addc (cp.timbre);
            if (lockDest == VoiceParams::LockCMorph)     addc (cp.morph);
        }
        if (diced) { cp.model = dice.cModel; cp.harmonics = dice.cHarm; cp.timbre = dice.cTimbre; cp.morph = dice.cMorph;
                     cp.lpg = dice.cLpg; cp.decay = juce::jlimit (0.0f, 1.0f, dice.cDecay + vp.diceDecay * 0.3f); cp.fm = cp.timbreMod = cp.morphMod = 0.0f; }
        if (modelOverride >= 0) cp.model = modelOverride;   // carril MODEL del secuenciador
        macro.set (base + vp.cPitch, cp);
        engA.update (midiToHz (base + vp.aPitch), a);
        engB.update (midiToHz (base + vp.bPitch), b, chunk);
    }

    enum Offset { OPosA = 0, OBright, ODampA, OPosB, OTone, OMaterial, ODecayB, OPitch, OVel, NumOffsets };
    std::array<float, NumOffsets> offsets {};
    float pan = 0.0f;
    bool heldByFreeze = false;

    const VoiceParams& vp;
    float sampleRate = 44100.0f, note = 60.0f, vel = 1.0f, bend = 0.0f, fenv = 0.0f;
    int engineMask = 3; // 1 = solo A, 2 = solo B, 3 = los dos
    MacroEngine macro;
    Svf filterCL, filterCR;
    float lock = 0.0f, drift = 0.0f;
    int lockDest = 0, modelOverride = -1;
    bool diced = false;
    struct Dice { int fam = 0, aModel = 0, aExc = 0, bModel = 0, bExc = 0, cModel = 0, oct = 0;
                  float abMix = 0.5f, aStruct = 0.3f, aBright = 0.5f, aDamp = 0.4f, aPos = 0.3f,
                        bMat = 0.4f, bDecay = 0.35f, bTone = 0.5f, bPos = 0.3f, cHarm = 0.5f, cTimbre = 0.5f, cMorph = 0.5f,
                        cMix = 0.0f, cDecay = 0.5f, att = 1.0f, dec = 500.0f, sus = 0.0f, rel = 300.0f,
                        cutHz = 8000.0f, res = 0.1f, fEnv = 0.0f, fEnvMs = 300.0f, gain = 1.0f;
                  bool cLpg = true; } dice;
    float excAgc = 1.0f, aFollow = 0.0f;   // control de nivel del motor C como excitador
    Noise airNoise;
    ResonatorEngine engA;
    ImpactEngine engB;
    Svf filterA, filterB;
    struct DcBlock   // pasa-altos Butterworth de 2.º orden (18 Hz)
    {
        float b0 = 1.0f, b1 = -2.0f, b2 = 1.0f, a1 = -2.0f, a2 = 1.0f, x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
        void prepare (double sr)
        {
            const double w = std::tan (juce::MathConstants<double>::pi * 18.0 / sr), q = 0.7071;
            const double n = 1.0 / (1.0 + w / q + w * w);
            b0 = (float) n; b1 = (float) (-2.0 * n); b2 = (float) n;
            a1 = (float) (2.0 * (w * w - 1.0) * n); a2 = (float) ((1.0 - w / q + w * w) * n);
            reset();
        }
        void reset() { x1 = x2 = y1 = y2 = 0.0f; }
        float process (float x)
        {
            const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x; y2 = y1; y1 = y;
            return y;
        }
    } dcL, dcR;
    mm::Envelope env;
    juce::Random seedRng;
    // DADO: generador propio que nunca se reinicia (semilla del sistema), así no sigue un patrón
    juce::Random diceRng { juce::Random::getSystemRandom().nextInt64() ^ juce::Time::getHighResolutionTicks() };
    struct LastDice { int fam = -1, fam2 = -1, left = 0; bool flip = false, has = false; Dice prev; };
    static LastDice& lastDice() { static LastDice l; return l; }   // compartido por todas las voces (hilo de audio)
public:
    static constexpr int kDiceFamilies = 7;
    // volumen de cada familia para que salgan parejas (medido con la prueba "dicefam")
    static inline std::array<float, kDiceFamilies> kDiceFamilyGain { 2.05f, 2.2f, 1.9f, 1.05f, 0.34f, 2.6f, 0.95f };
    static int& forcedFamily() { static int f = -1; return f; }   // solo pruebas
    static int lastFamily() { return lastDice().fam; }            // solo pruebas
    static inline std::atomic<float> testPitchShift { 0.0f };       // solo pruebas
    void reseed (juce::int64 seed) { seedRng.setSeed (seed); }
private:
};

} // namespace cz
