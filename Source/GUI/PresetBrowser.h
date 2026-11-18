#pragma once

// Buscador de presets (categorías, búsqueda, favoritos) y botones con ícono de la barra superior.
#include "LookAndFeel.h"
#include "Analyzer.h"
#include "../Presets.h"

namespace mm::gui
{

// Botón con ícono dibujado (no depende de que la tipografía tenga flechas o estrellas)
class IconButton : public juce::Button
{
public:
    enum class Icon { Undo, Redo, Browse, Cables, Dice };
    int level = 0;   // Cables: 0 = a la vista, 1 = transparentes, 2 = ocultos
    explicit IconButton (Icon i) : juce::Button ({}), icon (i) {}
    std::function<void()> onRightClick;   // clic derecho (menú)
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() && onRightClick) { onRightClick(); return; }
        juce::Button::mouseDown (e);
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        const bool on = getToggleState();
        g.setColour (Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (r.translated (0.0f, 1.0f), 4.0f);
        g.setGradientFill (ColourGradient (Colour (on ? 0xffff7a3d : 0xff3a3d43), r.getX(), r.getY(),
                                           Colour (on ? 0xffd9541c : 0xff24262a), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (Colours::white.withAlpha (down ? 0.02f : 0.1f));
        g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 3.0f, r.getRight() - 3.0f);

        const float a = isEnabled() ? (over ? 1.0f : 0.85f) : 0.3f;
        g.setColour ((on ? Colours::black : Palette::text).withAlpha (a));
        const auto c = r.getCentre();
        if (icon == Icon::Dice)
        {
            // dado: cuadrado redondeado con la cara del 5
            const auto d = Rectangle<float> (15.0f, 15.0f).withCentre (c);
            g.drawRoundedRectangle (d, 3.5f, 1.6f);
            const float o = 3.8f;
            for (auto p : { Point<float> (-o, -o), Point<float> (o, -o), Point<float> (0, 0), Point<float> (-o, o), Point<float> (o, o) })
                g.fillEllipse (c.x + p.x - 1.4f, c.y + p.y - 1.4f, 2.8f, 2.8f);
            return;
        }
        if (icon == Icon::Cables)
        {
            // dos jacks unidos por un cable que cuelga; transparente u oculto (tachado) según el modo
            const Point<float> j1 (c.x - 7.0f, c.y - 3.5f), j2 (c.x + 7.0f, c.y - 3.5f);
            Path cable;
            cable.startNewSubPath (j1);
            cable.cubicTo (j1.x, c.y + 7.0f, j2.x, c.y + 7.0f, j2.x, j2.y);
            g.setColour (Palette::accent.withAlpha (a * (level == 0 ? 1.0f : level == 1 ? 0.45f : 0.18f)));
            g.strokePath (cable, PathStrokeType (2.2f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour ((on ? Colours::black : Palette::text).withAlpha (a));
            for (auto j : { j1, j2 }) g.drawEllipse (j.x - 2.6f, j.y - 2.6f, 5.2f, 5.2f, 1.4f);
            if (level == 2)
                g.drawLine (c.x - 8.0f, c.y + 7.0f, c.x + 8.0f, c.y - 7.0f, 1.6f);
            return;
        }
        if (icon == Icon::Browse)
        {
            for (int i = -1; i <= 1; ++i)
            {
                g.fillEllipse (c.x - 7.0f, c.y + (float) i * 5.0f - 1.2f, 2.4f, 2.4f);
                g.fillRoundedRectangle (c.x - 3.0f, c.y + (float) i * 5.0f - 1.0f, 10.0f, 2.0f, 1.0f);
            }
            return;
        }
        // flecha curva: deshacer hacia la izquierda, rehacer hacia la derecha
        const float dir = icon == Icon::Undo ? 1.0f : -1.0f;
        Path arc;
        arc.addCentredArc (c.x, c.y + 2.0f, 6.0f, 5.0f, 0.0f, dir * -MathConstants<float>::halfPi, dir * MathConstants<float>::halfPi * 1.4f, true);
        g.strokePath (arc, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));
        const Point<float> tip (c.x - dir * 0.2f, c.y - 3.0f);
        Path head;
        head.addTriangle (tip.x - dir * 4.5f, tip.y, tip.x + dir * 0.5f, tip.y - 3.8f, tip.x + dir * 0.5f, tip.y + 3.8f);
        g.fillPath (head);
    }

private:
    Icon icon;
};

// Panel que tapa las páginas: categorías a la izquierda, búsqueda arriba, lista con estrellas.
// Clic = cargar (se escucha enseguida), doble clic / Enter / Esc = cerrar.
class PresetBrowser : public juce::Component, private juce::ListBoxModel, private juce::TextEditor::Listener,
                      private juce::KeyListener
{
public:
    struct Entry { juce::String name, category; int factoryIndex = -1; };   // -1 = preset de usuario

    std::function<void (int)> onLoadFactory;
    std::function<void (const juce::String&)> onLoadUser;
    std::function<juce::StringArray()> getUserPresets;
    std::function<juce::String()> getCurrentName;
    std::function<void (const juce::String& category)> onPreview;   // ESCUCHAR: toca una nota al elegir un preset
    juce::File favouritesFile;

    PresetBrowser()
    {
        setVisible (false);
        setWantsKeyboardFocus (true);
        search.addKeyListener (this);   // las flechas recorren la lista aunque el foco esté en la búsqueda
        list.addKeyListener (this);
        search.setTextToShowWhenEmpty ("SEARCH PRESETS...", Palette::dim.withAlpha (0.6f));
        search.setFont (uiFont (13.0f));
        search.setColour (juce::TextEditor::backgroundColourId, Palette::lcdBg);
        search.setColour (juce::TextEditor::outlineColourId, juce::Colours::black);
        search.setColour (juce::TextEditor::focusedOutlineColourId, Palette::accent.withAlpha (0.7f));
        search.setColour (juce::TextEditor::textColourId, Palette::text);
        search.setIndents (10, 7);
        search.addListener (this);
        addAndMakeVisible (search);

        list.setModel (this);
        list.setRowHeight (30);
        list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        list.setOutlineThickness (0);
        list.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId, Palette::dim.withAlpha (0.45f));
        addAndMakeVisible (list);

        close.setButtonText ("CLOSE");
        close.onClick = [this] { hide(); };
        addAndMakeVisible (close);

        preview.setButtonText ("PREVIEW");
        preview.setClickingTogglesState (true);
        preview.setToggleState (mm::lang::getSetting ("preview", 0) != 0, juce::dontSendNotification);
        preview.setColour (juce::TextButton::buttonOnColourId, Palette::accent);
        preview.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        preview.setTooltip ("Preview: plays a note every time you pick a preset");
        preview.onClick = [this] { mm::lang::setSetting ("preview", preview.getToggleState() ? 1 : 0); };
        addAndMakeVisible (preview);

        for (const auto* c : { "ALL", "FAVORITES", "USER" }) categories.add (c);
        for (const auto& p : presets::factory())
            if (! categories.contains (juce::String (p.category).toUpperCase())) categories.add (juce::String (p.category).toUpperCase());
    }

    void show()
    {
        loadFavourites();
        rebuild();
        setVisible (true);
        toFront (false);
        search.grabKeyboardFocus();
        selectCurrent();
    }
    void hide() { setVisible (false); }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        g.fillAll (Colours::black.withAlpha (0.55f));
        const auto panel = getLocalBounds().toFloat();
        drawModulePanel (g, panel, mm::lang::tr ("PRESETS"), false, {}, Palette::accent);

        // categorías
        auto cr = catArea();
        drawDisplay (g, cr.toFloat());
        for (int i = 0; i < categories.size(); ++i)
        {
            const auto row = catRow (i).toFloat();
            const bool sel = i == category;
            if (sel)
            {
                g.setColour (Palette::accent.withAlpha (0.18f));
                g.fillRoundedRectangle (row.reduced (4.0f, 1.0f), 3.0f);
                g.setColour (Palette::accent);
                g.fillRoundedRectangle (row.getX() + 4.0f, row.getY() + 6.0f, 3.0f, row.getHeight() - 12.0f, 1.5f);
            }
            g.setColour (sel ? Palette::text : Palette::dim);
            g.setFont (uiFont (11.0f).withExtraKerningFactor (0.1f));
            g.drawText (categoryLabel (categories[i]), row.withTrimmedLeft (16.0f), Justification::centredLeft);
            g.setColour (categoryColour (categories[i]).withAlpha (0.9f));
            g.drawText (String (countIn (i)), row.withTrimmedRight (12.0f), Justification::centredRight);
        }

        drawDisplay (g, listArea().toFloat().expanded (2.0f));
        g.setColour (Palette::dim.withAlpha (0.8f));
        g.setFont (uiFont (10.0f));
        const String sep = String::fromUTF8 ("   \xc2\xb7   ");
        g.drawText (String (shown.size()) + " " + mm::lang::tr ("PRESETS") + sep + mm::lang::tr ("CLICK = LOAD") + sep + mm::lang::tr ("ARROWS = BROWSE")
                        + sep + mm::lang::tr ("STAR = FAVORITE") + sep + mm::lang::tr ("ENTER / ESC = CLOSE"),
                    hintArea(), Justification::centredLeft);
    }

    void resized() override
    {
        auto top = getLocalBounds().reduced (20, 0).withTrimmedTop (34).removeFromTop (34);
        close.setBounds (top.removeFromRight (90).reduced (0, 2));
        top.removeFromRight (8);
        preview.setBounds (top.removeFromRight (104).reduced (0, 2));
        top.removeFromRight (10);
        search.setBounds (top.withTrimmedLeft (catArea().getWidth() + 14));
        list.setBounds (listArea());
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < categories.size(); ++i)
            if (catRow (i).contains (e.getPosition())) { category = i; rebuild(); repaint(); return; }
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::escapeKey || k == juce::KeyPress::returnKey) { hide(); return true; }
        return step (k);
    }

    // Flechas arriba / abajo (y página arriba / abajo): pasa al preset siguiente y lo carga
    bool keyPressed (const juce::KeyPress& k, juce::Component*) override
    {
        if (k == juce::KeyPress::escapeKey) { hide(); return true; }
        return step (k);
    }

private:
    // ---------------------------------------------------------------- lista
    int getNumRows() override { return shown.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        using namespace juce;
        if (row < 0 || row >= shown.size()) return;
        const auto& e = entries[(size_t) shown[row]];
        const bool current = getCurrentName && getCurrentName() == e.name;
        auto r = Rectangle<float> (0.0f, 0.0f, (float) w, (float) h);
        if (selected || current)
        {
            g.setColour ((current ? Palette::accent : Colours::white).withAlpha (current ? 0.16f : 0.06f));
            g.fillRoundedRectangle (r.reduced (4.0f, 2.0f), 3.0f);
        }
        if (row % 2 == 1) { g.setColour (Colours::white.withAlpha (0.015f)); g.fillRect (r); }
        drawStar (g, starArea (h), favourites.contains (e.name));
        g.setColour (current ? Palette::accent : Palette::text);
        g.setFont (uiFont (13.0f));
        g.drawText (e.name, r.withTrimmedLeft (44.0f).withTrimmedRight (160.0f), Justification::centredLeft);
        const auto col = categoryColour (e.category.toUpperCase());
        auto tag = r.removeFromRight (150.0f).reduced (10.0f, 7.0f);
        g.setColour (col.withAlpha (0.15f));
        g.fillRoundedRectangle (tag, 3.0f);
        g.setColour (col);
        g.setFont (uiFont (9.5f).withExtraKerningFactor (0.12f));
        g.drawText (categoryLabel (e.category.toUpperCase()), tag, Justification::centred);
    }

    bool step (const juce::KeyPress& k)
    {
        int delta = 0;
        if (k == juce::KeyPress::downKey) delta = 1;
        else if (k == juce::KeyPress::upKey) delta = -1;
        else if (k == juce::KeyPress::pageDownKey) delta = 10;
        else if (k == juce::KeyPress::pageUpKey) delta = -10;
        if (delta == 0 || shown.isEmpty()) return false;
        int row = list.getSelectedRow();
        if (row < 0) row = currentRow();
        row = juce::jlimit (0, shown.size() - 1, row < 0 ? 0 : row + delta);
        keyboardNav = false;
        list.selectRow (row);
        keyboardNav = true;
        list.scrollToEnsureRowIsOnscreen (row);
        load (entries[(size_t) shown[row]]);
        return true;
    }

    int currentRow() const
    {
        if (! getCurrentName) return -1;
        const auto cur = getCurrentName();
        for (int r = 0; r < shown.size(); ++r)
            if (entries[(size_t) shown[r]].name == cur) return r;
        return -1;
    }

    void listBoxItemClicked (int row, const juce::MouseEvent& e) override
    {
        if (row < 0 || row >= shown.size()) return;
        list.grabKeyboardFocus();   // después del clic, las flechas siguen recorriendo la lista
        const auto& en = entries[(size_t) shown[row]];
        if (starArea (list.getRowHeight()).expanded (6.0f).contains (e.position.withY ((float) list.getRowHeight() * 0.5f)))
        {
            toggleFavourite (en.name);
            return;
        }
        load (en);
    }
    void listBoxItemDoubleClicked (int, const juce::MouseEvent&) override { hide(); }
    void selectedRowsChanged (int row) override
    {
        // con las flechas del teclado se va escuchando cada preset (el clic ya carga por su lado)
        if (keyboardNav && ! juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown() && row >= 0 && row < shown.size())
            load (entries[(size_t) shown[row]]);
    }
    void returnKeyPressed (int) override { hide(); }

    void load (const Entry& e)
    {
        if (e.factoryIndex >= 0) { if (onLoadFactory) onLoadFactory (e.factoryIndex); }
        else if (onLoadUser) onLoadUser (e.name);
        if (preview.getToggleState() && onPreview) onPreview (e.category);
        list.repaint();
    }

    // ---------------------------------------------------------------- búsqueda
    void textEditorTextChanged (juce::TextEditor&) override { rebuild(); }
    void textEditorReturnKeyPressed (juce::TextEditor&) override
    {
        if (! shown.isEmpty()) load (entries[(size_t) shown[0]]);
        hide();
    }
    void textEditorEscapeKeyPressed (juce::TextEditor&) override { hide(); }

    void rebuild()
    {
        entries.clear();
        const auto& f = presets::factory();
        for (int i = 0; i < (int) f.size(); ++i) entries.push_back ({ f[(size_t) i].name, f[(size_t) i].category, i });
        if (getUserPresets)
            for (const auto& n : getUserPresets()) entries.push_back ({ n, "User", -1 });
        shown.clear();
        for (int i = 0; i < (int) entries.size(); ++i)
            if (matches (entries[(size_t) i], category)) shown.add (i);
        keyboardNav = false;
        list.updateContent();
        list.deselectAllRows();
        keyboardNav = true;
        list.repaint();
        repaint();
    }

    bool matches (const Entry& e, int cat) const
    {
        const auto c = categories[cat];
        if (c == "FAVORITES" && ! favourites.contains (e.name)) return false;
        if (c == "USER" && e.factoryIndex >= 0) return false;
        if (c != "ALL" && c != "FAVORITES" && c != "USER" && e.category.toUpperCase() != c) return false;
        const auto q = search.getText().trim();
        for (const auto& word : juce::StringArray::fromTokens (q, " ", {}))
            if (! e.name.containsIgnoreCase (word) && ! e.category.containsIgnoreCase (word)) return false;
        return true;
    }

    int countIn (int cat) const
    {
        int n = 0;
        const auto c = categories[cat];
        const auto& f = presets::factory();
        for (const auto& p : f)
            if (c == "ALL" || (c == "FAVORITES" && favourites.contains (p.name)) || juce::String (p.category).toUpperCase() == c) ++n;
        if ((c == "ALL" || c == "USER" || c == "FAVORITES") && getUserPresets)
            for (const auto& u : getUserPresets()) if (c != "FAVORITES" || favourites.contains (u)) ++n;
        return n;
    }

    void selectCurrent()
    {
        if (! getCurrentName) return;
        const auto cur = getCurrentName();
        for (int r = 0; r < shown.size(); ++r)
            if (entries[(size_t) shown[r]].name == cur)
            {
                keyboardNav = false;
                list.selectRow (r);
                keyboardNav = true;
                list.scrollToEnsureRowIsOnscreen (r);
                break;
            }
    }

    // ---------------------------------------------------------------- favoritos
    void loadFavourites()
    {
        favourites.clear();
        if (favouritesFile.existsAsFile())
            favourites.addLines (favouritesFile.loadFileAsString());
        favourites.removeEmptyStrings();
    }
    void toggleFavourite (const juce::String& name)
    {
        if (favourites.contains (name)) favourites.removeString (name); else favourites.add (name);
        if (favouritesFile != juce::File())
        {
            favouritesFile.getParentDirectory().createDirectory();
            favouritesFile.replaceWithText (favourites.joinIntoString ("\n"));
        }
        if (categories[category] == "FAVORITES") rebuild();
        list.repaint();
        repaint();
    }

    static void drawStar (juce::Graphics& g, juce::Rectangle<float> r, bool on)
    {
        juce::Path star;
        star.addStar (r.getCentre(), 5, r.getWidth() * 0.22f, r.getWidth() * 0.5f, -juce::MathConstants<float>::pi / 10.0f * 0.0f);
        if (on) { g.setColour (juce::Colour (0xffffc23d)); g.fillPath (star); }
        else { g.setColour (Palette::dim.withAlpha (0.45f)); g.strokePath (star, juce::PathStrokeType (1.2f)); }
    }

    // Nombre de categoría a mostrar (internamente siguen en inglés: así se guardan y se buscan)
    static juce::String categoryLabel (const juce::String& c)
    {
        if (! mm::lang::spanish()) return c;
        if (c == "SEQUENCE") return juce::String::fromUTF8 ("SECUENCIAS");
        if (c == "LIVE") return "VIVO";
        return mm::lang::tr (c);
    }

public:
    void retranslate()
    {
        search.setTextToShowWhenEmpty (mm::lang::tr ("SEARCH PRESETS..."), Palette::dim.withAlpha (0.6f));
        close.setButtonText (mm::lang::tr ("CLOSE"));
        preview.setButtonText (mm::lang::tr ("PREVIEW"));
        preview.setTooltip (mm::lang::tr ("Preview: plays a note every time you pick a preset"));
        list.repaint();
        repaint();
    }
private:

    static juce::Colour categoryColour (const juce::String& c)
    {
        if (c == "RESONATOR") return Palette::engineA;
        if (c == "IMPACT") return Palette::engineB;
        if (c == "MACRO") return Palette::engineC;
        if (c == "USER") return juce::Colour (0xffc39bff);
        if (c == "FAVORITES") return juce::Colour (0xffffc23d);
        if (c == "LIVE") return juce::Colour (0xffff8fb1);
        if (c == "BASS") return juce::Colour (0xff6fa8ff);
        if (c == "PAD")  return juce::Colour (0xff8fe3c8);
        if (c == "KEYS") return juce::Colour (0xffffd27a);
        if (c == "PERC") return juce::Colour (0xffff7a5c);
        if (c == "ALL") return Palette::text;
        return juce::Colour (0xffffb020);
    }

    juce::Rectangle<float> starArea (int h) const { return { 14.0f, (float) h * 0.5f - 8.0f, 16.0f, 16.0f }; }
    juce::Rectangle<int> catArea() const { return getLocalBounds().reduced (20, 0).withTrimmedTop (34).withTrimmedBottom (20).withWidth (200); }
    juce::Rectangle<int> catRow (int i) const { return { catArea().getX(), catArea().getY() + 8 + i * 32, catArea().getWidth(), 32 }; }
    juce::Rectangle<int> listArea() const
    {
        auto r = getLocalBounds().reduced (20, 0).withTrimmedTop (34 + 34 + 10).withTrimmedBottom (44);
        return r.withTrimmedLeft (catArea().getWidth() + 14);
    }
    juce::Rectangle<float> hintArea() const { return listArea().toFloat().withY ((float) listArea().getBottom() + 8.0f).withHeight (24.0f); }

    juce::TextEditor search;
    juce::ListBox list;
    juce::TextButton close, preview;
    juce::StringArray categories, favourites;
    std::vector<Entry> entries;
    juce::Array<int> shown;
    int category = 0;
    bool keyboardNav = true;
};

} // namespace mm::gui
