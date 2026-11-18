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
            auto u = [this] (float lo, float hi) { return lo + (hi - lo) * seedRng.nextFloat(); };
            dice.aModel = seedRng.nextInt (7);   dice.aExc = seedRng.nextInt (5);
            dice.bModel = seedRng.nextInt (12);  dice.bExc = seedRng.nextInt (4);
            dice.cModel = seedRng.nextInt (24);
            const int shape = seedRng.nextInt (3);   // A, B o mezcla
            dice.abMix = shape == 0 ? u (0.0f, 0.15f) : shape == 1 ? u (0.85f, 1.0f) : u (0.3f, 0.7f);
            dice.aStruct = u (0.05f, 0.85f); dice.aBright = u (0.2f, 0.85f); dice.aDamp = u (0.15f, 0.7f); dice.aPos = u (0.05f, 0.7f);
            dice.bMat = u (0.1f, 0.9f); dice.bDecay = u (0.2f, 0.7f); dice.bTone = u (0.3f, 0.85f); dice.bPos = u (0.05f, 0.7f);
            dice.cHarm = u (0.15f, 0.85f); dice.cTimbre = u (0.15f, 0.85f); dice.cMorph = u (0.15f, 0.85f);
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
        env.setParameters (vp.attack, vp.freeze ? 1.0e6f : vp.decay, vp.freeze ? 1.0f : vp.sustain, vp.release);
        const float fenvCoef = std::exp (-1.0f / (std::max (1.0f, vp.fltEnvDecayMs) * 0.001f * sampleRate));
        const float velGain = 1.0f - vp.velSens + vp.velSens * vel;
        // Nota de un solo motor (dos secuenciadores): ese motor suena entero
        // Motor C: qué motores puede usar esta nota (con dos secuenciadores, según SEQ 2 → B / C / B+C)
        const bool cAllowed = engineMask == 3 || (engineMask == 1 && vp.seq2Target == 0) || (engineMask == 2 && vp.seq2Target >= 1);
        const bool abAllowed = engineMask != 2 || vp.seq2Target != 1;
        const float gAB = abAllowed ? (cAllowed ? 1.0f - vp.cMix : 1.0f) : 0.0f;
        const float gC  = cAllowed ? (abAllowed ? vp.cMix : 1.0f) : 0.0f;
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

            const float cutoff = vp.cutoff * std::exp2 (vp.fltEnvAmt * fenv * 6.0f + vp.keyTrack * (note - 60.0f) / 12.0f
                                                        + (lockDest == VoiceParams::LockCutoff ? lock * 3.0f : 0.0f));
            const float air = vp.life * 0.0025f; // soplo: excita el resonador muy suavemente mientras la nota vive
            if (runA) filterA.set (cutoff, vp.resonance, vp.filterType);
            if (runB) filterB.set (cutoff, vp.resonance, vp.filterType);
            if (cOn) { filterCL.set (cutoff, vp.resonance, vp.filterType); filterCR.set (cutoff, vp.resonance, vp.filterType); }

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
            a.structure = dice.aStruct; a.brightness = dice.aBright; a.damping = dice.aDamp; a.position = dice.aPos;
            bDiced.model = (ImpModel) dice.bModel; bDiced.exciter = (ImpExciter) dice.bExc;
            bDiced.material = dice.bMat; bDiced.decay = dice.bDecay; bDiced.tone = dice.bTone; bDiced.position = dice.bPos;
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
        const float base = note + bend + vp.tune + vp.pitchMod + offsets[OPitch] * v * 0.12f + drift; // hasta ±12 cents
        // Motor C: mismos locks por paso (HARMONICS / TIMBRE / MORPH)
        MacroParams cp = vp.c;
        if (lock != 0.0f)
        {
            auto addc = [this] (float& x) { x = juce::jlimit (0.0f, 1.0f, x + lock * 0.5f); };
            if (lockDest == VoiceParams::LockCHarmonics) addc (cp.harmonics);
            if (lockDest == VoiceParams::LockCTimbre)    addc (cp.timbre);
            if (lockDest == VoiceParams::LockCMorph)     addc (cp.morph);
        }
        if (diced) { cp.model = dice.cModel; cp.harmonics = dice.cHarm; cp.timbre = dice.cTimbre; cp.morph = dice.cMorph; }
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
    struct Dice { int aModel = 0, aExc = 0, bModel = 0, bExc = 0, cModel = 0;
                  float abMix = 0.5f, aStruct = 0.3f, aBright = 0.5f, aDamp = 0.4f, aPos = 0.3f,
                        bMat = 0.4f, bDecay = 0.35f, bTone = 0.5f, bPos = 0.3f, cHarm = 0.5f, cTimbre = 0.5f, cMorph = 0.5f; } dice;
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
public:
    void reseed (juce::int64 seed) { seedRng.setSeed (seed); }
private:
};

} // namespace cz
