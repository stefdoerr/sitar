/*
 * Regression test: JAWARI, the per-string vowel sweep (VowelSweep.hpp).
 *
 * JAWARI adds the sitar's buzz and vowel sweep, not level (an older
 * wet-sum saturator once stepped the level +2.4 dB off zero, +18 dB at max):
 *
 *   1. Continuity: jawari = 0 vs jawari = 0.001 must produce essentially
 *      the same steady-state wet level (no audible step when engaging).
 *   2. No boost: jawari = 1 must not make a quiet wet signal louder than
 *      jawari = 0. Quieter is expected under a steady drive (~-4 dB): the
 *      envelope sits at its peak, so the formant parks at 4 kHz and adds
 *      little while the level trim still applies. On plucked notes it is
 *      about level-neutral.
 *   3. The sweep: one plucked string's formant (where JAWARI 1 boosts most
 *      over JAWARI 0) starts high at the attack (> 1.2 kHz) and has fallen
 *      below 700 Hz half a second later, following the string's own ring.
 *
 * Build:
 *   g++ -std=gnu++14 -g -O0 -fsanitize=address \
 *       -I../plugins/Sitar -I../dpf/distrho \
 *       test_jawari_gain.cpp ../plugins/Sitar/SitarPlugin.cpp \
 *       ../dpf/distrho/src/DistrhoPlugin.cpp -o test_jawari_gain
 */

#include "src/DistrhoPluginInternal.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

USE_NAMESPACE_DISTRHO

static bool recordRequestCallback(void*, uint32_t, float)
{
    return true;
}

// Resolve a parameter index by symbol so the test is immune to param-index
// reordering.
static uint32_t idx(PluginExporter& p, const char* sym)
{
    for (uint32_t i = 0; i < p.getParameterCount(); ++i)
        if (std::strcmp(p.getParameterSymbol(i), sym) == 0) return i;
    std::printf("FAIL: no param symbol '%s'\n", sym);
    return 0;
}

// Steady-state wet RMS with everything else held constant: drive string 1
// (root C3, 130.81 Hz) with a quiet sine at its resonant frequency, let the
// resonance build up, then measure the last quarter of the run.
static float wetRmsAtJawari(const float jawari)
{
    PluginExporter plugin(nullptr, nullptr, recordRequestCallback, nullptr);

    plugin.setParameterValue(idx(plugin, "mix"),    1.0f);     // all wet
    plugin.setParameterValue(idx(plugin, "jawari"), jawari);   // jawari
    plugin.setParameterValue(idx(plugin, "gate"),   0.0f);     // gate off

    plugin.activate();

    const uint32_t kBlock  = 512;
    const uint32_t kBlocks = 200;          // ~2.1 s at 48 kHz
    const double   freq    = 130.81;
    const double   omega   = 2.0 * M_PI * freq / 48000.0;

    std::vector<float> inBuf(kBlock), outL(kBlock), outR(kBlock);
    const float* inputs[1]  = { inBuf.data() };
    float*       outputs[2] = { outL.data(), outR.data() };

    double sumSq = 0.0;
    uint32_t count = 0;

    for (uint32_t b = 0; b < kBlocks; ++b)
    {
        for (uint32_t i = 0; i < kBlock; ++i)
            inBuf[i] = 0.02f * static_cast<float>(std::sin(omega * (b * kBlock + i)));

        plugin.run(inputs, outputs, kBlock);

        if (b >= (3 * kBlocks) / 4)
        {
            for (uint32_t i = 0; i < kBlock; ++i)
                sumSq += static_cast<double>(outL[i]) * outL[i];
            count += kBlock;
        }
    }

    return static_cast<float>(std::sqrt(sumSq / count));
}

int main()
{
    d_nextBufferSize = 512;
    d_nextSampleRate = 48000.0;
    d_nextCanRequestParameterValueChanges = true;

    int failures = 0;

    const float rmsOff  = wetRmsAtJawari(0.0f);
    const float rmsEps  = wetRmsAtJawari(0.001f);
    const float rmsFull = wetRmsAtJawari(1.0f);

    std::printf("wet RMS: jawari=0 -> %.5f, jawari=0.001 -> %.5f, jawari=1 -> %.5f\n",
                rmsOff, rmsEps, rmsFull);

    const float stepRatio = rmsEps / rmsOff;
    if (stepRatio < 0.98f || stepRatio > 1.02f)
    {
        std::printf("FAIL: level steps by x%.3f when jawari goes 0 -> 0.001\n", stepRatio);
        ++failures;
    }

    const float fullRatio = rmsFull / rmsOff;
    if (fullRatio > 1.2f)
    {
        std::printf("FAIL: jawari=1 boosts a quiet wet signal by x%.2f\n", fullRatio);
        ++failures;
    }
    if (fullRatio < 0.3f)
    {
        std::printf("FAIL: jawari=1 crushes the wet signal to x%.2f\n", fullRatio);
        ++failures;
    }

    if (failures != 0)
        return 1;

    // 3. The sweep, on one C#4 string plucked by the strum.
    {
        auto pluck = [&](float jaw) {
            PluginExporter p(nullptr, nullptr, recordRequestCallback, nullptr);
            p.setParameterValue(idx(p, "mix"),         1.0f);
            p.setParameterValue(idx(p, "stereo_mode"), 0.0f);
            p.setParameterValue(idx(p, "gate"),        0.0f);
            p.setParameterValue(idx(p, "num_active"),  1.0f);
            p.setParameterValue(idx(p, "octave"),      4.0f);
            p.setParameterValue(idx(p, "root_note"),   1.0f);
            p.setParameterValue(idx(p, "decay"),       0.7f);
            p.setParameterValue(idx(p, "strum_level"), 0.5f);
            p.setParameterValue(idx(p, "jawari"),      jaw);
            p.activate();
            p.setParameterValue(idx(p, "strum_up"), 1.0f);
            const uint32_t B = 256;
            std::vector<float> in(B, 0.0f), l(B), r(B), all;
            const float* ins[1] = { in.data() };
            float* outs[2] = { l.data(), r.data() };
            for (uint32_t b = 0; b < 48000 / B; ++b)
            {
                p.run(ins, outs, B);
                all.insert(all.end(), l.begin(), l.end());
            }
            return all;
        };
        const std::vector<float> on = pluck(1.0f), off = pluck(0.0f);
        auto mag = [](const std::vector<float>& x, size_t a, size_t n, double f) {
            double re = 0.0, im = 0.0;
            for (size_t i = 0; i < n; ++i)
            {
                const double w = 0.5 - 0.5 * std::cos(2.0 * M_PI * i / (n - 1));
                re += w * x[a + i] * std::cos(2.0 * M_PI * f * i / 48000.0);
                im -= w * x[a + i] * std::sin(2.0 * M_PI * f * i / 48000.0);
            }
            return std::sqrt(re * re + im * im);
        };
        auto formant = [&](double t) {
            const size_t a = static_cast<size_t>(t * 48000.0), n = 3840;   // 80 ms
            double best = 0.0, bestRatio = 0.0;
            for (double f = 200.0; f < 6000.0; f *= std::pow(2.0, 1.0 / 6.0))
            {
                const double ratio = mag(on, a, n, f) / (mag(off, a, n, f) + 1e-12);
                if (ratio > bestRatio) { bestRatio = ratio; best = f; }
            }
            return best;
        };
        const double early = formant(0.02), later = formant(0.5);
        std::printf("string formant: %.0f Hz at the attack, %.0f Hz after 0.5 s\n", early, later);
        if (early < 1200.0 || later > 700.0)
        {
            std::printf("FAIL: JAWARI does not sweep down over a string's ring\n");
            ++failures;
        }
    }

    if (failures > 0) return 1;
    std::printf("ok: jawari is continuous at 0, adds no level boost, and sweeps each string\n");
    return 0;
}
