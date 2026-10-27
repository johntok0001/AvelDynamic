#pragma once

// Avel Dynamic compressor core. Header-only, no JUCE dependency except juce::jlimit-style helpers kept local.
// Works in the dB domain (log detector), per sample, at whatever rate it is prepared for (oversampled or not).

#include <array>
#include <cmath>
#include <algorithm>
#include <vector>

namespace avel::dsp
{
    constexpr int maxChannels = 2;

    inline float dbToGain (float db) { return std::exp (db * 0.11512925464970229f); }   // ln(10)/20
    inline float gainToDb (float g)  { return 8.685889638065035f * std::log (std::max (g, 1.0e-9f)); } // 20/ln(10)

    enum class Style : int { clean = 0, classic, opto, vocal, mastering, bus, punch, pumping, count };

    /** How each style detects and smooths. */
    struct StyleSpec
    {
        float rmsMs;        // 0 = peak detector
        bool  feedback;     // detector looks at the output (classic analog topology)
        float kneeAdd;      // extra knee in dB on top of the Knee control
        float attackScale;  // multiplies the Attack control
        float releaseScale; // multiplies the Release control
        float program;      // 0..1 amount of program-dependent (dual) release
        bool  twoPole;      // second attack stage: rounder onset, lets transients through
        bool  linearRelease;// release in the linear gain domain: audible "pumping" recovery
    };

    inline const StyleSpec& styleSpec (Style s)
    {
        static const StyleSpec specs[] {
            /* clean     */ { 0.0f,  false, 0.0f,  1.0f, 1.0f, 0.0f,  false, false },
            /* classic   */ { 1.5f,  true,  3.0f,  1.0f, 1.0f, 0.35f, false, false },
            /* opto      */ { 8.0f,  true,  9.0f,  2.0f, 1.5f, 1.0f,  false, false },
            /* vocal     */ { 4.0f,  false, 10.0f, 1.0f, 1.0f, 0.5f,  false, false },
            /* mastering */ { 12.0f, false, 6.0f,  1.2f, 1.2f, 0.7f,  true,  false },
            /* bus       */ { 5.0f,  false, 4.0f,  1.0f, 1.0f, 0.45f, false, false },
            /* punch     */ { 0.0f,  false, 2.0f,  1.6f, 1.0f, 0.2f,  true,  false },
            /* pumping   */ { 0.5f,  false, 0.0f,  0.8f, 1.0f, 0.0f,  false, true  },
        };
        return specs[std::clamp ((int) s, 0, (int) Style::count - 1)];
    }

    struct Settings
    {
        Style style = Style::clean;
        float thresholdDb = -18.0f;
        float ratio = 4.0f;          // >= 1; >= 100 means infinity
        float kneeDb = 6.0f;
        float rangeDb = 60.0f;
        float attackMs = 10.0f;
        float releaseMs = 120.0f;
        float holdMs = 0.0f;
        bool  autoRelease = false;
        float link = 1.0f;           // 0..1
    };

    /** Static curves. Reduction is returned as a positive number of dB. */
    struct Curve
    {
        static float slope (float ratio) { return ratio >= 99.5f ? 1.0f : 1.0f - 1.0f / std::max (1.0f, ratio); }

        /** Feed-forward: input level -> reduction. Standard quadratic soft knee. */
        static float reduction (float xDb, float t, float ratio, float knee)
        {
            const float s = slope (ratio);
            const float over = xDb - t;
            if (knee > 0.01f)
            {
                if (2.0f * over <= -knee) return 0.0f;
                if (2.0f * over < knee)  { const float q = over + knee * 0.5f; return s * q * q / (2.0f * knee); }
            }
            return over > 0.0f ? s * over : 0.0f;
        }

        /** Feedback: output level -> reduction, chosen so the steady state matches reduction(). */
        static float reductionFromOutput (float yDb, float t, float ratio, float knee)
        {
            const float r = std::min (ratio, 50.0f);
            const float k = r - 1.0f; // y = T + (x - T)/R  <=>  x - y = (R - 1)(y - T)
            const float over = yDb - t;
            const float kn = knee / std::max (1.0f, r); // knee width measured at the output
            if (kn > 0.01f)
            {
                if (2.0f * over <= -kn) return 0.0f;
                if (2.0f * over < kn)  { const float q = over + kn * 0.5f; return k * q * q / (2.0f * kn); }
            }
            return over > 0.0f ? k * over : 0.0f;
        }

        /** Output level for an input level (what the transfer display draws), including Range. */
        static float output (float xDb, float t, float ratio, float knee, float range)
        {
            return xDb - limitRange (reduction (xDb, t, ratio, knee), range);
        }

        /** Smooth minimum so Range bends instead of clipping hard. */
        static float limitRange (float r, float range)
        {
            if (range >= 59.9f) return r;
            constexpr float k = 2.0f;
            const float d = r - range;
            const float m = 0.5f * (r + range - std::sqrt (d * d + k * k));
            return std::clamp (m, 0.0f, r);
        }
    };

    /** One branching smoother with optional hold and linear-gain release. */
    struct Smoother
    {
        float value = 0.0f;
        int holdLeft = 0;

        void reset() { value = 0.0f; holdLeft = 0; }

        float process (float target, float aAtt, float aRel, int holdSamples, bool linearRelease)
        {
            if (target >= value)
            {
                value = aAtt * value + (1.0f - aAtt) * target;
                holdLeft = holdSamples;
            }
            else if (holdLeft > 0)
            {
                --holdLeft;
            }
            else if (linearRelease)
            {
                const float g = dbToGain (-value), gt = dbToGain (-target);
                value = -gainToDb (aRel * g + (1.0f - aRel) * gt);
            }
            else
            {
                value = aRel * value + (1.0f - aRel) * target;
            }
            return value;
        }
    };

    /** Multi-channel compressor gain engine. Feed it sidechain frames, get reduction (dB) per channel. */
    class Engine
    {
    public:
        void prepare (double newSampleRate)
        {
            sampleRate = newSampleRate;
            update (settings);
            reset();
        }

        void reset()
        {
            for (auto& c : ch) c = {};
        }

        void update (const Settings& s)
        {
            settings = s;
            spec = styleSpec (s.style);
            const auto fs = (float) sampleRate;
            auto coef = [fs] (float ms) { return ms <= 0.0f ? 0.0f : std::exp (-1.0f / (ms * 0.001f * fs)); };

            const float att = std::max (0.001f, s.attackMs * spec.attackScale);
            const float rel = std::max (1.0f, s.releaseMs * spec.releaseScale);
            aAtt = coef (att);
            aAtt2 = coef (att * 0.6f);
            aRel = coef (rel);
            aRelFast = coef (std::max (12.0f, rel * 0.3f));
            aAttSlow = coef (rel * 1.5f + 40.0f);
            aRelSlow = coef (rel * 3.0f);
            aRms = spec.rmsMs > 0.0f ? coef (spec.rmsMs) : 0.0f;
            holdSamples = (int) std::round (s.holdMs * 0.001f * fs);
            program = s.autoRelease ? 1.0f : spec.program;
            knee = s.kneeDb + spec.kneeAdd;

            if (spec.feedback)
            {
                // A feedback loop with gain (R-1) is stable only if (1 - a) * R < 2: keep the smoother slow enough.
                const float r = std::min (s.ratio, 50.0f);
                const float maxStep = 1.5f / std::max (1.0f, r);
                aAtt = std::max (aAtt, 1.0f - maxStep);
                aRel = std::max (aRel, 1.0f - maxStep);
                aRelFast = std::max (aRelFast, 1.0f - maxStep);
            }
        }

        const Settings& getSettings() const { return settings; }
        float getKnee() const { return knee; }

        /** sc: sidechain samples per channel; active: which channels compress; out: reduction in dB (>= 0). */
        void process (const float* sc, const bool* active, float* out, int numChannels)
        {
            std::array<float, maxChannels> lvl {};
            float maxLvl = -200.0f;

            for (int c = 0; c < numChannels; ++c)
            {
                auto& st = ch[(size_t) c];
                float x = sc[c];
                if (spec.feedback) x *= dbToGain (-st.last);

                float l;
                if (aRms > 0.0f)
                {
                    st.ms = aRms * st.ms + (1.0f - aRms) * x * x;
                    l = 10.0f * std::log10 (std::max (st.ms, 1.0e-12f)) + 3.0103f;
                }
                else
                {
                    l = gainToDb (std::abs (x));
                }
                lvl[(size_t) c] = l;
                if (active[c]) maxLvl = std::max (maxLvl, l);
            }

            for (int c = 0; c < numChannels; ++c)
            {
                auto& st = ch[(size_t) c];
                if (! active[c]) { out[c] = 0.0f; st.last = 0.0f; continue; }

                float l = lvl[(size_t) c];
                if (numChannels > 1) l = settings.link * maxLvl + (1.0f - settings.link) * l;
                st.level = l;

                float target = spec.feedback ? Curve::reductionFromOutput (l, settings.thresholdDb, settings.ratio, knee)
                                             : Curve::reduction (l, settings.thresholdDb, settings.ratio, knee);
                target = Curve::limitRange (target, settings.rangeDb);

                float r = st.main.process (target, aAtt, aRel, holdSamples, spec.linearRelease);
                if (program > 0.0f)
                {
                    const float f = st.fast.process (target, aAtt, aRelFast, holdSamples, false);
                    const float s = st.slow.process (target, aAttSlow, aRelSlow, holdSamples, false);
                    r = (1.0f - program) * r + program * std::max (f, s);
                }
                if (spec.twoPole)
                    st.second = r > st.second ? aAtt2 * st.second + (1.0f - aAtt2) * r : r;
                else
                    st.second = r;

                st.last = st.second;
                out[c] = st.second;
            }
        }

        /** Detector level (dB) of the loudest active channel from the last frame. */
        float lastLevel (int c) const { return ch[(size_t) c].level; }

    private:
        struct Channel
        {
            float ms = 0.0f, level = -200.0f, second = 0.0f, last = 0.0f;
            Smoother main, fast, slow;
        };

        double sampleRate = 48000.0;
        Settings settings;
        StyleSpec spec = styleSpec (Style::clean);
        float aAtt = 0, aAtt2 = 0, aRel = 0, aRelFast = 0, aAttSlow = 0, aRelSlow = 0, aRms = 0, program = 0, knee = 0;
        int holdSamples = 0;
        std::array<Channel, maxChannels> ch {};
    };

    //==============================================================================
    enum class Character : int { off = 0, warm, bright };

    /** Gentle saturation applied after the gain stage. Unity small-signal gain.
        Warm: asymmetric curve (even harmonics), harmonics low-passed. Bright: symmetric (odd), harmonics high-passed. */
    class Saturator
    {
    public:
        void prepare (double fs)
        {
            sampleRate = fs;
            lpCoef = std::exp (-2.0f * 3.14159265f * 6000.0f / (float) fs);
            hpCoef = std::exp (-2.0f * 3.14159265f * 900.0f / (float) fs);
            dcCoef = 1.0f - 2.0f * 3.14159265f * 8.0f / (float) fs;
            reset();
        }

        void reset() { for (auto& s : st) s = {}; }

        void set (Character c, float drive01)
        {
            mode = c;
            k = 1.0f + 8.0f * drive01 * drive01 + 1.0f * drive01;
            bias = 0.12f + 0.25f * drive01;
            tb = std::tanh (bias);
            norm = 1.0f / (k * (1.0f - tb * tb));
        }

        float process (float x, int c)
        {
            if (mode == Character::off) return x;
            auto& s = st[(size_t) c];
            float d;
            if (mode == Character::warm)
            {
                d = (std::tanh (k * x + bias) - tb) * norm - x;
                s.lp = lpCoef * s.lp + (1.0f - lpCoef) * d;           // keep the harmonics warm
                const float y = s.lp - s.dcX + dcCoef * s.dcY;          // DC blocker on the generated part
                s.dcX = s.lp; s.dcY = y;
                d = y;
            }
            else
            {
                d = std::tanh (k * x) / k - x;
                s.lp = hpCoef * s.lp + (1.0f - hpCoef) * d;
                d = (d - s.lp) * 1.4f;                                  // only the upper harmonics
            }
            return x + d;
        }

    private:
        struct State { float lp = 0, dcX = 0, dcY = 0; };
        double sampleRate = 48000.0;
        Character mode = Character::off;
        float k = 1, bias = 0, tb = 0, norm = 1, lpCoef = 0, hpCoef = 0, dcCoef = 0.999f;
        std::array<State, maxChannels> st {};
    };

    //==============================================================================
    /** RBJ biquad, transposed direct form II. */
    class Biquad
    {
    public:
        void setHighPass (double fs, double f, double q = 0.70710678) { design (fs, f, q, true); }
        void setLowPass (double fs, double f, double q = 0.70710678)  { design (fs, f, q, false); }
        void setBypass() { b0 = 1; b1 = b2 = a1 = a2 = 0; }
        void reset() { for (auto& z : s) z = { 0, 0 }; }

        float process (float x, int c)
        {
            auto& z = s[(size_t) c];
            const double y = b0 * x + z[0];
            z[0] = b1 * x - a1 * y + z[1];
            z[1] = b2 * x - a2 * y;
            return (float) y;
        }

    private:
        void design (double fs, double f, double q, bool hp)
        {
            f = std::clamp (f, 5.0, fs * 0.45);
            const double w = 2.0 * 3.141592653589793 * f / fs, cw = std::cos (w), alpha = std::sin (w) / (2.0 * q);
            const double a0 = 1.0 + alpha;
            if (hp) { b0 = (1 + cw) * 0.5; b1 = -(1 + cw); b2 = b0; }
            else    { b0 = (1 - cw) * 0.5; b1 = 1 - cw;    b2 = b0; }
            a1 = -2.0 * cw; a2 = 1.0 - alpha;
            b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
        }

        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        std::array<std::array<double, 2>, maxChannels> s {};
    };

    //==============================================================================
    /** Integer multi-channel delay line. */
    class Delay
    {
    public:
        void prepare (int maxDelay, int channels)
        {
            size = 1;
            while (size < maxDelay + 2) size <<= 1;
            buf.assign ((size_t) channels, std::vector<float> ((size_t) size, 0.0f));
            write = 0;
        }

        void reset() { for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f); }

        /** In-place: replaces data with data delayed by d samples. */
        void process (float* const* data, int numChannels, int numSamples, int d)
        {
            d = std::clamp (d, 0, size - 2);
            const int mask = size - 1;
            for (int c = 0; c < numChannels && c < (int) buf.size(); ++c)
            {
                auto* b = buf[(size_t) c].data();
                int w = write;
                for (int i = 0; i < numSamples; ++i)
                {
                    b[w] = data[c][i];
                    data[c][i] = b[(w - d) & mask];
                    w = (w + 1) & mask;
                }
            }
            write = (write + numSamples) & mask;
        }

    private:
        std::vector<std::vector<float>> buf;
        int size = 0, write = 0;
    };
}
