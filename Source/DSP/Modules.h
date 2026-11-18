#pragma once

#include "Resonators.h"

namespace cz
{

//==============================================================================
// Filtro multimodo con drive de entrada:
//   SVF (TPT) 12 dB: pasa-bajos, pasa-banda, pasa-altos, notch
//   LADDER 24 dB: escalera de 4 polos ZDF con saturación en el lazo (grave y gordo)
//   COMB: filtro peine afinado al cutoff; RESONANCE = realimentación (tubos, metales, flanger)
//   FORMANT: tres pasa-banda en paralelo que recorren las vocales A-E-I-O-U con el cutoff
enum class FilterType { LowPass = 0, BandPass, HighPass, Notch, Ladder, Comb, Formant };

class Svf
{
public:
    void prepare (double sr)
    {
        sampleRate = (float) sr;
        comb.assign ((size_t) (sr / 20.0) + 8, 0.0f);
        reset();
    }
    void reset() noexcept
    {
        ic1 = ic2 = 0.0f;
        lad.fill (0.0f);
        for (auto& f : fmt) f.s1 = f.s2 = 0.0f;
        std::fill (comb.begin(), comb.end(), 0.0f);
        combPos = 0; combLp = 0.0f;
    }

    void set (float cutoff, float resonance, FilterType type = FilterType::LowPass) noexcept
    {
        const float fc = juce::jlimit (20.0f, sampleRate * 0.45f, cutoff);
        reso = juce::jlimit (0.0f, 1.0f, resonance);
        g = std::tan (juce::MathConstants<float>::pi * fc / sampleRate);
        k = 2.0f - 1.96f * reso;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;

        if (type == FilterType::Ladder)
        {
            G = g / (1.0f + g);
            kl = reso * reso * 4.0f;
        }
        else if (type == FilterType::Comb)
        {
            combLen = juce::jlimit (2.0f, (float) comb.size() - 4.0f, sampleRate / juce::jmax (20.0f, cutoff));
            combFb = 0.15f + 0.83f * reso;
        }
        else if (type == FilterType::Formant)
        {
            // El cutoff (20 Hz - 20 kHz, escala logarítmica) recorre A → E → I → O → U
            static constexpr float vowels[5][3][2] { { { 800, 1.0f }, { 1150, 0.5f }, { 2900, 0.25f } },    // A
                                                     { { 400, 1.0f }, { 1900, 0.4f }, { 2600, 0.3f } },     // E
                                                     { { 300, 1.0f }, { 2300, 0.3f }, { 3000, 0.25f } },    // I
                                                     { { 450, 1.0f }, { 800, 0.5f },  { 2830, 0.2f } },     // O
                                                     { { 325, 1.0f }, { 700, 0.4f },  { 2530, 0.15f } } };  // U
            const float pos = juce::jlimit (0.0f, 3.999f, std::log2 (juce::jlimit (20.0f, 20000.0f, cutoff) / 20.0f) / 9.966f * 4.0f);
            const int i0 = (int) pos;
            const float t = pos - (float) i0;
            const float q = 4.0f + reso * 16.0f;
            for (int f = 0; f < 3; ++f)
            {
                const float hz = vowels[i0][f][0] + (vowels[i0 + 1][f][0] - vowels[i0][f][0]) * t;
                fmt[(size_t) f].gain = vowels[i0][f][1] + (vowels[i0 + 1][f][1] - vowels[i0][f][1]) * t;
                const float gg = std::tan (juce::MathConstants<float>::pi * juce::jmin (hz, sampleRate * 0.45f) / sampleRate);
                const float kk = 1.0f / q;
                fmt[(size_t) f].a1 = 1.0f / (1.0f + gg * (gg + kk));
                fmt[(size_t) f].a2 = gg * fmt[(size_t) f].a1;
                fmt[(size_t) f].a3 = gg * fmt[(size_t) f].a2;
                fmt[(size_t) f].k = kk;
            }
        }
    }

    float process (float x, FilterType type, float drive) noexcept
    {
        x = drive > 0.001f ? fastTanh (x * (1.0f + drive * 6.0f)) / (1.0f + drive * 1.5f) : x;

        if (type == FilterType::Ladder)
        {
            const float G2 = G * G, G3 = G2 * G, G4 = G3 * G;
            const float S = (G3 * lad[0] + G2 * lad[1] + G * lad[2] + lad[3]) / (1.0f + g);
            float in = fastTanh ((x * (1.0f + kl * 0.35f) - kl * S) / (1.0f + kl * G4)); // compensa la pérdida de graves
            for (auto& st : lad)
            {
                const float v = (in - st) * G;
                const float y = v + st;
                st = y + v;
                in = y;
            }
            return in;
        }
        if (type == FilterType::Comb)
        {
            // parte entera y fracción por separado: "combPos - combLen" apenas negativo + largo, en float,
            // redondeaba al largo exacto y leía una muestra después del final del búfer
            const int sz = (int) comb.size();
            const float len = juce::jlimit (2.0f, (float) sz - 4.0f, combLen == combLen ? combLen : 2.0f);
            const int li = (int) len;
            const float t = len - (float) li;
            int i1 = combPos - li;      if (i1 < 0) i1 += sz;
            int i0 = i1 - 1;            if (i0 < 0) i0 += sz;
            const float d = comb[(size_t) i1] + (comb[(size_t) i0] - comb[(size_t) i1]) * t;
            combLp += (d - combLp) * 0.7f;                         // pierde agudos como un tubo real
            comb[(size_t) combPos] = fastTanh (x + combLp * combFb);
            combPos = (combPos + 1) % sz;
            return (x + combLp) * 0.6f;
        }
        if (type == FilterType::Formant)
        {
            float y = 0.0f;
            for (auto& f : fmt)
            {
                const float v3 = x - f.s2;
                const float v1 = f.a1 * f.s1 + f.a2 * v3;
                const float v2 = f.s2 + f.a2 * f.s1 + f.a3 * v3;
                f.s1 = 2.0f * v1 - f.s1;
                f.s2 = 2.0f * v2 - f.s2;
                y += v1 * f.k * f.gain;
            }
            return y * 2.2f;
        }

        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;

        switch (type)
        {
            case FilterType::BandPass: return v1 * k;
            case FilterType::HighPass: return x - k * v1 - v2;
            case FilterType::Notch:    return x - k * v1;
            default:                   return v2;
        }
    }

private:
    struct Band { float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, k = 1.0f, gain = 0.0f, s1 = 0.0f, s2 = 0.0f; };
    float sampleRate = 44100.0f, g = 0.1f, k = 1.4f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, ic1 = 0.0f, ic2 = 0.0f, reso = 0.0f;
    float G = 0.0f, kl = 0.0f;
    std::array<float, 4> lad {};
    std::array<Band, 3> fmt {};
    std::vector<float> comb;
    int combPos = 0;
    float combLen = 100.0f, combFb = 0.5f, combLp = 0.0f;
};

//==============================================================================
// LFO global: seno, triángulo, sierra, cuadrada, sample & hold y deriva suave
enum class LfoShape { Sine = 0, Triangle, Saw, Square, SampleHold, Drift };

class Lfo
{
public:
    void reset() noexcept { phase = 0.0; held = target = prevTarget = 0.0f; }

    // Avanza `samples` muestras y devuelve el valor bipolar (-1..1) al inicio del tramo
    float advance (LfoShape shape, double hz, double sampleRate, int samples) noexcept
    {
        const float ph = (float) phase;
        float v = 0.0f;
        switch (shape)
        {
            case LfoShape::Sine:       v = std::sin (juce::MathConstants<float>::twoPi * ph); break;
            case LfoShape::Triangle:   v = 1.0f - 4.0f * std::abs (ph - 0.5f); break;
            case LfoShape::Saw:        v = 2.0f * ph - 1.0f; break;
            case LfoShape::Square:     v = ph < 0.5f ? 1.0f : -1.0f; break;
            case LfoShape::SampleHold: v = held; break;
            case LfoShape::Drift:
            {
                const float t = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * ph);
                v = prevTarget + (target - prevTarget) * t;
                break;
            }
        }

        phase += hz * samples / sampleRate;
        if (phase >= 1.0)
        {
            phase -= std::floor (phase);
            held = rng.nextFloat() * 2.0f - 1.0f;
            prevTarget = target;
            target = rng.nextFloat() * 2.0f - 1.0f;
        }
        return v;
    }

private:
    double phase = 0.0;
    float held = 0.0f, target = 0.0f, prevTarget = 0.0f;
    juce::Random rng;
public:
    void reseed (juce::int64 seed) { rng.setSeed (seed); }
private:
};

//==============================================================================
// GLITCH (beat repeat): en cada semicorchea del compás, con probabilidad CHANCE, repite el
// pedacito que acaba de sonar (LENGTH) hasta completar 1 o 2 semicorcheas. VARIATION suma
// redobles que se aceleran, reversa y caída de cinta (pitch abajo). CRUSH degrada solo lo
// repetido. Sin clics: entra y sale con fundido de ~4 ms y cada repetición tiene ventana suave.
struct GlitchParams
{
    float prob = 0.3f, lengthFrac = 1.0f, variation = 0.5f, crush = 0.0f, mix = 1.0f;
};

class Glitch
{
public:
    void prepare (double sr)
    {
        sampleRate = (float) sr;
        for (auto& b : buf) b.assign ((size_t) (sr * 4.0), 0.0f);
        writePos = 0; active = false; gain = 0.0f; freePpq = 0.0; lastStep = -1;
        edge = juce::jmax (16, (int) (0.003 * sr));          // fundido cruzado de 3 ms entre repeticiones
        fadeInc = 1.0f / (0.004f * sampleRate);               // fundido de entrada/salida de 4 ms
        lpL = lpR = 0.0f;
    }

    // ppq / playing: posición del host (o reloj interno). Sin transporte el glitch usa su propio reloj.
    void process (float* l, float* r, int n, double bpm, double ppq, bool playing, const GlitchParams& p) noexcept
    {
        const int size = (int) buf[0].size();
        bpm = juce::jlimit (30.0, 300.0, bpm);
        const double ppqPerSample = bpm / (60.0 * sampleRate);
        const double stepPpq = 0.25;                          // semicorchea
        const int stepLen = juce::jmax (64, (int) (sampleRate * 60.0 / bpm * stepPpq));
        if (! playing) ppq = freePpq;

        // CRUSH: menos bits y menos frecuencia de muestreo, con un pasa-bajos suave después
        const float levels = std::exp2 (14.0f - p.crush * 10.0f);
        const float holdLen = 1.0f + p.crush * p.crush * 14.0f;
        const float lpCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * (16000.0f - p.crush * 11000.0f) / sampleRate);

        for (int i = 0; i < n; ++i)
        {
            buf[0][(size_t) writePos] = l[i];
            buf[1][(size_t) writePos] = r[i];

            // ¿empieza una semicorchea nueva?
            const auto stepIdx = (juce::int64) std::floor ((ppq + i * ppqPerSample) / stepPpq + 1.0e-9);
            if (stepIdx != lastStep)
            {
                lastStep = stepIdx;
                if (! active && rng.nextFloat() < p.prob)
                    start (stepLen, p, size);
            }

            float wl = 0.0f, wr = 0.0f;
            if (active)
            {
                const float rate = tapeStop ? juce::jmax (0.25f, 1.0f - 0.75f * (float) played / (float) total) : 1.0f;
                const float len = (float) segLen, e = (float) edge;
                read (readPos, size, wl, wr);
                // últimos `edge` del pedazo: fundido cruzado con el comienzo de la próxima repetición
                if (readPos > len - e)
                {
                    const float t = (readPos - (len - e)) / e;
                    float nl, nr;
                    read (readPos - (len - e), size, nl, nr);
                    const float fo = std::cos (t * juce::MathConstants<float>::halfPi), fi = std::sin (t * juce::MathConstants<float>::halfPi);
                    wl = wl * fo + nl * fi;
                    wr = wr * fo + nr * fi;
                }

                if (p.crush > 0.001f)
                {
                    holdCount += 1.0f;
                    if (holdCount >= holdLen) { holdCount -= holdLen; heldL = std::round (wl * levels) / levels; heldR = std::round (wr * levels) / levels; }
                    lpL += (heldL - lpL) * lpCoef; lpR += (heldR - lpR) * lpCoef;
                    wl = lpL; wr = lpR;
                }

                readPos += rate;
                if (readPos >= len)
                {
                    readPos -= len - e;   // seguimos donde llegó el fundido de entrada
                    // redoble que se acelera: cada repetición puede durar la mitad
                    if (accelerate && segLen > edge * 6) segLen /= 2;
                    readPos = juce::jmin (readPos, (float) segLen - e - 1.0f);
                }
                ++played;
                if (played >= total - (int) (1.0f / fadeInc)) releasing = true;
            }

            // fundido suave entre lo que suena y lo repetido
            const float target = active && ! releasing ? 1.0f : 0.0f;
            gain += juce::jlimit (-fadeInc, fadeInc, target - gain);
            if (releasing && gain <= 0.0f) { active = false; releasing = false; }

            const float g = gain * p.mix;
            l[i] = l[i] * (1.0f - g) + wl * g;
            r[i] = r[i] * (1.0f - g) + wr * g;
            writePos = (writePos + 1) % size;
        }
        if (! playing) freePpq = ppq + n * ppqPerSample;
    }

private:
    void start (int stepLen, const GlitchParams& p, int size) noexcept
    {
        active = true; releasing = false;
        segLen = juce::jlimit (edge * 4, size / 4, (int) ((float) stepLen * p.lengthFrac));
        segStart = writePos - segLen;                          // lo que acaba de sonar
        total = stepLen * (rng.nextFloat() < 0.35f + p.variation * 0.4f ? 2 : 1);
        reverse = rng.nextFloat() < p.variation * 0.35f;
        accelerate = ! reverse && rng.nextFloat() < p.variation * 0.45f;
        tapeStop = ! reverse && ! accelerate && rng.nextFloat() < p.variation * 0.25f;
        readPos = 0.0f; played = 0;
    }

    // Lee el pedazo en la posición x (con reversa e interpolación lineal)
    void read (float x, int size, float& outL, float& outR) const noexcept
    {
        const float pos = reverse ? (float) segLen - 1.0f - x : x;
        const float a = std::fmod ((float) segStart + pos + (float) size * 2.0f, (float) size);
        const int i0 = (int) a;
        const float t = a - (float) i0;
        const int i1 = (i0 + 1) % size;
        outL = buf[0][(size_t) i0] + (buf[0][(size_t) i1] - buf[0][(size_t) i0]) * t;
        outR = buf[1][(size_t) i0] + (buf[1][(size_t) i1] - buf[1][(size_t) i0]) * t;
    }

    float sampleRate = 44100.0f, gain = 0.0f, fadeInc = 0.005f, readPos = 0.0f;
    std::array<std::vector<float>, 2> buf;
    int writePos = 0, segLen = 0, segStart = 0, total = 0, played = 0, edge = 96;
    bool active = false, releasing = false, reverse = false, accelerate = false, tapeStop = false;
    float heldL = 0.0f, heldR = 0.0f, holdCount = 0.0f, lpL = 0.0f, lpR = 0.0f;
    double freePpq = 0.0;
    juce::int64 lastStep = -1;
    juce::Random rng;
public:
    void reseed (juce::int64 seed) { rng.setSeed (seed); }
private:
};

//==============================================================================
// TAPE DELAY (inspirado en un delay híbrido analógico): HP/LP y saturación en la
// realimentación, wow & flutter, ping-pong y deslizamiento de cinta al cambiar el tiempo.
struct DelayParams
{
    float timeMs = 375.0f, feedback = 0.4f, wow = 0.2f, drive = 0.2f;
    float hpHz = 120.0f, lpHz = 6000.0f, mix = 0.3f;
    float duck = 0.0f; // baja los repeats mientras entra señal
    bool pingPong = false;
};

// Chorus estéreo de dos voces (el clásico de los pianos eléctricos): retardos cortos modulados
class Chorus
{
public:
    void prepare (double sr)
    {
        sampleRate = (float) sr;
        lineL.prepare ((int) (sr * 0.05)); lineR.prepare ((int) (sr * 0.05));
        phase = 0.0f;
    }
    void process (float* l, float* r, int n, float mix, float rateHz, float depth) noexcept
    {
        if (mix < 0.001f) return;
        const float inc = rateHz / sampleRate;
        for (int i = 0; i < n; ++i)
        {
            phase += inc; if (phase >= 1.0f) phase -= 1.0f;
            const float mL = std::sin (juce::MathConstants<float>::twoPi * phase);
            const float mR = std::sin (juce::MathConstants<float>::twoPi * (phase + 0.25f));
            const float base = 0.007f * sampleRate, dep = (0.0005f + 0.004f * depth) * sampleRate;
            lineL.push (l[i]); lineR.push (r[i]);
            const float wl = lineL.read (base + dep * mL), wr = lineR.read (base + dep * mR);
            l[i] = l[i] * (1.0f - 0.5f * mix) + wl * mix * 0.7f;
            r[i] = r[i] * (1.0f - 0.5f * mix) + wr * mix * 0.7f;
        }
    }
private:
    DelayLine lineL, lineR;
    float sampleRate = 44100.0f, phase = 0.0f;
};

class TapeDelay
{
public:
    void prepare (double sr)
    {
        sampleRate = (float) sr;
        for (auto& d : lines) d.prepare ((int) (sr * 2.6));
        smoothedTime = 375.0f;
        hpL = hpR = lpL = lpR = 0.0f;
        wowPhase = flutterPhase = 0.0f;
    }

    void process (float* l, float* r, int n, const DelayParams& p) noexcept
    {
        const float hpC = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * p.hpHz / sampleRate);
        const float lpC = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * p.lpHz / sampleRate);
        const float fb = juce::jlimit (0.0f, 1.05f, p.feedback);
        const float driveGain = 1.0f + p.drive * 4.0f;
        const float duckAtt = 1.0f - std::exp (-1.0f / (0.005f * sampleRate));
        const float duckRel = 1.0f - std::exp (-1.0f / (0.25f * sampleRate));

        for (int i = 0; i < n; ++i)
        {
            smoothedTime += (p.timeMs - smoothedTime) * 0.0004f; // la cinta "se desliza" hacia el nuevo tiempo
            wowPhase += 0.6f / sampleRate;     if (wowPhase >= 1.0f) wowPhase -= 1.0f;
            flutterPhase += 6.5f / sampleRate; if (flutterPhase >= 1.0f) flutterPhase -= 1.0f;
            const float mod = p.wow * (3.0f * std::sin (juce::MathConstants<float>::twoPi * wowPhase)
                                     + 0.4f * std::sin (juce::MathConstants<float>::twoPi * flutterPhase));
            const float d = (smoothedTime + mod) * 0.001f * sampleRate;

            float yl = lines[0].read (d), yr = lines[1].read (d * (p.pingPong ? 1.0f : 1.012f));

            // Filtros + saturación en la realimentación
            lpL += (yl - lpL) * lpC; lpR += (yr - lpR) * lpC;
            hpL += (lpL - hpL) * hpC; hpR += (lpR - hpR) * hpC;
            const float fl = fastTanh ((lpL - hpL) * driveGain) / driveGain * fb;
            const float fr = fastTanh ((lpR - hpR) * driveGain) / driveGain * fb;

            if (p.pingPong)
            {
                lines[0].push ((l[i] + r[i]) * 0.5f + fr);
                lines[1].push (fl);
            }
            else
            {
                lines[0].push (l[i] + fl);
                lines[1].push (r[i] + fr);
            }

            // Ducking: seguidor de envolvente de la entrada seca
            const float level = std::max (std::abs (l[i]), std::abs (r[i]));
            duckEnv += (level - duckEnv) * (level > duckEnv ? duckAtt : duckRel);
            const float duckGain = 1.0f - p.duck * juce::jlimit (0.0f, 1.0f, duckEnv * 4.0f);

            l[i] = l[i] * (1.0f - p.mix * 0.3f) + yl * p.mix * duckGain;
            r[i] = r[i] * (1.0f - p.mix * 0.3f) + yr * p.mix * duckGain;
        }
    }

private:
    float sampleRate = 44100.0f, smoothedTime = 375.0f;
    std::array<DelayLine, 2> lines;
    float hpL = 0.0f, hpR = 0.0f, lpL = 0.0f, lpR = 0.0f, wowPhase = 0.0f, flutterPhase = 0.0f, duckEnv = 0.0f;
};

//==============================================================================
// SPRING REVERB: tres muelles por canal. Cada muelle es un lazo de retardo con una
// cadena de allpass de dispersión dentro (el "chirp"/"boing" característico).
struct SpringParams
{
    float tension = 0.5f, decaySec = 2.0f, tone = 0.5f, damp = 0.4f, drip = 0.5f, preDelayMs = 10.0f, mix = 0.25f;
};

class SpringReverb
{
public:
    void prepare (double sr)
    {
        sampleRate = (float) sr;
        for (auto& s : springs)
        {
            s.line.prepare ((int) (sr * 0.12));
            for (auto& ap : s.ap) ap.reset();
            s.lp = 0.0f;
        }
        dcL = dcR = dcxL = dcxR = 0.0f;
        preLine.prepare ((int) (sr * 0.26));
        toneLp = 0.0f;
    }

    void process (float* l, float* r, int n, const SpringParams& p) noexcept
    {
        static constexpr float baseMs[kSprings] { 33.0f, 41.5f, 52.7f, 36.1f, 45.3f, 57.9f };
        // TONE colorea la entrada; DAMP decide cuánto se apagan los agudos dentro de la cola
        const float toneC = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * (600.0f + p.tone * 9000.0f) / sampleRate);
        const float dampC = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * (12000.0f * (1.0f - p.damp) + 700.0f) / sampleRate);
        const float apCoef = -(0.45f + p.drip * 0.45f);
        const float apDelay = 10.0f * (1.0f - apCoef) / (1.0f + apCoef); // retardo de grupo de la cadena de dispersión
        const float preDelay = juce::jlimit (1.0f, (float) preLine.size() - 4.0f, p.preDelayMs * 0.001f * sampleRate);

        float len[kSprings], g[kSprings];
        for (int s = 0; s < kSprings; ++s)
        {
            len[s] = baseMs[s] * (0.6f + p.tension * 0.8f) * 0.001f * sampleRate;
            // Ganancia de lazo para que la cola caiga 60 dB en `decaySec` segundos
            g[s] = std::min (0.985f, std::pow (0.001f, (len[s] + apDelay) / (std::max (0.1f, p.decaySec) * sampleRate)));
            for (auto& ap : springs[(size_t) s].ap) ap.a = apCoef;
        }

        for (int i = 0; i < n; ++i)
        {
            preLine.push ((l[i] + r[i]) * 0.5f);
            toneLp += (preLine.read (preDelay) - toneLp) * toneC;
            const float in = toneLp;
            float outL = 0.0f, outR = 0.0f;

            for (int s = 0; s < kSprings; ++s)
            {
                auto& sp = springs[(size_t) s];
                float v = sp.line.read (len[s]);
                sp.lp += (v - sp.lp) * dampC;
                v = sp.lp;
                for (auto& ap : sp.ap) v = ap.process (v);
                sp.line.push (fastTanh (in * 0.4f + v * g[s]));
                (s < kSprings / 2 ? outL : outR) += v;
            }

            // DC blocker sobre la señal húmeda
            const float wl = outL - dcxL + 0.995f * dcL; dcxL = outL; dcL = wl;
            const float wr = outR - dcxR + 0.995f * dcR; dcxR = outR; dcR = wr;

            l[i] = l[i] * (1.0f - p.mix * 0.25f) + wl * p.mix * 0.6f;
            r[i] = r[i] * (1.0f - p.mix * 0.25f) + wr * p.mix * 0.6f;
        }
    }

private:
    static constexpr int kSprings = 6;
    struct Spring
    {
        DelayLine line;
        std::array<Allpass1, 10> ap;
        float lp = 0.0f;
    };
    std::array<Spring, kSprings> springs;
    DelayLine preLine;
    float sampleRate = 44100.0f, dcL = 0.0f, dcR = 0.0f, dcxL = 0.0f, dcxR = 0.0f, toneLp = 0.0f;
};

} // namespace cz
