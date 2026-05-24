/*
 * Sympathetic Resonance — Sitar-style string simulator.
 *
 * Up to 48 parallel tuned feedback comb filters with fractional delay,
 * a soft-clip "jawari" saturation on the summed wet signal,
 * and an L/R spread for the stereo output bus. The 48 strings span
 * (almost) 4 octaves above the root — wide enough to cover a bass
 * clarinet's playable range. For scales with fewer than 12 notes per
 * octave, only the first 4·notesPerOctave string slots are populated;
 * the remaining slots get freq = 0 and are silent.
 *
 * The user-facing UI surfaces two cursor knobs into this 48-string set:
 *   - NUM  STRINGS: how many strings ring simultaneously (1-48)
 *   - OFFSET:       index of the first ringing string (0-47)
 * with "clip if past the populated count" semantics.
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

// Equal-tempered Hz values for pitch classes C..B in octave 3 (MIDI 48..59).
// These are the reference frequencies at OCT=3 (the default). The OCT knob
// shifts everything by integer octaves: actual root Hz = kRootHz[idx]
// multiplied by 2^(fOctave - 3).
static constexpr float       kRootHz[12]     = { 130.81f, 138.59f, 146.83f, 155.56f,
                                                 164.81f, 174.61f, 185.00f, 196.00f,
                                                 207.65f, 220.00f, 233.08f, 246.94f };
static constexpr const char* kRootLabels[12] = { "C",  "C#", "D",  "D#", "E",  "F",
                                                 "F#", "G",  "G#", "A",  "A#", "B" };
static constexpr uint32_t kNumRootNotes = 12;

// ---------------------------------------------------------------------------------------------------------------------

class SitarPlugin : public Plugin
{
public:
    // 48 strings = 4 octaves of chromatic, which is the densest scale we
    // support. Sparser scales populate only their first 4·notesPerOctave
    // slots (e.g. 28 for 7-note, 20 for pentatonic) and leave the rest at
    // freq = 0 / inactive.
    static constexpr uint32_t kNumStrings        = 48;
    static constexpr uint32_t kStringRangeOctaves = 4;

    // String Hz range bounds the user-facing knob and any scale/root
    // automation. Lower bound is 0 because un-populated slots (when a
    // sparser scale than chromatic is selected) are explicitly set to 0
    // to signal "not used by this scale". Upper bound covers C8 (4186 Hz).
    static constexpr float kStringMinHz  = 0.0f;
    static constexpr float kStringMaxHz  = 4186.01f;
    static constexpr float kMinAllowedHz = 27.5f; // comb filter buffer floor

    // Absolute octave for the root: OCT=k means root pitch class lives in
    // octave k. Range covers the most musically useful sympathetic-string
    // territory:
    //   OCT=2  (root C → C2,  65 Hz)   sub-bass / drone
    //   OCT=3  (root C → C3, 131 Hz)   default — lower-middle register
    //   OCT=4  (root C → C4, 262 Hz)   middle C
    //   OCT=5  (root C → C5, 523 Hz)   "shimmer-only" — only upper harmonics
    //                                  of bassy input excite the strings
    //   OCT=6  (root C → C6, 1046 Hz)  airy / glassy
    // Strings whose computed frequency exceeds kStringMaxHz (4186 Hz / C8)
    // clip silently to that cap.
    static constexpr float kOctaveMin = 2.0f;
    static constexpr float kOctaveMax = 6.0f;
    // The reference octave at which kRootHz[] is tabulated; subtracted from
    // fOctave when computing the per-string Hz multiplier.
    static constexpr float kOctaveRef = 3.0f;

    enum ParamIndex {
        kParamString1 = 0,
        // ... up to kParamString1 + (kNumStrings - 1)
        kParamNumActive   = kNumStrings,      // 48 — how many strings ring (1..kNumStrings)
        kParamOctave,                         // 49 — global pitch shift (kOctaveMin..kOctaveMax)
        kParamDecay,                          // 50
        kParamMix,                            // 51
        kParamJawari,                         // 52
        kParamScale,                          // 53 — enum, selects from kScales[]
        kParamRootNote,                       // 54 — enum, selects from kRootHz[]
        kParamAudition,                       // 55 — boolean, "Test Scale" button
        kParamBloom,                          // 56 — bridge cross-coupling between strings
        kParamStereoWide,                     // 57 — boolean, "wide" alternating-pan mode
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
        fNumActive    = 13;          // pick a musical default below 48
        fOctave       = kOctaveRef;  // default = octave 3 (C3 if root=C)
        fStereoWide   = false;       // linear sweep panning by default
        fDecay        = 0.5f;
        fMix          = 0.5f;
        fJawari       = 0.0f;
        fBloom        = 0.0f;
        fScaleIdx     = 0;           // Major
        fRootIdx      = 0;           // C2

        sampleRateChanged(getSampleRate());
        // Seed the string_N values and the pan table from the default scale
        // + root. We do NOT notify the host here — parameters are queried at
        // startup via getParameterValue(), and requestParameterValueChange
        // is not safe before the plugin is registered with the host.
        applyScaleAndRoot(/*notifyHost=*/ false);
    }

protected:
    // ---------------- Information ----------------

    // All branding methods derive from DistrhoPluginInfo.h, which is the
    // single source of truth for stable-vs-beta identity (see SITAR_BETA).
    const char* getLabel()       const override { return DISTRHO_PLUGIN_BRAND; }
    const char* getDescription() const override
    {
        return "Sympathetic resonance string simulator. Drives up to 48 tuned comb-filter strings spanning "
               "4 octaves above the root to produce sitar-like ringing, microtonal accompaniment.";
    }
    const char* getMaker()    const override { return DISTRHO_PLUGIN_BRAND; }
    const char* getHomePage() const override { return DISTRHO_PLUGIN_URI; }
    const char* getLicense()  const override { return "ISC"; }
    uint32_t    getVersion()  const override { return d_version(0, 1, 0); }
    int64_t     getUniqueId() const override
    {
        // Match the d_cconst that DPF uses for DISTRHO_PLUGIN_UNIQUE_ID.
#ifdef SITAR_BETA
        return d_cconst('d', 'S', 't', 'b');
#else
        return d_cconst('d', 'S', 't', 'r');
#endif
    }

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
        case kParamNumActive:
            parameter.hints      = kParameterIsAutomatable | kParameterIsInteger;
            parameter.name       = "Active Strings";
            parameter.symbol     = "num_active";
            parameter.ranges.min = 1.0f;
            parameter.ranges.max = static_cast<float>(kNumStrings);
            parameter.ranges.def = 13.0f;
            break;

        case kParamOctave:
            parameter.hints      = kParameterIsAutomatable | kParameterIsInteger;
            parameter.name       = "Octave";
            parameter.symbol     = "octave";
            parameter.ranges.min = kOctaveMin;
            parameter.ranges.max = kOctaveMax;
            parameter.ranges.def = kOctaveRef;
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

        case kParamStereoWide:
            parameter.hints      = kParameterIsAutomatable | kParameterIsBoolean;
            parameter.name       = "Wide Stereo";
            parameter.symbol     = "stereo_wide";
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
        case kParamNumActive:    return static_cast<float>(fNumActive);
        case kParamOctave:       return fOctave;
        case kParamDecay:        return fDecay;
        case kParamMix:          return fMix;
        case kParamJawari:       return fJawari;
        case kParamScale:        return static_cast<float>(fScaleIdx);
        case kParamRootNote:     return static_cast<float>(fRootIdx);
        case kParamAudition:     return fAuditionActive ? 1.0f : 0.0f;
        case kParamBloom:        return fBloom;
        case kParamStereoWide:   return fStereoWide ? 1.0f : 0.0f;
        }
        return 0.0f;
    }

    void setParameterValue(uint32_t index, float value) override
    {
        if (index < kNumStrings)
        {
            // Per-string ports are user-overridable. Any non-zero value the
            // user writes is treated as an explicit "play this string at this
            // frequency"; a write of 0 silences the string. The DSP echoes
            // these ports via requestParameterValueChange whenever scale/root
            // change, so most "writes" we see are actually echoes — that's
            // fine, we just store and retune.
            fStringFreqs[index] = value;
            fStringActive[index] = (value >= kMinAllowedHz);
            retuneString(index);
            return;
        }
        switch (index)
        {
        case kParamNumActive:
        {
            int32_t n = static_cast<int32_t>(value + 0.5f);
            if (n < 1)                                       n = 1;
            if (n > static_cast<int32_t>(kNumStrings))       n = kNumStrings;
            const uint32_t newN = static_cast<uint32_t>(n);
            if (newN != fNumActive)
            {
                fNumActive = newN;
                // Re-populate so strings beyond NUM zero out in the UI and
                // strings inside NUM resume their scale-anchored frequencies.
                applyScaleAndRoot(/*notifyHost=*/ true);
            }
            break;
        }
        case kParamOctave:
        {
            float o = value;
            if (o < kOctaveMin) o = kOctaveMin;
            if (o > kOctaveMax) o = kOctaveMax;
            if (o != fOctave)
            {
                fOctave = o;
                applyScaleAndRoot(/*notifyHost=*/ true);
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
                while (fAuditionPhase < kNumStrings && !fStringActive[fAuditionPhase])
                    ++fAuditionPhase;
                fAuditionSampleCount = 0;
                // Re-seed the PRNG so every run of the test produces the
                // exact same noise burst → same envelope → same peak levels.
                // Without this, the seed carries over from the previous run
                // and successive auditions sound (and meter) differently.
                fNoiseSeed           = 0x12345u;
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

        case kParamStereoWide:
        {
            const bool wide = value > 0.5f;
            if (wide != fStereoWide)
            {
                fStereoWide = wide;
                // Re-derive the pan table for the new mode.
                applyScaleAndRoot(/*notifyHost=*/ false);
            }
            break;
        }
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

        // Count effectively-ringing strings — used to dynamically normalise
        // the wet sum (1/√N keeps perceived loudness roughly constant as
        // the user sweeps NUM) and to scale the bloom stability bound.
        // applyScaleAndRoot already zeroes any string beyond NUM by setting
        // its frequency to 0 and fStringActive to false, so we just count
        // the active flags here.
        uint32_t activeCount = 0;
        for (uint32_t s = 0; s < kNumStrings; ++s)
            if (fStringActive[s]) ++activeCount;
        const float stringNorm = activeCount > 0
            ? 1.0f / std::sqrt(static_cast<float>(activeCount))
            : 0.0f;
        const float bridgeNorm = activeCount > 0
            ? 1.0f / static_cast<float>(activeCount)
            : 0.0f;

        // Bridge-bus coupling: every active string contributes to a shared
        // bridge signal that's fed back into every string's input next sample,
        // mirroring how a real sitar's tarafs share one physical bridge.
        //
        // Stability: a feedback comb has resonance peaks at *every* integer
        // multiple of its tuned frequency, so K ≈ 2–4 strings typically
        // respond at any given input pitch (octave doublets, harmonic
        // overlaps). The cross-coupled system is bounded for
        // bloomCoupling < N · (1 - fb) / K, and beyond that the wet-sum
        // tanh limiter catches any saturation. We scale the stability bound
        // by activeCount (not the static kNumStrings) so the bloom feedback
        // halo behaves consistently as the user sweeps NUM STRINGS.
        const float stableMax = 0.3f
            * static_cast<float>(activeCount > 0 ? activeCount : 1)
            * (1.0f - fFeedback);
        const float bloomCouplingMax = stableMax < kBloomCouplingCap
            ? stableMax : kBloomCouplingCap;
        const float bloomCoupling = fBloom * bloomCouplingMax;

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
                    if (!fStringActive[s]) continue;     // skip inactive combs entirely
                    const float v = fCombs[s].process(s == fAuditionPhase ? excite : 0.0f);
                    wetL += v * fPanL[s];
                    wetR += v * fPanR[s];
                }

                // Audition uses a fixed soft norm — only one string is excited
                // at a time, so √N would over-attenuate.
                constexpr float kAuditionNorm = 1.0f / 2.0f;
                wetL *= kAuditionNorm;
                wetR *= kAuditionNorm;

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
                // We only walk through the strings that are scale-populated
                // (fStringActive[]). For pentatonic scales etc. that's
                // far less than 48, keeping the audition's running time
                // sensible.
                if (++fAuditionSampleCount >= fAuditionPhaseSamples)
                {
                    fAuditionSampleCount = 0;
                    // Re-seed the PRNG so every string gets the *same* noise
                    // burst. Without this each string sees a different chunk
                    // of the same sequence and the random alignment with its
                    // resonance frequency adds ~0.5-1 dB of peak-level jitter
                    // between strings.
                    fNoiseSeed = 0x12345u;
                    // Find the next active phase index.
                    uint32_t next = fAuditionPhase + 1;
                    while (next < kNumStrings && !fStringActive[next])
                        ++next;
                    if (next >= kNumStrings)
                    {
                        fAuditionActive = false;
                        fAuditionPhase  = 0;
                        requestParameterValueChange(kParamAudition, 0.0f);
                    }
                    else
                    {
                        fAuditionPhase = next;
                    }
                }
                continue; // skip normal-mode mixing below
            }

            // ----- Normal mode -----
            const float bloomInput = (bloomCoupling > 0.0f)
                ? std::tanh(bloomCoupling * fBridgeState)
                : 0.0f;
            const float stringInput = x + bloomInput;

            // Skip inactive combs entirely — saves ~98% of comb math when
            // NUM is low. Inactive combs get cleared in applyScaleAndRoot
            // on the active→inactive transition so they don't return with
            // stale resonance baked in if the user turns NUM back up.
            float bridgeAcc = 0.0f;
            for (uint32_t s = 0; s < kNumStrings; ++s)
            {
                if (!fStringActive[s]) continue;
                const float v = fCombs[s].process(stringInput);
                wetL += v * fPanL[s];
                wetR += v * fPanR[s];
                bridgeAcc += v;
            }

            // One-pole DC blocker on the bridge bus (cutoff ~30 Hz at 48 kHz).
            // Every comb has gain 1/(1-fb) at DC and all active strings
            // respond identically to DC, so without this the cross-coupled
            // DC mode would go unstable for any non-trivial bloom, silently
            // pushing the strings into a saturated DC state until the audible
            // AC content collapses to zero.
            //   y[n] = x[n] - x[n-1] + alpha * y[n-1]
            const float bridgeRaw = bridgeAcc * bridgeNorm;
            fBridgeState = bridgeRaw - fBridgeDcX + kBridgeDcAlpha * fBridgeState;
            fBridgeDcX   = bridgeRaw;

            wetL *= stringNorm;
            wetR *= stringNorm;

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
        // The comb-filter buffer needs a valid frequency above the floor.
        // Slots with freq = 0 (un-populated by the current scale) still get
        // a comb at the floor so it has a safe state, but fStringActive[i]
        // is false so run() / bloom skip it entirely.
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
       Recompute every string_N frequency from the current scale + root.
       The first 4·notesPerOctave slots get scale-anchored frequencies (root
       in octave 0 through degree n-1 in octave 3, i.e. roughly the bass
       clarinet range, 4 octaves up). Any remaining slots get freq = 0 and
       are marked inactive — that's how a sparser scale (e.g. pentatonic with
       20 populated slots) tells the wet sum and the bloom bus to skip them.

       JS-side scale/root dropdowns just write to the LV2 scale/root_note
       ports; this function then pushes the recomputed string frequencies
       back to the host so any generic-UI display stays in sync.
     */
    void applyScaleAndRoot(bool notifyHost)
    {
        const ScaleDef& scale  = kScales[fScaleIdx];
        const float     rootHz = kRootHz[fRootIdx];
        const uint32_t  n      = scale.notesPerOctave;
        const uint32_t  populated = kStringRangeOctaves * n;   // e.g. 28 for 7-note, 48 for chromatic
        // The "effective" count caps NUM by the scale's populated count.
        // Strings beyond this get freq = 0 and are silent — that way the
        // UI shows exactly which strings are actually contributing, and
        // turning NUM down literally zeros the trailing knobs.
        const uint32_t  effective = fNumActive < populated ? fNumActive : populated;
        // OCT knob is absolute octave (2..6). Convert to a multiplier relative
        // to the reference octave at which kRootHz[] is tabulated.
        const float     octMul = std::pow(2.0f, fOctave - kOctaveRef);

        // Pan table: spread the *effective* (= active and audible) string
        // count across [-1, +1]. Two layouts:
        //   - Linear  (fStereoWide=false): string i -> pos = 2i/(N-1) - 1,
        //     so adjacent strings sit next to each other in the stereo
        //     field. Scale ascends left-to-right.
        //   - Wide    (fStereoWide=true):  string 0 -> hard-L, string 1 ->
        //     hard-R, string 2 -> next-to-hard-L, string 3 -> next-to-hard-R,
        //     ... pairs converge toward the centre. Adjacent strings sit on
        //     opposite sides; the audible width is more enveloping at the
        //     cost of pitch-position correlation. Mono playback is
        //     unaffected: both modes share the same set of pan positions.
        // Strings beyond effective are silent; their pan gains don't matter
        // but we zero them for cleanliness.
        for (uint32_t i = 0; i < kNumStrings; ++i)
        {
            if (i < effective)
            {
                float pos;
                if (effective == 1)
                {
                    pos = 0.0f;
                }
                else if (fStereoWide)
                {
                    const uint32_t pair = i / 2u;
                    const float magnitude = 1.0f
                        - 2.0f * static_cast<float>(pair) / static_cast<float>(effective - 1);
                    const float sign = (i & 1u) ? 1.0f : -1.0f;
                    pos = sign * magnitude;
                }
                else
                {
                    pos = (2.0f * static_cast<float>(i) / static_cast<float>(effective - 1)) - 1.0f;
                }
                const float theta = (pos + 1.0f) * 0.5f * kHalfPi;
                fPanL[i] = std::cos(theta);
                fPanR[i] = std::sin(theta);
            }
            else
            {
                fPanL[i] = 0.0f;
                fPanR[i] = 0.0f;
            }
        }

        for (uint32_t i = 0; i < kNumStrings; ++i)
        {
            float freq = 0.0f;
            bool  active = false;

            if (i < effective)
            {
                const uint32_t scaleIdx = i % n;
                const uint32_t octaves  = i / n;
                freq = rootHz * scale.ratios[scaleIdx]
                              * std::pow(2.0f, static_cast<float>(octaves))
                              * octMul;
                if (freq > kStringMaxHz) freq = kStringMaxHz;
                active = (freq >= kMinAllowedHz);
            }

            // active→inactive transition: clear the comb buffer so it doesn't
            // carry stale resonance into a future re-activation. run() skips
            // inactive combs (saves CPU); without this clear, a string that
            // re-activates would start from whatever state it was frozen in.
            if (fStringActive[i] && !active)
                fCombs[i].clear();

            fStringFreqs[i]  = freq;
            fStringActive[i] = active;
            retuneString(i);
            if (notifyHost)
                requestParameterValueChange(i, freq);
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

    // Equal-power pan: theta in [0, pi/2]; -1 -> all-left, +1 -> all-right.
    // The pan table is rebuilt inside applyScaleAndRoot based on the current
    // scale's populated count, so audible strings always span the full
    // stereo field.
    static constexpr float kHalfPi = 1.5707963267948966f;

    CombFilter fCombs[kNumStrings];

    // fStringFreqs[i] is the effective frequency for slot i. Slots beyond
    // the current scale's populated count get 0.0f (signalling "unused by
    // this scale"); the host's generic UI shows 0 Hz for those.
    // fStringActive[i] is false either because the slot is un-populated or
    // because the user explicitly wrote 0 to its port. Inactive slots are
    // skipped in the wet sum and the bloom bus.
    float fStringFreqs[kNumStrings] {};
    bool  fStringActive[kNumStrings] {};

    // User-visible window into the 48-string set: the first fNumActive
    // populated strings ring, the rest are silent. fOctave applies a global
    // 2^fOctave multiplier on top of the scale-anchored frequencies.
    uint32_t fNumActive = 13;
    float    fOctave    = 0.0f;
    bool     fStereoWide = false;

    float fDecay  = 0.5f;
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
