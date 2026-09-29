/*
 * Sitar — the jawari vowel sweep ("ee-eh-ah-oh-oo").
 *
 * On a sitar the string rolls over the curved jawari bridge; as its swing
 * decays the contact point moves and the band of overtones it excites sweeps
 * down. Modelled perceptually (physical bridge models tried first — a
 * displacement-modulated delay and a one-sided barrier — dulled the string or
 * added a static buzz; see DEVELOPERS.md): a peak-following envelope places a
 * resonant band-pass (Q 3) from ~4 kHz at a note's attack down to ~350 Hz as
 * it fades 30 dB below its own peak, added on top of the note like a wah,
 * with a touch of tanh buzz. Following each note's own peak (not absolute
 * level) sweeps the same at any playing level. At depth 1 it is the recipe
 * picked by ear from A/B renders; roughly level-neutral on plucked notes.
 *
 * Used once on the dry sound (LEAD) and once per string (JAWARI). Cheap
 * enough for 48 strings on a Dwarf: the per-string sweeps retune their
 * filter every 16 samples (LEAD, a single instance on sharp guitar attacks,
 * every sample) from a table of the filter coefficient over the sweep, so
 * no pow()/tan() runs on the audio thread; the buzz uses a cheap rational
 * tanh (see padeTanh).
 * Header-only so the plugin and the host-less tests link it directly.
 */
#ifndef SITAR_VOWEL_SWEEP_HPP_INCLUDED
#define SITAR_VOWEL_SWEEP_HPP_INCLUDED

#include <cmath>
#include <cstdint>

namespace sitar {

class VowelSweep
{
public:
    // Samples between filter retunes: 1 for LEAD, 16 for the strings.
    explicit VowelSweep(uint32_t update = 16) : fUpdate(update) {}

    // Sample-rate constants, shared by every instance.
    struct Rates
    {
        static constexpr int kTable = 256;
        float envRel  = 0.0f;   // 60 ms note envelope release
        float peakRel = 0.0f;   // 1.5 s peak hold release
        float g[kTable + 1] {}; // TPT coefficient tan(pi fc / fs) over rel = env/peak in 0..1

        void set(float fs)
        {
            envRel  = std::exp(-1.0f / (fs * 0.060f));
            peakRel = std::exp(-1.0f / (fs * 1.5f));
            for (int i = 0; i <= kTable; ++i)
            {
                // 350 Hz * (4000/350)^(1 + dB(rel)/30), 0..-30 dB -> 4 kHz..350 Hz,
                // folded into one power: 4000 * rel^(log10(4000/350) * 2/3).
                float fc = 4000.0f * std::pow(static_cast<float>(i) / kTable, 0.70527f);
                fc = fc < 350.0f ? 350.0f : (fc > 4000.0f ? 4000.0f : fc);
                g[i] = std::tan(3.14159265f * fc / fs);
            }
        }

        float coefficient(float rel) const
        {
            const float pos = rel * kTable;                 // rel in 0..1 (env never exceeds peak)
            int i = static_cast<int>(pos);
            if (i >= kTable) return g[kTable];
            return g[i] + (pos - static_cast<float>(i)) * (g[i + 1] - g[i]);
        }
    };

    void reset()
    {
        fEnv = fPeak = fIc1 = fIc2 = 0.0f;
        fCount = 0;
    }

    float process(const float x, const float depth, const Rates& r)
    {
        const float ax = std::fabs(x);
        fEnv  = ax > fEnv ? ax : fEnv * r.envRel;          // instant attack
        fPeak = fEnv > fPeak ? fEnv : fPeak * r.peakRel;

        if (fCount-- == 0)
        {
            fCount = fUpdate - 1;
            // Simper TPT state-variable band-pass, Q 3.
            const float g = r.coefficient(fPeak > 1e-6f ? fEnv / fPeak : 0.0f);
            fA1 = 1.0f / (1.0f + g * (g + kK));
            fA2 = g * fA1;
            fA3 = g * fA2;
        }

        const float sat = padeTanh(3.0f * x) / 3.0f;        // light buzz
        const float v3 = sat - fIc2;
        const float v1 = fA1 * fIc1 + fA2 * v3;
        const float v2 = fIc2 + fA2 * fIc1 + fA3 * v3;
        fIc1 = 2.0f * v1 - fIc1;
        fIc2 = 2.0f * v2 - fIc2;

        constexpr float kFormant = 2.0f, kBuzz = 0.5f, kTrim = 0.46f;
        return (x + depth * (kFormant * kK * v1 + kBuzz * (sat - x))) / (1.0f + kTrim * depth);
    }

private:
    static constexpr float    kK      = 1.0f / 3.0f;     // 1/Q

    // The music-DSP "fast tanh" t(27+t²)/(27+9t²): not the true [3/2] Padé
    // (t(15+t²)/(15+6t²)) but tuned to reach exactly 1 at t = 3 with zero
    // slope, a smooth soft clip; ~2% off tanh inside ±3, clamped beyond.
    // Only shapes the light buzz, and is what the approved renders used.
    static float padeTanh(const float t)
    {
        if (t >  3.0f) return  1.0f;
        if (t < -3.0f) return -1.0f;
        const float t2 = t * t;
        return t * (27.0f + t2) / (27.0f + 9.0f * t2);
    }

    float fEnv = 0.0f, fPeak = 0.0f, fIc1 = 0.0f, fIc2 = 0.0f;
    float fA1 = 1.0f, fA2 = 0.0f, fA3 = 0.0f;
    uint32_t fUpdate;
    uint32_t fCount = 0;
};

} // namespace sitar

#endif // SITAR_VOWEL_SWEEP_HPP_INCLUDED
