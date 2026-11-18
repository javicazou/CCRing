#pragma once

#include "URingFonts.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include "Lang.h"

namespace mm::gui
{

// Estética de modular minimalista: rack oscuro, módulos de aluminio anodizado,
// serigrafía blanca y un único color de LED (acento).
// Los nombres "green/amber/red/cyan" se mantienen por compatibilidad.
namespace Palette
{
    inline const juce::Colour bg        { 0xff0e0f11 }; // rack
    inline const juce::Colour panel     { 0xff2a2c31 }; // aluminio anodizado
    inline const juce::Colour panelEdge { 0xff3d4047 };
    inline const juce::Colour text      { 0xffeeeeee }; // serigrafía
    inline const juce::Colour dim       { 0xffa3a8b0 };
    inline const juce::Colour accent    { 0xffff6a2b }; // LED
    inline const juce::Colour green     { 0xffeeeeee };
    inline const juce::Colour amber     { 0xff8d9097 };
    inline const juce::Colour red       { 0xffff6a2b };
    inline const juce::Colour cyan      { 0xffeeeeee };
    inline const juce::Colour padOff    { 0xff1a1b1e }; // display / zonas hundidas
    inline const juce::Colour lcdBg     { 0xff0b0c0e };
    inline const juce::Colour lcdText   { 0xffeeeeee };
    // Un color por motor (franja de los módulos, botones, cables del SEQ)
    inline const juce::Colour engineA   { 0xffff6a2b };
    inline const juce::Colour engineB   { 0xff3fd8ff };
    inline const juce::Colour engineC   { 0xff7be3a5 };
}

// Tipografía de todo el plugin: Montserrat Bold (incrustada; licencia OFL)
inline juce::Typeface::Ptr montserratBold()
{
    static const juce::Typeface::Ptr t = juce::Typeface::createSystemTypefaceFor (URingFonts::MontserratBold_ttf,
                                                                                 (size_t) URingFonts::MontserratBold_ttfSize);
    return t;
}

// Montserrat es más ancha que la fuente del sistema: se achica un poco para que los rótulos entren
inline juce::Font uiFont (float height, bool /*bold*/ = false)
{
    juce::Font f { juce::FontOptions (montserratBold()).withHeight (height * 0.92f) };
    f.setExtraKerningFactor (0.02f);
    return f;
}

// Textura de aluminio cepillado (vetas horizontales), generada una sola vez
inline const juce::Image& brushedTexture()
{
    static const juce::Image img = []
    {
        juce::Image im (juce::Image::ARGB, 512, 128, true);
        juce::Random r (42);
        juce::Graphics g (im);
        for (int y = 0; y < im.getHeight(); ++y)
        {
            float x = 0.0f;
            while (x < (float) im.getWidth())
            {
                const float len = 20.0f + r.nextFloat() * 160.0f;
                const bool light = r.nextBool();
                g.setColour ((light ? juce::Colours::white : juce::Colours::black).withAlpha (0.02f + r.nextFloat() * 0.05f));
                g.fillRect (x, (float) y, len, 1.0f);
                x += len + r.nextFloat() * 12.0f;
            }
        }
        return im;
    }();
    return img;
}

inline void drawScrew (juce::Graphics& g, juce::Point<float> c, float r = 4.0f)
{
    using namespace juce;
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillEllipse (c.x - r, c.y - r + 0.8f, r * 2.0f, r * 2.0f);
    g.setGradientFill (ColourGradient (Colour (0xffb9bcc2), c.x - r, c.y - r, Colour (0xff5d6067), c.x + r, c.y + r, false));
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    g.setColour (Colour (0xff2b2d31));
    g.drawLine (c.x - r * 0.6f, c.y - r * 0.25f, c.x + r * 0.6f, c.y + r * 0.25f, 1.2f); // ranura
}

// Riel de rack (aluminio con tira roscada y agujeros)
inline void drawRail (juce::Graphics& g, juce::Rectangle<float> r)
{
    using namespace juce;
    g.setGradientFill (ColourGradient (Colour (0xffa9acb2), r.getX(), r.getY(), Colour (0xff4f5257), r.getX(), r.getBottom(), false));
    g.fillRect (r);
    g.setColour (Colours::white.withAlpha (0.25f));
    g.drawHorizontalLine ((int) r.getY(), r.getX(), r.getRight());
    g.setColour (Colour (0xff2a2c30));
    g.fillRect (r.withSizeKeepingCentre (r.getWidth(), r.getHeight() * 0.36f));
    g.setColour (Colours::black.withAlpha (0.8f));
    for (float x = r.getX() + 6.0f; x < r.getRight(); x += 10.16f) // paso de 2 HP
        g.fillEllipse (x - 1.6f, r.getCentreY() - 1.6f, 3.2f, 3.2f);
}

// Rótulo de un jack (también se vuelve a pintar encima de los cables para que siempre se lea)
inline void drawJackLabel (juce::Graphics& g, juce::Point<float> c, const juce::String& label, bool lightPanel,
                           bool labelBelow, bool isOutput)
{
    using namespace juce;
    const float r = 6.5f;
    const auto area = labelBelow ? Rectangle<float> (c.x - 24.0f, c.y + r + 5.0f, 48.0f, 11.0f)
                                 : Rectangle<float> (c.x - 30.0f - r, c.y - 6.0f, 28.0f, 12.0f);
    const auto just = labelBelow ? Justification::centred : Justification::centredRight;
    g.setFont (uiFont (8.5f, true));
    g.setColour ((lightPanel ? Colours::white : Colours::black).withAlpha (0.55f));
    g.drawText (label, area.translated (0.0f, 1.0f), just);
    g.setColour (isOutput ? Palette::accent : (lightPanel ? Colour (0xff222326) : Palette::dim));
    g.drawText (label, area, just);
}

// Jack de 3,5 mm con tuerca hexagonal (decorativo)
inline void drawJack (juce::Graphics& g, juce::Point<float> c, const juce::String& label, bool lightPanel,
                      bool labelBelow = false, bool isOutput = false)
{
    using namespace juce;
    const float r = 6.5f;
    Path hex;
    for (int i = 0; i < 6; ++i)
    {
        const float a = MathConstants<float>::pi / 6.0f + MathConstants<float>::twoPi * (float) i / 6.0f;
        const auto p = c + Point<float> (std::cos (a), std::sin (a)) * r;
        if (i == 0) hex.startNewSubPath (p); else hex.lineTo (p);
    }
    hex.closeSubPath();
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillPath (hex, AffineTransform::translation (0.0f, 1.0f));
    g.setGradientFill (ColourGradient (Colour (0xffd5d7db), c.x - r, c.y - r, Colour (0xff6f7278), c.x + r, c.y + r, false));
    g.fillPath (hex);
    g.setColour (Colour (0xff1b1c1f));
    g.fillEllipse (c.x - 3.6f, c.y - 3.6f, 7.2f, 7.2f);
    g.setColour (Colours::black);
    g.fillEllipse (c.x - 2.2f, c.y - 2.2f, 4.4f, 4.4f);

    drawJackLabel (g, c, label, lightPanel, labelBelow, isOutput);
}

// Cable de patch con caída natural, sombra, brillo y plugs en los extremos
inline void drawCable (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b, juce::Colour colour, float sag)
{
    using namespace juce;
    Path p;
    p.startNewSubPath (a);
    p.cubicTo (a.translated (0.0f, sag), b.translated (0.0f, sag), b);

    // Cable fino y algo transparente: deja leer los rótulos que cruza
    g.setColour (Colours::black.withAlpha (0.22f));
    g.strokePath (p, PathStrokeType (4.5f, PathStrokeType::curved, PathStrokeType::rounded), AffineTransform::translation (2.0f, 4.0f));
    g.setColour (colour.darker (0.6f).withAlpha (0.8f));
    g.strokePath (p, PathStrokeType (4.5f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (colour.withAlpha (0.78f));
    g.strokePath (p, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Colours::white.withAlpha (0.3f));
    g.strokePath (p, PathStrokeType (0.8f, PathStrokeType::curved, PathStrokeType::rounded), AffineTransform::translation (-0.6f, -0.6f));

    for (auto c : { a, b })
    {
        g.setColour (Colours::black.withAlpha (0.5f));
        g.fillEllipse (c.x - 7.5f, c.y - 6.0f, 15.0f, 15.0f);
        g.setGradientFill (ColourGradient (Colour (0xff3a3c41), c.x, c.y - 7.0f, Colour (0xff111214), c.x, c.y + 7.0f, false));
        g.fillEllipse (c.x - 7.0f, c.y - 7.0f, 14.0f, 14.0f);
        g.setColour (colour);
        g.fillEllipse (c.x - 4.2f, c.y - 4.2f, 8.4f, 8.4f);
        g.setColour (Colours::white.withAlpha (0.35f));
        g.fillEllipse (c.x - 2.8f, c.y - 3.6f, 3.5f, 2.5f);
    }
}

// Título serigrafiado de un módulo (con LED del color del motor si tiene)
inline void drawPanelTitle (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, bool light, juce::Colour tag = {})
{
    using namespace juce;
    float x = 22.0f;
    if (! tag.isTransparent())
    {
        const Point<float> c (r.getX() + 25.0f, r.getY() + 13.5f);
        g.setColour (tag.withAlpha (0.3f));
        g.fillEllipse (c.x - 4.5f, c.y - 4.5f, 9.0f, 9.0f);
        g.setColour (tag);
        g.fillEllipse (c.x - 2.5f, c.y - 2.5f, 5.0f, 5.0f);
        x = 34.0f;
    }
    g.setColour (light ? Colour (0xff1c1d20) : Palette::text);
    g.setFont (uiFont (10.5f, true).withExtraKerningFactor (0.08f));
    g.drawText (title, r.withTrimmedLeft (x).withHeight (20.0f).translated (0.0f, 3.5f), Justification::centredLeft);
}

// Panel de módulo: aluminio cepillado (plateado o negro anodizado), bisel, tornillos,
// título serigrafiado y jacks decorativos arriba a la derecha
inline void drawModulePanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title,
                             bool light = false, const juce::StringArray& jacks = {}, juce::Colour tag = {})
{
    using namespace juce;
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 4.0f);

    const Colour base = light ? Colour (0xffc4c7cc) : Palette::panel;
    g.setGradientFill (ColourGradient (base.brighter (light ? 0.12f : 0.08f), r.getX(), r.getY(),
                                       base.darker (light ? 0.2f : 0.12f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 4.0f);
    g.setTiledImageFill (brushedTexture(), 0, 0, light ? 1.0f : 0.9f);
    g.fillRoundedRectangle (r, 4.0f);

    g.setColour (Colours::white.withAlpha (light ? 0.5f : 0.08f));
    g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 4.0f, r.getRight() - 4.0f);
    g.setColour (Colours::black.withAlpha (light ? 0.55f : 0.75f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    const float s = 9.0f;
    drawScrew (g, { r.getX() + s, r.getY() + s });
    drawScrew (g, { r.getRight() - s, r.getY() + s });
    drawScrew (g, { r.getX() + s, r.getBottom() - s });
    drawScrew (g, { r.getRight() - s, r.getBottom() - s });

    // Franja de color del motor / función, como la serigrafía de un módulo de fábrica
    if (! tag.isTransparent())
    {
        const auto strip = Rectangle<float> (r.getX() + 20.0f, r.getY() + 3.0f, r.getWidth() - 40.0f, 2.5f);
        g.setColour (tag.withAlpha (0.18f));
        g.fillRoundedRectangle (strip.expanded (0.0f, 2.0f), 2.0f);
        g.setColour (tag);
        g.fillRoundedRectangle (strip, 1.2f);
    }
    if (title.isNotEmpty())
        drawPanelTitle (g, r, title, light, tag);

    for (int i = 0; i < jacks.size(); ++i)
        drawJack (g, { r.getRight() - 30.0f - (float) i * 46.0f, r.getY() + 13.0f }, jacks[i], light);
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    // Cualquier texto (etiquetas, menús, listas, cuadros de diálogo, LCD) usa Montserrat Bold
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override { return montserratBold(); }

    LookAndFeel()
    {
        using namespace juce;
        setColour (Slider::rotarySliderFillColourId, Palette::accent);
        setColour (Label::textColourId, Palette::text);

        setColour (ComboBox::backgroundColourId, Palette::lcdBg);
        setColour (ComboBox::outlineColourId, Colours::black);
        setColour (ComboBox::textColourId, Palette::text);
        setColour (ComboBox::arrowColourId, Palette::dim);

        setColour (PopupMenu::backgroundColourId, Colour (0xff1c1d21));
        setColour (PopupMenu::textColourId, Palette::text);
        setColour (PopupMenu::headerTextColourId, Palette::dim);
        setColour (PopupMenu::highlightedBackgroundColourId, Palette::accent);
        setColour (PopupMenu::highlightedTextColourId, Colours::black);

        setColour (TextButton::buttonColourId, Colour (0xff303237));
        setColour (TextButton::buttonOnColourId, Palette::accent); // color del LED
        setColour (TextButton::textColourOffId, Palette::dim);
        setColour (TextButton::textColourOnId, Palette::text);
    }

    // Perilla de modular: escala serigrafiada, faldón estriado y tapa con brillo
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        using namespace juce;
        const auto bounds = Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
        const float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();
        const float toAngle = startAngle + pos * (endAngle - startAngle);
        auto polar = [c] (float a, float rad) { return c + Point<float> (std::sin (a), -std::cos (a)) * rad; };

        const bool light = s.getProperties()["light"]; // perilla negra sobre panel plateado

        // Escala: 11 marcas serigrafiadas
        g.setColour (light ? Colour (0xff2a2b2e) : Palette::dim.withAlpha (0.8f));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = startAngle + (endAngle - startAngle) * (float) i / 10.0f;
            const float len = (i == 0 || i == 5 || i == 10) ? 4.0f : 2.5f;
            g.drawLine (Line<float> (polar (a, radius - len), polar (a, radius)), 1.0f);
        }

        // Arco de valor muy fino (LED)
        const bool bipolar = s.getProperties()["bipolar"];
        const float fromAngle = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
        if (std::abs (toAngle - fromAngle) > 0.001f)
        {
            Path arc;
            const float ar = radius - 6.5f;
            arc.addCentredArc (c.x, c.y, ar, ar, 0.0f, jmin (fromAngle, toAngle), jmax (fromAngle, toAngle), true);
            g.setColour (Palette::accent.withAlpha (0.9f));
            g.strokePath (arc, PathStrokeType (1.5f, PathStrokeType::curved, PathStrokeType::rounded));
        }

        // Automatización grabada (cian): punto abajo = tiene movimiento grabado; arco = valor que suena ahora
        const double au = s.getProperties().getWithDefault ("auto", -1.0);
        if (au > -0.75)
        {
            const Colour cyan (0xff3fd8ff);
            const auto bottom = polar (MathConstants<float>::pi, radius - 2.0f);
            g.setColour (cyan.withAlpha (0.3f));
            g.fillEllipse (bottom.x - 4.5f, bottom.y - 4.5f, 9.0f, 9.0f);
            g.setColour (cyan);
            g.fillEllipse (bottom.x - 2.5f, bottom.y - 2.5f, 5.0f, 5.0f);
            if (au >= 0.0)
            {
                const float a = startAngle + (float) au * (endAngle - startAngle);
                Path arc;
                const float ar = radius - 3.0f;
                arc.addCentredArc (c.x, c.y, ar, ar, 0.0f, startAngle, a, true);
                g.setColour (cyan.withAlpha (0.85f));
                g.strokePath (arc, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
                const auto tip = polar (a, ar);
                g.fillEllipse (tip.x - 3.0f, tip.y - 3.0f, 6.0f, 6.0f);
            }
        }

        const float skirt = jmax (radius * 0.6f, radius - 9.0f);
        const float cap = skirt * 0.72f;

        // Sombra
        for (int i = 3; i > 0; --i)
        {
            g.setColour (Colours::black.withAlpha (0.12f * (float) i));
            const float rr = skirt + (float) i;
            g.fillEllipse (c.x - rr, c.y - rr + 2.5f, rr * 2.0f, rr * 2.0f);
        }

        // Faldón estriado: negro sobre panel plateado, plateado sobre panel negro
        g.setGradientFill (light ? ColourGradient (Colour (0xff2a2b2f), c.x, c.y - skirt, Colour (0xff0c0c0e), c.x, c.y + skirt, false)
                                 : ColourGradient (Colour (0xffa7aab0), c.x, c.y - skirt, Colour (0xff484b51), c.x, c.y + skirt, false));
        g.fillEllipse (c.x - skirt, c.y - skirt, skirt * 2.0f, skirt * 2.0f);
        g.setColour (light ? Colours::white.withAlpha (0.07f) : Colours::black.withAlpha (0.22f));
        for (int i = 0; i < 40; ++i)
        {
            const float a = MathConstants<float>::twoPi * (float) i / 40.0f + toAngle;
            g.drawLine (Line<float> (polar (a, cap + 1.0f), polar (a, skirt - 0.5f)), 1.0f);
        }

        // Tapa
        g.setGradientFill (light ? ColourGradient (Colour (0xff4a4c52), c.x, c.y - cap, Colour (0xff141518), c.x, c.y + cap, false)
                                 : ColourGradient (Colour (0xffe6e8eb), c.x, c.y - cap, Colour (0xff8f9399), c.x, c.y + cap, false));
        g.fillEllipse (c.x - cap, c.y - cap, cap * 2.0f, cap * 2.0f);
        g.setColour (Colours::white.withAlpha (0.10f));
        g.fillEllipse (c.x - cap * 0.75f, c.y - cap * 0.9f, cap * 1.2f, cap * 0.7f);
        g.setColour (Colours::black.withAlpha (0.6f));
        g.drawEllipse (c.x - cap, c.y - cap, cap * 2.0f, cap * 2.0f, 0.8f);

        // Línea indicadora (se ilumina al tocar)
        g.setColour (s.isMouseOverOrDragging() ? Palette::accent : (light ? Palette::text : Colour (0xff141518)));
        g.drawLine (Line<float> (polar (toAngle, cap * 0.15f), polar (toAngle, skirt - 1.0f)), 2.0f);
    }

    // Botón de goma con LED: el LED se enciende cuando el botón está activo
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bgColour,
                               bool highlighted, bool down) override
    {
        using namespace juce;
        auto r = b.getLocalBounds().toFloat().reduced (2.0f);
        const bool on = b.getToggleState();
        const auto face = bgColour.isTransparent() ? b.findColour (TextButton::buttonColourId) : bgColour;

        g.setColour (Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (r.translated (0.0f, down ? 0.5f : 1.5f), 3.0f);
        g.setGradientFill (ColourGradient (face.brighter (highlighted ? 0.25f : 0.15f), r.getX(), r.getY(),
                                           face.darker (0.35f), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r.translated (0.0f, down ? 1.0f : 0.0f), 3.0f);
        g.setColour (Colours::black.withAlpha (0.8f));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);

        if (b.getClickingTogglesState())
        {
            const auto led = b.findColour (TextButton::buttonOnColourId);
            const auto lc = Point<float> (r.getX() + 8.0f, r.getCentreY());
            if (on)
            {
                g.setColour (led.withAlpha (0.35f));
                g.fillEllipse (lc.x - 5.0f, lc.y - 5.0f, 10.0f, 10.0f);
            }
            g.setColour (on ? led : Colour (0xff3a1a10));
            g.fillEllipse (lc.x - 2.5f, lc.y - 2.5f, 5.0f, 5.0f);
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        auto r = b.getLocalBounds().reduced (4, 0);
        if (b.getClickingTogglesState())
            r.removeFromLeft (10); // espacio para el LED
        g.setColour (b.findColour (b.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId));
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.drawFittedText (b.getButtonText(), r, juce::Justification::centred, 1);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override
    {
        return uiFont (juce::jmin (11.0f, (float) h * 0.42f), true);
    }

    // Selector como pequeña ventana de display hundida
    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        using namespace juce;
        const auto r = Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);
        g.setColour (box.findColour (ComboBox::backgroundColourId));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (Colours::black.withAlpha (0.8f));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
        g.setColour (Colours::white.withAlpha (0.06f));
        g.drawHorizontalLine ((int) r.getBottom() - 1, r.getX() + 3.0f, r.getRight() - 3.0f);

        Path arrow;
        const float ax = (float) width - 9.0f, ay = (float) height * 0.5f;
        arrow.addTriangle (ax - 3.0f, ay - 1.5f, ax + 3.0f, ay - 1.5f, ax, ay + 2.0f);
        g.setColour (box.findColour (ComboBox::arrowColourId));
        g.fillPath (arrow);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (4, 1, box.getWidth() - 16, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return uiFont (11.5f); }
    juce::Font getPopupMenuFont() override               { return uiFont (13.5f); }
};

} // namespace mm::gui
