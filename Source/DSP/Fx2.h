#pragma once

// Efectos de la página TEXTURE y etapa de salida:
//   TextureFx  → procesador granular de Mutable Instruments Clouds (MIT), con conversión de
//                frecuencia de muestreo (Clouds trabaja a 32 kHz en bloques de 32 muestras)
//   PlateReverb → reverb de placa (topología clásica de Dattorro, 1997)
//   MasterBus  → EQ de 3 bandas, saturador y limitador de picos (techo -0.3 dBFS)
#include <juce_dsp/juce_dsp.h>
#include "Modules.h"
#include "clouds/dsp/granular_processor.h"
#include <vector>

namespace cz
{

//==============================================================================
struct TextureParams
{
    int mode = 0;                  // 0 Granular, 1 Stretch, 2 Looping Delay, 3 Spectral
    float position = 0.5f, size = 0.5f, pitch = 0.0f, density = 0.6f, texture = 0.5f;
    float mix = 0.5f, spread = 0.5f, feedback = 0.2f, reverb = 0.3f;
    bool freeze = false;
};

class TextureFx
{
public:
    TextureFx()
    {
        large.assign (kSmall + kWorkspace, 0);
        small.assign (kSmall, 0);
        clouds.Init (large.data(), large.size(), small.data(), small.size());
        clouds.set_quality (0);    // estéreo, alta fidelidad
        clouds.set_silence (false); // en el módulo lo inicializa el firmware; acá quedaba sin valor
        clouds.set_playback_mode (clouds::PLAYBACK_MODE_GRANULAR);   // idem: Prepare() lo lee antes del primer process()
    }

    void prepare (double sr)
    {
        hostRate = sr;
        stepIn = kCloudsRate / sr;
        stepOut = kCloudsRate / sr;
        phaseIn = 0.0; phaseOut = 0.0;
        prevL = prevR = 0.0f;
        inCount = 0;
        outQ.assign (kQueue, {});
        outRead = 0; outWrite = kLatencyFrames; // arranca con colchón para no quedarse sin muestras
        for (auto& f : lpIn) f.reset();
        const float fc = (float) std::min (13000.0, sr * 0.45);
        for (auto& f : lpIn) f.setLowPass ((float) sr, fc);
        clouds.Prepare();
    }

    void process (float* l, float* r, int n, const TextureParams& p)
    {
        auto* cp = clouds.mutable_parameters();
        // Clouds interpola tablas con parámetro × largo: con 1.0 exacto lee una posición después del final
        // (en el módulo la perilla nunca llega a 1.0). Se deja apenas por debajo, inaudible.
        auto u = [] (float x) { return juce::jlimit (0.0f, 0.9999f, x); };
        cp->position = u (p.position);
        cp->size = u (p.size);
        cp->pitch = juce::jlimit (-48.0f, 48.0f, p.pitch);
        cp->density = u (p.density);
        cp->texture = u (p.texture);
        cp->dry_wet = 0.9999f;              // la mezcla seco/húmedo se hace acá (sin latencia en lo seco)
        cp->stereo_spread = u (p.spread);
        cp->feedback = u (p.feedback);
        cp->reverb = u (p.reverb);
        cp->freeze = p.freeze;
        cp->trigger = false;
        cp->gate = false;
        clouds.set_playback_mode ((clouds::PlaybackMode) juce::jlimit (0, 3, p.mode));

        const float wet = p.mix, dry = 1.0f - p.mix * p.mix * 0.6f;
        for (int i = 0; i < n; ++i)
        {
            // ---- host → 32 kHz (pasa-bajos + interpolación lineal)
            // entrada saneada: un valor inválido o enorme no puede llegar al búfer de Clouds
            auto safe = [] (float x) { return std::isfinite (x) ? juce::jlimit (-4.0f, 4.0f, x) : 0.0f; };
            const float xl = lpIn[0].process (safe (l[i])), xr = lpIn[1].process (safe (r[i]));
            phaseIn += stepIn;
            while (phaseIn >= 1.0)
            {
                phaseIn -= 1.0;
                const float t = 1.0f - (float) (phaseIn / stepIn);
                pushIn (prevL + (xl - prevL) * juce::jlimit (0.0f, 1.0f, t), prevR + (xr - prevR) * juce::jlimit (0.0f, 1.0f, t));
            }
            prevL = xl; prevR = xr;

            // ---- 32 kHz → host (interpolación cúbica sobre la cola de salida)
            float yl = 0.0f, yr = 0.0f;
            if (available() >= 4)
            {
                const float t = (float) phaseOut;
                const auto& a = outQ[(size_t) outRead];
                const auto& b = outQ[(size_t) ((outRead + 1) % kQueue)];
                const auto& c = outQ[(size_t) ((outRead + 2) % kQueue)];
                const auto& d = outQ[(size_t) ((outRead + 3) % kQueue)];
                yl = cubic (a.l, b.l, c.l, d.l, t);
                yr = cubic (a.r, b.r, c.r, d.r, t);
                phaseOut += stepOut;
                while (phaseOut >= 1.0) { phaseOut -= 1.0; outRead = (outRead + 1) % kQueue; }
            }
            l[i] = l[i] * dry + yl * wet;
            r[i] = r[i] * dry + yr * wet;
        }
    }

private:
    static constexpr double kCloudsRate = 32000.0;
    static constexpr size_t kSmall = 262144;      // ~4 s de memoria por canal (Clouds original: ~1 s)
    static constexpr size_t kWorkspace = 65536;
    static constexpr int kQueue = 1024, kLatencyFrames = 72;

    struct Frame { float l = 0.0f, r = 0.0f; };
    struct OnePoleLp // 2 polos en cascada: antialias antes de bajar a 32 kHz
    {
        float a = 0.5f, s1 = 0.0f, s2 = 0.0f;
        void setLowPass (float sr, float fc) { a = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * fc / sr); }
        void reset() { s1 = s2 = 0.0f; }
        float process (float x) { s1 += a * (x - s1); s2 += a * (s1 - s2); return s2; }
    };

    static float cubic (float y0, float y1, float y2, float y3, float t) noexcept
    {
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }

    int available() const noexcept { return (outWrite - outRead + kQueue) % kQueue; }

    void pushIn (float xl, float xr) noexcept
    {
        auto toShort = [] (float x) { return (short) juce::jlimit (-32767.0f, 32767.0f, x * 32767.0f); };
        inBlock[(size_t) inCount] = { toShort (xl), toShort (xr) };
        if (++inCount < (int) clouds::kMaxBlockSize)
            return;
        inCount = 0;
        clouds.Prepare();
        clouds.Process (inBlock.data(), outBlock.data(), clouds::kMaxBlockSize);
        for (const auto& f : outBlock)
        {
            if (available() >= kQueue - 2) break; // nunca debería pasar: se descarta en vez de pisar
            outQ[(size_t) outWrite] = { (float) f.l / 32768.0f, (float) f.r / 32768.0f };
            outWrite = (outWrite + 1) % kQueue;
        }
    }

    clouds::GranularProcessor clouds;
    std::vector<uint8_t> large, small;
    std::array<clouds::ShortFrame, clouds::kMaxBlockSize> inBlock {}, outBlock {};
    std::vector<Frame> outQ;
    std::array<OnePoleLp, 2> lpIn;
    double hostRate = 48000.0, stepIn = 0.66, stepOut = 0.66, phaseIn = 0.0, phaseOut = 0.0;
    float prevL = 0.0f, prevR = 0.0f;
    int inCount = 0, outRead = 0, outWrite = 0;
};

//==============================================================================
struct PlateParams
{
    float size = 0.6f, decay = 0.6f, damp = 0.4f, preDelayMs = 15.0f, mod = 0.3f, mix = 0.25f;
};

class PlateReverb
{
public:
    void prepare (double sr)
    {
        sampleRate = (float) sr;
        const float k = sampleRate / 29761.0f; // tiempos originales a 29.761 kHz
        auto make = [k] (Line& d, float len, float extra = 0.0f) { d.init ((int) std::ceil (len * k + extra * k) + 4); d.base = len * k; };
        make (pre, 0.25f * 29761.0f);
        for (int i = 0; i < 4; ++i) make (inAp[(size_t) i], kInAp[i]);
        make (modAp[0], 672.0f, 32.0f); make (modAp[1], 908.0f, 32.0f);
        make (del1[0], 4453.0f); make (del1[1], 4217.0f);
        make (ap2[0], 1800.0f);  make (ap2[1], 2656.0f);
        make (del2[0], 3720.0f); make (del2[1], 3163.0f);
        bw = damp1[0] = damp1[1] = 0.0f;
        lfoPhase = 0.0f;
        scale = k;
    }

    void process (float* l, float* r, int n, const PlateParams& p)
    {
        const float sz = 0.45f + 0.55f * juce::jlimit (0.0f, 1.0f, p.size);
        const float decay = 0.2f + 0.78f * juce::jlimit (0.0f, 1.0f, p.decay);
        const float dampC = 0.05f + 0.85f * juce::jlimit (0.0f, 1.0f, p.damp);
        const float bandwidth = 0.9995f - 0.3f * p.damp;
        const float preLen = juce::jlimit (1.0f, (float) pre.size() - 2.0f, p.preDelayMs * 0.001f * sampleRate);
        const float excursion = (2.0f + 14.0f * p.mod) * scale;
        const float lfoInc = juce::MathConstants<float>::twoPi * 0.9f / sampleRate;
        const float wet = p.mix, dry = 1.0f - 0.5f * p.mix * p.mix;

        for (int i = 0; i < n; ++i)
        {
            float x = (l[i] + r[i]) * 0.5f;
            pre.write (x);
            x = pre.read (preLen);
            bw += (x - bw) * bandwidth; // filtro de ancho de banda
            x = bw;
            x = inAp[0].allpass (x, 0.75f, kInAp[0] * scale);
            x = inAp[1].allpass (x, 0.75f, kInAp[1] * scale);
            x = inAp[2].allpass (x, 0.625f, kInAp[2] * scale);
            x = inAp[3].allpass (x, 0.625f, kInAp[3] * scale);

            lfoPhase += lfoInc;
            if (lfoPhase > juce::MathConstants<float>::twoPi) lfoPhase -= juce::MathConstants<float>::twoPi;
            const float m0 = std::sin (lfoPhase) * excursion, m1 = std::cos (lfoPhase) * excursion;

            // Tanque: dos ramas que se realimentan en cruz
            float a = x + tankOut[1] * decay;
            float b = x + tankOut[0] * decay;
            a = modAp[0].allpass (a, -0.7f, modAp[0].base * sz + m0);
            b = modAp[1].allpass (b, -0.7f, modAp[1].base * sz + m1);
            del1[0].write (a); del1[1].write (b);
            a = del1[0].read (del1[0].base * sz);
            b = del1[1].read (del1[1].base * sz);
            damp1[0] += (a - damp1[0]) * (1.0f - dampC);
            damp1[1] += (b - damp1[1]) * (1.0f - dampC);
            a = ap2[0].allpass (damp1[0] * decay, 0.5f, ap2[0].base * sz);
            b = ap2[1].allpass (damp1[1] * decay, 0.5f, ap2[1].base * sz);
            del2[0].write (a); del2[1].write (b);
            tankOut[0] = del2[0].read (del2[0].base * sz);
            tankOut[1] = del2[1].read (del2[1].base * sz);

            auto tap = [this, sz] (Line& d, float t) { return d.read (t * scale * sz); };
            const float yl = tap (del1[1], 266.0f) + tap (del1[1], 2974.0f) - tap (ap2[1], 1913.0f) + tap (del2[1], 1996.0f)
                           - tap (del1[0], 1990.0f) - tap (ap2[0], 187.0f) - tap (del2[0], 1066.0f);
            const float yr = tap (del1[0], 353.0f) + tap (del1[0], 3627.0f) - tap (ap2[0], 1228.0f) + tap (del2[0], 2673.0f)
                           - tap (del1[1], 2111.0f) - tap (ap2[1], 335.0f) - tap (del2[1], 121.0f);
            l[i] = l[i] * dry + yl * 0.6f * wet;
            r[i] = r[i] * dry + yr * 0.6f * wet;
        }
    }

private:
    static constexpr float kInAp[4] { 142.0f, 107.0f, 379.0f, 277.0f };

    struct Line
    {
        std::vector<float> buf;
        int pos = 0;
        float base = 1.0f;
        void init (int len) { buf.assign ((size_t) std::max (8, len), 0.0f); pos = 0; }
        size_t size() const { return buf.size(); }
        void write (float x) { buf[(size_t) pos] = x; pos = (pos + 1) % (int) buf.size(); }
        // lectura `delay` muestras hacia atrás desde la última escrita (interpolación lineal)
        float read (float delay) const
        {
            const int sz = (int) buf.size();
            if (! (delay == delay)) delay = 1.0f;   // NaN: no puede llegar a un índice
            delay = juce::jlimit (1.0f, (float) sz - 2.0f, delay);
            // parte entera y fracción por separado: con "pos - delay" apenas negativo, sumar el largo en
            // float redondeaba al largo exacto y se leía una muestra después del final del búfer
            const int di = (int) delay;
            const float frac = delay - (float) di;
            int i1 = pos - di;           // muestra "delay" entero hacia atrás
            if (i1 < 0) i1 += sz;
            int i0 = i1 - 1;             // una más atrás (para la fracción)
            if (i0 < 0) i0 += sz;
            return buf[(size_t) i1] + (buf[(size_t) i0] - buf[(size_t) i1]) * frac;
        }
        float allpass (float x, float g, float delay)
        {
            const float d = read (delay);
            const float v = x + g * d;
            write (v);
            return d - g * v;
        }
    };

    float sampleRate = 48000.0f, scale = 1.0f, bw = 0.0f, lfoPhase = 0.0f;
    float damp1[2] {}, tankOut[2] {};
    Line pre;
    std::array<Line, 4> inAp;
    std::array<Line, 2> modAp, del1, ap2, del2;
};

//==============================================================================
struct MasterParams
{
    float low = 0.0f, mid = 0.0f, midHz = 1000.0f, high = 0.0f; // dB
    float drive = 0.0f;
    bool limiter = true;
};

class MasterBus
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        using AC = juce::dsp::IIR::ArrayCoefficients<float>;
        for (auto& ch : eq)
        {
            ch[0].coefficients = new juce::dsp::IIR::Coefficients<float> (AC::makeLowShelf (sr, 150.0, 0.7f, 1.0f));
            ch[1].coefficients = new juce::dsp::IIR::Coefficients<float> (AC::makePeakFilter (sr, 1000.0, 0.8f, 1.0f));
            ch[2].coefficients = new juce::dsp::IIR::Coefficients<float> (AC::makeHighShelf (sr, 6000.0, 0.7f, 1.0f));
            for (auto& f : ch) f.reset();
        }
        env = 1.0f;
        lastLow = lastMid = lastHigh = lastHz = -999.0f;
        holdGain = 1.0f;
    }

    void process (float* l, float* r, int n, const MasterParams& p)
    {
        if (p.low != lastLow || p.mid != lastMid || p.high != lastHigh || p.midHz != lastHz)
        {
            lastLow = p.low; lastMid = p.mid; lastHigh = p.high; lastHz = p.midHz;
            // Coeficientes sin reservar memoria (seguro en el hilo de audio)
            using AC = juce::dsp::IIR::ArrayCoefficients<float>;
            const auto lo = AC::makeLowShelf (sampleRate, 150.0, 0.7f, juce::Decibels::decibelsToGain (p.low));
            const auto mi = AC::makePeakFilter (sampleRate, juce::jlimit (100.0, sampleRate * 0.4, (double) p.midHz), 0.8f, juce::Decibels::decibelsToGain (p.mid));
            const auto hi = AC::makeHighShelf (sampleRate, 6000.0, 0.7f, juce::Decibels::decibelsToGain (p.high));
            for (auto& ch : eq) { *ch[0].coefficients = lo; *ch[1].coefficients = mi; *ch[2].coefficients = hi; }
        }
        const bool eqOn = p.low != 0.0f || p.mid != 0.0f || p.high != 0.0f;
        const float pre = 1.0f + p.drive * 5.0f, post = 1.0f / (1.0f + p.drive * 2.2f);
        const float ceiling = juce::Decibels::decibelsToGain (-0.3f);
        const float release = std::exp (-1.0f / (0.12f * (float) sampleRate));

        for (int i = 0; i < n; ++i)
        {
            float a = l[i], b = r[i];
            if (eqOn)
            {
                for (auto& f : eq[0]) a = f.processSample (a);
                for (auto& f : eq[1]) b = f.processSample (b);
            }
            if (p.drive > 0.001f) // saturador de cinta: asimétrico suave, compensa el volumen
            {
                a = fastTanh (a * pre + 0.05f * a * a * pre) * post;
                b = fastTanh (b * pre + 0.05f * b * b * pre) * post;
            }
            if (p.limiter)
            {
                // Limitador de picos: ataque inmediato, suelta en ~120 ms; recorte suave por encima del techo
                const float peak = std::max (std::abs (a), std::abs (b));
                const float need = peak > ceiling ? ceiling / peak : 1.0f;
                env = need < env ? need : env + (1.0f - env) * (1.0f - release);
                a *= env; b *= env;
                a = juce::jlimit (-1.0f, 1.0f, a); b = juce::jlimit (-1.0f, 1.0f, b);
            }
            l[i] = a; r[i] = b;
        }
        gainReduction = env;
    }

    float getGainReduction() const noexcept { return gainReduction; }

private:
    using Biquad = juce::dsp::IIR::Filter<float>;
    double sampleRate = 48000.0;
    std::array<std::array<Biquad, 3>, 2> eq {};
    float env = 1.0f, holdGain = 1.0f, gainReduction = 1.0f;
    float lastLow = -999.0f, lastMid = -999.0f, lastHigh = -999.0f, lastHz = -999.0f;
};

} // namespace cz
