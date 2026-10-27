#pragma once

#include "Theme.h"
#include "Display.h"
#include "../PluginProcessor.h"
#include "../Presets.h"

namespace avel::ui
{
    /** Vertical meters: input L/R, gain reduction, output L/R. Values are pushed by the editor timer. */
    class Meters : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        Meters() { setTooltip ("Input, gain reduction and output peak meters. Click to reset the peak values."); }

        void setLevels (const float in[2], const float out[2], float gr, int numChannels)
        {
            channels = numChannels;
            for (int c = 0; c < 2; ++c)
            {
                update (inDb[c], inHold[c], inHoldT[c], gainToDbSafe (in[c]));
                update (outDb[c], outHold[c], outHoldT[c], gainToDbSafe (out[c]));
                inMax = juce::jmax (inMax, gainToDbSafe (in[c]));
                outMax = juce::jmax (outMax, gainToDbSafe (out[c]));
            }
            if (gr >= grDb) grDb = gr; else grDb += (gr - grDb) * 0.12f;
            grMax = juce::jmax (grMax, gr);
            repaint();
        }

        void mouseDown (const juce::MouseEvent&) override { inMax = outMax = -120.0f; grMax = 0.0f; repaint(); }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            auto values = r.removeFromTop (16.0f);
            auto captions = r.removeFromBottom (16.0f);
            r.removeFromBottom (6.0f);
            r.removeFromTop (4.0f);

            const float barW = 6.0f, gap = 2.0f, grW = 8.0f;
            const float groupW = barW * 2 + gap;
            const float total = groupW * 2 + grW;
            const float spacing = (r.getWidth() - total) / 4.0f;
            const float xIn = r.getX() + spacing, xGr = xIn + groupW + spacing, xOut = xGr + grW + spacing;

            auto level = [&] (float x, float w, float db, float hold)
            {
                auto bar = juce::Rectangle<float> (x, r.getY(), w, r.getHeight());
                g.setColour (col::fill.withMultipliedAlpha (0.7f));
                g.fillRoundedRectangle (bar, 2.0f);
                const float n = levelNorm (db);
                if (n > 0.0f)
                {
                    auto lit = bar.withTop (bar.getBottom() - bar.getHeight() * n);
                    // gradient positions are measured from the bottom
                    juce::ColourGradient grad (col::green, 0, bar.getBottom(), col::red, 0, bar.getY(), false);
                    grad.addColour (levelNorm (-12.0f), col::green);
                    grad.addColour (levelNorm (-3.0f), col::yellow);
                    g.setGradientFill (grad);
                    g.fillRoundedRectangle (lit, 2.0f);
                }
                const float hn = levelNorm (hold);
                if (hn > 0.0f)
                {
                    g.setColour ((hold > -0.1f ? col::red : juce::Colours::white).withAlpha (0.85f));
                    g.fillRect (juce::Rectangle<float> (x, bar.getBottom() - bar.getHeight() * hn - 1.0f, w, 1.5f));
                }
            };

            level (xIn, barW, inDb[0], inHold[0]);
            level (xIn + barW + gap, barW, channels > 1 ? inDb[1] : inDb[0], channels > 1 ? inHold[1] : inHold[0]);
            level (xOut, barW, outDb[0], outHold[0]);
            level (xOut + barW + gap, barW, channels > 1 ? outDb[1] : outDb[0], channels > 1 ? outHold[1] : outHold[0]);

            // gain reduction grows downwards from the top (0 .. 30 dB)
            {
                auto bar = juce::Rectangle<float> (xGr, r.getY(), grW, r.getHeight());
                g.setColour (col::fill.withMultipliedAlpha (0.7f));
                g.fillRoundedRectangle (bar, 2.0f);
                const float n = juce::jlimit (0.0f, 1.0f, grDb / 30.0f);
                if (n > 0.002f)
                {
                    g.setGradientFill (juce::ColourGradient (col::orange, 0, bar.getY(), col::red, 0, bar.getY() + bar.getHeight() * 0.5f, false));
                    g.fillRoundedRectangle (bar.withHeight (juce::jmax (2.0f, bar.getHeight() * n)), 2.0f);
                }
            }

            // scale ticks between groups
            g.setFont (font (9.0f, Weight::medium));
            for (int db : { 0, -6, -12, -24, -36, -48 })
            {
                const float y = r.getBottom() - r.getHeight() * levelNorm ((float) db);
                g.setColour (col::label4);
                g.fillRect (juce::Rectangle<float> (xIn - 4.0f, y, 2.0f, 1.0f));
                g.fillRect (juce::Rectangle<float> (xOut + groupW + 2.0f, y, 2.0f, 1.0f));
            }

            auto num = [] (float db, bool gr)
            {
                if (! gr && db <= -99.0f) return juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92\xe2\x88\x9e"));
                const float a = std::abs (db);
                juce::String s = a < 10.0f ? juce::String (a, 1) : juce::String (juce::roundToInt (a));
                if (gr) return s;
                return (db < -0.05f ? juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) : db > 0.05f ? juce::String ("+") : juce::String()) + s;
            };
            g.setFont (font (10.0f, Weight::semibold));
            auto cell = [&] (float cx, const juce::String& s, juce::Colour c) { g.setColour (c); g.drawText (s, juce::Rectangle<float> (cx - 16.0f, values.getY(), 32.0f, values.getHeight()), juce::Justification::centred, false); };
            cell (xIn + groupW * 0.5f, num (inMax, false), inMax > -0.1f ? col::red : col::label2);
            cell (xGr + grW * 0.5f, num (grMax, true), col::label2);
            cell (xOut + groupW * 0.5f, num (outMax, false), outMax > -0.1f ? col::red : col::label2);

            g.setFont (captionFont());
            g.setColour (col::label3);
            auto cap = [&] (float cx, const char* s) { g.drawText (s, juce::Rectangle<float> (cx - 16.0f, captions.getY(), 32.0f, captions.getHeight()), juce::Justification::centred, false); };
            cap (xIn + groupW * 0.5f, "IN");
            cap (xGr + grW * 0.5f, "GR");
            cap (xOut + groupW * 0.5f, "OUT");
        }

    private:
        static float gainToDbSafe (float g) { return g > 1.0e-6f ? 20.0f * std::log10 (g) : -120.0f; }
        static float levelNorm (float db) { return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f); }

        static void update (float& shown, float& hold, int& holdT, float db)
        {
            if (db >= shown) shown = db; else shown = juce::jmax (db, shown - 0.45f); // ~27 dB/s fall at 60 Hz
            if (db >= hold) { hold = db; holdT = 90; }
            else if (--holdT <= 0) hold = juce::jmax (db, hold - 0.6f);
        }

        int channels = 2;
        float inDb[2] { -120, -120 }, outDb[2] { -120, -120 }, inHold[2] { -120, -120 }, outHold[2] { -120, -120 };
        int inHoldT[2] {}, outHoldT[2] {};
        float grDb = 0.0f, inMax = -120.0f, outMax = -120.0f, grMax = 0.0f;
    };

    //==============================================================================
    /** Preset selector pill: [<]  Name  v  [>] */
    class PresetPill : public juce::Component, public juce::SettableTooltipClient, private Animated
    {
    public:
        explicit PresetPill (AvelDynamicProcessor& p) : proc (p)
        {
            setRepaintsOnMouseActivity (true);
            setTooltip ("Presets. Click for the list, arrows step through the factory presets.");
        }

        void setPresetName (const juce::String& s) { if (s != name) { name = s; repaint(); } }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (col::fill.withMultipliedAlpha (0.55f + 0.35f * hover));
            g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            g.setColour (col::stroke);
            g.drawRoundedRectangle (r.reduced (0.5f), r.getHeight() * 0.5f, 1.0f);

            auto left = r.removeFromLeft (r.getHeight()), right = r.removeFromRight (r.getHeight());
            icon::draw (g, icon::chevronLeft(), left.withSizeKeepingCentre (14, 14), zone == 0 ? col::label : col::label3, 1.8f);
            icon::draw (g, icon::chevronRight(), right.withSizeKeepingCentre (14, 14), zone == 2 ? col::label : col::label3, 1.8f);

            const auto f = font (12.5f, Weight::semibold);
            auto text = name;
            const float maxW = r.getWidth() - 20.0f;
            while (text.length() > 3 && textWidth (f, text) > maxW) text = text.dropLastCharacters (2) + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xa6"));
            const float tw = textWidth (f, text);
            auto tr = r.withSizeKeepingCentre (tw + 16.0f, r.getHeight());
            g.setColour (zone == 1 ? col::label : col::label.withAlpha (0.88f));
            g.setFont (f);
            g.drawText (text, tr.withTrimmedRight (16.0f), juce::Justification::centredLeft, false);
            icon::draw (g, icon::chevronDown(), tr.removeFromRight (14.0f).withSizeKeepingCentre (12, 12), col::label3, 1.8f);
        }

        void mouseMove (const juce::MouseEvent& e) override { setZone (zoneAt (e.position.x)); }
        void mouseEnter (const juce::MouseEvent&) override { startAnimating(); }
        void mouseExit (const juce::MouseEvent&) override { setZone (-1); startAnimating(); }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const int z = zoneAt (e.position.x);
            const int count = (int) presets::factory().size();
            const int cur = proc.getFactoryIndex();
            if (z == 0) proc.loadFactoryPreset (cur < 0 ? 0 : (cur + count - 1) % count);
            else if (z == 2) proc.loadFactoryPreset (cur < 0 ? 0 : (cur + 1) % count);
            else showMenu();
        }

        std::function<void()> onPresetLoaded;

    private:
        int zoneAt (float x) const { const float h = (float) getHeight(); return x < h ? 0 : x > (float) getWidth() - h ? 2 : 1; }
        void setZone (int z) { if (z != zone) { zone = z; repaint(); } }

        bool animate() override { const bool m = approach (hover, isMouseOver() ? 1.0f : 0.0f, 0.3f); repaint(); return m; }

        void showMenu()
        {
            juce::PopupMenu m;
            const auto& f = presets::factory();
            juce::StringArray cats;
            for (auto& p : f) cats.addIfNotAlreadyThere (p.category);
            for (auto& c : cats)
            {
                juce::PopupMenu sub;
                bool anyTicked = false;
                for (int i = 0; i < (int) f.size(); ++i)
                    if (c == f[(size_t) i].category)
                    {
                        const bool t = proc.getFactoryIndex() == i;
                        anyTicked |= t;
                        sub.addItem (1 + i, f[(size_t) i].name, true, t);
                    }
                m.addSubMenu (c, sub, true, nullptr, anyTicked);
            }

            userFiles = AvelDynamicProcessor::userPresetFolder().findChildFiles (juce::File::findFiles, false, "*.avelpreset");
            userFiles.sort();
            m.addSeparator();
            if (userFiles.isEmpty())
                m.addItem (900, "No user presets yet", false);
            else
            {
                m.addSectionHeader ("User");
                for (int i = 0; i < userFiles.size(); ++i)
                    m.addItem (1000 + i, userFiles[i].getFileNameWithoutExtension(), true,
                               proc.getFactoryIndex() < 0 && proc.getPresetName() == userFiles[i].getFileNameWithoutExtension());
            }
            m.addSeparator();
            m.addItem (800, "Save Preset As...");
            m.addItem (801, "Open Presets Folder");
            m.addItem (802, "Reset to Default");

            juce::Component::SafePointer<PresetPill> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()),
                             [safe] (int result) { if (safe != nullptr) safe->menuResult (result); });
        }

        void menuResult (int result)
        {
            if (result <= 0) return;
            if (result >= 1 && result <= (int) presets::factory().size()) proc.loadFactoryPreset (result - 1);
            else if (result >= 1000 && result - 1000 < userFiles.size()) proc.loadPresetFile (userFiles[result - 1000]);
            else if (result == 802) proc.loadFactoryPreset (0);
            else if (result == 801)
            {
                auto dir = AvelDynamicProcessor::userPresetFolder();
                dir.createDirectory();
                dir.revealToUser();
            }
            else if (result == 800)
            {
                auto dir = AvelDynamicProcessor::userPresetFolder();
                dir.createDirectory();
                chooser = std::make_unique<juce::FileChooser> ("Save preset", dir.getChildFile (proc.getPresetName() + ".avelpreset"), "*.avelpreset");
                juce::Component::SafePointer<PresetPill> safe (this);
                chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                                      [safe] (const juce::FileChooser& fc)
                                      {
                                          if (safe == nullptr) return;
                                          auto file = fc.getResult();
                                          if (file != juce::File()) safe->proc.savePresetFile (file.withFileExtension ("avelpreset"));
                                          safe->repaint();
                                      });
            }
            if (onPresetLoaded) onPresetLoaded();
        }

        AvelDynamicProcessor& proc;
        juce::String name;
        juce::Array<juce::File> userFiles;
        std::unique_ptr<juce::FileChooser> chooser;
        int zone = -1;
        float hover = 0.0f;
    };
}
