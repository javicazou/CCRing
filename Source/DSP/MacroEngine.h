#pragma once

// MOTOR C — MACRO: los 24 modelos de síntesis de Mutable Instruments Plaits (código MIT de
// Emilie Gillet, ver ThirdParty/mutable/LICENSE-MIT.txt). Plaits trabaja a 48 kHz en bloques de
// 12 muestras; acá se convierte a la frecuencia del DAW y se maneja la compuerta por nota.
#include <array>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include "plaits/dsp/voice.h"

namespace cz
{

inline const char* macroModelName (int i)
{
    static const char* n[] { "VA + Filter", "Phase Dist", "6-Op FM I", "6-Op FM II", "6-Op FM III", "Wave Terrain",
                             "String Machine", "Chiptune", "Virtual Analog", "Waveshaper", "FM", "Grain / Formant",
                             "Harmonic", "Wavetable", "Chords", "Speech", "Swarm", "Noise", "Particles",
                             "Inharmonic String", "Modal", "Bass Drum", "Snare", "Hi-Hat" };
    return n[i < 0 ? 0 : (i > 23 ? 23 : i)];
}

struct MacroParams
{
    int model = 8;
    float harmonics = 0.5f, timbre = 0.5f, morph = 0.5f;
    float fm = 0.0f, timbreMod = 0.0f, morphMod = 0.0f;   // -1..1 (envolvente interna)
    float decay = 0.5f, colour = 0.5f;
    bool lpg = true;       // compuerta de paso bajo con golpe (apagada = sonido continuo)
    int outMode = 0;       // 0 = MAIN, 1 = AUX, 2 = estéreo (MAIN izquierda, AUX derecha)
};

class MacroEngine
{
public:
    MacroEngine()
    {
        stmlib::BufferAllocator allocator (buffer.data(), buffer.size());
        voice.Init (&allocator);
    }

    void prepare (double sampleRate)
    {
        step = 48000.0 / sampleRate;
        readPos = (double) plaits::kBlockSize; // renderiza un bloque nuevo en la primera muestra
        prevMain = prevAux = 0.0f;
        gate = false;
    }

    void noteOn (float midiNote)
    {
        note = midiNote;
        lowBlocks = gate ? 1 : 0; // nota nueva con la compuerta ya alta: un bloque abajo para que dispare
        gate = true;
    }
    void noteOff() noexcept { gate = false; }

    // Nota (MIDI, con afinación y bend ya sumados) y parámetros; se aplican en el próximo bloque
    void set (float midiNote, const MacroParams& params) noexcept { note = midiNote; p = params; }

    // Una muestra a la frecuencia del DAW (interpolación lineal desde 48 kHz)
    void next (float& left, float& right) noexcept
    {
        while (readPos >= (double) plaits::kBlockSize)
        {
            prevMain = main[plaits::kBlockSize - 1];
            prevAux = aux[plaits::kBlockSize - 1];
            renderBlock();
            readPos -= (double) plaits::kBlockSize;
        }
        const int i = (int) readPos;
        const float f = (float) (readPos - i);
        const float m0 = i == 0 ? prevMain : main[(size_t) i - 1], m1 = main[(size_t) i];
        const float a0 = i == 0 ? prevAux : aux[(size_t) i - 1], a1 = aux[(size_t) i];
        const float m = m0 + f * (m1 - m0), a = a0 + f * (a1 - a0);
        readPos += step;
        switch (p.outMode)
        {
            case 1:  left = right = a; break;
            case 2:  left = m; right = a; break;
            default: left = right = m; break;
        }
    }

private:
    void renderBlock() noexcept
    {
        // Plaits: todos los modelos comparten la misma memoria de trabajo. Al cambiar de modelo
        // algunos leen lo que dejó el anterior (hasta producir NaN), así que se limpia y se reinicia.
        if (p.model != activeModel)
        {
            std::fill (buffer.begin(), buffer.end(), (char) 0);
            stmlib::BufferAllocator allocator (buffer.data(), buffer.size());
            voice.Init (&allocator);
            activeModel = p.model;
            if (gate) lowBlocks = 1;   // la nota que sigue sonando vuelve a disparar con el modelo nuevo
        }
        plaits::Patch patch {};
        // Entradas saneadas: un NaN o un valor fuera de rango puede colgar algunos modelos de Plaits
        // (CHIP se quedaba en un bucle sin fin y el DAW decía que el audio no responde)
        auto unit = [] (float x) { return std::isfinite (x) ? juce::jlimit (0.0f, 1.0f, x) : 0.5f; };
        auto bip  = [] (float x) { return std::isfinite (x) ? juce::jlimit (-1.0f, 1.0f, x) : 0.0f; };
        if (! std::isfinite (note)) note = 60.0f;
        // Plaits calcula la afinación para 47872 Hz (su reloj real): se corrige para 48 kHz exactos
        patch.note = juce::jlimit (-36.0f, 150.0f, note) - 0.0461f;
        patch.harmonics = unit (p.harmonics);
        patch.timbre = unit (p.timbre);
        patch.morph = unit (p.morph);
        patch.frequency_modulation_amount = bip (p.fm);
        patch.timbre_modulation_amount = bip (p.timbreMod);
        patch.morph_modulation_amount = bip (p.morphMod);
        patch.engine = juce::jlimit (0, 23, p.model);
        patch.decay = unit (p.decay);
        patch.lpg_colour = unit (p.colour);

       #if ! defined (NDEBUG)
        for (float v : { patch.note, patch.harmonics, patch.timbre, patch.morph, patch.frequency_modulation_amount,
                         patch.timbre_modulation_amount, patch.morph_modulation_amount, patch.decay, patch.lpg_colour })
            if (! std::isfinite (v))
                std::fprintf (stderr, "MACRO NAN IN: note %f h %f t %f m %f fm %f tm %f mm %f d %f c %f model %d\n", patch.note, patch.harmonics,
                              patch.timbre, patch.morph, patch.frequency_modulation_amount, patch.timbre_modulation_amount,
                              patch.morph_modulation_amount, patch.decay, patch.lpg_colour, patch.engine);
       #endif
        plaits::Modulations mod {};
        mod.trigger = gate && lowBlocks == 0 ? 1.0f : 0.0f;
        mod.trigger_patched = p.lpg;
        if (lowBlocks > 0) --lowBlocks;

        voice.Render (patch, mod, frames.data(), plaits::kBlockSize);
        for (size_t k = 0; k < plaits::kBlockSize; ++k)
        {
            main[k] = (float) frames[k].out / 32768.0f;
            aux[k] = (float) frames[k].aux / 32768.0f;
        }
        if (! std::isfinite (readPos) || readPos > 4.0 * plaits::kBlockSize) readPos = 0.0;
    }

    plaits::Voice voice;
    std::array<char, 16384> buffer {};
    std::array<plaits::Voice::Frame, plaits::kBlockSize> frames {};
    std::array<float, plaits::kBlockSize> main {}, aux {};
    MacroParams p;
    double step = 1.0, readPos = 12.0;
    float note = 60.0f, prevMain = 0.0f, prevAux = 0.0f;
    bool gate = false;
    int lowBlocks = 0;
    int activeModel = -1;
};

} // namespace cz
