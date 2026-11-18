#pragma once

#include <juce_core/juce_core.h>
#include <cmath>

namespace mm
{

// ADSR de segmentos exponenciales tipo analógico. El ataque apunta a 1.3 para
// obtener la curva cóncava de un condensador cargándose; decay/release son
// exponenciales puros. Ataque mínimo ~0.3 ms para transitorios de bajo "punchy".
class Envelope
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void prepare (double sr) noexcept { sampleRate = (float) sr; reset(); }
    void reset() noexcept             { stage = Stage::Idle; value = 0.0f; }

    void setParameters (float attackMs, float decayMs, float sustainLevel, float releaseMs) noexcept
    {
        attackRate  = rateFor (attackMs, 1.4663f); // ln(1.3 / 0.3)
        decayRate   = rateFor (decayMs, 4.6f);     // ~ -40 dB
        releaseRate = rateFor (releaseMs, 4.6f);
        sustain     = juce::jlimit (0.0f, 1.0f, sustainLevel);
    }

    void noteOn() noexcept  { stage = Stage::Attack; } // reinicia desde el valor actual (sin clicks)
    void noteOff() noexcept { if (stage != Stage::Idle) stage = Stage::Release; }

    bool isActive() const noexcept { return stage != Stage::Idle; }
    float getValue() const noexcept { return value; }

    float next() noexcept
    {
        switch (stage)
        {
            case Stage::Attack:
                value += (1.3f - value) * attackRate;
                if (value >= 1.0f) { value = 1.0f; stage = Stage::Decay; }
                break;
            case Stage::Decay:
                value += (sustain - value) * decayRate;
                if (std::abs (value - sustain) < 1.0e-4f) { value = sustain; stage = Stage::Sustain; }
                break;
            case Stage::Sustain:
                value += (sustain - value) * 0.002f; // sigue cambios del sustain sin saltos
                break;
            case Stage::Release:
                value -= value * releaseRate;
                if (value < 1.0e-5f) { value = 0.0f; stage = Stage::Idle; }
                break;
            case Stage::Idle:
                break;
        }
        return value;
    }

private:
    float rateFor (float ms, float k) const noexcept
    {
        const float samples = std::max (0.3f, ms) * 0.001f * sampleRate;
        return 1.0f - std::exp (-k / samples);
    }

    float sampleRate = 44100.0f;
    Stage stage = Stage::Idle;
    float value = 0.0f, sustain = 1.0f;
    float attackRate = 1.0f, decayRate = 0.01f, releaseRate = 0.01f;
};

} // namespace mm
