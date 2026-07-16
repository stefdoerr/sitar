/*
 * Regression test: writing 0 to a string port must clear its comb filter.
 *
 * run() skips inactive combs entirely, so a string silenced by a port
 * write of 0 freezes with its ring energy still in the delay buffer.
 * Without a clear on the active→inactive transition, re-enabling the
 * string later resumes that stale ring at the new tuning — a ghost note
 * out of nowhere. applyScaleAndRoot() already clears on this transition;
 * the direct port-write path must do the same. A plain retune while the
 * string stays active must NOT clear (sweeping a fine-tune knob should
 * not kill the ring).
 *
 * Build:
 *   g++ -std=gnu++14 -g -O0 -fsanitize=address \
 *       -I../plugins/Sitar -I../dpf/distrho \
 *       test_string_zero_clear.cpp ../plugins/Sitar/SitarPlugin.cpp \
 *       ../dpf/distrho/src/DistrhoPlugin.cpp -o test_string_zero_clear
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

// Feed `blocks` blocks of a sine (amp = 0 for silence) and return the output
// RMS over the whole run. sampleClock keeps the phase continuous across calls.
static float feed(PluginExporter& plugin, const float amp, const uint32_t blocks,
                  uint64_t& sampleClock)
{
    const uint32_t kBlock = 512;
    const double   omega  = 2.0 * M_PI * 130.81 / 48000.0;

    std::vector<float> inBuf(kBlock), outL(kBlock), outR(kBlock);
    const float* inputs[1]  = { inBuf.data() };
    float*       outputs[2] = { outL.data(), outR.data() };

    double sumSq = 0.0;
    for (uint32_t b = 0; b < blocks; ++b)
    {
        for (uint32_t i = 0; i < kBlock; ++i)
            inBuf[i] = amp * static_cast<float>(std::sin(omega * static_cast<double>(sampleClock++)));

        plugin.run(inputs, outputs, kBlock);

        for (uint32_t i = 0; i < kBlock; ++i)
            sumSq += static_cast<double>(outL[i]) * outL[i];
    }
    return static_cast<float>(std::sqrt(sumSq / (blocks * kBlock)));
}

int main()
{
    d_nextBufferSize = 512;
    d_nextSampleRate = 48000.0;
    d_nextCanRequestParameterValueChanges = true;

    PluginExporter plugin(nullptr, nullptr, recordRequestCallback, nullptr);

    plugin.setParameterValue(48, 1.0f);   // num_active: only string 1
    plugin.setParameterValue(51, 1.0f);   // mix: all wet
    plugin.setParameterValue(58, 0.0f);   // gate off

    plugin.activate();

    uint64_t clock = 0;
    int failures = 0;

    // Build up resonance on string 1 (tuned to the root, 130.81 Hz).
    const float ringing = feed(plugin, 0.02f, 100, clock);
    if (ringing < 0.03f)
    {
        std::printf("FAIL: string 1 not resonating during drive (rms %.5f)\n", ringing);
        ++failures;
    }

    // Retune while ringing (active -> active): the ring must survive.
    plugin.setParameterValue(0, 138.59f);
    const float retuned = feed(plugin, 0.0f, 5, clock);
    if (retuned < 1e-3f)
    {
        std::printf("FAIL: plain retune killed the ring (rms %.5f)\n", retuned);
        ++failures;
    }

    // Silence the string mid-ring, wait, then re-enable it. No input is
    // playing — the re-enabled string must come back silent, not resume
    // the ring that was frozen in its buffer.
    plugin.setParameterValue(0, 0.0f);
    feed(plugin, 0.0f, 20, clock);
    plugin.setParameterValue(0, 130.81f);
    const float ghost = feed(plugin, 0.0f, 10, clock);
    if (ghost > 1e-4f)
    {
        std::printf("FAIL: re-enabled string plays a stale ghost ring (rms %.5f)\n", ghost);
        ++failures;
    }

    // And it must still be a functional string afterwards.
    const float revived = feed(plugin, 0.02f, 100, clock);
    if (revived < 0.03f)
    {
        std::printf("FAIL: re-enabled string no longer resonates (rms %.5f)\n", revived);
        ++failures;
    }

    if (failures != 0)
        return 1;

    std::printf("ok: zeroing a string clears its comb; retunes keep the ring\n");
    return 0;
}
