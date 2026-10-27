#pragma once

#include "Theme.h"
#include "../PluginProcessor.h"

namespace avel::ui
{
    /** Product colours for Avel Dynamic (logo gradient + display glow). Everything else is shared Avel. */
    namespace product
    {
        const juce::Colour accentA { 0xff64d2ff };   // cyan
        const juce::Colour accentB { 0xff30d158 };   // green
        const juce::Colour accent  { 0xff4fd8c4 };   // teal, for curve / threshold
    }

    /** Main display: scrolling level history (input, output, gain reduction), threshold line and transfer curve. */
    class Display : public juce::Component, public juce::SettableTooltipClient, private Animated
    {
    public:
        explicit Display (AvelDynamicProcessor& p);

        /** Height at the bottom covered by floating cards; the graph scale ends above it. */
        void setReservedBottom (float px) { if (std::abs (px - reservedBottom) > 0.01f) { reservedBottom = px; repaint(); } }

        /** Pull new points from the processor (called from the editor's 60 Hz timer). */
        void tick();

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

        static constexpr float stripH = 22.0f;   // bottom label strip
        static constexpr float dbTop = 0.0f, dbBottom = -60.0f;

        juce::Rectangle<float> graphArea() const;     // the scaled area (0 .. -60 dB)
        juce::Rectangle<float> transferArea() const;  // inset transfer-curve panel

    private:
        bool animate() override;
        float dbToY (float db, juce::Rectangle<float> g) const;
        float yToDb (float y, juce::Rectangle<float> g) const;
        bool overThresholdLine (juce::Point<float> p) const;
        void drawHistory (juce::Graphics&, juce::Rectangle<float> g);
        void drawTransfer (juce::Graphics&, juce::Rectangle<float> t);
        void drawScale (juce::Graphics&, juce::Rectangle<float> g);

        AvelDynamicProcessor& proc;
        juce::RangedAudioParameter& thresholdParam;
        juce::RangedAudioParameter& ratioParam;
        juce::ParameterAttachment threshold, ratio;
        float thresholdDb = -18.0f, ratioValue = 4.0f;

        static constexpr int historySize = 1000; // 5 s at 200 points/s
        std::vector<HistoryPoint> hist;
        int writePos = 0;
        float detector = -120.0f, detectorShown = -120.0f, grNow = 0.0f, grShown = 0.0f;

        enum class Drag { none, threshold, transfer };
        Drag drag = Drag::none;
        bool hoverLine = false, hoverTransfer = false, wheelGesture = false;
        int wheelCountdown = 0;
        juce::ParameterAttachment* wheelAtt = nullptr;
        float lineHover = 0.0f, transferHover = 0.0f, reservedBottom = 0.0f, dragOffset = 0.0f;
    };
}
