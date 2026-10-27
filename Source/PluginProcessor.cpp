#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

using namespace avel;
namespace pid = avel::params::id;

AvelDynamicProcessor::AvelDynamicProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, &undo, "PARAMS", params::createLayout())
{
    auto p = [this] (const char* id) { auto* v = apvts.getRawParameterValue (id); jassert (v != nullptr); return v; };
    pStyle = p (pid::style);         pThreshold = p (pid::threshold); pRatio = p (pid::ratio);       pKnee = p (pid::knee);
    pRange = p (pid::range);         pAttack = p (pid::attack);       pRelease = p (pid::release);   pAutoRelease = p (pid::autoRelease);
    pHold = p (pid::hold);           pLookahead = p (pid::lookahead); pCharacter = p (pid::character); pDrive = p (pid::drive);
    pLink = p (pid::link);           pChannelMode = p (pid::channelMode); pScHpf = p (pid::scHpf); pScLpf = p (pid::scLpf);
    pScExternal = p (pid::scExternal); pScListen = p (pid::scListen); pOversampling = p (pid::oversampling);
    pInput = p (pid::input);         pOutput = p (pid::output);       pMix = p (pid::mix);           pAutoGain = p (pid::autoGain);

    uiState.setProperty ("width", 1180, nullptr);
    uiState.setProperty ("height", 720, nullptr);
}

bool AvelDynamicProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo()) return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo()) return false;
    }
    return true;
}

void AvelDynamicProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = juce::jmax (16, samplesPerBlock);

    for (int i = 1; i <= 2; ++i)
    {
        using OS = juce::dsp::Oversampling<float>;
        osMain[i] = std::make_unique<OS> (2, (size_t) i, OS::filterHalfBandFIREquiripple, true, true);
        osSc[i]   = std::make_unique<OS> (2, (size_t) i, OS::filterHalfBandFIREquiripple, true, true);
        osMain[i]->initProcessing ((size_t) maxBlock);
        osSc[i]->initProcessing ((size_t) maxBlock);
    }

    const int maxLook = (int) std::ceil (0.02 * sr) + 4;
    lookaheadDelay.prepare (maxLook, 2);
    dryDelay.prepare (maxLook + 1024, 2);
    dryBuf.setSize (2, maxBlock);
    scBuf.setSize (2, maxBlock);
    grBase.assign ((size_t) maxBlock, 0.0f);
    lvlBase.assign ((size_t) maxBlock, -120.0f);

    inGain.reset (sr, 0.02);   inGain.setCurrentAndTargetValue (dsp::dbToGain (pInput->load()));
    outGain.reset (sr, 0.02);  outGain.setCurrentAndTargetValue (dsp::dbToGain (pOutput->load()));
    mixAmt.reset (sr, 0.03);   mixAmt.setCurrentAndTargetValue (pMix->load() * 0.01f);
    makeup.reset (sr, 0.08);   makeup.setCurrentAndTargetValue (1.0f);

    scHp.reset(); scLp.reset(); lastHp = lastLp = -1.0f;
    hop = juce::jmax (1, (int) std::round (sr / 200.0));
    hopCount = 0; acc = {};
    osIndex = -1;
    latency = -1;
    updateLatency ((int) std::round (pLookahead->load() * 0.001 * sr), (int) pOversampling->load());
}

dsp::Settings AvelDynamicProcessor::currentSettings() const
{
    dsp::Settings s;
    s.style = (dsp::Style) juce::jlimit (0, 7, (int) pStyle->load());
    s.thresholdDb = pThreshold->load();
    s.ratio = pRatio->load();
    s.kneeDb = pKnee->load();
    s.rangeDb = pRange->load();
    s.attackMs = pAttack->load();
    s.releaseMs = pRelease->load();
    s.holdMs = pHold->load();
    s.autoRelease = pAutoRelease->load() > 0.5f;
    s.link = pLink->load() * 0.01f;
    return s;
}

float AvelDynamicProcessor::effectiveKnee() const
{
    const auto s = currentSettings();
    return s.kneeDb + dsp::styleSpec (s.style).kneeAdd;
}

void AvelDynamicProcessor::updateLatency (int lookaheadSamples, int osIdx)
{
    int total = lookaheadSamples;
    if (osIdx > 0 && osMain[osIdx] != nullptr)
        total += (int) std::round (osMain[osIdx]->getLatencyInSamples());
    if (total != latency)
    {
        latency = total;
        setLatencySamples (total);
    }
}

void AvelDynamicProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // keep the timing identical to the processed signal so bypass does not shift audio
    const int numCh = juce::jmin (2, getMainBusNumInputChannels(), buffer.getNumChannels());
    if (latency > 0)
        dryDelay.process (buffer.getArrayOfWritePointers(), numCh, buffer.getNumSamples(), latency);
    for (int c = numCh; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, buffer.getNumSamples());
}

void AvelDynamicProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto mainBuf = getBusBuffer (buffer, true, 0);
    const int numCh = juce::jmin (2, mainBuf.getNumChannels());
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());
    if (numCh == 0) return;

    juce::AudioBuffer<float> ext;
    int extCh = 0;
    if (getBusCount (true) > 1 && getBus (true, 1) != nullptr && getBus (true, 1)->isEnabled())
    {
        ext = getBusBuffer (buffer, true, 1);
        extCh = juce::jmin (2, ext.getNumChannels());
    }
    sidechainConnected.store (extCh > 0, std::memory_order_relaxed);

    // ---- block-rate settings
    auto settings = currentSettings();
    const int mode = numCh == 2 ? (int) pChannelMode->load() : 0;
    if (mode == 1) settings.link = 0.0f; // M/S: mid and side are compressed independently

    const int osIdx = juce::jlimit (0, 2, (int) pOversampling->load());
    const int factor = 1 << osIdx;
    if (osIdx != osIndex)
    {
        osIndex = osIdx;
        engine.prepare (sr * factor);
        saturator.prepare (sr * factor);
        const float mk = makeup.getTargetValue();
        makeup.reset (sr * factor, 0.08);
        makeup.setCurrentAndTargetValue (mk);
        for (int i = 1; i <= 2; ++i) { if (osMain[i]) osMain[i]->reset(); if (osSc[i]) osSc[i]->reset(); }
    }
    engine.update (settings);
    saturator.set ((dsp::Character) juce::jlimit (0, 2, (int) pCharacter->load()), pDrive->load() * 0.01f);

    const int look = (int) std::round (pLookahead->load() * 0.001 * sr);
    updateLatency (look, osIdx);

    const float hpF = pScHpf->load(), lpF = pScLpf->load();
    if (! juce::exactlyEqual (hpF, lastHp)) { lastHp = hpF; if (hpF > 20.5f) scHp.setHighPass (sr, hpF); else scHp.setBypass(); }
    if (! juce::exactlyEqual (lpF, lastLp)) { lastLp = lpF; if (lpF < 19950.0f) scLp.setLowPass (sr, lpF); else scLp.setBypass(); }
    const bool useHp = hpF > 20.5f, useLp = lpF < 19950.0f;

    const bool external = pScExternal->load() > 0.5f && extCh > 0;
    const bool listen = pScListen->load() > 0.5f;
    const bool autoGain = pAutoGain->load() > 0.5f;

    // auto gain: a signal at -12 dBFS keeps its level (static reduction at that point is given back)
    {
        const float red = dsp::Curve::limitRange (dsp::Curve::reduction (-12.0f, settings.thresholdDb, settings.ratio, engine.getKnee()), settings.rangeDb);
        const float mk = autoGain ? juce::jmin (18.0f, red) : 0.0f;
        makeupDb.store (mk);
        makeup.setTargetValue (dsp::dbToGain (mk));
    }
    inGain.setTargetValue (dsp::dbToGain (pInput->load()));
    outGain.setTargetValue (dsp::dbToGain (pOutput->load()));
    mixAmt.setTargetValue (juce::jlimit (0.0f, 1.0f, pMix->load() * 0.01f));

    const int total = buffer.getNumSamples();
    for (int offset = 0; offset < total; offset += maxBlock)
    {
        const int n = juce::jmin (maxBlock, total - offset);
        float* io[2] = { mainBuf.getWritePointer (0, offset), numCh > 1 ? mainBuf.getWritePointer (1, offset) : nullptr };

        // input gain (same per-sample gain on every channel)
        for (int i = 0; i < n; ++i)
        {
            const float g = inGain.getNextValue();
            for (int c = 0; c < numCh; ++c) io[c][i] *= g;
        }

        // sidechain source + filters
        for (int c = 0; c < numCh; ++c)
        {
            auto* s = scBuf.getWritePointer (c);
            const float* src = external ? ext.getReadPointer (juce::jmin (c, extCh - 1), offset) : io[c];
            for (int i = 0; i < n; ++i)
            {
                float x = src[i];
                if (useHp) x = scHp.process (x, c);
                if (useLp) x = scLp.process (x, c);
                s[i] = x;
            }
        }

        // dry path, delayed to line up with the wet path
        for (int c = 0; c < numCh; ++c) dryBuf.copyFrom (c, 0, io[c], n);
        dryDelay.process (dryBuf.getArrayOfWritePointers(), numCh, n, latency);

        if (listen)
        {
            for (int c = 0; c < numCh; ++c) juce::FloatVectorOperations::copy (io[c], scBuf.getReadPointer (c), n);
            for (int c = 0; c < numCh; ++c) { outPeak[c].raise (juce::FloatVectorOperations::findMaximum (io[c], n)); }
            continue;
        }

        // lookahead: delay the audio, not the detector
        if (look > 0) lookaheadDelay.process (io, numCh, n, look);

        juce::dsp::AudioBlock<float> mainBlock (io, (size_t) numCh, (size_t) n);
        juce::dsp::AudioBlock<float> scBlock (scBuf.getArrayOfWritePointers(), (size_t) numCh, (size_t) n);
        auto up = osIdx > 0 ? osMain[osIdx]->processSamplesUp (mainBlock) : mainBlock;
        auto upSc = osIdx > 0 ? osSc[osIdx]->processSamplesUp (scBlock) : scBlock;
        const int N = (int) up.getNumSamples();

        float* m[2] = { up.getChannelPointer (0), numCh > 1 ? up.getChannelPointer (1) : nullptr };
        const float* s[2] = { upSc.getChannelPointer (0), numCh > 1 ? upSc.getChannelPointer (1) : nullptr };
        const bool active[4][2] = { { true, true }, { true, true }, { true, false }, { false, true } };
        std::fill (grBase.begin(), grBase.begin() + n, 0.0f);
        std::fill (lvlBase.begin(), lvlBase.begin() + n, -120.0f);

        for (int j = 0; j < N; ++j)
        {
            float a[2], sc[2], r[2] = { 0, 0 };
            for (int c = 0; c < numCh; ++c) { a[c] = m[c][j]; sc[c] = s[c][j]; }
            if (mode > 0)
            {
                const float mA = (a[0] + a[1]) * 0.5f, sA = (a[0] - a[1]) * 0.5f;
                const float mS = (sc[0] + sc[1]) * 0.5f, sS = (sc[0] - sc[1]) * 0.5f;
                a[0] = mA; a[1] = sA; sc[0] = mS; sc[1] = sS;
            }

            engine.process (sc, active[mode], r, numCh);

            float grMax = 0.0f, lvl = -120.0f;
            const float mkGain = makeup.getNextValue(); // only on the parts that are compressed (matters for Mid / Side)
            for (int c = 0; c < numCh; ++c)
            {
                a[c] *= dsp::dbToGain (-r[c]) * (active[mode][c] ? mkGain : 1.0f);
                grMax = juce::jmax (grMax, r[c]);
                if (active[mode][c]) lvl = juce::jmax (lvl, engine.lastLevel (c));
            }
            if (mode > 0)
            {
                const float l = a[0] + a[1], rr = a[0] - a[1];
                a[0] = l; a[1] = rr;
            }
            for (int c = 0; c < numCh; ++c) m[c][j] = saturator.process (a[c], c);

            const int bi = j >> osIdx;
            grBase[(size_t) bi] = juce::jmax (grBase[(size_t) bi], grMax);
            lvlBase[(size_t) bi] = juce::jmax (lvlBase[(size_t) bi], lvl);
        }

        if (osIdx > 0) osMain[osIdx]->processSamplesDown (mainBlock);

        // makeup, mix, output + metering
        float inMax[2] = { 0, 0 }, outMax[2] = { 0, 0 }, grMaxBlock = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float mx = mixAmt.getNextValue(), og = outGain.getNextValue();
            float hopIn = 0.0f, hopOut = 0.0f;
            for (int c = 0; c < numCh; ++c)
            {
                const float d = dryBuf.getSample (c, i);
                const float y = (d * (1.0f - mx) + io[c][i] * mx) * og;
                io[c][i] = y;
                const float ad = std::abs (d), ay = std::abs (y);
                inMax[c] = juce::jmax (inMax[c], ad);
                outMax[c] = juce::jmax (outMax[c], ay);
                hopIn = juce::jmax (hopIn, ad); hopOut = juce::jmax (hopOut, ay);
            }
            grMaxBlock = juce::jmax (grMaxBlock, grBase[(size_t) i]);

            acc.inDb = juce::jmax (acc.inDb, dsp::gainToDb (hopIn));
            acc.outDb = juce::jmax (acc.outDb, dsp::gainToDb (hopOut));
            acc.grDb = juce::jmax (acc.grDb, grBase[(size_t) i]);
            acc.scDb = juce::jmax (acc.scDb, lvlBase[(size_t) i]);
            if (++hopCount >= hop)
            {
                history.push (acc);
                detectorDb.store (acc.scDb, std::memory_order_relaxed);
                acc = {};
                hopCount = 0;
            }
        }
        for (int c = 0; c < numCh; ++c) { inPeak[c].raise (inMax[c]); outPeak[c].raise (outMax[c]); }
        grPeak.raise (grMaxBlock);
    }

    if (numCh == 1 && getMainBusNumOutputChannels() > 1 && buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, buffer.getNumSamples());
}

//==============================================================================
juce::AudioProcessorEditor* AvelDynamicProcessor::createEditor() { return new AvelDynamicEditor (*this); }

int AvelDynamicProcessor::getNumPrograms() { return (int) presets::factory().size(); }

const juce::String AvelDynamicProcessor::getProgramName (int index)
{
    const auto& f = presets::factory();
    return juce::isPositiveAndBelow (index, (int) f.size()) ? juce::String (f[(size_t) index].name) : juce::String();
}

void AvelDynamicProcessor::resetToDefaults()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->getDefaultValue());
            rp->endChangeGesture();
        }
}

void AvelDynamicProcessor::applyPresetValues (const std::vector<std::pair<const char*, float>>& values)
{
    for (auto& [id, v] : values)
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (v));
            p->endChangeGesture();
        }
}

void AvelDynamicProcessor::loadFactoryPreset (int index)
{
    const auto& f = presets::factory();
    if (! juce::isPositiveAndBelow (index, (int) f.size())) return;
    undo.beginNewTransaction ("Preset");
    resetToDefaults();
    applyPresetValues (f[(size_t) index].values);
    presetName = f[(size_t) index].name;
    currentFactory = index;
    ++presetSerial;
}

juce::File AvelDynamicProcessor::userPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Avel").getChildFile ("Avel Dynamic").getChildFile ("Presets");
}

bool AvelDynamicProcessor::savePresetFile (const juce::File& f)
{
    auto state = apvts.copyState();
    juce::XmlElement root ("AvelDynamicPreset");
    root.setAttribute ("name", f.getFileNameWithoutExtension());
    root.setAttribute ("version", AVEL_VERSION);
    if (auto xml = state.createXml()) root.addChildElement (xml.release());
    f.getParentDirectory().createDirectory();
    if (! root.writeTo (f)) return false;
    presetName = f.getFileNameWithoutExtension();
    currentFactory = -1;
    return true;
}

bool AvelDynamicProcessor::loadPresetFile (const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr || ! xml->hasTagName ("AvelDynamicPreset")) return false;
    auto* paramsXml = xml->getChildByName (apvts.state.getType());
    if (paramsXml == nullptr) return false;
    auto tree = juce::ValueTree::fromXml (*paramsXml);

    undo.beginNewTransaction ("Preset");
    resetToDefaults();
    for (const auto& child : tree)
        if (auto* p = apvts.getParameter (child.getProperty ("id").toString()))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 ((float) child.getProperty ("value")));
            p->endChangeGesture();
        }
    presetName = f.getFileNameWithoutExtension();
    currentFactory = -1;
    ++presetSerial;
    return true;
}

//==============================================================================
void AvelDynamicProcessor::selectAB (int index)
{
    if (index == abIndex || ! juce::isPositiveAndBelow (index, 2)) return;
    abSlots[abIndex] = apvts.copyState();
    abIndex = index;
    if (abSlots[index].isValid())
    {
        undo.beginNewTransaction ("A/B");
        for (const auto& child : abSlots[index])
            if (auto* p = apvts.getParameter (child.getProperty ("id").toString()))
            {
                const float v = p->convertTo0to1 ((float) child.getProperty ("value"));
                if (std::abs (p->getValue() - v) > 1.0e-6f)
                {
                    p->beginChangeGesture();
                    p->setValueNotifyingHost (v);
                    p->endChangeGesture();
                }
            }
    }
}

void AvelDynamicProcessor::copyCurrentToOther()
{
    abSlots[1 - abIndex] = apvts.copyState();
}

void AvelDynamicProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", presetName, nullptr);
    state.setProperty ("factory", currentFactory, nullptr);
    state.setProperty ("ab", abIndex, nullptr);
    state.removeChild (state.getChildWithName ("UI"), nullptr);
    state.appendChild (uiState.createCopy(), nullptr);
    for (int i = 0; i < 2; ++i)
        if (abSlots[i].isValid() && i != abIndex)
        {
            auto slot = abSlots[i].createCopy();
            slot.setProperty ("slot", i, nullptr);
            juce::ValueTree holder ("ABSLOT");
            holder.appendChild (slot, nullptr);
            state.appendChild (holder, nullptr);
        }
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void AvelDynamicProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto tree = juce::ValueTree::fromXml (*xml);

    if (auto ui = tree.getChildWithName ("UI"); ui.isValid())
    {
        uiState.copyPropertiesFrom (ui, nullptr);
        tree.removeChild (ui, nullptr);
    }
    abSlots[0] = {}; abSlots[1] = {};
    for (int i = tree.getNumChildren(); --i >= 0;)
    {
        auto c = tree.getChild (i);
        if (c.hasType ("ABSLOT"))
        {
            if (c.getNumChildren() > 0)
            {
                auto slot = c.getChild (0).createCopy();
                const int idx = juce::jlimit (0, 1, (int) slot.getProperty ("slot"));
                slot.removeProperty ("slot", nullptr);
                abSlots[idx] = slot;
            }
            tree.removeChild (i, nullptr);
        }
    }
    presetName = tree.getProperty ("preset", "Default").toString();
    currentFactory = (int) tree.getProperty ("factory", -1);
    abIndex = juce::jlimit (0, 1, (int) tree.getProperty ("ab", 0));
    apvts.replaceState (tree);
    undo.clearUndoHistory();
    ++presetSerial;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AvelDynamicProcessor(); }
