#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "LookAndFeel.h"

namespace mm::gui
{

using APVTS = juce::AudioProcessorValueTreeState;

// Slider que reserva el clic derecho para un menú (MIDI Learn) en vez de moverse
class LearnSlider : public juce::Slider
{
public:
    std::function<void()> onRightClick;

    void mouseDown (const juce::MouseEvent& e) override
    {
        rightClick = e.mods.isPopupMenu();
        if (rightClick) { if (onRightClick) onRightClick(); return; }
        juce::Slider::mouseDown (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override { if (! rightClick) juce::Slider::mouseDrag (e); }
    void mouseUp (const juce::MouseEvent& e) override   { if (! rightClick) juce::Slider::mouseUp (e); }

private:
    bool rightClick = false;
};

// Encoder con etiqueta, ligado a un parámetro del APVTS
class Knob : public juce::Component, private juce::Slider::Listener, private juce::Timer
{
public:
    Knob (APVTS& state, const juce::String& id, const juce::String& title,
          bool bipolar = false, juce::Colour colour = Palette::accent)
        : paramId (id), titleText (title), param (state.getParameter (id))
    {
        using namespace juce;
        slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
        slider.setMouseDragSensitivity (220);
        slider.getProperties().set ("bipolar", bipolar);
        slider.setColour (Slider::rotarySliderFillColourId, colour);
        addAndMakeVisible (slider);

        label.setText (mm::lang::tr (title), dontSendNotification);
        label.setJustificationType (Justification::centred);
        label.setFont (uiFont (11.5f));
        label.setColour (Label::textColourId, Palette::dim);
        label.setInterceptsMouseClicks (false, false);
        label.setMinimumHorizontalScale (0.55f);
        addAndMakeVisible (label);

        attachment = std::make_unique<APVTS::SliderAttachment> (state, id, slider);
        slider.addListener (this);

        if (auto* p = state.getParameter (id))
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromBottom (17));
        slider.setBounds (r);
    }

    const juce::String paramId;
    LearnSlider slider;
    juce::Label label;

    void retranslate() { if (! dragging && ! isTimerRunning()) label.setText (mm::lang::tr (titleText), juce::dontSendNotification); }
    const juce::String& getTitle() const noexcept { return titleText; }

private:
    // Mientras se gira, el rótulo muestra el valor (en el color de acento) y vuelve al nombre al soltar
    void showValue()
    {
        if (param == nullptr) return;
        label.setText (param->getCurrentValueAsText(), juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, Palette::accent);
    }
    void sliderValueChanged (juce::Slider*) override { if (dragging) showValue(); }
    void sliderDragStarted (juce::Slider*) override  { dragging = true; stopTimer(); showValue(); }
    void sliderDragEnded (juce::Slider*) override    { dragging = false; startTimer (900); }
    void timerCallback() override
    {
        stopTimer();
        label.setText (mm::lang::tr (titleText), juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, slider.getProperties()["light"] ? juce::Colour (0xff1a1b1e) : Palette::dim);
    }

    juce::String titleText;
    juce::RangedAudioParameter* param = nullptr;
    bool dragging = false;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

// Selector (parámetro de tipo choice)
class ChoiceBox : public juce::Component
{
public:
    ChoiceBox (APVTS& state, const juce::String& id, const juce::String& title)
        : paramId (id), titleText (title)
    {
        using namespace juce;
        if (auto* c = dynamic_cast<AudioParameterChoice*> (state.getParameter (id)))
        {
            choices = c->choices;
            for (int i = 0; i < choices.size(); ++i) box.addItem (mm::lang::tr (choices[i]), i + 1);
        }
        addAndMakeVisible (box);

        label.setText (mm::lang::tr (title), dontSendNotification);
        label.setJustificationType (Justification::centred);
        label.setFont (uiFont (10.5f));
        label.setColour (Label::textColourId, Palette::dim);
        addAndMakeVisible (label);

        attachment = std::make_unique<APVTS::ComboBoxAttachment> (state, id, box);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromBottom (16));
        box.setBounds (r.withSizeKeepingCentre (r.getWidth(), 24));
    }

    const juce::String paramId;
    juce::ComboBox box;
    juce::Label label;

    void retranslate()
    {
        label.setText (mm::lang::tr (titleText), juce::dontSendNotification);
        for (int i = 0; i < choices.size(); ++i) box.changeItemText (i + 1, mm::lang::tr (choices[i]));
        box.setText (box.getItemText (box.getSelectedItemIndex()), juce::dontSendNotification);
    }

private:
    juce::String titleText;
    juce::StringArray choices;
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
};

// Pad retroiluminado on/off (parámetro bool)
class Pad : public juce::Component
{
public:
    Pad (APVTS& state, const juce::String& id, const juce::String& text, const juce::String& title,
         juce::Colour onColour = Palette::accent)
        : paramId (id), buttonText (text), titleText (title)
    {
        using namespace juce;
        button.setButtonText (mm::lang::tr (text));
        button.setClickingTogglesState (true);
        button.setColour (TextButton::buttonOnColourId, onColour);
        addAndMakeVisible (button);

        label.setText (mm::lang::tr (title), dontSendNotification);
        label.setJustificationType (Justification::centred);
        label.setFont (uiFont (10.5f));
        label.setColour (Label::textColourId, Palette::dim);
        addAndMakeVisible (label);

        attachment = std::make_unique<APVTS::ButtonAttachment> (state, id, button);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromBottom (16));
        button.setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth(), 64), 30));
        if (dice != nullptr) dice->setBounds (button.getBounds());
    }

    float diceHold = 0.0f;   // 0..1 mientras se mantiene apretado el dado
    int diceFace = 1;        // cara que se ve (1..6)
    juce::MouseListener* holdListener = nullptr;   // (pruebas) el que maneja toque / mantener
    void repaintDice() { repaint(); if (dice != nullptr) dice->repaint(); }

    // Botón con un dado dibujado en vez de texto (SORTEAR)
    void showDice()
    {
        struct DiceFace : juce::Component
        {
            juce::Button* b = nullptr;
            void paint (juce::Graphics& g) override
            {
                using namespace juce;
                const bool on = b->getToggleState();
                const auto c = getLocalBounds().toFloat().getCentre();
                // mantener apretado: una barra se llena abajo hasta que cambia el modo
                if (auto* pd = dynamic_cast<Pad*> (getParentComponent()); pd != nullptr && pd->diceHold > 0.0f)
                {
                    auto bar = getLocalBounds().toFloat().reduced (4.0f, 0.0f).removeFromBottom (3.0f).translated (0.0f, -2.0f);
                    g.setColour ((on ? Colour (0xff1a1b1e) : Palette::accent).withAlpha (0.9f));
                    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * pd->diceHold), 1.5f);
                }
                const float s = jmin (20.0f, (float) getHeight() - 8.0f);
                const auto d = Rectangle<float> (s, s).withCentre (c);
                g.setColour (on ? Colour (0xff1a1b1e) : Palette::text.withAlpha (b->isOver() ? 1.0f : 0.8f));
                g.drawRoundedRectangle (d, s * 0.22f, 1.7f);
                const float o = s * 0.26f, rr = s * 0.095f;
                const int face = [this] { auto* pd = dynamic_cast<Pad*> (getParentComponent()); return pd != nullptr ? juce::jlimit (1, 6, pd->diceFace) : 5; }();
                std::vector<Point<float>> pips;
                if (face % 2 == 1) pips.push_back ({ 0, 0 });
                if (face >= 2) { pips.push_back ({ -o, -o }); pips.push_back ({ o, o }); }
                if (face >= 4) { pips.push_back ({ o, -o }); pips.push_back ({ -o, o }); }
                if (face == 6) { pips.push_back ({ -o, 0 }); pips.push_back ({ o, 0 }); }
                for (auto p : pips)
                    g.fillEllipse (c.x + p.x - rr, c.y + p.y - rr, rr * 2.0f, rr * 2.0f);
            }
        };
        auto f = std::make_unique<DiceFace>();
        f->b = &button;
        f->setInterceptsMouseClicks (false, false);
        addAndMakeVisible (*f);
        button.onStateChange = [d = f.get()] { d->repaint(); };
        dice = std::move (f);
        resized();
    }

    const juce::String paramId;
    juce::TextButton button;
    juce::Label label;

    void retranslate()
    {
        button.setButtonText (mm::lang::tr (buttonText));
        label.setText (mm::lang::tr (titleText), juce::dontSendNotification);
    }

private:
    juce::String buttonText, titleText;
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
    std::unique_ptr<juce::Component> dice;
};

// Display LCD de 2 líneas
class Lcd : public juce::Component
{
public:
    // Mensaje (valor de una perilla, preset, MIDI...): se muestra 3 s y vuelve al modo en vivo
    void setText (const juce::String& top, const juce::String& bottom)
    {
        line1 = mm::lang::ui (top).toUpperCase().substring (0, 26);
        line2 = mm::lang::ui (bottom).toUpperCase().substring (0, 22);
        shownAt = juce::Time::getMillisecondCounter();
        hasMessage = true;
        repaint();
    }

    // Modo en vivo: nombre del preset arriba y la forma de onda de la salida abajo
    void setIdle (const juce::String& title, const float* wave, int n)
    {
        if (hasMessage && juce::Time::getMillisecondCounter() - shownAt > 3000) hasMessage = false;
        idleTitle = title.toUpperCase().substring (0, 26);
        samples.assign (wave, wave + n);
        if (! hasMessage) repaint();
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        const auto r = getLocalBounds().toFloat();
        g.setColour (Colours::black.withAlpha (0.8f));
        g.fillRoundedRectangle (r, 4.0f);
        auto inner = r.reduced (3.0f);
        g.setGradientFill (ColourGradient (Colour (0xff111316), inner.getX(), inner.getY(),
                                           Colour (0xff07080a), inner.getX(), inner.getBottom(), false));
        g.fillRoundedRectangle (inner, 2.5f);
        // reflejo del vidrio
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.06f), inner.getX(), inner.getY(),
                                           Colours::transparentWhite, inner.getX(), inner.getCentreY(), false));
        g.fillRoundedRectangle (inner.withHeight (inner.getHeight() * 0.5f), 2.5f);
        g.setColour (Colours::white.withAlpha (0.05f));
        g.drawHorizontalLine ((int) r.getBottom() - 1, r.getX() + 4.0f, r.getRight() - 4.0f);

        inner = inner.reduced (10.0f, 4.0f);
        const auto mono = Font::getDefaultMonospacedFontName();
        const bool idle = ! hasMessage && ! samples.empty();
        g.setColour (Palette::dim);
        g.setFont (Font (FontOptions (mono, 10.5f, Font::plain)));
        g.drawText (idle ? idleTitle : line1, inner.removeFromTop (inner.getHeight() * 0.42f), Justification::bottomLeft, false);
        if (! idle)
        {
            g.setColour (Palette::accent);
            g.setFont (Font (FontOptions (mono, 15.0f, Font::bold)));
            g.drawText (mm::lang::tr (line2), inner, Justification::centredLeft, false);
            return;
        }
        // forma de onda (envolvente por columna, estilo grabador)
        const int cols = juce::jmax (1, (int) (inner.getWidth() / 2.0f));
        const int per = juce::jmax (1, (int) samples.size() / cols);
        const float mid = inner.getCentreY(), half = inner.getHeight() * 0.5f;
        g.setColour (Palette::accent.withAlpha (0.25f));
        g.drawHorizontalLine ((int) mid, inner.getX(), inner.getRight());
        g.setColour (Palette::accent);
        for (int c = 0; c < cols; ++c)
        {
            float pk = 0.0f;
            for (int i = c * per; i < (c + 1) * per && i < (int) samples.size(); ++i) pk = juce::jmax (pk, std::abs (samples[(size_t) i]));
            const float h = juce::jlimit (0.5f, half, std::sqrt (pk) * half * 1.4f);
            g.fillRect (inner.getX() + (float) c * 2.0f, mid - h, 1.2f, h * 2.0f);
        }
    }

private:
    juce::String line1 { "CCRING" }, line2 { "PHYSICAL MODELS READY" }, idleTitle;
    std::vector<float> samples;
    juce::uint32 shownAt = 0;
    bool hasMessage = false;
};

} // namespace mm::gui
