#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP/Compressor.h"
#include "Params.h"

/** One point of the scrolling level display (~200 per second). */
struct HistoryPoint
{
    float inDb = -120.0f;   // input peak (after Input gain)
    float outDb = -120.0f;  // output peak
    float grDb = 0.0f;      // gain reduction, positive dB
    float scDb = -120.0f;   // detector level
};

/** Lock-free single-producer / single-consumer queue for the display. */
class HistoryFifo
{
public:
    void push (const HistoryPoint& p)
    {
        int s1, n1, s2, n2;
        fifo.prepareToWrite (1, s1, n1, s2, n2);
        if (n1 > 0) data[(size_t) s1] = p;
        else if (n2 > 0) data[(size_t) s2] = p;
        fifo.finishedWrite (n1 + n2);
    }

    template <typename Fn> void pop (Fn&& fn)
    {
        const int ready = fifo.getNumReady();
        int s1, n1, s2, n2;
        fifo.prepareToRead (ready, s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) fn (data[(size_t) (s1 + i)]);
        for (int i = 0; i < n2; ++i) fn (data[(size_t) (s2 + i)]);
        fifo.finishedRead (n1 + n2);
    }

private:
    static constexpr int size = 4096;
    juce::AbstractFifo fifo { size };
    std::array<HistoryPoint, size> data {};
};

/** Peak value that the audio thread raises and the UI reads-and-clears. */
struct PeakAtomic
{
    std::atomic<float> v { 0.0f };
    void raise (float x) { auto c = v.load (std::memory_order_relaxed); while (x > c && ! v.compare_exchange_weak (c, x, std::memory_order_relaxed)) {} }
    float take() { return v.exchange (0.0f, std::memory_order_relaxed); }
};

class AvelDynamicProcessor : public juce::AudioProcessor
{
public:
    AvelDynamicProcessor();
    ~AvelDynamicProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Avel Dynamic"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return juce::jmax (0, currentFactory); }
    void setCurrentProgram (int index) override { loadFactoryPreset (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- presets
    void loadFactoryPreset (int index);
    bool loadPresetFile (const juce::File& f);
    bool savePresetFile (const juce::File& f);
    static juce::File userPresetFolder();
    juce::String getPresetName() const { return presetName; }
    int getFactoryIndex() const { return currentFactory; }

    // ---- A/B
    int getABIndex() const { return abIndex; }
    void selectAB (int index);
    void copyCurrentToOther();

    // ---- metering, read by the editor
    HistoryFifo history;
    PeakAtomic inPeak[2], outPeak[2], grPeak;
    std::atomic<float> detectorDb { -120.0f };
    std::atomic<bool> sidechainConnected { false };
    float getMakeupDb() const { return makeupDb.load(); }

    juce::UndoManager undo { 30000, 60 };
    juce::AudioProcessorValueTreeState apvts;
    juce::ValueTree uiState { "UI" };

    /** Settings used for the transfer curve on screen. */
    avel::dsp::Settings currentSettings() const;
    float effectiveKnee() const;

    /** Bumped whenever a preset or a whole state is loaded; the editor polls it. */
    std::atomic<int> presetSerial { 0 };

private:
    void updateLatency (int lookaheadSamples, int osIndex);
    void applyPresetValues (const std::vector<std::pair<const char*, float>>& values);
    void resetToDefaults();

    // parameters
    std::atomic<float>* pStyle, * pThreshold, * pRatio, * pKnee, * pRange, * pAttack, * pRelease, * pAutoRelease, * pHold,
                      * pLookahead, * pCharacter, * pDrive, * pLink, * pChannelMode, * pScHpf, * pScLpf, * pScExternal, * pScListen,
                      * pOversampling, * pInput, * pOutput, * pMix, * pAutoGain;

    double sr = 48000.0;
    int maxBlock = 512, osIndex = -1, latency = -1;
    avel::dsp::Engine engine;
    avel::dsp::Saturator saturator;
    avel::dsp::Biquad scHp, scLp;
    float lastHp = -1, lastLp = -1;
    avel::dsp::Delay lookaheadDelay, dryDelay;
    std::unique_ptr<juce::dsp::Oversampling<float>> osMain[3], osSc[3];
    juce::AudioBuffer<float> dryBuf, scBuf;
    std::vector<float> grBase, lvlBase;
    juce::SmoothedValue<float> inGain, outGain, mixAmt, makeup;
    std::atomic<float> makeupDb { 0.0f };

    // history accumulation
    int hop = 240, hopCount = 0;
    HistoryPoint acc;

    // presets / AB
    juce::String presetName { "Default" };
    int currentFactory = 0;
    int abIndex = 0;
    juce::ValueTree abSlots[2];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AvelDynamicProcessor)
};
