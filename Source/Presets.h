#pragma once

#include <utility>
#include <vector>

// Presets de fábrica originales. Cada preset fija solo los parámetros que difieren del
// valor por defecto; el resto vuelve a su default.
namespace presets
{

struct Preset
{
    const char* category;
    const char* name;
    std::vector<std::pair<const char*, float>> values;
};

inline const std::vector<Preset>& factory()
{
    // Choices — A model: 0 Modal 1 String 2 Sympathetic 3 Metallic 4 Piano 5 Tine Keys 6 Reed Keys
    //           A exciter: 0 Pluck 1 Strike 2 Bow 3 Blow 4 Hammer
    //           B model: 0 Membrane 1 Bar 2 Plate 3 Bell 4 Tube 5 Glass 6 Marimba 7 Vibes 8 Mbira 9 Steel Pan
//                    10 Hand Drum 11 Gong
    //           B exciter: 0 Mallet 1 Noise 2 Bounce 3 Crackle
    //           Filter: 0 LP 1 BP 2 HP 3 Notch | LFO shape: 0 Sine 1 Tri 2 Saw 3 Square 4 S&H 5 Drift
    //           LFO dest: 0 Off 1 Pitch 2 A Structure 3 A Bright 4 A Damping 5 A Position 6 B Material
    //                     7 B Decay 8 B Tone 9 B Spread 10 Cutoff 11 Reso 12 A/B Mix 13 Delay Time 14 Glitch Prob 15 Spring Mix
    //                     16 A>B Feed 17 Volume (tremolo)
    //           SEQ 2 follow: 0 Off 1 Unison 2 +3rd m 3 +3rd M 4 +4th 5 +5th 6 +Oct 7 -Oct
    //           Glitch len: 0 1/8 1 1/16 2 1/32 3 1/64 | Delay div: 0 1/16 1 1/8T 2 1/8 3 1/8D 4 1/4 5 1/4D 6 1/2
    static const std::vector<Preset> list {
        // ---------------------------------------------------------------- RESONATOR
        { "Resonator", "Init Ring", { { "volume", 21 } } },

        { "Resonator", "Glass Harmonics", {
            { "aModel", 0 }, { "aStructure", 0.1f }, { "aBright", 0.8f }, { "aDamping", 0.3f },
            { "aPosition", 0.15f }, { "abMix", 0.0f }, { "delayMix", 0.25f }, { "springMix", 0.25f }, { "volume", 21 } } },

        { "Resonator", "Bowed Metal", {
            { "aModel", 3 }, { "aExciter", 2 }, { "aStructure", 0.55f }, { "aBright", 0.6f },
            { "aDamping", 0.2f }, { "attack", 300 }, { "release", 1500 }, { "abMix", 0.0f },
            { "lfo1Dest", 3 }, { "lfo1Depth", 0.3f }, { "volume", 11 } } },

        { "Resonator", "Sympathetic Sitar", {
            { "aModel", 2 }, { "aExciter", 0 }, { "aStructure", 0.1f }, { "aBright", 0.7f },
            { "aDamping", 0.35f }, { "aPosition", 0.1f }, { "abMix", 0.0f }, { "springMix", 0.3f }, { "volume", 6 } } },

        { "Resonator", "Karplus Pluck", {
            { "aModel", 1 }, { "aBright", 0.6f }, { "aDamping", 0.35f }, { "aStructure", 0.05f },
            { "abMix", 0.0f }, { "delayMix", 0.3f }, { "volume", 13 } } },

        { "Resonator", "Breath Tube", {
            { "aModel", 0 }, { "aExciter", 3 }, { "aStructure", 0.0f }, { "aBright", 0.35f },
            { "aDamping", 0.25f }, { "attack", 150 }, { "abMix", 0.0f }, { "cutoff", 4000 }, { "volume", 13 } } },

        // ---------------------------------------------------------------- IMPACT
        { "Impact", "Wood Block", {
            { "bModel", 1 }, { "bMaterial", 0.1f }, { "bDecay", 0.2f }, { "bTone", 0.7f },
            { "abMix", 1.0f }, { "delayMix", 0.15f }, { "volume", 23 } } },

        { "Impact", "Tuned Membrane", {
            { "bModel", 0 }, { "bSweep", 0.35f }, { "bDecay", 0.3f }, { "bMaterial", 0.3f },
            { "bTone", 0.4f }, { "abMix", 1.0f }, { "bPitch", -12 }, { "volume", 14 } } },

        { "Impact", "Bell Garden", {
            { "bModel", 3 }, { "bMaterial", 0.9f }, { "bDecay", 0.7f }, { "bTone", 0.6f },
            { "abMix", 1.0f }, { "springMix", 0.3f }, { "delayMix", 0.25f }, { "volume", 10 } } },

        { "Impact", "Glass Bounce", {
            { "bModel", 5 }, { "bExciter", 2 }, { "bMaterial", 0.8f }, { "bDecay", 0.5f },
            { "bTone", 0.7f }, { "abMix", 1.0f }, { "volume", 12 } } },

        { "Impact", "Steel Plate", {
            { "bModel", 2 }, { "bMaterial", 0.85f }, { "bDecay", 0.6f }, { "bTone", 0.5f },
            { "bSpread", 0.15f }, { "abMix", 1.0f }, { "volume", 13 } } },

        // ---------------------------------------------------------------- HYBRID
        { "Hybrid", "Ring Into Drum", {
            { "aModel", 1 }, { "bModel", 0 }, { "abMix", 0.5f }, { "xfeed", 0.6f }, { "bSweep", 0.2f }, { "volume", 16 } } },

        { "Hybrid", "Mbira Ghost", {
            { "aModel", 0 }, { "aStructure", 0.25f }, { "aBright", 0.6f }, { "aDamping", 0.5f },
            { "bModel", 1 }, { "bMaterial", 0.3f }, { "abMix", 0.5f }, { "delayMix", 0.3f }, { "volume", 18 } } },

        // ---------------------------------------------------------------- GLITCH
        { "Glitch", "Crackle Box", {
            { "bExciter", 3 }, { "bModel", 2 }, { "bMaterial", 0.7f }, { "bDecay", 0.4f },
            { "bSpread", 0.4f }, { "abMix", 0.85f }, { "glitchOn", 1 }, { "glitchProb", 0.35f },
            { "glitchCrush", 0.3f }, { "lfo2Dest", 9 }, { "lfo2Depth", 0.5f }, { "volume", 17 } } },

        { "Glitch", "Broken Machine", {
            { "aModel", 3 }, { "aStructure", 0.8f }, { "bExciter", 2 }, { "bModel", 4 },
            { "xfeed", 0.5f }, { "glitchOn", 1 }, { "glitchProb", 0.5f }, { "glitchVar", 0.8f },
            { "glitchLen", 2 }, { "lfo1Shape", 4 }, { "lfo1Dest", 1 }, { "lfo1Depth", 0.1f },
            { "lfo1Rate", 8 }, { "delayMix", 0.25f }, { "volume", 18 } } },

        { "Glitch", "Bit Rain", {
            { "bModel", 5 }, { "bExciter", 3 }, { "glitchOn", 1 }, { "glitchCrush", 0.6f },
            { "glitchProb", 0.3f }, { "abMix", 1.0f }, { "springMix", 0.3f }, { "volume", 20 } } },

        // ---------------------------------------------------------------- SEQUENCE (secuenciador euclidiano)
        { "Sequence", "Euclid Plonks", {
            { "bModel", 1 }, { "bMaterial", 0.2f }, { "bDecay", 0.25f }, { "bTone", 0.65f },
            { "abMix", 1.0f }, { "seqOn", 1 }, { "seqPulses", 5 }, { "seqRange", 0.5f },
            { "variation", 0.5f }, { "width", 0.6f }, { "delayMix", 0.25f }, { "volume", 18 } } },

        { "Sequence", "Glass Machine", {
            { "bModel", 5 }, { "bMaterial", 0.8f }, { "bDecay", 0.45f }, { "abMix", 0.8f },
            { "seqOn", 1 }, { "seqPulses", 7 }, { "seqRotate", 2 }, { "seqScale", 2 },
            { "variation", 0.6f }, { "width", 0.8f }, { "springMix", 0.3f }, { "volume", 15 } } },

        { "Sequence", "Membrane Groove", {
            { "bModel", 0 }, { "bSweep", 0.4f }, { "bDecay", 0.3f }, { "bMaterial", 0.3f },
            { "bPitch", -12 }, { "abMix", 1.0f }, { "seqOn", 1 }, { "seqPulses", 6 },
            { "seqRange", 0.2f }, { "seqSwing", 0.57f }, { "variation", 0.4f }, { "width", 0.4f }, { "volume", 13 } } },

        { "Sequence", "Resonant Arp", {
            { "aModel", 2 }, { "aBright", 0.7f }, { "aDamping", 0.45f }, { "abMix", 0.0f },
            { "seqOn", 1 }, { "seqPulses", 9 }, { "seqRange", 0.8f }, { "seqGate", 0.3f },
            { "variation", 0.5f }, { "width", 0.7f }, { "delayMix", 0.3f }, { "volume", 10 } } },

        { "Sequence", "Glitch Euclid", {
            { "bModel", 2 }, { "bExciter", 3 }, { "bSpread", 0.3f }, { "abMix", 0.85f },
            { "seqOn", 1 }, { "seqPulses", 11 }, { "seqDiv", 0 }, { "glitchOn", 1 },
            { "glitchProb", 0.3f }, { "variation", 0.8f }, { "width", 1.0f }, { "volume", 17 } } },

        { "Sequence", "Polymeter Steps", {
            { "aModel", 0 }, { "aStructure", 0.2f }, { "aBright", 0.65f }, { "aDamping", 0.5f },
            { "bModel", 1 }, { "bMaterial", 0.3f }, { "abMix", 0.4f }, { "seqOn", 1 },
            { "seqPulses", 11 }, { "seqLenNote", 7 }, { "seqLenOct", 11 }, { "seqLenVel", 5 },
            { "seqLenGate", 3 }, { "seqLenProb", 13 }, { "seqRange", 0.6f }, { "variation", 0.4f },
            { "width", 0.7f }, { "delayMix", 0.3f }, { "volume", 13 } } },

        { "Sequence", "Random Walker", {
            { "aModel", 2 }, { "aBright", 0.6f }, { "aDamping", 0.4f }, { "abMix", 0.15f },
            { "seqOn", 1 }, { "seqPulses", 9 }, { "seqDir", 3 }, { "seqRange", 0.5f },
            { "seqRandom", 0.25f }, { "width", 0.8f }, { "springMix", 0.3f }, { "volume", 8 } } },

        // ---------------------------------------------------------------- DRONE
        { "Drone", "Sympathetic Drone", {
            { "aModel", 2 }, { "aExciter", 2 }, { "aStructure", 0.45f }, { "aDamping", 0.1f },
            { "attack", 800 }, { "release", 3000 }, { "abMix", 0.2f }, { "lfo1Shape", 5 },
            { "lfo1Dest", 2 }, { "lfo1Depth", 0.3f }, { "lfo1Rate", 0.1f }, { "springMix", 0.35f },
            { "delayMix", 0.3f }, { "volume", 3 } } },

        { "Drone", "Tape Cathedral", {
            { "aModel", 0 }, { "aExciter", 3 }, { "aStructure", 0.6f }, { "aDamping", 0.15f },
            { "aBright", 0.4f }, { "attack", 1200 }, { "release", 4000 }, { "abMix", 0.0f },
            { "delayMix", 0.4f }, { "delayFb", 0.6f }, { "delayWow", 0.5f }, { "springMix", 0.4f }, { "volume", 9 } } },

        // ---------------------------------------------------------------- ACOUSTIC
        // Instrumentos acústicos y electromecánicos modelados (presets originales; referencia: el
        // realismo de los sintes de modelado físico de piano, mallets y cuerdas)
        { "Acoustic", "Felt Upright", {
            { "aModel", 4 }, { "aExciter", 4 }, { "aStructure", 0.4f }, { "aBright", 0.3f }, { "aDamping", 0.4f },
            { "aPosition", 0.12f }, { "abMix", 0.0f }, { "body", 0.55f }, { "cutoff", 6500 }, { "keyTrack", 0.2f },
            { "attack", 0.5f }, { "decay", 10000 }, { "release", 350 }, { "velSens", 0.85f }, { "variation", 0.1f },
            { "width", 0.45f }, { "delayMix", 0.08f }, { "springMix", 0.18f }, { "volume", 25 } } },

        { "Acoustic", "Grand Room", {
            { "aModel", 4 }, { "aExciter", 4 }, { "aStructure", 0.25f }, { "aBright", 0.62f }, { "aDamping", 0.28f },
            { "aPosition", 0.1f }, { "abMix", 0.0f }, { "body", 0.65f }, { "keyTrack", 0.3f }, { "attack", 0.3f },
            { "decay", 10000 }, { "release", 500 }, { "velSens", 0.9f }, { "variation", 0.05f }, { "width", 0.75f },
            { "delayMix", 0.0f }, { "springMix", 0.22f }, { "springDecay", 2.4f }, { "volume", 22 } } },

        { "Acoustic", "Broken Grand", {
            { "aModel", 4 }, { "aExciter", 4 }, { "aStructure", 1.0f }, { "aBright", 0.45f }, { "aDamping", 0.35f },
            { "aPosition", 0.2f }, { "bModel", 2 }, { "bMaterial", 0.8f }, { "bSpread", 0.5f }, { "bDecay", 0.45f },
            { "abMix", 0.3f }, { "body", 0.5f }, { "release", 400 }, { "velSens", 0.8f }, { "variation", 0.5f },
            { "width", 0.7f }, { "delayMix", 0.2f }, { "delayWow", 0.5f }, { "springMix", 0.25f }, { "volume", 15 } } },

        { "Acoustic", "Tine Keys", {
            { "aModel", 5 }, { "aExciter", 4 }, { "aStructure", 0.3f }, { "aBright", 0.4f }, { "aDamping", 0.38f },
            { "abMix", 0.0f }, { "cutoff", 9000 }, { "decay", 10000 }, { "release", 250 }, { "velSens", 0.85f },
            { "width", 0.6f }, { "variation", 0.1f }, { "delayMix", 0.12f }, { "springMix", 0.15f },
            { "lfo1Shape", 0 }, { "lfo1Dest", 5 }, { "lfo1Depth", 0.05f }, { "volume", 7 } } },

        { "Acoustic", "Tine Keys Bark", {
            { "aModel", 5 }, { "aExciter", 4 }, { "aStructure", 0.85f }, { "aBright", 0.75f }, { "aDamping", 0.42f },
            { "abMix", 0.0f }, { "drive", 0.2f }, { "decay", 10000 }, { "release", 220 }, { "velSens", 0.95f },
            { "width", 0.5f }, { "delayMix", 0.18f }, { "delaySync", 1 }, { "delayDiv", 3 }, { "springMix", 0.1f }, { "volume", 2 } } },

        { "Acoustic", "Reed Keys", {
            { "aModel", 6 }, { "aExciter", 4 }, { "aStructure", 0.45f }, { "aBright", 0.5f }, { "aDamping", 0.5f },
            { "abMix", 0.0f }, { "drive", 0.12f }, { "cutoff", 7500 }, { "decay", 10000 }, { "release", 180 },
            { "velSens", 0.9f }, { "width", 0.35f }, { "delayMix", 0.1f }, { "springMix", 0.2f }, { "volume", 2 } } },

        { "Acoustic", "Marimba Rosewood", {
            { "bModel", 6 }, { "bExciter", 0 }, { "bMaterial", 0.22f }, { "bDecay", 0.42f }, { "bTone", 0.42f },
            { "bPosition", 0.25f }, { "bSweep", 0.0f }, { "abMix", 1.0f }, { "body", 0.25f }, { "velSens", 0.8f },
            { "width", 0.6f }, { "variation", 0.15f }, { "delayMix", 0.1f }, { "springMix", 0.15f }, { "volume", 11 } } },

        { "Acoustic", "Vibes", {
            { "bModel", 7 }, { "bExciter", 0 }, { "bMaterial", 0.95f }, { "bDecay", 0.72f }, { "bTone", 0.35f },
            { "bPosition", 0.3f }, { "bSweep", 0.0f }, { "abMix", 1.0f }, { "release", 1500 }, { "velSens", 0.75f },
            { "width", 0.7f }, { "lfo1Shape", 0 }, { "lfo1Sync", 1 }, { "lfo1Div", 4 }, { "lfo1Dest", 8 }, { "lfo1Depth", 0.15f },
            { "delayMix", 0.18f }, { "springMix", 0.22f }, { "volume", 5 } } },

        { "Acoustic", "Thumb Piano Box", {
            { "bModel", 8 }, { "bExciter", 0 }, { "bMaterial", 0.7f }, { "bDecay", 0.5f }, { "bTone", 0.6f },
            { "bPosition", 0.15f }, { "bSweep", 0.0f }, { "abMix", 1.0f }, { "body", 0.7f }, { "velSens", 0.7f },
            { "width", 0.5f }, { "variation", 0.25f }, { "delayMix", 0.15f }, { "springMix", 0.12f }, { "volume", 9 } } },

        { "Acoustic", "Steel Pan", {
            { "bModel", 9 }, { "bExciter", 0 }, { "bMaterial", 0.85f }, { "bDecay", 0.55f }, { "bTone", 0.55f },
            { "bPosition", 0.35f }, { "bSweep", 0.0f }, { "bSpread", 0.08f }, { "abMix", 1.0f }, { "velSens", 0.7f },
            { "width", 0.55f }, { "delayMix", 0.12f }, { "springMix", 0.2f }, { "volume", 13 } } },

        { "Acoustic", "Nylon Harp", {
            { "aModel", 1 }, { "aExciter", 0 }, { "aStructure", 0.05f }, { "aBright", 0.42f }, { "aDamping", 0.3f },
            { "aPosition", 0.18f }, { "abMix", 0.0f }, { "body", 0.6f }, { "release", 900 }, { "velSens", 0.7f },
            { "width", 0.65f }, { "variation", 0.2f }, { "delayMix", 0.1f }, { "springMix", 0.25f }, { "volume", 17 } } },

        { "Acoustic", "Cello Body", {
            { "aModel", 1 }, { "aExciter", 2 }, { "aStructure", 0.1f }, { "aBright", 0.5f }, { "aDamping", 0.12f },
            { "aPosition", 0.12f }, { "aPitch", -12 }, { "abMix", 0.0f }, { "body", 0.85f }, { "attack", 140 },
            { "release", 700 }, { "width", 0.4f }, { "lfo1Shape", 0 }, { "lfo1Rate", 5.0f }, { "lfo1Dest", 1 },
            { "lfo1Depth", 0.01f }, { "springMix", 0.25f }, { "volume", 12 } } },

        { "Acoustic", "Hand Drum", {
            { "bModel", 10 }, { "bExciter", 0 }, { "bMaterial", 0.2f }, { "bDecay", 0.28f }, { "bTone", 0.35f },
            { "bPosition", 0.4f }, { "bSweep", 0.18f }, { "bPitch", -12 }, { "abMix", 1.0f }, { "body", 0.45f },
            { "velSens", 0.9f }, { "variation", 0.35f }, { "width", 0.4f }, { "delayMix", 0.05f }, { "springMix", 0.1f }, { "volume", 12 } } },

        { "Acoustic", "Bronze Gongs", {
            { "bModel", 11 }, { "bExciter", 0 }, { "bMaterial", 1.0f }, { "bDecay", 0.82f }, { "bTone", 0.38f },
            { "bSpread", 0.3f }, { "bSweep", 0.0f }, { "bPitch", -12 }, { "abMix", 1.0f }, { "release", 3000 },
            { "velSens", 0.7f }, { "width", 0.8f }, { "delayMix", 0.15f }, { "springMix", 0.3f }, { "volume", 3 } } },

        // ---------------------------------------------------------------- LIVE (pedal, vida, chorus, trémolo, SEQ 2)
        { "Live", "Pedal Grand", {
            { "aModel", 4 }, { "aExciter", 4 }, { "aStructure", 0.3f }, { "aBright", 0.55f }, { "aDamping", 0.3f },
            { "aPosition", 0.1f }, { "abMix", 0.0f }, { "body", 0.6f }, { "pedal", 1 }, { "sympathy", 0.6f }, { "life", 0.25f },
            { "release", 500 }, { "velSens", 0.9f }, { "width", 0.75f }, { "delayMix", 0.0f }, { "springMix", 0.2f }, { "volume", 23 } } },

        { "Live", "Tine Chorus", {
            { "aModel", 5 }, { "aExciter", 4 }, { "aStructure", 0.45f }, { "aBright", 0.5f }, { "aDamping", 0.4f },
            { "abMix", 0.0f }, { "chorusMix", 0.55f }, { "chorusRate", 0.7f }, { "chorusDepth", 0.6f }, { "life", 0.2f },
            { "decay", 10000 }, { "release", 260 }, { "velSens", 0.9f }, { "width", 0.6f }, { "delayMix", 0.12f },
            { "springMix", 0.12f }, { "volume", 8 } } },

        { "Live", "Vibes Motor", {
            { "bModel", 7 }, { "bExciter", 0 }, { "bMaterial", 0.95f }, { "bDecay", 0.7f }, { "bTone", 0.38f },
            { "bSweep", 0.0f }, { "abMix", 1.0f }, { "release", 1500 }, { "velSens", 0.75f }, { "width", 0.7f },
            { "lfo1Shape", 0 }, { "lfo1Sync", 1 }, { "lfo1Div", 5 }, { "lfo1Dest", 17 }, { "lfo1Depth", 0.35f },
            { "delayMix", 0.15f }, { "springMix", 0.22f }, { "volume", 4 } } },

        { "Live", "Bass Marimba", {
            { "bModel", 6 }, { "bExciter", 0 }, { "bMaterial", 0.18f }, { "bDecay", 0.5f }, { "bTone", 0.3f },
            { "bSweep", 0.0f }, { "bPitch", -24 }, { "abMix", 1.0f }, { "body", 0.4f }, { "velSens", 0.8f },
            { "width", 0.3f }, { "seqOctave", 2 }, { "delayMix", 0.05f }, { "springMix", 0.08f }, { "volume", -6 } } },

        { "Live", "Breathing Strings", {
            { "aModel", 1 }, { "aExciter", 0 }, { "aStructure", 0.1f }, { "aBright", 0.45f }, { "aDamping", 0.15f },
            { "aPosition", 0.15f }, { "abMix", 0.0f }, { "body", 0.7f }, { "life", 0.85f }, { "attack", 3 },
            { "release", 900 }, { "width", 0.6f }, { "springMix", 0.3f }, { "chorusMix", 0.2f }, { "volume", 17 } } },

        { "Live", "Fifths Machine", {
            { "aModel", 5 }, { "aExciter", 4 }, { "aStructure", 0.4f }, { "aBright", 0.5f }, { "aDamping", 0.45f },
            { "bModel", 6 }, { "bMaterial", 0.25f }, { "bDecay", 0.4f }, { "bTone", 0.45f }, { "bSweep", 0.0f },
            { "abMix", 0.5f }, { "seq2On", 1 }, { "seq2Follow", 5 }, { "seq2Div", 3 }, { "life", 0.2f },
            { "chorusMix", 0.25f }, { "width", 0.6f }, { "delayMix", 0.15f }, { "volume", 10 } } },

        { "Live", "Mutating Plonks", {
            { "bModel", 8 }, { "bExciter", 0 }, { "bMaterial", 0.7f }, { "bDecay", 0.45f }, { "bTone", 0.6f },
            { "bSweep", 0.0f }, { "abMix", 1.0f }, { "body", 0.6f }, { "seqMutate", 0.6f }, { "seqPulses", 7 },
            { "lockDest", 5 }, { "life", 0.3f }, { "width", 0.5f }, { "delayMix", 0.2f }, { "springMix", 0.15f }, { "volume", -6 } } },

        // ---------------------------------------------------------------- MACRO (motor C, modelos de Plaits)
        // C model: 0 VA+VCF 1 Phase Dist 2-4 6-Op FM 5 Terrain 6 String Machine 7 Chiptune 8 VA 9 Waveshaper
        //          10 FM 11 Grain 12 Harmonic 13 Wavetable 14 Chords 15 Speech 16 Swarm 17 Noise 18 Particles
        //          19 Inharmonic String 20 Modal 21 Kick 22 Snare 23 Hi-Hat | LFO dest 18 C Timbre 19 C Morph 20 C Harmonics
        { "Macro", "Dub Chord Stab", {
            { "cModel", 14 }, { "cMix", 1.0f }, { "cHarm", 0.55f }, { "cTimbre", 0.35f }, { "cMorph", 0.3f },
            { "cDecay", 0.45f }, { "cColour", 0.35f }, { "cOut", 2 }, { "delayMix", 0.35f }, { "delayDiv", 3 },
            { "springMix", 0.25f }, { "width", 0.6f }, { "lfo1Dest", 18 }, { "lfo1Depth", 0.2f }, { "volume", 10 } } },

        { "Macro", "FM Plonk", {
            { "cModel", 10 }, { "cMix", 1.0f }, { "cHarm", 0.32f }, { "cTimbre", 0.55f }, { "cMorph", 0.2f },
            { "cTimbreMod", 0.5f }, { "cDecay", 0.3f }, { "cColour", 0.6f }, { "delayMix", 0.2f },
            { "springMix", 0.15f }, { "volume", 9 } } },

        { "Macro", "Granular Dust", {
            { "cModel", 11 }, { "cMix", 1.0f }, { "cHarm", 0.6f }, { "cTimbre", 0.4f }, { "cMorph", 0.65f },
            { "cLpg", 0 }, { "attack", 250 }, { "release", 1200 }, { "lfo1Shape", 5 }, { "lfo1Dest", 19 },
            { "lfo1Depth", 0.3f }, { "chorusMix", 0.3f }, { "springMix", 0.3f }, { "width", 0.7f }, { "volume", 20 } } },

        { "Macro", "Talking Loop", {
            { "cModel", 15 }, { "cMix", 1.0f }, { "cHarm", 0.3f }, { "cTimbre", 0.5f }, { "cMorph", 0.3f },
            { "cDecay", 0.5f }, { "lfo1Shape", 4 }, { "lfo1Sync", 1 }, { "lfo1Div", 5 }, { "lfo1Dest", 19 },
            { "lfo1Depth", 0.5f }, { "delayMix", 0.25f }, { "volume", 15 } } },

        { "Macro", "Kick + Resonator", {
            { "cModel", 21 }, { "cHarm", 0.35f }, { "cTimbre", 0.4f }, { "cMorph", 0.45f }, { "cDecay", 0.5f },
            { "seq2On", 1 }, { "seq2Target", 1 }, { "seq2Div", 3 }, { "aModel", 1 }, { "aBright", 0.55f },
            { "aDamping", 0.4f }, { "abMix", 0.0f }, { "cPitch", -15 }, { "delayMix", 0.15f }, { "volume", 5 } } },

        { "Macro", "Modal Into Strings", {
            { "cModel", 20 }, { "cMix", 0.35f }, { "cFeed", 0.7f }, { "cHarm", 0.4f }, { "cTimbre", 0.55f },
            { "cMorph", 0.45f }, { "aModel", 2 }, { "aExciter", 1 }, { "aBright", 0.6f }, { "aDamping", 0.3f },
            { "abMix", 0.0f }, { "body", 0.5f }, { "springMix", 0.25f }, { "volume", 9 } } },

        { "Macro", "Wavetable Drift", {
            { "cModel", 13 }, { "cMix", 1.0f }, { "cHarm", 0.2f }, { "cTimbre", 0.5f }, { "cMorph", 0.4f },
            { "cLpg", 0 }, { "attack", 40 }, { "release", 600 }, { "cutoff", 3500 }, { "lfo1Shape", 5 },
            { "lfo1Dest", 18 }, { "lfo1Depth", 0.35f }, { "chorusMix", 0.25f }, { "delayMix", 0.2f }, { "volume", 16 } } },

        { "Macro", "Inharmonic Pluck", {
            { "cModel", 19 }, { "cMix", 1.0f }, { "cHarm", 0.25f }, { "cTimbre", 0.6f }, { "cMorph", 0.4f },
            { "cDecay", 0.6f }, { "lockDest", 9 }, { "delayMix", 0.2f }, { "springMix", 0.2f }, { "width", 0.5f }, { "volume", 13 } } },

        { "Macro", "Minimal Hats + Wood", {
            { "cModel", 23 }, { "cHarm", 0.5f }, { "cTimbre", 0.6f }, { "cMorph", 0.25f }, { "seq2On", 1 },
            { "seq2Target", 1 }, { "seq2Div", 1 }, { "bModel", 1 }, { "bMaterial", 0.1f }, { "bDecay", 0.25f },
            { "abMix", 1.0f }, { "delayMix", 0.12f }, { "volume", 15 } } },

        // ---------------------------------------------------------------- MACRO 2: minimal / rominimal
        { "Macro", "Rubber Blip", {
            { "cModel", 8 }, { "cMix", 1.0f }, { "cHarm", 0.15f }, { "cTimbre", 0.3f }, { "cMorph", 0.6f }, { "cDecay", 0.18f },
            { "cColour", 0.7f }, { "cTimbreMod", 0.4f }, { "seqDiv", 1 }, { "seqLength", 16 }, { "seqPulses", 5 }, { "seqSwing", 0.6f },
            { "seqRange", 0.3f }, { "delayMix", 0.25f }, { "delayDiv", 3 }, { "delayFb", 0.45f }, { "volume", 12 } } },

        { "Macro", "Click Bass", {
            { "cModel", 10 }, { "cMix", 1.0f }, { "cHarm", 0.22f }, { "cTimbre", 0.25f }, { "cMorph", 0.15f }, { "cDecay", 0.3f },
            { "cColour", 0.35f }, { "cTimbreMod", 0.6f }, { "cPitch", -12 }, { "seqDiv", 1 }, { "seqPulses", 6 }, { "seqLength", 16 },
            { "seqRotate", 2 }, { "seqSwing", 0.58f }, { "seqRange", 0.15f }, { "width", 0.0f }, { "volume", 7 } } },

        { "Macro", "Dub Stab Fifths", {
            { "cModel", 14 }, { "cMix", 1.0f }, { "cHarm", 0.75f }, { "cTimbre", 0.3f }, { "cMorph", 0.25f }, { "cDecay", 0.35f },
            { "cColour", 0.3f }, { "seqDiv", 3 }, { "seqPulses", 3 }, { "seqLength", 16 }, { "seqRotate", 3 }, { "seqRange", 0.0f },
            { "delayMix", 0.4f }, { "delayDiv", 3 }, { "delayFb", 0.6f }, { "springMix", 0.3f }, { "cutoff", 2500 }, { "volume", 13 } } },

        { "Macro", "Wood Tick", {
            { "cModel", 20 }, { "cMix", 1.0f }, { "cHarm", 0.65f }, { "cTimbre", 0.35f }, { "cMorph", 0.2f }, { "cDecay", 0.2f },
            { "seqDiv", 0 }, { "seqPulses", 11 }, { "seqLength", 32 }, { "seqProb", 0.7f }, { "seqRange", 0.6f }, { "seqSwing", 0.56f },
            { "width", 0.7f }, { "delayMix", 0.15f }, { "volume", 12 } } },

        { "Macro", "Micro Shaker", {
            { "cModel", 23 }, { "cMix", 1.0f }, { "cHarm", 0.2f }, { "cTimbre", 0.75f }, { "cMorph", 0.15f }, { "cDecay", 0.12f },
            { "seqDiv", 1 }, { "seqPulses", 13 }, { "seqLength", 16 }, { "seqProb", 0.8f }, { "seqSwing", 0.62f }, { "velSens", 1.0f },
            { "width", 0.8f }, { "seqRandom", 0.5f }, { "volume", 19 } } },

        { "Macro", "Rim Ghost", {
            { "cModel", 22 }, { "cMix", 1.0f }, { "cHarm", 0.7f }, { "cTimbre", 0.8f }, { "cMorph", 0.25f }, { "cDecay", 0.2f },
            { "seqDiv", 1 }, { "seqPulses", 3 }, { "seqLength", 16 }, { "seqRotate", 4 }, { "seqSwing", 0.6f }, { "springMix", 0.3f },
            { "springDecay", 0.4f }, { "delayMix", 0.2f }, { "volume", 10 } } },

        { "Macro", "Deep Kick 7/16", {
            { "cModel", 21 }, { "cMix", 1.0f }, { "cHarm", 0.3f }, { "cTimbre", 0.35f }, { "cMorph", 0.55f }, { "cDecay", 0.45f },
            { "cPitch", -15 }, { "seqDiv", 1 }, { "seqPulses", 4 }, { "seqLength", 16 }, { "seqRange", 0.0f }, { "width", 0.0f },
            { "volume", 8 } } },

        { "Macro", "Speech Glitch", {
            { "cModel", 15 }, { "cMix", 1.0f }, { "cHarm", 0.5f }, { "cTimbre", 0.6f }, { "cMorph", 0.5f }, { "cDecay", 0.25f },
            { "cMorphMod", 0.3f }, { "seqDiv", 1 }, { "seqPulses", 5 }, { "seqLength", 12 }, { "seqRange", 0.5f },
            { "glitchOn", 1 }, { "glitchProb", 0.35f }, { "glitchMix", 0.6f }, { "lfo1Shape", 4 }, { "lfo1Sync", 1 }, { "lfo1Div", 6 },
            { "lfo1Dest", 19 }, { "lfo1Depth", 0.6f }, { "delayMix", 0.2f }, { "volume", 6 } } },

        { "Macro", "Formant Drone", {
            { "cModel", 11 }, { "cMix", 1.0f }, { "cHarm", 0.35f }, { "cTimbre", 0.5f }, { "cMorph", 0.7f }, { "cLpg", 0 },
            { "attack", 1200 }, { "release", 3000 }, { "seqDiv", 5 }, { "seqPulses", 2 }, { "seqLength", 8 }, { "seqRange", 0.2f },
            { "lfo1Shape", 5 }, { "lfo1Dest", 18 }, { "lfo1Depth", 0.35f }, { "lfo2Shape", 0 }, { "lfo2Dest", 19 }, { "lfo2Depth", 0.25f },
            { "chorusMix", 0.35f }, { "springMix", 0.35f }, { "width", 0.9f }, { "volume", 24 } } },

        { "Macro", "Swarm Pad", {
            { "cModel", 16 }, { "cMix", 1.0f }, { "cHarm", 0.3f }, { "cTimbre", 0.4f }, { "cMorph", 0.5f }, { "cLpg", 0 },
            { "attack", 800 }, { "release", 2500 }, { "cutoff", 3000 }, { "seqDiv", 5 }, { "seqPulses", 2 }, { "seqLength", 8 },
            { "seqRange", 0.25f }, { "lfo1Shape", 5 }, { "lfo1Dest", 18 }, { "lfo1Depth", 0.3f }, { "chorusMix", 0.3f },
            { "springMix", 0.3f }, { "width", 0.9f }, { "volume", 16 } } },

        { "Macro", "Particle Rain", {
            { "cModel", 18 }, { "cMix", 1.0f }, { "cHarm", 0.6f }, { "cTimbre", 0.3f }, { "cMorph", 0.6f }, { "cLpg", 0 },
            { "attack", 400 }, { "release", 2000 }, { "seqDiv", 4 }, { "seqPulses", 3 }, { "seqLength", 8 }, { "seqRange", 0.6f },
            { "springMix", 0.45f }, { "delayMix", 0.3f }, { "width", 1.0f }, { "volume", 26 } } },

        { "Macro", "Noise Wash", {
            { "cModel", 17 }, { "cMix", 1.0f }, { "cHarm", 0.6f }, { "cTimbre", 0.45f }, { "cMorph", 0.7f }, { "cLpg", 0 },
            { "attack", 1500 }, { "release", 3000 }, { "seqDiv", 5 }, { "seqPulses", 1 }, { "seqLength", 16 }, { "lfo1Shape", 0 },
            { "lfo1Sync", 1 }, { "lfo1Div", 1 }, { "lfo1Dest", 18 }, { "lfo1Depth", 0.4f }, { "springMix", 0.4f }, { "width", 1.0f },
            { "volume", 17 } } },

        { "Macro", "Bell Pluck Chain", {
            { "cModel", 3 }, { "cMix", 1.0f }, { "cHarm", 0.4f }, { "cTimbre", 0.45f }, { "cMorph", 0.35f }, { "cDecay", 0.4f },
            { "seqDiv", 1 }, { "seqPulses", 5 }, { "seqLength", 16 }, { "seqRange", 0.5f }, { "seqMutate", 0.3f }, { "delayMix", 0.3f },
            { "delayDiv", 3 }, { "springMix", 0.2f }, { "volume", 10 } } },

        { "Macro", "Chip Bleeps", {
            { "cModel", 7 }, { "cMix", 1.0f }, { "cHarm", 0.15f }, { "cTimbre", 0.5f }, { "cMorph", 0.3f }, { "cDecay", 0.15f },
            { "seqDiv", 0 }, { "seqPulses", 7 }, { "seqLength", 32 }, { "seqRange", 0.7f }, { "seqProb", 0.75f }, { "delayMix", 0.25f },
            { "delayDiv", 1 }, { "cutoff", 5000 }, { "volume", -3 } } },

        { "Macro", "Phase Organ", {
            { "cModel", 1 }, { "cMix", 1.0f }, { "cHarm", 0.5f }, { "cTimbre", 0.35f }, { "cMorph", 0.4f }, { "cLpg", 0 },
            { "attack", 20 }, { "release", 500 }, { "seqDiv", 3 }, { "seqPulses", 3 }, { "seqLength", 8 }, { "seqRange", 0.2f },
            { "chorusMix", 0.45f }, { "lfo1Dest", 18 }, { "lfo1Depth", 0.15f }, { "springMix", 0.2f }, { "volume", 4 } } },

        { "Macro", "Waveshaper Acid", {
            { "cModel", 9 }, { "cMix", 1.0f }, { "cHarm", 0.3f }, { "cTimbre", 0.4f }, { "cMorph", 0.5f }, { "cDecay", 0.3f },
            { "cTimbreMod", 0.5f }, { "cPitch", -12 }, { "seqDiv", 1 }, { "seqPulses", 9 }, { "seqLength", 16 }, { "seqRange", 0.35f },
            { "seqLongShort", 0.4f }, { "lockDest", 9 }, { "delayMix", 0.2f }, { "volume", 4 } } },

        { "Macro", "String Machine Haze", {
            { "cModel", 6 }, { "cMix", 1.0f }, { "cHarm", 0.45f }, { "cTimbre", 0.4f }, { "cMorph", 0.6f }, { "cLpg", 0 },
            { "attack", 600 }, { "release", 2000 }, { "seqDiv", 5 }, { "seqPulses", 2 }, { "seqLength", 8 }, { "seqRange", 0.2f },
            { "chorusMix", 0.4f }, { "springMix", 0.35f }, { "cutoff", 4000 }, { "width", 0.9f }, { "volume", 26 } } },

        { "Macro", "Harmonic Steps", {
            { "cModel", 12 }, { "cMix", 1.0f }, { "cHarm", 0.5f }, { "cTimbre", 0.3f }, { "cMorph", 0.5f }, { "cDecay", 0.35f },
            { "seqDiv", 1 }, { "seqPulses", 7 }, { "seqLength", 12 }, { "seqRange", 0.4f }, { "lockDest", 8 }, { "seqMutate", 0.25f },
            { "delayMix", 0.25f }, { "volume", 10 } } },

        { "Macro", "Terrain Pulse", {
            { "cModel", 5 }, { "cMix", 1.0f }, { "cHarm", 0.4f }, { "cTimbre", 0.5f }, { "cMorph", 0.45f }, { "cDecay", 0.3f },
            { "seqDiv", 1 }, { "seqPulses", 6 }, { "seqLength", 16 }, { "seqRange", 0.3f }, { "lfo1Shape", 1 }, { "lfo1Sync", 1 },
            { "lfo1Div", 2 }, { "lfo1Dest", 19 }, { "lfo1Depth", 0.4f }, { "delayMix", 0.2f }, { "volume", 6 } } },

        { "Macro", "Kick Into Modal", {
            { "cModel", 21 }, { "cMix", 0.2f }, { "cFeed", 0.8f }, { "cHarm", 0.6f }, { "cTimbre", 0.6f }, { "cDecay", 0.2f },
            { "bModel", 6 }, { "bMaterial", 0.3f }, { "bDecay", 0.45f }, { "abMix", 0.0f }, { "aModel", 0 }, { "aStructure", 0.3f },
            { "aBright", 0.45f }, { "aDamping", 0.45f }, { "seqDiv", 1 }, { "seqPulses", 5 }, { "seqLength", 16 }, { "seqRange", 0.35f },
            { "delayMix", 0.2f }, { "volume", 5 } } },
        // ================================================================ PACK 1.3 (al final para no mover los
        // índices de los presets anteriores: los proyectos viejos siguen abriendo el mismo sonido)

        // ---------------------------------------------------------------- BASS
        { "Bass", "Plucked Sub", {
            { "aModel", 1 }, { "aStructure", 0.02f }, { "aBright", 0.3f }, { "aDamping", 0.45f },
            { "aPosition", 0.2f }, { "abMix", 0.0f }, { "cutoff", 1200 }, { "keyTrack", 0.4f }, { "body", 0.3f },
            { "release", 150 }, { "width", 0.15f }, { "variation", 0.05f }, { "delayMix", 0.0f }, { "springMix", 0.0f }, { "volume", 22 } } },

        { "Bass", "Membrane Bass", {
            { "bModel", 0 }, { "bSweep", 0.25f }, { "bDecay", 0.45f }, { "bMaterial", 0.2f }, { "bTone", 0.3f },
            { "bPitch", -12 }, { "abMix", 1.0f }, { "width", 0.15f }, { "delayMix", 0.05f }, { "springMix", 0.0f }, { "volume", 11 } } },

        { "Bass", "Wood Bass", {
            { "bModel", 6 }, { "bPitch", -24 }, { "bMaterial", 0.15f }, { "bDecay", 0.45f }, { "bTone", 0.25f },
            { "bSweep", 0.0f }, { "abMix", 1.0f }, { "body", 0.5f }, { "width", 0.2f }, { "delayMix", 0.0f }, { "springMix", 0.05f }, { "volume", -3 } } },

        { "Bass", "FM Rubber Bass", {
            { "cModel", 10 }, { "cMix", 1.0f }, { "cHarm", 0.25f }, { "cTimbre", 0.35f }, { "cMorph", 0.3f },
            { "cDecay", 0.35f }, { "cColour", 0.4f }, { "cPitch", -24 }, { "width", 0.1f }, { "delayMix", 0.0f }, { "springMix", 0.0f }, { "volume", 5 } } },

        { "Bass", "Acid Resonator", {
            { "cModel", 0 }, { "cMix", 1.0f }, { "cHarm", 0.6f }, { "cTimbre", 0.35f }, { "cMorph", 0.5f },
            { "cDecay", 0.4f }, { "cPitch", -24 }, { "lfo1Shape", 1 }, { "lfo1Sync", 1 }, { "lfo1Div", 2 }, { "lfo1Dest", 18 },
            { "lfo1Depth", 0.25f }, { "width", 0.1f }, { "delayMix", 0.12f }, { "delayDiv", 3 }, { "springMix", 0.0f }, { "volume", 3 } } },

        { "Bass", "Tuned Boom", {
            { "cModel", 21 }, { "cMix", 1.0f }, { "cHarm", 0.3f }, { "cTimbre", 0.2f }, { "cMorph", 0.65f },
            { "cDecay", 0.6f }, { "cPitch", -12 }, { "width", 0.0f }, { "delayMix", 0.0f }, { "springMix", 0.0f }, { "volume", -1 } } },

        { "Bass", "Upright Pluck Bass", {
            { "aModel", 1 }, { "aExciter", 0 }, { "aStructure", 0.05f }, { "aBright", 0.4f },
            { "aDamping", 0.3f }, { "aPosition", 0.15f }, { "abMix", 0.0f }, { "body", 0.8f }, { "release", 250 },
            { "width", 0.3f }, { "delayMix", 0.0f }, { "springMix", 0.08f }, { "volume", 14 } } },

        { "Bass", "Tine Bass", {
            { "aModel", 5 }, { "aExciter", 4 }, { "aPitch", -12 }, { "aStructure", 0.3f }, { "aBright", 0.3f },
            { "aDamping", 0.45f }, { "abMix", 0.0f }, { "cutoff", 3000 }, { "chorusMix", 0.15f }, { "release", 200 },
            { "velSens", 0.8f }, { "width", 0.25f }, { "delayMix", 0.0f }, { "springMix", 0.05f }, { "volume", 7 } } },

        { "Bass", "Rubber Sweep", {
            { "bModel", 0 }, { "bSweep", 0.6f }, { "bDecay", 0.35f }, { "bMaterial", 0.3f }, { "bTone", 0.4f },
            { "bPitch", -12 }, { "abMix", 1.0f }, { "cutoff", 2500 }, { "drive", 0.3f }, { "width", 0.1f },
            { "delayMix", 0.08f }, { "springMix", 0.0f }, { "volume", 11 } } },

        { "Bass", "Harmonic Low", {
            { "cModel", 12 }, { "cMix", 1.0f }, { "cHarm", 0.3f }, { "cTimbre", 0.25f }, { "cMorph", 0.2f },
            { "cDecay", 0.4f }, { "cPitch", -24 }, { "width", 0.1f }, { "delayMix", 0.0f }, { "springMix", 0.0f }, { "volume", 7 } } },

        { "Bass", "Phase Bass", {
            { "cModel", 1 }, { "cMix", 1.0f }, { "cHarm", 0.4f }, { "cTimbre", 0.3f }, { "cMorph", 0.45f },
            { "cDecay", 0.35f }, { "cPitch", -24 }, { "drive", 0.2f }, { "width", 0.1f }, { "delayMix", 0.0f }, { "springMix", 0.0f }, { "volume", -4 } } },

        { "Bass", "Hand Drum Bass", {
            { "bModel", 10 }, { "bPitch", -24 }, { "bMaterial", 0.15f }, { "bDecay", 0.4f }, { "bTone", 0.25f },
            { "bSweep", 0.12f }, { "abMix", 1.0f }, { "body", 0.4f }, { "width", 0.15f }, { "delayMix", 0.0f }, { "springMix", 0.05f }, { "volume", 6 } } },

        // ---------------------------------------------------------------- PAD
        { "Pad", "Glass Cloud", {
            { "aModel", 0 }, { "aExciter", 2 }, { "aStructure", 0.2f }, { "aBright", 0.7f }, { "aDamping", 0.15f },
            { "abMix", 0.0f }, { "attack", 900 }, { "release", 3500 }, { "chorusMix", 0.4f }, { "lfo1Shape", 5 },
            { "lfo1Rate", 0.15f }, { "lfo1Dest", 3 }, { "lfo1Depth", 0.2f }, { "width", 0.9f }, { "delayMix", 0.25f }, { "springMix", 0.35f }, { "volume", 8 } } },

        { "Pad", "Sympathetic Halo", {
            { "aModel", 2 }, { "aExciter", 3 }, { "aStructure", 0.4f }, { "aBright", 0.5f }, { "aDamping", 0.12f },
            { "abMix", 0.0f }, { "sympathy", 0.5f }, { "attack", 1500 }, { "release", 4000 }, { "width", 1.0f },
            { "delayMix", 0.2f }, { "springMix", 0.4f }, { "volume", 4 } } },

        { "Pad", "Bowed Choir", {
            { "aModel", 1 }, { "aExciter", 2 }, { "aStructure", 0.1f }, { "aBright", 0.45f }, { "aDamping", 0.1f },
            { "abMix", 0.0f }, { "life", 0.5f }, { "attack", 600 }, { "release", 2500 }, { "chorusMix", 0.5f },
            { "width", 0.8f }, { "delayMix", 0.15f }, { "springMix", 0.3f }, { "volume", 10 } } },

        { "Pad", "Swarm Strings", {
            { "cModel", 16 }, { "cMix", 1.0f }, { "cLpg", 0 }, { "cHarm", 0.45f }, { "cTimbre", 0.3f }, { "cMorph", 0.5f },
            { "attack", 800 }, { "release", 3000 }, { "chorusMix", 0.3f }, { "cutoff", 7000 }, { "width", 0.9f },
            { "delayMix", 0.15f }, { "springMix", 0.3f }, { "volume", 2 } } },

        { "Pad", "Wavetable Dawn", {
            { "cModel", 13 }, { "cMix", 1.0f }, { "cLpg", 0 }, { "cHarm", 0.4f }, { "cTimbre", 0.5f }, { "cMorph", 0.3f },
            { "lfo1Shape", 0 }, { "lfo1Rate", 0.08f }, { "lfo1Dest", 19 }, { "lfo1Depth", 0.4f }, { "attack", 1200 },
            { "release", 3000 }, { "cutoff", 5000 }, { "chorusMix", 0.25f }, { "width", 0.85f }, { "delayMix", 0.2f }, { "springMix", 0.3f }, { "volume", 4 } } },

        { "Pad", "Minor Chord Pad", {
            { "cModel", 14 }, { "cMix", 1.0f }, { "cLpg", 0 }, { "cHarm", 0.4f }, { "cTimbre", 0.3f }, { "cMorph", 0.3f },
            { "attack", 700 }, { "release", 2500 }, { "cutoff", 4500 }, { "chorusMix", 0.35f }, { "width", 0.8f },
            { "delayMix", 0.2f }, { "springMix", 0.3f }, { "volume", 4 } } },

        { "Pad", "Breathing Tubes", {
            { "aModel", 0 }, { "aExciter", 3 }, { "aStructure", 0.05f }, { "aBright", 0.3f }, { "aDamping", 0.2f },
            { "bModel", 4 }, { "bExciter", 1 }, { "bMaterial", 0.5f }, { "bDecay", 0.7f }, { "abMix", 0.4f },
            { "attack", 1000 }, { "release", 3000 }, { "lfo1Shape", 0 }, { "lfo1Rate", 0.2f }, { "lfo1Dest", 17 },
            { "lfo1Depth", 0.15f }, { "width", 0.8f }, { "delayMix", 0.15f }, { "springMix", 0.35f }, { "volume", 10 } } },

        { "Pad", "Metal Haze", {
            { "aModel", 3 }, { "aExciter", 2 }, { "aStructure", 0.65f }, { "aBright", 0.4f }, { "aDamping", 0.1f },
            { "abMix", 0.0f }, { "attack", 1500 }, { "release", 4500 }, { "cutoff", 3500 }, { "width", 1.0f },
            { "delayMix", 0.3f }, { "delayFb", 0.55f }, { "springMix", 0.35f }, { "volume", 5 } } },

        { "Pad", "Warm String Machine", {
            { "cModel", 6 }, { "cMix", 1.0f }, { "cLpg", 0 }, { "cHarm", 0.5f }, { "cTimbre", 0.4f }, { "cMorph", 0.5f },
            { "attack", 400 }, { "release", 2000 }, { "cutoff", 6000 }, { "chorusMix", 0.5f }, { "width", 0.8f },
            { "delayMix", 0.1f }, { "springMix", 0.25f }, { "volume", 8 } } },

        { "Pad", "Bell Shimmer", {
            { "bModel", 3 }, { "bExciter", 1 }, { "bMaterial", 0.9f }, { "bDecay", 0.9f }, { "bTone", 0.5f },
            { "abMix", 1.0f }, { "attack", 1200 }, { "release", 4000 }, { "cutoff", 8000 }, { "width", 1.0f },
            { "delayMix", 0.25f }, { "springMix", 0.4f }, { "volume", 20 } } },

        // ---------------------------------------------------------------- KEYS
        { "Keys", "Dusty Upright", {
            { "aModel", 4 }, { "aExciter", 4 }, { "aStructure", 0.6f }, { "aBright", 0.25f }, { "aDamping", 0.45f },
            { "aPosition", 0.12f }, { "abMix", 0.0f }, { "body", 0.6f }, { "cutoff", 4500 }, { "decay", 10000 }, { "release", 350 },
            { "velSens", 0.85f }, { "variation", 0.3f }, { "chorusMix", 0.1f }, { "width", 0.5f }, { "delayMix", 0.1f },
            { "delayWow", 0.6f }, { "springMix", 0.2f }, { "volume", 27 } } },

        { "Keys", "Soft Tines", {
            { "aModel", 5 }, { "aExciter", 4 }, { "aStructure", 0.15f }, { "aBright", 0.25f }, { "aDamping", 0.4f },
            { "abMix", 0.0f }, { "cutoff", 5000 }, { "decay", 10000 }, { "release", 260 }, { "velSens", 0.8f },
            { "chorusMix", 0.3f }, { "width", 0.6f }, { "delayMix", 0.1f }, { "springMix", 0.15f }, { "volume", 9 } } },

        { "Keys", "Reed Growl", {
            { "aModel", 6 }, { "aExciter", 4 }, { "aStructure", 0.7f }, { "aBright", 0.65f }, { "aDamping", 0.5f },
            { "abMix", 0.0f }, { "drive", 0.35f }, { "decay", 10000 }, { "release", 180 }, { "velSens", 0.95f },
            { "lfo1Shape", 0 }, { "lfo1Rate", 5.5f }, { "lfo1Dest", 17 }, { "lfo1Depth", 0.25f }, { "width", 0.4f },
            { "delayMix", 0.08f }, { "springMix", 0.15f }, { "volume", -2 } } },

        { "Keys", "Toy Piano", {
            { "bModel", 1 }, { "bMaterial", 0.75f }, { "bDecay", 0.35f }, { "bTone", 0.75f }, { "bPitch", 12 },
            { "bSweep", 0.0f }, { "abMix", 1.0f }, { "body", 0.4f }, { "velSens", 0.7f }, { "variation", 0.25f },
            { "width", 0.5f }, { "delayMix", 0.12f }, { "springMix", 0.18f }, { "volume", 21 } } },

        { "Keys", "Glass Keys", {
            { "aModel", 0 }, { "aExciter", 4 }, { "aStructure", 0.15f }, { "aBright", 0.75f }, { "aDamping", 0.35f },
            { "bModel", 5 }, { "bMaterial", 0.85f }, { "bDecay", 0.4f }, { "abMix", 0.3f }, { "chorusMix", 0.2f },
            { "velSens", 0.8f }, { "width", 0.6f }, { "delayMix", 0.15f }, { "springMix", 0.2f }, { "volume", 18 } } },

        { "Keys", "FM Electric", {
            { "cModel", 2 }, { "cMix", 1.0f }, { "cHarm", 0.2f }, { "cTimbre", 0.5f }, { "cMorph", 0.3f },
            { "cDecay", 0.55f }, { "cColour", 0.5f }, { "chorusMix", 0.25f }, { "velSens", 0.8f }, { "width", 0.55f },
            { "delayMix", 0.1f }, { "springMix", 0.15f }, { "volume", 12 } } },

        { "Keys", "Celesta Box", {
            { "bModel", 1 }, { "bMaterial", 0.9f }, { "bDecay", 0.55f }, { "bTone", 0.7f }, { "bPitch", 12 },
            { "bSweep", 0.0f }, { "abMix", 1.0f }, { "body", 0.5f }, { "velSens", 0.75f }, { "width", 0.55f },
            { "delayMix", 0.1f }, { "springMix", 0.25f }, { "volume", 16 } } },

        { "Keys", "Quill Keys", {
            { "aModel", 1 }, { "aExciter", 0 }, { "aStructure", 0.08f }, { "aBright", 0.8f }, { "aDamping", 0.35f },
            { "aPosition", 0.05f }, { "abMix", 0.0f }, { "body", 0.6f }, { "keyTrack", 0.4f }, { "release", 120 },
            { "velSens", 0.4f }, { "width", 0.5f }, { "delayMix", 0.0f }, { "springMix", 0.2f }, { "volume", 11 } } },

        // ---------------------------------------------------------------- PERC (para minimal / rominimal)
        { "Perc", "Felt Kick", {
            { "cModel", 21 }, { "cMix", 1.0f }, { "cHarm", 0.3f }, { "cTimbre", 0.25f }, { "cMorph", 0.5f },
            { "cDecay", 0.5f }, { "cPitch", -12 }, { "width", 0.0f }, { "delayMix", 0.0f }, { "springMix", 0.0f }, { "volume", 6 } } },

        { "Perc", "Dry Snare", {
            { "cModel", 22 }, { "cMix", 1.0f }, { "cHarm", 0.5f }, { "cTimbre", 0.5f }, { "cMorph", 0.4f },
            { "width", 0.2f }, { "delayMix", 0.0f }, { "springMix", 0.15f }, { "volume", 9 } } },

        { "Perc", "Tight Hat", {
            { "cModel", 23 }, { "cMix", 1.0f }, { "cHarm", 0.5f }, { "cTimbre", 0.7f }, { "cMorph", 0.2f },
            { "width", 0.3f }, { "delayMix", 0.1f }, { "springMix", 0.0f }, { "volume", 14 } } },

        { "Perc", "Open Hat", {
            { "cModel", 23 }, { "cMix", 1.0f }, { "cHarm", 0.5f }, { "cTimbre", 0.6f }, { "cMorph", 0.7f },
            { "width", 0.4f }, { "delayMix", 0.0f }, { "springMix", 0.05f }, { "volume", 9 } } },

        { "Perc", "Rim Click", {
            { "bModel", 1 }, { "bMaterial", 0.3f }, { "bDecay", 0.08f }, { "bTone", 0.85f }, { "bPitch", 12 },
            { "bSweep", 0.0f }, { "abMix", 1.0f }, { "width", 0.2f }, { "delayMix", 0.0f }, { "springMix", 0.05f }, { "volume", 25 } } },

        { "Perc", "Wood Clave", {
            { "bModel", 1 }, { "bMaterial", 0.2f }, { "bDecay", 0.2f }, { "bTone", 0.8f }, { "bPitch", 12 },
            { "bSweep", 0.0f }, { "abMix", 1.0f }, { "width", 0.3f }, { "delayMix", 0.15f }, { "springMix", 0.0f }, { "volume", 21 } } },

        { "Perc", "Low Tom", {
            { "bModel", 0 }, { "bSweep", 0.3f }, { "bDecay", 0.4f }, { "bMaterial", 0.25f }, { "bTone", 0.4f },
            { "bPitch", -12 }, { "abMix", 1.0f }, { "width", 0.2f }, { "delayMix", 0.0f }, { "springMix", 0.1f }, { "volume", 12 } } },

        { "Perc", "Metal Tick", {
            { "bModel", 2 }, { "bMaterial", 0.9f }, { "bDecay", 0.12f }, { "bTone", 0.9f }, { "bSpread", 0.3f },
            { "abMix", 1.0f }, { "width", 0.5f }, { "delayMix", 0.15f }, { "springMix", 0.0f }, { "volume", 25 } } },

        { "Perc", "Glass Shaker", {
            { "bModel", 5 }, { "bExciter", 3 }, { "bMaterial", 0.8f }, { "bDecay", 0.08f }, { "bTone", 0.8f },
            { "abMix", 1.0f }, { "width", 0.6f }, { "delayMix", 0.0f }, { "springMix", 0.0f }, { "volume", 21 } } },

        { "Perc", "Conga Slap", {
            { "bModel", 10 }, { "bMaterial", 0.3f }, { "bDecay", 0.25f }, { "bTone", 0.55f }, { "bSweep", 0.1f },
            { "abMix", 1.0f }, { "body", 0.3f }, { "velSens", 0.9f }, { "width", 0.3f }, { "delayMix", 0.0f }, { "springMix", 0.08f }, { "volume", 10 } } },

        { "Perc", "Tin Bell", {
            { "bModel", 3 }, { "bMaterial", 0.5f }, { "bDecay", 0.2f }, { "bTone", 0.6f }, { "bPitch", 12 },
            { "abMix", 1.0f }, { "width", 0.3f }, { "delayMix", 0.12f }, { "springMix", 0.0f }, { "volume", 13 } } },

        { "Perc", "Noise Snap", {
            { "cModel", 17 }, { "cMix", 1.0f }, { "cHarm", 0.6f }, { "cTimbre", 0.6f }, { "cMorph", 0.5f },
            { "cDecay", 0.2f }, { "width", 0.5f }, { "delayMix", 0.0f }, { "springMix", 0.3f }, { "volume", 3 } } },

        // ---------------------------------------------------------------- DRONE
        { "Drone", "Frozen Glass", {
            { "aModel", 0 }, { "aExciter", 2 }, { "aStructure", 0.3f }, { "aBright", 0.65f }, { "aDamping", 0.05f },
            { "abMix", 0.0f }, { "freeze", 1 }, { "attack", 2000 }, { "release", 5000 }, { "width", 1.0f },
            { "delayMix", 0.3f }, { "springMix", 0.4f }, { "volume", 13 } } },

        { "Drone", "Low Om", {
            { "aModel", 1 }, { "aExciter", 3 }, { "aPitch", -12 }, { "aStructure", 0.05f }, { "aBright", 0.4f },
            { "aDamping", 0.05f }, { "abMix", 0.0f }, { "body", 0.5f }, { "cutoff", 1500 }, { "attack", 2500 }, { "release", 5000 },
            { "lfo1Shape", 5 }, { "lfo1Rate", 0.1f }, { "lfo1Dest", 10 }, { "lfo1Depth", 0.2f }, { "width", 0.7f },
            { "delayMix", 0.15f }, { "springMix", 0.3f }, { "volume", 11 } } },

        { "Drone", "Gong Bath", {
            { "bModel", 11 }, { "bExciter", 0 }, { "bMaterial", 1.0f }, { "bDecay", 0.95f }, { "bTone", 0.35f },
            { "bSpread", 0.25f }, { "abMix", 1.0f }, { "attack", 400 }, { "release", 6000 }, { "width", 1.0f },
            { "delayMix", 0.15f }, { "springMix", 0.4f }, { "volume", 9 } } },

        { "Drone", "Noise Ocean", {
            { "cModel", 17 }, { "cMix", 1.0f }, { "cLpg", 0 }, { "cHarm", 0.3f }, { "cTimbre", 0.4f }, { "cMorph", 0.3f },
            { "lfo1Shape", 0 }, { "lfo1Rate", 0.07f }, { "lfo1Dest", 19 }, { "lfo1Depth", 0.5f }, { "attack", 2000 },
            { "release", 5000 }, { "cutoff", 5000 }, { "width", 1.0f }, { "delayMix", 0.1f }, { "springMix", 0.4f }, { "volume", 0 } } },

        { "Drone", "Sympathetic Field", {
            { "aModel", 2 }, { "aExciter", 1 }, { "aStructure", 0.35f }, { "aBright", 0.55f }, { "aDamping", 0.05f },
            { "abMix", 0.0f }, { "freeze", 1 }, { "sympathy", 0.8f }, { "pedal", 1 }, { "release", 5000 },
            { "width", 0.9f }, { "delayMix", 0.2f }, { "springMix", 0.35f }, { "volume", 1 } } },

        { "Drone", "Tape Organ Drone", {
            { "cModel", 12 }, { "cMix", 1.0f }, { "cLpg", 0 }, { "cHarm", 0.5f }, { "cTimbre", 0.35f }, { "cMorph", 0.4f },
            { "attack", 1500 }, { "release", 4000 }, { "chorusMix", 0.3f }, { "cutoff", 4000 }, { "width", 0.8f },
            { "delayMix", 0.35f }, { "delayFb", 0.6f }, { "delayWow", 0.8f }, { "springMix", 0.25f }, { "volume", 2 } } },

        // ---------------------------------------------------------------- HYBRID
        { "Hybrid", "String Into Plate", {
            { "aModel", 1 }, { "aBright", 0.6f }, { "aDamping", 0.4f }, { "bModel", 2 }, { "bMaterial", 0.8f },
            { "bDecay", 0.5f }, { "abMix", 0.5f }, { "xfeed", 0.7f }, { "width", 0.6f }, { "delayMix", 0.15f }, { "springMix", 0.2f }, { "volume", 13 } } },

        { "Hybrid", "Bowed Bells", {
            { "aModel", 3 }, { "aExciter", 2 }, { "aStructure", 0.5f }, { "aDamping", 0.25f }, { "bModel", 3 },
            { "bMaterial", 0.9f }, { "bDecay", 0.7f }, { "abMix", 0.5f }, { "xfeed", 0.5f }, { "attack", 300 },
            { "release", 2000 }, { "width", 0.8f }, { "delayMix", 0.2f }, { "springMix", 0.3f }, { "volume", 5 } } },

        { "Hybrid", "Piano Gamelan", {
            { "aModel", 4 }, { "aExciter", 4 }, { "aStructure", 0.35f }, { "aBright", 0.5f }, { "aDamping", 0.35f },
            { "bModel", 11 }, { "bMaterial", 0.9f }, { "bDecay", 0.6f }, { "bSpread", 0.2f }, { "abMix", 0.45f },
            { "xfeed", 0.3f }, { "body", 0.4f }, { "width", 0.7f }, { "delayMix", 0.12f }, { "springMix", 0.25f }, { "volume", 13 } } },

        { "Hybrid", "Tine Marimba", {
            { "aModel", 5 }, { "aExciter", 4 }, { "aStructure", 0.35f }, { "aBright", 0.45f }, { "aDamping", 0.42f },
            { "bModel", 6 }, { "bMaterial", 0.25f }, { "bDecay", 0.45f }, { "bSweep", 0.0f }, { "abMix", 0.5f },
            { "chorusMix", 0.2f }, { "width", 0.6f }, { "delayMix", 0.12f }, { "springMix", 0.15f }, { "volume", 10 } } },

        { "Hybrid", "FM Into Drum", {
            { "cModel", 10 }, { "cMix", 0.4f }, { "cFeed", 0.6f }, { "cHarm", 0.35f }, { "cTimbre", 0.5f }, { "cDecay", 0.3f },
            { "bModel", 0 }, { "bSweep", 0.2f }, { "bDecay", 0.35f }, { "abMix", 1.0f }, { "width", 0.4f },
            { "delayMix", 0.15f }, { "springMix", 0.1f }, { "volume", 15 } } },

        { "Hybrid", "Morphing Objects", {
            { "aModel", 0 }, { "aStructure", 0.35f }, { "aBright", 0.6f }, { "aDamping", 0.4f }, { "bModel", 5 },
            { "bMaterial", 0.75f }, { "bDecay", 0.45f }, { "abMix", 0.5f }, { "lfo2Shape", 1 }, { "lfo2Sync", 1 },
            { "lfo2Div", 2 }, { "lfo2Dest", 12 }, { "lfo2Depth", 0.6f }, { "width", 0.7f }, { "delayMix", 0.2f }, { "springMix", 0.2f }, { "volume", 20 } } },

        // ---------------------------------------------------------------- GLITCH
        { "Glitch", "Stutter Bells", {
            { "bModel", 3 }, { "bMaterial", 0.85f }, { "bDecay", 0.55f }, { "abMix", 1.0f }, { "glitchOn", 1 },
            { "glitchProb", 0.45f }, { "glitchLen", 2 }, { "glitchVar", 0.6f }, { "width", 0.8f }, { "delayMix", 0.2f }, { "volume", 11 } } },

        { "Glitch", "Crushed Piano", {
            { "aModel", 4 }, { "aExciter", 4 }, { "aStructure", 0.4f }, { "aBright", 0.5f }, { "abMix", 0.0f },
            { "glitchOn", 1 }, { "glitchCrush", 0.7f }, { "glitchProb", 0.3f }, { "width", 0.6f }, { "springMix", 0.2f }, { "volume", 18 } } },

        { "Glitch", "Reverse Rain", {
            { "cModel", 18 }, { "cMix", 1.0f }, { "cHarm", 0.5f }, { "cTimbre", 0.5f }, { "cMorph", 0.6f },
            { "glitchOn", 1 }, { "glitchProb", 0.5f }, { "glitchVar", 0.9f }, { "width", 1.0f }, { "springMix", 0.3f }, { "volume", 11 } } },

        { "Glitch", "Data Chirps", {
            { "cModel", 7 }, { "cMix", 1.0f }, { "cHarm", 0.5f }, { "cTimbre", 0.5f }, { "cMorph", 0.5f }, { "cDecay", 0.3f },
            { "glitchOn", 1 }, { "glitchLen", 3 }, { "glitchProb", 0.6f }, { "glitchCrush", 0.4f }, { "lfo2Shape", 4 },
            { "lfo2Dest", 18 }, { "lfo2Depth", 0.5f }, { "width", 0.7f }, { "delayMix", 0.15f }, { "volume", -3 } } },

        { "Glitch", "Broken Tape", {
            { "aModel", 1 }, { "aBright", 0.55f }, { "aDamping", 0.35f }, { "abMix", 0.0f }, { "glitchOn", 1 },
            { "glitchProb", 0.25f }, { "lfo1Shape", 4 }, { "lfo1Dest", 1 }, { "lfo1Depth", 0.05f }, { "lfo1Rate", 3 },
            { "delayMix", 0.4f }, { "delayFb", 0.6f }, { "delayWow", 1.0f }, { "width", 0.6f }, { "volume", 13 } } },

        { "Glitch", "Glitch Membrane", {
            { "bModel", 0 }, { "bExciter", 3 }, { "bSweep", 0.3f }, { "bDecay", 0.3f }, { "abMix", 1.0f },
            { "glitchOn", 1 }, { "glitchProb", 0.4f }, { "glitchLen", 1 }, { "glitchCrush", 0.3f }, { "width", 0.5f },
            { "delayMix", 0.15f }, { "volume", 17 } } },

        // ---------------------------------------------------------------- IMPACT
        { "Impact", "Hollow Log", {
            { "bModel", 4 }, { "bMaterial", 0.2f }, { "bDecay", 0.35f }, { "bTone", 0.45f }, { "abMix", 1.0f },
            { "body", 0.4f }, { "width", 0.4f }, { "delayMix", 0.12f }, { "springMix", 0.12f }, { "volume", 14 } } },

        { "Impact", "Tuned Pipes", {
            { "bModel", 4 }, { "bMaterial", 0.75f }, { "bDecay", 0.65f }, { "bTone", 0.55f }, { "abMix", 1.0f },
            { "width", 0.6f }, { "delayMix", 0.15f }, { "springMix", 0.25f }, { "volume", 12 } } },

        { "Impact", "Bright Plate", {
            { "bModel", 2 }, { "bMaterial", 0.95f }, { "bDecay", 0.5f }, { "bTone", 0.8f }, { "bSpread", 0.1f },
            { "abMix", 1.0f }, { "width", 0.6f }, { "delayMix", 0.12f }, { "springMix", 0.2f }, { "volume", 20 } } },

        { "Impact", "Temple Bell", {
            { "bModel", 3 }, { "bMaterial", 1.0f }, { "bDecay", 0.85f }, { "bTone", 0.45f }, { "bPitch", -12 },
            { "abMix", 1.0f }, { "release", 3000 }, { "width", 0.8f }, { "delayMix", 0.15f }, { "springMix", 0.3f }, { "volume", 2 } } },

        { "Impact", "Bouncing Wood", {
            { "bModel", 1 }, { "bExciter", 2 }, { "bMaterial", 0.15f }, { "bDecay", 0.3f }, { "bTone", 0.6f },
            { "abMix", 1.0f }, { "width", 0.5f }, { "delayMix", 0.12f }, { "springMix", 0.1f }, { "volume", 14 } } },

        { "Impact", "Crystal Shards", {
            { "bModel", 5 }, { "bExciter", 2 }, { "bMaterial", 0.95f }, { "bDecay", 0.4f }, { "bTone", 0.9f },
            { "bSpread", 0.4f }, { "abMix", 1.0f }, { "width", 0.9f }, { "delayMix", 0.2f }, { "springMix", 0.25f }, { "volume", 21 } } },

        // ---------------------------------------------------------------- RESONATOR
        { "Resonator", "Koto Pluck", {
            { "aModel", 1 }, { "aStructure", 0.15f }, { "aBright", 0.7f }, { "aDamping", 0.4f }, { "aPosition", 0.08f },
            { "abMix", 0.0f }, { "body", 0.5f }, { "width", 0.5f }, { "delayMix", 0.15f }, { "springMix", 0.2f }, { "volume", 16 } } },

        { "Resonator", "Metallic Strike", {
            { "aModel", 3 }, { "aExciter", 1 }, { "aStructure", 0.4f }, { "aBright", 0.65f }, { "aDamping", 0.4f },
            { "abMix", 0.0f }, { "width", 0.6f }, { "delayMix", 0.15f }, { "springMix", 0.25f }, { "volume", 18 } } },

        { "Resonator", "Modal Wood", {
            { "aModel", 0 }, { "aExciter", 1 }, { "aStructure", 0.7f }, { "aBright", 0.3f }, { "aDamping", 0.6f },
            { "abMix", 0.0f }, { "body", 0.4f }, { "width", 0.4f }, { "delayMix", 0.12f }, { "springMix", 0.12f }, { "volume", 13 } } },

        { "Resonator", "Bowed Glass", {
            { "aModel", 0 }, { "aExciter", 2 }, { "aStructure", 0.05f }, { "aBright", 0.8f }, { "aDamping", 0.2f },
            { "abMix", 0.0f }, { "attack", 200 }, { "release", 1500 }, { "width", 0.7f }, { "delayMix", 0.2f }, { "springMix", 0.3f }, { "volume", 10 } } },

        { "Resonator", "Blown Bottle", {
            { "aModel", 0 }, { "aExciter", 3 }, { "aStructure", 0.0f }, { "aBright", 0.25f }, { "aDamping", 0.3f },
            { "aPosition", 0.5f }, { "abMix", 0.0f }, { "attack", 80 }, { "release", 400 }, { "width", 0.4f },
            { "delayMix", 0.1f }, { "springMix", 0.25f }, { "volume", 12 } } },

        { "Resonator", "Twelve String", {
            { "aModel", 2 }, { "aStructure", 0.25f }, { "aBright", 0.6f }, { "aDamping", 0.4f }, { "abMix", 0.0f },
            { "variation", 0.3f }, { "chorusMix", 0.2f }, { "width", 0.7f }, { "delayMix", 0.1f }, { "springMix", 0.2f }, { "volume", 7 } } },

        // ---------------------------------------------------------------- SEQUENCE
        { "Sequence", "Minimal Wood Loop", {
            { "bModel", 1 }, { "bMaterial", 0.15f }, { "bDecay", 0.2f }, { "bTone", 0.65f }, { "abMix", 1.0f },
            { "seqPulses", 5 }, { "seqLength", 16 }, { "seqRotate", 3 }, { "seqRange", 0.3f }, { "seqSwing", 0.58f },
            { "variation", 0.3f }, { "width", 0.5f }, { "delayMix", 0.2f }, { "volume", 15 } } },

        { "Sequence", "Dub Chord Steps", {
            { "cModel", 14 }, { "cMix", 1.0f }, { "cHarm", 0.45f }, { "cTimbre", 0.35f }, { "cDecay", 0.35f },
            { "seqPulses", 3 }, { "seqLength", 8 }, { "seqDiv", 3 }, { "seqRange", 0.15f }, { "width", 0.6f },
            { "delayMix", 0.4f }, { "delayDiv", 3 }, { "delayFb", 0.55f }, { "springMix", 0.2f }, { "volume", 6 } } },

        { "Sequence", "Bassline 7/16", {
            { "cModel", 0 }, { "cMix", 1.0f }, { "cHarm", 0.55f }, { "cTimbre", 0.4f }, { "cDecay", 0.35f },
            { "seqPulses", 7 }, { "seqLength", 16 }, { "seqOctave", 2 }, { "seqRange", 0.35f }, { "seqGate", 0.3f },
            { "width", 0.1f }, { "delayMix", 0.08f }, { "springMix", 0.0f }, { "volume", 7 } } },

        { "Sequence", "Bell Polyrhythm", {
            { "bModel", 3 }, { "bMaterial", 0.85f }, { "bDecay", 0.5f }, { "abMix", 1.0f }, { "seqPulses", 5 },
            { "seqLength", 12 }, { "seqLenNote", 7 }, { "seqRange", 0.7f }, { "width", 0.8f }, { "delayMix", 0.25f }, { "springMix", 0.2f }, { "volume", 7 } } },

        { "Sequence", "Mbira Cycle", {
            { "bModel", 8 }, { "bMaterial", 0.7f }, { "bDecay", 0.45f }, { "bTone", 0.6f }, { "abMix", 1.0f },
            { "body", 0.6f }, { "seqPulses", 9 }, { "seqLength", 16 }, { "seqDir", 2 }, { "seqRange", 0.6f },
            { "seqScale", 8 }, { "width", 0.6f }, { "delayMix", 0.15f }, { "volume", 1 } } },

        { "Sequence", "Probability Pluck", {
            { "aModel", 1 }, { "aBright", 0.6f }, { "aDamping", 0.4f }, { "abMix", 0.0f }, { "seqPulses", 11 },
            { "seqProb", 0.65f }, { "seqMutate", 0.3f }, { "seqRange", 0.55f }, { "width", 0.6f }, { "delayMix", 0.25f },
            { "springMix", 0.15f }, { "volume", 10 } } },

        // ================================================================ PACK 1.5 (al final: los índices viejos no cambian)
        // Carril MODEL ("@m0"… = modelo del motor C por paso), motor C como excitador de A, C > B FEED
        { "Sequence", "Model Hopper", {
            { "cModel", 8 }, { "cMix", 1.0f }, { "cHarm", 0.5f }, { "cTimbre", 0.45f }, { "cDecay", 0.3f },
            { "seqLength", 16 }, { "seqPulses", 9 }, { "seqRange", 0.3f }, { "seqOctave", 3 },
            { "@m0", 21 }, { "@m1", -1 }, { "@m2", 23 }, { "@m3", 14 }, { "@m4", 22 }, { "@m5", -1 }, { "@m6", 23 }, { "@m7", 10 },
            { "width", 0.5f }, { "delayMix", 0.18f }, { "volume", 5 } } },

        { "Perc", "Plaits Kit Walk", {
            { "cModel", 21 }, { "cMix", 1.0f }, { "cHarm", 0.4f }, { "cTimbre", 0.55f }, { "cMorph", 0.45f }, { "cDecay", 0.25f },
            { "seqLength", 16 }, { "seqPulses", 11 }, { "seqRotate", 1 }, { "seqRange", 0.1f }, { "seqOctave", 2 }, { "seqSwing", 0.58f },
            { "@m0", 21 }, { "@m1", 23 }, { "@m2", 23 }, { "@m3", 22 }, { "@m4", 23 }, { "@m5", 21 }, { "@m6", 23 }, { "@m7", 22 },
            { "width", 0.4f }, { "springMix", 0.08f }, { "volume", 2 } } },

        { "Sequence", "Chord Morph Steps", {
            { "cModel", 14 }, { "cMix", 1.0f }, { "cHarm", 0.4f }, { "cTimbre", 0.4f }, { "cMorph", 0.3f }, { "cDecay", 0.45f },
            { "seqLength", 8 }, { "seqPulses", 3 }, { "seqDiv", 3 }, { "seqRange", 0.2f },
            { "@m0", 14 }, { "@m1", 6 }, { "@m2", 12 }, { "@m3", 14 }, { "@m4", 13 },
            { "width", 0.7f }, { "delayMix", 0.35f }, { "delayDiv", 3 }, { "delayFb", 0.5f }, { "springMix", 0.2f }, { "volume", 15 } } },

        { "Glitch", "Speech Ratchets", {
            { "cModel", 15 }, { "cMix", 1.0f }, { "cHarm", 0.6f }, { "cTimbre", 0.5f }, { "cMorph", 0.6f }, { "cDecay", 0.3f },
            { "seqLength", 16 }, { "seqPulses", 7 }, { "seqRange", 0.4f }, { "seqRandom", 0.3f },
            { "@m0", 15 }, { "@m1", 11 }, { "@m2", 15 }, { "@m3", 7 },
            { "glitchOn", 1 }, { "glitchProb", 0.35f }, { "glitchMix", 0.6f }, { "delayMix", 0.2f }, { "volume", 9 } } },

        { "Sequence", "Polymeter Models", {
            { "cModel", 19 }, { "cMix", 0.7f }, { "abMix", 1.0f }, { "bModel", 1 }, { "bMaterial", 0.4f }, { "bDecay", 0.3f },
            { "cHarm", 0.5f }, { "cTimbre", 0.5f }, { "cDecay", 0.35f },
            { "seqLength", 16 }, { "seqPulses", 7 }, { "seqLenModel", 5 }, { "seqRange", 0.45f },
            { "@m0", 19 }, { "@m1", 20 }, { "@m2", 10 }, { "@m3", 19 }, { "@m4", 2 },
            { "width", 0.6f }, { "delayMix", 0.22f }, { "volume", 8 } } },

        { "Drone", "Plaits Bowed Modal", {
            { "aModel", 0 }, { "aExciter", 5 }, { "aStructure", 0.35f }, { "aBright", 0.55f }, { "aDamping", 0.2f }, { "abMix", 0.0f },
            { "cModel", 13 }, { "cLpg", 0 }, { "cHarm", 0.35f }, { "cTimbre", 0.4f }, { "cMorph", 0.5f },
            { "attack", 400 }, { "release", 2500 }, { "width", 0.7f }, { "springMix", 0.25f }, { "chorusMix", 0.25f }, { "volume", -2 } } },

        { "Hybrid", "FM Struck String", {
            { "aModel", 1 }, { "aExciter", 5 }, { "aBright", 0.6f }, { "aDamping", 0.35f }, { "abMix", 0.0f },
            { "cModel", 2 }, { "cHarm", 0.45f }, { "cTimbre", 0.55f }, { "cMorph", 0.4f }, { "cDecay", 0.2f },
            { "width", 0.5f }, { "delayMix", 0.15f }, { "volume", 5 } } },

        { "Keys", "Noise Tine", {
            { "aModel", 5 }, { "aExciter", 5 }, { "aBright", 0.5f }, { "aStructure", 0.3f }, { "abMix", 0.0f },
            { "cModel", 17 }, { "cHarm", 0.6f }, { "cTimbre", 0.7f }, { "cDecay", 0.15f },
            { "chorusMix", 0.3f }, { "width", 0.5f }, { "volume", 6 } } },

        { "Pad", "Chord Resonator", {
            { "aModel", 2 }, { "aExciter", 5 }, { "aStructure", 0.5f }, { "aBright", 0.5f }, { "aDamping", 0.25f }, { "abMix", 0.0f },
            { "cModel", 14 }, { "cLpg", 0 }, { "cHarm", 0.4f }, { "cTimbre", 0.3f },
            { "attack", 600 }, { "release", 3000 }, { "width", 0.8f }, { "springMix", 0.3f }, { "volume", 6 } } },

        { "Impact", "Gong Driven", {
            { "bModel", 11 }, { "bMaterial", 0.7f }, { "bDecay", 0.6f }, { "abMix", 1.0f },
            { "cModel", 8 }, { "cFeedB", 0.6f }, { "cHarm", 0.5f }, { "cTimbre", 0.4f }, { "cDecay", 0.4f },
            { "width", 0.6f }, { "springMix", 0.2f }, { "volume", 7 } } },

        { "Bass", "Bar Feedback Bass", {
            { "bModel", 1 }, { "bMaterial", 0.35f }, { "bDecay", 0.3f }, { "bTone", 0.45f }, { "abMix", 1.0f },
            { "cModel", 0 }, { "cFeedB", 0.45f }, { "cHarm", 0.5f }, { "cTimbre", 0.35f }, { "cDecay", 0.3f },
            { "width", 0.1f }, { "volume", 11 } } },

        { "Drone", "Granular Plate", {
            { "bModel", 2 }, { "bMaterial", 0.75f }, { "bDecay", 0.7f }, { "abMix", 1.0f },
            { "cModel", 18 }, { "cLpg", 0 }, { "cFeedB", 0.8f }, { "cHarm", 0.5f }, { "cTimbre", 0.6f }, { "cMorph", 0.5f },
            { "attack", 300 }, { "release", 3000 }, { "width", 0.8f }, { "springMix", 0.3f }, { "volume", 16 } } },
    };
    return list;
}

} // namespace presets
