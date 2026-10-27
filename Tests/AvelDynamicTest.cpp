// Offline test harness for Avel Dynamic: DSP checks + editor screenshots.
// Usage: AvelDynamicTest [outputDir]
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iostream>

namespace pid = avel::params::id;

static int failures = 0;
static void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  ok    " : "  FAIL  ") << what << std::endl;
    if (! ok) ++failures;
}

static void setParam (AvelDynamicProcessor& p, const char* id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

static std::unique_ptr<AvelDynamicProcessor> makeProc (double sr = 48000.0, int block = 512, int channels = 2)
{
    auto p = std::make_unique<AvelDynamicProcessor>();
    if (channels == 1)
    {
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add (juce::AudioChannelSet::mono());
        l.inputBuses.add (juce::AudioChannelSet::disabled());
        l.outputBuses.add (juce::AudioChannelSet::mono());
        p->setBusesLayout (l);
    }
    p->setRateAndBufferSizeDetails (sr, block);
    p->prepareToPlay (sr, block);
    return p;
}

/** Runs a mono-source signal through the plug-in (both channels), returns output channel 0. */
static std::vector<float> run (AvelDynamicProcessor& p, const std::vector<float>& in, int block = 512, const std::vector<float>* side = nullptr)
{
    const int total = p.getTotalNumInputChannels();
    juce::AudioBuffer<float> buf (juce::jmax (total, p.getTotalNumOutputChannels()), block);
    juce::MidiBuffer midi;
    std::vector<float> out (in.size());
    const int mainCh = p.getMainBusNumInputChannels();
    for (size_t pos = 0; pos < in.size(); pos += (size_t) block)
    {
        const int n = (int) juce::jmin ((size_t) block, in.size() - pos);
        buf.setSize (buf.getNumChannels(), n, false, false, true);
        buf.clear();
        for (int c = 0; c < mainCh; ++c)
            for (int i = 0; i < n; ++i) buf.setSample (c, i, in[pos + (size_t) i]);
        if (side != nullptr)
            for (int c = mainCh; c < total; ++c)
                for (int i = 0; i < n; ++i) buf.setSample (c, i, (*side)[pos + (size_t) i]);
        p.processBlock (buf, midi);
        for (int i = 0; i < n; ++i) out[pos + (size_t) i] = buf.getSample (0, i);
    }
    return out;
}

static std::vector<float> sine (double sr, double f, float amp, int n)
{
    std::vector<float> v ((size_t) n);
    for (int i = 0; i < n; ++i) v[(size_t) i] = amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * f * i / sr);
    return v;
}

static float peakDb (const std::vector<float>& v, size_t from, size_t to)
{
    float m = 0;
    for (size_t i = from; i < to && i < v.size(); ++i) m = juce::jmax (m, std::abs (v[i]));
    return juce::Decibels::gainToDecibels (m, -200.0f);
}

/** Drum-ish program material: kick, snare, hats and a bass line. */
static std::vector<float> music (double sr, int n, juce::Random& rnd)
{
    std::vector<float> v ((size_t) n);
    const int beat = (int) (sr * 0.25);
    float noiseLp = 0;
    for (int i = 0; i < n; ++i)
    {
        const int b = i / beat, t = i % beat;
        const float tt = (float) t / (float) sr;
        float x = 0;
        if (b % 4 == 0 || b % 8 == 7) x += 0.85f * std::exp (-tt * 9.0f) * std::sin (2.0f * 3.14159f * (48.0f + 90.0f * std::exp (-tt * 30.0f)) * tt);
        if (b % 4 == 2) { const float nz = rnd.nextFloat() * 2 - 1; x += 0.55f * std::exp (-tt * 18.0f) * (nz * 0.7f + 0.3f * std::sin (2.0f * 3.14159f * 190.0f * tt)); }
        { const float nz = rnd.nextFloat() * 2 - 1; noiseLp = 0.6f * noiseLp + 0.4f * nz; const int h = i % (beat / 2); x += 0.12f * std::exp (-(float) h / (float) sr * 60.0f) * (nz - noiseLp); }
        x += 0.18f * std::sin (2.0f * 3.14159f * 55.0f * (float) i / (float) sr) * (0.6f + 0.4f * std::sin ((float) i / (float) sr * 1.3f));
        v[(size_t) i] = x;
    }
    return v;
}

static void dspTests()
{
    std::cout << "DSP" << std::endl;
    const double sr = 48000.0;

    {   // 1. ratio 1 => bit-transparent (no oversampling, no lookahead)
        auto p = makeProc (sr);
        setParam (*p, pid::ratio, 1.0f);
        auto in = sine (sr, 440, 0.5f, 48000);
        auto out = run (*p, in);
        float err = 0; for (size_t i = 1000; i < in.size(); ++i) err = juce::jmax (err, std::abs (out[i] - in[i]));
        check (err < 1.0e-5f, "ratio 1:1 is transparent (max error " + juce::String (err, 8) + ")");
        check (p->getLatencySamples() == 0, "no latency by default");
    }

    {   // 2. static curve: -6 dBFS sine, T -20, R 4, hard knee => about -16.5 dBFS
        for (int style = 0; style < 8; ++style)
        {
            auto p = makeProc (sr);
            setParam (*p, pid::style, (float) style);
            setParam (*p, pid::threshold, -20.0f); setParam (*p, pid::ratio, 4.0f); setParam (*p, pid::knee, 0.0f);
            setParam (*p, pid::attack, 5.0f); setParam (*p, pid::release, 200.0f);
            auto out = run (*p, sine (sr, 1000, juce::Decibels::decibelsToGain (-6.0f), 96000));
            const float lvl = peakDb (out, 72000, 96000);
            check (std::abs (lvl - (-16.5f)) < 1.6f, "style " + avel::params::styleNames()[style] + ": -6 dB sine -> " + juce::String (lvl, 2) + " dB (ideal -16.5)");
        }
    }

    {   // 3. lookahead + 4x oversampling: dry path stays aligned; reported latency is honest
        auto p = makeProc (sr);
        setParam (*p, pid::lookahead, 5.0f); setParam (*p, pid::oversampling, 2.0f); setParam (*p, pid::mix, 0.0f);
        std::vector<float> in (48000, 0.0f);
        in[10000] = 1.0f;
        auto out = run (*p, in);
        const int lat = p->getLatencySamples();
        int peakAt = 0; for (int i = 0; i < (int) out.size(); ++i) if (std::abs (out[(size_t) i]) > std::abs (out[(size_t) peakAt])) peakAt = i;
        check (peakAt - 10000 == lat, "dry path delayed by reported latency (" + juce::String (lat) + " samples, impulse at +" + juce::String (peakAt - 10000) + ")");

        setParam (*p, pid::mix, 100.0f); setParam (*p, pid::ratio, 1.0f);
        auto out2 = run (*p, in);
        int peak2 = 0; for (int i = 0; i < (int) out2.size(); ++i) if (std::abs (out2[(size_t) i]) > std::abs (out2[(size_t) peak2])) peak2 = i;
        check (std::abs ((peak2 - 10000) - lat) <= 1, "wet path aligned with latency (impulse at +" + juce::String (peak2 - 10000) + ")");

        // 50 % mix of identical dry/wet must not comb-filter: sine level stays put
        setParam (*p, pid::mix, 50.0f);
        auto s = sine (sr, 7000, 0.5f, 48000);
        auto o = run (*p, s);
        check (std::abs (peakDb (o, 24000, 48000) - juce::Decibels::gainToDecibels (0.5f)) < 0.3f, "50 % mix at 7 kHz keeps level (no comb filtering)");
    }

    {   // 4. auto gain restores roughly the static loss
        auto p = makeProc (sr);
        setParam (*p, pid::threshold, -24.0f); setParam (*p, pid::ratio, 4.0f); setParam (*p, pid::autoGain, 1.0f);
        auto out = run (*p, sine (sr, 1000, 0.5f, 96000));
        check (std::abs (p->getMakeupDb() - 9.0f) < 0.6f, "auto gain makeup " + juce::String (p->getMakeupDb(), 2) + " dB (expected ~9)");
        juce::ignoreUnused (out);
    }

    {   // 5. stability: extreme settings, all styles, all modes, no NaN/Inf and bounded output
        juce::Random rnd (7);
        bool finite = true; float worst = -200.0f;
        for (int style = 0; style < 8; ++style)
            for (int os = 0; os < 3; ++os)
            {
                auto p = makeProc (sr, 256);
                setParam (*p, pid::style, (float) style); setParam (*p, pid::oversampling, (float) os);
                setParam (*p, pid::threshold, -60.0f); setParam (*p, pid::ratio, 100.0f); setParam (*p, pid::knee, 0.0f);
                setParam (*p, pid::attack, 0.005f); setParam (*p, pid::release, 5.0f); setParam (*p, pid::character, 1.0f + (float) (style % 2));
                setParam (*p, pid::drive, 100.0f); setParam (*p, pid::channelMode, (float) (style % 4)); setParam (*p, pid::autoGain, 1.0f);
                setParam (*p, pid::input, 24.0f); setParam (*p, pid::hold, 30.0f); setParam (*p, pid::lookahead, (float) (style * 2));
                auto out = run (*p, music (sr, 24000, rnd), 256);
                for (auto x : out) if (! std::isfinite (x)) finite = false;
                const float pk = peakDb (out, 0, out.size());
                if (pk > 30.0f) std::cout << "  info  style " << style << " os " << os << " peak " << pk << " makeup " << p->getMakeupDb() << std::endl;
                worst = juce::jmax (worst, pk);
            }
        check (finite, "no NaN/Inf with extreme settings (8 styles x 3 oversampling)");
        check (worst < 30.0f, "output stays bounded with extreme settings (peak " + juce::String (worst, 1) + " dBFS)");
    }

    {   // 6. external sidechain: silent main key => no reduction; loud key => ducking
        auto p = std::make_unique<AvelDynamicProcessor>();
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add (juce::AudioChannelSet::stereo());
        l.inputBuses.add (juce::AudioChannelSet::stereo());
        l.outputBuses.add (juce::AudioChannelSet::stereo());
        check (p->setBusesLayout (l), "stereo + stereo sidechain layout accepted");
        p->prepareToPlay (sr, 512);
        setParam (*p, pid::scExternal, 1.0f); setParam (*p, pid::threshold, -30.0f); setParam (*p, pid::ratio, 10.0f);
        auto mainSig = sine (sr, 300, 0.1f, 48000);
        std::vector<float> key (48000, 0.0f);
        auto quiet = run (*p, mainSig, 512, &key);
        check (std::abs (peakDb (quiet, 24000, 48000) - (-20.0f)) < 0.2f, "external key silent -> no reduction");
        auto loudKey = sine (sr, 60, 0.9f, 48000);
        auto ducked = run (*p, mainSig, 512, &loudKey);
        check (peakDb (ducked, 24000, 48000) < -40.0f, "loud external key ducks the main signal (" + juce::String (peakDb (ducked, 24000, 48000), 1) + " dBFS)");
    }

    {   // 7. M/S: Side-only mode leaves a mono signal untouched
        auto p = makeProc (sr);
        setParam (*p, pid::channelMode, 3.0f); setParam (*p, pid::threshold, -40.0f); setParam (*p, pid::ratio, 20.0f);
        auto in = sine (sr, 500, 0.5f, 48000);
        auto out = run (*p, in);
        float err = 0; for (size_t i = 1000; i < in.size(); ++i) err = juce::jmax (err, std::abs (out[i] - in[i]));
        check (err < 1.0e-4f, "Side-only mode leaves mono material alone");
    }

    {   // 8. mono layout
        auto p = makeProc (sr, 512, 1);
        check (p->getMainBusNumInputChannels() == 1, "mono layout accepted");
        setParam (*p, pid::threshold, -20.0f);
        auto out = run (*p, sine (sr, 1000, 0.5f, 48000));
        check (peakDb (out, 24000, 48000) < -9.0f, "mono compresses");
    }

    {   // 9. state round trip incl. A/B and UI size
        auto p = makeProc (sr);
        setParam (*p, pid::threshold, -31.0f); setParam (*p, pid::style, 4.0f);
        p->selectAB (1);
        setParam (*p, pid::threshold, -12.0f);
        p->uiState.setProperty ("width", 1300, nullptr);
        juce::MemoryBlock mb; p->getStateInformation (mb);
        auto q = makeProc (sr);
        q->setStateInformation (mb.getData(), (int) mb.getSize());
        const float t = q->apvts.getRawParameterValue (pid::threshold)->load();
        check (std::abs (t + 12.0f) < 0.01f && q->getABIndex() == 1, "state restores parameters and A/B slot");
        q->selectAB (0);
        check (std::abs (q->apvts.getRawParameterValue (pid::threshold)->load() + 31.0f) < 0.01f, "A slot restored after reload");
        check ((int) q->uiState.getProperty ("width") == 1300, "UI size restored");
    }

    {   // 10. presets load, undo restores
        auto p = makeProc (sr);
        for (int i = 0; i < p->getNumPrograms(); ++i) p->setCurrentProgram (i);
        p->setCurrentProgram (4);
        check (p->getPresetName() == "Vocal Rider", "factory preset loads (" + p->getPresetName() + ")");
        p->undo.undo();
        check (std::abs (p->apvts.getRawParameterValue (pid::threshold)->load() - (-18.0f)) > 0.01f, "undo after preset changes the state back");
    }

    {   // 11. block size larger than prepared + odd sizes
        auto p = makeProc (sr, 256);
        setParam (*p, pid::oversampling, 1.0f);
        auto out = run (*p, sine (sr, 1000, 0.5f, 20000), 1031);
        bool fin = true; for (auto x : out) fin &= std::isfinite (x);
        check (fin, "blocks larger than prepared size are handled");
    }

    {   // 12. CPU: 10 s of stereo at 4x
        auto p = makeProc (sr, 512);
        setParam (*p, pid::oversampling, 2.0f); setParam (*p, pid::style, 4.0f); setParam (*p, pid::character, 1.0f);
        juce::Random rnd (3);
        auto sig = music (sr, 480000, rnd);
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        run (*p, sig);
        const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
        std::cout << "  info  10 s stereo, Mastering + Warm, 4x oversampling: " << ms << " ms (" << (ms / 100.0) << " % of one core)" << std::endl;
    }
}

static void snapshot (AvelDynamicProcessor& p, juce::AudioProcessorEditor& ed, const juce::File& file, int frames, juce::Random& rnd)
{
    juce::AudioBuffer<float> buf (2, 816);
    juce::MidiBuffer midi;
    static int pos = 0;
    for (int f = 0; f < frames; ++f)
    {
        auto sig = music (48000.0, 816, rnd);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 816; ++i)
            {
                const int k = pos + i;
                const int beat = 12000, b = k / beat, t = k % beat;
                const float tt = (float) t / 48000.0f;
                float x = 0;
                if (b % 4 == 0 || b % 8 == 7) x += 0.85f * std::exp (-tt * 9.0f) * std::sin (2.0f * 3.14159f * (48.0f + 90.0f * std::exp (-tt * 30.0f)) * tt);
                if (b % 4 == 2) x += 0.5f * std::exp (-tt * 16.0f) * (rnd.nextFloat() * 2 - 1);
                x += 0.15f * std::sin (2.0f * 3.14159f * 55.0f * (float) k / 48000.0f);
                x += 0.05f * sig[(size_t) i];
                buf.setSample (c, i, x * (c == 0 ? 1.0f : 0.92f));
            }
        pos += 816;
        p.processBlock (buf, midi);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (17);
    }
    auto img = ed.createComponentSnapshot (ed.getLocalBounds(), true, 1.0f);
    file.deleteFile();
    juce::FileOutputStream os (file);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::cout << "  wrote " << file.getFullPathName() << std::endl;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    dspTests();

    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getChildFile ("shots").getFullPathName());
    outDir.createDirectory();
    std::cout << "Screenshots" << std::endl;
    juce::Random rnd (11);

    {
        auto p = makeProc (48000.0, 816);
        p->setCurrentProgram (8); // Drum Bus Glue
        std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditor());
        snapshot (*p, *ed, outDir.getChildFile ("01_default.png"), 300, rnd);

        setParam (*p, pid::scHpf, 120.0f);
        setParam (*p, pid::lookahead, 2.0f);
        p->uiState.setProperty ("scOpen", true, nullptr);
        ed.reset();
        ed.reset (p->createEditor());
        snapshot (*p, *ed, outDir.getChildFile ("02_sidechain_open.png"), 120, rnd);

        ed->setSize (1080, 640);
        snapshot (*p, *ed, outDir.getChildFile ("03_min_size.png"), 60, rnd);

        ed->setSize (1600, 960);
        p->setCurrentProgram (17); // Mix Bus Glue
        setParam (*p, pid::character, 1.0f);
        snapshot (*p, *ed, outDir.getChildFile ("04_large.png"), 120, rnd);
    }
    {
        auto p = makeProc (48000.0, 816, 1);
        p->setCurrentProgram (10);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditor());
        snapshot (*p, *ed, outDir.getChildFile ("05_mono_punch.png"), 200, rnd);
    }

    {   // popup menu + tooltip in the Avel look
        avel::ui::LookAndFeel lnf;
        juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
        juce::PopupMenu sub; sub.addItem (1, "Drum Bus Glue", true, true); sub.addItem (2, "Kick Punch"); sub.addItem (3, "Snare Crack");
        juce::PopupMenu m;
        m.addSubMenu ("General", juce::PopupMenu()); m.addSubMenu ("Vocals", juce::PopupMenu()); m.addSubMenu ("Drums", sub, true, nullptr, true);
        m.addSeparator(); m.addSectionHeader ("User"); m.addItem (10, "My Vocal Chain"); m.addSeparator();
        m.addItem (11, "Save Preset As..."); m.addItem (12, "Open Presets Folder"); m.addItem (13, "Reset to Default");
        {
            // draw the menu with the same LookAndFeel calls JUCE uses for a real popup
            juce::ignoreUnused (m);
            struct Row { juce::String text; int kind; bool ticked, highlighted, sub; }; // kind 0 item, 1 separator, 2 header
            const Row rows[] { { "General", 0, false, false, true }, { "Vocals", 0, false, false, true }, { "Drums", 0, true, true, true },
                               { "", 1, false, false, false }, { "User", 2, false, false, false }, { "My Vocal Chain", 0, false, false, false },
                               { "", 1, false, false, false }, { "Save Preset As...", 0, false, false, false },
                               { "Open Presets Folder", 0, false, false, false }, { "Reset to Default", 0, false, false, false } };
            const int w = 220;
            int h = 12;
            for (auto& r : rows) h += r.kind == 1 ? 9 : 26;
            juce::Image out (juce::Image::ARGB, w + 40, h + 40, true);
            juce::Graphics g (out);
            g.fillAll (juce::Colour (0xff111113));
            g.setOrigin ({ 20, 20 });
            juce::PopupMenu::Options opts;
            lnf.drawPopupMenuBackgroundWithOptions (g, w, h, opts);
            int y = 6;
            for (auto& r : rows)
            {
                const int rh = r.kind == 1 ? 9 : 26;
                juce::Rectangle<int> area (6, y, w - 12, rh);
                if (r.kind == 2) lnf.drawPopupMenuSectionHeaderWithOptions (g, area, r.text, opts);
                else
                {
                    juce::PopupMenu::Item it; it.text = r.text; it.isSeparator = r.kind == 1; it.isTicked = r.ticked; it.itemID = 1;
                    juce::PopupMenu dummySub; dummySub.addItem (1, "x");
                    if (r.sub) it.subMenu = std::make_unique<juce::PopupMenu> (dummySub);
                    lnf.drawPopupMenuItemWithOptions (g, area, r.highlighted, it, opts);
                }
                y += rh;
            }
            juce::FileOutputStream os (outDir.getChildFile ("06_menu.png"));
            os.setPosition (0); os.truncate();
            juce::PNGImageFormat().writeImageToStream (out, os);
            std::cout << "  wrote 06_menu.png" << std::endl;
        }
        juce::PopupMenu::dismissAllActiveMenus();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);

        juce::Image tip (juce::Image::ARGB, 360, 80, true);
        {
            juce::Graphics g (tip);
            g.fillAll (juce::Colour (0xff111113));
            const juce::String text = "Threshold: level above which compression starts. You can also drag the line in the display.";
            auto b = lnf.getTooltipBounds (text, { 10, 10 }, { 0, 0, 2000, 2000 });
            g.setOrigin ({ 10, 10 });
            lnf.drawTooltip (g, text, b.getWidth(), b.getHeight());
        }
        juce::FileOutputStream os (outDir.getChildFile ("07_tooltip.png"));
        os.setPosition (0); os.truncate();
        juce::PNGImageFormat().writeImageToStream (tip, os);
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    std::cout << (failures == 0 ? "ALL TESTS PASSED" : juce::String (failures) + " TEST(S) FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
