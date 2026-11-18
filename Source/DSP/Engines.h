#pragma once

#include "Resonators.h"

namespace cz
{

// Fuente de ruido blanco barata (xorshift), sin estado compartido
struct Noise
{
    uint32_t state = 0x9E3779B9u;
    void seed (uint32_t s) noexcept { state = s ? s : 1u; }
    float next() noexcept
    {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        return (float) (int32_t) state * (1.0f / 2147483648.0f);
    }
};

//==============================================================================
// MOTOR A — RESONATOR (resonador modal con excitador):
// un excitador (pluck/strike/bow/blow) alimenta un resonador modal o de cuerdas.
//   structure  → inarmonicidad / rigidez / acorde simpático
//   brightness → inclinación espectral y pérdidas de agudos
//   damping    → tiempo de decaimiento
//   position   → punto de excitación (filtro peine)
// Modelos acústicos (referencia: cómo modelan pianos y pianos eléctricos los sintes de modelado físico):
//   Piano → tres cuerdas por nota (desafinación = STRUCTURE), rigidez que crece hacia los agudos,
//           apagador al soltar la tecla. Tine → lengüeta + barra de tono con pastilla magnética
//           (STRUCTURE = cercanía de la pastilla). Reed → lámina de acero con pastilla
//           electrostática (más gruñido con la velocidad).
enum class ResModel   { Modal = 0, String, Sympathetic, Metallic, Piano, Tine, Reed };

// Calibración de nivel por modelo y excitador: un golpe fuerte (velocidad 1) llega a la voz
// a unos -6 dB, sin saturar. Los valores salen de la prueba "calib" (URingDemo).
namespace calib
{
    inline float resonator[7][5] { { 0.037890f, 0.016341f, 0.161983f, 0.234141f, 0.059735f }, { 0.306930f, 0.248800f, 0.712499f, 0.896118f, 0.425621f }, { 0.278676f, 0.263112f, 0.311503f, 0.341891f, 0.303547f }, { 0.054223f, 0.023265f, 0.218462f, 0.315224f, 0.106136f }, { 0.094434f, 0.076102f, 0.316403f, 0.424501f, 0.130060f }, { 0.092854f, 0.032350f, 0.164778f, 0.222362f, 0.084249f }, { 0.098131f, 0.055796f, 0.166367f, 0.214200f, 0.106197f } };
    inline float impact[12][4] { { 0.007799f, 0.164958f, 0.007799f, 0.219777f }, { 0.015717f, 0.185373f, 0.015717f, 0.230413f }, { 0.014395f, 0.264167f, 0.013602f, 0.202658f }, { 0.011297f, 0.184988f, 0.011297f, 0.185153f }, { 0.015462f, 0.182906f, 0.015462f, 0.223575f }, { 0.015654f, 0.236399f, 0.015350f, 0.186530f }, { 0.010005f, 0.182451f, 0.010005f, 0.177112f }, { 0.010997f, 0.178985f, 0.010997f, 0.172742f }, { 0.012674f, 0.151098f, 0.012408f, 0.189970f }, { 0.007408f, 0.146284f, 0.007408f, 0.138880f }, { 0.007799f, 0.164958f, 0.007799f, 0.219777f }, { 0.014395f, 0.264167f, 0.013602f, 0.202658f } };
}
// Hammer: martillo de fieltro; cuanto más fuerte el golpe, más corto el contacto y más brillo
// ENGINE C: el resonador no tiene excitador propio; lo hace vibrar el motor C (Plaits) de la misma voz
enum class ResExciter { Pluck = 0, Strike, Bow, Blow, Hammer, EngineC };

struct ResParams
{
    ResModel model = ResModel::Modal;
    ResExciter exciter = ResExciter::Pluck;
    float structure = 0.3f, brightness = 0.5f, damping = 0.4f, position = 0.3f;
    float decayScale = 1.0f; // FREEZE: alarga muchísimo la resonancia
};

class ResonatorEngine
{
public:
    void prepare (double sr)
    {
        sampleRate = (float) sr;
        for (auto& s : strings) s.prepare (sr);
        excLine.prepare ((int) (sr / 15.0) + 8);
        reset();
    }

    void reset() noexcept
    {
        bank.reset();
        for (auto& s : strings) s.reset();
        excLine.reset();
        burst = 0; pulse = 0; excLp = 0.0f; bowEnv = 0.0f; gate = false;
    }

    void trigger (float velocity, uint32_t seed) noexcept
    {
        vel = velocity;
        noise.seed (seed);
        gate = true;
        burst = (int) std::max (48.0f, period);
        pulseWidth = 2 + (int) ((1.0f - p.brightness) * 0.002f * sampleRate);
        pulse = pulseWidth;
        // Martillo: contacto de 0,4 a 3 ms según dureza (BRIGHTNESS) y velocidad
        hammerWidth = juce::jmax (4, (int) (sampleRate * (0.0004f + 0.0026f * (1.0f - p.brightness) * (1.15f - 0.75f * velocity))));
        hammer = hammerWidth;
        thump = (int) (0.004f * sampleRate);
        dcX = dcY = 0.0f;
    }

    void release() noexcept { gate = false; }

    // Se llama por bloques cortos (32 muestras): recalcula coeficientes con la modulación aplicada
    void update (float freq, const ResParams& params) noexcept
    {
        p = params;
        // El golpe se mide en tiempo: su área en muestras crece con la frecuencia de muestreo, así que
        // la entrada al banco modal se escala por 48 kHz / fs (mismo nivel a 44.1, 96 o con oversampling)
        // ENGINE C usa la calibración del arco (excitación continua)
        const int excCal = p.exciter == ResExciter::EngineC ? (int) ResExciter::Bow : juce::jlimit (0, 4, (int) p.exciter);
        cal = calib::resonator[juce::jlimit (0, 6, (int) p.model)][excCal];
        modalIn = cal * 48000.0f / sampleRate;
        // Arco y soplido no paran de meter energía: con FREEZE (resonancia 40 veces más larga) el nivel
        // crecería sin límite; se compensa para que el drone quede al mismo volumen que la nota
        if (p.exciter == ResExciter::Bow || p.exciter == ResExciter::Blow || p.exciter == ResExciter::EngineC)
            modalIn /= std::sqrt (juce::jmax (1.0f, p.decayScale));
        f0 = juce::jlimit (20.0f, sampleRate * 0.4f, freq);
        period = sampleRate / f0;
        excLpCoef = 0.05f + 0.9f * p.brightness * p.brightness;
        if (p.exciter == ResExciter::Hammer) // golpe fuerte = más agudos (el fieltro se endurece)
        {
            const float h = juce::jlimit (0.0f, 1.0f, p.brightness * 0.65f + vel * 0.45f);
            excLpCoef = 0.04f + 0.92f * h * h;
        }

        float t60 = 12.0f * std::exp (-p.damping * 5.5f) * p.decayScale;
        const float pos = 0.02f + 0.48f * p.position;

        if (p.model == ResModel::Piano)
        {
            // Las notas graves suenan más; al soltar la tecla actúa el apagador (no en el registro agudo, como en un piano)
            t60 *= juce::jlimit (0.35f, 2.2f, std::sqrt (220.0f / f0));
            if (! gate && p.decayScale < 2.0f && f0 < 1400.0f) t60 = 0.18f;
            const float octaves = std::log2 (f0 / 55.0f);
            const float stiffness = juce::jlimit (0.05f, 0.85f, 0.08f + octaves * 0.09f);
            const float cents = 0.5f + p.structure * 7.0f;
            static const float spread[3] { -1.0f, 0.15f, 1.0f };
            for (int s = 0; s < 3; ++s)
                strings[(size_t) s].setParams (f0 * std::exp2 (spread[s] * cents / 1200.0f), t60 * (s == 1 ? 2.0f : 1.6f),
                                               0.35f + p.brightness * 0.6f, stiffness);
            return;
        }
        if (p.model == ResModel::Tine || p.model == ResModel::Reed)
        {
            if (! gate && p.decayScale < 2.0f) t60 = 0.09f; // apagador de fieltro
            ModalBank::Spec spec;
            const bool tine = p.model == ResModel::Tine;
            // Tine: fundamental + 2.º armónico de la barra + "ladrido" inarmónico de la lengüeta (corto, con la velocidad)
            // Reed: lámina empotrada 1 : 6.27 : 17.55
            const float ratios[] { 1.0f, tine ? 2.0f : 6.27f, tine ? 7.18f : 17.55f, tine ? 19.8f : 34.4f };
            const float gains[]  { 1.0f, tine ? 0.08f : 0.22f * vel, tine ? 0.15f + 0.55f * vel * p.brightness : 0.06f, 0.04f * p.brightness };
            const float t60s[]   { t60 * juce::jlimit (0.4f, 2.0f, std::sqrt (330.0f / f0)), t60 * 0.5f, 0.11f, 0.04f };
            spec.count = 4;
            for (int i = 0; i < 4; ++i) { spec.ratio[(size_t) i] = ratios[i]; spec.gain[(size_t) i] = gains[i]; spec.t60[(size_t) i] = t60s[i]; }
            bank.configure (f0, spec, sampleRate);
            pickupDrive = tine ? 1.0f + p.structure * 5.0f : 1.5f + p.structure * 3.0f + vel * 3.0f;
            // En los extremos la saturación de la pastilla deforma la afinación (y en el agudo genera
            // aliasing): se suaviza fuera del registro central
            pickupDrive = 1.0f + (pickupDrive - 1.0f) * juce::jlimit (0.25f, 1.0f, std::min (f0 / 80.0f, 1000.0f / f0));
            return;
        }

        if (p.model == ResModel::Modal || p.model == ResModel::Metallic)
        {
            ModalBank::Spec spec;
            spec.count = ModalBank::kMaxModes;
            // Como en las cuerdas reales, la inarmonicidad es menor en el grave (si no, los graves
            // suenan desafinados hacia arriba)
            const float stiff = p.structure * p.structure * 0.02f * juce::jlimit (0.15f, 1.0f, f0 / 261.6f);
            const float tilt = 1.6f - p.brightness * 1.4f;
            const float hf = 0.02f + (1.0f - p.brightness) * 0.25f;

            for (int i = 0; i < spec.count; ++i)
            {
                const float k = (float) (i + 1);
                // Modal: la rigidez estira los armónicos, pero la fundamental queda en su nota
                // (antes todo el espectro subía y los graves sonaban hasta +37 cents)
                const float ratio = p.model == ResModel::Modal
                                        ? k * std::sqrt ((1.0f + stiff * k * k) / (1.0f + stiff))
                                        : std::pow (k, 1.3f + p.structure * 0.8f);
                spec.ratio[(size_t) i] = ratio;
                spec.gain[(size_t) i]  = std::pow (k, -tilt) * std::max (0.05f, std::abs (std::sin (juce::MathConstants<float>::pi * k * pos)));
                spec.t60[(size_t) i]   = t60 / (1.0f + (ratio - 1.0f) * hf);
            }
            bank.configure (f0, spec, sampleRate);
        }
        else
        {
            // El filtro de pérdidas del lazo acorta el decaimiento: se compensa alargando el T60
            strings[0].setParams (f0, t60 * 1.8f, p.brightness, p.structure * 0.6f);
            if (p.model == ResModel::Sympathetic)
            {
                // structure elige el acorde de las cuerdas simpáticas
                static const float chords[5][3] { { 7, 12, 19 }, { 5, 12, 17 }, { 4, 7, 12 }, { 3, 7, 10 }, { 12, 19, 24 } };
                const auto& c = chords[juce::jlimit (0, 4, (int) (p.structure * 4.99f))];
                for (int s = 1; s < 4; ++s)
                    strings[(size_t) s].setParams (f0 * std::exp2 (c[s - 1] / 12.0f), t60 * 2.5f, p.brightness * 0.8f, 0.1f);
            }
        }
    }

    float process (float extra) noexcept
    {
        // ---- Excitador
        float e = 0.0f;
        switch (p.exciter)
        {
            case ResExciter::Pluck:
                if (burst > 0) { --burst; e = noise.next() * vel; }
                break;
            case ResExciter::Strike:
                if (pulse > 0)
                {
                    --pulse;
                    e = std::sin (juce::MathConstants<float>::pi * (float) pulse / (float) pulseWidth) * vel * 1.5f;
                }
                break;
            case ResExciter::Bow:
                bowEnv += ((gate ? 1.0f : 0.0f) - bowEnv) * 0.0008f;
                e = noise.next() * 0.05f * vel * bowEnv;
                break;
            case ResExciter::Blow:
                bowEnv += ((gate ? 1.0f : 0.0f) - bowEnv) * 0.0004f;
                e = noise.next() * 0.035f * vel * bowEnv;
                break;
            case ResExciter::Hammer:
                // Pulso de contacto (seno al cuadrado) + un "thump" de ruido grave muy corto
                if (hammer > 0)
                {
                    --hammer;
                    const float s = std::sin (juce::MathConstants<float>::pi * (float) hammer / (float) hammerWidth);
                    // Área del pulso proporcional a la velocidad (un golpe suave es más ancho pero no más fuerte)
                    e = s * s * vel * 0.45f * juce::jmin (2.0f, 0.0007f * sampleRate / (float) hammerWidth);
                }
                if (thump > 0) { --thump; e += noise.next() * 0.04f * vel * (float) thump / (0.004f * sampleRate); }
                break;
            case ResExciter::EngineC:   // la excitación llega toda por `extra` (la voz le pasa el motor C)
                break;
        }
        excLp += (e - excLp) * excLpCoef;
        e = excLp + extra;

        // ---- Posición: peine sobre la excitación
        excLine.push (e);
        e -= 0.8f * excLine.read (std::max (1.0f, p.position * 0.5f * period));

        // ---- Resonador
        switch (p.model)
        {
            case ResModel::Piano:
                return (strings[0].process (e) + strings[1].process (e) + strings[2].process (e)) * 2.4f * cal;
            case ResModel::Tine:
            case ResModel::Reed:
            {
                // Pastilla: la tine es asimétrica (armónicos pares), la lámina más simétrica (impares)
                const float x = bank.process (e * modalIn) * 0.6f;
                const float d = pickupDrive;
                const float y = p.model == ResModel::Tine ? fastTanh (d * x + 0.35f * d * x * x) / std::sqrt (d)
                                                          : fastTanh (d * x + 0.08f * d * x * x) / std::sqrt (d);
                dcY = y - dcX + 0.995f * dcY; // quita la continua que deja la asimetría
                dcX = y;
                return dcY * 1.4f;
            }
            case ResModel::String:
                return strings[0].process (e) * 2.2f * cal;
            case ResModel::Sympathetic:
            {
                const float main = strings[0].process (e);
                float sym = 0.0f;
                for (int s = 1; s < 4; ++s)
                    sym += strings[(size_t) s].process (main * 0.12f);
                return (main + sym) * 1.8f * cal;
            }
            default:
                return bank.process (e * modalIn) * 0.6f;
        }
    }

private:
    float sampleRate = 44100.0f, f0 = 220.0f, period = 200.0f, vel = 1.0f;
    ResParams p;
    ModalBank bank;
    std::array<KsString, 4> strings;
    DelayLine excLine;
    Noise noise;
    int burst = 0, pulse = 0, pulseWidth = 4, hammer = 0, hammerWidth = 8, thump = 0;
    float excLp = 0.0f, excLpCoef = 0.5f, bowEnv = 0.0f, pickupDrive = 2.0f, dcX = 0.0f, dcY = 0.0f;
    float cal = 1.0f, modalIn = 1.0f;
    bool gate = false;
};

//==============================================================================
// MOTOR B — IMPACT (inspirado en un módulo de percusión física tipo Plonk):
// un golpe (mazo/ruido/rebote/crujido) excita un objeto resonante.
//   material → madera (agudos se apagan rápido) … metal (todo resuena)
//   decay    → tiempo de decaimiento
//   tone     → dureza/brillo del excitador
//   position → punto de golpe
//   spread   → desafinación aleatoria de parciales (caos inarmónico)
//   sweep    → caída de afinación al golpear (tom/kick)
// Marimba / Vibes / Mbira / SteelPan: instrumentos afinados (referencia: los mallets y membranas
// de los sintes de modelado físico). El mazo responde a la velocidad: golpe fuerte = contacto
// más corto = más brillo.
enum class ImpModel   { Membrane = 0, Bar, Plate, Bell, Tube, Glass, Marimba, Vibes, Mbira, SteelPan, HandDrum, Gong };
enum class ImpExciter { Mallet = 0, Noise, Bounce, Crackle };

struct ImpParams
{
    ImpModel model = ImpModel::Membrane;
    ImpExciter exciter = ImpExciter::Mallet;
    float material = 0.5f, decay = 0.4f, tone = 0.5f, position = 0.3f, spread = 0.0f, sweep = 0.0f;
    float decayScale = 1.0f;
};

class ImpactEngine
{
public:
    void prepare (double sr)
    {
        sampleRate = (float) sr;
        reset();
    }

    void reset() noexcept
    {
        bank.reset();
        pulse = 0; noiseEnv = 0.0f; crackleEnv = 0.0f; bounceLeft = 0; sweepEnv = 0.0f; excLp = 0.0f;
    }

    void trigger (float velocity, uint32_t seed) noexcept
    {
        vel = velocity;
        noise.seed (seed);
        for (auto& d : detune) d = noise.next();
        sweepEnv = 1.0f;
        hardness = 1.3f - 0.65f * velocity;
        setPulseWidth();
        startPulse (1.0f);
        noiseEnv = 1.0f;
        crackleEnv = 1.0f;
        bounceLeft = 9;
        bounceInterval = 0.14f * sampleRate;
        bounceTimer = bounceInterval;
        bounceAmp = 1.0f;
    }

    void release() noexcept {}

    void update (float freq, const ImpParams& params, int chunkSamples) noexcept
    {
        p = params;
        const float f = juce::jlimit (20.0f, sampleRate * 0.4f, freq * (1.0f + p.sweep * 3.0f * sweepEnv));
        sweepEnv *= std::exp (-(float) chunkSamples / (0.035f * sampleRate));
        const float t = juce::jlimit (0.0f, 1.0f, p.tone + (vel - 0.6f) * 0.3f);
        excLpCoef = 0.03f + 0.95f * t * t;
        setPulseWidth();

        const float* table = Ratios::membrane;
        int count = 16;
        switch (p.model)
        {
            case ImpModel::Bar:   table = Ratios::bar;   count = 8;  break;
            case ImpModel::Plate: table = Ratios::plate; count = 24; break;
            case ImpModel::Bell:  table = Ratios::bell;  count = 16; break;
            case ImpModel::Tube:  table = Ratios::tube;  count = 16; break;
            case ImpModel::Glass: table = Ratios::glass; count = 16; break;
            case ImpModel::Marimba:  table = Ratios::marimba;  count = 8;  break;
            case ImpModel::Vibes:    table = Ratios::vibes;    count = 8;  break;
            case ImpModel::Mbira:  table = Ratios::mbira;  count = 8;  break;
            case ImpModel::SteelPan: table = Ratios::steelpan; count = 12; break;
            case ImpModel::HandDrum: table = Ratios::membrane; count = 16; break;
            case ImpModel::Gong:     table = Ratios::plate;    count = 24; break;
            default: break;
        }
        // Los modelos nuevos llegan a la voz con margen (sin saturar), así la velocidad cambia el volumen
        // y el brillo como en un instrumento real; los viejos quedan como estaban
        // nivel calibrado por modelo y excitador; la entrada se escala por 48 kHz / fs (golpe medido en tiempo)
        outGain = calib::impact[juce::jlimit (0, 11, (int) p.model)][juce::jlimit (0, 3, (int) p.exciter)];
        modalIn = 48000.0f / sampleRate;
        const bool tuned = p.model == ImpModel::Marimba || p.model == ImpModel::Vibes || p.model == ImpModel::Mbira;

        const float t60 = 0.04f * std::pow (150.0f, p.decay) * p.decayScale;
        const float hf = (1.0f - p.material) * 1.2f + 0.01f;
        const float pos = 0.05f + 0.45f * p.position;
        const float tilt = 1.3f - p.tone * 0.9f;

        ModalBank::Spec spec;
        spec.count = count;
        for (int i = 0; i < count; ++i)
        {
            const float ratio = table[i] * (1.0f + p.spread * 0.07f * detune[(size_t) i] * (i > 0 ? 1.0f : 0.2f));
            spec.ratio[(size_t) i] = ratio;
            spec.gain[(size_t) i]  = std::pow ((float) (i + 1), -tilt)
                                   * std::max (0.08f, std::abs (std::sin (juce::MathConstants<float>::pi * (float) (i + 1) * pos)));
            spec.t60[(size_t) i]   = t60 / (1.0f + hf * (ratio - 1.0f));
            if (tuned && i == 0) { spec.gain[0] *= 1.7f; spec.t60[0] *= 1.6f; }  // el resonador / la caja refuerza la nota
            if (p.model == ImpModel::SteelPan && i < 3) spec.gain[(size_t) i] *= 1.8f;
        }
        bank.configure (f, spec, sampleRate);
    }

    float process (float extra) noexcept
    {
        float e = 0.0f;
        switch (p.exciter)
        {
            case ImpExciter::Mallet:
                e = nextPulse();
                break;
            case ImpExciter::Noise:
                if (noiseEnv > 1.0e-4f)
                {
                    e = noise.next() * noiseEnv * vel * 0.6f;
                    noiseEnv *= 0.9985f;
                }
                break;
            case ImpExciter::Bounce: // pelota rebotando: golpes cada vez más rápidos y suaves
                if (bounceLeft > 0 && --bounceTimer <= 0.0f)
                {
                    startPulse (bounceAmp);
                    bounceAmp *= 0.78f;
                    bounceInterval *= 0.7f;
                    bounceTimer = bounceInterval;
                    --bounceLeft;
                }
                e = nextPulse();
                break;
            case ImpExciter::Crackle: // impulsos aleatorios dispersos (polvo / glitch)
                if (crackleEnv > 1.0e-3f)
                {
                    if (std::abs (noise.next()) < 0.004f * crackleEnv)
                        e = noise.next() * vel * 1.5f;
                    crackleEnv *= 0.99995f;
                }
                break;
        }

        excLp += (e - excLp) * excLpCoef;
        return bank.process ((excLp + extra) * modalIn) * 0.7f * outGain;
    }

private:
    void startPulse (float amp) noexcept { pulse = pulseWidth; pulseAmp = amp; }

    // Ancho del golpe del mazo según TONE. Hasta TONE 0.9 es el de siempre (los presets están calibrados
    // así); más allá el golpe se sigue acortando (más brillo) pero conserva su energía. Antes perdía
    // volumen y con un pulso de 1 muestra daba cero: con TONE al máximo el motor B quedaba mudo.
    void setPulseWidth() noexcept
    {
        pulseWidth = 1 + (int) ((1.0f - p.tone) * 0.004f * sampleRate * hardness);
        const float knee = 1.0f + 0.1f * 0.004f * sampleRate * hardness;   // ancho con TONE 0.9
        pulseComp = std::pow (std::max (1.0f, knee / (float) pulseWidth), 0.75f);   // 0.75: nivel parejo de 0.9 a 1
    }

    float nextPulse() noexcept
    {
        if (pulse <= 0)
            return 0.0f;
        --pulse;
        // ÷ hardness: el área del golpe sigue a la velocidad (el mazo suave da un golpe más ancho, no más fuerte).
        // Muestra al centro de cada paso (+0.5): un golpe de 1 muestra vale 1, no 0.
        const float ph = pulseWidth > 1 ? (float) pulse / (float) pulseWidth : 0.5f;
        return std::sin (juce::MathConstants<float>::pi * ph) * vel * pulseAmp * 1.5f / hardness * pulseComp;
    }

    float sampleRate = 44100.0f, vel = 1.0f;
    ImpParams p;
    ModalBank bank;
    Noise noise;
    std::array<float, ModalBank::kMaxModes> detune {};
    int pulse = 0, pulseWidth = 4, bounceLeft = 0;
    float pulseAmp = 1.0f, noiseEnv = 0.0f, crackleEnv = 0.0f, sweepEnv = 0.0f;
    float bounceInterval = 0.0f, bounceTimer = 0.0f, bounceAmp = 1.0f;
    float excLp = 0.0f, excLpCoef = 0.5f, hardness = 1.0f, outGain = 1.0f, modalIn = 1.0f, pulseComp = 1.0f;
};

//==============================================================================
// Resonancia simpática: 24 cuerdas libres (Do2 a Si3, fundamental + 2.º armónico) que vibran con
// lo que suena. Con el PEDAL apretado quedan sin apagador y cantan largo, como un piano real.
class SympatheticStrings
{
public:
    void prepare (double sr) { sampleRate = (float) sr; setPedal (false, true); for (auto& m : modes) m.reset(); }
    void setPedal (bool down, bool force = false)
    {
        if (down == pedal && ! force) return;
        pedal = down;
        const float t60 = down ? 3.5f : 0.12f;
        for (int k = 0; k < kStrings; ++k)
            for (int h = 0; h < 2; ++h)
            {
                const float f = 65.406f * std::exp2 ((float) k / 12.0f) * (float) (h + 1);
                const float r = std::exp (-6.9078f / (t60 * sampleRate));
                const float G = (h == 0 ? 0.05f : 0.025f);
                modes[(size_t) (k * 2 + h)].set (f, t60, G * 2.0f * (1.0f - r) / std::sin (juce::MathConstants<float>::twoPi * f / sampleRate), sampleRate);
            }
    }
    void process (float* l, float* r, int n, float amount) noexcept
    {
        if (amount < 0.001f) return;
        const float mix = amount * (pedal ? 1.0f : 0.3f);
        for (int i = 0; i < n; ++i)
        {
            const float in = (l[i] + r[i]) * 0.5f;
            float s = 0.0f;
            for (auto& m : modes) s += m.process (in);
            l[i] += s * mix;
            r[i] += s * mix;
        }
    }
private:
    static constexpr int kStrings = 24;
    std::array<Mode, kStrings * 2> modes;
    float sampleRate = 44100.0f;
    bool pedal = false;
};

//==============================================================================
// BODY: caja de resonancia de madera (modos fijos de cuerpo, distintos en L y R) que suena junto
// con el instrumento, como la tapa armónica de un piano o el cuerpo de una guitarra.
class BodyResonator
{
public:
    void prepare (double sr)
    {
        static const float fr[] { 96.0f, 141.0f, 198.0f, 262.0f, 349.0f, 468.0f, 611.0f, 838.0f, 1147.0f, 1590.0f, 2210.0f, 3050.0f };
        static const float t[]  { 0.42f, 0.38f, 0.34f, 0.30f, 0.26f, 0.22f, 0.19f, 0.16f, 0.13f, 0.10f, 0.08f, 0.06f };
        // Ganancia de cada modo escalada para que su pico de resonancia valga ~G (si no, un modo
        // largo amplifica decenas de veces lo que entra a su frecuencia)
        auto peakGain = [sr] (float t60, float G) { const float r = std::exp (-6.9078f / (t60 * (float) sr)); return G * 2.0f * (1.0f - r); };
        for (int i = 0; i < kModes; ++i)
        {
            const float G = 0.35f / std::sqrt ((float) (i + 1));
            const float fR = fr[i] * (i % 2 ? 1.035f : 0.968f);
            modesL[(size_t) i].set (fr[i], t[i], peakGain (t[i], G) / std::sin (juce::MathConstants<float>::twoPi * fr[i] / (float) sr), (float) sr);
            modesR[(size_t) i].set (fR, t[i] * 0.9f, peakGain (t[i] * 0.9f, G) / std::sin (juce::MathConstants<float>::twoPi * fR / (float) sr), (float) sr);
        }
        for (auto& m : modesL) m.reset();
        for (auto& m : modesR) m.reset();
    }
    void process (float* l, float* r, int n, float amount) noexcept
    {
        if (amount < 0.001f) return;
        for (int i = 0; i < n; ++i)
        {
            const float in = (l[i] + r[i]) * 0.5f;
            float bl = 0.0f, br = 0.0f;
            for (int k = 0; k < kModes; ++k) { bl += modesL[(size_t) k].process (in); br += modesR[(size_t) k].process (in); }
            l[i] = l[i] * (1.0f - 0.25f * amount) + bl * amount * 0.55f;
            r[i] = r[i] * (1.0f - 0.25f * amount) + br * amount * 0.55f;
        }
    }
private:
    static constexpr int kModes = 12;
    std::array<Mode, kModes> modesL, modesR;
};

} // namespace cz
