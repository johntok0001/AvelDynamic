#include "Display.h"

namespace avel::ui
{
    using namespace avel::dsp;

    static juce::String minusText (const juce::String& s) { return s.replace ("-", juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92"))); }

    Display::Display (AvelDynamicProcessor& p)
        : proc (p),
          thresholdParam (*p.apvts.getParameter (params::id::threshold)),
          ratioParam (*p.apvts.getParameter (params::id::ratio)),
          threshold (thresholdParam, [this] (float v) { thresholdDb = v; repaint(); }, &p.undo),
          ratio (ratioParam, [this] (float v) { ratioValue = v; repaint(); }, &p.undo)
    {
        hist.assign ((size_t) historySize, HistoryPoint {});
        proc.history.pop ([] (const HistoryPoint&) {}); // drop points queued while the editor was closed
        threshold.sendInitialUpdate();
        ratio.sendInitialUpdate();
        setOpaque (false);
    }

    juce::Rectangle<float> Display::graphArea() const
    {
        auto r = getLocalBounds().toFloat();
        const float bottom = r.getBottom() - stripH - reservedBottom - 10.0f;
        return { r.getX(), r.getY() + 16.0f, r.getWidth() - 36.0f, juce::jmax (60.0f, bottom - (r.getY() + 16.0f)) };
    }

    juce::Rectangle<float> Display::transferArea() const
    {
        auto g = graphArea();
        const float s = juce::jlimit (110.0f, 196.0f, g.getHeight() * 0.62f);
        return { g.getX() + 14.0f, g.getY() + 0.0f, s, s };
    }

    float Display::dbToY (float db, juce::Rectangle<float> g) const
    {
        return juce::jmap (db, dbTop, dbBottom, g.getY(), g.getBottom());
    }

    float Display::yToDb (float y, juce::Rectangle<float> g) const
    {
        return juce::jmap (y, g.getY(), g.getBottom(), dbTop, dbBottom);
    }

    void Display::resized() {}

    void Display::tick()
    {
        bool any = false;
        proc.history.pop ([this, &any] (const HistoryPoint& p)
        {
            hist[(size_t) writePos] = p;
            writePos = (writePos + 1) % historySize;
            grNow = p.grDb;
            any = true;
        });
        detector = proc.detectorDb.load();
        if (any) startAnimating();
        repaint();
    }

    bool Display::animate()
    {
        bool m = approach (lineHover, hoverLine || drag == Drag::threshold ? 1.0f : 0.0f, 0.3f);
        m |= approach (transferHover, hoverTransfer || drag == Drag::transfer ? 1.0f : 0.0f, 0.3f);
        m |= approach (detectorShown, juce::jmax (-80.0f, detector), 0.35f);
        m |= approach (grShown, grNow, 0.3f);
        if (wheelGesture && --wheelCountdown <= 0) { wheelGesture = false; if (wheelAtt != nullptr) wheelAtt->endGesture(); wheelAtt = nullptr; }
        m |= wheelGesture;
        repaint();
        return m;
    }

    //==============================================================================
    void Display::paint (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat();

        // panel
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff17171a), 0, r.getY(), juce::Colour (0xff0c0c0e), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 14.0f);
        {
            juce::ColourGradient glow (product::accent.withAlpha (0.035f), r.getCentreX(), r.getY(),
                                       product::accent.withAlpha (0.0f), r.getCentreX(), r.getY() + r.getHeight() * 0.75f, true);
            g.setGradientFill (glow);
            g.fillRoundedRectangle (r, 14.0f);
        }

        auto ga = graphArea();
        {
            juce::Graphics::ScopedSaveState s (g);
            juce::Path clip; clip.addRoundedRectangle (r.reduced (1.0f), 13.0f);
            g.reduceClipRegion (clip);
            drawScale (g, ga);
            drawHistory (g, ga);
        }

        // threshold line + knee band
        {
            const float knee = proc.effectiveKnee();
            const float y = dbToY (thresholdDb, ga);
            if (knee > 0.1f)
            {
                const float y0 = dbToY (juce::jmin (0.0f, thresholdDb + knee * 0.5f), ga), y1 = dbToY (juce::jmax (dbBottom, thresholdDb - knee * 0.5f), ga);
                g.setColour (product::accent.withAlpha (0.045f + 0.03f * lineHover));
                g.fillRect (juce::Rectangle<float> (ga.getX(), y0, ga.getWidth(), y1 - y0));
            }
            g.setColour (product::accent.withAlpha (0.10f * lineHover));
            g.fillRect (juce::Rectangle<float> (ga.getX(), y - 3.0f, ga.getWidth(), 6.0f));
            g.setColour (product::accent.withAlpha (0.55f + 0.4f * lineHover));
            g.fillRect (juce::Rectangle<float> (ga.getX(), y - 0.5f - 0.5f * lineHover, ga.getWidth(), 1.0f + lineHover));

            const auto txt = minusText ("Threshold  " + params::formatDb (thresholdDb));
            const auto f = font (11.0f, Weight::semibold);
            const float w = textWidth (f, txt) + 20.0f;
            auto pill = juce::Rectangle<float> (ga.getRight() - w - 10.0f, y - 11.0f, w, 22.0f);
            pill.setY (juce::jlimit (ga.getY() - 4.0f, ga.getBottom() - 18.0f, pill.getY()));
            drawPill (g, pill, col::pill);
            g.setColour (product::accent.interpolatedWith (col::label, 0.35f + 0.4f * lineHover));
            g.setFont (f);
            g.drawText (txt, pill, juce::Justification::centred, false);
        }

        // GR / makeup info pill (top centre)
        {
            const auto f = font (11.0f, Weight::semibold);
            juce::String a = "GR  " + juce::String (grShown, 1) + " dB";
            if (grShown >= 0.05f) a = minusText ("GR  -" + juce::String (grShown, 1) + " dB");
            const float mk = proc.getMakeupDb();
            const juce::String b = mk > 0.05f ? "Auto Gain  +" + juce::String (mk, 1) + " dB" : juce::String();
            const float wa = textWidth (f, a) + 34.0f, wb = b.isEmpty() ? 0.0f : textWidth (f, b) + 24.0f;
            auto pill = juce::Rectangle<float> (wa + wb, 24.0f).withCentre ({ ga.getCentreX() + 40.0f, ga.getY() + 4.0f });
            drawPill (g, pill, col::pill);
            auto pa = pill.removeFromLeft (wa);
            g.setColour (col::red.withAlpha (0.25f + 0.75f * juce::jlimit (0.0f, 1.0f, grShown / 3.0f)));
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ pa.getX() + 15.0f, pa.getCentreY() }));
            g.setColour (col::label);
            g.setFont (f);
            g.drawText (a, pa.withTrimmedLeft (24.0f), juce::Justification::centredLeft, false);
            if (b.isNotEmpty())
            {
                g.setColour (col::stroke);
                g.fillRect (juce::Rectangle<float> (pill.getX(), pill.getY() + 6.0f, 1.0f, pill.getHeight() - 12.0f));
                g.setColour (col::label2);
                g.drawText (b, pill, juce::Justification::centred, false);
            }
        }

        drawTransfer (g, transferArea());

        // bottom strip: time labels
        {
            auto strip = juce::Rectangle<float> (r.getX(), r.getBottom() - stripH, ga.getWidth(), stripH);
            g.setFont (font (10.5f, Weight::medium));
            g.setColour (col::label3);
            for (int s = 1; s <= 4; ++s)
            {
                const float x = ga.getRight() - ga.getWidth() * (float) s / 5.0f;
                g.drawText (minusText ("-" + juce::String (s) + " s"), juce::Rectangle<float> (x - 30.0f, strip.getY(), 60.0f, strip.getHeight() - 4.0f),
                            juce::Justification::centred, false);
            }
            g.drawText ("now", juce::Rectangle<float> (ga.getRight() - 44.0f, strip.getY(), 40.0f, strip.getHeight() - 4.0f), juce::Justification::centredRight, false);
        }

        g.setColour (col::stroke);
        g.drawRoundedRectangle (r.reduced (0.5f), 14.0f, 1.0f);
    }

    void Display::drawScale (juce::Graphics& g, juce::Rectangle<float> ga)
    {
        auto r = getLocalBounds().toFloat();
        const float bottom = r.getBottom() - stripH;

        // time grid
        for (int s = 1; s <= 4; ++s)
        {
            const float x = ga.getRight() - ga.getWidth() * (float) s / 5.0f;
            g.setColour (juce::Colours::white.withAlpha (0.03f));
            g.fillRect (juce::Rectangle<float> (x, r.getY(), 1.0f, bottom - r.getY()));
        }

        g.setFont (font (10.5f, Weight::medium));
        for (int db = 0; db >= (int) dbBottom; db -= 6)
        {
            const float y = dbToY ((float) db, ga);
            const bool major = db % 12 == 0;
            g.setColour (juce::Colours::white.withAlpha (major ? 0.075f : 0.03f));
            g.fillRect (juce::Rectangle<float> (ga.getX(), y, ga.getWidth(), 1.0f));
            if (major || ga.getHeight() > 300.0f)
            {
                g.setColour (col::label3);
                g.drawText (minusText (juce::String (db)), juce::Rectangle<float> (ga.getRight() + 2.0f, y - 8.0f, 30.0f, 16.0f), juce::Justification::centredLeft, false);
            }
        }
    }

    void Display::drawHistory (juce::Graphics& g, juce::Rectangle<float> ga)
    {
        auto r = getLocalBounds().toFloat();
        const float base = r.getBottom() - stripH;
        const int W = juce::jmax (2, (int) ga.getWidth());
        const float x0 = ga.getX();

        juce::Path inFill, outFill, outLine, grLine, grFill;
        inFill.startNewSubPath (x0, base);
        outFill.startNewSubPath (x0, base);
        grFill.startNewSubPath (x0, ga.getY());

        auto clampY = [&] (float db) { return juce::jmin (base, dbToY (juce::jmax (db, -100.0f), ga)); };
        bool hadData = false;

        for (int px = 0; px <= W; px += 1)
        {
            const int k0 = (int) ((float) px / (float) (W + 1) * historySize);
            const int k1 = juce::jmax (k0 + 1, (int) ((float) (px + 1) / (float) (W + 1) * historySize));
            float in = -120.0f, out = -120.0f, gr = 0.0f;
            for (int k = k0; k < k1 && k < historySize; ++k)
            {
                const auto& p = hist[(size_t) ((writePos + k) % historySize)];
                in = juce::jmax (in, p.inDb); out = juce::jmax (out, p.outDb); gr = juce::jmax (gr, p.grDb);
            }
            const float x = x0 + (float) px * ga.getWidth() / (float) W;
            const float yIn = clampY (in), yOut = clampY (out);
            const float yGr = juce::jmin (base, dbToY (-gr, ga));
            const bool hasData = in > -110.0f || out > -110.0f;
            inFill.lineTo (x, yIn);
            outFill.lineTo (x, yOut);
            if (hasData && ! hadData) { outLine.startNewSubPath (x, yOut); grLine.startNewSubPath (x, yGr); }
            else if (hasData) { outLine.lineTo (x, yOut); grLine.lineTo (x, yGr); }
            hadData = hasData;
            grFill.lineTo (x, yGr);
        }
        const float xe = x0 + ga.getWidth();
        inFill.lineTo (xe, base); inFill.closeSubPath();
        outFill.lineTo (xe, base); outFill.closeSubPath();
        grFill.lineTo (xe, ga.getY()); grFill.closeSubPath();

        // input: dim blue-grey
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6b7d9c).withAlpha (0.30f), 0, ga.getY(), juce::Colour (0xff6b7d9c).withAlpha (0.12f), 0, base, false));
        g.fillPath (inFill);

        // output: white-ish gradient + 1 px line
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.22f), 0, ga.getY(), juce::Colours::white.withAlpha (0.04f), 0, base, false));
        g.fillPath (outFill);
        g.setColour (juce::Colours::white.withAlpha (0.62f));
        g.strokePath (outLine, juce::PathStrokeType (1.0f));

        // gain reduction from the top
        g.setGradientFill (juce::ColourGradient (col::red.withAlpha (0.20f), 0, ga.getY(), col::red.withAlpha (0.05f), 0, ga.getY() + ga.getHeight() * 0.4f, false));
        g.fillPath (grFill);
        g.setColour (col::red.withAlpha (0.25f));
        g.strokePath (grLine, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (col::red);
        g.strokePath (grLine, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void Display::drawTransfer (juce::Graphics& g, juce::Rectangle<float> t)
    {
        {
            juce::Path p; p.addRoundedRectangle (t, 12.0f);
            juce::DropShadow (juce::Colours::black.withAlpha (0.45f), 18, { 0, 6 }).drawForPath (g, p);
        }
        g.setColour (juce::Colour (0xf2111113));
        g.fillRoundedRectangle (t, 12.0f);
        g.setColour (transferHover > 0.01f ? col::stroke.interpolatedWith (col::strokeHi, transferHover) : col::stroke);
        g.drawRoundedRectangle (t.reduced (0.5f), 12.0f, 1.0f);

        auto inner = t.reduced (10.0f);
        g.setFont (captionFont());
        g.setColour (col::label3);
        g.drawText ("TRANSFER", inner.removeFromTop (12.0f), juce::Justification::centredLeft, false);
        g.setColour (col::label2);
        g.setFont (font (10.5f, Weight::semibold));
        g.drawText (params::formatRatio (ratioValue), t.reduced (10.0f).removeFromTop (12.0f), juce::Justification::centredRight, false);
        inner.removeFromTop (6.0f);
        const float side = juce::jmin (inner.getWidth(), inner.getHeight());
        auto plot = inner.withSizeKeepingCentre (side, side);

        auto px = [&] (float db) { return juce::jmap (db, -60.0f, 0.0f, plot.getX(), plot.getRight()); };
        auto py = [&] (float db) { return juce::jmap (juce::jmax (-60.0f, db), -60.0f, 0.0f, plot.getBottom(), plot.getY()); };

        for (int db = -48; db < 0; db += 12)
        {
            g.setColour (juce::Colours::white.withAlpha (0.045f));
            g.fillRect (juce::Rectangle<float> (px ((float) db), plot.getY(), 1.0f, plot.getHeight()));
            g.fillRect (juce::Rectangle<float> (plot.getX(), py ((float) db), plot.getWidth(), 1.0f));
        }
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawRect (plot, 1.0f);

        // unity
        {
            juce::Path u; u.startNewSubPath (plot.getX(), plot.getBottom()); u.lineTo (plot.getRight(), plot.getY());
            const float dash[] { 3.0f, 3.0f };
            juce::Path d; juce::PathStrokeType (1.0f).createDashedStroke (d, u, dash, 2);
            g.setColour (col::label4);
            g.fillPath (d);
        }

        const auto s = proc.currentSettings();
        const float knee = proc.effectiveKnee();

        // threshold marker
        g.setColour (product::accent.withAlpha (0.25f));
        g.fillRect (juce::Rectangle<float> (px (s.thresholdDb), plot.getY(), 1.0f, plot.getHeight()));

        juce::Path curve, fillP;
        fillP.startNewSubPath (plot.getX(), plot.getBottom());
        for (int i = 0; i <= 240; ++i)
        {
            const float x = -60.0f + (float) i * 0.25f;
            const float y = Curve::output (x, s.thresholdDb, s.ratio, knee, s.rangeDb);
            if (i == 0) curve.startNewSubPath (px (x), py (y)); else curve.lineTo (px (x), py (y));
            fillP.lineTo (px (x), py (y));
        }
        fillP.lineTo (plot.getRight(), plot.getBottom()); fillP.closeSubPath();
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (plot.toNearestInt());
            g.setGradientFill (juce::ColourGradient (product::accent.withAlpha (0.16f), 0, plot.getY(), product::accent.withAlpha (0.0f), 0, plot.getBottom(), false));
            g.fillPath (fillP);
            g.setColour (product::accent.withAlpha (0.18f));
            g.strokePath (curve, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (product::accent);
            g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // live detector dot (node style)
        if (detectorShown > -59.0f)
        {
            const float lx = juce::jmin (0.0f, detectorShown);
            const auto c = juce::Point<float> (px (lx), py (Curve::output (lx, s.thresholdDb, s.ratio, knee, s.rangeDb)));
            const bool over = detectorShown > s.thresholdDb - knee * 0.5f;
            const auto nc = over ? col::red.interpolatedWith (col::orange, 0.3f) : product::accent;
            g.setColour (nc.withAlpha (0.22f));
            g.fillEllipse (juce::Rectangle<float> (18.0f, 18.0f).withCentre (c));
            g.setGradientFill (juce::ColourGradient (nc.brighter (0.3f), c.x, c.y - 5.0f, nc.darker (0.2f), c.x, c.y + 5.0f, false));
            g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (c));
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.drawEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (c), 1.4f);
        }
    }

    //==============================================================================
    bool Display::overThresholdLine (juce::Point<float> p) const
    {
        auto ga = graphArea();
        return ga.expanded (0, 6).contains (p) && std::abs (p.y - dbToY (thresholdDb, ga)) < 6.0f && ! transferArea().contains (p);
    }

    void Display::mouseMove (const juce::MouseEvent& e)
    {
        const bool l = overThresholdLine (e.position);
        const bool t = transferArea().contains (e.position);
        if (l != hoverLine || t != hoverTransfer)
        {
            hoverLine = l; hoverTransfer = t;
            setMouseCursor (l || t ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
            setTooltip (t ? "Transfer curve. Drag up/down to set the threshold, scroll to change the ratio. The dot shows the detector level."
                      : l ? "Threshold. Drag to move it, double-click to reset." : juce::String());
            startAnimating();
        }
    }

    void Display::mouseExit (const juce::MouseEvent&)
    {
        hoverLine = hoverTransfer = false;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        startAnimating();
    }

    void Display::mouseDown (const juce::MouseEvent& e)
    {
        if (transferArea().contains (e.position)) drag = Drag::transfer;
        else if (overThresholdLine (e.position)) drag = Drag::threshold;
        else { drag = Drag::none; return; }
        dragOffset = thresholdDb;
        threshold.beginGesture();
        startAnimating();
    }

    void Display::mouseDrag (const juce::MouseEvent& e)
    {
        if (drag == Drag::none) return;
        const float span = drag == Drag::transfer ? transferArea().getHeight() - 30.0f : graphArea().getHeight();
        const float fine = e.mods.isShiftDown() ? 0.2f : 1.0f;
        const float v = juce::jlimit (-60.0f, 0.0f, dragOffset - (float) e.getDistanceFromDragStartY() * 60.0f / juce::jmax (40.0f, span) * fine);
        threshold.setValueAsPartOfGesture (v);
    }

    void Display::mouseUp (const juce::MouseEvent&)
    {
        if (drag != Drag::none) threshold.endGesture();
        drag = Drag::none;
        startAnimating();
    }

    void Display::mouseDoubleClick (const juce::MouseEvent& e)
    {
        if (transferArea().contains (e.position) || overThresholdLine (e.position))
            threshold.setValueAsCompleteGesture (thresholdParam.convertFrom0to1 (thresholdParam.getDefaultValue()));
    }

    void Display::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
    {
        const float delta = (std::abs (w.deltaY) > std::abs (w.deltaX) ? w.deltaY : -w.deltaX) * (w.isReversed ? -1.0f : 1.0f);
        if (std::abs (delta) < 1.0e-5f) return;
        if (transferArea().contains (e.position))
        {
            if (wheelGesture && wheelAtt != &ratio) { wheelAtt->endGesture(); wheelGesture = false; }
            if (! wheelGesture) { ratio.beginGesture(); wheelGesture = true; wheelAtt = &ratio; }
            const float n = juce::jlimit (0.0f, 1.0f, ratioParam.getValue() + delta * (e.mods.isShiftDown() ? 0.01f : 0.05f));
            ratio.setValueAsPartOfGesture (ratioParam.convertFrom0to1 (n));
        }
        else if (overThresholdLine (e.position))
        {
            if (wheelGesture && wheelAtt != &threshold) { wheelAtt->endGesture(); wheelGesture = false; }
            if (! wheelGesture) { threshold.beginGesture(); wheelGesture = true; wheelAtt = &threshold; }
            threshold.setValueAsPartOfGesture (juce::jlimit (-60.0f, 0.0f, thresholdDb + delta * (e.mods.isShiftDown() ? 1.0f : 4.0f)));
        }
        else return;
        wheelCountdown = 20;
        startAnimating();
    }
}
