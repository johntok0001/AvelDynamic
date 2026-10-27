#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace avel::params
{
    namespace id
    {
        inline constexpr const char* style       = "style";
        inline constexpr const char* threshold   = "threshold";
        inline constexpr const char* ratio       = "ratio";
        inline constexpr const char* knee        = "knee";
        inline constexpr const char* range       = "range";
        inline constexpr const char* attack      = "attack";
        inline constexpr const char* release     = "release";
        inline constexpr const char* autoRelease = "autoRelease";
        inline constexpr const char* hold        = "hold";
        inline constexpr const char* lookahead   = "lookahead";
        inline constexpr const char* character   = "character";
        inline constexpr const char* drive       = "drive";
        inline constexpr const char* link        = "link";
        inline constexpr const char* channelMode = "channelMode";
        inline constexpr const char* scHpf       = "scHpf";
        inline constexpr const char* scLpf       = "scLpf";
        inline constexpr const char* scExternal  = "scExternal";
        inline constexpr const char* scListen    = "scListen";
        inline constexpr const char* oversampling= "oversampling";
        inline constexpr const char* input       = "input";
        inline constexpr const char* output      = "output";
        inline constexpr const char* mix         = "mix";
        inline constexpr const char* autoGain    = "autoGain";
    }

    inline const juce::StringArray& styleNames()
    {
        static const juce::StringArray s { "Clean", "Classic", "Opto", "Vocal", "Mastering", "Bus", "Punch", "Pumping" };
        return s;
    }

    inline const juce::StringArray& styleTips()
    {
        static const juce::StringArray s {
            "Clean: transparent peak compressor, does exactly what the controls say",
            "Classic: feedback design with a slightly soft knee, like a vintage VCA",
            "Opto: slow, smooth optical-style leveller with program-dependent release",
            "Vocal: wide automatic knee and RMS detection that rides a voice evenly",
            "Mastering: gentle RMS detection, rounded attack and adaptive release",
            "Bus: glue for groups and mix bus, light RMS and soft knee",
            "Punch: rounded attack lets transients through for snappy drums",
            "Pumping: release recovers in the linear domain for audible pumping"
        };
        return s;
    }

    inline juce::String formatMs (float v)
    {
        if (v < 0.1f)   return juce::String (v, 3) + " ms";
        if (v < 10.0f)  return juce::String (v, 2) + " ms";
        if (v < 100.0f) return juce::String (v, 1) + " ms";
        if (v < 1000.0f) return juce::String (juce::roundToInt (v)) + " ms";
        return juce::String (v / 1000.0f, 2) + " s";
    }

    inline float parseMs (const juce::String& t)
    {
        const float v = t.retainCharacters ("-.0123456789").getFloatValue();
        return t.containsIgnoreCase ("ms") || ! t.trim().endsWithIgnoreCase ("s") ? v : v * 1000.0f;
    }

    inline juce::String formatDb (float v, int decimals = 1)
    {
        if (std::abs (v) < 0.05f) v = 0.0f;
        return (v > 0.0f ? "+" : "") + juce::String (v, decimals) + " dB";
    }

    inline juce::String formatRatio (float v)
    {
        if (v >= 99.5f) return juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x9e : 1"));
        return juce::String (v, v < 10.0f ? 1 : 0) + " : 1";
    }

    inline juce::String formatHz (float v)
    {
        if (v >= 1000.0f) return juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz";
        return juce::String (juce::roundToInt (v)) + " Hz";
    }

    inline float parseHz (const juce::String& t)
    {
        const float v = t.retainCharacters (".0123456789").getFloatValue();
        return t.containsIgnoreCase ("k") ? v * 1000.0f : v;
    }

    inline juce::NormalisableRange<float> skewed (float lo, float hi, float centre)
    {
        juce::NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        AudioProcessorValueTreeState::ParameterLayout l;
        auto f = [&] (const char* pid, const char* name, NormalisableRange<float> r, float def,
                      std::function<String (float, int)> toStr, std::function<float (const String&)> fromStr, const char* label = "")
        {
            l.add (std::make_unique<AudioParameterFloat> (ParameterID { pid, 1 }, name, r, def,
                       AudioParameterFloatAttributes().withLabel (label).withStringFromValueFunction (std::move (toStr))
                                                      .withValueFromStringFunction (std::move (fromStr))));
        };
        // On/off switches are two-step choices: their normalised value is always exactly 0 or 1,
        // so a host that writes 0.3 still reads back what the state restores (pluginval state test).
        auto onOff = [&] (const char* pid, const char* name, bool def)
        {
            l.add (std::make_unique<AudioParameterChoice> (ParameterID { pid, 1 }, name, StringArray { "Off", "On" }, def ? 1 : 0));
        };
        auto dbParse = [] (const String& t) { return t.retainCharacters ("-+.0123456789").getFloatValue(); };
        auto pctParse = [] (const String& t) { return t.retainCharacters ("-.0123456789").getFloatValue(); };
        auto pct = [] (float v, int) { return String (roundToInt (v)) + " %"; };

        l.add (std::make_unique<AudioParameterChoice> (ParameterID { id::style, 1 }, "Style", styleNames(), 0));
        f (id::threshold, "Threshold", { -60.0f, 0.0f, 0.01f }, -18.0f, [] (float v, int) { return formatDb (v); }, dbParse);
        {
            NormalisableRange<float> r (1.0f, 100.0f, 0.01f);
            r.setSkewForCentre (4.0f);
            f (id::ratio, "Ratio", r, 4.0f, [] (float v, int) { return formatRatio (v); },
               [] (const String& t) { return t.contains (CharPointer_UTF8 ("\xe2\x88\x9e")) || t.containsIgnoreCase ("inf") ? 100.0f
                                                                                                              : t.upToFirstOccurrenceOf (":", false, false).retainCharacters (".0123456789").getFloatValue(); });
        }
        f (id::knee, "Knee", { 0.0f, 36.0f, 0.01f }, 6.0f, [] (float v, int) { return String (v, 1) + " dB"; }, dbParse);
        f (id::range, "Range", { 0.0f, 60.0f, 0.01f }, 60.0f, [] (float v, int) { return v >= 59.95f ? String ("Full") : String (v, 1) + " dB"; },
           [dbParse] (const String& t) { return t.containsIgnoreCase ("full") ? 60.0f : std::abs (dbParse (t)); });
        f (id::attack, "Attack", skewed (0.005f, 250.0f, 10.0f), 10.0f, [] (float v, int) { return formatMs (v); }, parseMs);
        f (id::release, "Release", skewed (5.0f, 2500.0f, 150.0f), 120.0f, [] (float v, int) { return formatMs (v); }, parseMs);
        onOff (id::autoRelease, "Auto Release", false);
        f (id::hold, "Hold", skewed (0.0f, 500.0f, 50.0f), 0.0f, [] (float v, int) { return v < 0.05f ? String ("Off") : formatMs (v); }, parseMs);
        f (id::lookahead, "Lookahead", { 0.0f, 20.0f, 0.01f }, 0.0f, [] (float v, int) { return v < 0.005f ? String ("Off") : formatMs (v); }, parseMs);
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { id::character, 1 }, "Character", StringArray { "Off", "Warm", "Bright" }, 0));
        f (id::drive, "Drive", { 0.0f, 100.0f, 0.1f }, 30.0f, pct, pctParse);
        f (id::link, "Stereo Link", { 0.0f, 100.0f, 0.1f }, 100.0f, pct, pctParse);
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { id::channelMode, 1 }, "Channels", StringArray { "Stereo", "M/S", "Mid", "Side" }, 0));
        f (id::scHpf, "SC High-pass", skewed (20.0f, 2000.0f, 200.0f), 20.0f, [] (float v, int) { return v <= 20.5f ? String ("Off") : formatHz (v); }, parseHz);
        f (id::scLpf, "SC Low-pass", skewed (1000.0f, 20000.0f, 6000.0f), 20000.0f, [] (float v, int) { return v >= 19950.0f ? String ("Off") : formatHz (v); }, parseHz);
        onOff (id::scExternal, "External Sidechain", false);
        onOff (id::scListen, "Sidechain Listen", false);
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { id::oversampling, 1 }, "Oversampling", StringArray { "Off", "2x", "4x" }, 0));
        f (id::input, "Input", { -24.0f, 24.0f, 0.01f }, 0.0f, [] (float v, int) { return formatDb (v); }, dbParse);
        f (id::output, "Output", { -24.0f, 24.0f, 0.01f }, 0.0f, [] (float v, int) { return formatDb (v); }, dbParse);
        f (id::mix, "Mix", { 0.0f, 100.0f, 0.1f }, 100.0f, pct, pctParse);
        onOff (id::autoGain, "Auto Gain", false);
        return l;
    }
}
