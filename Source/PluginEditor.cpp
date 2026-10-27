#include "PluginEditor.h"

using namespace avel::ui;
namespace pid = avel::params::id;

static juce::Path logoGlyph()
{
    // a compressor's transfer curve: unity, then a soft knee into a flat line
    juce::Path p;
    p.startNewSubPath (5.5f, 18.5f);
    p.lineTo (11.0f, 13.0f);
    p.quadraticTo (13.0f, 11.0f, 16.0f, 10.2f);
    p.lineTo (19.0f, 9.4f);
    p.startNewSubPath (5.5f, 5.5f);
    p.lineTo (5.5f, 18.5f);
    p.lineTo (19.0f, 18.5f);
    return p;
}

AvelDynamicEditor::AvelDynamicEditor (AvelDynamicProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);

    auto* um = &proc.undo;
    auto& ap = proc.apvts;

    // ---- top bar
    addAndMakeVisible (presetPill);
    for (auto* b : { &undoBtn, &redoBtn, &copyBtn }) addAndMakeVisible (*b);
    undoBtn.setTooltip ("Undo");
    redoBtn.setTooltip ("Redo");
    undoBtn.onClick = [this] { proc.undo.undo(); };
    redoBtn.onClick = [this] { proc.undo.redo(); };

    styleSeg.setItems (avel::params::styleNames());
    styleSeg.setTooltips (avel::params::styleTips());
    styleSeg.bind (*ap.getParameter (pid::style), um);
    addAndMakeVisible (styleSeg);

    abSeg.setItems ({ "A", "B" });
    abSeg.setTooltips ({ "Setting A", "Setting B. Switch to compare two settings." });
    abSeg.setSelected (proc.getABIndex(), juce::dontSendNotification);
    abSeg.snap();
    abSeg.onChange = [this] (int i) { proc.selectAB (i); };
    addAndMakeVisible (abSeg);
    copyBtn.setTooltip ("Copy the current setting to the other A/B slot");
    copyBtn.onClick = [this] { proc.copyCurrentToOther(); };

    autoGain.setLabel ("Auto Gain");
    autoGain.bind (*ap.getParameter (pid::autoGain), um);
    autoGain.setTooltip ("Auto Gain: compensates the level lost to compression so you can judge the sound, not the loudness");
    addAndMakeVisible (autoGain);

    // ---- display + card
    addAndMakeVisible (display);
    addAndMakeVisible (card);
    addChildComponent (scCard);

    setupDial (threshold, pid::threshold, "Threshold: level above which compression starts. You can also drag the line in the display.");
    setupDial (ratio, pid::ratio, "Ratio: how strongly the level above the threshold is reduced");
    setupDial (knee, pid::knee, "Knee: width of the soft transition around the threshold");
    setupDial (range, pid::range, "Range: maximum gain reduction. Full means no limit.");
    setupDial (attack, pid::attack, "Attack: how fast the compressor reacts to rising levels");
    setupDial (release, pid::release, "Release: how fast the gain recovers");
    setupDial (hold, pid::hold, "Hold: keeps the reduction for a moment before the release starts");
    setupDial (lookahead, pid::lookahead, "Lookahead: delays the audio so the compressor can react before a peak. Adds latency.");
    setupDial (drive, pid::drive, "Drive: amount of the selected character (saturation)", col::orange);
    setupDial (link, pid::link, "Stereo link: 100 % compresses both channels together, 0 % independently");
    for (auto* d : { &threshold, &ratio, &knee, &range, &attack, &release, &hold, &lookahead, &drive, &link })
        card.addAndMakeVisible (*d);

    autoRelease.setLabel ("Auto Release");
    autoRelease.setOnColour (col::accent);
    autoRelease.bind (*ap.getParameter (pid::autoRelease), um);
    autoRelease.setTooltip ("Auto release: program-dependent release. Fast after short peaks, slower on sustained compression.");
    card.addAndMakeVisible (autoRelease);

    characterSeg.setItems ({ "Off", "Warm", "Bright" });
    characterSeg.setTooltips ({ "No saturation", "Warm: even harmonics with a soft top end", "Bright: odd harmonics in the upper range" });
    characterSeg.bind (*ap.getParameter (pid::character), um);
    card.addAndMakeVisible (characterSeg);

    channelSeg.setItems ({ "Stereo", "M/S", "Mid", "Side" });
    channelSeg.setTooltips ({ "Stereo: left and right (see Link)", "Mid/Side: mid and side compressed independently",
                              "Mid only: compress the centre, leave the sides", "Side only: compress the sides, leave the centre" });
    channelSeg.bind (*ap.getParameter (pid::channelMode), um);
    card.addAndMakeVisible (channelSeg);

    osSeg.setItems ({ "1x", "2x", "4x" });
    osSeg.setTooltips ({ "Oversampling off (lowest CPU and latency)", "2x oversampling", "4x oversampling: least aliasing with fast attack and Drive" });
    osSeg.bind (*ap.getParameter (pid::oversampling), um);
    card.addAndMakeVisible (osSeg);

    scButton.setText ("Sidechain");
    scButton.setClickingTogglesState (true);
    scButton.setTooltip ("Show the sidechain filters and the external sidechain input");
    scButton.onClick = [this] { scOpen = scButton.getToggleState(); proc.uiState.setProperty ("scOpen", scOpen, nullptr); if (scOpen) scCard.setVisible (true); };
    card.addAndMakeVisible (scButton);

    card.decorate = [this] (juce::Graphics& g)
    {
        g.setFont (captionFont());
        g.setColour (col::label3);
        g.drawText ("CHARACTER", characterCaption, juce::Justification::centredRight, false);
        g.drawText ("OS", osCaption, juce::Justification::centredRight, false);

        g.setColour (col::stroke);
        for (auto* d : { &range, &lookahead })
        {
            auto b = d->getBounds().toFloat();
            const float x = b.getRight() + 14.0f;
            g.fillRect (juce::Rectangle<float> (x, b.getY() + 10.0f, 1.0f, b.getHeight() - 22.0f));
        }
        g.fillRect (juce::Rectangle<float> (14.0f, (float) autoRelease.getY() - 5.0f, (float) card.getWidth() - 28.0f, 1.0f));
    };

    // ---- sidechain card
    setupDial (scHpf, pid::scHpf, "Sidechain high-pass: keeps low end (kick, bass) from triggering the compressor");
    setupDial (scLpf, pid::scLpf, "Sidechain low-pass: makes the compressor ignore the top end");
    scCard.addAndMakeVisible (scHpf);
    scCard.addAndMakeVisible (scLpf);
    scExternal.setLabel ("External");
    scExternal.setOnColour (col::accent);
    scExternal.bind (*ap.getParameter (pid::scExternal), um);
    scExternal.setTooltip ("Use the plug-in's sidechain input as the detector (route a track to it in your DAW)");
    scCard.addAndMakeVisible (scExternal);
    scListen.setText ("Listen");
    scListen.setClickingTogglesState (true);
    scListen.setTint (col::yellow);
    scListen.setTooltip ("Listen to the filtered sidechain signal");
    {
        auto* lp = ap.getParameter (pid::scListen);
        scListen.setToggleState (lp->getValue() > 0.5f, juce::dontSendNotification);
        scListen.onClick = [this, lp]
        {
            proc.undo.beginNewTransaction();
            lp->beginChangeGesture();
            lp->setValueNotifyingHost (scListen.getToggleState() ? 1.0f : 0.0f);
            lp->endChangeGesture();
        };
    }
    scCard.addAndMakeVisible (scListen);
    scCard.decorate = [this] (juce::Graphics& g)
    {
        auto r = scCard.getLocalBounds().toFloat().reduced (16.0f, 12.0f);
        auto right = r.withTrimmedLeft ((float) scExternal.getX() - r.getX());
        g.setFont (captionFont());
        g.setColour (col::label3);
        g.drawText ("SIDECHAIN", right.removeFromTop (14.0f), juce::Justification::centredLeft, false);
        if (scExternal.getState() && ! proc.sidechainConnected.load())
        {
            g.setFont (font (11.0f, Weight::medium));
            g.setColour (col::orange);
            g.drawText ("No sidechain input routed", juce::Rectangle<float> (right.getX(), (float) scListen.getBottom() + 4.0f, right.getWidth(), 16.0f),
                        juce::Justification::centredLeft, false);
        }
        g.setColour (col::stroke);
        g.fillRect (juce::Rectangle<float> ((float) scLpf.getRight() + 10.0f, 18.0f, 1.0f, (float) scCard.getHeight() - 36.0f));
    };

    // ---- right column
    addAndMakeVisible (meters);
    setupDial (inputDial, pid::input, "Input gain before the compressor");
    setupDial (mixDial, pid::mix, "Mix: blend the compressed signal with the dry signal (parallel compression)");
    setupDial (outputDial, pid::output, "Output gain");
    inputDial.setBipolar (true);
    outputDial.setBipolar (true);
    for (auto* d : { &inputDial, &mixDial, &outputDial }) addAndMakeVisible (*d);

    scOpen = (bool) proc.uiState.getProperty ("scOpen", false);
    scButton.setToggleState (scOpen, juce::dontSendNotification);
    scAnim = scOpen ? 1.0f : 0.0f;
    scCard.setVisible (scOpen);

    setResizable (true, true);
    setResizeLimits (1080, 640, 2400, 1500);
    setSize (juce::jlimit (1080, 2400, (int) proc.uiState.getProperty ("width", 1180)),
             juce::jlimit (640, 1500, (int) proc.uiState.getProperty ("height", 720)));
    sizeRestored = true;
    startTimerHz (60);
}

AvelDynamicEditor::~AvelDynamicEditor()
{
    stopTimer();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void AvelDynamicEditor::setupDial (Dial& d, const char* paramId, const juce::String& tip, juce::Colour c)
{
    d.bind (*proc.apvts.getParameter (paramId), &proc.undo);
    d.setTooltip (tip);
    d.setColour (c);
}

//==============================================================================
void AvelDynamicEditor::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff141416), 0, 0, juce::Colour (0xff09090a), 0, r.getBottom(), false));
    g.fillAll();

    // logo mark
    auto logo = juce::Rectangle<float> (16.0f, 16.0f, 26.0f, 26.0f);
    g.setGradientFill (juce::ColourGradient (product::accentA, logo.getX(), logo.getY(), product::accentB, logo.getRight(), logo.getBottom(), false));
    g.fillRoundedRectangle (logo, 7.0f);
    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.drawRoundedRectangle (logo.reduced (0.5f), 7.0f, 1.0f);
    {
        const float s = logo.getWidth() / 24.0f;
        g.setColour (juce::Colours::white);
        g.strokePath (logoGlyph(), juce::PathStrokeType (1.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::scale (s).translated (logo.getX(), logo.getY()));
    }

    const auto fA = font (15.0f, Weight::semibold), fB = font (15.0f, Weight::regular);
    const float wa = textWidth (fA, "Avel");
    g.setFont (fA); g.setColour (col::label);
    g.drawText ("Avel", juce::Rectangle<float> (52.0f, 16.0f, wa + 2.0f, 26.0f), juce::Justification::centredLeft, false);
    g.setFont (fB); g.setColour (col::label2);
    g.drawText ("Dynamic", juce::Rectangle<float> (52.0f + wa + 5.0f, 16.0f, 80.0f, 26.0f), juce::Justification::centredLeft, false);

    // right column panel
    auto c = columnBounds.toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff17171a), 0, c.getY(), juce::Colour (0xff0c0c0e), 0, c.getBottom(), false));
    g.fillRoundedRectangle (c, 14.0f);
    g.setColour (col::stroke);
    g.drawRoundedRectangle (c.reduced (0.5f), 14.0f, 1.0f);
    g.fillRect (juce::Rectangle<float> (c.getX() + 14.0f, (float) inputDial.getY() - 8.0f, c.getWidth() - 28.0f, 1.0f));

    // latency hint
    const int lat = proc.getLatencySamples();
    if (lat > 0)
    {
        g.setFont (font (10.0f, Weight::medium));
        g.setColour (col::label3);
        const float ms = (float) lat * 1000.0f / (float) juce::jmax (1.0, proc.getSampleRate());
        g.drawText ("Latency " + juce::String (ms, 1) + " ms", juce::Rectangle<float> (c.getX(), c.getBottom() + 0.0f, c.getWidth(), 12.0f),
                    juce::Justification::centred, false);
    }
}

void AvelDynamicEditor::resized()
{
    auto b = getLocalBounds();
    if (sizeRestored)
    {
        proc.uiState.setProperty ("width", getWidth(), nullptr);
        proc.uiState.setProperty ("height", getHeight(), nullptr);
    }

    // ---- top bar (58 px)
    const auto fA = font (15.0f, Weight::semibold), fB = font (15.0f, Weight::regular);
    const int titleRight = 52 + (int) (textWidth (fA, "Avel") + 5.0f + textWidth (fB, "Dynamic"));
    presetPill.setBounds (titleRight + 18, 14, 196, 30);
    const int leftEnd = presetPill.getRight();

    int x = getWidth() - 16;
    const int agW = autoGain.getIdealWidth();
    autoGain.setBounds (x - agW, 17, agW, 24); x -= agW + 18;
    copyBtn.setBounds (x - 28, 15, 28, 28); x -= 28 + 4;
    abSeg.setBounds (x - 64, 15, 64, 28); x -= 64 + 14;
    redoBtn.setBounds (x - 28, 15, 28, 28); x -= 28 + 2;
    undoBtn.setBounds (x - 28, 15, 28, 28); x -= 28;
    const int rightStart = x;

    const int sw = styleSeg.getIdealWidth();
    int sx = (getWidth() - sw) / 2;
    sx = juce::jmax (sx, leftEnd + 14);
    sx = juce::jmin (sx, rightStart - 14 - sw);
    styleSeg.setBounds (sx, 14, sw, 30);

    // ---- content
    auto content = b.withTrimmedTop (58).reduced (12, 0).withTrimmedBottom (14);
    columnBounds = content.removeFromRight (colW);
    content.removeFromRight (12);
    displayBounds = content;
    display.setBounds (displayBounds);

    {
        auto col = columnBounds.reduced (6, 12);
        outputDial.setBounds (col.removeFromBottom (80));
        col.removeFromBottom (6);
        mixDial.setBounds (col.removeFromBottom (80));
        col.removeFromBottom (6);
        inputDial.setBounds (col.removeFromBottom (80));
        col.removeFromBottom (20);
        meters.setBounds (col.reduced (2, 2));
    }

    // ---- card internals
    const int cw = juce::jmin (cardW, displayBounds.getWidth() - 24);
    card.setSize (cw, cardH);
    {
        auto inner = card.getLocalBounds().reduced (18, 12);
        auto row = inner.removeFromTop (94);
        inner.removeFromTop (10);
        auto bar = inner.removeFromTop (26);

        const int gap = 28;
        const int dw = (row.getWidth() - 2 * gap) / 10;
        int dx = row.getX() + (row.getWidth() - (dw * 10 + 2 * gap)) / 2;
        Dial* order[] { &threshold, &ratio, &knee, &range, &attack, &release, &hold, &lookahead, &drive, &link };
        for (int i = 0; i < 10; ++i)
        {
            if (i == 4 || i == 8) dx += gap;
            order[i]->setBounds (dx, row.getY(), dw, row.getHeight());
            dx += dw;
        }

        const int arW = autoRelease.getIdealWidth();
        autoRelease.setBounds (bar.removeFromLeft (arW).withSizeKeepingCentre (arW, 22));
        bar.removeFromLeft (20);
        const float capW = textWidth (captionFont(), "CHARACTER") + 2.0f;
        characterCaption = bar.removeFromLeft ((int) std::ceil (capW)).toFloat();
        bar.removeFromLeft (8);
        characterSeg.setBounds (bar.removeFromLeft (juce::jmax (150, characterSeg.getIdealWidth())));

        const int ow = juce::jmax (96, osSeg.getIdealWidth());
        osSeg.setBounds (bar.removeFromRight (ow));
        bar.removeFromRight (8);
        osCaption = bar.removeFromRight ((int) std::ceil (textWidth (captionFont(), "OS") + 2.0f)).toFloat();
        bar.removeFromRight (18);
        channelSeg.setBounds (bar.removeFromRight (juce::jmax (180, channelSeg.getIdealWidth())));
        bar.removeFromRight (18);
        const int sbw = scButton.getIdealWidth (26);
        scButton.setBounds (bar.removeFromRight (sbw));
    }

    // ---- sidechain card internals
    scCard.setSize (scW, scH);
    {
        auto inner = scCard.getLocalBounds().reduced (16, 12);
        scHpf.setBounds (inner.removeFromLeft (76));
        scLpf.setBounds (inner.removeFromLeft (76));
        inner.removeFromLeft (20);
        inner.removeFromTop (20);
        const int ew = scExternal.getIdealWidth();
        scExternal.setBounds (inner.removeFromTop (24).withWidth (ew));
        inner.removeFromTop (10);
        scListen.setBounds (inner.removeFromTop (26).withWidth (scListen.getIdealWidth (26)));
    }

    layoutFloating();
}

void AvelDynamicEditor::layoutFloating()
{
    const auto d = displayBounds;
    const int cardBottom = d.getBottom() - (int) Display::stripH - 2;
    card.setTopLeftPosition (d.getCentreX() - card.getWidth() / 2, cardBottom - card.getHeight());

    const int scY = card.getY() - 10 - scH + (int) std::round ((1.0f - scAnim) * 14.0f);
    scCard.setTopLeftPosition (card.getRight() - scW, scY);
    scCard.setAlpha (scAnim);
    scCard.setVisible (scAnim > 0.01f);

    display.setReservedBottom ((float) card.getHeight() + 2.0f + scAnim * (float) (scH + 10));
}

//==============================================================================
void AvelDynamicEditor::timerCallback()
{
    display.tick();

    float in[2] = { proc.inPeak[0].take(), proc.inPeak[1].take() };
    float out[2] = { proc.outPeak[0].take(), proc.outPeak[1].take() };
    const int nch = juce::jmax (1, proc.getMainBusNumInputChannels());
    if (nch == 1) { in[1] = in[0]; out[1] = out[0]; }
    meters.setLevels (in, out, proc.grPeak.take(), nch);

    undoBtn.setEnabled (proc.undo.canUndo());
    redoBtn.setEnabled (proc.undo.canRedo());
    presetPill.setPresetName (proc.getPresetName());
    if (abSeg.getSelected() != proc.getABIndex()) abSeg.setSelected (proc.getABIndex(), juce::dontSendNotification);

    auto& ap = proc.apvts;
    const bool stereo = nch > 1;
    const int mode = (int) ap.getRawParameterValue (pid::channelMode)->load();
    drive.setEnabled (ap.getRawParameterValue (pid::character)->load() > 0.5f);
    channelSeg.setEnabled (stereo);
    link.setEnabled (stereo && mode == 0);
    const bool listen = ap.getRawParameterValue (pid::scListen)->load() > 0.5f;
    if (scListen.getToggleState() != listen) scListen.setToggleState (listen, juce::dontSendNotification);
    const bool ext = ap.getRawParameterValue (pid::scExternal)->load() > 0.5f;
    juce::ignoreUnused (ext);

    if (proc.presetSerial.load() != lastSerial)
    {
        lastSerial = proc.presetSerial.load();
        repaint();
    }

    if (approach (scAnim, scOpen ? 1.0f : 0.0f, 0.3f))
        layoutFloating();
    if (scCard.isVisible()) scCard.repaint();

    // latency hint can change with lookahead / oversampling
    if (proc.getLatencySamples() != lastLatency) { lastLatency = proc.getLatencySamples(); repaint (columnBounds.withTrimmedTop (columnBounds.getHeight() - 4).expanded (0, 14)); }
}
