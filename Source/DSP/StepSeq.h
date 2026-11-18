#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <atomic>
#include "Scales.h"
#include "marbles/random/random_sequence.h"   // Mutable Instruments Marbles (MIT): bucle aleatorio DÉJÀ VU
#include "marbles/random/distributions.h"

namespace cz
{

// Ritmo euclidiano (Bresenham): reparte `pulses` golpes lo más parejo posible en `steps`
inline bool euclidHit (int step, int pulses, int steps, int rotate) noexcept
{
    if (steps <= 0 || pulses <= 0) return false;
    if (pulses >= steps) return true;
    const int i = ((step - rotate) % steps + steps) % steps;
    return ((i * pulses) % steps) < pulses;
}

//==============================================================================
// Carriles del secuenciador melódico. RHYTHM usa la longitud STEPS; el resto tiene su
// propia longitud (polimetría: la melodía se desfasa y no se repite igual).
// LOCK: parameter lock por paso (-1..1, 0 = sin lock) sobre el parámetro elegido en LOCK DEST
// MODEL: modelo del motor C para ese paso (-1 = el de la perilla). Idea tomada de Flechtwerk Pro.
enum class Lane { Note = 0, Octave, Velocity, Gate, Prob, Ratchet, Cond, Lock, Model, NumValueLanes };
constexpr int kNumCModels = 24;
constexpr int kNumValueLanes = (int) Lane::NumValueLanes;

enum class Direction { Forward = 0, Backward, PingPong, Random };

// Condiciones por paso (1:2, 3:4, fill, etc.)
enum class Cond { Always = 0, OneOfTwo, TwoOfTwo, OneOfThree, OneOfFour, Fill, NotFill, NumConds };
inline const char* condName (int c)
{
    static const char* names[] { "ALL", "1:2", "2:2", "1:3", "1:4", "FILL", "!FILL" };
    return names[juce::jlimit (0, (int) Cond::NumConds - 1, c)];
}

// Índice del paso n-ésimo para un carril de longitud `len` según la dirección
inline int directionIndex (juce::int64 n, int len, Direction dir, juce::Random& rng) noexcept
{
    if (len <= 1) return 0;
    const int fwd = (int) (((n % len) + len) % len);
    switch (dir)
    {
        case Direction::Backward: return len - 1 - fwd;
        case Direction::PingPong:
        {
            const int period = 2 * (len - 1);
            const int m = (int) (((n % period) + period) % period);
            return m < len ? m : period - m;
        }
        case Direction::Random:   return rng.nextInt (len);
        default:                  return fwd;
    }
}

//==============================================================================
// Patrón de hasta 64 pasos (4 páginas de 16). El ritmo es euclidiano y cada pad editado
// lo invierte (XOR), así los knobs PULSES/ROTATE siguen funcionando sobre tus ediciones.
class SeqPattern
{
public:
    static constexpr int kSteps = 64;
    static constexpr int kPageSize = 16;
    static constexpr int kMinNote = -12, kMaxNote = 24;   // semitonos desde la tónica (cromático)
    static constexpr int kMinOctave = -2, kMaxOctave = 2;

    static constexpr int kMaxRatchet = 4;

    struct Step
    {
        bool flip = false;
        int semis = 0, octave = 0;
        float velocity = 0.8f, gate = 0.4f, prob = 1.0f;
        int ratchet = 1;   // golpes dentro del paso (1-4)
        int cond = 0;      // Cond
        float lock = 0.0f; // parameter lock (-1..1; 0 = nada)
        int model = -1;    // modelo del motor C en este paso (-1 = el de la perilla)
    };

    // Ajustes rítmicos propios de cada memoria de patrón
    struct Meta { int length = 16, pulses = 5, rotate = 0, direction = 0; };

    Step get (int i) const noexcept
    {
        const auto& s = steps[(size_t) i];
        return { s.flip.load(), s.semis.load(), s.octave.load(), s.velocity.load(), s.gate.load(), s.prob.load(),
                 s.ratchet.load(), s.cond.load(), s.lock.load(), s.model.load() };
    }

    void set (int i, const Step& d) noexcept
    {
        auto& s = steps[(size_t) i];
        s.flip = d.flip; s.semis = d.semis; s.octave = d.octave;
        s.velocity = d.velocity; s.gate = d.gate; s.prob = d.prob;
        s.ratchet = d.ratchet; s.cond = d.cond; s.lock = d.lock; s.model = d.model;
    }

    Meta getMeta() const noexcept   { return { length.load(), pulses.load(), rotate.load(), direction.load() }; }
    void setMeta (const Meta& m) noexcept
    {
        length = m.length; pulses = m.pulses; rotate = m.rotate; direction = m.direction;
    }

    void copyFrom (const SeqPattern& other) noexcept
    {
        for (int i = 0; i < kSteps; ++i)
            set (i, other.get (i));
        setMeta (other.getMeta());
    }

    // Melodía larga en la escala: la página 1 es la frase, la 2 una respuesta transpuesta,
    // la 3 repite la frase con pequeños cambios y la 4 cierra volviendo a la tónica.
    void generate (float range, const mm::Scale& sc, juce::Random& r)
    {
        const int maxDeg = maxDegree (range);
        std::array<int, kPageSize> motif {};
        for (auto& d : motif)
            d = randomDegree (maxDeg, r);

        for (int i = 0; i < kSteps; ++i)
        {
            const int page = i / kPageSize, k = i % kPageSize;
            int deg = motif[(size_t) k];
            if (page == 1)      deg += (k < 8 ? 2 : -1);                       // respuesta
            else if (page == 2) deg += r.nextFloat() < 0.2f ? (r.nextBool() ? 1 : -1) : 0;
            else if (page == 3) deg = k >= 12 ? (k == 15 ? 0 : deg / 2) : deg; // cierre hacia la tónica

            Step s;
            s.semis    = juce::jlimit (kMinNote, kMaxNote, mm::degreeToSemis (sc, deg));
            s.octave   = r.nextFloat() < 0.12f ? 1 : 0;
            s.velocity = 0.55f + 0.45f * r.nextFloat();
            s.gate     = 0.25f + 0.35f * r.nextFloat();
            s.prob     = r.nextFloat() < 0.8f ? 1.0f : 0.5f + 0.3f * r.nextFloat();
            set (i, s);
        }
    }

    // ---- Aleatorización por carril (o todos a la vez)
    // DENSITY: con valores altos se agregan golpes fuera del euclidiano; con bajos se quitan
    void randomizeRhythm (juce::Random& r, float density, int pulses, int length, int rotate)
    {
        for (int i = 0; i < kSteps; ++i)
        {
            auto d = get (i);
            const bool hit = euclidHit (i, pulses, juce::jmax (1, length), rotate);
            d.flip = hit ? r.nextFloat() < (1.0f - density) * 0.35f
                         : r.nextFloat() < density * 0.4f;
            set (i, d);
        }
    }

    void randomizeLane (Lane lane, float range, const mm::Scale& sc, juce::Random& r)
    {
        const int maxDeg = maxDegree (range);
        for (int i = 0; i < kSteps; ++i)
        {
            auto d = get (i);
            switch (lane)
            {
                case Lane::Note:     d.semis = juce::jlimit (kMinNote, kMaxNote, mm::degreeToSemis (sc, randomDegree (maxDeg, r))); break;
                case Lane::Octave:   d.octave = r.nextFloat() < 0.6f ? 0 : (r.nextBool() ? 1 : (r.nextFloat() < 0.7f ? -1 : 2)); break;
                case Lane::Velocity: d.velocity = 0.35f + 0.65f * r.nextFloat(); break;
                case Lane::Gate:     d.gate = r.nextFloat() < 0.2f ? 1.0f + 2.0f * r.nextFloat()     // algunas largas
                                                                   : 0.1f + 0.7f * r.nextFloat() * r.nextFloat(); break;
                case Lane::Prob:     d.prob = r.nextFloat() < 0.55f ? 1.0f : 0.25f + 0.7f * r.nextFloat(); break;
                case Lane::Ratchet:  d.ratchet = r.nextFloat() < 0.75f ? 1 : 2 + r.nextInt (kMaxRatchet - 1); break;
                case Lane::Cond:     d.cond = r.nextFloat() < 0.7f ? 0 : 1 + r.nextInt ((int) Cond::NumConds - 1); break;
                case Lane::Lock:     d.lock = r.nextFloat() < 0.5f ? 0.0f : r.nextFloat() * 2.0f - 1.0f; break;
                case Lane::Model:    d.model = r.nextFloat() < 0.5f ? -1 : r.nextInt (kNumCModels); break;
                default: break;
            }
            set (i, d);
        }
    }

    void randomizeAll (float range, const mm::Scale& sc, float density, int pulses, int length, int rotate, juce::Random& r)
    {
        randomizeRhythm (r, density, pulses, length, rotate);
        for (int l = 0; l < kNumValueLanes; ++l)
            if ((Lane) l != Lane::Model)   // cambiar de modelo cambia todo el sonido: solo con RND LANE
                randomizeLane ((Lane) l, range, sc, r);
    }

    // FIT SCALE: lleva cada nota a la nota más cercana de la escala
    void fitToScale (const mm::Scale& sc)
    {
        for (int i = 0; i < kSteps; ++i)
        {
            auto d = get (i);
            d.semis = juce::jlimit (kMinNote, kMaxNote, mm::quantizeToScale (sc, d.semis));
            set (i, d);
        }
    }

    juce::ValueTree toValueTree() const
    {
        juce::ValueTree t ("SEQ");
        const auto m = getMeta();
        t.setProperty ("len", m.length, nullptr);
        t.setProperty ("pulses", m.pulses, nullptr);
        t.setProperty ("rot", m.rotate, nullptr);
        t.setProperty ("dir", m.direction, nullptr);
        for (int i = 0; i < kSteps; ++i)
        {
            const auto d = get (i);
            juce::ValueTree s ("STEP");
            s.setProperty ("flip", d.flip, nullptr);
            s.setProperty ("note", d.semis, nullptr);
            s.setProperty ("oct", d.octave, nullptr);
            s.setProperty ("vel", d.velocity, nullptr);
            s.setProperty ("gate", d.gate, nullptr);
            s.setProperty ("prob", d.prob, nullptr);
            s.setProperty ("ratch", d.ratchet, nullptr);
            s.setProperty ("cond", d.cond, nullptr);
            if (d.lock != 0.0f) s.setProperty ("lock", d.lock, nullptr);
            if (d.model >= 0) s.setProperty ("model", d.model, nullptr);
            t.appendChild (s, nullptr);
        }
        return t;
    }

    void fromValueTree (const juce::ValueTree& t)
    {
        setMeta ({ (int) t.getProperty ("len", 16), (int) t.getProperty ("pulses", 5),
                   (int) t.getProperty ("rot", 0), (int) t.getProperty ("dir", 0) });
        for (int i = 0; i < juce::jmin (kSteps, t.getNumChildren()); ++i)
        {
            const auto s = t.getChild (i);
            Step d;
            d.flip     = s.getProperty ("flip", false);
            d.semis    = juce::jlimit (kMinNote, kMaxNote, (int) s.getProperty ("note", 0));
            d.octave   = juce::jlimit (kMinOctave, kMaxOctave, (int) s.getProperty ("oct", 0));
            d.velocity = s.getProperty ("vel", 0.8f);
            d.gate     = juce::jlimit (0.05f, 4.0f, (float) s.getProperty ("gate", 0.4f));
            d.prob     = s.getProperty ("prob", 1.0f);
            d.ratchet  = juce::jlimit (1, kMaxRatchet, (int) s.getProperty ("ratch", 1));
            d.cond     = juce::jlimit (0, (int) Cond::NumConds - 1, (int) s.getProperty ("cond", 0));
            d.lock     = juce::jlimit (-1.0f, 1.0f, (float) s.getProperty ("lock", 0.0f));
            d.model    = juce::jlimit (-1, kNumCModels - 1, (int) s.getProperty ("model", -1));
            set (i, d);
        }
    }

private:
    static int maxDegree (float range) { return 1 + juce::roundToInt (juce::jlimit (0.0f, 1.0f, range) * 9.0f); }

    static int randomDegree (int maxDeg, juce::Random& r)
    {
        return r.nextFloat() < 0.35f ? 0 : r.nextInt ({ -maxDeg / 2, maxDeg + 1 });
    }

    struct AtomicStep
    {
        std::atomic<bool> flip { false };
        std::atomic<int> semis { 0 }, octave { 0 };
        std::atomic<float> velocity { 0.8f }, gate { 0.4f }, prob { 1.0f };
        std::atomic<int> ratchet { 1 }, cond { 0 };
        std::atomic<float> lock { 0.0f };
        std::atomic<int> model { -1 };
    };
    std::array<AtomicStep, kSteps> steps;
    std::atomic<int> length { 16 }, pulses { 5 }, rotate { 0 }, direction { 0 };
};

//==============================================================================
struct SeqSettings
{
    double stepPpq = 0.25;
    int length = 16, pulses = 5, rotate = 0;
    std::array<int, kNumValueLanes> laneLength {}; // 0 = AUTO: igual a `length`
    bool fill = false;                             // botón FILL (condiciones FILL / !FILL)
    Direction direction = Direction::Forward;
    int baseNote = 48;
    mm::Scale scale = mm::scales()[0];
    float gate = 0.4f, swing = 0.5f, globalProb = 1.0f;
    float randomAmt = 0.0f;     // RANDOM en vivo: varía nota/octava/velocidad (no destructivo)
    double randomRatePpq = 0.0; // cada cuánto cambia el random (0 = en cada disparo)
    float randomTime = 0.0f;    // retrasos aleatorios de las notas (humanize)
    float longShort = 0.0f;     // mezcla aleatoria de notas largas y cortas
    // MARBLES: las notas salen de un generador aleatorio con memoria (en vez del carril NOTE)
    bool marbles = false;
    float dejaVu = 0.4f;        // 0 = siempre nuevo, 0.5 = bucle fijo, 1 = salta dentro del bucle
    int marblesLength = 8;      // largo del bucle (1-16)
    float spread = 0.5f, bias = 0.5f, range = 0.4f;
};

// Secuenciador melódico sincronizado a la posición PPQ: el carril RHYTHM decide si hay
// golpe y cada carril de valores avanza con su propia longitud y la dirección elegida.
// Escribe note-on/off en el MidiBuffer que luego consume el sintetizador (y la salida MIDI).
class StepSequencer
{
public:
    static constexpr float kMaxGateSteps = 4.0f; // una nota puede durar hasta 4 pasos
    int channel = 1;                             // canal MIDI de las notas (14 = los dos motores, 15 = A, 16 = B)
    float* lockTable = nullptr;                  // [nota] → lock del paso que la tocó (lo lee la voz)
    int* modelTable = nullptr;                   // [nota] → modelo del motor C de ese paso (-1 = perilla)

    void prepare (double sr) noexcept
    {
        sampleRate = sr;
        initMarbles (0x6d61726bu);
        wasRunning = false;
        numPending = numPendingOn = 0;
        currentStep = -1;
        heldPeriod = -1;
        for (auto& l : laneStep) l = -1;
    }

    int getCurrentStep() const noexcept           { return currentStep.load(); }
    // Suelta todas las notas que este secuenciador tiene sonando (antes de cambiarle el canal)
    void allNotesOff (juce::MidiBuffer& midi, int sample)  { flushAll (midi, sample); }
    int getLaneStep (Lane lane) const noexcept     { return laneStep[(size_t) lane].load(); }

    void process (bool running, double ppqStart, double bpm, int numSamples,
                  const SeqPattern& pattern, const SeqSettings& s, juce::MidiBuffer& midi)
    {
        if (! running)
        {
            if (wasRunning)
                flushAll (midi, 0);
            wasRunning = false;
            currentStep = -1;
            for (auto& l : laneStep) l = -1;
            return;
        }

        bpm = juce::jmax (1.0, bpm);
        const double ppqPerSample = bpm / (60.0 * sampleRate);
        const double ppqEnd = ppqStart + numSamples * ppqPerSample;
        const double stepLen = juce::jmax (1.0e-3, s.stepPpq);

        if (! wasRunning || std::abs (ppqStart - expectedPpq) > ppqPerSample * 2.0 + 1.0e-6 || stepLen != lastStepLen)
        {
            flushAll (midi, 0);
            lastFired = (juce::int64) std::floor (ppqStart / stepLen) - 2;
            heldPeriod = -1;
        }
        wasRunning = true;
        expectedPpq = ppqEnd;
        lastStepLen = stepLen;

        auto toSample = [&] (double ppq) { return juce::jlimit (0, numSamples - 1, (int) ((ppq - ppqStart) / ppqPerSample)); };

        // Note-ons retrasados (TIME) que caen en este bloque
        for (int i = 0; i < numPendingOn;)
        {
            const auto p = pendingOn[(size_t) i];
            if (p.onPpq < ppqEnd)
            {
                pendingOn[(size_t) i] = pendingOn[(size_t) --numPendingOn];
                startNote (midi, p.note, p.velocity, p.onPpq, p.offPpq, ppqEnd, toSample);
            }
            else ++i;
        }

        // Note-offs pendientes que caen en este bloque
        for (int i = 0; i < numPending;)
        {
            if (pending[(size_t) i].offPpq < ppqEnd)
            {
                midi.addEvent (juce::MidiMessage::noteOff (channel, pending[(size_t) i].note), toSample (pending[(size_t) i].offPpq));
                pending[(size_t) i] = pending[(size_t) --numPending];
            }
            else ++i;
        }

        const double swingOffset = (juce::jlimit (0.5f, 0.75f, s.swing) - 0.5) * 2.0 * stepLen;
        const auto first = (juce::int64) std::floor (ppqStart / stepLen) - 1;
        const auto last  = (juce::int64) std::floor (ppqEnd / stepLen);
        const int len = juce::jlimit (1, SeqPattern::kSteps, s.length);

        for (auto n = first; n <= last; ++n)
        {
            if (n <= lastFired) continue;
            const double onset = (double) n * stepLen + ((n & 1) ? swingOffset : 0.0);
            if (onset < ppqStart || onset >= ppqEnd) continue;
            lastFired = n;

            // Cada carril calcula su propio paso (longitud propia + dirección)
            const int idx = directionIndex (n, len, s.direction, rng);
            currentStep = idx;
            std::array<int, kNumValueLanes> li {};
            for (int l = 0; l < kNumValueLanes; ++l)
            {
                const int ll = s.laneLength[(size_t) l] > 0 ? s.laneLength[(size_t) l] : len;
                li[(size_t) l] = directionIndex (n, juce::jlimit (1, SeqPattern::kSteps, ll), s.direction, rng);
                laneStep[(size_t) l] = li[(size_t) l];
            }

            const bool on = euclidHit (idx, s.pulses, len, s.rotate) != pattern.get (idx).flip;
            const float prob = pattern.get (li[(size_t) Lane::Prob]).prob;
            if (! on || rng.nextFloat() >= prob * s.globalProb)
                continue;

            // Condición del paso: se evalúa con el número de vuelta del carril COND
            {
                const int condLen = s.laneLength[(size_t) Lane::Cond] > 0 ? s.laneLength[(size_t) Lane::Cond] : len;
                const auto cycle = (juce::int64) std::floor ((double) n / (double) juce::jmax (1, condLen));
                bool pass = true;
                switch ((Cond) pattern.get (li[(size_t) Lane::Cond]).cond)
                {
                    case Cond::OneOfTwo:   pass = cycle % 2 == 0; break;
                    case Cond::TwoOfTwo:   pass = cycle % 2 == 1; break;
                    case Cond::OneOfThree: pass = cycle % 3 == 0; break;
                    case Cond::OneOfFour:  pass = cycle % 4 == 0; break;
                    case Cond::Fill:       pass = s.fill; break;
                    case Cond::NotFill:    pass = ! s.fill; break;
                    default: break;
                }
                if (! pass)
                    continue;
            }

            int semis = pattern.get (li[(size_t) Lane::Note]).semis;
            int octaveSemis = pattern.get (li[(size_t) Lane::Octave]).octave * 12;
            float velocity = pattern.get (li[(size_t) Lane::Velocity]).velocity;
            if (s.marbles)
            {
                // Distribución con SPREAD (ancho) y BIAS (hacia graves / agudos), cuantizada a la escala
                marbNotes.set_deja_vu (s.dejaVu);
                marbNotes.set_length (juce::jlimit (1, marbles::kDejaVuBufferSize, s.marblesLength));
                marbVel.set_deja_vu (s.dejaVu);
                marbVel.set_length (juce::jlimit (1, marbles::kDejaVuBufferSize, s.marblesLength));
                const float u = marbles::BetaDistributionSample (marbNotes.NextValue (false, 0.0f), s.spread, s.bias);
                const int span = 2 + juce::roundToInt (juce::jlimit (0.0f, 1.0f, s.range) * 12.0f);   // grados de la escala
                semis = mm::degreeToSemis (s.scale, juce::roundToInt (u * (float) span) - span / 3);
                octaveSemis = 0;
                velocity = juce::jlimit (0.3f, 1.0f, 0.55f + 0.45f * marbVel.NextValue (false, 0.0f));
            }
            float gateSteps = pattern.get (li[(size_t) Lane::Gate]).gate * s.gate * 2.5f; // GATE global 0.4 = tal cual

            // RANDOM con velocidad (RATE): el desvío se sostiene durante cada período
            if (s.randomAmt > 0.0f)
            {
                const auto period = s.randomRatePpq > 0.0 ? (juce::int64) std::floor (onset / s.randomRatePpq) : n;
                if (period != heldPeriod)
                {
                    heldPeriod = period;
                    held.degreeShift = rng.nextFloat() < s.randomAmt * 0.5f
                                           ? (rng.nextBool() ? 1 : -1) * (rng.nextFloat() < s.randomAmt * 0.5f ? 2 : 1) : 0;
                    held.octaveShift = rng.nextFloat() < s.randomAmt * 0.15f ? (rng.nextBool() ? 12 : -12) : 0;
                    held.velocity    = (rng.nextFloat() * 2.0f - 1.0f) * s.randomAmt * 0.4f;
                }
                if (held.degreeShift != 0)
                    semis = mm::degreeToSemis (s.scale, mm::semisToNearestDegree (s.scale, semis) + held.degreeShift);
                octaveSemis += held.octaveShift;
                velocity = juce::jlimit (0.05f, 1.0f, velocity + held.velocity);
            }

            // LONG/SHORT: algunas notas pasan a ser largas (1.5–4 pasos) y otras cortas (staccato)
            if (s.longShort > 0.0f)
            {
                const float r = rng.nextFloat();
                if (r < s.longShort * 0.35f)      gateSteps = 1.5f + rng.nextFloat() * (kMaxGateSteps - 1.5f);
                else if (r < s.longShort * 0.7f)  gateSteps = 0.06f + rng.nextFloat() * 0.14f;
            }
            gateSteps = juce::jlimit (0.05f, kMaxGateSteps, gateSteps);

            // TIME: retraso aleatorio de hasta ~45 % de un paso
            const double onPpq = onset + (s.randomTime > 0.0f ? rng.nextFloat() * s.randomTime * 0.45 * stepLen : 0.0);
            const int note = juce::jlimit (0, 127, s.baseNote + semis + octaveSemis);
            if (lockTable != nullptr) lockTable[note] = pattern.get (li[(size_t) Lane::Lock]).lock;
            if (modelTable != nullptr) modelTable[note] = pattern.get (li[(size_t) Lane::Model]).model;

            // RATCHET: repite el golpe 1-4 veces dentro del paso
            const int ratchet = juce::jlimit (1, SeqPattern::kMaxRatchet, pattern.get (li[(size_t) Lane::Ratchet]).ratchet);
            const double sub = stepLen / ratchet;
            const double noteLen = ratchet > 1 ? std::min ((double) gateSteps * stepLen, sub * 0.8) : (double) gateSteps * stepLen;
            for (int k = 0; k < ratchet; ++k)
            {
                const double on = onPpq + k * sub;
                const float v = juce::jlimit (0.05f, 1.0f, velocity * (1.0f - 0.12f * (float) k)); // repeticiones algo más suaves
                if (on < ppqEnd)
                    startNote (midi, note, v, on, on + noteLen, ppqEnd, toSample);
                else if (numPendingOn < (int) pendingOn.size())
                    pendingOn[(size_t) numPendingOn++] = { note, v, on, on + noteLen };
            }
        }
    }

private:
    template <typename ToSample>
    void startNote (juce::MidiBuffer& midi, int note, float velocity, double onPpq, double offPpq,
                    double ppqEnd, ToSample&& toSample)
    {
        const int onSample = toSample (onPpq);

        // Si la misma nota sigue sonando (nota larga), se corta justo antes de volver a tocarla
        for (int i = 0; i < numPending; ++i)
            if (pending[(size_t) i].note == note)
            {
                midi.addEvent (juce::MidiMessage::noteOff (channel, note), onSample);
                pending[(size_t) i] = pending[(size_t) --numPending];
                break;
            }

        midi.addEvent (juce::MidiMessage::noteOn (channel, note, juce::jlimit (0.05f, 1.0f, velocity)), onSample);
        if (offPpq < ppqEnd)
            midi.addEvent (juce::MidiMessage::noteOff (channel, note), juce::jmax (onSample + 1, toSample (offPpq)));
        else if (numPending < (int) pending.size())
            pending[(size_t) numPending++] = { note, offPpq };
    }

    void flushAll (juce::MidiBuffer& midi, int sample)
    {
        for (int i = 0; i < numPending; ++i)
            midi.addEvent (juce::MidiMessage::noteOff (channel, pending[(size_t) i].note), sample);
        numPending = 0;
        numPendingOn = 0;
    }

    struct Pending   { int note = 0; double offPpq = 0.0; };
    struct PendingOn { int note = 0; float velocity = 0.8f; double onPpq = 0.0, offPpq = 0.0; };
    struct Held      { int degreeShift = 0, octaveShift = 0; float velocity = 0.0f; };

    double sampleRate = 44100.0, expectedPpq = 0.0, lastStepLen = 0.0;
    bool wasRunning = false;
    juce::int64 lastFired = 0, heldPeriod = -1;
    Held held;
    std::array<Pending, 64> pending {};
    std::array<PendingOn, 32> pendingOn {};
    int numPending = 0, numPendingOn = 0;
    std::atomic<int> currentStep { -1 };
    std::array<std::atomic<int>, kNumValueLanes> laneStep {};
    juce::Random rng;
    marbles::RandomGenerator marbGen;
    marbles::RandomStream marbStream;
    marbles::RandomSequence marbNotes, marbVel;

    void initMarbles (uint32_t seed) noexcept
    {
        marbGen.Init (seed);
        marbStream.Init (&marbGen);
        marbNotes.Init (&marbStream);
        marbVel.Init (&marbStream);
    }
public:
    void reseed (juce::int64 seed) { rng.setSeed (seed); initMarbles ((uint32_t) seed ^ 0x6d61726bu); }
private:
};

} // namespace cz
