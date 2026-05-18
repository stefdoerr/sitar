/*
 * Sympathetic Resonance — Sitar-style string simulator.
 *
 * 13 parallel tuned feedback comb filters with fractional delay,
 * a soft-clip "jawari" saturation on the summed wet signal,
 * and an alternating L/R spread for the stereo output bus.
 */

#include "DistrhoPlugin.hpp"
#include "CombFilter.hpp"

#include <cmath>

START_NAMESPACE_DISTRHO

// ---------------------------------------------------------------------------------------------------------------------
// Scale + Root tables — keep these IN SYNC with the parallel SCALE_KEYS and
// ROOT_HZ arrays in modgui/script-sitar.js. The C++ table is the source of
// truth: the JS only maps the dropdown selection to an integer index that
// goes to the LV2 `scale` / `root_note` ports, and the DSP recomputes the 13
// string frequencies from there.

struct ScaleDef {
    const char* label;            // display name (used in enum scale points)
    uint32_t    notesPerOctave;
    float       ratios[12];       // first ratio is always 1.0
};

static constexpr ScaleDef kScales[] = {
    // ---- Western ----
    { "Major",            7, { 1.0f,  9.0f/8,  5.0f/4,  4.0f/3,  3.0f/2,  5.0f/3, 15.0f/8 } },
    { "Natural Minor",    7, { 1.0f,  9.0f/8,  6.0f/5,  4.0f/3,  3.0f/2,  8.0f/5,  9.0f/5 } },
    { "Harmonic Minor",   7, { 1.0f,  9.0f/8,  6.0f/5,  4.0f/3,  3.0f/2,  8.0f/5, 15.0f/8 } },
    { "Melodic Minor",    7, { 1.0f,  9.0f/8,  6.0f/5,  4.0f/3,  3.0f/2,  5.0f/3, 15.0f/8 } },
    { "Dorian",           7, { 1.0f,  9.0f/8,  6.0f/5,  4.0f/3,  3.0f/2,  5.0f/3, 16.0f/9 } },
    { "Phrygian",         7, { 1.0f, 16.0f/15, 6.0f/5,  4.0f/3,  3.0f/2,  8.0f/5, 16.0f/9 } },
    { "Lydian",           7, { 1.0f,  9.0f/8,  5.0f/4, 45.0f/32, 3.0f/2,  5.0f/3, 15.0f/8 } },
    { "Mixolydian",       7, { 1.0f,  9.0f/8,  5.0f/4,  4.0f/3,  3.0f/2,  5.0f/3, 16.0f/9 } },
    { "Locrian",          7, { 1.0f, 16.0f/15, 6.0f/5,  4.0f/3, 64.0f/45, 8.0f/5, 16.0f/9 } },
    { "Major Pentatonic", 5, { 1.0f,  9.0f/8,  5.0f/4,  3.0f/2,  5.0f/3                  } },
    { "Minor Pentatonic", 5, { 1.0f,  6.0f/5,  4.0f/3,  3.0f/2,  9.0f/5                  } },
    { "Blues",            6, { 1.0f,  6.0f/5,  4.0f/3, 45.0f/32, 3.0f/2,  9.0f/5         } },
    { "Pythagorean",      7, { 1.0f,  9.0f/8, 81.0f/64, 4.0f/3,  3.0f/2, 27.0f/16, 243.0f/128 } },
    { "Chromatic 12-TET",12, { 1.0f,
                               1.0594630943592953f, 1.122462048309373f,  1.1892071150027210f,
                               1.2599210498948732f, 1.3348398541700344f, 1.4142135623730951f,
                               1.4983070768766815f, 1.5874010519681994f, 1.6817928305074290f,
                               1.7817974362806785f, 1.8877486253633871f } },
    // ---- Turkish / Arabic makams ----
    // Every makam needs at least one neutral interval (the "in-between" pitches)
    // to actually sound like itself — pure 5-limit JI collapses them to plain
    // major/minor scales. 11- and 13-limit JI ratios give simple rational
    // approximations of the canonical neutral perdes.
    //
    //   Rast    — neutral 3rd (11/9 ≈ 347¢) and neutral 7th (11/6 ≈ 1049¢)
    //   Ushshak — neutral 2nd (12/11 ≈ 151¢, "Segâh perde")
    //   Hicaz   — Arabic variant: neutral 6th (13/8 ≈ 840¢) instead of M6
    //   Saba    — neutral 2nd (12/11) and Hisar 6th (13/8 ≈ 840¢) plus the
    //             characteristic diminished 4th (320/243 ≈ 476¢)
    { "Rast",             7, { 1.0f,   9.0f/8,   11.0f/9,    4.0f/3,    3.0f/2,  27.0f/16,  11.0f/6 } },
    { "Ushshak",          7, { 1.0f,  12.0f/11, 32.0f/27,    4.0f/3,    3.0f/2, 128.0f/81,  16.0f/9 } },
    { "Hicaz",            7, { 1.0f,  16.0f/15,  5.0f/4,     4.0f/3,    3.0f/2,  13.0f/8,   16.0f/9 } },
    { "Saba",             7, { 1.0f,  12.0f/11, 32.0f/27, 320.0f/243,   3.0f/2,  13.0f/8,   16.0f/9 } },

    // ---- Hindustani ragas (sitar sympathetic-string tunings) ----
    // Standard Just-intonation tunings of the raga's swaras (notes), used
    // for the taraf (sympathetic) strings beneath the frets. Some ragas
    // (Marwa, Malkauns) use fewer than 7 swaras — the wrap-around logic in
    // applyScaleAndRoot picks them up automatically via notesPerOctave.
    //
    //   Yaman    — evening raga, Lydian flavor (tivra Ma)
    //   Bhairav  — morning raga, komal re + komal dha + shuddha Ga + Ni
    //   Bhairavi — closing raga, all komal (Phrygian-equivalent)
    //   Todi    — morning, komal re/ga/dha + tivra Ma + shuddha Ni
    //   Marwa   — sunset, hexatonic (no Pa); same notes as Puriya
    //   Malkauns — late night, pentatonic (no Re, no Pa)
    { "Raga Yaman",       7, { 1.0f,   9.0f/8,    5.0f/4, 45.0f/32,  3.0f/2,  5.0f/3,  15.0f/8 } },
    { "Raga Bhairav",     7, { 1.0f,  16.0f/15,   5.0f/4,  4.0f/3,   3.0f/2,  8.0f/5,  15.0f/8 } },
    { "Raga Bhairavi",    7, { 1.0f,  16.0f/15,   6.0f/5,  4.0f/3,   3.0f/2,  8.0f/5,  16.0f/9 } },
    { "Raga Todi",        7, { 1.0f,  16.0f/15,   6.0f/5, 45.0f/32,  3.0f/2,  8.0f/5,  15.0f/8 } },
    { "Raga Marwa",       6, { 1.0f,  16.0f/15,   5.0f/4, 45.0f/32,           5.0f/3,  15.0f/8 } },
    { "Raga Malkauns",    5, { 1.0f,             6.0f/5,  4.0f/3,            8.0f/5,   16.0f/9 } },
};
static constexpr uint32_t kNumScales = sizeof(kScales) / sizeof(kScales[0]);

// Standard equal-tempered Hz values, C2 (MIDI 36) through B2 (MIDI 47).
static constexpr float       kRootHz[12]     = { 65.41f, 69.30f,  73.42f,  77.78f,
                                                 82.41f, 87.31f,  92.50f,  98.00f,
                                                103.83f, 110.00f, 116.54f, 123.47f };
static constexpr const char* kRootLabels[12] = { "C2", "C#2", "D2", "D#2", "E2", "F2",
                                                 "F#2","G2", "G#2","A2", "A#2","B2" };
static constexpr uint32_t kNumRootNotes = 12;

// ---------------------------------------------------------------------------------------------------------------------

class SitarPlugin : public Plugin
{
public:
    static constexpr uint32_t kNumStrings = 13;
    // Range trimmed to where octave changes still produce an audibly distinct
    // result. Below -1 strings either drop sub-bass or mute against the floor;
    // above +3 the lower strings start crowding the upper piano register
    // where sympathetic resonance loses its grounding feel.
    static constexpr float    kOctaveMin  = -1.0f;
    static constexpr float    kOctaveMax  =  3.0f;

    // String Hz range bounds the user-facing knob and any scale/root/octave
    // automation. Picked to cover the full piano: A0 (27.5 Hz) ... C8 (4186 Hz).
    // Extreme combinations (e.g. Minor Pentatonic at B2 + octave +3 ~5267 Hz)
    // will clip into this range, which keeps the strings in musically useful
    // territory and avoids subsonic / dog-whistle resonators.
    static constexpr float kStringMinHz  = 27.5f;
    static constexpr float kStringMaxHz  = 4186.01f;
    static constexpr float kMinAllowedHz = 27.5f; // comb filter buffer floor

    enum ParamIndex {
        kParamString1 = 0,
        // ... up to kParamString1 + 12
        kParamOctave  = kNumStrings,      // 13
        kParamDecay,                      // 14
        kParamMix,                        // 15
        kParamJawari,                     // 16
        kParamScale,                      // 17  — enum, selects from kScales[]
        kParamRootNote,                   // 18  — enum, selects from kRootHz[]
        kParamAudition,                   // 19  — boolean, "Test Scale" button
        kParamBloom,                      // 20  — bridge cross-coupling between strings
        kNumParams
    };

    // Absolute cap on bridge-bus coupling. With the bridge DC blocker in
    // place, the cross-coupled-comb system is mathematically stable for
    // bloomCoupling < N · (1 - fb), so we tie the effective coupling to the
    // current per-string feedback at runtime (see run()) and additionally
    // cap it so a low-Decay setting doesn't produce an absurdly hot bloom
    // path. tanh is kept as a microsecond backstop; with the DC blocker and
    // the stability-tied scaling it almost never engages.
    static constexpr float kBloomCouplingCap = 0.4f;

    // One-pole HPF pole for the bridge DC blocker. alpha = 0.996 puts the
    // -3 dB cutoff at ~30 Hz at 48 kHz — just below the lowest piano string
    // (A0 = 27.5 Hz), so it removes DC and sub-rumble without touching the
    // audible bass register.
    static constexpr float kBridgeDcAlpha = 0.996f;

    SitarPlugin()
        : Plugin(kNumParams, 0, 0)
    {
        fOctave    = 1.0f;
        fDecay     = 0.5f;
        fMix       = 0.5f;
        fJawari    = 0.0f;
        fBloom     = 0.0f;
        fScaleIdx  = 0;   // Major
        fRootIdx   = 0;   // C2

        initPanTable();
        sampleRateChanged(getSampleRate());
        // Seed the 13 string_N values from the default scale + root. We do NOT
        // notify the host here — parameters are queried at startup via
        // getParameterValue(), and requestParameterValueChange is not safe
        // before the plugin is registered with the host.
        applyScaleAndRoot(/*notifyHost=*/ false);
    }

protected:
    // ---------------- Information ----------------

    const char* getLabel()       const override { return "sitar"; }
    const char* getDescription() const override
    {
        return "Sympathetic resonance string simulator. Drives 13 tuned comb-filter strings from the input signal "
               "to produce sitar-like ringing, microtonal accompaniment.";
    }
    const char* getMaker()    const override { return "sitar"; }
    const char* getHomePage() const override { return "http://sitar.local/plugins/sitar"; }
    const char* getLicense()  const override { return "ISC"; }
    uint32_t    getVersion()  const override { return d_version(0, 1, 0); }
    int64_t     getUniqueId() const override { return d_cconst('d', 'S', 't', 'r'); }

    // ---------------- Init ----------------

    void initAudioPort(bool input, uint32_t index, AudioPort& port) override
    {
        if (input)
        {
            port.groupId = kPortGroupMono;
        }
        else
        {
            port.groupId = kPortGroupStereo;
        }
        Plugin::initAudioPort(input, index, port);
    }

    void initParameter(uint32_t index, Parameter& parameter) override
    {
        if (index < kNumStrings)
        {
            char nameBuf[16];
            std::snprintf(nameBuf, sizeof(nameBuf), "String %u", static_cast<unsigned>(index + 1));
            char symBuf[16];
            std::snprintf(symBuf, sizeof(symBuf), "string_%u", static_cast<unsigned>(index + 1));

            parameter.hints      = kParameterIsAutomatable;
            parameter.name       = nameBuf;
            parameter.symbol     = symBuf;
            parameter.unit       = "Hz";
            parameter.ranges.min = kStringMinHz;
            parameter.ranges.max = kStringMaxHz;
            parameter.ranges.def = fStringFreqs[index];
            return;
        }

        switch (index)
        {
        case kParamOctave:
            parameter.hints      = kParameterIsAutomatable | kParameterIsInteger;
            parameter.name       = "Octave";
            parameter.symbol     = "octave";
            parameter.unit       = "oct";
            parameter.ranges.min = kOctaveMin;
            parameter.ranges.max = kOctaveMax;
            parameter.ranges.def = 1.0f;
            break;

        case kParamDecay:
            parameter.hints      = kParameterIsAutomatable;
            parameter.name       = "Decay";
            parameter.symbol     = "decay";
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 1.0f;
            parameter.ranges.def = 0.5f;
            break;

        case kParamMix:
            parameter.hints      = kParameterIsAutomatable;
            parameter.name       = "Mix";
            parameter.symbol     = "mix";
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 1.0f;
            parameter.ranges.def = 0.5f;
            break;

        case kParamJawari:
            parameter.hints      = kParameterIsAutomatable;
            parameter.name       = "Jawari";
            parameter.symbol     = "jawari";
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 1.0f;
            parameter.ranges.def = 0.0f;
            break;

        case kParamScale:
        {
            parameter.hints      = kParameterIsAutomatable | kParameterIsInteger;
            parameter.name       = "Scale";
            parameter.symbol     = "scale";
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = static_cast<float>(kNumScales - 1);
            parameter.ranges.def = 0.0f;
            parameter.enumValues.count          = static_cast<uint8_t>(kNumScales);
            parameter.enumValues.restrictedMode = true;
            ParameterEnumerationValue* const ev = new ParameterEnumerationValue[kNumScales];
            for (uint32_t i = 0; i < kNumScales; ++i)
            {
                ev[i].value = static_cast<float>(i);
                ev[i].label = kScales[i].label;
            }
            parameter.enumValues.values = ev;
            break;
        }

        case kParamRootNote:
        {
            parameter.hints      = kParameterIsAutomatable | kParameterIsInteger;
            parameter.name       = "Root Note";
            parameter.symbol     = "root_note";
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = static_cast<float>(kNumRootNotes - 1);
            parameter.ranges.def = 0.0f;
            parameter.enumValues.count          = static_cast<uint8_t>(kNumRootNotes);
            parameter.enumValues.restrictedMode = true;
            ParameterEnumerationValue* const ev = new ParameterEnumerationValue[kNumRootNotes];
            for (uint32_t i = 0; i < kNumRootNotes; ++i)
            {
                ev[i].value = static_cast<float>(i);
                ev[i].label = kRootLabels[i];
            }
            parameter.enumValues.values = ev;
            break;
        }

        case kParamAudition:
            parameter.hints      = kParameterIsAutomatable | kParameterIsBoolean;
            parameter.name       = "Test Scale";
            parameter.symbol     = "audition";
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 1.0f;
            parameter.ranges.def = 0.0f;
            break;

        case kParamBloom:
            parameter.hints      = kParameterIsAutomatable;
            parameter.name       = "Bloom";
            parameter.symbol     = "bloom";
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 1.0f;
            parameter.ranges.def = 0.0f;
            break;
        }
    }

    // ---------------- State access ----------------

    float getParameterValue(uint32_t index) const override
    {
        if (index < kNumStrings) return fStringFreqs[index];
        switch (index)
        {
        case kParamOctave:   return fOctave;
        case kParamDecay:    return fDecay;
        case kParamMix:      return fMix;
        case kParamJawari:   return fJawari;
        case kParamScale:    return static_cast<float>(fScaleIdx);
        case kParamRootNote: return static_cast<float>(fRootIdx);
        case kParamAudition: return fAuditionActive ? 1.0f : 0.0f;
        case kParamBloom:    return fBloom;
        }
        return 0.0f;
    }

    void setParameterValue(uint32_t index, float value) override
    {
        if (index < kNumStrings)
        {
            // If `value` matches what we'd currently compute from the stored
            // base + octave (within float tolerance), this is just the host
            // echoing back our own requestParameterValueChange — leave the
            // base alone so an octave round-trip can restore clipped strings.
            // Otherwise the change came from the user (knob drag, automation,
            // preset recall) and we back-derive a fresh base from it.
            const float octMul = std::pow(2.0f, fOctave);
            float expectedEff = fStringBaseFreqs[index] * octMul;
            if (expectedEff < kStringMinHz) expectedEff = kStringMinHz;
            if (expectedEff > kStringMaxHz) expectedEff = kStringMaxHz;

            const bool isEcho = std::fabs(value - expectedEff) < 0.01f;
            fStringFreqs[index] = value;
            if (!isEcho && octMul > 0.0f)
            {
                fStringBaseFreqs[index] = value / octMul;
                // A user-initiated edit is by definition in range, so reactivate.
                fStringActive[index] = true;
            }

            retuneString(index);
            return;
        }
        switch (index)
        {
        case kParamOctave:
        {
            if (value != fOctave)
            {
                fOctave = value;
                recomputeEffectiveFromBase(/*notifyHost=*/ true);
            }
            break;
        }
        case kParamDecay:
            fDecay = value;
            applyDecay();
            break;
        case kParamMix:
            fMix = value;
            break;
        case kParamJawari:
            fJawari = value;
            break;
        case kParamScale:
        {
            uint32_t idx = static_cast<uint32_t>(value + 0.5f);
            if (idx >= kNumScales) idx = kNumScales - 1;
            if (idx != fScaleIdx)
            {
                fScaleIdx = idx;
                applyScaleAndRoot(/*notifyHost=*/ true);
            }
            break;
        }
        case kParamRootNote:
        {
            uint32_t idx = static_cast<uint32_t>(value + 0.5f);
            if (idx >= kNumRootNotes) idx = kNumRootNotes - 1;
            if (idx != fRootIdx)
            {
                fRootIdx = idx;
                applyScaleAndRoot(/*notifyHost=*/ true);
            }
            break;
        }
        case kParamAudition:
        {
            const bool wantActive = value > 0.5f;
            if (wantActive && !fAuditionActive)
            {
                fAuditionActive      = true;
                fAuditionPhase       = 0;
                fAuditionSampleCount = 0;
                for (uint32_t i = 0; i < kNumStrings; ++i)
                    fCombs[i].clear();
                fBridgeState = 0.0f;
                fBridgeDcX   = 0.0f;
            }
            else if (!wantActive && fAuditionActive)
            {
                fAuditionActive = false;
            }
            break;
        }
        case kParamBloom:
            if (value < 0.0f) value = 0.0f;
            if (value > 1.0f) value = 1.0f;
            fBloom = value;
            break;
        }
    }

    // ---------------- Lifecycle ----------------

    void activate() override
    {
        for (uint32_t i = 0; i < kNumStrings; ++i)
            fCombs[i].clear();
        fBridgeState = 0.0f;
        fBridgeDcX   = 0.0f;
    }

    void sampleRateChanged(double newSampleRate) override
    {
        const uint32_t maxDelay = static_cast<uint32_t>(std::ceil(newSampleRate / kMinAllowedHz));
        for (uint32_t i = 0; i < kNumStrings; ++i)
            fCombs[i].setSampleRate(newSampleRate, maxDelay);

        fAuditionPhaseSamples = static_cast<uint32_t>(newSampleRate * 2.0);       // 2 seconds per string
        fAuditionPluckSamples = static_cast<uint32_t>(newSampleRate * 0.030);     // 30 ms pluck
        if (fAuditionPluckSamples == 0) fAuditionPluckSamples = 1;

        retuneAllStrings();
        applyDecay();
    }

    // ---------------- DSP ----------------

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        const float* const in   = inputs[0];
        /* */ float* const outL = outputs[0];
        /* */ float* const outR = outputs[1];

        const float mix     = fMix;
        const float dryGain = 1.0f - mix;
        const float wetGain = mix;
        const float jawari  = fJawari;

        // Jawari drive: 1x to 8x gain into a tanh saturator. Make-up keeps output level roughly steady.
        const float drive   = 1.0f + jawari * 7.0f;
        const float makeUp  = 1.0f / std::tanh(drive);

        constexpr float kStringNorm = 1.0f / 6.0f;

        // Bridge-bus coupling: every active string contributes to a shared
        // bridge signal that's fed back into every string's input next sample,
        // mirroring how a real sitar's tarafs share one physical bridge.
        //
        // Stability: a feedback comb has resonance peaks at *every* integer
        // multiple of its tuned frequency, so K ≈ 2–4 strings typically
        // respond at any given input pitch (octave doublets, harmonic
        // overlaps). The cross-coupled system is bounded for
        // bloomCoupling < N · (1 - fb) / K, and beyond that the wet-sum
        // tanh limiter catches any saturation. We pick coeff = 0.3 here so
        // the closed-loop boost is ~1.4× at K=1 and ~2.5× at K=2 (the most
        // common case) — clearly audible cross-coupling halo. K=4+ overlaps
        // saturate into the limiter rather than running away.
        const float stableMax = 0.3f
            * static_cast<float>(kNumStrings)
            * (1.0f - fFeedback);
        const float bloomCouplingMax = stableMax < kBloomCouplingCap
            ? stableMax : kBloomCouplingCap;
        const float bloomCoupling = fBloom * bloomCouplingMax;

        uint32_t activeCount = 0;
        for (uint32_t s = 0; s < kNumStrings; ++s)
            if (fStringActive[s]) ++activeCount;
        const float bridgeNorm = activeCount > 0
            ? 1.0f / static_cast<float>(activeCount)
            : 0.0f;

        for (uint32_t f = 0; f < frames; ++f)
        {
            const float x = in[f];

            float wetL = 0.0f;
            float wetR = 0.0f;

            // ----- Audition mode: noise pluck routed into a single string -----
            if (fAuditionActive)
            {
                // Synthesize a brief noise burst into only the active string.
                float excite = 0.0f;
                if (fAuditionSampleCount < fAuditionPluckSamples)
                {
                    // Linear-congruential PRNG, real-time safe.
                    fNoiseSeed = fNoiseSeed * 1664525u + 1013904223u;
                    const float n = static_cast<float>(static_cast<int32_t>(fNoiseSeed))
                                  / 2147483648.0f;
                    // Triangular attack-decay envelope across the pluck window.
                    const float t = static_cast<float>(fAuditionSampleCount)
                                  / static_cast<float>(fAuditionPluckSamples);
                    const float env = (t < 0.1f) ? (t / 0.1f) : ((1.0f - t) / 0.9f);
                    excite = n * env * 0.5f;
                }

                for (uint32_t s = 0; s < kNumStrings; ++s)
                {
                    const float v = fCombs[s].process(s == fAuditionPhase ? excite : 0.0f);
                    // During audition, ignore the muted-string flag so we hear
                    // every degree, including those that happen to clip.
                    wetL += v * fPanL[s];
                    wetR += v * fPanR[s];
                }

                wetL *= kStringNorm;
                wetR *= kStringNorm;

                // Same permanent soft-clipper as normal mode.
                wetL = std::tanh(wetL);
                wetR = std::tanh(wetR);

                if (jawari > 0.0f)
                {
                    wetL = std::tanh(wetL * drive) * makeUp;
                    wetR = std::tanh(wetR * drive) * makeUp;
                }

                // Output wet only — pass-through dry would confuse the test.
                outL[f] = wetL;
                outR[f] = wetR;

                // Advance the phase / auto-stop at the end of the sequence.
                if (++fAuditionSampleCount >= fAuditionPhaseSamples)
                {
                    fAuditionSampleCount = 0;
                    if (++fAuditionPhase >= kNumStrings)
                    {
                        fAuditionActive = false;
                        fAuditionPhase  = 0;
                        // Tell the host the button is now released.
                        requestParameterValueChange(kParamAudition, 0.0f);
                    }
                }
                continue; // skip normal-mode mixing below
            }

            // ----- Normal mode -----
            const float bloomInput = (bloomCoupling > 0.0f)
                ? std::tanh(bloomCoupling * fBridgeState)
                : 0.0f;
            const float stringInput = x + bloomInput;

            float bridgeAcc = 0.0f;
            for (uint32_t s = 0; s < kNumStrings; ++s)
            {
                const float v = fCombs[s].process(stringInput);
                if (fStringActive[s])
                {
                    wetL += v * fPanL[s];
                    wetR += v * fPanR[s];
                    bridgeAcc += v;
                }
            }

            // One-pole DC blocker on the bridge bus (cutoff ~30 Hz at 48 kHz).
            // Every comb has gain 1/(1-fb) at DC and all 13 strings respond
            // identically to DC, so without this the cross-coupled DC mode
            // would go unstable for any non-trivial bloom, silently pushing
            // the strings into a saturated DC state until the audible AC
            // content collapses to zero.
            //   y[n] = x[n] - x[n-1] + alpha * y[n-1]
            const float bridgeRaw = bridgeAcc * bridgeNorm;
            fBridgeState = bridgeRaw - fBridgeDcX + kBridgeDcAlpha * fBridgeState;
            fBridgeDcX   = bridgeRaw;

            wetL *= kStringNorm;
            wetR *= kStringNorm;

            // Permanent safety soft-clipper. Each comb's resonance peak gain
            // is 1/(1-fb), which can reach ~333× at max Decay; sustained
            // input near a string's resonance would otherwise drive the wet
            // sum well past ±1 and overflow the host's audio bus. tanh is
            // transparent at typical play levels (~6 % drop at wet = 0.5)
            // and hard-bounds the output to ±1 absolutely. Jawari adds a
            // further drive stage on top for the characteristic sitar buzz.
            wetL = std::tanh(wetL);
            wetR = std::tanh(wetR);

            if (jawari > 0.0f)
            {
                wetL = std::tanh(wetL * drive) * makeUp;
                wetR = std::tanh(wetR * drive) * makeUp;
            }

            outL[f] = dryGain * x + wetGain * wetL;
            outR[f] = dryGain * x + wetGain * wetR;
        }
    }

private:
    // ---------------- Helpers ----------------

    void retuneString(uint32_t i)
    {
        // String frequency values now include the octave shift directly —
        // the comb filter just plays them as-is.
        float target = fStringFreqs[i];
        if (target < kMinAllowedHz) target = kMinAllowedHz;
        fCombs[i].setFrequency(target);
    }

    void retuneAllStrings()
    {
        for (uint32_t i = 0; i < kNumStrings; ++i)
            retuneString(i);
    }

    /**
       Recompute every effective string frequency from the stored base values
       and the current octave. Called when the user turns the Octave knob.
       Clamping happens only on the effective value — base values are kept at
       full precision so an octave round-trip restores clipped strings.
     */
    void recomputeEffectiveFromBase(bool notifyHost)
    {
        const float octMul = std::pow(2.0f, fOctave);
        for (uint32_t i = 0; i < kNumStrings; ++i)
        {
            const float raw = fStringBaseFreqs[i] * octMul;
            fStringActive[i] = (raw >= kStringMinHz && raw <= kStringMaxHz);
            float eff = raw;
            if (eff < kStringMinHz) eff = kStringMinHz;
            if (eff > kStringMaxHz) eff = kStringMaxHz;
            fStringFreqs[i] = eff;
            retuneString(i);
            if (notifyHost)
                requestParameterValueChange(i, eff);
        }
    }

    /**
       Recompute every string_N frequency from the current scale + root_note,
       retune the matching comb filter, and (optionally) push the new values
       back to the host so its UI widgets update. JS-side scale/root dropdowns
       just write to the LV2 scale/root_note ports and let the DSP do the rest.
     */
    void applyScaleAndRoot(bool notifyHost)
    {
        const ScaleDef& scale  = kScales[fScaleIdx];
        const float     rootHz = kRootHz[fRootIdx];
        const uint32_t  n      = scale.notesPerOctave;
        const float     octMul = std::pow(2.0f, fOctave);

        for (uint32_t i = 0; i < kNumStrings; ++i)
        {
            const uint32_t scaleIdx = i % n;
            const uint32_t octaves  = i / n;
            const float baseFreq = rootHz * scale.ratios[scaleIdx]
                                 * std::pow(2.0f, static_cast<float>(octaves));
            fStringBaseFreqs[i] = baseFreq;

            const float raw = baseFreq * octMul;
            fStringActive[i] = (raw >= kStringMinHz && raw <= kStringMaxHz);
            float eff = raw;
            if (eff < kStringMinHz) eff = kStringMinHz;
            if (eff > kStringMaxHz) eff = kStringMaxHz;
            fStringFreqs[i] = eff;
            retuneString(i);
            if (notifyHost)
                requestParameterValueChange(i, eff);
        }
    }

    void applyDecay()
    {
        // The perceived sustain depends exponentially on (1 - feedback), so a
        // linear decay -> feedback map crowds all the useful range into the
        // top 5% of the knob. Apply a (1-x)^7 curve to push the audible
        // sustain time down into the lower half of the knob:
        //
        //   decay = 0.0  -> fb = 0       (instant dieoff)
        //   decay = 0.2  -> fb = 0.79    (short percussive pluck)
        //   decay = 0.3  -> fb = 0.918   (audible short ring)
        //   decay = 0.5  -> fb = 0.992   (~1 s ring, default)
        //   decay = 0.7  -> fb = 0.9998  (many seconds, sympathetic-feel)
        //   decay = 0.9  -> fb ≈ 1       (drone)
        //   decay = 1.0  -> fb clamped to 0.99999 (near-infinite)
        // Exponential approach to a hard cap of 0.998. The previous
        // (1-decay)^7 curve was useful at the bottom of the knob but pushed
        // fb to ~0.99999 at the top, which gives 100,000×+ resonance gain
        // and minute-scale ring times — sustained input near one of the
        // 13 resonances would just integrate upward forever, sounding like
        // runaway. The exp curve still ramps fast through the lower half
        // (fb ≈ 0.97 at decay=0.5) but levels off below 0.998 (max ring
        // ~2.5 s at the lowest string, max gain ~333×, which the wet-output
        // tanh in run() can soft-clip safely).
        const float fb   = 0.998f * (1.0f - std::exp(-7.0f * fDecay));
        // Damping (1-pole LPF coeff inside the loop). Kept fairly close to
        // 1.0 even at short decays so the high-frequency loss doesn't smother
        // the ring before feedback alone has a chance to decay it.
        const float damp = 0.95f + 0.049f * fDecay;     // 0.95 .. 0.999
        for (uint32_t i = 0; i < kNumStrings; ++i)
        {
            fCombs[i].setFeedback(fb);
            fCombs[i].setDamping(damp);
        }
        // Cache the effective feedback so run() can derive a stable bloom
        // coupling from it (the CombFilter clamps to <= 0.99999 internally).
        fFeedback = fb < 0.99999f ? fb : 0.99999f;
    }

    // Pre-baked equal-power pan coefficients for each string.
    // Strings spread evenly across the stereo field; the centre string is dead-centre.
    static constexpr float kHalfPi = 1.5707963267948966f;

    void initPanTable()
    {
        for (uint32_t i = 0; i < kNumStrings; ++i)
        {
            // Map i -> [-1, +1]
            const float pos = (kNumStrings == 1)
                ? 0.0f
                : (2.0f * static_cast<float>(i) / static_cast<float>(kNumStrings - 1)) - 1.0f;
            // Equal-power pan: theta in [0, pi/2], with -1 -> all-left, +1 -> all-right.
            const float theta = (pos + 1.0f) * 0.5f * kHalfPi;
            fPanL[i] = std::cos(theta);
            fPanR[i] = std::sin(theta);
        }
    }

    CombFilter fCombs[kNumStrings];

    // fStringFreqs[i] is the effective frequency (what the user sees on the
    // knob, clamped to [kStringMinHz, kStringMaxHz]).
    // fStringBaseFreqs[i] is the un-clamped logical frequency BEFORE the
    // octave knob is applied — kept at full precision so an octave round-trip
    // (e.g. +3 and back to 0) restores any strings that were temporarily
    // clipped at the extremes.
    // fStringActive[i] is false when the unclamped effective frequency falls
    // outside the piano range. Inactive strings are muted from the wet sum;
    // their knobs sit pinned at the range boundary until octave brings them
    // back in range.
    float fStringFreqs[kNumStrings] {};
    float fStringBaseFreqs[kNumStrings] {};
    bool  fStringActive[kNumStrings] {};

    float fOctave = 1.0f;
    float fDecay  = 0.85f;
    float fMix    = 0.5f;
    float fJawari = 0.0f;
    float fBloom  = 0.0f;

    // Shared bridge-bus state, one sample delayed and DC-blocked. Holds the
    // HPF'd average of all active strings' outputs from the previous sample;
    // bloomCoupling * tanh() of this is mixed into every string's input
    // next sample.
    float fBridgeState = 0.0f;
    // Previous bridge-raw value for the one-pole DC blocker.
    float fBridgeDcX   = 0.0f;

    // Cached comb-filter feedback coefficient, set in applyDecay(). Used by
    // run() to derive the stable bloom-coupling max for the current Decay.
    float fFeedback = 0.0f;

    uint32_t fScaleIdx = 0;
    uint32_t fRootIdx  = 0;

    // Audition (Test Scale): plucks each string in turn for 1s so the user
    // can hear the pitch of every degree of the current scale.
    bool     fAuditionActive       = false;
    uint32_t fAuditionPhase        = 0;     // which string is currently active
    uint32_t fAuditionSampleCount  = 0;     // samples elapsed in the current phase
    uint32_t fAuditionPhaseSamples = 48000; // updated in sampleRateChanged
    uint32_t fAuditionPluckSamples = 1440;  // 30ms at 48kHz, updated below
    uint32_t fNoiseSeed            = 0x12345u;

    float fPanL[kNumStrings] {};
    float fPanR[kNumStrings] {};

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SitarPlugin)
};

// ---------------------------------------------------------------------------------------------------------------------

Plugin* createPlugin()
{
    return new SitarPlugin();
}

END_NAMESPACE_DISTRHO
