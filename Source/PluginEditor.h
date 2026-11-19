#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Presets.h"
#include "GUI/LookAndFeel.h"
#include "GUI/Widgets.h"
#include "GUI/SeqGrid.h"
#include "GUI/Analyzer.h"
#include "GUI/PresetBrowser.h"

class URingAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit URingAudioProcessorEditor (URingAudioProcessor&);
    ~URingAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override { userActed = true; }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override { userActed = true; }
    bool keyPressed (const juce::KeyPress&) override;
    void openPresetBrowser() { browser.show(); }
    void closePresetBrowser() { browser.hide(); }
    bool isPresetBrowserOpen() const { return browser.isVisible(); }

    void showTab (int index);
    void showAbout (bool show) { about.setVisible (show); if (show) about.toFront (false); }

    static constexpr int baseW = 1240, baseH = 800;

    // Para las pruebas automáticas (Tools/Demo.cpp)
    void testSetCableView (int view)                { setCableView (view, false); }
    int testCableView() const                       { return synthPage.cableView; }
    void testHoverJack (int jackId)                 { synthPage.hoverJack = jackId; synthPage.hoverPos = synthPage.jackPositionForId (jackId); synthPage.repaint(); }
    juce::String testHoverText() const              { return synthPage.hoverText(); }
    void testPreview (const juce::String& category) { playPreview (category); }
    bool testPreviewSounding() const                { return previewNote >= 0; }
    void testShowSeqLane (int lane)                 { seqGrid.showLane (lane); }

private:
    struct Section
    {
        juce::String title;
        std::vector<juce::Component*> items;
        juce::Rectangle<int> bounds;
        bool light = false;          // panel plateado (true) = fuente de sonido; negro = control / efectos
        juce::Colour tag {};         // color del motor (franja y LED del título)
        bool spanLast = false;       // el último control, si queda solo en su columna, ocupa la columna entera
        juce::StringArray jacks;     // etiquetas de los jacks
        std::vector<int> jackIds;    // >= 100: salida (fuente 100+n); si no: entrada (destino de modulación)
    };

    // Página de rack: filas de módulos estilo Eurorack. Con controlRows > 1 los módulos son
    // verticales (controles en columnas y jacks abajo); con 1 son horizontales.
    struct Page : public juce::Component
    {
        struct Cable { int rowA, secA; juce::String jackA; int rowB, secB; juce::String jackB; juce::Colour colour; };

        std::vector<std::vector<Section>> rows;
        std::vector<int> rowTop, rowHeight;
        int controlRows = 1;
        std::vector<Cable> cables;

        // Patcheo con cables (solo en la página que tiene `proc`)
        URingAudioProcessor* proc = nullptr;
        std::function<void (const juce::String&, const juce::String&)> onFeedback;
        int dragSource = -1, dragDest = -1;      // extremo fijo del cable que se arrastra
        float dragAmount = 0.5f;
        juce::Point<float> dragPos;
        // Vista de cables: 0 = a la vista, 1 = transparentes, 2 = ocultos (siguen funcionando)
        int cableView = 0;
        // Jack bajo el mouse: globo con lo que tiene conectado
        int hoverJack = -1;
        juce::Point<float> hoverPos;
        juce::String hoverText() const;
        void paintHover (juce::Graphics&);
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;

        void paint (juce::Graphics&) override;
        void paintOverChildren (juce::Graphics&) override;
        void paintCables (juce::Graphics&);
        void layout (int width);
        juce::Point<float> jackCentre (const Section& s, int index) const;
        bool findJack (juce::Point<float> p, int& jackId, juce::Point<float>& centre) const;
        juce::Point<float> jackPositionForId (int jackId) const;
        static juce::String jackName (int jackId);
        static juce::Colour sourceColour (int source);
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    };

    // Teclado en pantalla que también se toca con el teclado de la computadora:
    // A W S E D F T G Y H U J K = notas, Z/X = octava, C/V = velocidad
    struct QwertyKeyboard : public juce::MidiKeyboardComponent
    {
        QwertyKeyboard (juce::MidiKeyboardState& s) : juce::MidiKeyboardComponent (s, horizontalKeyboard), kbState (s) {}
        juce::MidiKeyboardState& kbState;
        bool keyPressed (const juce::KeyPress& key) override;
        std::function<void (const juce::String&, const juce::String&)> onFeedback;
        int octave = 4;
        float velocity = 0.8f;
    };

    struct Canvas : public juce::Component
    {
        std::function<void (juce::Graphics&)> painter;
        void paint (juce::Graphics& g) override { if (painter) painter (g); }
    };

    void addKnob (Page&, Section&, const juce::String& id, const char* title, bool bipolar = false);
    void addChoice (Page&, Section&, const juce::String& id, const char* title);
    void addPad (Page&, Section&, const juce::String& id, const char* text, const char* title);
    void showParam (const juce::String& id);
    void showMidiMenu (const juce::String& id);
    void loadPreset (int index);
    void timerCallback() override;
    void retranslateUi();
    int seenLanguage = -1;

    URingAudioProcessor& proc;
    mm::gui::LookAndFeel lnf;

    Canvas canvas;
    mm::gui::Analyzer fxAnalyzer { proc }, liveAnalyzer { proc }, macroAnalyzer { proc }, texAnalyzer { proc };
    mm::gui::MiniMeter miniMeter { proc };
    std::unique_ptr<mm::gui::XYPad> xyPad;
    mm::gui::AboutOverlay about;
    mm::gui::PresetBrowser browser;
    mm::gui::IconButton undoBtn { mm::gui::IconButton::Icon::Undo }, redoBtn { mm::gui::IconButton::Icon::Redo },
                        browseBtn { mm::gui::IconButton::Icon::Browse }, cablesBtn { mm::gui::IconButton::Icon::Cables },
                        diceBtn { mm::gui::IconButton::Icon::Dice };
    void rollDice (URingAudioProcessor::RandomScope scope, bool vary);
    juce::TextButton* rollButton = nullptr;   // NUEVO SONIDO (módulo AZAR de la pestaña SEQ)
    void showDiceMenu();
    void toggleDiceEveryNote();
    void landDice (int face);
    int diceModeIndex() const;
    mm::gui::Pad* dicePad = nullptr;
    std::unique_ptr<juce::MouseListener> diceHold;   // mantener apretado el dado
    void setCableView (int view, bool announce);
    int previewNote = -1, previewId = 0;
    float ledFlash = 0.0f;   // LED del logo (titila al tempo)
    int lastLedStep = -1;
    void playPreview (const juce::String& category);
    bool userActed = false;
    int undoCounter = 0;
    void doUndo (bool redo);
    struct LogoHotspot : public juce::Component
    {
        std::function<void()> onClick;
        void mouseUp (const juce::MouseEvent& e) override { if (onClick && contains (e.getPosition())) onClick(); }
    } logoHotspot;
    std::array<float, URingAudioProcessor::kScopeSize> lcdWave {};
    Page synthPage, fxPage, seqPage, livePage, macroPage, texturePage;
    std::vector<juce::TextButton*> modelButtons;   // los 24 modelos del motor C
    mm::gui::SeqGrid seqGrid;
    mm::gui::Lcd lcd;
    QwertyKeyboard keyboard;
    juce::TextButton saveBtn { "SAVE" };
    void populatePresetMenu();
    void saveUserPresetDialog();
    std::unique_ptr<juce::AlertWindow> saveDialog;
    juce::TextButton synthTab { "SYNTH" }, fxTab { "FX" }, seqTab { "SEQ" }, liveTab { "LIVE" }, macroTab { "MACRO" }, textureTab { "TEXTURE" }, prevPreset { "<" }, nextPreset { ">" };
    juce::ComboBox presetBox;

    // ---- REC de automatización (clic = grabar, clic derecho = lista de lo grabado / borrar)
    struct RecButton : public juce::TextButton
    {
        std::function<void()> onMenu;
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu()) { if (onMenu) onMenu(); return; }
            juce::TextButton::mouseDown (e);
        }
    };
    RecButton recBtn;
    // Selector de qué secuenciador se edita en la grilla (SEQ 1 → motor A, SEQ 2 → motor B)
    struct SeqSelect : public juce::Component
    {
        SeqSelect();
        void resized() override;
        void setSelected (int seq);
        std::function<void (int)> onSelect;
        juce::TextButton one { "SEQ 1  A" }, two { "SEQ 2  B" };
        juce::Label label;
    };
    SeqSelect* seqSelect = nullptr;
    int lastRecState = 0;
    juce::ComboBox recLenBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> recAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> recLenAttach;
    std::vector<std::pair<juce::Slider*, int>> knobParams;   // perilla → índice de parámetro (indicador cian)
    int lastRecShown = -1, blink = 0;
    juce::uint32 quietUntil = 0;   // al cargar un preset las perillas se mueven solas: el LCD muestra el preset, no cada perilla
    void showAutomationMenu();

    std::vector<std::unique_ptr<juce::Component>> controls;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (URingAudioProcessorEditor)
};
