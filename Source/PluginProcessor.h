#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>   // antes que los encabezados de Mutable (sus macros chocan con JUCE)
#include "DSP/Voice.h"
#include "DSP/StepSeq.h"
#include "DSP/Fx2.h"
#include <unordered_map>

namespace IDs
{
    // Motor A — Resonator
    inline constexpr const char* aModel     = "aModel";
    inline constexpr const char* aExciter   = "aExciter";
    inline constexpr const char* aStructure = "aStructure";
    inline constexpr const char* aBright    = "aBright";
    inline constexpr const char* aDamping   = "aDamping";
    inline constexpr const char* aPosition  = "aPosition";
    inline constexpr const char* aPitch     = "aPitch";
    // Motor B — Impact
    inline constexpr const char* bModel     = "bModel";
    inline constexpr const char* bExciter   = "bExciter";
    inline constexpr const char* bMaterial  = "bMaterial";
    inline constexpr const char* bDecay     = "bDecay";
    inline constexpr const char* bTone      = "bTone";
    inline constexpr const char* bPosition  = "bPosition";
    inline constexpr const char* bSpread    = "bSpread";
    inline constexpr const char* bSweep     = "bSweep";
    inline constexpr const char* bPitch     = "bPitch";
    // Mezcla
    inline constexpr const char* abMix      = "abMix";
    inline constexpr const char* xfeed      = "xfeed";
    inline constexpr const char* tune       = "tune";
    inline constexpr const char* variation  = "variation";
    inline constexpr const char* width      = "width";
    // Filtro / envolvente
    inline constexpr const char* fltType    = "fltType";
    inline constexpr const char* cutoff     = "cutoff";
    inline constexpr const char* resonance  = "resonance";
    inline constexpr const char* fltEnv     = "fltEnv";
    inline constexpr const char* fltDecay   = "fltDecay";
    inline constexpr const char* keyTrack   = "keyTrack";
    inline constexpr const char* drive      = "drive";
    inline constexpr const char* attack     = "attack";
    inline constexpr const char* decay      = "decay";
    inline constexpr const char* sustain    = "sustain";
    inline constexpr const char* release    = "release";
    inline constexpr const char* velSens    = "velSens";
    inline constexpr const char* volume     = "volume";
    inline constexpr const char* body       = "body";       // caja de resonancia de madera
    // Vida, pedal y resonancia simpática
    inline constexpr const char* life       = "life";
    inline constexpr const char* pedal      = "pedal";
    inline constexpr const char* sympathy   = "sympathy";
    // Chorus
    inline constexpr const char* chorusMix   = "chorusMix";
    inline constexpr const char* chorusRate  = "chorusRate";
    inline constexpr const char* chorusDepth = "chorusDepth";
    // Secuenciador: destino de los locks, SEQ 2 sigue a SEQ 1, mutación por ciclo
    inline constexpr const char* lockDest   = "lockDest";
    inline constexpr const char* seq2Follow = "seq2Follow";
    inline constexpr const char* seqMutate  = "seqMutate";
    // En vivo: morph A ↔ B y 4 macros
    inline constexpr const char* morph      = "morph";
    inline constexpr const char* macro[]    { "macro1", "macro2", "macro3", "macro4" };
    // Motor C: MACRO (24 modelos de síntesis)
    inline constexpr const char* cModel     = "cModel";
    inline constexpr const char* cHarm      = "cHarm";
    inline constexpr const char* cTimbre    = "cTimbre";
    inline constexpr const char* cMorph     = "cMorph";
    inline constexpr const char* cFm        = "cFm";
    inline constexpr const char* cTimbreMod = "cTimbreMod";
    inline constexpr const char* cMorphMod  = "cMorphMod";
    inline constexpr const char* cDecay     = "cDecay";
    inline constexpr const char* cColour    = "cColour";
    inline constexpr const char* cLpg       = "cLpg";
    inline constexpr const char* cPitch     = "cPitch";
    inline constexpr const char* cOut       = "cOut";
    inline constexpr const char* cMix       = "cMix";
    inline constexpr const char* cFeed      = "cFeed";
    inline constexpr const char* cFeedB     = "cFeedB";     // 1.5: el motor C excita al motor B
    inline constexpr const char* seq2Target = "seq2Target";
    // FX
    inline constexpr const char* glitchOn    = "glitchOn";
    inline constexpr const char* glitchProb  = "glitchProb";
    inline constexpr const char* glitchLen   = "glitchLen";
    inline constexpr const char* glitchVar   = "glitchVar";
    inline constexpr const char* glitchCrush = "glitchCrush";
    inline constexpr const char* glitchMix   = "glitchMix";
    inline constexpr const char* delayOn     = "delayOn";
    inline constexpr const char* delaySync   = "delaySync";
    inline constexpr const char* delayDiv    = "delayDiv";
    inline constexpr const char* delayTime   = "delayTime";
    inline constexpr const char* delayFb     = "delayFb";
    inline constexpr const char* delayWow    = "delayWow";
    inline constexpr const char* delayDrive  = "delayDrive";
    inline constexpr const char* delayHp     = "delayHp";
    inline constexpr const char* delayLp     = "delayLp";
    inline constexpr const char* delayPing   = "delayPing";
    inline constexpr const char* delayMix    = "delayMix";
    inline constexpr const char* springOn      = "springOn";
    inline constexpr const char* springTension = "springTension";
    inline constexpr const char* springDecay   = "springDecay";
    inline constexpr const char* springTone    = "springTone";
    inline constexpr const char* springDrip    = "springDrip";
    inline constexpr const char* springMix     = "springMix";
    inline constexpr const char* springDamp    = "springDamp";
    inline constexpr const char* springPre     = "springPre";
    inline constexpr const char* delayDuck     = "delayDuck";
    // Secuenciador euclidiano
    inline constexpr const char* seqOn      = "seqOn";
    inline constexpr const char* seqDiv     = "seqDiv";
    inline constexpr const char* seqLength  = "seqLength";
    inline constexpr const char* seqPulses  = "seqPulses";
    inline constexpr const char* seqRotate  = "seqRotate";
    inline constexpr const char* seqRoot    = "seqRoot";
    inline constexpr const char* seqOctave  = "seqOctave";
    inline constexpr const char* seqScale   = "seqScale";
    inline constexpr const char* seqRange   = "seqRange";
    inline constexpr const char* seqGate    = "seqGate";
    inline constexpr const char* seqSwing   = "seqSwing";
    inline constexpr const char* seqProb    = "seqProb";
    inline constexpr const char* seqRandom  = "seqRandom";
    inline constexpr const char* seqDir     = "seqDir";
    inline constexpr const char* seqDensity = "seqDensity";
    inline constexpr const char* midiOut    = "midiOut";
    inline constexpr const char* seqRndRate = "seqRndRate";
    inline constexpr const char* seqRndTime = "seqRndTime";
    inline constexpr const char* seqLongShort = "seqLongShort";
    // Longitud propia de cada carril (polimetría)
    inline constexpr const char* seqLaneLen[] { "seqLenNote", "seqLenOct", "seqLenVel", "seqLenGate", "seqLenProb",
                                                "seqLenRatch", "seqLenCond", "seqLenLock", "seqLenModel" };
    inline constexpr const char* seqFill    = "seqFill";
    inline constexpr const char* seqChainOn = "seqChainOn";
    inline constexpr const char* freeze     = "freeze";
    inline constexpr const char* tempo      = "tempo";
    // Grabación de automatizaciones (motion REC)
    inline constexpr const char* autoRec    = "autoRec";
    inline constexpr const char* autoLen    = "autoLen";
    // Segundo secuenciador (toca el motor B; el primero pasa a tocar solo el motor A)
    inline constexpr const char* seq2On     = "seq2On";
    inline constexpr const char* seq2Div    = "seq2Div";
    inline constexpr const char* seq2Oct    = "seq2Oct";
    // 1.4: calidad (oversampling), MARBLES en el secuenciador, página TEXTURE y salida
    inline constexpr const char* quality    = "quality";
    inline constexpr const char* seqMarbles = "seqMarbles";
    inline constexpr const char* seqDejaVu  = "seqDejaVu";
    inline constexpr const char* seqMarbLen = "seqMarbLen";
    inline constexpr const char* seqSpread  = "seqSpread";
    inline constexpr const char* seqBias    = "seqBias";
    inline constexpr const char* texOn       = "texOn";
    inline constexpr const char* texMode     = "texMode";
    inline constexpr const char* texPosition = "texPosition";
    inline constexpr const char* texSize     = "texSize";
    inline constexpr const char* texPitch    = "texPitch";
    inline constexpr const char* texDensity  = "texDensity";
    inline constexpr const char* texTexture  = "texTexture";
    inline constexpr const char* texSpread   = "texSpread";
    inline constexpr const char* texFeedback = "texFeedback";
    inline constexpr const char* texReverb   = "texReverb";
    inline constexpr const char* texFreeze   = "texFreeze";
    inline constexpr const char* texMix      = "texMix";
    inline constexpr const char* plateOn     = "plateOn";
    inline constexpr const char* plateSize   = "plateSize";
    inline constexpr const char* plateDecay  = "plateDecay";
    inline constexpr const char* plateDamp   = "plateDamp";
    inline constexpr const char* platePre    = "platePre";
    inline constexpr const char* plateMod    = "plateMod";
    inline constexpr const char* plateMix    = "plateMix";
    inline constexpr const char* eqLow       = "eqLow";
    inline constexpr const char* eqMid       = "eqMid";
    inline constexpr const char* eqMidHz     = "eqMidHz";
    inline constexpr const char* eqHigh      = "eqHigh";
    inline constexpr const char* masterDrive = "masterDrive";
    inline constexpr const char* limiterOn   = "limiterOn";

    // LFO 1/2: "lfo1Shape", "lfo2Rate", …
    inline juce::String lfo (int i, const char* what) { return "lfo" + juce::String (i + 1) + what; }
}

class URingAudioProcessor : public juce::AudioProcessor, private juce::Timer,
                            private juce::AudioProcessorParameter::Listener
{
public:
    URingAudioProcessor();
    ~URingAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; } // salida MIDI del secuenciador
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // ---- MIDI Learn: asigna un CC del controlador a cualquier parámetro (message thread)
    void startMidiLearn (const juce::String& paramId);
    void cancelMidiLearn()                         { learnParam = -1; }
    void clearMidiMapping (const juce::String& paramId);
    void clearAllMidiMappings();
    int getMappedCc (const juce::String& paramId) const;
    bool isLearning (const juce::String& paramId) const { return learnParam.load() >= 0 && learnParam.load() == paramIndex (paramId); }
    // Devuelve "CC n → parámetro" una sola vez tras completar un aprendizaje (vacío si no hubo)
    juce::String popLearnEvent();

    void loadPreset (int index);
    void generatePattern (juce::Random& r);
    void generatePattern()                { juce::Random r; generatePattern (r); }

    void randomizeRhythm();
    void randomizeLane (cz::Lane lane);
    void randomizeAll();
    // Botón 🙂: patrón completamente nuevo (ritmo, rotación, melodía, a veces dirección/polimetría).
    // Devuelve un resumen para la pantalla.
    juce::String surprise();
    void fitPatternToScale()              { pattern().fitToScale (currentScale()); }
    int baseNote() const;
    mm::Scale currentScale() const;

    // ---- Memorias de patrón A–H (cambian al terminar el ciclo; cadena opcional)
    static constexpr int kNumPatterns = 8;
    // pattern() = el patrón activo del secuenciador que se está editando (SEQ 1 o SEQ 2)
    cz::SeqPattern& pattern()                     { return patternOf (editSeq.load()); }
    const cz::SeqPattern& pattern() const         { return (editSeq.load() == 1 ? patterns2 : patterns)[(size_t) activePattern.load()]; }
    cz::SeqPattern& patternOf (int seq)           { return (seq == 1 ? patterns2 : patterns)[(size_t) activePattern.load()]; }
    int getEditSequencer() const                  { return editSeq.load(); }
    void setEditSequencer (int seq);              // la grilla y STEPS/PULSES/ROTATE/DIRECTION pasan a ese secuenciador
    cz::StepSequencer& editSequencer()            { return editSeq.load() == 1 ? sequencer2 : sequencer; }
    int getActivePattern() const                  { return activePattern.load(); }
    int getQueuedPattern() const                  { return queuedPattern.load(); }
    int getChainPosition() const                  { return chainPos.load(); }
    void selectPattern (int index);
    void copyPattern()                            { clipboard.copyFrom (pattern()); hasClipboard = true; }
    bool pastePattern()                           { if (hasClipboard) { pattern().copyFrom (clipboard); metaLoadRequest = true; } return hasClipboard; }
    void setChain (const juce::String& text);
    juce::String getChain() const                 { return chainText; }

    cz::StepSequencer sequencer, sequencer2;

    // ---- Patcheo: fuentes (LFO 1, LFO 2, ENV) → entradas de modulación
    enum PatchSource { SrcLfo1 = 0, SrcLfo2, SrcEnv, NumPatchSources };
    enum Dest { DOff = 0, DPitch, DAStructure, DABright, DADamping, DAPosition, DBMaterial, DBDecay,
                DBTone, DBSpread, DCutoff, DResonance, DABMix, DDelayTime, DGlitchProb, DSpringMix, DXFeed, DVolume,
                DCTimbre, DCMorph, DCHarmonics, NumDests };
    struct PatchCable { int source = -1, dest = -1; float amount = 0.5f; };
    static constexpr int kMaxPatches = 16;
    std::vector<PatchCable> getPatches() const;
    void addPatch (int source, int dest, float amount = 0.5f);
    void removePatch (int source, int dest);
    void setPatchAmount (int source, int dest, float amount);
    void clearPatches();
    float getSourceValue (int source) const      { return sourceValue[(size_t) source].load(); }

    // ---- Deshacer / rehacer (hilo de mensajes): fotos del sonido completo — perillas, selectores,
    // botones, pasos de los dos secuenciadores y cables. El editor llama undoTick() unas veces por
    // segundo cuando no se está arrastrando nada; cada cambio nuevo queda como un paso.
    // userChange = el usuario tocó algo desde el último tick (si no, lo que cambió solo —MUTATE,
    // random, automatización del DAW— se absorbe sin crear un paso)
    void undoTick (bool userChange);
    bool undo();
    bool redo();
    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }

    // ---- Salida para la pantalla (osciloscopio, espectro, vúmetro): sin locks
    static constexpr int kScopeSize = 4096;
    // Copia los últimos n samples (mono L+R) en orden cronológico; n <= kScopeSize
    void readScope (float* dest, int n) const
    {
        const int w = scopeWrite.load (std::memory_order_acquire);
        for (int i = 0; i < n; ++i)
            dest[i] = scope[(size_t) ((w - n + i) & (kScopeSize - 1))].load (std::memory_order_relaxed);
    }
    // Pico desde la última lectura (y lo reinicia). Cada pantalla usa su propio lector (0 = barra superior, 1 = analizador)
    float takePeak (int reader, int ch) { return peaks[(size_t) (reader * 2 + ch)].exchange (0.0f); }

    // ---- Presets de usuario (archivos en ~/Music/URing/Presets)
    static juce::File userPresetFolder();
    juce::StringArray getUserPresetNames() const;
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::String& name);
    juce::String getUserPresetName() const        { return userPresetName; }
    int getPresetIndex() const { return presetIndex.load(); }
    // Preset modificado (perillas movidas desde que se cargó o guardó): se ve con un * en el menú.
    // Los presets de fábrica nunca se modifican; los cambios solo quedan si guardás (SAVE).
    bool isPresetModified() const                  { return presetDirty.load(); }
    juce::String getCurrentPresetName() const;
    static constexpr float kStartVolumeDb = -20.0f; // volumen general al cargar cualquier preset

    // ---- Automatización grabada (REC): mientras suena, los movimientos de perillas se graban
    //      en un loop de 1-8 compases (resolución de semicorchea) por memoria de patrón A–H
    static constexpr int kAutoSteps = 128;          // 8 compases de semicorcheas
    int indexOf (const juce::String& paramId) const { return paramIndex (paramId); }
    bool hasAutomation (int paramIndex) const;      // en el patrón activo
    float getAutoDisplay01 (int paramIndex) const;  // valor que suena ahora (0–1) o -1
    juce::Array<int> getAutomatedParams() const;
    juce::String getParamDisplayName (int paramIndex) const;
    void clearAutomation (int paramIndex);          // -1 = todo el patrón activo
    void clearAllAutomation();
    // REC: queda armado hasta el paso 1, graba un ciclo completo del loop y se apaga solo
    enum RecState { RecIdle = 0, RecArmed, RecRecording };
    bool isRecordingAutomation() const              { return recState.load() == RecRecording && autoRunning.load(); }
    bool isAutomationArmed() const                  { return recState.load() == RecArmed; }
    float getAutoLoopPosition() const               { return autoPos.load(); }   // 0–1 dentro del loop
    std::atomic<int> lastRecorded { -1 };

    // ---- MORPH: dos fotos del sonido (A y B); la perilla MORPH pasa de una a otra
    void storeMorph (int slot);                   // 0 = A, 1 = B (guarda el sonido actual)
    bool hasMorph (int slot) const                { return morphHas[(size_t) slot]; }
    float getEffective (juce::StringRef id) const { return get (id); } // valor que suena (con macros y automatización)

    // ---- Macros: qué mueve cada una (nombre para la GUI)
    static const char* macroName (int i)          { static const char* n[] { "MATERIAL", "SPACE", "MOTION", "TONE" }; return n[i]; }

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

    static constexpr int kNumVoices = 10;
    static constexpr int kNumLfos = 2;

private:

    // Valor efectivo de un parámetro: la automatización grabada pisa a la perilla mientras suena
    float get (juce::StringRef id) const { return value (apvts.getRawParameterValue (id)); }
    float value (const std::atomic<float>* raw) const
    {
        const auto it = rawIndex.find (raw);
        if (it == rawIndex.end()) return raw->load();
        const auto i = (size_t) it->second;
        float v = overrideOn[i].load() ? overrideVal[i].load() : raw->load();
        if (macroOff != nullptr)
            if (const float off = macroOff[i].load(); off != 0.0f) // las macros empujan el valor (en su rango normalizado)
            {
                auto* p = rangedParams.getUnchecked ((int) i);
                v = p->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, p->convertTo0to1 (v) + off));
            }
        return v;
    }
    void updateAutomation (double ppq, bool running, juce::int64 clock, double unitPpq, int loopLen);
    void parameterValueChanged (int index, float) override;
    void parameterGestureChanged (int index, bool starting) override;
    struct SuppressTouch // cambios internos (presets, patrones, estado) no cuentan como "tocar"
    {
        explicit SuppressTouch (URingAudioProcessor& p) : proc (p) { ++proc.suppressTouch; }
        ~SuppressTouch() { --proc.suppressTouch; }
        URingAudioProcessor& proc;
    };

    struct AutoLane
    {
        std::array<std::atomic<float>, kAutoSteps> v {};
        std::array<std::atomic<juce::uint64>, 2> mask {};
        bool has (int s) const noexcept { return ((mask[(size_t) (s >> 6)].load() >> (s & 63)) & 1) != 0; }
        void set (int s) noexcept       { mask[(size_t) (s >> 6)].fetch_or ((juce::uint64) 1 << (s & 63)); }
        bool any() const noexcept       { return mask[0].load() != 0 || mask[1].load() != 0; }
        void clear() noexcept           { mask[0] = 0; mask[1] = 0; }
    };
    AutoLane& lane (int pat, int i) const { return lanes[(size_t) (pat * numParams + i)]; }
    int numParams = 0, autoRecIdx = 0, autoLenIdx = 0;
    std::vector<std::atomic<float>*> raw;
    std::vector<char> autoable, isFloat;
    std::unordered_map<const std::atomic<float>*, int> rawIndex;
    std::unique_ptr<AutoLane[]> lanes;
    std::unique_ptr<std::atomic<float>[]> overrideVal;
    std::unique_ptr<std::atomic<bool>[]> overrideOn, gesture;
    std::unique_ptr<std::atomic<juce::int64>[]> touchedUntil;
    std::atomic<juce::int64> sampleClock { 0 };
    std::atomic<int> suppressTouch { 0 };
    std::atomic<bool> autoRunning { false }, recOffRequest { false };
    std::atomic<float> autoPos { 0.0f };
    std::atomic<int> recState { RecIdle };
    bool autoWasRunning = false;
    double autoLastPos = 0.0;

    // Segundo secuenciador
    std::array<cz::SeqPattern, kNumPatterns> patterns2;
    juce::MidiBuffer seqMidi2;
    std::atomic<int> editSeq { 0 }, pendingEdit { -1 };
    std::atomic<juce::uint32> lastBlockMs { 0 };
    // Nivel propio de cada preset (dB): la perilla VOLUME siempre arranca en -20 y esto mantiene el
    // equilibrio entre presets (uno de piano y uno de marimba suenan parejos)
    std::atomic<float> presetTrimDb { 0.0f };
    std::array<std::atomic<float>, kScopeSize> scope {};
    std::atomic<int> scopeWrite { 0 };
    std::array<std::atomic<float>, 4> peaks {};

    struct UndoSnap
    {
        std::vector<float> params;
        juce::ValueTree pats, pats2, patch;
        int presetIndex = -1;
        float trim = 0.0f;
        bool dirty = false;
        juce::String userName;
        bool sameSound (const UndoSnap& o) const
        {
            if (params.size() != o.params.size()) return false;
            for (size_t i = 0; i < params.size(); ++i)
                if (std::abs (params[i] - o.params[i]) > 1.0e-5f) return false;
            return pats.isEquivalentTo (o.pats) && pats2.isEquivalentTo (o.pats2) && patch.isEquivalentTo (o.patch);
        }
    };
    UndoSnap captureSnap() const;
    void restoreSnap (const UndoSnap&);
    std::vector<UndoSnap> undoStack, redoStack;
    UndoSnap undoCurrent;
    bool undoStarted = false;
    static constexpr size_t kUndoMax = 100;
    std::atomic<bool> presetDirty { false };
    bool loadingUserPreset = false;
    void setStartVolume();
    void computeVoiceParams (const float* mod);
    int paramIndex (const juce::String& paramId) const;
    void timerCallback() override; // aplica en el message thread los CC recibidos

    juce::Array<juce::RangedAudioParameter*> rangedParams;
    std::array<std::atomic<int>, 128> ccMap;        // CC → índice de parámetro (-1 = libre)
    std::array<std::atomic<float>, 128> ccValue;
    std::array<std::atomic<bool>, 128> ccDirty;
    std::atomic<int> learnParam { -1 }, learnedCc { -1 }, learnedParam { -1 };

    // Punteros a los parámetros de los LFO (evita construir Strings en el audio thread)
    enum LfoParam { LShape = 0, LRate, LSync, LDiv, LDest, LDepth, NumLfoParams };
    std::array<std::array<std::atomic<float>*, NumLfoParams>, kNumLfos> lfoParams {};

    juce::Synthesiser synth;
    cz::VoiceParams voiceParams;
    std::array<cz::Lfo, kNumLfos> lfos;
    cz::Glitch glitch;
    cz::TapeDelay delay;
    cz::SpringReverb spring;
    cz::TextureFx texture;
    cz::PlateReverb plate;
    cz::MasterBus master;
    // QUALITY HIGH: las voces se calculan al doble de frecuencia y se bajan con un filtro de media banda
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::MidiBuffer midiOs;
    // Notas de cada sub-bloque de 32 muestras: el Synthesiser de JUCE dispara en el acto cualquier
    // nota posterior al tramo que se le pide, así que cada tramo recibe solo las suyas (timing exacto)
    juce::MidiBuffer midiSub;
    int preparedFactor = 1, preparedBlock = 512;
    void prepareVoices();
public:
    float getLimiterReduction() const noexcept { return limiterGr.load(); }
private:
    std::atomic<float> limiterGr { 1.0f };
    cz::BodyResonator bodyRes;
    cz::SympatheticStrings sympathetic;
    cz::Chorus chorus;
    bool pedalWas = false, hostPedal = false;

    // SEQ 2 FOLLOW: nota del SEQ 1 + intervalo (el ritmo sigue siendo del SEQ 2)
    juce::MidiBuffer seqMidi2b;
    int lastSeq1Note = -1, lastFollow = 0;
    bool lastSplit = false;
    std::array<int, 128> followMap {};
    juce::Random mutateRng;
    // Todo lo aleatorio arranca igual en cada reproducción: exportar o congelar en el DAW
    // da siempre el mismo audio (la probabilidad, el random y el MUTATE siguen sonando vivos)
    void reseedRandomness();
    bool lastTransportPlaying = false;

    // Morph
    std::array<std::vector<float>, 2> morphSnap;
    juce::SpinLock morphLock; // el DAW puede guardar el proyecto desde otro hilo
    std::array<bool, 2> morphHas { false, false };
    std::vector<char> morphable;
    float lastMorph = -1.0f;
    std::atomic<bool> morphSkip { true };
    void applyMorph();

    // Macros: desplazamiento normalizado de cada parámetro (lo calcula el audio thread por bloque)
    struct MacroTarget { int param; float amount; };
    std::array<std::vector<MacroTarget>, 4> macroTargets;
    std::array<int, 4> macroIdx {};
    std::unique_ptr<std::atomic<float>[]> macroOff;
    void updateMacros();

    double sampleRateHz = 44100.0;
    double internalPpq = 0.0;
    juce::MidiBuffer seqMidi; // notas generadas por el secuenciador (se tocan y se emiten)
    float modWheel = 0.0f; // CC1: más STRUCTURE (A) y SPREAD (B)
    std::atomic<int> presetIndex { 0 };
    juce::String userPresetName;

    // Memorias de patrón
    std::array<cz::SeqPattern, kNumPatterns> patterns;
    std::atomic<int> activePattern { 0 }, queuedPattern { -1 }, chainLen { 0 }, chainPos { -1 };
    std::array<std::atomic<int>, 16> chainSteps {};
    juce::String chainText;
    std::atomic<bool> metaLoadRequest { false }, transportRunning { false };
    juce::int64 lastCycle = -1;
    cz::SeqPattern clipboard;
    bool hasClipboard = false;
    void syncPatternMeta();
    void setParamValue (juce::StringRef id, float realValue);

    // Patcheo
    struct PatchSlot { std::atomic<int> source { -1 }, dest { -1 }; std::atomic<float> amount { 0.5f }; };
    std::array<PatchSlot, kMaxPatches> patchSlots;
    std::array<std::atomic<float>, NumPatchSources> sourceValue {};
    float modEnv = 0.0f;
    bool modEnvAttack = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (URingAudioProcessor)
};
