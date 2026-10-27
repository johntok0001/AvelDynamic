#pragma once

// Avel design system: shared look for every Avel product (EQ, Ember, Dynamic, ...).
// Keep this file identical across products; product-specific colours live in the editor.

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "BinaryData.h"

namespace avel::ui
{
    namespace col
    {
        const juce::Colour bg0       { 0xff0b0b0d };
        const juce::Colour bg1       { 0xff151518 };
        const juce::Colour bg2       { 0xff1c1c1f };
        const juce::Colour glass     { 0xe6202023 };
        const juce::Colour glassHi   { 0xff2c2c2f };
        const juce::Colour stroke    { 0x17ffffff };
        const juce::Colour strokeHi  { 0x26ffffff };
        const juce::Colour fill      { 0x33767680 };
        const juce::Colour fillHi    { 0x59767680 };
        const juce::Colour thumb     { 0xff636366 };
        const juce::Colour label     { 0xfff5f5f7 };
        const juce::Colour label2    { 0x99ebebf5 };
        const juce::Colour label3    { 0x4debebf5 };
        const juce::Colour label4    { 0x29ebebf5 };
        const juce::Colour accent    { 0xff0a84ff };
        const juce::Colour green     { 0xff30d158 };
        const juce::Colour red       { 0xffff453a };
        const juce::Colour yellow    { 0xffffd60a };
        const juce::Colour orange    { 0xffff9f0a };
        const juce::Colour pill      { 0xf0232326 };

        inline juce::Colour band (int index)
        {
            static const juce::Colour c[] {
                juce::Colour (0xff0a84ff), juce::Colour (0xffff9f0a), juce::Colour (0xff30d158), juce::Colour (0xffbf5af2),
                juce::Colour (0xffff375f), juce::Colour (0xff64d2ff), juce::Colour (0xffffd60a), juce::Colour (0xff5e5ce6),
                juce::Colour (0xff66d4cf), juce::Colour (0xffff6961), juce::Colour (0xffac8e68), juce::Colour (0xffda8fff)
            };
            return c[(size_t) ((index % 12) + 12) % 12];
        }
    }

    enum class Weight { regular, medium, semibold };

    inline juce::Typeface::Ptr typeface (Weight w)
    {
        static juce::Typeface::Ptr r = juce::Typeface::createSystemTypefaceFor (BinaryData::InterRegular_otf, BinaryData::InterRegular_otfSize);
        static juce::Typeface::Ptr m = juce::Typeface::createSystemTypefaceFor (BinaryData::InterMedium_otf, BinaryData::InterMedium_otfSize);
        static juce::Typeface::Ptr s = juce::Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_otf, BinaryData::InterSemiBold_otfSize);
        return w == Weight::regular ? r : w == Weight::medium ? m : s;
    }

    inline juce::Font font (float size, Weight w = Weight::medium, float kerning = 0.0f)
    {
        return juce::Font (juce::FontOptions (typeface (w)).withHeight (size).withKerningFactor (kerning));
    }

    /** 10 px SemiBold uppercase caption with +0.06 tracking. */
    inline juce::Font captionFont()   { return font (10.0f, Weight::semibold, 0.06f); }
    inline juce::Font valueFont()     { return font (12.0f, Weight::semibold); }

    inline float textWidth (const juce::Font& f, const juce::String& s)
    {
        return juce::GlyphArrangement::getStringWidth (f, s);
    }

    inline void drawGlass (juce::Graphics& g, juce::Rectangle<float> r, float radius, float shadow = 1.0f)
    {
        if (shadow > 0.0f)
        {
            juce::Path p;
            p.addRoundedRectangle (r, radius);
            juce::DropShadow (juce::Colours::black.withAlpha (0.55f * shadow), (int) (30 * shadow), { 0, (int) (12 * shadow) }).drawForPath (g, p);
            juce::DropShadow (juce::Colours::black.withAlpha (0.35f * shadow), 4, { 0, 1 }).drawForPath (g, p);
        }
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xf0262629), r.getX(), r.getY(), juce::Colour (0xf01b1b1e), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, radius);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.06f), r.getX(), r.getY(),
                                                 juce::Colours::transparentWhite, r.getX(), r.getY() + juce::jmin (40.0f, r.getHeight() * 0.5f), false));
        g.fillRoundedRectangle (r.reduced (1.0f), radius - 1.0f);
        g.setColour (col::stroke);
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawLine (r.getX() + radius, r.getY() + 0.75f, r.getRight() - radius, r.getY() + 0.75f, 1.0f);
    }

    /** Info pill: 0xf0232326 fill, radius 13 (or half height), hairline. */
    inline void drawPill (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fillColour = col::pill)
    {
        const float rad = juce::jmin (13.0f, r.getHeight() * 0.5f);
        g.setColour (fillColour);
        g.fillRoundedRectangle (r, rad);
        g.setColour (col::stroke);
        g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
    }

    inline bool approach (float& v, float target, float speed = 0.25f)
    {
        if (std::abs (v - target) < 0.002f) { const bool changed = ! juce::exactlyEqual (v, target); v = target; return changed; }
        v += (target - v) * speed;
        return true;
    }

    namespace icon
    {
        using P = juce::Path;
        inline P undo() { P p; p.startNewSubPath (9, 6); p.lineTo (5, 10); p.lineTo (9, 14); p.startNewSubPath (5, 10); p.lineTo (14, 10);
                          p.cubicTo (17.5f, 10, 20, 12.5f, 20, 15); p.cubicTo (20, 17.5f, 17.5f, 20, 14, 20); p.lineTo (11, 20); return p; }
        inline P redo() { auto p = undo(); p.applyTransform (juce::AffineTransform::scale (-1, 1).translated (25, 0)); return p; }
        inline P trash() { P p; p.startNewSubPath (4.5f, 7); p.lineTo (19.5f, 7); p.startNewSubPath (9.5f, 7); p.lineTo (10, 4.5f); p.lineTo (14, 4.5f); p.lineTo (14.5f, 7);
                           p.startNewSubPath (6.5f, 7); p.lineTo (7.5f, 19.5f); p.lineTo (16.5f, 19.5f); p.lineTo (17.5f, 7);
                           p.startNewSubPath (10.25f, 10.5f); p.lineTo (10.5f, 16.5f); p.startNewSubPath (13.75f, 10.5f); p.lineTo (13.5f, 16.5f); return p; }
        inline P headphones() { P p; p.startNewSubPath (4.5f, 15); p.cubicTo (4.5f, 8, 8, 4.5f, 12, 4.5f); p.cubicTo (16, 4.5f, 19.5f, 8, 19.5f, 15);
                                p.addRoundedRectangle (4.0f, 13.5f, 4.0f, 6.5f, 1.6f); p.addRoundedRectangle (16.0f, 13.5f, 4.0f, 6.5f, 1.6f); return p; }
        inline P power() { P p; p.addCentredArc (12, 13, 7, 7, 0, juce::degreesToRadians (35.0f), juce::degreesToRadians (325.0f), true);
                           p.startNewSubPath (12, 4); p.lineTo (12, 11.5f); return p; }
        inline P chevronDown() { P p; p.startNewSubPath (7, 10); p.lineTo (12, 15); p.lineTo (17, 10); return p; }
        inline P chevronLeft()  { P p; p.startNewSubPath (14, 6); p.lineTo (8, 12); p.lineTo (14, 18); return p; }
        inline P chevronRight() { P p; p.startNewSubPath (10, 6); p.lineTo (16, 12); p.lineTo (10, 18); return p; }
        inline P copy() { P p; p.addRoundedRectangle (8.5f, 8.5f, 11.0f, 11.0f, 2.5f); p.startNewSubPath (5, 15); p.lineTo (5, 7); p.quadraticTo (5, 5, 7, 5); p.lineTo (15, 5); return p; }
        inline P close() { P p; p.startNewSubPath (7, 7); p.lineTo (17, 17); p.startNewSubPath (17, 7); p.lineTo (7, 17); return p; }
        inline P record() { P p; p.addEllipse (7.5f, 7.5f, 9.0f, 9.0f); return p; }
        inline P pencil() { P p; p.startNewSubPath (5, 19); p.lineTo (6, 15); p.lineTo (15.5f, 5.5f); p.lineTo (18.5f, 8.5f); p.lineTo (9, 18); p.closeSubPath();
                            p.startNewSubPath (13.5f, 7.5f); p.lineTo (16.5f, 10.5f); return p; }
        inline P file() { P p; p.startNewSubPath (7, 4); p.lineTo (14, 4); p.lineTo (18, 8); p.lineTo (18, 20); p.lineTo (7, 20); p.closeSubPath();
                          p.startNewSubPath (14, 4); p.lineTo (14, 8); p.lineTo (18, 8); return p; }
        inline P sidechain() { P p; p.startNewSubPath (3, 12); p.lineTo (7, 12); p.lineTo (9, 7); p.lineTo (12, 17); p.lineTo (15, 9); p.lineTo (17, 12); p.lineTo (21, 12);
                               p.startNewSubPath (5, 20); p.lineTo (19, 20); return p; }
        inline P sliders() { P p; p.startNewSubPath (4, 7); p.lineTo (20, 7); p.startNewSubPath (4, 12); p.lineTo (20, 12); p.startNewSubPath (4, 17); p.lineTo (20, 17);
                             p.addEllipse (13, 5, 4, 4); p.addEllipse (6, 10, 4, 4); p.addEllipse (11, 15, 4, 4); return p; }
        inline P save() { P p; p.startNewSubPath (5, 5); p.lineTo (16, 5); p.lineTo (19, 8); p.lineTo (19, 19); p.lineTo (5, 19); p.closeSubPath();
                          p.startNewSubPath (8, 5); p.lineTo (8, 9); p.lineTo (15, 9); p.lineTo (15, 5); p.addRoundedRectangle (8.0f, 13.0f, 8.0f, 6.0f, 1.0f); return p; }
        inline P check() { P p; p.startNewSubPath (5.5f, 12.5f); p.lineTo (10, 17); p.lineTo (18.5f, 7.5f); return p; }

        inline void draw (juce::Graphics& g, const P& p, juce::Rectangle<float> area, juce::Colour c, float stroke = 1.6f, float designSize = 24.0f)
        {
            const float s = juce::jmin (area.getWidth(), area.getHeight()) / designSize;
            auto t = juce::AffineTransform::scale (s).translated (area.getCentreX() - designSize * 0.5f * s, area.getCentreY() - designSize * 0.5f * s);
            g.setColour (c);
            g.strokePath (p, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), t);
        }
    }

    //==============================================================================
    /** Menus, tooltips and text editors in the Avel style. */
    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel()
        {
            setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xf2232326)); // translucent => rounded window
            setColour (juce::PopupMenu::textColourId, col::label);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent);
            setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
            setColour (juce::PopupMenu::headerTextColourId, col::label3);
            setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf2232326));
            setColour (juce::TooltipWindow::textColourId, col::label);
            setColour (juce::TooltipWindow::outlineColourId, col::strokeHi);
            setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff2a2a2d));
            setColour (juce::TextEditor::textColourId, col::label);
            setColour (juce::TextEditor::highlightColourId, col::accent.withAlpha (0.45f));
            setColour (juce::TextEditor::highlightedTextColourId, juce::Colours::white);
            setColour (juce::TextEditor::outlineColourId, col::strokeHi);
            setColour (juce::TextEditor::focusedOutlineColourId, col::accent);
            setColour (juce::CaretComponent::caretColourId, col::accent);
            setColour (juce::ResizableWindow::backgroundColourId, col::bg0);
            setColour (juce::AlertWindow::backgroundColourId, col::bg2);
            setColour (juce::AlertWindow::textColourId, col::label);
            setColour (juce::AlertWindow::outlineColourId, col::strokeHi);
            setColour (juce::TextButton::buttonColourId, col::glassHi);
            setColour (juce::TextButton::textColourOffId, col::label);
            setColour (juce::ScrollBar::thumbColourId, col::thumb);
        }

        juce::Typeface::Ptr getTypefaceForFont (const juce::Font& f) override
        {
            const auto style = f.getTypefaceStyle();
            if (style.containsIgnoreCase ("bold") || style.containsIgnoreCase ("semi")) return typeface (Weight::semibold);
            if (style.containsIgnoreCase ("medium")) return typeface (Weight::medium);
            return typeface (Weight::regular);
        }

        // ---- popup menus
        juce::Font getPopupMenuFont() override { return font (13.0f, Weight::medium); }

        int getPopupMenuBorderSizeWithOptions (const juce::PopupMenu::Options&) override { return 6; }

        void drawPopupMenuBackgroundWithOptions (juce::Graphics& g, int width, int height, const juce::PopupMenu::Options&) override
        {
            auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height);
            g.setColour (findColour (juce::PopupMenu::backgroundColourId));
            g.fillRoundedRectangle (r, 10.0f);
            g.setColour (col::strokeHi);
            g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
        }

        void getIdealPopupMenuItemSizeWithOptions (const juce::String& text, bool isSeparator, int standardMenuItemHeight,
                                                   int& idealWidth, int& idealHeight, const juce::PopupMenu::Options&) override
        {
            juce::ignoreUnused (standardMenuItemHeight);
            if (isSeparator) { idealWidth = 50; idealHeight = 9; return; }
            idealHeight = 26;
            idealWidth = (int) textWidth (getPopupMenuFont(), text) + 26 + 34;
        }

        void drawPopupMenuItemWithOptions (juce::Graphics& g, const juce::Rectangle<int>& area, bool isHighlighted,
                                           const juce::PopupMenu::Item& item, const juce::PopupMenu::Options&) override
        {
            if (item.isSeparator)
            {
                auto r = area.toFloat().reduced (8.0f, 0.0f);
                g.setColour (col::stroke);
                g.fillRect (r.withHeight (1.0f).withCentre (r.getCentre()));
                return;
            }

            auto r = area.toFloat().reduced (2.0f, 1.0f);
            const bool hl = isHighlighted && item.isEnabled;
            if (hl)
            {
                g.setColour (col::accent);
                g.fillRoundedRectangle (r, 6.0f);
            }

            const auto textCol = ! item.isEnabled ? col::label3 : hl ? juce::Colours::white : col::label;
            auto tickArea = r.removeFromLeft (24.0f);
            if (item.isTicked)
                icon::draw (g, icon::check(), tickArea.withSizeKeepingCentre (14, 14), textCol, 2.0f);

            if (item.subMenu != nullptr)
            {
                auto arrow = r.removeFromRight (18.0f);
                icon::draw (g, icon::chevronRight(), arrow.withSizeKeepingCentre (12, 12), hl ? juce::Colours::white : col::label2, 1.8f);
            }

            g.setColour (textCol);
            g.setFont (getPopupMenuFont());
            g.drawText (item.text, r.withTrimmedRight (8.0f), juce::Justification::centredLeft, true);
        }

        void drawPopupMenuSectionHeaderWithOptions (juce::Graphics& g, const juce::Rectangle<int>& area,
                                                    const juce::String& text, const juce::PopupMenu::Options&) override
        {
            g.setColour (col::label3);
            g.setFont (captionFont());
            g.drawText (text.toUpperCase(), area.toFloat().withTrimmedLeft (26.0f), juce::Justification::centredLeft, true);
        }

        // ---- tooltips
        juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override
        {
            const auto f = font (12.0f, Weight::medium);
            const int w = juce::jmin (320, (int) textWidth (f, tipText) + 22);
            const int lines = juce::jmax (1, (int) std::ceil (textWidth (f, tipText) / 300.0f));
            const int h = 10 + lines * 16;
            return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 16,
                                         screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 8) : screenPos.y + 18,
                                         w, h).constrainedWithin (parentArea);
        }

        void drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height) override
        {
            auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height);
            g.setColour (juce::Colour (0xf62a2a2d));
            g.fillRoundedRectangle (r, 7.0f);
            g.setColour (col::strokeHi);
            g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
            g.setColour (col::label);
            g.setFont (font (12.0f, Weight::medium));
            g.drawFittedText (text, r.reduced (11.0f, 4.0f).toNearestInt(), juce::Justification::centredLeft, 4, 1.0f);
        }

        // ---- window resize corner: three hairlines in label4
        void drawCornerResizer (juce::Graphics& g, int w, int h, bool isMouseOver, bool isMouseDragging) override
        {
            g.setColour ((isMouseOver || isMouseDragging) ? col::label3 : col::label4);
            for (int i = 0; i < 3; ++i)
            {
                const float o = 4.0f + (float) i * 4.0f;
                g.drawLine ((float) w - o, (float) h - 2.0f, (float) w - 2.0f, (float) h - o, 1.2f);
            }
        }

        // ---- text editor (inline value entry)
        void fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor& e) override
        {
            g.setColour (e.findColour (juce::TextEditor::backgroundColourId));
            g.fillRoundedRectangle (juce::Rectangle<float> (0, 0, (float) width, (float) height), 6.0f);
        }

        void drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& e) override
        {
            g.setColour (e.hasKeyboardFocus (true) ? col::accent : col::strokeHi);
            g.drawRoundedRectangle (juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f), 6.0f, 1.0f);
        }
    };

    //==============================================================================
    /** Small helper: a component that animates with approach() from a 60 Hz timer and stops when settled. */
    class Animated : private juce::Timer
    {
    public:
        virtual ~Animated() = default;
    protected:
        void startAnimating() { if (! isTimerRunning()) startTimerHz (60); }
        /** Return true while anything is still moving. */
        virtual bool animate() = 0;
    private:
        void timerCallback() override { if (! animate()) stopTimer(); }
    };

    //==============================================================================
    /** iOS-style segmented control with an animated thumb. Segment widths follow their labels. */
    class Segmented : public juce::Component, public juce::SettableTooltipClient, private Animated
    {
    public:
        Segmented() { setRepaintsOnMouseActivity (true); }

        void setItems (const juce::StringArray& newItems)
        {
            items = newItems;
            hover.assign ((size_t) items.size(), 0.0f);
            resized();
            repaint();
        }

        const juce::StringArray& getItems() const { return items; }
        void setFontSize (float s) { fontSize = s; resized(); repaint(); }
        void setThumbColour (juce::Colour c) { thumbColour = c; repaint(); }
        void setTooltips (const juce::StringArray& tips) { itemTips = tips; }

        int getSelected() const { return selected; }

        void setSelected (int index, juce::NotificationType n = juce::sendNotificationSync)
        {
            index = juce::jlimit (0, juce::jmax (0, items.size() - 1), index);
            if (index == selected) return;
            selected = index;
            startAnimating();
            if (n != juce::dontSendNotification && onChange) onChange (selected);
        }

        /** Width needed to show every label without truncation. */
        int getIdealWidth() const
        {
            float w = 4.0f;
            for (auto& s : items) w += textWidth (font (fontSize, Weight::semibold), s) + 2.0f * padding;
            return (int) std::ceil (w);
        }

        void bind (juce::RangedAudioParameter& p, juce::UndoManager* um)
        {
            attachment = std::make_unique<juce::ParameterAttachment> (p, [this] (float v) { setSelected (juce::roundToInt (v), juce::dontSendNotification); }, um);
            onChange = [this] (int i) { attachment->setValueAsCompleteGesture ((float) i); };
            attachment->sendInitialUpdate();
            snap();
        }

        void snap() { auto r = segment (selected); thumbX = r.getX(); thumbW = r.getWidth(); repaint(); }

        std::function<void (int)> onChange;

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            const float rad = juce::jmin (8.0f, r.getHeight() * 0.32f);
            g.setColour (col::fill.withMultipliedAlpha (0.8f));
            g.fillRoundedRectangle (r, rad);

            if (items.isEmpty()) return;

            // separators between unselected neighbours
            for (int i = 1; i < items.size(); ++i)
            {
                if (i == selected || i - 1 == selected) continue;
                auto s = segment (i);
                g.setColour (col::stroke);
                g.fillRect (juce::Rectangle<float> (s.getX() - 0.5f, r.getY() + r.getHeight() * 0.28f, 1.0f, r.getHeight() * 0.44f));
            }

            auto t = juce::Rectangle<float> (thumbX, r.getY() + 2.0f, thumbW, r.getHeight() - 4.0f);
            {
                juce::Path p; p.addRoundedRectangle (t, rad - 1.5f);
                juce::DropShadow (juce::Colours::black.withAlpha (0.35f), 6, { 0, 2 }).drawForPath (g, p);
            }
            g.setColour (isEnabled() ? thumbColour : thumbColour.withMultipliedAlpha (0.5f));
            g.fillRoundedRectangle (t, rad - 1.5f);
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.drawRoundedRectangle (t.reduced (0.5f), rad - 1.5f, 1.0f);

            g.setFont (font (fontSize, Weight::semibold));
            for (int i = 0; i < items.size(); ++i)
            {
                auto s = segment (i);
                auto c = i == selected ? col::label : col::label2.interpolatedWith (col::label, hover[(size_t) i] * 0.6f);
                if (! isEnabled()) c = c.withMultipliedAlpha (0.45f);
                g.setColour (c);
                g.drawText (items[i], s, juce::Justification::centred, false);
            }
        }

        void resized() override { snap(); }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const int i = indexAt (e.position);
            if (i >= 0) setSelected (i);
        }

        void mouseMove (const juce::MouseEvent& e) override { hoverIndex = indexAt (e.position); updateTip(); startAnimating(); }
        void mouseExit (const juce::MouseEvent&) override { hoverIndex = -1; startAnimating(); }

    private:
        juce::Rectangle<float> segment (int i) const
        {
            auto r = getLocalBounds().toFloat().reduced (2.0f, 0.0f);
            if (items.isEmpty()) return r;
            i = juce::jlimit (0, items.size() - 1, i);
            const auto f = font (fontSize, Weight::semibold);
            float total = 0.0f;
            juce::Array<float> w;
            for (auto& s : items) { w.add (textWidth (f, s) + 2.0f * padding); total += w.getLast(); }
            const float extra = (r.getWidth() - total) / (float) items.size();
            float x = r.getX();
            for (int k = 0; k < i; ++k) x += w[k] + extra;
            return { x, r.getY(), w[i] + extra, r.getHeight() };
        }

        int indexAt (juce::Point<float> p) const
        {
            for (int i = 0; i < items.size(); ++i)
                if (segment (i).contains (p.x, (float) getHeight() * 0.5f)) return i;
            return -1;
        }

        void updateTip()
        {
            if (hoverIndex >= 0 && hoverIndex < itemTips.size()) setTooltip (itemTips[hoverIndex]);
        }

        bool animate() override
        {
            auto target = segment (selected);
            bool moving = approach (thumbX, target.getX(), 0.3f);
            moving |= approach (thumbW, target.getWidth(), 0.3f);
            for (size_t i = 0; i < hover.size(); ++i)
                moving |= approach (hover[i], (int) i == hoverIndex ? 1.0f : 0.0f, 0.3f);
            repaint();
            return moving;
        }

        juce::StringArray items, itemTips;
        std::vector<float> hover;
        int selected = 0, hoverIndex = -1;
        float thumbX = 0, thumbW = 0, fontSize = 11.5f, padding = 9.0f;
        juce::Colour thumbColour { col::thumb };
        std::unique_ptr<juce::ParameterAttachment> attachment;
    };

    //==============================================================================
    /** iOS toggle with an optional label on its left. */
    class Switch : public juce::Component, public juce::SettableTooltipClient, private Animated
    {
    public:
        Switch() { setRepaintsOnMouseActivity (true); }

        void setLabel (const juce::String& s) { text = s; repaint(); }
        void setOnColour (juce::Colour c) { onColour = c; repaint(); }

        bool getState() const { return state; }
        void setState (bool s, juce::NotificationType n = juce::sendNotificationSync)
        {
            if (s == state) return;
            state = s;
            startAnimating();
            if (n != juce::dontSendNotification && onChange) onChange (state);
        }

        int getIdealWidth() const
        {
            return (int) std::ceil ((text.isEmpty() ? 0.0f : textWidth (font (12.0f, Weight::medium), text) + 8.0f) + trackW);
        }

        void bind (juce::RangedAudioParameter& p, juce::UndoManager* um)
        {
            attachment = std::make_unique<juce::ParameterAttachment> (p, [this] (float v) { setState (v >= 0.5f, juce::dontSendNotification); }, um);
            onChange = [this] (bool s) { attachment->setValueAsCompleteGesture (s ? 1.0f : 0.0f); };
            attachment->sendInitialUpdate();
            pos = state ? 1.0f : 0.0f;
        }

        std::function<void (bool)> onChange;

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            auto track = r.removeFromRight (trackW).withSizeKeepingCentre (trackW, trackH);
            const float alpha = isEnabled() ? 1.0f : 0.45f;

            if (text.isNotEmpty())
            {
                g.setColour ((isMouseOver() ? col::label : col::label2).withMultipliedAlpha (alpha));
                g.setFont (font (12.0f, Weight::medium));
                g.drawText (text, r.withTrimmedRight (8.0f), juce::Justification::centredRight, false);
            }

            g.setColour (col::fill.interpolatedWith (onColour, pos).withMultipliedAlpha (alpha));
            g.fillRoundedRectangle (track, trackH * 0.5f);

            const float d = trackH - 4.0f;
            auto knob = juce::Rectangle<float> (track.getX() + 2.0f + pos * (trackW - trackH), track.getY() + 2.0f, d, d);
            {
                juce::Path p; p.addEllipse (knob);
                juce::DropShadow (juce::Colours::black.withAlpha (0.35f), 4, { 0, 1 }).drawForPath (g, p);
            }
            g.setColour (juce::Colours::white.withAlpha (alpha));
            g.fillEllipse (knob);
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            if (isEnabled() && getLocalBounds().contains (e.getPosition()) && ! e.mouseWasDraggedSinceMouseDown())
                setState (! state);
        }

    private:
        bool animate() override { const bool m = approach (pos, state ? 1.0f : 0.0f, 0.3f); repaint(); return m; }

        juce::String text;
        bool state = false;
        float pos = 0.0f, trackW = 34.0f, trackH = 20.0f;
        juce::Colour onColour { col::green };
        std::unique_ptr<juce::ParameterAttachment> attachment;
    };

    //==============================================================================
    /** Stroke icon (24 px grid) and/or short text. Hover fade, toggled tint, optional filled "primary" style. */
    class IconButton : public juce::Button, private Animated
    {
    public:
        IconButton (const juce::String& name = {}, juce::Path iconPath = {}) : juce::Button (name), path (std::move (iconPath)) {}

        void setIcon (juce::Path p) { path = std::move (p); repaint(); }
        void setText (const juce::String& s) { text = s; repaint(); }
        void setPrimary (bool p) { primary = p; repaint(); }
        void setTint (juce::Colour c) { tint = c; repaint(); }
        void setIconSize (float s) { iconSize = s; repaint(); }

        int getIdealWidth (int height) const
        {
            float w = path.isEmpty() ? 0.0f : (float) height;
            if (text.isNotEmpty()) w += textWidth (font (12.0f, Weight::semibold), text) + (path.isEmpty() ? 22.0f : 10.0f);
            return (int) std::ceil (w);
        }

        void paintButton (juce::Graphics& g, bool, bool) override
        {
            auto r = getLocalBounds().toFloat();
            const float rad = juce::jmin (8.0f, r.getHeight() * 0.5f);
            const bool on = getToggleState();
            const float alpha = isEnabled() ? 1.0f : 0.45f;

            juce::Colour bg = primary ? tint : col::fill.withMultipliedAlpha (0.0f);
            if (primary) bg = bg.brighter (0.15f * hoverAmt).darker (isDown() ? 0.15f : 0.0f);
            else         bg = col::fill.withMultipliedAlpha (0.55f * hoverAmt + (on ? 0.0f : 0.0f) + (isDown() ? 0.4f : 0.0f));
            if (on && ! primary) bg = tint.withAlpha (0.18f + 0.06f * hoverAmt);

            g.setColour (bg.withMultipliedAlpha (alpha));
            g.fillRoundedRectangle (r, rad);
            if (on && ! primary)
            {
                g.setColour (tint.withAlpha (0.35f * alpha));
                g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
            }

            const auto fg = (primary ? juce::Colours::white
                             : on ? tint.brighter (0.25f)
                                  : col::label2.interpolatedWith (col::label, hoverAmt)).withMultipliedAlpha (alpha);

            auto content = r;
            if (! path.isEmpty())
            {
                auto ia = text.isEmpty() ? content : content.removeFromLeft (r.getHeight()).translated (4.0f, 0.0f);
                icon::draw (g, path, ia.withSizeKeepingCentre (iconSize, iconSize), fg, 1.6f);
            }
            if (text.isNotEmpty())
            {
                g.setColour (fg);
                g.setFont (font (12.0f, Weight::semibold));
                g.drawText (text, path.isEmpty() ? content : content.withTrimmedRight (8.0f), juce::Justification::centred, false);
            }
        }

        void buttonStateChanged() override { startAnimating(); }

    private:
        bool animate() override
        {
            const bool m = approach (hoverAmt, isOver() || isDown() ? 1.0f : 0.0f, 0.3f);
            repaint();
            return m;
        }

        juce::Path path;
        juce::String text;
        bool primary = false;
        float hoverAmt = 0.0f, iconSize = 18.0f;
        juce::Colour tint { col::accent };
    };

    //==============================================================================
    /** Ring dial: caption above, value below. Drag (Shift = fine), wheel, double-click resets,
        click the value to type it. Bind to a parameter or to a plain value. */
    class Dial : public juce::Component, public juce::SettableTooltipClient, private Animated
    {
    public:
        Dial (const juce::String& captionText = {}) : caption (captionText)
        {
            setRepaintsOnMouseActivity (true);
            setWantsKeyboardFocus (false);
        }

        void setCaption (const juce::String& s) { caption = s; repaint(); }
        void setColour (juce::Colour c) { colour = c; repaint(); }
        void setBipolar (bool b) { bipolar = b; repaint(); }
        void setShowCaption (bool b) { showCaption = b; resized(); repaint(); }
        /** Draw the value arc as if this normalised value were set (e.g. GR overlay). 0 = none. */
        void setSecondaryArc (float amount01, juce::Colour c) { if (std::abs (amount01 - secondary) > 0.001f || c != secondaryColour) { secondary = amount01; secondaryColour = c; repaint(); } }

        float getNormalised() const { return norm; }

        void bind (juce::RangedAudioParameter& p, juce::UndoManager* um)
        {
            param = &p;
            range = p.getNormalisableRange();
            defaultNorm = p.getDefaultValue();
            toText = [this] (float v) { return param->getText (param->convertTo0to1 (v), 16); };
            fromText = [this] (const juce::String& s) { return param->convertFrom0to1 (param->getValueForText (s)); };
            attachment = std::make_unique<juce::ParameterAttachment> (p, [this] (float v) { setNormInternal (range.convertTo0to1 (v)); }, um);
            attachment->sendInitialUpdate();
        }

        /** For values that are not plug-in parameters. */
        void bindPlain (juce::NormalisableRange<float> r, float defaultValue, float current,
                        std::function<void (float)> setter, std::function<juce::String (float)> formatter,
                        std::function<float (const juce::String&)> parser = nullptr)
        {
            param = nullptr;
            attachment.reset();
            range = r;
            defaultNorm = range.convertTo0to1 (defaultValue);
            plainSetter = std::move (setter);
            toText = std::move (formatter);
            fromText = parser ? std::move (parser) : [] (const juce::String& s) { return s.retainCharacters ("-+.0123456789").getFloatValue(); };
            setNormInternal (range.convertTo0to1 (current));
        }

        /** Update a plain-bound dial from outside without calling the setter. */
        void setPlainValue (float v) { setNormInternal (range.convertTo0to1 (v)); }

        void paint (juce::Graphics& g) override
        {
            const bool enabled = isEnabled();
            auto knob = knobArea();
            const float d = knob.getWidth();
            const auto c = knob.getCentre();
            const float start = juce::MathConstants<float>::pi * -0.75f, end = juce::MathConstants<float>::pi * 0.75f;
            const float ringW = juce::jlimit (2.5f, 4.0f, d * 0.075f);
            const float ringR = d * 0.5f - ringW * 0.5f - 1.0f;

            if (showCaption)
            {
                g.setColour (col::label3.interpolatedWith (col::label2, hoverAmt * 0.7f));
                g.setFont (captionFont());
                g.drawText (caption.toUpperCase(), captionArea(), juce::Justification::centred, false);
            }

            // track
            juce::Path track;
            track.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, start, end, true);
            g.setColour (col::fill.withMultipliedAlpha (1.0f + 0.4f * hoverAmt));
            g.strokePath (track, juce::PathStrokeType (ringW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            const float from = bipolar ? 0.0f : start;
            const float to = start + (end - start) * shown;
            const auto arcCol = enabled ? colour : col::label3;

            if (secondary > 0.001f)
            {
                juce::Path s;
                s.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, start, start + (end - start) * juce::jlimit (0.0f, 1.0f, secondary), true);
                g.setColour (secondaryColour.withAlpha (0.55f));
                g.strokePath (s, juce::PathStrokeType (ringW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            if (std::abs (to - from) > 0.002f)
            {
                juce::Path arc;
                arc.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, juce::jmin (from, to), juce::jmax (from, to), true);
                if (enabled) // glow
                {
                    g.setColour (arcCol.withAlpha (0.16f + 0.10f * hoverAmt));
                    g.strokePath (arc, juce::PathStrokeType (ringW * 3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                }
                g.setColour (arcCol);
                g.strokePath (arc, juce::PathStrokeType (ringW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            // knob face
            const float faceR = ringR - ringW - 2.5f;
            auto face = juce::Rectangle<float> (faceR * 2.0f, faceR * 2.0f).withCentre (c);
            {
                juce::Path p; p.addEllipse (face);
                juce::DropShadow (juce::Colours::black.withAlpha (0.6f), (int) (faceR * 0.35f) + 2, { 0, 2 }).drawForPath (g, p);
            }
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff36363a).interpolatedWith (juce::Colour (0xff404045), hoverAmt * 0.5f), c.x, face.getY(),
                                                     juce::Colour (0xff1d1d20), c.x, face.getBottom(), false));
            g.fillEllipse (face);
            g.setColour (juce::Colours::white.withAlpha (0.09f));
            g.drawEllipse (face.reduced (0.5f), 1.0f);

            // indicator
            const float a = start + (end - start) * shown;
            const auto dir = juce::Point<float> (std::sin (a), -std::cos (a));
            g.setColour (juce::Colours::white.withAlpha (enabled ? 0.95f : 0.45f));
            g.drawLine ({ c + dir * (faceR * 0.38f), c + dir * (faceR * 0.86f) }, juce::jmax (1.6f, d * 0.035f));

            // value
            if (! editor)
            {
                auto va = valueArea();
                g.setFont (valueFont());
                g.setColour ((dragging || hoverValue ? col::label : col::label2.interpolatedWith (col::label, 0.55f)).withMultipliedAlpha (enabled ? 1.0f : 0.45f));
                g.drawText (currentText(), va, juce::Justification::centred, false);
            }
        }

        void resized() override { if (editor) editor->setBounds (valueArea().toNearestInt().expanded (2, 1)); }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (! isEnabled()) return;
            if (valueArea().contains (e.position) && ! knobArea().expanded (0, -4).contains (e.position))
            {
                valueClick = true;
                return;
            }
            valueClick = false;
            dragging = true;
            dragStartNorm = norm;
            lastY = e.position.y; lastX = e.position.x;
            beginGesture();
            e.source.enableUnboundedMouseMovement (true);
            repaint();
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (! dragging) return;
            const float dy = lastY - e.position.y, dx = e.position.x - lastX;
            lastY = e.position.y; lastX = e.position.x;
            const float sens = e.mods.isShiftDown() || e.mods.isCommandDown() ? 1.0f / 1200.0f : 1.0f / 220.0f;
            setNormFromUser (norm + (dy + dx * 0.5f) * sens);
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            if (valueClick)
            {
                valueClick = false;
                if (valueArea().contains (e.position) && ! e.mouseWasDraggedSinceMouseDown()) showEditor();
                return;
            }
            if (! dragging) return;
            dragging = false;
            e.source.enableUnboundedMouseMovement (false);
            endGesture();
            repaint();
        }

        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            if (! isEnabled() || valueArea().contains (e.position)) return;
            beginGesture();
            setNormFromUser (defaultNorm);
            endGesture();
        }

        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            if (! isEnabled()) return;
            const float delta = (std::abs (w.deltaY) > std::abs (w.deltaX) ? w.deltaY : -w.deltaX) * (w.isReversed ? -1.0f : 1.0f);
            if (std::abs (delta) < 1.0e-5f) return;
            if (! wheelGesture) { beginGesture(); wheelGesture = true; }
            setNormFromUser (norm + delta * (e.mods.isShiftDown() ? 0.01f : 0.06f));
            wheelEndCountdown = 20;
            startAnimating();
        }

        void mouseMove (const juce::MouseEvent& e) override
        {
            const bool hv = valueArea().contains (e.position) && ! knobArea().expanded (0, -4).contains (e.position);
            if (hv != hoverValue) { hoverValue = hv; setMouseCursor (hv ? juce::MouseCursor::IBeamCursor : juce::MouseCursor::NormalCursor); repaint(); }
        }

        void mouseEnter (const juce::MouseEvent&) override { startAnimating(); }
        void mouseExit (const juce::MouseEvent&) override { hoverValue = false; setMouseCursor (juce::MouseCursor::NormalCursor); startAnimating(); }
        void enablementChanged() override { repaint(); }

    private:
        juce::Rectangle<float> captionArea() const { return getLocalBounds().toFloat().removeFromTop (showCaption ? 14.0f : 0.0f); }
        juce::Rectangle<float> valueArea() const   { return getLocalBounds().toFloat().removeFromBottom (17.0f); }
        juce::Rectangle<float> knobArea() const
        {
            auto r = getLocalBounds().toFloat();
            r.removeFromTop (showCaption ? 16.0f : 0.0f);
            r.removeFromBottom (18.0f);
            const float d = juce::jmax (10.0f, juce::jmin (r.getWidth(), r.getHeight()));
            return r.withSizeKeepingCentre (d, d);
        }

        juce::String currentText() const
        {
            if (! toText) return {};
            return toText (range.convertFrom0to1 (norm));
        }

        void setNormInternal (float n)
        {
            n = juce::jlimit (0.0f, 1.0f, n);
            if (std::abs (n - norm) < 1.0e-7f && ! firstValue) return;
            firstValue = false;
            norm = n;
            if (dragging || wheelGesture) shown = norm; // direct while the user drives it
            startAnimating();
            repaint();
        }

        void setNormFromUser (float n)
        {
            n = juce::jlimit (0.0f, 1.0f, n);
            const float v = range.convertFrom0to1 (n);
            if (attachment) attachment->setValueAsPartOfGesture (v);
            else if (plainSetter) plainSetter (v);
            norm = range.convertTo0to1 (v);
            shown = n;
            repaint();
        }

        void beginGesture() { if (attachment) attachment->beginGesture(); else if (onPlainGestureStart) onPlainGestureStart(); }
        void endGesture()   { if (attachment) attachment->endGesture(); }

        void showEditor()
        {
            editor = std::make_unique<juce::TextEditor>();
            editor->setFont (valueFont());
            editor->setJustification (juce::Justification::centred);
            editor->setText (currentText(), false);
            editor->setSelectAllWhenFocused (true);
            editor->setIndents (2, 1);
            addAndMakeVisible (*editor);
            resized();
            editor->grabKeyboardFocus();
            editor->onReturnKey = [this]
            {
                const float v = fromText (editor->getText());
                beginGesture();
                setNormFromUser (range.convertTo0to1 (juce::jlimit (range.start, range.end, v)));
                endGesture();
                closeEditorAsync();
            };
            editor->onEscapeKey = [this] { closeEditorAsync(); };
            editor->onFocusLost = [this] { closeEditorAsync(); };
        }

        void closeEditorAsync()
        {
            juce::Component::SafePointer<Dial> safe (this);
            juce::MessageManager::callAsync ([safe] { if (safe != nullptr) { safe->editor.reset(); safe->repaint(); } });
        }

        bool animate() override
        {
            bool moving = approach (hoverAmt, isMouseOverOrDragging() ? 1.0f : 0.0f, 0.3f);
            moving |= approach (shown, norm, 0.35f);
            if (wheelGesture && --wheelEndCountdown <= 0) { wheelGesture = false; endGesture(); }
            moving |= wheelGesture;
            repaint();
            return moving;
        }

    public:
        /** Called before a plain value starts changing (for undo transactions). */
        std::function<void()> onPlainGestureStart;

    private:
        juce::String caption;
        juce::Colour colour { col::accent }, secondaryColour { col::red };
        bool bipolar = false, showCaption = true, dragging = false, valueClick = false, hoverValue = false, wheelGesture = false, firstValue = true;
        int wheelEndCountdown = 0;
        float norm = 0.0f, shown = 0.0f, defaultNorm = 0.0f, dragStartNorm = 0.0f, lastY = 0.0f, lastX = 0.0f, hoverAmt = 0.0f, secondary = 0.0f;
        juce::NormalisableRange<float> range;
        juce::RangedAudioParameter* param = nullptr;
        std::unique_ptr<juce::ParameterAttachment> attachment;
        std::function<juce::String (float)> toText;
        std::function<float (const juce::String&)> fromText;
        std::function<void (float)> plainSetter;
        std::unique_ptr<juce::TextEditor> editor;
    };
}
