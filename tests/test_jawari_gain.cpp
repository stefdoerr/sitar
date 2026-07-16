/*
 * Regression test: the JAWARI stage must not act as a wet-level boost.
 *
 * The old make-up gain (1/tanh(drive)) normalized the saturator at full
 * scale, which for realistic (sub-unity) wet levels meant a gain of
 * drive/tanh(drive): a +2.4 dB step the instant the knob left zero, and
 * up to +18 dB of boost at full drive. JAWARI should add buzz, not level:
 *
 *   1. Continuity: jawari = 0 vs jawari = 0.001 must produce essentially
 *      the same steady-state wet level (no audible step when engaging).
 *   2. No boost: jawari = 1 must not make a quiet wet signal louder than
 *      jawari = 0 (a little quieter is fine — saturation + tilt LPF).
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
#include <vector>

USE_NAMESPACE_DISTRHO

static bool recordRequestCallback(void*, uint32_t, float)
{
    return true;
}

// Steady-state wet RMS with everything else held constant: drive string 1
// (root C3, 130.81 Hz) with a quiet sine at its resonant frequency, let the
// resonance build up, then measure the last quarter of the run.
static float wetRmsAtJawari(const float jawari)
{
    PluginExporter plugin(nullptr, nullptr, recordRequestCallback, nullptr);

    plugin.setParameterValue(51, 1.0f);    // mix: all wet
    plugin.setParameterValue(52, jawari);  // jawari
    plugin.setParameterValue(58, 0.0f);    // gate off

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

    std::printf("ok: jawari is continuous at 0 and adds no level boost\n");
    return 0;
}
