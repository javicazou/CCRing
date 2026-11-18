#pragma once

// Pantallas "en vivo" de URing: analizador de salida (osciloscopio + espectro + vúmetro),
// vúmetro chico de la barra superior, pad X/Y para tocar el MORPH y el panel de créditos.
#include <juce_dsp/juce_dsp.h>
#include "LookAndFeel.h"
#include "../PluginProcessor.h"
#include "../Links.h"
#include <array>

namespace mm::gui
{

// Display hundido (vidrio oscuro) con grilla fina
inline void drawDisplay (juce::Graphics& g, juce::Rectangle<float> r)
{
    using namespace juce;
    g.setColour (Colours::black.withAlpha (0.85f));
    g.fillRoundedRectangle (r, 4.0f);
    auto in = r.reduced (2.0f);
    g.setGradientFill (ColourGradient (Colour (0xff121417), in.getX(), in.getY(), Colour (0xff07080a), in.getX(), in.getBottom(), false));
    g.fillRoundedRectangle (in, 3.0f);
    g.setColour (Colours::white.withAlpha (0.05f));
    g.drawHorizontalLine ((int) r.getBottom() - 1, r.getX() + 4.0f, r.getRight() - 4.0f);
}

// Vúmetro estéreo vertical con escala en dB, retención de pico y LED de saturación
class StereoMeter
{
public:
    void update (float pl, float pr)
    {
        const float in[2] { pl, pr };
        for (int c = 0; c < 2; ++c)
        {
            const float db = juce::Decibels::gainToDecibels (in[c], -60.0f);
            level[c] = db > level[c] ? db : juce::jmax (-60.0f, level[c] - 1.2f);   // caída ~36 dB/s a 30 Hz
            if (db >= hold[c]) { hold[c] = db; holdAge[c] = 0; }
            else if (++holdAge[c] > 45) hold[c] = juce::jmax (-60.0f, hold[c] - 1.5f);
            if (in[c] >= 0.995f) clipAge[c] = 60;
            else if (clipAge[c] > 0) --clipAge[c];
        }
    }

    void draw (juce::Graphics& g, juce::Rectangle<float> r, bool scale) const
    {
        using namespace juce;
        drawDisplay (g, r);
        auto in = r.reduced (scale ? 5.0f : 3.0f, 4.0f);
        if (scale) in.removeFromLeft (16.0f);
        auto ledArea = in.removeFromTop (5.0f);
        in.removeFromTop (2.0f);
        auto toY = [&] (float db) { return jmap (jlimit (-60.0f, 0.0f, db), -60.0f, 0.0f, in.getBottom(), in.getY()); };
        if (scale)
        {
            g.setFont (uiFont (7.5f));
            for (int db : { 0, -6, -12, -24, -48 })
            {
                g.setColour (Palette::dim.withAlpha (0.7f));
                g.drawText (String (db), Rectangle<float> (r.getX() + 2.0f, toY ((float) db) - 5.0f, 17.0f, 10.0f), Justification::centredRight);
            }
        }
        const float gap = 2.0f, w = (in.getWidth() - gap) * 0.5f;
        for (int c = 0; c < 2; ++c)
        {
            const auto bar = Rectangle<float> (in.getX() + (float) c * (w + gap), in.getY(), w, in.getHeight());
            g.setColour (Colours::white.withAlpha (0.04f));
            g.fillRect (bar);
            const float top = toY (level[c]);
            ColourGradient grad (Colour (0xffff3a2a), bar.getX(), toY (0.0f), Colour (0xff3fd8a0), bar.getX(), bar.getBottom(), false);
            grad.addColour (0.12, Colour (0xffffb020));
            grad.addColour (0.3, Colour (0xff7be3a5));
            g.setGradientFill (grad);
            g.fillRect (bar.withTop (top));
            // segmentos (look de LED)
            g.setColour (Colour (0xff07080a));
            for (float y = bar.getBottom() - 3.0f; y > bar.getY(); y -= 3.0f) g.fillRect (bar.getX(), y, bar.getWidth(), 1.0f);
            if (hold[c] > -59.0f)
            {
                g.setColour (Colours::white.withAlpha (0.85f));
                g.fillRect (bar.getX(), toY (hold[c]) - 1.0f, bar.getWidth(), 1.5f);
            }
            const auto led = Rectangle<float> (bar.getX(), ledArea.getY(), bar.getWidth(), ledArea.getHeight());
            g.setColour (clipAge[c] > 0 ? Colour (0xffff3030) : Colour (0xff3a1514));
            g.fillRoundedRectangle (led, 1.5f);
        }
    }

private:
    float level[2] { -60.0f, -60.0f }, hold[2] { -60.0f, -60.0f };
    int holdAge[2] {}, clipAge[2] {};
};

// Módulo "OUTPUT": osciloscopio (estable, dispara en el cruce por cero), espectro con
// escala logarítmica y vúmetro estéreo. Lee la salida del procesador sin bloquear el audio.
class Analyzer : public juce::Component, private juce::Timer
{
public:
    explicit Analyzer (URingAudioProcessor& p, juce::String panelTitle = "OUTPUT  /  ANALYZER")
        : proc (p), title (std::move (panelTitle))
    {
        setInterceptsMouseClicks (false, false);
        spectrum.fill (-100.0f);
        peakHold.fill (-100.0f);
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        const auto b = getLocalBounds().toFloat();
        drawModulePanel (g, b, mm::lang::tr (title), false, {}, Palette::text.withAlpha (0.6f));
        auto area = b.reduced (16.0f, 0.0f).withTrimmedTop (28.0f).withTrimmedBottom (14.0f);
        const bool compact = area.getHeight() < 160.0f;
        auto meterR = area.removeFromRight (compact ? 46.0f : 58.0f);
        area.removeFromRight (10.0f);
        auto scopeR = area.removeFromLeft (area.getWidth() * 0.36f);
        area.removeFromLeft (10.0f);
        drawScope (g, scopeR);
        drawSpectrum (g, area);
        meter.draw (g, meterR, true);
    }

private:
    // Visible si él y todos sus padres están visibles (la pestaña de atrás no gasta CPU)
    bool onScreen() const
    {
        for (auto* c = static_cast<const juce::Component*> (this); c->getParentComponent() != nullptr; c = c->getParentComponent())
            if (! c->isVisible()) return false;   // la ventana del plugin la maneja el DAW
        return true;
    }

    void timerCallback() override
    {
        if (! onScreen()) return;
        proc.readScope (samples.data(), (int) samples.size());
        meter.update (proc.takePeak (1, 0), proc.takePeak (1, 1));

        // Espectro: ventana de Hann, magnitud en dB, caída suave y pico retenido
        std::fill (fftData.begin(), fftData.end(), 0.0f);
        for (int i = 0; i < kFftSize; ++i)
            fftData[(size_t) i] = samples[(size_t) (URingAudioProcessor::kScopeSize - kFftSize + i)] * window[(size_t) i];
        fft.performFrequencyOnlyForwardTransform (fftData.data());
        for (int i = 0; i < kFftSize / 2; ++i)
        {
            const float db = juce::Decibels::gainToDecibels (fftData[(size_t) i] * 4.0f / (float) kFftSize, -100.0f);
            auto& s = spectrum[(size_t) i];
            s = db > s ? s + 0.6f * (db - s) : s - 1.4f;
            auto& h = peakHold[(size_t) i];
            h = s > h ? s : h - 0.35f;
        }
        repaint();
    }

    void drawScope (juce::Graphics& g, juce::Rectangle<float> r)
    {
        using namespace juce;
        drawDisplay (g, r);
        auto in = r.reduced (6.0f);
        g.setColour (Colours::white.withAlpha (0.06f));
        for (int i = 1; i < 4; ++i) g.drawHorizontalLine ((int) (in.getY() + in.getHeight() * (float) i / 4.0f), in.getX(), in.getRight());
        for (int i = 1; i < 8; ++i) g.drawVerticalLine ((int) (in.getX() + in.getWidth() * (float) i / 8.0f), in.getY(), in.getBottom());
        g.setColour (Colours::white.withAlpha (0.12f));
        g.drawHorizontalLine ((int) in.getCentreY(), in.getX(), in.getRight());

        // ventana de 1024 samples que empieza en un cruce por cero ascendente (imagen quieta)
        constexpr int span = 1024;
        const int last = URingAudioProcessor::kScopeSize - span;
        int start = last;
        for (int i = last; i > last - 2048 && i > 1; --i)
            if (samples[(size_t) i - 1] < 0.0f && samples[(size_t) i] >= 0.0f) { start = i; break; }
        float pk = 0.02f;
        for (int i = 0; i < span; ++i) pk = jmax (pk, std::abs (samples[(size_t) (start + i)]));
        const float gain = 0.9f / pk;   // autoescala: siempre se ve la forma de onda
        Path p;
        for (int i = 0; i < span; ++i)
        {
            const float x = in.getX() + in.getWidth() * (float) i / (float) (span - 1);
            const float y = in.getCentreY() - samples[(size_t) (start + i)] * gain * in.getHeight() * 0.5f;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (Palette::accent.withAlpha (0.25f));
        g.strokePath (p, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Palette::accent.brighter (0.3f));
        g.strokePath (p, PathStrokeType (1.4f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Palette::dim.withAlpha (0.8f));
        g.setFont (uiFont (8.5f));
        g.drawText (mm::lang::tr ("SCOPE"), in.reduced (2.0f), Justification::topLeft);
        g.drawText (String (Decibels::gainToDecibels (pk), 1) + " dB", in.reduced (2.0f), Justification::topRight);
    }

    void drawSpectrum (juce::Graphics& g, juce::Rectangle<float> r)
    {
        using namespace juce;
        drawDisplay (g, r);
        auto in = r.reduced (6.0f);
        auto labels = in.removeFromBottom (12.0f);
        const double sr = juce::jmax (8000.0, proc.getSampleRate());
        auto xOf = [&] (double hz) { return in.getX() + in.getWidth() * (float) (std::log (hz / 20.0) / std::log (1000.0)); };
        auto yOf = [&] (float db) { return jmap (jlimit (-96.0f, 0.0f, db), -96.0f, 0.0f, in.getBottom(), in.getY()); };

        g.setFont (uiFont (8.0f));
        for (double hz : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
        {
            const float x = xOf (hz);
            g.setColour (Colours::white.withAlpha (0.06f));
            g.drawVerticalLine ((int) x, in.getY(), in.getBottom());
            g.setColour (Palette::dim.withAlpha (0.6f));
            g.drawText (hz >= 1000.0 ? String ((int) (hz / 1000.0)) + "k" : String ((int) hz),
                        Rectangle<float> (x - 15.0f, labels.getY(), 30.0f, 12.0f), Justification::centred);
        }
        for (float db : { -24.0f, -48.0f, -72.0f })
        {
            g.setColour (Colours::white.withAlpha (0.06f));
            g.drawHorizontalLine ((int) yOf (db), in.getX(), in.getRight());
            g.setColour (Palette::dim.withAlpha (0.5f));
            g.drawText (String ((int) db), Rectangle<float> (in.getRight() - 26.0f, yOf (db) - 11.0f, 24.0f, 10.0f), Justification::centredRight);
        }

        // Curva: un punto por píxel (máximo de los bins que caen en ese píxel)
        Path fill, line, hold;
        const int px = jmax (2, (int) in.getWidth());
        for (int k = 0; k <= px; ++k)
        {
            const double f0 = 20.0 * std::pow (1000.0, (double) k / px), f1 = 20.0 * std::pow (1000.0, (double) (k + 1) / px);
            int b0 = (int) (f0 * kFftSize / sr), b1 = jmax (b0, (int) (f1 * kFftSize / sr));
            b0 = jlimit (1, kFftSize / 2 - 1, b0); b1 = jlimit (1, kFftSize / 2 - 1, b1);
            float v = -100.0f, h = -100.0f;
            const double fb = f0 * kFftSize / sr;
            if (f1 * kFftSize / sr - fb < 1.0)   // graves: menos de un bin por píxel -> interpolación (sin escalones)
            {
                const int i0 = jlimit (1, kFftSize / 2 - 2, (int) fb);
                const float fr = (float) jlimit (0.0, 1.0, fb - i0);
                auto lerp = [fr, i0] (const auto& a) { return a[(size_t) i0] + fr * (a[(size_t) i0 + 1] - a[(size_t) i0]); };
                v = lerp (spectrum); h = lerp (peakHold);
            }
            else
                for (int bi = b0; bi <= b1; ++bi) { v = jmax (v, spectrum[(size_t) bi]); h = jmax (h, peakHold[(size_t) bi]); }
            const float x = in.getX() + (float) k * in.getWidth() / (float) px;
            if (k == 0) { line.startNewSubPath (x, yOf (v)); hold.startNewSubPath (x, yOf (h)); fill.startNewSubPath (x, in.getBottom()); }
            else { line.lineTo (x, yOf (v)); hold.lineTo (x, yOf (h)); }
            fill.lineTo (x, yOf (v));
        }
        fill.lineTo (in.getRight(), in.getBottom());
        fill.closeSubPath();
        g.setGradientFill (ColourGradient (Palette::accent.withAlpha (0.45f), 0.0f, in.getY(), Palette::accent.withAlpha (0.03f), 0.0f, in.getBottom(), false));
        g.fillPath (fill);
        g.setColour (Palette::accent.brighter (0.2f));
        g.strokePath (line, PathStrokeType (1.3f));
        g.setColour (Colours::white.withAlpha (0.28f));
        g.strokePath (hold, PathStrokeType (0.8f));
        g.setColour (Palette::dim.withAlpha (0.8f));
        g.setFont (uiFont (8.5f));
        g.drawText (mm::lang::tr ("SPECTRUM"), in.reduced (2.0f), Justification::topLeft);
    }

    static constexpr int kFftOrder = 12, kFftSize = 1 << kFftOrder;
    URingAudioProcessor& proc;
    juce::String title;
    juce::dsp::FFT fft { kFftOrder };
    std::array<float, URingAudioProcessor::kScopeSize> samples {};
    std::array<float, kFftSize * 2> fftData {};
    std::array<float, kFftSize / 2> spectrum {}, peakHold {};
    const std::array<float, kFftSize> window = []
    {
        std::array<float, kFftSize> w {};
        for (int i = 0; i < kFftSize; ++i) w[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (kFftSize - 1));
        return w;
    }();
    StereoMeter meter;
};

// Vúmetro chico de la barra superior (siempre visible)
class MiniMeter : public juce::Component, private juce::Timer
{
public:
    explicit MiniMeter (URingAudioProcessor& p) : proc (p) { startTimerHz (30); }
    void paint (juce::Graphics& g) override { meter.draw (g, getLocalBounds().toFloat(), false); }
private:
    void timerCallback() override
    {
        meter.update (proc.takePeak (0, 0), proc.takePeak (0, 1));
        repaint();
    }
    URingAudioProcessor& proc;
    StereoMeter meter;
};

// Pad X/Y: X = MORPH (A ↔ B), Y = un macro. Se arrastra con el mouse; doble clic = centro.
class XYPad : public juce::Component
{
public:
    XYPad (juce::AudioProcessorValueTreeState& s, juce::String xId, juce::String yId, juce::String xName, juce::String yName)
        : state (s), xParam (s.getParameter (xId)), yParam (s.getParameter (yId)), xLabel (std::move (xName)), yLabel (std::move (yName)) {}

    std::function<void (const juce::String&, const juce::String&)> onFeedback;

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        const auto b = getLocalBounds().toFloat();
        drawModulePanel (g, b, mm::lang::tr ("PERFORM  /  X-Y"), false, {}, Palette::accent);
        const auto pad = padArea();
        drawDisplay (g, pad);
        auto in = pad.reduced (4.0f);
        g.setColour (Colours::white.withAlpha (0.05f));
        for (int i = 1; i < 8; ++i)
        {
            g.drawVerticalLine ((int) (in.getX() + in.getWidth() * (float) i / 8.0f), in.getY(), in.getBottom());
            g.drawHorizontalLine ((int) (in.getY() + in.getHeight() * (float) i / 8.0f), in.getX(), in.getRight());
        }
        const float x = xParam != nullptr ? xParam->getValue() : 0.5f, y = yParam != nullptr ? yParam->getValue() : 0.5f;
        const Point<float> p (in.getX() + x * in.getWidth(), in.getBottom() - y * in.getHeight());
        g.setColour (Palette::accent.withAlpha (0.35f));
        g.drawVerticalLine ((int) p.x, in.getY(), in.getBottom());
        g.drawHorizontalLine ((int) p.y, in.getX(), in.getRight());
        for (float rr : { 26.0f, 16.0f })
        {
            g.setColour (Palette::accent.withAlpha (rr > 20.0f ? 0.12f : 0.25f));
            g.fillEllipse (p.x - rr, p.y - rr, rr * 2.0f, rr * 2.0f);
        }
        g.setColour (Palette::accent);
        g.fillEllipse (p.x - 7.0f, p.y - 7.0f, 14.0f, 14.0f);
        g.setColour (Colours::white.withAlpha (0.8f));
        g.fillEllipse (p.x - 2.5f, p.y - 4.0f, 4.0f, 3.0f);

        g.setFont (uiFont (9.5f));
        g.setColour (Palette::accent);
        g.drawText ("A", in.reduced (8.0f), Justification::bottomLeft);
        g.setColour (Colour (0xff3fd8ff));
        g.drawText ("B", in.reduced (8.0f), Justification::bottomRight);
        g.setColour (Palette::dim);
        g.drawText (xLabel + "  " + String (roundToInt (x * 100.0f)) + "%", in.reduced (8.0f), Justification::centredBottom);
        g.drawText (yLabel + "  " + String (roundToInt ((y * 2.0f - 1.0f) * 100.0f)) + "%", in.reduced (8.0f), Justification::centredTop);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (auto* p : { xParam, yParam }) if (p != nullptr) p->beginChangeGesture();
        dragging = true;
        mouseDrag (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto in = padArea().reduced (4.0f);
        const float x = juce::jlimit (0.0f, 1.0f, (e.position.x - in.getX()) / in.getWidth());
        const float y = juce::jlimit (0.0f, 1.0f, (in.getBottom() - e.position.y) / in.getHeight());
        if (xParam != nullptr) xParam->setValueNotifyingHost (x);
        if (yParam != nullptr) yParam->setValueNotifyingHost (y);
        if (onFeedback) onFeedback (xLabel + " / " + yLabel, juce::String (juce::roundToInt (x * 100.0f)) + "%  /  "
                                                              + juce::String (juce::roundToInt ((y * 2.0f - 1.0f) * 100.0f)) + "%");
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (! dragging) return;
        dragging = false;
        for (auto* p : { xParam, yParam }) if (p != nullptr) p->endChangeGesture();
    }
    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        for (auto* p : { xParam, yParam })
            if (p != nullptr) { p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture(); }
        repaint();
    }

    // El editor llama esto en su timer: sigue a la automatización / presets
    void refresh()
    {
        const float x = xParam != nullptr ? xParam->getValue() : 0.0f, y = yParam != nullptr ? yParam->getValue() : 0.0f;
        if (std::abs (x - lastX) > 1.0e-4f || std::abs (y - lastY) > 1.0e-4f) { lastX = x; lastY = y; repaint(); }
    }

private:
    juce::Rectangle<float> padArea() const { return getLocalBounds().toFloat().reduced (16.0f, 0.0f).withTrimmedTop (28.0f).withTrimmedBottom (16.0f); }
    juce::AudioProcessorValueTreeState& state;
    juce::RangedAudioParameter* xParam;
    juce::RangedAudioParameter* yParam;
    juce::String xLabel, yLabel;
    bool dragging = false;
    float lastX = -1.0f, lastY = -1.0f;
};

// Panel "acerca de": se abre tocando el logo, se cierra con un clic
class AboutOverlay : public juce::Component
{
public:
    AboutOverlay() { setVisible (false); }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        g.fillAll (Colours::black.withAlpha (0.72f));
        const auto card = cardArea();
        drawModulePanel (g, card, {}, false, {}, Palette::accent);
        auto r = card.reduced (40.0f, 30.0f);

        g.setColour (Palette::accent.withAlpha (0.35f));
        g.fillEllipse (r.getX(), r.getY() + 8.0f, 22.0f, 22.0f);
        g.setColour (Palette::accent);
        g.fillEllipse (r.getX() + 5.0f, r.getY() + 13.0f, 12.0f, 12.0f);
        g.setColour (Palette::text);
        g.setFont (uiFont (34.0f).withExtraKerningFactor (0.25f));
        g.drawText ("CCRING", r.withTrimmedLeft (34.0f).removeFromTop (40.0f), Justification::centredLeft);
        r.removeFromTop (44.0f);
        g.setColour (Palette::dim);
        g.setFont (uiFont (11.0f).withExtraKerningFactor (0.2f));
        g.drawText (String::fromUTF8 (mm::lang::spanish() ? "CCR   \xc2\xb7   VERSI\xc3\x93N " : "CCR   \xc2\xb7   VERSION ") + URING_VERSION,
                    r.removeFromTop (18.0f), Justification::centredLeft);

        // Idioma: ENGLISH / ESPAÑOL (se guarda para todas las ventanas de URing)
        g.setColour (Palette::dim);
        g.setFont (uiFont (9.0f).withExtraKerningFactor (0.15f));
        g.drawText (mm::lang::tr ("LANGUAGE"), langArea (0).withX (langArea (0).getX() - 90.0f).withWidth (84.0f), Justification::centredRight);
        for (int i = 0; i < 2; ++i)
        {
            const bool sel = (int) mm::lang::current() == i;
            const auto a = langArea (i);
            g.setColour (sel ? Palette::accent : Palette::dim.withAlpha (a.contains (hover) ? 0.4f : 0.2f));
            g.fillRoundedRectangle (a, 3.0f);
            g.setColour (sel ? Colours::black : Palette::text);
            g.setFont (uiFont (9.5f).withExtraKerningFactor (0.1f));
            g.drawText (i == 0 ? String ("ENGLISH") : String::fromUTF8 ("ESPA\xc3\x91OL"), a, Justification::centred);
        }
        r.removeFromTop (18.0f);

        auto block = [&] (const String& head, const String& body, int lines)
        {
            g.setColour (Palette::accent);
            g.setFont (uiFont (10.0f).withExtraKerningFactor (0.15f));
            g.drawText (mm::lang::tr (head), r.removeFromTop (16.0f), Justification::centredLeft);
            g.setColour (Palette::text.withAlpha (0.85f));
            g.setFont (uiFont (10.5f));
            g.drawFittedText (mm::lang::tr (body), r.removeFromTop (16.0f * (float) lines).toNearestInt(), Justification::topLeft, lines);
            r.removeFromTop (10.0f);
        };
        block ("ENGINES", "A  Resonator (modal, string, piano, tine, reed)    B  Impact (bars, plates, bells, drums)\n"
                          "C  Macro: 24 synthesis models    + granular TEXTURE, plate reverb, 7 filter types", 2);
        block ("CREDITS", String::fromUTF8 ("Engine C, TEXTURE and MARBLES use Plaits, Clouds and Marbles by Mutable Instruments\n"
                                            "(\xc3\x89milie Gillet), MIT license. Typeface: Montserrat, SIL Open Font License."), 2);
        block ("COMPUTER KEYBOARD", "A W S E D F T G Y H U J K = notes     Z / X = octave     C / V = velocity\n"
                                    "Right-click a knob: MIDI learn     Double-click: default value", 2);
        block ("FREE & OPEN SOURCE", "CCRing is free software (GNU AGPL-3.0). If it makes your music better,\n"
                                     "a small donation helps keep it alive and growing.", 2);
        g.setColour (Palette::text.withAlpha (0.85f));
        g.setFont (uiFont (10.5f));
        g.drawText (mm::lang::tr ("Mercado Pago (Argentina): alias") + " " + links::mercadoPagoAlias + "   " + mm::lang::tr ("(the button copies it)"), r.removeFromTop (16.0f), Justification::centredLeft);

        for (const auto& b : buttons())
        {
            const bool live = juce::String (b.url).isNotEmpty() || juce::String (b.copy).isNotEmpty();
            const bool over = live && b.area.contains (hover);
            g.setColour (live ? Palette::accent.withAlpha (over ? 1.0f : 0.85f) : Palette::dim.withAlpha (0.25f));
            g.fillRoundedRectangle (b.area, 4.0f);
            g.setColour (live ? Colours::black : Palette::dim);
            g.setFont (uiFont (10.0f).withExtraKerningFactor (0.12f));
            const bool copied = juce::String (b.copy).isNotEmpty() && juce::Time::getMillisecondCounter() < copiedUntil;
            const auto label = copied ? mm::lang::tr (String::fromUTF8 ("\xe2\x9c\x93 ALIAS COPIED")) : mm::lang::tr (String::fromUTF8 (b.label));
            g.drawText (live ? label : label + String::fromUTF8 (" \xc2\xb7 ") + mm::lang::tr ("SOON"), b.area, Justification::centred);
        }

        g.setColour (Palette::dim.withAlpha (0.7f));
        g.setFont (uiFont (9.0f));
        g.drawText (mm::lang::tr ("CLICK ANYWHERE ELSE TO CLOSE"), card.reduced (20.0f, 14.0f), Justification::centredBottom);
    }

    void mouseMove (const juce::MouseEvent& e) override { hover = e.position; repaint(); }
    void mouseExit (const juce::MouseEvent&) override   { hover = {}; repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < 2; ++i)
            if (langArea (i).contains (e.position))
            {
                mm::lang::setLanguage ((mm::lang::Lang) i);
                repaint();
                return;
            }
        for (const auto& b : buttons())
            if (b.area.contains (e.position))
            {
                if (juce::String (b.copy).isNotEmpty())   // Mercado Pago: copia el alias para pegarlo en la app
                {
                    juce::SystemClipboard::copyTextToClipboard (b.copy);
                    copiedUntil = juce::Time::getMillisecondCounter() + 2500;
                    repaint();
                    juce::Component::SafePointer<juce::Component> safe (this);
                    juce::Timer::callAfterDelay (2600, [safe] { if (safe != nullptr) safe->repaint(); });
                }
                else if (juce::String (b.url).isNotEmpty()) juce::URL (b.url).launchInDefaultBrowser();
                return;
            }
        setVisible (false);
    }

private:
    struct LinkButton { const char* label; const char* url; juce::Rectangle<float> area; const char* copy = ""; };
    juce::uint32 copiedUntil = 0;

    juce::Rectangle<float> cardArea() const { return getLocalBounds().toFloat().withSizeKeepingCentre (560.0f, 460.0f); }
    juce::Rectangle<float> langArea (int i) const
    {
        const auto c = cardArea().reduced (40.0f, 30.0f);
        return { c.getRight() - 152.0f + (float) i * 78.0f, c.getY() + 12.0f, 74.0f, 22.0f };
    }

    std::array<LinkButton, 3> buttons() const
    {
        auto row = cardArea().reduced (40.0f, 30.0f).withTrimmedBottom (22.0f).removeFromBottom (30.0f);
        const float w = (row.getWidth() - 20.0f) / 3.0f;
        std::array<LinkButton, 3> b { LinkButton { "\xe2\x99\xa5 DONATE \xc2\xb7 PAYPAL", links::donatePaypal, {} },
                                      LinkButton { "\xe2\x99\xa5 MERCADO PAGO", "", {}, links::mercadoPagoAlias },
                                      LinkButton { "SOURCE CODE", links::sourceCode, {} } };
        for (auto& x : b) { x.area = row.removeFromLeft (w); row.removeFromLeft (10.0f); }
        return b;
    }

    juce::Point<float> hover;
};

} // namespace mm::gui
