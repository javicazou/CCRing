#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace
{
    const juce::StringArray aModels   { "Modal", "String", "Sympathetic", "Metallic", "Piano", "Tine Keys", "Reed Keys" };
    const juce::StringArray aExciters { "Pluck", "Strike", "Bow", "Blow", "Hammer", "Engine C" };
    const juce::StringArray bModels   { "Membrane", "Bar", "Plate", "Bell", "Tube", "Glass", "Marimba", "Vibes", "Mbira", "Steel Pan", "Hand Drum", "Gong" };
    const juce::StringArray bExciters { "Mallet", "Noise", "Bounce", "Crackle" };
    const juce::StringArray fltTypes  { "Low Pass", "Band Pass", "High Pass", "Notch", "Ladder 24", "Comb", "Formant" };
    const juce::StringArray lfoShapes { "Sine", "Triangle", "Saw", "Square", "S&H", "Drift" };
    const juce::StringArray lfoDivs   { "4 Bars", "2 Bars", "1 Bar", "1/2", "1/4", "1/8", "1/16", "1/32" };
    constexpr double lfoDivBeats[]    { 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125 };
    const juce::StringArray lfoDests  { "Off", "Pitch", "A Structure", "A Bright", "A Damping", "A Position",
                                        "B Material", "B Decay", "B Tone", "B Spread", "Cutoff", "Resonance",
                                        "A/B Mix", "Delay Time", "Glitch Prob", "Spring Mix", "A > B Feed", "Volume (Tremolo)",
                                        "C Timbre", "C Morph", "C Harmonics" };
    const juce::StringArray glitchLens { "1/8", "1/16", "1/32", "1/64" };
    constexpr float glitchLenFrac[]    { 2.0f, 1.0f, 0.5f, 0.25f };
    const juce::StringArray delayDivs  { "1/16", "1/8T", "1/8", "1/8D", "1/4", "1/4D", "1/2" };
    constexpr double delayDivBeats[]   { 0.25, 1.0 / 3.0, 0.5, 0.75, 1.0, 1.5, 2.0 };
    const juce::StringArray seqDivs    { "1/32", "1/16", "1/16T", "1/8", "1/8T", "1/4" };
    constexpr double seqDivPpq[]       { 0.125, 0.25, 1.0 / 6.0, 0.5, 1.0 / 3.0, 1.0 };
    const juce::StringArray rndRates   { "Step", "1/8", "1/4", "1 Bar", "2 Bars", "4 Bars" };
    constexpr double rndRatePpq[]      { 0.0, 0.5, 1.0, 4.0, 8.0, 16.0 };
    const juce::StringArray noteNames  { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    juce::String hzText (float v, int)   { return v < 1000.0f ? juce::String (v, v < 10.0f ? 2 : 0) + " Hz" : juce::String (v / 1000.0f, 2) + " kHz"; }
    juce::String msText (float v, int)   { return v < 1000.0f ? juce::String (v, v < 10.0f ? 1 : 0) + " ms" : juce::String (v / 1000.0f, 2) + " s"; }
    juce::String pctText (float v, int)  { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }
    juce::String bipText (float v, int)  { const int p = juce::roundToInt (v * 100.0f); return (p > 0 ? "+" : "") + juce::String (p) + " %"; }
    juce::String centText (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " ct"; }
    juce::String dbText (float v, int)   { return juce::String (v, 1) + " dB"; }

    juce::NormalisableRange<float> skewed (float lo, float hi, float centre)
    {
        juce::NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    float clamp01 (float v) { return juce::jlimit (0.0f, 1.0f, v); }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout URingAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto addFloat = [&] (const String& id, const String& name, NormalisableRange<float> range, float def,
                         String (*text) (float, int))
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def,
                        AudioParameterFloatAttributes().withStringFromValueFunction (text)));
    };
    auto addChoice = [&] (const String& id, const String& name, const StringArray& choices, int def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, choices, def));
    };
    auto addBool = [&] (const String& id, const String& name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };
    auto addInt = [&] (const String& id, const String& name, int lo, int hi, int def)
    {
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def,
                        AudioParameterIntAttributes().withStringFromValueFunction (
                            [] (int v, int) { return (v > 0 ? "+" : "") + String (v) + " st"; })));
    };

    // ---- Motor A
    addChoice (IDs::aModel,     "A Model", aModels, 0);
    addChoice (IDs::aExciter,   "A Exciter", aExciters, 0);
    addFloat  (IDs::aStructure, "A Structure", { 0.0f, 1.0f }, 0.3f, pctText);
    addFloat  (IDs::aBright,    "A Brightness", { 0.0f, 1.0f }, 0.55f, pctText);
    addFloat  (IDs::aDamping,   "A Damping", { 0.0f, 1.0f }, 0.45f, pctText);
    addFloat  (IDs::aPosition,  "A Position", { 0.0f, 1.0f }, 0.3f, pctText);
    addInt    (IDs::aPitch,     "A Pitch", -24, 24, 0);

    // ---- Motor B
    addChoice (IDs::bModel,     "B Model", bModels, 0);
    addChoice (IDs::bExciter,   "B Exciter", bExciters, 0);
    addFloat  (IDs::bMaterial,  "B Material", { 0.0f, 1.0f }, 0.4f, pctText);
    addFloat  (IDs::bDecay,     "B Decay", { 0.0f, 1.0f }, 0.35f, pctText);
    addFloat  (IDs::bTone,      "B Tone", { 0.0f, 1.0f }, 0.5f, pctText);
    addFloat  (IDs::bPosition,  "B Position", { 0.0f, 1.0f }, 0.3f, pctText);
    addFloat  (IDs::bSpread,    "B Spread", { 0.0f, 1.0f }, 0.0f, pctText);
    addFloat  (IDs::bSweep,     "B Sweep", { 0.0f, 1.0f }, 0.1f, pctText);
    addInt    (IDs::bPitch,     "B Pitch", -24, 24, 0);

    // ---- Mezcla
    addFloat (IDs::abMix, "A/B Mix", { 0.0f, 1.0f }, 0.35f, pctText);
    addFloat (IDs::xfeed, "A to B Feed", { 0.0f, 1.0f }, 0.0f, pctText);
    addFloat (IDs::tune,  "Fine Tune", { -50.0f, 50.0f }, 0.0f, centText);
    addFloat (IDs::variation, "Variation", { 0.0f, 1.0f }, 0.2f, pctText);
    addFloat (IDs::width,     "Stereo Width", { 0.0f, 1.0f }, 0.5f, pctText);

    // ---- Filtro / envolvente / salida
    addChoice (IDs::fltType,   "Filter Type", fltTypes, 0);
    addFloat  (IDs::cutoff,    "Cutoff", skewed (20.0f, 20000.0f, 1500.0f), 12000.0f, hzText);
    addFloat  (IDs::resonance, "Resonance", { 0.0f, 1.0f }, 0.1f, pctText);
    addFloat  (IDs::fltEnv,    "Filter Env", { -1.0f, 1.0f }, 0.0f, bipText);
    addFloat  (IDs::fltDecay,  "Filter Decay", skewed (5.0f, 5000.0f, 300.0f), 300.0f, msText);
    addFloat  (IDs::keyTrack,  "Key Track", { 0.0f, 1.0f }, 0.3f, pctText);
    addFloat  (IDs::drive,     "Drive", { 0.0f, 1.0f }, 0.0f, pctText);
    addFloat  (IDs::attack,    "Attack",  skewed (0.3f, 5000.0f, 100.0f), 1.0f, msText);
    addFloat  (IDs::decay,     "Decay",   skewed (5.0f, 10000.0f, 1000.0f), 3000.0f, msText);
    addFloat  (IDs::sustain,   "Sustain", { 0.0f, 1.0f }, 1.0f, pctText);
    addFloat  (IDs::release,   "Release", skewed (5.0f, 8000.0f, 800.0f), 700.0f, msText);
    addFloat  (IDs::velSens,   "Velocity", { 0.0f, 1.0f }, 0.6f, pctText);
    addFloat  (IDs::volume,    "Volume", { -36.0f, 6.0f }, URingAudioProcessor::kStartVolumeDb, dbText);
    addFloat  (IDs::body,      "Body Resonance", { 0.0f, 1.0f }, 0.0f, pctText);
    addFloat  (IDs::life,      "Life", { 0.0f, 1.0f }, 0.0f, pctText);
    addBool   (IDs::pedal,     "Sustain Pedal", false);
    addFloat  (IDs::sympathy,  "Sympathetic Resonance", { 0.0f, 1.0f }, 0.0f, pctText);
    addFloat  (IDs::chorusMix,   "Chorus Mix", { 0.0f, 1.0f }, 0.0f, pctText);
    addFloat  (IDs::chorusRate,  "Chorus Rate", skewed (0.05f, 6.0f, 0.8f), 0.6f, hzText);
    addFloat  (IDs::chorusDepth, "Chorus Depth", { 0.0f, 1.0f }, 0.5f, pctText);
    addChoice (IDs::lockDest,  "Lock Destination", { "A Brightness", "A Damping", "A Position", "A Structure",
                                                     "B Material", "B Tone", "B Decay", "Filter Cutoff",
                                                     "C Harmonics", "C Timbre", "C Morph" }, 0);
    // ---- Motor C: MACRO (24 modelos de Plaits)
    {
        StringArray models;
        for (int m = 0; m < 24; ++m) models.add (cz::macroModelName (m));
        addChoice (IDs::cModel, "C Model", models, 8);
    }
    addFloat  (IDs::cHarm,      "C Harmonics", { 0.0f, 1.0f }, 0.5f, pctText);
    addFloat  (IDs::cTimbre,    "C Timbre", { 0.0f, 1.0f }, 0.5f, pctText);
    addFloat  (IDs::cMorph,     "C Morph", { 0.0f, 1.0f }, 0.5f, pctText);
    addFloat  (IDs::cFm,        "C FM", { -1.0f, 1.0f }, 0.0f, bipText);
    addFloat  (IDs::cTimbreMod, "C Timbre Mod", { -1.0f, 1.0f }, 0.0f, bipText);
    addFloat  (IDs::cMorphMod,  "C Morph Mod", { -1.0f, 1.0f }, 0.0f, bipText);
    addFloat  (IDs::cDecay,     "C LPG Decay", { 0.0f, 1.0f }, 0.5f, pctText);
    addFloat  (IDs::cColour,    "C LPG Colour", { 0.0f, 1.0f }, 0.5f, pctText);
    addBool   (IDs::cLpg,       "C LPG On", true);
    addInt    (IDs::cPitch,     "C Pitch", -24, 24, 0);
    addChoice (IDs::cOut,       "C Output", { "Main", "Aux", "Stereo" }, 0);
    addFloat  (IDs::cMix,       "A/B - C Mix", { 0.0f, 1.0f }, 0.0f, pctText);
    addFloat  (IDs::cFeed,      "C to A Feed", { 0.0f, 1.0f }, 0.0f, pctText);
    addFloat  (IDs::cFeedB,     "C to B Feed", { 0.0f, 1.0f }, 0.0f, pctText);
    addChoice (IDs::seq2Target, "Seq 2 Plays", { "Engine B", "Engine C", "B + C" }, 0);
    addChoice (IDs::seq2Follow, "Seq 2 Follow", { "Off", "Unison", "+3rd (m)", "+3rd (M)", "+4th", "+5th", "+Octave", "-Octave" }, 0);
    addFloat  (IDs::seqMutate, "Seq Mutate", { 0.0f, 1.0f }, 0.0f, pctText);
    addFloat  (IDs::morph,     "Morph A-B", { 0.0f, 1.0f }, 0.0f, pctText);
    for (int i = 0; i < 4; ++i)
        addFloat (IDs::macro[i], String ("Macro ") + URingAudioProcessor::macroName (i), { -1.0f, 1.0f }, 0.0f, bipText);

    // ---- LFOs
    for (int i = 0; i < kNumLfos; ++i)
    {
        const String n = "LFO " + String (i + 1) + " ";
        addChoice (IDs::lfo (i, "Shape"), n + "Shape", lfoShapes, i == 0 ? 0 : 4);
        addFloat  (IDs::lfo (i, "Rate"),  n + "Rate", skewed (0.02f, 40.0f, 2.0f), i == 0 ? 0.3f : 4.0f, hzText);
        addBool   (IDs::lfo (i, "Sync"),  n + "Sync", false);
        addChoice (IDs::lfo (i, "Div"),   n + "Division", lfoDivs, 4);
        addChoice (IDs::lfo (i, "Dest"),  n + "Destination", lfoDests, DOff); // por defecto se patchea con cables
        addFloat  (IDs::lfo (i, "Depth"), n + "Depth", { -1.0f, 1.0f }, i == 0 ? 0.2f : 0.3f, bipText);
    }

    // ---- FX: glitch
    addBool   (IDs::glitchOn,    "Glitch On", false);
    addFloat  (IDs::glitchProb,  "Glitch Prob", { 0.0f, 1.0f }, 0.25f, pctText);
    addChoice (IDs::glitchLen,   "Glitch Length", glitchLens, 1);
    addFloat  (IDs::glitchVar,   "Glitch Variation", { 0.0f, 1.0f }, 0.4f, pctText);
    addFloat  (IDs::glitchCrush, "Glitch Crush", { 0.0f, 1.0f }, 0.0f, pctText);
    addFloat  (IDs::glitchMix,   "Glitch Mix", { 0.0f, 1.0f }, 1.0f, pctText);
    // ---- FX: tape delay
    addBool   (IDs::delayOn,    "Delay On", true);
    addBool   (IDs::delaySync,  "Delay Sync", true);
    addChoice (IDs::delayDiv,   "Delay Division", delayDivs, 3);
    addFloat  (IDs::delayTime,  "Delay Time", skewed (10.0f, 2000.0f, 350.0f), 375.0f, msText);
    addFloat  (IDs::delayFb,    "Delay Feedback", { 0.0f, 1.05f }, 0.35f, pctText);
    addFloat  (IDs::delayWow,   "Delay Wow", { 0.0f, 1.0f }, 0.2f, pctText);
    addFloat  (IDs::delayDrive, "Delay Drive", { 0.0f, 1.0f }, 0.2f, pctText);
    addFloat  (IDs::delayHp,    "Delay HP", skewed (20.0f, 2000.0f, 200.0f), 150.0f, hzText);
    addFloat  (IDs::delayLp,    "Delay LP", skewed (500.0f, 18000.0f, 4000.0f), 5000.0f, hzText);
    addBool   (IDs::delayPing,  "Delay Ping-Pong", true);
    addFloat  (IDs::delayMix,   "Delay Mix", { 0.0f, 1.0f }, 0.2f, pctText);
    addFloat  (IDs::delayDuck,  "Delay Duck", { 0.0f, 1.0f }, 0.0f, pctText);
    // ---- FX: spring reverb
    addBool   (IDs::springOn,      "Spring On", true);
    addFloat  (IDs::springTension, "Spring Tension", { 0.0f, 1.0f }, 0.5f, pctText);
    addFloat  (IDs::springDecay,   "Spring Decay", skewed (0.2f, 12.0f, 2.0f), 2.0f,
               [] (float v, int) { return String (v, v < 10.0f ? 2 : 1) + " s"; });
    addFloat  (IDs::springDamp,    "Spring Damp", { 0.0f, 1.0f }, 0.4f, pctText);
    addFloat  (IDs::springPre,     "Spring Pre-Delay", skewed (0.0f, 250.0f, 40.0f), 10.0f, msText);
    addFloat  (IDs::springTone,    "Spring Tone", { 0.0f, 1.0f }, 0.5f, pctText);
    addFloat  (IDs::springDrip,    "Spring Drip", { 0.0f, 1.0f }, 0.5f, pctText);
    addFloat  (IDs::springMix,     "Spring Mix", { 0.0f, 1.0f }, 0.18f, pctText);

    // ---- Secuenciador euclidiano
    auto stepsText = [] (int v, int) { return String (v); };
    addBool   (IDs::seqOn,     "Seq Run", false);   // arranca apagado: se prende con RUN
    addBool   (IDs::midiOut,   "Seq MIDI Out", true);
    addChoice (IDs::seqDiv,    "Seq Division", seqDivs, 1);
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { IDs::seqLength, 1 }, "Seq Steps", 1, cz::SeqPattern::kSteps, 16,
                    AudioParameterIntAttributes().withStringFromValueFunction (stepsText)));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { IDs::seqPulses, 1 }, "Seq Pulses", 0, cz::SeqPattern::kSteps, 5,
                    AudioParameterIntAttributes().withStringFromValueFunction (stepsText)));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { IDs::seqRotate, 1 }, "Seq Rotate", 0, cz::SeqPattern::kSteps - 1, 0,
                    AudioParameterIntAttributes().withStringFromValueFunction (stepsText)));
    addChoice (IDs::seqRoot,   "Seq Root", noteNames, 0);
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { IDs::seqOctave, 1 }, "Seq Octave", 1, 6, 3,
                    AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return "Oct " + String (v); })));
    StringArray scaleNames;
    for (const auto& sc : mm::scales()) scaleNames.add (sc.name);
    addChoice (IDs::seqScale,  "Seq Scale", scaleNames, 7);
    addFloat  (IDs::seqRange,  "Seq Range", { 0.0f, 1.0f }, 0.4f, pctText);
    addFloat  (IDs::seqGate,   "Seq Gate", { 0.05f, 0.98f }, 0.4f, pctText);
    addFloat  (IDs::seqSwing,  "Seq Swing", { 0.5f, 0.75f }, 0.54f, pctText);
    addFloat  (IDs::seqProb,   "Seq Probability", { 0.0f, 1.0f }, 1.0f, pctText);
    addFloat  (IDs::seqRandom, "Seq Random", { 0.0f, 1.0f }, 0.0f, pctText);
    addChoice (IDs::seqDir,    "Seq Direction", StringArray { "Forward", "Backward", "Ping-Pong", "Random" }, 0);
    addFloat  (IDs::seqDensity, "Seq Density", { 0.0f, 1.0f }, 0.45f, pctText);
    addBool   (IDs::seqFill,    "Seq Fill", false);
    addBool   (IDs::seqChainOn, "Pattern Chain", false);
    addBool   (IDs::freeze,     "Freeze", false);
    addChoice (IDs::seqRndRate, "Random Rate", rndRates, 0);
    addFloat  (IDs::seqRndTime, "Random Time", { 0.0f, 1.0f }, 0.0f, pctText);
    addFloat  (IDs::seqLongShort, "Long / Short", { 0.0f, 1.0f }, 0.0f, pctText);
    static const char* laneNames[] { "Note", "Octave", "Velocity", "Gate", "Prob", "Ratchet", "Condition", "Lock", "C Model" };
    for (int l = 0; l < cz::kNumValueLanes; ++l)
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { IDs::seqLaneLen[l], 1 },
                        String ("Seq ") + laneNames[l] + " Length", 0, cz::SeqPattern::kSteps, 0, // 0 = AUTO (sigue a STEPS)
                        AudioParameterIntAttributes().withStringFromValueFunction (
                            [] (int v, int) { return v == 0 ? String ("Auto") : String (v); })));
    addFloat  (IDs::tempo,     "Internal Tempo", { 60.0f, 200.0f, 0.1f }, 120.0f,
               [] (float v, int) { return String (v, 1) + " BPM"; });
    addBool   (IDs::autoRec,   "Automation Rec", false);
    addChoice (IDs::autoLen,   "Automation Length", { "Seq Cycle", "1 Bar", "2 Bars", "4 Bars", "8 Bars" }, 0);
    addBool   (IDs::seq2On,    "Seq 2 On", false);
    addChoice (IDs::seq2Div,   "Seq 2 Division", seqDivs, 3);
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { IDs::seq2Oct, 1 }, "Seq 2 Octave", -2, 2, 0));

    // ---- 1.4
    addChoice (IDs::quality,    "Quality", { "Eco", "High (2x)" }, 1);
    addBool   (IDs::seqMarbles, "Seq Marbles", false);
    addFloat  (IDs::seqDejaVu,  "Seq Deja Vu", { 0.0f, 1.0f }, 0.4f, pctText);
    addInt    (IDs::seqMarbLen, "Seq Marbles Length", 1, 16, 8);
    addFloat  (IDs::seqSpread,  "Seq Spread", { 0.0f, 1.0f }, 0.5f, pctText);
    addFloat  (IDs::seqBias,    "Seq Bias", { 0.0f, 1.0f }, 0.5f, pctText);

    addBool   (IDs::texOn,       "Texture On", false);
    addChoice (IDs::texMode,     "Texture Mode", { "Granular", "Stretch", "Looping Delay", "Spectral" }, 0);
    addFloat  (IDs::texPosition, "Texture Position", { 0.0f, 1.0f }, 0.5f, pctText);
    addFloat  (IDs::texSize,     "Texture Size", { 0.0f, 1.0f }, 0.5f, pctText);
    addFloat  (IDs::texPitch,    "Texture Pitch", { -24.0f, 24.0f, 0.01f }, 0.0f,
               [] (float v, int) { return String (v, 1) + " st"; });
    addFloat  (IDs::texDensity,  "Texture Density", { 0.0f, 1.0f }, 0.65f, pctText);
    addFloat  (IDs::texTexture,  "Texture Texture", { 0.0f, 1.0f }, 0.5f, pctText);
    addFloat  (IDs::texSpread,   "Texture Spread", { 0.0f, 1.0f }, 0.5f, pctText);
    addFloat  (IDs::texFeedback, "Texture Feedback", { 0.0f, 1.0f }, 0.2f, pctText);
    addFloat  (IDs::texReverb,   "Texture Reverb", { 0.0f, 1.0f }, 0.3f, pctText);
    addBool   (IDs::texFreeze,   "Texture Freeze", false);
    addFloat  (IDs::texMix,      "Texture Mix", { 0.0f, 1.0f }, 0.5f, pctText);

    addBool   (IDs::plateOn,    "Plate On", false);
    addFloat  (IDs::plateSize,  "Plate Size", { 0.0f, 1.0f }, 0.6f, pctText);
    addFloat  (IDs::plateDecay, "Plate Decay", { 0.0f, 1.0f }, 0.6f, pctText);
    addFloat  (IDs::plateDamp,  "Plate Damp", { 0.0f, 1.0f }, 0.4f, pctText);
    addFloat  (IDs::platePre,   "Plate Pre-Delay", skewed (0.0f, 200.0f, 30.0f), 15.0f, msText);
    addFloat  (IDs::plateMod,   "Plate Modulation", { 0.0f, 1.0f }, 0.3f, pctText);
    addFloat  (IDs::plateMix,   "Plate Mix", { 0.0f, 1.0f }, 0.25f, pctText);

    auto dbGain = [] (float v, int) { return (v > 0.0f ? "+" : "") + String (v, 1) + " dB"; };
    addFloat  (IDs::eqLow,       "EQ Low", { -12.0f, 12.0f, 0.1f }, 0.0f, dbGain);
    addFloat  (IDs::eqMid,       "EQ Mid", { -12.0f, 12.0f, 0.1f }, 0.0f, dbGain);
    addFloat  (IDs::eqMidHz,     "EQ Mid Freq", skewed (150.0f, 6000.0f, 1000.0f), 1000.0f, hzText);
    addFloat  (IDs::eqHigh,      "EQ High", { -12.0f, 12.0f, 0.1f }, 0.0f, dbGain);
    addFloat  (IDs::masterDrive, "Master Drive", { 0.0f, 1.0f }, 0.0f, pctText);
    addBool   (IDs::limiterOn,   "Limiter", true);

    return layout;
}

//==============================================================================
URingAudioProcessor::URingAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    for (int i = 0; i < kNumVoices; ++i)
        synth.addVoice (new cz::CzVoice (voiceParams));
    synth.addSound (new cz::CzSound (3)); // teclado / MIDI / SEQ 1 solo: los dos motores
    synth.addSound (new cz::CzSound (1)); // SEQ 1 con SEQ 2 encendido: motor A
    synth.addSound (new cz::CzSound (2)); // SEQ 2: motor B

    // Cada memoria de patrón arranca con su propia melodía
    for (int i = 0; i < kNumPatterns; ++i)
    {
        juce::Random seeded (777 + i * 31);
        patterns[(size_t) i].generate (0.4f, mm::scales()[7], seeded);
        juce::Random seeded2 (4242 + i * 17);   // SEQ 2: otra melodía y otro dibujo rítmico
        patterns2[(size_t) i].generate (0.4f, mm::scales()[7], seeded2);
        patterns2[(size_t) i].setMeta ({ 12, 5, 2, 0 });
    }

    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rangedParams.add (rp);
    for (int i = 0; i < 128; ++i) { ccMap[(size_t) i] = -1; ccValue[(size_t) i] = 0.0f; ccDirty[(size_t) i] = false; }
    addPatch (SrcLfo1, DAPosition, 0.15f);
    startTimerHz (60);

    static const char* names[] { "Shape", "Rate", "Sync", "Div", "Dest", "Depth" };
    for (int i = 0; i < kNumLfos; ++i)
        for (int k = 0; k < NumLfoParams; ++k)
            lfoParams[(size_t) i][(size_t) k] = apvts.getRawParameterValue (IDs::lfo (i, names[k]));

    // Tabla de parámetros para la automatización grabada
    numParams = rangedParams.size();
    for (int i = 0; i < numParams; ++i)
    {
        auto* rp = rangedParams.getUnchecked (i);
        const auto id = rp->getParameterID();
        raw.push_back (apvts.getRawParameterValue (id));
        rawIndex[raw.back()] = i;
        bool excluded = id == IDs::autoRec || id == IDs::autoLen || id == IDs::seqOn || id == IDs::midiOut || id == IDs::seqChainOn
                     || id == IDs::tempo || id == IDs::seqLength || id == IDs::seqPulses || id == IDs::seqRotate || id == IDs::seqDir;
        for (auto* l : IDs::seqLaneLen) excluded = excluded || id == l;
        autoable.push_back (excluded ? 0 : 1);
        isFloat.push_back (dynamic_cast<juce::AudioParameterFloat*> (rp) != nullptr ? 1 : 0);
        rp->addListener (this);
    }
    // Macros: cada una mueve varios parámetros a la vez (cantidad en unidades normalizadas a fondo)
    const std::vector<std::vector<std::pair<const char*, float>>> macroDefs {
        { { IDs::aBright, 0.35f }, { IDs::aDamping, -0.3f }, { IDs::aStructure, 0.2f }, { IDs::bMaterial, 0.45f }, { IDs::bTone, 0.25f },
          { IDs::cTimbre, 0.3f } },                                                                                                       // MATERIAL: madera ↔ metal
        { { IDs::springMix, 0.4f }, { IDs::delayMix, 0.35f }, { IDs::body, 0.4f }, { IDs::width, 0.3f }, { IDs::sympathy, 0.3f } },      // SPACE: seco ↔ espacio
        { { IDs::variation, 0.4f }, { IDs::seqRandom, 0.35f }, { IDs::life, 0.5f }, { IDs::chorusMix, 0.3f }, { IDs::seqMutate, 0.25f },
          { IDs::cMorph, 0.3f } },                                                                                                        // MOTION: quieto ↔ vivo
        { { IDs::cutoff, 0.35f }, { IDs::resonance, 0.2f }, { IDs::drive, 0.3f }, { IDs::aBright, 0.15f }, { IDs::bTone, 0.15f },
          { IDs::cColour, 0.35f } } };                                                                                                    // TONE: oscuro ↔ brillante y sucio
    for (int k = 0; k < 4; ++k)
    {
        macroIdx[(size_t) k] = paramIndex (IDs::macro[k]);
        for (const auto& [id, amt] : macroDefs[(size_t) k])
            if (const int pi = paramIndex (id); pi >= 0) macroTargets[(size_t) k].push_back ({ pi, amt });
    }
    macroOff.reset (new std::atomic<float>[(size_t) numParams]);
    for (int i = 0; i < numParams; ++i) macroOff[(size_t) i] = 0.0f;
    // Morph: solo el sonido (no el secuenciador, ni el volumen, ni las macros)
    for (int i = 0; i < numParams; ++i)
    {
        const auto id = rangedParams.getUnchecked (i)->getParameterID();
        const bool skip = id.startsWith ("seq") || id.startsWith ("auto") || id.startsWith ("macro") || id == IDs::morph
                       || id == IDs::volume || id == IDs::midiOut || id == IDs::tempo || id == IDs::freeze || id == IDs::pedal;
        morphable.push_back (skip ? 0 : 1);
    }

    autoRecIdx = paramIndex (IDs::autoRec);
    autoLenIdx = paramIndex (IDs::autoLen);
    lanes.reset (new AutoLane[(size_t) (kNumPatterns * numParams)]);
    overrideVal.reset (new std::atomic<float>[(size_t) numParams]);
    overrideOn.reset (new std::atomic<bool>[(size_t) numParams]);
    gesture.reset (new std::atomic<bool>[(size_t) numParams]);
    touchedUntil.reset (new std::atomic<juce::int64>[(size_t) numParams]);
    for (int i = 0; i < numParams; ++i) { overrideVal[(size_t) i] = 0.0f; overrideOn[(size_t) i] = false; gesture[(size_t) i] = false; touchedUntil[(size_t) i] = 0; }

    // Al abrir URing suena "Init Ring" con su nivel propio (si el DAW trae un proyecto, lo reemplaza)
    loadPreset (0);
}

URingAudioProcessor::~URingAudioProcessor()
{
    for (auto* rp : rangedParams) rp->removeListener (this);
}

//==============================================================================
// Automatización grabada
void URingAudioProcessor::parameterValueChanged (int i, float)
{
    if (suppressTouch.load() > 0 || i < 0 || i >= numParams) return;
    if (i != autoRecIdx) presetDirty = true;
    touchedUntil[(size_t) i] = sampleClock.load() + (juce::int64) (0.25 * sampleRateHz);
}

void URingAudioProcessor::parameterGestureChanged (int i, bool starting)
{
    if (suppressTouch.load() > 0 || i < 0 || i >= numParams) return;
    gesture[(size_t) i] = starting;
    if (! starting) touchedUntil[(size_t) i] = sampleClock.load() + (juce::int64) (0.1 * sampleRateHz);
}

// unitPpq: duración de un paso del loop (un paso del SEQ 1, o una semicorchea en modo compases);
// loopLen: pasos del loop. REC queda armado hasta el paso 1, graba un ciclo y se apaga solo.
void URingAudioProcessor::updateAutomation (double ppq, bool running, juce::int64 clock, double unitPpq, int loopLen)
{
    const bool recParam = raw[(size_t) autoRecIdx]->load() > 0.5f;
    if (! recParam) recState = RecIdle;
    else if (recState.load() == RecIdle && ! recOffRequest.load()) recState = RecArmed;
    autoRunning = running;
    if (! running)
    {
        for (int i = 0; i < numParams; ++i) overrideOn[(size_t) i] = false;
        autoWasRunning = false;
        if (recState.load() == RecRecording) recState = RecArmed; // se paró a mitad: vuelve a esperar el paso 1
        return;
    }
    const int pat = activePattern.load();
    const int len = juce::jlimit (1, kAutoSteps, loopLen);
    const double pos = std::fmod (juce::jmax (0.0, ppq) / juce::jmax (1.0e-3, unitPpq), (double) len);
    // Paso 1 del loop: al arrancar desde el principio, al dar la vuelta, o si el DAW salta al inicio.
    // Un salto a mitad del loop (relocalizar) corta la grabación en curso y vuelve a esperar el paso 1.
    bool loopStart = false;
    if (! autoWasRunning) loopStart = pos < 1.0;
    else if (pos < autoLastPos - 0.5) loopStart = (autoLastPos >= len - 1.5 && pos < 1.5) || pos < 1.0;
    const bool relocated = autoWasRunning && ! loopStart && (pos < autoLastPos - 0.5 || pos > autoLastPos + 2.0);
    if (relocated && recState.load() == RecRecording) recState = RecArmed;
    autoWasRunning = true;
    autoLastPos = pos;
    if (loopStart)
    {
        if (recState.load() == RecRecording) { recState = RecIdle; recOffRequest = true; } // ciclo completo: fin
        else if (recState.load() == RecArmed) recState = RecRecording;
    }
    const int step = juce::jlimit (0, len - 1, (int) pos);
    const int next = (step + 1) % len;
    const float frac = (float) (pos - step);
    autoPos = (float) (pos / len);
    const bool rec = recState.load() == RecRecording;

    for (int i = 0; i < numParams; ++i)
    {
        if (! autoable[(size_t) i]) continue;
        auto& ln = lane (pat, i);
        const bool touched = gesture[(size_t) i].load() || touchedUntil[(size_t) i].load() > clock;
        if (touched) // mientras tocás una perilla se escucha la perilla (y se graba si REC)
        {
            overrideOn[(size_t) i] = false;
            if (rec)
            {
                ln.v[(size_t) step] = raw[(size_t) i]->load();
                ln.set (step);
                lastRecorded = i;
            }
            continue;
        }
        if (! ln.has (step)) { overrideOn[(size_t) i] = false; continue; }
        float v = ln.v[(size_t) step].load();
        if (isFloat[(size_t) i] && ln.has (next))
            v += (ln.v[(size_t) next].load() - v) * frac; // movimiento suave entre pasos
        overrideVal[(size_t) i] = v;
        overrideOn[(size_t) i] = true;
    }
}

bool URingAudioProcessor::hasAutomation (int i) const
{
    return i >= 0 && i < numParams && lane (activePattern.load(), i).any();
}

float URingAudioProcessor::getAutoDisplay01 (int i) const
{
    if (i < 0 || i >= numParams || ! overrideOn[(size_t) i].load()) return -1.0f;
    return rangedParams.getUnchecked (i)->convertTo0to1 (overrideVal[(size_t) i].load());
}

juce::Array<int> URingAudioProcessor::getAutomatedParams() const
{
    juce::Array<int> list;
    for (int i = 0; i < numParams; ++i)
        if (hasAutomation (i)) list.add (i);
    return list;
}

juce::String URingAudioProcessor::getParamDisplayName (int i) const
{
    return i >= 0 && i < numParams ? rangedParams.getUnchecked (i)->getName (40) : juce::String();
}

void URingAudioProcessor::clearAutomation (int i)
{
    const int pat = activePattern.load();
    for (int k = 0; k < numParams; ++k)
        if (i < 0 || k == i) lane (pat, k).clear();
}

void URingAudioProcessor::clearAllAutomation()
{
    for (int k = 0; k < kNumPatterns * numParams; ++k) lanes[(size_t) k].clear();
}

bool URingAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void URingAudioProcessor::prepareVoices()
{
    // QUALITY: las voces corren a 1x (ECO) o 2x (HIGH); el resto (secuenciador, LFO, efectos) a 1x
    preparedFactor = get (IDs::quality) > 0.5f ? 2 : 1;
    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampler->initProcessing ((size_t) juce::jmax (preparedBlock, 8192)); // algunos DAW mandan bloques más grandes que lo anunciado
    oversampler->reset();
    midiOs.ensureSize (8192);
    midiSub.ensureSize (8192);
    const double voiceRate = sampleRateHz * preparedFactor;
    synth.setCurrentPlaybackSampleRate (voiceRate);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<cz::CzVoice*> (synth.getVoice (i)))
            v->prepare (voiceRate);
    setLatencySamples (preparedFactor > 1 ? juce::roundToInt (oversampler->getLatencyInSamples()) : 0);
}

void URingAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampleRateHz = sampleRate;
    preparedBlock = juce::jmax (32, samplesPerBlock);
    prepareVoices();
    texture.prepare (sampleRate);
    plate.prepare (sampleRate);
    master.prepare (sampleRate);
    for (auto& l : lfos) l.reset();
    glitch.prepare (sampleRate);
    delay.prepare (sampleRate);
    spring.prepare (sampleRate);
    bodyRes.prepare (sampleRate);
    sympathetic.prepare (sampleRate);
    chorus.prepare (sampleRate);
    seqMidi2b.ensureSize (4096);
    followMap.fill (-1);
    keyboardState.reset();
    sequencer.prepare (sampleRate);
    sequencer2.prepare (sampleRate);
    reseedRandomness();
    autoWasRunning = false;
    seqMidi2.ensureSize (4096);
    internalPpq = 0.0;
    transportRunning = false;
    lastCycle = -1;
    seqMidi.ensureSize (4096);
}

int URingAudioProcessor::baseNote() const
{
    return 12 * ((int) get (IDs::seqOctave) + 1) + (int) get (IDs::seqRoot);
}

mm::Scale URingAudioProcessor::currentScale() const
{
    const auto& all = mm::scales();
    return all[(size_t) juce::jlimit (0, (int) all.size() - 1, (int) get (IDs::seqScale))];
}

void URingAudioProcessor::generatePattern (juce::Random& r)
{
    pattern().generate (get (IDs::seqRange), currentScale(), r);
}

void URingAudioProcessor::randomizeRhythm()
{
    juce::Random r;
    pattern().randomizeRhythm (r, get (IDs::seqDensity), (int) get (IDs::seqPulses), (int) get (IDs::seqLength), (int) get (IDs::seqRotate));
}
void URingAudioProcessor::randomizeLane (cz::Lane lane) { juce::Random r; pattern().randomizeLane (lane, get (IDs::seqRange), currentScale(), r); }
void URingAudioProcessor::randomizeAll()
{
    juce::Random r;
    pattern().randomizeAll (get (IDs::seqRange), currentScale(), get (IDs::seqDensity), (int) get (IDs::seqPulses),
                          (int) get (IDs::seqLength), (int) get (IDs::seqRotate), r);
}

juce::String URingAudioProcessor::surprise()
{
    juce::Random r;
    auto setValue = [this] (juce::StringRef id, float v)
    {
        auto* p = apvts.getParameter (id);
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (v));
        p->endChangeGesture();
    };

    const int steps = (int) get (IDs::seqLength);
    const float density = get (IDs::seqDensity);

    // Ritmo: la cantidad de golpes sigue a DENSITY, con algo de azar
    const int pulses = juce::jlimit (1, steps, juce::roundToInt ((float) steps * (0.12f + 0.6f * density) + r.nextFloat() * 4.0f - 2.0f));
    const int rotate = r.nextInt (juce::jmax (1, steps));
    setValue (IDs::seqPulses, (float) pulses);
    setValue (IDs::seqRotate, (float) rotate);

    // Dirección: casi siempre hacia adelante, a veces una sorpresa
    const int dir = r.nextFloat() < 0.7f ? 0 : 1 + r.nextInt (3);
    setValue (IDs::seqDir, (float) dir);

    // Swing ocasional
    setValue (IDs::seqSwing, r.nextFloat() < 0.5f ? 0.5f : 0.53f + 0.07f * r.nextFloat());

    // Polimetría ocasional en notas y velocidad
    const bool poly = r.nextFloat() < 0.3f;
    setValue (IDs::seqLaneLen[(int) cz::Lane::Note], poly ? (float) (3 + r.nextInt (juce::jmax (1, steps - 3))) : 0.0f);
    setValue (IDs::seqLaneLen[(int) cz::Lane::Velocity], poly && r.nextBool() ? (float) (3 + r.nextInt (5)) : 0.0f);

    pattern().generate (get (IDs::seqRange), currentScale(), r);
    pattern().randomizeLane (cz::Lane::Octave, get (IDs::seqRange), currentScale(), r);
    pattern().randomizeRhythm (r, density, pulses, steps, rotate);

    static const char* dirs[] { "FWD", "BACK", "PING", "RND" };
    return juce::String (pulses) + " HITS  ROT " + juce::String (rotate) + "  " + dirs[dir] + (poly ? "  POLY" : "");
}

void URingAudioProcessor::computeVoiceParams (const float* mod)
{
    auto& v = voiceParams;
    v.a.model      = (cz::ResModel) (int) get (IDs::aModel);
    v.a.exciter    = (cz::ResExciter) (int) get (IDs::aExciter);
    v.a.structure  = clamp01 (get (IDs::aStructure) + mod[DAStructure]);
    v.a.brightness = clamp01 (get (IDs::aBright) + mod[DABright]);
    v.a.damping    = clamp01 (get (IDs::aDamping) + mod[DADamping]);
    v.a.position   = clamp01 (get (IDs::aPosition) + mod[DAPosition]);

    v.b.model    = (cz::ImpModel) (int) get (IDs::bModel);
    v.b.exciter  = (cz::ImpExciter) (int) get (IDs::bExciter);
    v.b.material = clamp01 (get (IDs::bMaterial) + mod[DBMaterial]);
    v.b.decay    = clamp01 (get (IDs::bDecay) + mod[DBDecay]);
    v.b.tone     = clamp01 (get (IDs::bTone) + mod[DBTone]);
    v.b.position = clamp01 (get (IDs::bPosition));
    v.b.spread   = clamp01 (get (IDs::bSpread) + mod[DBSpread]);
    v.b.sweep    = clamp01 (get (IDs::bSweep));

    v.aPitch   = get (IDs::aPitch);
    v.bPitch   = get (IDs::bPitch);
    v.tune     = get (IDs::tune) * 0.01f;
    v.pitchMod = mod[DPitch] * 12.0f;
    v.abMix    = clamp01 (get (IDs::abMix) + mod[DABMix]);
    v.xfeed    = clamp01 (get (IDs::xfeed) + mod[DXFeed]);
    v.variation = get (IDs::variation);
    v.width     = get (IDs::width);

    v.filterType    = (cz::FilterType) (int) get (IDs::fltType);
    v.cutoff        = juce::jlimit (20.0f, 20000.0f, get (IDs::cutoff) * std::exp2 (mod[DCutoff] * 4.0f));
    v.resonance     = clamp01 (get (IDs::resonance) + mod[DResonance]);
    v.fltEnvAmt     = get (IDs::fltEnv);
    v.fltEnvDecayMs = get (IDs::fltDecay);
    v.keyTrack      = get (IDs::keyTrack);
    v.drive         = get (IDs::drive);
    v.attack  = get (IDs::attack);
    v.decay   = get (IDs::decay);
    v.sustain = get (IDs::sustain);
    v.release = get (IDs::release);
    v.freeze  = get (IDs::freeze) > 0.5f;
    v.velSens = get (IDs::velSens);
    v.life    = get (IDs::life);
    v.lockDest = (int) get (IDs::lockDest);

    v.c.model     = juce::jlimit (0, 23, (int) get (IDs::cModel));
    v.c.harmonics = clamp01 (get (IDs::cHarm) + mod[DCHarmonics]);
    v.c.timbre    = clamp01 (get (IDs::cTimbre) + mod[DCTimbre]);
    v.c.morph     = clamp01 (get (IDs::cMorph) + mod[DCMorph]);
    v.c.fm        = get (IDs::cFm);
    v.c.timbreMod = get (IDs::cTimbreMod);
    v.c.morphMod  = get (IDs::cMorphMod);
    v.c.decay     = get (IDs::cDecay);
    v.c.colour    = get (IDs::cColour);
    v.c.lpg       = get (IDs::cLpg) > 0.5f;
    v.c.outMode   = (int) get (IDs::cOut);
    v.cPitch      = get (IDs::cPitch);
    v.cMix        = get (IDs::cMix);
    v.cFeed       = get (IDs::cFeed);
    v.cFeedB      = get (IDs::cFeedB);
    v.seq2Target  = (int) get (IDs::seq2Target);
}

void URingAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (! m.isController())
            continue;

        const int cc = m.getControllerNumber();
        if (cc == 64) hostPedal = m.getControllerValue() >= 64;
        if (cc == 1)
            modWheel = (float) m.getControllerValue() / 127.0f;
        if (cc >= 120)
            continue; // mensajes de modo de canal

        if (const int learning = learnParam.load(); learning >= 0)
        {
            for (auto& slot : ccMap)
                if (slot.load() == learning) slot = -1; // un parámetro, un CC
            ccMap[(size_t) cc] = learning;
            learnedCc = cc;
            learnedParam = learning;
            learnParam = -1;
        }
        else if (ccMap[(size_t) cc].load() >= 0)
        {
            ccValue[(size_t) cc] = (float) m.getControllerValue() / 127.0f;
            ccDirty[(size_t) cc] = true;
        }
    }

    // ---- Transporte: el del host, o reloj interno si no hay host (Standalone)
    const bool seqOn = get (IDs::seqOn) > 0.5f;
    double bpm = 120.0, ppq = 0.0;
    bool hostTransport = false, playing = false;
    if (auto* ph = getPlayHead())
        if (const auto pos = ph->getPosition())
        {
            if (const auto b = pos->getBpm()) bpm = *b;
            if (const auto p = pos->getPpqPosition())
            {
                hostTransport = true;
                ppq = *p;
                playing = pos->getIsPlaying();
            }
        }
    if (! hostTransport)
    {
        bpm = get (IDs::tempo);
        playing = seqOn;
        if (! playing) internalPpq = 0.0;
        ppq = internalPpq;
        if (playing) internalPpq += numSamples * bpm / (60.0 * sampleRateHz);
    }
    if (playing && ! lastTransportPlaying) reseedRandomness();   // Play en el DAW: mismo azar que la vez anterior
    lastTransportPlaying = playing;

    // Cambio de secuenciador en edición: se hace acá para que las perillas STEPS/PULSES… no escriban
    // en el patrón equivocado durante el cambio
    if (const int pe = pendingEdit.exchange (-1); pe >= 0) { metaLoadRequest = true; editSeq = pe; }
    lastBlockMs = juce::Time::getMillisecondCounter();

    // ---- Memorias de patrón: el cambio (o el paso de la cadena) ocurre al empezar cada ciclo
    const double stepPpq = seqDivPpq[juce::jlimit (0, 5, (int) get (IDs::seqDiv))];
    const bool running = seqOn && playing;
    transportRunning = running;
    if (running)
    {
        const auto meta = patterns[(size_t) activePattern.load()].getMeta();
        const double cyclePpq = juce::jmax (1, meta.length) * stepPpq;
        const auto cycle = (juce::int64) std::floor (ppq / cyclePpq + 1.0e-9);
        if (cycle != lastCycle)
        {
            // MUTATE: al empezar cada vuelta, con esa probabilidad cambia una nota (de la escala) del patrón
            if (lastCycle >= 0)
                if (const float mu = get (IDs::seqMutate); mu > 0.0f)
                    for (int sq = 0; sq < 2; ++sq)
                        if ((sq == 0 || get (IDs::seq2On) > 0.5f) && mutateRng.nextFloat() < mu)
                        {
                            auto& pt = (sq == 0 ? patterns : patterns2)[(size_t) activePattern.load()];
                            const auto sc = currentScale();
                            const int st = mutateRng.nextInt (juce::jmax (1, pt.getMeta().length));
                            auto d = pt.get (st);
                            const int deg = mm::semisToNearestDegree (sc, d.semis) + (mutateRng.nextBool() ? 1 : -1) * (mutateRng.nextFloat() < 0.3f ? 2 : 1);
                            d.semis = juce::jlimit (cz::SeqPattern::kMinNote, cz::SeqPattern::kMaxNote, mm::degreeToSemis (sc, deg));
                            if (mutateRng.nextFloat() < 0.15f) d.octave = juce::jlimit (-1, 1, d.octave + (mutateRng.nextBool() ? 1 : -1));
                            pt.set (st, d);
                        }
            lastCycle = cycle;
            int target = -1;
            if (get (IDs::seqChainOn) > 0.5f && chainLen.load() > 0)
            {
                const int pos = (chainPos.load() + 1) % chainLen.load();
                chainPos = pos;
                target = chainSteps[(size_t) pos].load();
            }
            else
            {
                target = queuedPattern.exchange (-1);
            }
            if (target >= 0 && target < kNumPatterns && target != activePattern.load())
            {
                activePattern = target;
                metaLoadRequest = true; // el timer pasa sus ajustes a las perillas
            }
        }
    }
    else
    {
        lastCycle = -1;
        chainPos = -1;
    }

    // Las perillas STEPS/PULSES/ROTATE/DIRECTION editan el patrón activo
    if (! metaLoadRequest.load() && editSeq.load() == 0)
        patterns[(size_t) activePattern.load()].setMeta ({ (int) get (IDs::seqLength), (int) get (IDs::seqPulses),
                             (int) get (IDs::seqRotate), (int) get (IDs::seqDir) });
    const auto meta = patterns[(size_t) activePattern.load()].getMeta();

    // ---- Automatización grabada: se lee (o graba) una vez por bloque. El loop dura un ciclo del
    //      SEQ 1 (SEQ CYCLE) o 1-8 compases.
    {
        const int lenMode = (int) raw[(size_t) autoLenIdx]->load();
        if (lenMode == 0) updateAutomation (ppq, playing, sampleClock.load(), stepPpq, juce::jmax (1, meta.length));
        else              updateAutomation (ppq, playing, sampleClock.load(), 0.25, 16 << juce::jlimit (0, 3, lenMode - 1));
        sampleClock += numSamples;
    }

    // ---- Secuenciador euclidiano → notas MIDI para el sinte
    cz::SeqSettings ss;
    ss.stepPpq    = stepPpq;
    ss.length     = meta.length;
    ss.pulses     = meta.pulses;
    ss.rotate     = meta.rotate;
    ss.fill       = get (IDs::seqFill) > 0.5f;
    ss.baseNote   = baseNote();
    ss.scale      = currentScale();
    ss.gate       = get (IDs::seqGate);
    ss.swing      = get (IDs::seqSwing);
    ss.globalProb = get (IDs::seqProb);
    ss.randomAmt  = get (IDs::seqRandom);
    ss.randomRatePpq = rndRatePpq[juce::jlimit (0, 5, (int) get (IDs::seqRndRate))];
    ss.randomTime = get (IDs::seqRndTime);
    ss.longShort  = get (IDs::seqLongShort);
    ss.direction  = (cz::Direction) meta.direction;
    ss.marbles    = get (IDs::seqMarbles) > 0.5f;
    ss.dejaVu     = get (IDs::seqDejaVu);
    ss.marblesLength = (int) get (IDs::seqMarbLen);
    ss.spread     = get (IDs::seqSpread);
    ss.bias       = get (IDs::seqBias);
    ss.range      = get (IDs::seqRange);
    for (int l = 0; l < cz::kNumValueLanes; ++l)
        ss.laneLength[(size_t) l] = (int) get (IDs::seqLaneLen[l]);
    // Con SEQ 2 encendido cada secuenciador toca su motor: SEQ 1 → A (canal 15), SEQ 2 → B (canal 16).
    // Apagado, SEQ 1 toca los dos motores como siempre (canal 1).
    const bool split = get (IDs::seq2On) > 0.5f;
    seqMidi.clear();
    if (split != lastSplit) // el SEQ 1 cambia de canal: primero suelta lo que suena en el canal viejo
    {
        sequencer.allNotesOff (seqMidi, 0);
        lastSplit = split;
    }
    sequencer.channel = split ? 15 : 14;
    sequencer.lockTable = voiceParams.noteLock[0].data();
    sequencer2.lockTable = voiceParams.noteLock[1].data();
    sequencer.modelTable = voiceParams.noteModel[0].data();
    sequencer2.modelTable = voiceParams.noteModel[1].data();
    sequencer.process (running, ppq, bpm, numSamples, patterns[(size_t) activePattern.load()], ss, seqMidi);
    midi.addEvents (seqMidi, 0, numSamples, 0); // el sinte toca las notas del secuenciador

    seqMidi2.clear();
    {
        auto& p2 = patterns2[(size_t) activePattern.load()];
        if (editSeq.load() == 1 && ! metaLoadRequest.load())
            p2.setMeta ({ (int) get (IDs::seqLength), (int) get (IDs::seqPulses), (int) get (IDs::seqRotate), (int) get (IDs::seqDir) });
        const auto m2 = p2.getMeta();
        cz::SeqSettings s2 = ss;
        s2.stepPpq   = seqDivPpq[juce::jlimit (0, 5, (int) get (IDs::seq2Div))];
        s2.length    = m2.length; s2.pulses = m2.pulses; s2.rotate = m2.rotate;
        s2.direction = (cz::Direction) m2.direction;
        s2.baseNote  = baseNote() + 12 * (int) get (IDs::seq2Oct);
        for (auto& l : s2.laneLength) l = 0;
        sequencer2.channel = 16;
        sequencer2.process (running && split, ppq, bpm, numSamples, p2, s2, seqMidi2);

        // FOLLOW: cada nota del SEQ 2 pasa a ser la última nota del SEQ 1 + el intervalo elegido
        static const int intervals[] { 0, 0, 3, 4, 5, 7, 12, -12 };
        const int follow = juce::jlimit (0, 7, (int) get (IDs::seq2Follow));
        if (follow == 0 && lastFollow != 0) // se apagó FOLLOW: suelta las notas transpuestas que quedaron sonando
            for (auto& mapped : followMap)
                if (mapped >= 0) { seqMidi2.addEvent (juce::MidiMessage::noteOff (16, mapped), 0); mapped = -1; }
        lastFollow = follow;
        if (follow > 0 && ! seqMidi2.isEmpty())
        {
            seqMidi2b.clear();
            for (const auto ev : seqMidi2)
            {
                const auto m = ev.getMessage();
                for (const auto e1 : seqMidi) // última nota del SEQ 1 hasta este instante
                    if (e1.samplePosition <= ev.samplePosition && e1.getMessage().isNoteOn()) lastSeq1Note = e1.getMessage().getNoteNumber();
                if (m.isNoteOn() && lastSeq1Note >= 0)
                {
                    const int nn = juce::jlimit (0, 127, lastSeq1Note + intervals[follow] + 12 * (int) get (IDs::seq2Oct));
                    followMap[(size_t) m.getNoteNumber()] = nn;
                    voiceParams.noteLock[1][(size_t) nn] = voiceParams.noteLock[1][(size_t) m.getNoteNumber()];
                    voiceParams.noteModel[1][(size_t) nn] = voiceParams.noteModel[1][(size_t) m.getNoteNumber()];
                    seqMidi2b.addEvent (juce::MidiMessage::noteOn (16, nn, m.getFloatVelocity()), ev.samplePosition);
                }
                else if (m.isNoteOff() && followMap[(size_t) m.getNoteNumber()] >= 0)
                {
                    seqMidi2b.addEvent (juce::MidiMessage::noteOff (16, followMap[(size_t) m.getNoteNumber()]), ev.samplePosition);
                    followMap[(size_t) m.getNoteNumber()] = -1;
                }
                else seqMidi2b.addEvent (m, ev.samplePosition);
            }
            seqMidi2.swapWith (seqMidi2b);
        }
        else
            for (const auto e1 : seqMidi) if (e1.getMessage().isNoteOn()) lastSeq1Note = e1.getMessage().getNoteNumber();
        midi.addEvents (seqMidi2, 0, numSamples, 0);
    }

    // ---- PEDAL (perilla o CC64 del teclado): sustain en todos los canales que usa el sinte
    {
        const bool pedalNow = get (IDs::pedal) > 0.5f;
        if (pedalNow != pedalWas)
        {
            for (int ch : { 1, 14, 15, 16 }) midi.addEvent (juce::MidiMessage::controllerEvent (ch, 64, pedalNow ? 127 : 0), 0);
            pedalWas = pedalNow;
        }
        sympathetic.setPedal (pedalNow || hostPedal);
    }
    updateMacros();

    // ---- QUALITY HIGH: las voces escriben en un búfer al doble de frecuencia (MIDI con tiempos x2)
    juce::AudioBuffer<float> osWrap;
    juce::AudioBuffer<float>* voiceBuf = nullptr;
    if (preparedFactor > 1 && oversampler != nullptr && buffer.getNumChannels() >= 2)
    {
        juce::dsp::AudioBlock<float> inBlock (buffer);
        auto up = oversampler->processSamplesUp (inBlock.getSubBlock (0, (size_t) numSamples));
        up.clear();
        float* chans[2] { up.getChannelPointer (0), up.getChannelPointer (1) };
        osWrap.setDataToReferTo (chans, 2, (int) up.getNumSamples());
        voiceBuf = &osWrap;
        midiOs.clear();
        for (const auto meta : midi)
            midiOs.addEvent (meta.getMessage(), meta.samplePosition * 2);
    }

    // ---- Síntesis por sub-bloques de 32 muestras con los LFO aplicados
    float mod[NumDests] {};
    float src[NumPatchSources] {};
    constexpr int kSub = 32;
    for (int pos = 0; pos < numSamples; pos += kSub)
    {
        const int n = std::min (kSub, numSamples - pos);
        std::fill (std::begin (mod), std::end (mod), 0.0f);

        for (int i = 0; i < kNumLfos; ++i)
        {
            const auto& lp = lfoParams[(size_t) i];
            const int dest = (int) value (lp[LDest]);
            const double hz = value (lp[LSync]) > 0.5f
                                  ? bpm / 60.0 / lfoDivBeats[juce::jlimit (0, 7, (int) value (lp[LDiv]))]
                                  : (double) value (lp[LRate]);
            const float v = lfos[(size_t) i].advance ((cz::LfoShape) (int) value (lp[LShape]), hz, sampleRateHz, n);
            src[i] = v;
            if (dest > DOff && dest < NumDests)
                mod[dest] += v * value (lp[LDepth]);
        }

        // ENV: envolvente global AD que se dispara con cada nota (ataque/decay del AMP ENV)
        for (const auto meta : midi)
            if (meta.samplePosition >= pos && meta.samplePosition < pos + n && meta.getMessage().isNoteOn())
                modEnvAttack = true;
        if (modEnvAttack)
        {
            modEnv += (1.05f - modEnv) * (1.0f - std::exp (-(float) n / (std::max (0.5f, get (IDs::attack)) * 0.001f * (float) sampleRateHz)));
            if (modEnv >= 1.0f) { modEnv = 1.0f; modEnvAttack = false; }
        }
        else
        {
            modEnv *= std::exp (-(float) n / (std::max (5.0f, get (IDs::decay) * 0.3f) * 0.001f * (float) sampleRateHz));
        }
        src[SrcEnv] = modEnv;
        for (int k = 0; k < NumPatchSources; ++k)
            sourceValue[(size_t) k] = src[k];

        // Cables de patch: fuente × cantidad → entrada
        for (const auto& slot : patchSlots)
        {
            const int sIdx = slot.source.load(), dIdx = slot.dest.load();
            if (sIdx >= 0 && sIdx < NumPatchSources && dIdx > DOff && dIdx < NumDests)
                mod[dIdx] += src[sIdx] * slot.amount.load();
        }

        mod[DAStructure] += modWheel * 0.5f;
        mod[DBSpread]    += modWheel * 0.6f;
        computeVoiceParams (mod);
        auto sliceMidi = [this] (const juce::MidiBuffer& src, int from, int len) -> const juce::MidiBuffer&
        {
            midiSub.clear();
            for (const auto meta : src)
                if (meta.samplePosition >= from && meta.samplePosition < from + len)
                    midiSub.addEvent (meta.data, meta.numBytes, meta.samplePosition);
            return midiSub;
        };
        if (voiceBuf != nullptr)
        {
            synth.renderNextBlock (*voiceBuf, sliceMidi (midiOs, pos * 2, n * 2), pos * 2, n * 2);
            if (mod[DVolume] != 0.0f) // trémolo
                voiceBuf->applyGain (pos * 2, n * 2, juce::jlimit (0.0f, 2.0f, 1.0f + mod[DVolume]));
        }
        else
        {
            synth.renderNextBlock (buffer, sliceMidi (midi, pos, n), pos, n);
            if (mod[DVolume] != 0.0f) // trémolo
                buffer.applyGain (pos, n, juce::jlimit (0.0f, 2.0f, 1.0f + mod[DVolume]));
        }
    }
    if (voiceBuf != nullptr) // HIGH: de 2x a 1x con el filtro de media banda
    {
        juce::dsp::AudioBlock<float> outBlock (buffer);
        auto outSub = outBlock.getSubBlock (0, (size_t) numSamples);
        oversampler->processSamplesDown (outSub);
    }

    // ---- FX: cuerpo → glitch → tape delay → spring (con la modulación del último sub-bloque)
    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (1);
    bodyRes.process (l, r, numSamples, get (IDs::body));
    sympathetic.process (l, r, numSamples, get (IDs::sympathy));
    chorus.process (l, r, numSamples, get (IDs::chorusMix), get (IDs::chorusRate), get (IDs::chorusDepth));

    if (get (IDs::glitchOn) > 0.5f)
    {
        cz::GlitchParams gp;
        gp.prob       = clamp01 (get (IDs::glitchProb) + mod[DGlitchProb]);
        gp.lengthFrac = glitchLenFrac[juce::jlimit (0, 3, (int) get (IDs::glitchLen))];
        gp.variation  = get (IDs::glitchVar);
        gp.crush      = get (IDs::glitchCrush);
        gp.mix        = get (IDs::glitchMix);
        glitch.process (l, r, numSamples, bpm, ppq, playing, gp);
    }

    if (get (IDs::delayOn) > 0.5f)
    {
        cz::DelayParams dp;
        const double syncedMs = 60000.0 / bpm * delayDivBeats[juce::jlimit (0, 6, (int) get (IDs::delayDiv))];
        dp.timeMs   = juce::jlimit (10.0f, 2400.0f, (get (IDs::delaySync) > 0.5f ? (float) syncedMs : get (IDs::delayTime))
                                                      * std::exp2 (mod[DDelayTime]));
        dp.feedback = get (IDs::delayFb);
        dp.wow      = get (IDs::delayWow);
        dp.drive    = get (IDs::delayDrive);
        dp.hpHz     = get (IDs::delayHp);
        dp.lpHz     = get (IDs::delayLp);
        dp.pingPong = get (IDs::delayPing) > 0.5f;
        dp.mix      = get (IDs::delayMix);
        dp.duck     = get (IDs::delayDuck);
        delay.process (l, r, numSamples, dp);
    }

    if (get (IDs::springOn) > 0.5f)
    {
        cz::SpringParams sp;
        sp.tension = get (IDs::springTension);
        sp.decaySec   = get (IDs::springDecay);
        sp.damp       = get (IDs::springDamp);
        sp.preDelayMs = get (IDs::springPre);
        sp.tone    = get (IDs::springTone);
        sp.drip    = get (IDs::springDrip);
        sp.mix     = clamp01 (get (IDs::springMix) + mod[DSpringMix]);
        spring.process (l, r, numSamples, sp);
    }

    // ---- TEXTURE: granular (Clouds) y plate reverb
    if (get (IDs::texOn) > 0.5f)
    {
        cz::TextureParams tp;
        tp.mode     = (int) get (IDs::texMode);
        tp.position = get (IDs::texPosition);
        tp.size     = get (IDs::texSize);
        tp.pitch    = get (IDs::texPitch);
        tp.density  = get (IDs::texDensity);
        tp.texture  = get (IDs::texTexture);
        tp.spread   = get (IDs::texSpread);
        tp.feedback = get (IDs::texFeedback);
        tp.reverb   = get (IDs::texReverb);
        tp.freeze   = get (IDs::texFreeze) > 0.5f;
        tp.mix      = get (IDs::texMix);
        texture.process (l, r, numSamples, tp);
    }
    if (get (IDs::plateOn) > 0.5f)
    {
        cz::PlateParams pp;
        pp.size  = get (IDs::plateSize);
        pp.decay = get (IDs::plateDecay);
        pp.damp  = get (IDs::plateDamp);
        pp.preDelayMs = get (IDs::platePre);
        pp.mod   = get (IDs::plateMod);
        pp.mix   = get (IDs::plateMix);
        plate.process (l, r, numSamples, pp);
    }

    // ---- Volumen → EQ → saturador → limitador de picos (nunca pasa de -0.3 dBFS)
    const float gain = juce::Decibels::decibelsToGain (get (IDs::volume) + presetTrimDb.load());
    buffer.applyGain (0, numSamples, gain);
    {
        cz::MasterParams mp;
        mp.low   = get (IDs::eqLow);
        mp.mid   = get (IDs::eqMid);
        mp.midHz = get (IDs::eqMidHz);
        mp.high  = get (IDs::eqHigh);
        mp.drive = get (IDs::masterDrive);
        mp.limiter = get (IDs::limiterOn) > 0.5f;
        master.process (l, r, numSamples, mp);
        limiterGr = master.getGainReduction();
        if (! mp.limiter)
        {
            // Sin limitador el sonido queda libre, pero nunca pasa de +3.5 dBFS: con perillas extremas
            // (drive, resonancia, feedback) podía llegar a +30 dB y golpear los parlantes. Por debajo
            // de +2 dBFS no cambia nada; entre +2 y +3.5 dBFS dobla suave.
            auto safety = [] (float x) noexcept
            {
                const float a = std::abs (x);
                if (a <= 1.25f) return x;
                const float y = 1.25f + 0.25f * std::tanh ((a - 1.25f) * 4.0f);
                return std::copysign (y, x);
            };
            for (int i = 0; i < numSamples; ++i) { l[i] = safety (l[i]); r[i] = safety (r[i]); }
        }
    }

    // ---- Copia para la pantalla (osciloscopio / espectro / vúmetro)
    {
        int w = scopeWrite.load (std::memory_order_relaxed);
        float pl = 0.0f, pr = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            scope[(size_t) w].store (0.5f * (l[i] + r[i]), std::memory_order_relaxed);
            w = (w + 1) & (kScopeSize - 1);
            pl = juce::jmax (pl, std::abs (l[i]));
            pr = juce::jmax (pr, std::abs (r[i]));
        }
        scopeWrite.store (w, std::memory_order_release);
        for (size_t k = 0; k < peaks.size(); ++k)
        {
            const float v = (k & 1) ? pr : pl;
            float old = peaks[k].load();
            while (v > old && ! peaks[k].compare_exchange_weak (old, v)) {}
        }
    }

    // ---- Salida MIDI: solo las notas del secuenciador (para grabarlas en otra pista del DAW)
    midi.clear();
    if (get (IDs::midiOut) > 0.5f)
    {
        midi.addEvents (seqMidi, 0, numSamples, 0);
        midi.addEvents (seqMidi2, 0, numSamples, 0);
    }
}

//==============================================================================
int URingAudioProcessor::paramIndex (const juce::String& paramId) const
{
    for (int i = 0; i < rangedParams.size(); ++i)
        if (rangedParams.getUnchecked (i)->getParameterID() == paramId)
            return i;
    return -1;
}

void URingAudioProcessor::startMidiLearn (const juce::String& paramId) { learnParam = paramIndex (paramId); }

void URingAudioProcessor::clearMidiMapping (const juce::String& paramId)
{
    const int idx = paramIndex (paramId);
    for (auto& slot : ccMap)
        if (slot.load() == idx) slot = -1;
}

void URingAudioProcessor::clearAllMidiMappings()
{
    for (auto& slot : ccMap) slot = -1;
}

int URingAudioProcessor::getMappedCc (const juce::String& paramId) const
{
    const int idx = paramIndex (paramId);
    for (int cc = 0; cc < 128; ++cc)
        if (idx >= 0 && ccMap[(size_t) cc].load() == idx)
            return cc;
    return -1;
}

juce::String URingAudioProcessor::popLearnEvent()
{
    const int cc = learnedCc.exchange (-1);
    const int idx = learnedParam.exchange (-1);
    if (cc < 0 || idx < 0 || idx >= rangedParams.size())
        return {};
    return "CC " + juce::String (cc) + " > " + rangedParams.getUnchecked (idx)->getName (24);
}

void URingAudioProcessor::timerCallback()
{
    // QUALITY cambió: se vuelven a preparar las voces con el audio detenido un instante
    if ((get (IDs::quality) > 0.5f ? 2 : 1) != preparedFactor && sampleRateHz > 0.0)
    {
        suspendProcessing (true);
        prepareVoices();
        suspendProcessing (false);
    }
    syncPatternMeta();
    if (recOffRequest.exchange (false)) setParamValue (IDs::autoRec, 0.0f); // terminó el ciclo grabado
    applyMorph();

    for (int cc = 0; cc < 128; ++cc)
    {
        if (! ccDirty[(size_t) cc].exchange (false))
            continue;
        const int idx = ccMap[(size_t) cc].load();
        if (idx >= 0 && idx < rangedParams.size())
        {
            auto* p = rangedParams.getUnchecked (idx);
            p->beginChangeGesture();
            p->setValueNotifyingHost (ccValue[(size_t) cc].load());
            p->endChangeGesture();
        }
    }
}

//==============================================================================
void URingAudioProcessor::setParamValue (juce::StringRef id, float realValue)
{
    const SuppressTouch st (*this);
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
        p->endChangeGesture();
    }
}

// Tras cambiar de patrón, sus STEPS/PULSES/ROTATE/DIRECTION pasan a las perillas
void URingAudioProcessor::syncPatternMeta()
{
    if (! metaLoadRequest.load())
        return;
    const auto m = pattern().getMeta();
    setParamValue (IDs::seqLength, (float) m.length);
    setParamValue (IDs::seqPulses, (float) m.pulses);
    setParamValue (IDs::seqRotate, (float) m.rotate);
    setParamValue (IDs::seqDir, (float) m.direction);
    metaLoadRequest = false;
}

void URingAudioProcessor::setEditSequencer (int seq)
{
    seq = juce::jlimit (0, 1, seq);
    if (seq == editSeq.load()) return;
    // El audio thread hace el cambio (si no está corriendo, lo hace el timer)
    pendingEdit = seq;
    if (juce::Time::getMillisecondCounter() - lastBlockMs.load() > 200) { pendingEdit = -1; metaLoadRequest = true; editSeq = seq; syncPatternMeta(); }
}

void URingAudioProcessor::selectPattern (int index)
{
    index = juce::jlimit (0, kNumPatterns - 1, index);
    if (transportRunning.load())
    {
        queuedPattern = index; // cambia al terminar el ciclo actual
    }
    else
    {
        activePattern = index;
        queuedPattern = -1;
        metaLoadRequest = true;
        syncPatternMeta();
    }
}

// Cadena: letras A–H separadas o juntas ("A A B C" o "AABC")
void URingAudioProcessor::setChain (const juce::String& text)
{
    chainText = text.toUpperCase().retainCharacters ("ABCDEFGH ");
    int n = 0;
    for (auto c : chainText)
        if (c >= 'A' && c <= 'H' && n < (int) chainSteps.size())
            chainSteps[(size_t) n++] = (int) (c - 'A');
    chainLen = n;
    chainPos = -1;
}

//==============================================================================
std::vector<URingAudioProcessor::PatchCable> URingAudioProcessor::getPatches() const
{
    std::vector<PatchCable> out;
    for (const auto& slot : patchSlots)
        if (slot.source.load() >= 0 && slot.dest.load() > DOff)
            out.push_back ({ slot.source.load(), slot.dest.load(), slot.amount.load() });
    return out;
}

void URingAudioProcessor::addPatch (int source, int dest, float amount)
{
    if (source < 0 || source >= NumPatchSources || dest <= DOff || dest >= NumDests)
        return;
    for (auto& slot : patchSlots)
        if (slot.source.load() == source && slot.dest.load() == dest)
        {
            slot.amount = amount;
            return;
        }
    for (auto& slot : patchSlots)
        if (slot.source.load() < 0)
        {
            slot.amount = amount;
            slot.dest = dest;
            slot.source = source; // se escribe al final: el audio thread solo lee slots completos
            return;
        }
}

void URingAudioProcessor::removePatch (int source, int dest)
{
    for (auto& slot : patchSlots)
        if (slot.source.load() == source && slot.dest.load() == dest)
        {
            slot.source = -1;
            slot.dest = -1;
        }
}

void URingAudioProcessor::setPatchAmount (int source, int dest, float amount)
{
    for (auto& slot : patchSlots)
        if (slot.source.load() == source && slot.dest.load() == dest)
            slot.amount = juce::jlimit (-1.0f, 1.0f, amount);
}

void URingAudioProcessor::clearPatches()
{
    for (auto& slot : patchSlots) { slot.source = -1; slot.dest = -1; }
}

//==============================================================================
juce::File URingAudioProcessor::userPresetFolder()
{
    // ~/Music/CCRing/Presets. La primera vez trae los presets de usuario y favoritos de URing (~/Music/URing)
    const auto music = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    const auto folder = music.getChildFile ("CCRing").getChildFile ("Presets");
    static bool migrated = false;
    if (! migrated)
    {
        migrated = true;
        const auto old = music.getChildFile ("URing");
        if (! folder.isDirectory() && old.getChildFile ("Presets").isDirectory())
        {
            folder.createDirectory();
            for (const auto& f : old.getChildFile ("Presets").findChildFiles (juce::File::findFiles, false, "*.uring"))
                f.copyFileTo (folder.getChildFile (f.getFileName()));
        }
        const auto fav = folder.getParentDirectory().getChildFile ("Favorites.txt");
        if (! fav.existsAsFile() && old.getChildFile ("Favorites.txt").existsAsFile())
        {
            folder.getParentDirectory().createDirectory();
            old.getChildFile ("Favorites.txt").copyFileTo (fav);
        }
    }
    return folder;
}

juce::StringArray URingAudioProcessor::getUserPresetNames() const
{
    juce::StringArray names;
    for (const auto& f : userPresetFolder().findChildFiles (juce::File::findFiles, false, "*.uring"))
        names.add (f.getFileNameWithoutExtension());
    names.sortNatural();
    return names;
}

bool URingAudioProcessor::saveUserPreset (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty())
        return false;
    juce::MemoryBlock block;
    getStateInformation (block);
    auto folder = userPresetFolder();
    folder.createDirectory();
    userPresetName = clean;
    const bool ok = folder.getChildFile (clean + ".uring").replaceWithData (block.getData(), block.getSize());
    if (ok) presetDirty = false;
    return ok;
}

bool URingAudioProcessor::loadUserPreset (const juce::String& name)
{
    juce::MemoryBlock block;
    if (! userPresetFolder().getChildFile (name + ".uring").loadFileAsData (block))
        return false;
    loadingUserPreset = true;
    setStateInformation (block.getData(), (int) block.getSize());
    loadingUserPreset = false;
    {   // como los de fábrica: el secuenciador queda apagado hasta que el usuario toca RUN
        const SuppressTouch st (*this);
        setParamValue (IDs::seqOn, 0.0f);
    }
    presetIndex = -1; // preset de usuario (no de fábrica)
    userPresetName = name;
    metaLoadRequest = true;
    // Mismo volumen general que los de fábrica: lo que tenía la perilla pasa al nivel propio del preset
    presetTrimDb = presetTrimDb.load() + get (IDs::volume) - kStartVolumeDb;
    setStartVolume();
    presetDirty = false;
    morphSkip = true;
    return true;
}

void URingAudioProcessor::updateMacros()
{
    for (int i = 0; i < numParams; ++i) macroOff[(size_t) i] = 0.0f;
    for (int k = 0; k < 4; ++k)
    {
        const float m = raw[(size_t) macroIdx[(size_t) k]]->load();
        if (std::abs (m) < 1.0e-4f) continue;
        for (const auto& t : macroTargets[(size_t) k])
            macroOff[(size_t) t.param] = macroOff[(size_t) t.param].load() + m * t.amount;
    }
}

void URingAudioProcessor::storeMorph (int slot)
{
    slot = juce::jlimit (0, 1, slot);
    std::vector<float> v ((size_t) numParams, 0.0f);
    for (int i = 0; i < numParams; ++i) v[(size_t) i] = rangedParams.getUnchecked (i)->getValue();
    {
        const juce::SpinLock::ScopedLockType sl (morphLock);
        morphSnap[(size_t) slot].swap (v);
        morphHas[(size_t) slot] = true;
    }
    lastMorph = get (IDs::morph); // guardar no mueve nada; recién al girar MORPH
}

// MORPH (en el timer): las perillas continuas se mezclan; las elecciones cambian a mitad de camino
void URingAudioProcessor::applyMorph()
{
    const float m = get (IDs::morph);
    if (morphSkip.exchange (false)) { lastMorph = m; return; } // tras cargar un preset no se aplica
    if (! morphHas[0] || ! morphHas[1] || std::abs (m - lastMorph) < 1.0e-3f) return;
    lastMorph = m;
    std::array<std::vector<float>, 2> snap;
    {
        const juce::SpinLock::ScopedLockType sl (morphLock);
        snap = morphSnap;
    }
    {
        const SuppressTouch st (*this);
        for (int i = 0; i < numParams; ++i)
        {
            if (! morphable[(size_t) i]) continue;
            const float a = snap[0][(size_t) i], b = snap[1][(size_t) i];
            const float target = isFloat[(size_t) i] ? a + (b - a) * m : (m < 0.5f ? a : b);
            auto* p = rangedParams.getUnchecked (i);
            if (std::abs (p->getValue() - target) > 1.0e-5f) p->setValueNotifyingHost (target);
        }
    }
    presetDirty = true;
}

void URingAudioProcessor::setStartVolume()
{
    const SuppressTouch st (*this);
    setParamValue (IDs::volume, kStartVolumeDb);
}

juce::String URingAudioProcessor::getCurrentPresetName() const
{
    const int i = presetIndex.load();
    if (i >= 0 && i < (int) presets::factory().size()) return presets::factory()[(size_t) i].name;
    return userPresetName.isNotEmpty() ? userPresetName : juce::String ("Init");
}


//==============================================================================
float URingAudioProcessor::measureSoundLevelDb()
{
    URingAudioProcessor probe;
    for (int i = 0; i < numParams && i < probe.numParams; ++i)
        probe.rangedParams.getUnchecked (i)->setValueNotifyingHost (rangedParams.getUnchecked (i)->getValue());
    probe.clearPatches();
    for (const auto& c : getPatches()) probe.addPatch (c.source, c.dest, c.amount);
    probe.presetTrimDb = presetTrimDb.load();
    probe.setParamValue (IDs::seqOn, 0.0f);
    constexpr double sr = 48000.0;
    constexpr int bs = 512;
    probe.setPlayConfigDetails (0, 2, sr, bs);
    probe.prepareToPlay (sr, bs);
    juce::AudioBuffer<float> b (2, bs);
    const int total = (int) (1.6 * sr), win = (int) (0.4 * sr);
    std::vector<float> sq ((size_t) total, 0.0f);
    static const int notes[] { 48, 55, 63 };
    for (int pos = 0; pos + bs <= total; pos += bs)
    {
        juce::MidiBuffer m;
        if (pos == 0) for (int n : notes) m.addEvent (juce::MidiMessage::noteOn (1, n, 0.8f), 0);
        if (pos <= (int) (1.0 * sr) && pos + bs > (int) (1.0 * sr)) for (int n : notes) m.addEvent (juce::MidiMessage::noteOff (1, n), 0);
        probe.processBlock (b, m);
        for (int i = 0; i < bs; ++i)
            sq[(size_t) (pos + i)] = 0.5f * (b.getSample (0, i) * b.getSample (0, i) + b.getSample (1, i) * b.getSample (1, i));
    }
    double best = 0.0, sum = 0.0;
    for (int i = 0; i < total; ++i)
    {
        sum += sq[(size_t) i];
        if (i >= win) sum -= sq[(size_t) (i - win)];
        if (i >= win - 1) best = std::max (best, sum / win);
    }
    return juce::Decibels::gainToDecibels ((float) std::sqrt (std::max (0.0, best)), -120.0f);
}

//==============================================================================
// RANDOM (dado). Cada tirada queda como un paso de deshacer (lo registra el editor).
juce::String URingAudioProcessor::randomizeSound (RandomScope scope, bool vary)
{
    juce::Random r;
    auto& P = *this;
    auto setReal = [&P] (const char* id, float v) { P.setParamValue (id, v); };
    auto uni = [&r] (float lo, float hi) { return lo + (hi - lo) * r.nextFloat(); };
    auto logu = [&r] (float lo, float hi) { return lo * std::pow (hi / lo, r.nextFloat()); };
    auto chance = [&r] (float p) { return r.nextFloat() < p; };

    if (vary)
    {
        // VARIAR: cada perilla continua del grupo se corre hasta ±15 % (los selectores no cambian)
        static const char* all[] { IDs::aStructure, IDs::aBright, IDs::aDamping, IDs::aPosition, IDs::bMaterial, IDs::bDecay,
                                   IDs::bTone, IDs::bPosition, IDs::bSpread, IDs::cHarm, IDs::cTimbre, IDs::cMorph, IDs::cDecay,
                                   IDs::cColour, IDs::cutoff, IDs::resonance, IDs::fltEnv, IDs::attack, IDs::decay, IDs::release,
                                   IDs::variation, IDs::width, IDs::chorusMix, IDs::delayMix, IDs::springMix };
        int moved = 0;
        for (auto* id : all)
        {
            const juce::String s (id);
            const bool inScope = scope == RandAll || (scope == RandEngineA && s.startsWith ("a"))
                              || (scope == RandEngineB && s.startsWith ("b")) || (scope == RandEngineC && s.startsWith ("c") && s != IDs::cutoff && ! s.startsWith ("chorus"))
                              || (scope == RandFx && (s.startsWith ("chorus") || s.startsWith ("delay") || s.startsWith ("spring")));
            if (! inScope) continue;
            if (auto* p = apvts.getParameter (id))
            {
                const SuppressTouch st (*this);
                p->beginChangeGesture();
                p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, p->getValue() + (r.nextFloat() * 2.0f - 1.0f) * 0.15f));
                p->endChangeGesture();
                ++moved;
            }
        }
        presetDirty = true;
        return "VARIATION  " + juce::String (moved) + " KNOBS";
    }

    juce::StringArray parts;
    const bool all = scope == RandAll;
    // Forma general del sonido (solo con "todo"): A sola, B sola, A+B, C protagonista o C excitando a A
    int shape = 0;
    if (all)
    {
        shape = r.nextInt (5);
        setReal (IDs::abMix, shape == 0 ? uni (0.0f, 0.15f) : shape == 1 ? uni (0.85f, 1.0f) : uni (0.3f, 0.7f));
        setReal (IDs::cMix, shape == 3 ? uni (0.6f, 1.0f) : chance (0.25f) ? uni (0.1f, 0.35f) : 0.0f);
        setReal (IDs::xfeed, chance (0.25f) ? uni (0.05f, 0.35f) : 0.0f);
        setReal (IDs::cFeed, shape == 4 ? 0.0f : (chance (0.15f) ? uni (0.2f, 0.7f) : 0.0f));
        setReal (IDs::cFeedB, chance (0.15f) ? uni (0.2f, 0.7f) : 0.0f);
        setReal (IDs::variation, uni (0.0f, 0.45f));
        setReal (IDs::width, uni (0.3f, 0.85f));
        setReal (IDs::tune, 0.0f);
        // filtro y envolvente
        setReal (IDs::fltType, chance (0.6f) ? 0.0f : (float) r.nextInt (7));
        setReal (IDs::cutoff, logu (900.0f, 16000.0f));
        setReal (IDs::resonance, uni (0.0f, 0.5f));
        setReal (IDs::fltEnv, chance (0.5f) ? uni (0.0f, 0.6f) : 0.0f);
        setReal (IDs::fltDecay, logu (60.0f, 1500.0f));
        setReal (IDs::drive, chance (0.3f) ? uni (0.05f, 0.4f) : 0.0f);
        const bool pad = chance (0.25f);
        setReal (IDs::attack, pad ? logu (150.0f, 1500.0f) : logu (0.5f, 25.0f));
        setReal (IDs::decay, logu (300.0f, 6000.0f));
        setReal (IDs::sustain, pad ? uni (0.6f, 1.0f) : uni (0.0f, 0.8f));
        setReal (IDs::release, pad ? logu (900.0f, 4000.0f) : logu (150.0f, 2000.0f));
        setReal (IDs::body, chance (0.4f) ? uni (0.1f, 0.6f) : 0.0f);
        if (pad) parts.add ("PAD");
    }
    const bool doA = all || scope == RandEngineA, doB = all || scope == RandEngineB, doC = all || scope == RandEngineC;
    if (doA)
    {
        const int m = r.nextInt (7);
        setReal (IDs::aModel, (float) m);
        setReal (IDs::aExciter, shape == 4 ? 5.0f : (float) r.nextInt (5));
        setReal (IDs::aStructure, uni (0.05f, 0.85f));
        setReal (IDs::aBright, uni (0.2f, 0.85f));
        setReal (IDs::aDamping, uni (0.15f, 0.7f));
        setReal (IDs::aPosition, uni (0.05f, 0.7f));
        if (! all || shape != 1) parts.add (apvts.getParameter (IDs::aModel)->getCurrentValueAsText().toUpperCase());
    }
    if (doB)
    {
        setReal (IDs::bModel, (float) r.nextInt (12));
        setReal (IDs::bExciter, (float) r.nextInt (4));
        setReal (IDs::bMaterial, uni (0.1f, 0.9f));
        setReal (IDs::bDecay, uni (0.2f, 0.7f));
        setReal (IDs::bTone, uni (0.3f, 0.85f));
        setReal (IDs::bPosition, uni (0.05f, 0.7f));
        setReal (IDs::bSpread, chance (0.3f) ? uni (0.05f, 0.4f) : 0.0f);
        setReal (IDs::bSweep, chance (0.25f) ? uni (0.05f, 0.35f) : 0.0f);
        if (! all || shape != 0) parts.add (apvts.getParameter (IDs::bModel)->getCurrentValueAsText().toUpperCase());
    }
    if (doC)
    {
        setReal (IDs::cModel, (float) r.nextInt (24));
        setReal (IDs::cHarm, uni (0.15f, 0.85f));
        setReal (IDs::cTimbre, uni (0.15f, 0.85f));
        setReal (IDs::cMorph, uni (0.15f, 0.85f));
        setReal (IDs::cFm, chance (0.2f) ? uni (-0.4f, 0.4f) : 0.0f);
        setReal (IDs::cTimbreMod, chance (0.3f) ? uni (-0.5f, 0.5f) : 0.0f);
        setReal (IDs::cMorphMod, chance (0.3f) ? uni (-0.5f, 0.5f) : 0.0f);
        setReal (IDs::cDecay, uni (0.15f, 0.75f));
        setReal (IDs::cColour, uni (0.2f, 0.8f));
        setReal (IDs::cLpg, chance (0.8f) ? 1.0f : 0.0f);
        if (scope == RandEngineC && get (IDs::cMix) < 0.05f) setReal (IDs::cMix, 0.5f);   // que se escuche
        if (! all || shape >= 3 || get (IDs::cMix) > 0.05f)
            parts.add (apvts.getParameter (IDs::cModel)->getCurrentValueAsText().toUpperCase());
    }
    if (all || scope == RandFx)
    {
        setReal (IDs::chorusMix, chance (0.4f) ? uni (0.1f, 0.45f) : 0.0f);
        setReal (IDs::glitchOn, chance (0.12f) ? 1.0f : 0.0f);
        setReal (IDs::glitchProb, uni (0.1f, 0.4f));
        setReal (IDs::delayOn, chance (0.7f) ? 1.0f : 0.0f);
        setReal (IDs::delayMix, uni (0.08f, 0.35f));
        setReal (IDs::delayFb, uni (0.2f, 0.6f));
        setReal (IDs::delayWow, uni (0.0f, 0.4f));
        setReal (IDs::springOn, chance (0.7f) ? 1.0f : 0.0f);
        setReal (IDs::springMix, uni (0.08f, 0.35f));
        setReal (IDs::springDecay, logu (0.8f, 4.0f));
        setReal (IDs::plateOn, chance (0.25f) ? 1.0f : 0.0f);
        setReal (IDs::plateMix, uni (0.1f, 0.35f));
        setReal (IDs::texOn, chance (0.1f) ? 1.0f : 0.0f);
        setReal (IDs::texMix, uni (0.2f, 0.5f));
        if (scope == RandFx) parts.add ("FX");
    }
    if (all || scope == RandMod)
    {
        // LFOs nuevos y, a veces, un cable de LFO a algo que se note
        for (int i = 0; i < kNumLfos; ++i)
        {
            setReal (IDs::lfo (i, "Shape").toRawUTF8(), (float) r.nextInt (6));
            setReal (IDs::lfo (i, "Rate").toRawUTF8(), logu (0.05f, 6.0f));
            setReal (IDs::lfo (i, "Depth").toRawUTF8(), uni (-0.5f, 0.5f));
        }
        clearPatches();
        static const int dests[] { DAPosition, DABright, DBMaterial, DBTone, DCutoff, DCTimbre, DCMorph };
        const int cables = all ? r.nextInt (3) : 1 + r.nextInt (2);
        for (int k = 0; k < cables; ++k)
            addPatch (r.nextInt (2), dests[r.nextInt (7)], uni (0.1f, 0.45f) * (chance (0.25f) ? -1.0f : 1.0f));
        if (scope == RandMod) parts.add (juce::String (cables) + (cables == 1 ? " CABLE" : " CABLES"));
    }

    // Como un preset nuevo (volumen a -20 dB y un nivel propio parejo), pero el secuenciador sigue
    // como estaba: si está sonando, las notas siguientes ya salen con el sonido nuevo
    if (all)
    {
        const SuppressTouch st (*this);
        setParamValue (IDs::limiterOn, 1.0f);
        presetTrimDb = randomTrimDb;
        setStartVolume();
        presetIndex = -1;
        userPresetName = "Random";
    }
    // Nivelado: se mide en un procesador aparte y se corrige el nivel propio para que suene como los presets
    {
        constexpr float target = -23.0f;
        const float level = measureSoundLevelDb();
        if (level < -75.0f && all && ++rerolls < 4)   // casi mudo: otra tirada
            return randomizeSound (scope, vary);
        rerolls = 0;
        if (level > -75.0f)
            presetTrimDb = juce::jlimit (-12.0f, 30.0f, presetTrimDb.load() + (target - level));
    }
    presetDirty = true;
    return parts.isEmpty() ? juce::String ("NEW SOUND") : parts.joinIntoString (" + ");
}

//==============================================================================
void URingAudioProcessor::loadPreset (int index)
{
    const auto& list = presets::factory();
    if (index < 0 || index >= (int) list.size())
        return;
    const SuppressTouch st (*this);
    clearAllAutomation(); // sonido nuevo: sin movimientos grabados

    auto set = [] (juce::RangedAudioParameter* p, float normalised)
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (normalised);
        p->endChangeGesture();
    };

    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            set (rp, rp->getDefaultValue());

    for (const auto& [id, value] : list[(size_t) index].values)
        if (auto* rp = apvts.getParameter (id))
            set (rp, rp->convertTo0to1 (value));

    clearPatches();
    addPatch (SrcLfo1, DAPosition, 0.15f); // cable por defecto: LFO 1 → POSITION del motor A
    userPresetName = {};

    // Los presets cargan con el secuenciador apagado (se prende con RUN) y su propio patrón
    // (reproducible). Se rehacen las 8 memorias de los dos secuenciadores: el preset suena
    // siempre igual, sin restos de lo que se tocó antes.
    set (apvts.getParameter (IDs::seqOn), 0.0f);
    activePattern = 0; queuedPattern = -1;
    setChain ({});
    const cz::SeqPattern::Meta meta { (int) get (IDs::seqLength), (int) get (IDs::seqPulses), (int) get (IDs::seqRotate), (int) get (IDs::seqDir) };
    for (int i = 0; i < kNumPatterns; ++i)
    {
        juce::Random seeded (1000 + index + i * 7919);
        patterns[(size_t) i].generate (get (IDs::seqRange), currentScale(), seeded);
        patterns[(size_t) i].setMeta (meta);
        juce::Random seeded2 (5000 + index + i * 7919);   // SEQ 2: su propio dibujo (se oye al encenderlo)
        patterns2[(size_t) i].generate (get (IDs::seqRange), currentScale(), seeded2);
        patterns2[(size_t) i].setMeta ({ 12, 5, 2, 0 });
    }
    editSeq = 0; pendingEdit = -1; // las perillas del preset son del SEQ 1
    // Presets con LOCK DEST traen locks en el patrón (cada golpe con otro timbre)
    for (const auto& [id, value] : list[(size_t) index].values)
        if (juce::String (id) == IDs::lockDest)
            for (int i = 0; i < kNumPatterns; ++i)
            {
                juce::Random rl (3000 + index + i);
                patterns[(size_t) i].randomizeLane (cz::Lane::Lock, get (IDs::seqRange), currentScale(), rl);
                patterns2[(size_t) i].randomizeLane (cz::Lane::Lock, get (IDs::seqRange), currentScale(), rl);
            }

    // Carril MODEL de fábrica: "@m0", "@m1"… = modelo del motor C de cada paso (el dibujo se repite
    // cada tantos pasos como valores traiga el preset), en las 8 memorias del SEQ 1
    {
        std::vector<int> models;
        for (const auto& [id, value] : list[(size_t) index].values)
            if (juce::String (id).startsWith ("@m")) models.push_back ((int) value);
        if (! models.empty())
            for (auto& pat : patterns)
                for (int s = 0; s < cz::SeqPattern::kSteps; ++s)
                {
                    auto d = pat.get (s);
                    d.model = juce::jlimit (-1, cz::kNumCModels - 1, models[(size_t) s % models.size()]);
                    pat.set (s, d);
                }
    }

    // VOLUME vuelve a -20 dB; el nivel que traía el preset queda como su nivel propio
    float presetVol = -6.0f;
    for (const auto& [id, value] : list[(size_t) index].values)
        if (juce::String (id) == IDs::volume) presetVol = value;
    presetTrimDb = presetVol + 6.0f;
    set (apvts.getParameter (IDs::volume), apvts.getParameter (IDs::volume)->convertTo0to1 (kStartVolumeDb));

    presetIndex = index;
    presetDirty = false;
    morphSkip = true;
}

void URingAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetIndex", presetIndex.load(), nullptr);
    state.setProperty ("presetTrim", presetTrimDb.load(), nullptr);
    state.setProperty ("presetDirty", presetDirty.load(), nullptr);
    state.removeChild (state.getChildWithName ("MORPH"), nullptr);
    juce::ValueTree mo ("MORPH");
    const juce::SpinLock::ScopedLockType msl (morphLock);
    for (int s = 0; s < 2; ++s)
        if (morphHas[(size_t) s])
        {
            juce::StringArray vals;
            for (int i = 0; i < numParams; ++i)
                vals.add (rangedParams.getUnchecked (i)->getParameterID() + "=" + juce::String (morphSnap[(size_t) s][(size_t) i], 5));
            mo.setProperty (s == 0 ? "a" : "b", vals.joinIntoString (" "), nullptr);
        }
    state.appendChild (mo, nullptr);
    state.removeChild (state.getChildWithName ("PATTERNS"), nullptr);
    juce::ValueTree pats ("PATTERNS");
    pats.setProperty ("active", activePattern.load(), nullptr);
    pats.setProperty ("chain", chainText, nullptr);
    for (const auto& pt : patterns)
        pats.appendChild (pt.toValueTree(), nullptr);
    state.appendChild (pats, nullptr);
    state.removeChild (state.getChildWithName ("PATTERNS2"), nullptr);
    juce::ValueTree pats2 ("PATTERNS2");
    pats2.setProperty ("edit", editSeq.load(), nullptr);
    for (const auto& pt : patterns2)
        pats2.appendChild (pt.toValueTree(), nullptr);
    state.appendChild (pats2, nullptr);

    state.removeChild (state.getChildWithName ("PATCH"), nullptr);
    juce::ValueTree patch ("PATCH");
    for (const auto& c : getPatches())
    {
        juce::ValueTree e ("CABLE");
        e.setProperty ("src", c.source, nullptr);
        e.setProperty ("dst", c.dest, nullptr);
        e.setProperty ("amt", c.amount, nullptr);
        patch.appendChild (e, nullptr);
    }
    state.appendChild (patch, nullptr);

    state.removeChild (state.getChildWithName ("MIDIMAP"), nullptr);
    juce::ValueTree map ("MIDIMAP");
    for (int cc = 0; cc < 128; ++cc)
        if (const int idx = ccMap[(size_t) cc].load(); idx >= 0 && idx < rangedParams.size())
        {
            juce::ValueTree e ("CC");
            e.setProperty ("cc", cc, nullptr);
            e.setProperty ("param", rangedParams.getUnchecked (idx)->getParameterID(), nullptr);
            map.appendChild (e, nullptr);
        }
    state.appendChild (map, nullptr);

    state.removeChild (state.getChildWithName ("AUTOMATION"), nullptr);
    juce::ValueTree au ("AUTOMATION");
    for (int pat = 0; pat < kNumPatterns; ++pat)
        for (int i = 0; i < numParams; ++i)
        {
            const auto& ln = lane (pat, i);
            if (! ln.any()) continue;
            juce::StringArray steps;
            for (int s = 0; s < kAutoSteps; ++s)
                if (ln.has (s)) steps.add (juce::String (s) + ":" + juce::String (ln.v[(size_t) s].load(), 5));
            juce::ValueTree e ("LANE");
            e.setProperty ("pat", pat, nullptr);
            e.setProperty ("param", rangedParams.getUnchecked (i)->getParameterID(), nullptr);
            e.setProperty ("steps", steps.joinIntoString (" "), nullptr);
            au.appendChild (e, nullptr);
        }
    state.appendChild (au, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void URingAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.hasType (apvts.state.getType()))
            return;
        // La app Standalone arranca siempre limpia: no recupera lo que quedó tocado la última vez
        // (los sonidos solo se guardan con SAVE). En Ableton el proyecto sí guarda todo.
        if (wrapperType == wrapperType_Standalone && ! loadingUserPreset)
            return;
        presetIndex = (int) tree.getProperty ("presetIndex", 0);
        presetTrimDb = (float) tree.getProperty ("presetTrim", 0.0f);
        morphSkip = true;
        if (auto mo = tree.getChildWithName ("MORPH"); mo.isValid())
        {
            const juce::SpinLock::ScopedLockType msl (morphLock);
            for (int s = 0; s < 2; ++s)
            {
                const auto txt = mo.getProperty (s == 0 ? "a" : "b").toString();
                morphHas[(size_t) s] = txt.isNotEmpty();
                if (! morphHas[(size_t) s]) continue;
                auto& v = morphSnap[(size_t) s];
                v.assign ((size_t) numParams, 0.0f);
                for (int i = 0; i < numParams; ++i) v[(size_t) i] = rangedParams.getUnchecked (i)->getValue();
                for (const auto& t : juce::StringArray::fromTokens (txt, " ", {}))
                    if (const int pi = paramIndex (t.upToFirstOccurrenceOf ("=", false, false)); pi >= 0)
                        v[(size_t) pi] = t.fromFirstOccurrenceOf ("=", false, false).getFloatValue();
            }
            tree.removeChild (mo, nullptr);
        }
        presetDirty = (bool) tree.getProperty ("presetDirty", false);
        if (auto map = tree.getChildWithName ("MIDIMAP"); map.isValid())
        {
            clearAllMidiMappings();
            for (const auto& e : map)
            {
                const int cc = (int) e.getProperty ("cc", -1);
                const int idx = paramIndex (e.getProperty ("param").toString());
                if (cc >= 0 && cc < 128 && idx >= 0)
                    ccMap[(size_t) cc] = idx;
            }
            tree.removeChild (map, nullptr);
        }
        if (auto pats = tree.getChildWithName ("PATTERNS"); pats.isValid())
        {
            for (int i = 0; i < juce::jmin (kNumPatterns, pats.getNumChildren()); ++i)
                patterns[(size_t) i].fromValueTree (pats.getChild (i));
            activePattern = juce::jlimit (0, kNumPatterns - 1, (int) pats.getProperty ("active", 0));
            setChain (pats.getProperty ("chain").toString());
            tree.removeChild (pats, nullptr);
        }
        if (auto pats2 = tree.getChildWithName ("PATTERNS2"); pats2.isValid())
        {
            for (int i = 0; i < juce::jmin (kNumPatterns, pats2.getNumChildren()); ++i)
                patterns2[(size_t) i].fromValueTree (pats2.getChild (i));
            editSeq = juce::jlimit (0, 1, (int) pats2.getProperty ("edit", 0));
            tree.removeChild (pats2, nullptr);
        }
        metaLoadRequest = true;
        if (auto patch = tree.getChildWithName ("PATCH"); patch.isValid())
        {
            clearPatches();
            for (const auto& e : patch)
                addPatch ((int) e.getProperty ("src", -1), (int) e.getProperty ("dst", -1), (float) e.getProperty ("amt", 0.5f));
            tree.removeChild (patch, nullptr);
        }
        clearAllAutomation();
        if (auto au = tree.getChildWithName ("AUTOMATION"); au.isValid())
        {
            for (const auto& e : au)
            {
                const int pat = (int) e.getProperty ("pat", 0), i = paramIndex (e.getProperty ("param").toString());
                if (pat < 0 || pat >= kNumPatterns || i < 0) continue;
                auto& ln = lane (pat, i);
                for (const auto& t : juce::StringArray::fromTokens (e.getProperty ("steps").toString(), " ", {}))
                {
                    const int s = t.upToFirstOccurrenceOf (":", false, false).getIntValue();
                    if (s < 0 || s >= kAutoSteps) continue;
                    ln.v[(size_t) s] = t.fromFirstOccurrenceOf (":", false, false).getFloatValue();
                    ln.set (s);
                }
            }
            tree.removeChild (au, nullptr);
        }
        const SuppressTouch st (*this);
        apvts.replaceState (tree);
    }
}

juce::AudioProcessorEditor* URingAudioProcessor::createEditor()
{
    return new URingAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new URingAudioProcessor();
}

//==============================================================================
// Deshacer / rehacer
URingAudioProcessor::UndoSnap URingAudioProcessor::captureSnap() const
{
    UndoSnap s;
    s.params.reserve ((size_t) numParams);
    for (int i = 0; i < numParams; ++i)
        if (rangedParams.getUnchecked (i)->getParameterID() != IDs::autoRec)   // el botón REC no es "sonido"
            s.params.push_back (rangedParams.getUnchecked (i)->getValue());
    s.pats = juce::ValueTree ("P");
    s.pats2 = juce::ValueTree ("P");
    for (const auto& pt : patterns)  s.pats.appendChild (pt.toValueTree(), nullptr);
    for (const auto& pt : patterns2) s.pats2.appendChild (pt.toValueTree(), nullptr);
    s.patch = juce::ValueTree ("C");
    for (const auto& c : getPatches())
    {
        juce::ValueTree e ("E");
        e.setProperty ("s", c.source, nullptr);
        e.setProperty ("d", c.dest, nullptr);
        e.setProperty ("a", c.amount, nullptr);
        s.patch.appendChild (e, nullptr);
    }
    s.presetIndex = presetIndex.load();
    s.trim = presetTrimDb.load();
    s.dirty = presetDirty.load();
    s.userName = userPresetName;
    return s;
}

void URingAudioProcessor::restoreSnap (const UndoSnap& s)
{
    const SuppressTouch st (*this);
    morphSkip = true;
    size_t k = 0;
    for (int i = 0; i < numParams; ++i)
    {
        auto* p = rangedParams.getUnchecked (i);
        if (p->getParameterID() == IDs::autoRec) continue;
        if (k >= s.params.size()) break;
        const float v = s.params[k++];
        if (std::abs (p->getValue() - v) > 1.0e-7f)
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (v);
            p->endChangeGesture();
        }
    }
    for (int i = 0; i < juce::jmin (kNumPatterns, s.pats.getNumChildren()); ++i)  patterns[(size_t) i].fromValueTree (s.pats.getChild (i));
    for (int i = 0; i < juce::jmin (kNumPatterns, s.pats2.getNumChildren()); ++i) patterns2[(size_t) i].fromValueTree (s.pats2.getChild (i));
    clearPatches();
    for (const auto& e : s.patch)
        addPatch ((int) e.getProperty ("s", -1), (int) e.getProperty ("d", -1), (float) e.getProperty ("a", 0.5f));
    presetIndex = s.presetIndex;
    presetTrimDb = s.trim;
    userPresetName = s.userName;
    presetDirty = s.dirty;
}

void URingAudioProcessor::undoTick (bool userChange)
{
    if (! undoStarted || ! userChange) { undoCurrent = captureSnap(); undoStarted = true; return; }
    auto now = captureSnap();
    if (now.sameSound (undoCurrent))
    {
        undoCurrent = std::move (now);   // nombre / estado del preset al día, sin crear un paso
        return;
    }
    undoStack.push_back (std::move (undoCurrent));
    if (undoStack.size() > kUndoMax) undoStack.erase (undoStack.begin());
    redoStack.clear();
    undoCurrent = std::move (now);
}

bool URingAudioProcessor::undo()
{
    undoTick (true);   // un cambio que todavía no se registró cuenta como el último paso
    if (undoStack.empty()) return false;
    redoStack.push_back (undoCurrent);
    undoCurrent = std::move (undoStack.back());
    undoStack.pop_back();
    restoreSnap (undoCurrent);
    undoCurrent = captureSnap();   // lo que quedó de verdad (el redondeo de algún parámetro no cuenta como cambio)
    return true;
}

bool URingAudioProcessor::redo()
{
    undoTick (true);
    if (redoStack.empty()) return false;
    undoStack.push_back (undoCurrent);
    undoCurrent = std::move (redoStack.back());
    redoStack.pop_back();
    restoreSnap (undoCurrent);
    undoCurrent = captureSnap();
    return true;
}

void URingAudioProcessor::reseedRandomness()
{
    mutateRng.setSeed (0x5eed01);
    sequencer.reseed (0x5eed11);
    sequencer2.reseed (0x5eed22);
    glitch.reseed (0x5eed33);
    for (size_t i = 0; i < lfos.size(); ++i) lfos[i].reseed (0x5eed40 + (juce::int64) i);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<cz::CzVoice*> (synth.getVoice (i)))
            v->reseed (0x5eed100 + i);
    stmlib::Random::Seed (0x5eed77);   // ruido interno de Plaits (compartido entre instancias)
}
