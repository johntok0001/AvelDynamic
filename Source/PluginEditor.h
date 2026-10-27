#pragma once

#include "PluginProcessor.h"
#include "UI/Theme.h"
#include "UI/Display.h"
#include "UI/Widgets.h"

/** Glass card with captions/separators drawn behind its child controls. */
class GlassCard : public juce::Component
{
public:
    std::function<void (juce::Graphics&)> decorate;
    float alpha = 1.0f;

    void paint (juce::Graphics& g) override
    {
        avel::ui::drawGlass (g, getLocalBounds().toFloat().reduced (shadowPad), 18.0f, 1.0f);
        if (decorate) decorate (g);
    }

    static constexpr float shadowPad = 0.0f;
};

class AvelDynamicEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AvelDynamicEditor (AvelDynamicProcessor&);
    ~AvelDynamicEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Pump the UI once (used by the offline snapshot test). */
    void tickForTest() { timerCallback(); }

private:
    void timerCallback() override;
    void layoutFloating();
    void setupDial (avel::ui::Dial& d, const char* paramId, const juce::String& tip, juce::Colour c = avel::ui::col::accent);

    AvelDynamicProcessor& proc;
    avel::ui::LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    // top bar
    avel::ui::PresetPill presetPill { proc };
    avel::ui::IconButton undoBtn { "Undo", avel::ui::icon::undo() }, redoBtn { "Redo", avel::ui::icon::redo() };
    avel::ui::Segmented styleSeg, abSeg;
    avel::ui::IconButton copyBtn { "Copy", avel::ui::icon::copy() };
    avel::ui::Switch autoGain;

    // display + card
    avel::ui::Display display { proc };
    GlassCard card, scCard;
    avel::ui::Dial threshold { "Threshold" }, ratio { "Ratio" }, knee { "Knee" }, range { "Range" },
                   attack { "Attack" }, release { "Release" }, hold { "Hold" }, lookahead { "Lookahead" },
                   drive { "Drive" }, link { "Link" };
    avel::ui::Switch autoRelease;
    avel::ui::Segmented characterSeg, channelSeg, osSeg;
    avel::ui::IconButton scButton { "Sidechain", avel::ui::icon::sidechain() };

    // sidechain card
    avel::ui::Dial scHpf { "High-pass" }, scLpf { "Low-pass" };
    avel::ui::Switch scExternal;
    avel::ui::IconButton scListen { "Listen", avel::ui::icon::headphones() };

    // right column
    avel::ui::Meters meters;
    avel::ui::Dial inputDial { "Input" }, mixDial { "Mix" }, outputDial { "Output" };

    // layout state
    juce::Rectangle<int> displayBounds, columnBounds;
    juce::Rectangle<float> characterCaption, osCaption;
    bool scOpen = false, sizeRestored = false;
    float scAnim = 0.0f;
    int lastSerial = -1, lastLatency = -1;

    static constexpr int cardW = 872, cardH = 150, scW = 372, scH = 118, colW = 90;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AvelDynamicEditor)
};
