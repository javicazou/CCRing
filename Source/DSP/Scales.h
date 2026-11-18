#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>

namespace mm
{

struct Scale
{
    const char* name;
    int size;
    std::array<int, 12> steps; // semitonos ascendentes desde la tónica (solo los primeros `size`)
};

inline const std::array<Scale, 10>& scales()
{
    static const std::array<Scale, 10> s {{
        { "Minor",          7, { 0, 2, 3, 5, 7, 8, 10 } },
        { "Major",          7, { 0, 2, 4, 5, 7, 9, 11 } },
        { "Dorian",         7, { 0, 2, 3, 5, 7, 9, 10 } },
        { "Phrygian",       7, { 0, 1, 3, 5, 7, 8, 10 } },
        { "Mixolydian",     7, { 0, 2, 4, 5, 7, 9, 10 } },
        { "Harmonic Minor", 7, { 0, 2, 3, 5, 7, 8, 11 } },
        { "Phrygian Dom",   7, { 0, 1, 4, 5, 7, 8, 10 } },
        { "Minor Penta",    5, { 0, 3, 5, 7, 10 } },
        { "Major Penta",    5, { 0, 2, 4, 7, 9 } },
        { "Blues",          6, { 0, 3, 5, 6, 7, 10 } },
    }};
    return s;
}

// Índice del parámetro "Scale" que usa la máscara de 12 tonos editable
inline constexpr int kCustomScaleIndex = 10;

inline uint16_t scaleMask (const Scale& sc) noexcept
{
    uint16_t m = 0;
    for (int i = 0; i < sc.size; ++i)
        m = (uint16_t) (m | (1u << sc.steps[(size_t) i]));
    return m;
}

// La tónica (bit 0) siempre está presente
inline Scale scaleFromMask (uint16_t mask) noexcept
{
    mask = (uint16_t) (mask | 1u);
    Scale s { "Custom", 0, {} };
    for (int i = 0; i < 12; ++i)
        if (mask & (1u << i))
            s.steps[(size_t) s.size++] = i;
    return s;
}

// Grado de la escala (puede ser negativo o > size) → semitonos desde la tónica
inline int degreeToSemis (const Scale& sc, int degree) noexcept
{
    const int oct = (int) std::floor ((double) degree / sc.size);
    const int idx = degree - oct * sc.size;
    return oct * 12 + sc.steps[(size_t) idx];
}

inline int semisToNearestDegree (const Scale& sc, int semis) noexcept
{
    int best = 0, bestDist = 1000;
    for (int d = -4 * sc.size; d <= 5 * sc.size; ++d)
    {
        const int dist = std::abs (degreeToSemis (sc, d) - semis);
        if (dist < bestDist) { bestDist = dist; best = d; }
    }
    return best;
}

inline int quantizeToScale (const Scale& sc, int semis) noexcept
{
    return degreeToSemis (sc, semisToNearestDegree (sc, semis));
}

} // namespace mm
