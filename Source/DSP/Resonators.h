#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <cmath>
#include <complex>
#include <vector>

namespace cz
{

inline float fastTanh (float x) noexcept
{
    if (x > 3.0f)  return 1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

inline float midiToHz (float note) noexcept { return 440.0f * std::exp2 ((note - 69.0f) / 12.0f); }

//==============================================================================
// Modo resonante en forma acoplada (rotación de un vector u, v que decae con r). Un impulso
// unitario produce una oscilación de amplitud ≈ gain sea cual sea la frecuencia, y cambiar la
// afinación en medio de la nota (SWEEP, LFO, LIFE) no cambia la energía guardada: sin clics
// ni zumbido de "escalones". También es mucho más preciso que la forma directa en los graves.
struct Mode
{
    float c = 1.0f, s = 0.0f, g = 0.0f, u = 0.0f, v = 0.0f, fade = 1.0f;

    void set (float freq, float t60, float gain, float sampleRate) noexcept
    {
        // Debajo de 20 Hz o cerca de Nyquist el modo se apaga suave (sin cortar de golpe)
        if (freq <= 20.0f || freq >= sampleRate * 0.45f || gain <= 0.0f)
        {
            g = 0.0f;
            fade = 0.995f;
            return;
        }
        const float r = std::exp (-6.9078f / (std::max (0.005f, t60) * sampleRate));
        const float w = juce::MathConstants<float>::twoPi * freq / sampleRate;
        c = r * std::cos (w);
        s = r * std::sin (w);
        g = gain;
        fade = 1.0f;
    }

    float process (float x) noexcept
    {
        const float un = c * u - s * v + g * x;
        const float vn = (s * u + c * v) * fade;
        u = un * fade;
        v = vn;
        return v;
    }

    void reset() noexcept { u = v = 0.0f; }
};

// Banco de hasta 24 modos en paralelo
class ModalBank
{
public:
    static constexpr int kMaxModes = 24;

    struct Spec
    {
        std::array<float, kMaxModes> ratio {}, gain {}, t60 {};
        int count = 0;
    };

    void configure (float f0, const Spec& spec, float sampleRate) noexcept
    {
        active = juce::jlimit (0, kMaxModes, spec.count);
        float gainSum = 0.0f;
        for (int i = 0; i < active; ++i)
            gainSum += spec.gain[(size_t) i];
        const float norm = gainSum > 0.0f ? 1.0f / std::sqrt (gainSum) : 1.0f;

        for (int i = 0; i < active; ++i)
            modes[(size_t) i].set (f0 * spec.ratio[(size_t) i], spec.t60[(size_t) i],
                                   spec.gain[(size_t) i] * norm, sampleRate);
    }

    float process (float x) noexcept
    {
        float sum = 0.0f;
        for (int i = 0; i < active; ++i)
            sum += modes[(size_t) i].process (x);
        return sum;
    }

    void reset() noexcept { for (auto& m : modes) m.reset(); }

private:
    std::array<Mode, kMaxModes> modes;
    int active = 0;
};

//==============================================================================
// Tablas de parciales (relativas a la fundamental)
namespace Ratios
{
    // Membrana circular (modos de Bessel)
    inline constexpr float membrane[] { 1.00f, 1.59f, 2.14f, 2.30f, 2.65f, 2.92f, 3.16f, 3.50f,
                                        3.60f, 3.65f, 4.06f, 4.15f, 4.23f, 4.60f, 4.83f, 4.93f };
    // Barra afinada tipo marimba
    inline constexpr float bar[]      { 1.00f, 3.99f, 9.90f, 17.3f, 26.5f, 37.6f, 50.5f, 65.3f,
                                        2.76f, 5.40f, 8.93f, 13.34f, 18.64f, 24.81f, 31.87f, 39.81f };
    // Placa cuadrada (m² + n²) / 2
    inline constexpr float plate[]    { 1.00f, 2.50f, 4.00f, 5.00f, 6.50f, 8.50f, 9.00f, 10.0f,
                                        12.5f, 13.0f, 14.5f, 16.0f, 17.0f, 18.5f, 20.0f, 20.5f,
                                        22.5f, 25.0f, 26.0f, 26.5f, 29.0f, 30.5f, 32.5f, 34.0f };
    // Campana (hum, prime, tierce, quint, nominal…)
    inline constexpr float bell[]     { 0.50f, 1.00f, 1.183f, 1.506f, 2.00f, 2.514f, 2.662f, 3.011f,
                                        4.166f, 5.433f, 6.796f, 8.215f, 9.67f, 11.2f, 12.8f, 14.5f };
    // Tubo cerrado: armónicos impares ligeramente estirados
    inline constexpr float tube[]     { 1.00f, 3.01f, 5.03f, 7.06f, 9.10f, 11.15f, 13.2f, 15.3f,
                                        17.4f, 19.5f, 21.6f, 23.8f, 26.0f, 28.2f, 30.4f, 32.6f };
    // Barra de marimba afinada (1 : 4 : 10, el tubo resonador refuerza la fundamental)
    inline constexpr float marimba[]  { 1.00f, 3.99f, 9.92f, 17.1f, 27.0f, 38.6f, 2.74f, 5.40f };
    // Barra de vibráfono (aluminio, también afinada 1 : 4 : 10)
    inline constexpr float vibes[]    { 1.00f, 3.99f, 9.95f, 17.4f, 26.9f, 2.85f, 6.12f, 13.6f };
    // Lengüeta empotrada con masa en la punta (mbira / piano de pulgar)
    inline constexpr float mbira[]  { 1.00f, 5.92f, 16.4f, 32.0f, 52.6f, 78.4f, 2.1f, 9.3f };
    // Nota de steel pan: armónicos afinados (fundamental, octava, quinta…) + parciales del parche
    inline constexpr float steelpan[] { 1.00f, 2.00f, 3.00f, 4.01f, 5.03f, 6.02f, 2.46f, 3.62f,
                                        4.74f, 7.10f, 8.05f, 9.30f };
    // Copa de cristal
    inline constexpr float glass[]    { 1.00f, 2.32f, 4.25f, 6.63f, 9.38f, 12.5f, 16.0f, 19.8f,
                                        23.9f, 28.3f, 33.0f, 38.0f, 43.3f, 48.9f, 54.8f, 61.0f };
}

//==============================================================================
// Línea de retardo con lectura fraccionaria (interpolación lineal)
class DelayLine
{
public:
    void prepare (int maxSamples)
    {
        buffer.assign ((size_t) juce::nextPowerOfTwo (maxSamples + 4), 0.0f);
        mask = (int) buffer.size() - 1;
        writePos = 0;
    }

    void reset() noexcept { std::fill (buffer.begin(), buffer.end(), 0.0f); }

    void push (float x) noexcept
    {
        buffer[(size_t) writePos] = x;
        writePos = (writePos + 1) & mask;
    }

    float read (float delaySamples) const noexcept
    {
        const float d = juce::jlimit (1.0f, (float) mask - 2.0f, delaySamples);
        const int i = (int) d;
        const float f = d - (float) i;
        const float a = buffer[(size_t) ((writePos - i) & mask)];
        const float b = buffer[(size_t) ((writePos - i - 1) & mask)];
        return a + f * (b - a);
    }

    int size() const noexcept { return (int) buffer.size(); }

private:
    std::vector<float> buffer;
    int mask = 0, writePos = 0;
};

// Allpass de primer orden (dispersión / rigidez)
struct Allpass1
{
    float a = 0.0f, x1 = 0.0f, y1 = 0.0f;
    float process (float x) noexcept
    {
        const float y = a * x + x1 - a * y1;
        x1 = x;
        y1 = y;
        return y;
    }
    void reset() noexcept { x1 = y1 = 0.0f; }
};

//==============================================================================
// Cuerda Karplus-Strong extendida: filtro de pérdidas, dispersión (rigidez) y
// ganancia de lazo derivada del T60 deseado.
class KsString
{
public:
    void prepare (double sr)
    {
        sampleRate = (float) sr;
        line.prepare ((int) (sr / 15.0) + 8);
        reset();
    }

    void reset() noexcept
    {
        line.reset();
        lp = 0.0f;
        for (auto& ap : disp) ap.reset();
    }

    void setParams (float freq, float t60, float brightness, float stiffness) noexcept
    {
        freq = juce::jlimit (20.0f, sampleRate * 0.4f, freq);
        const float w = juce::MathConstants<float>::twoPi * freq / sampleRate;
        const float periodTotal = sampleRate / freq;

        // Filtro de pérdidas: BRIGHTNESS define el brillo, pero el pasabajos no puede comerse la
        // fundamental (si no, las notas agudas se apagan en milisegundos). Se abre lo necesario para
        // que en T60 el filtro solo quite ~20 dB a la fundamental.
        lpCoef = 0.3f + 0.69f * juce::jlimit (0.0f, 1.0f, brightness);
        auto lpMag = [w] (float c) { const float q = 1.0f - c; return c / std::sqrt (1.0f - 2.0f * q * std::cos (w) + q * q); };
        const float minPerPeriod = std::pow (0.1f, 1.0f / juce::jmax (1.0f, std::max (0.01f, t60) * freq));
        if (lpMag (lpCoef) < minPerPeriod)
        {
            float lo = lpCoef, hi = 0.999f;
            for (int i = 0; i < 14; ++i) { const float mid = 0.5f * (lo + hi); (lpMag (mid) < minPerPeriod ? lo : hi) = mid; }
            lpCoef = hi;
        }

        // Retardo de fase EXACTO de los filtros a la frecuencia de la nota (la aproximación de
        // continua desafinaba los agudos). Si la rigidez no entra en el período, se reduce.
        auto lpDelay = [w] (float c) { const float q = 1.0f - c; return std::atan2 (q * std::sin (w), 1.0f - q * std::cos (w)) / w; };
        auto apDelay = [w] (float a)
        {
            // H = (a + z^-1) / (1 + a z^-1)
            const std::complex<float> z1 = std::polar (1.0f, -w);
            return -std::arg ((a + z1) / (1.0f + a * z1)) / w;
        };
        float a = -0.7f * juce::jlimit (0.0f, 1.0f, stiffness);
        float delay = lpDelay (lpCoef) + apDelay (a) * (float) disp.size();
        while (periodTotal - delay < 2.0f && a < -0.01f)
        {
            a *= 0.8f;
            delay = lpDelay (lpCoef) + apDelay (a) * (float) disp.size();
        }
        for (auto& ap : disp) ap.a = a;
        period = std::max (2.0f, periodTotal - delay);
        loopGain = std::pow (0.001f, 1.0f / (std::max (0.01f, t60) * freq));
    }

    float process (float excitation) noexcept
    {
        float y = line.read (period);
        lp += (y - lp) * lpCoef;
        float v = lp;
        for (auto& ap : disp) v = ap.process (v);
        line.push (fastTanh (excitation + v * loopGain));
        return y;
    }

private:
    DelayLine line;
    std::array<Allpass1, 4> disp;
    float sampleRate = 44100.0f, period = 100.0f, loopGain = 0.99f, lpCoef = 0.5f, lp = 0.0f;
};

} // namespace cz
