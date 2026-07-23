/*
 * Regression test: disabling the gate while it is closed must ramp the
 * comb feed open through the 8 ms smoother, not snap it in one sample.
 *
 * The gate state fGateOpen normally glides through a one-pole smoother to
 * avoid clicks. The gate-off branch assigned 1.0 directly, so zeroing the
 * GATE knob while the gate was shut stepped the signal feeding all combs
 * from 0 to full instantly — an audible thump.
 *
 * Setup makes the gate envelope directly observable: DECAY = 0 turns the
 * single active comb into a pure delay (feedback 0), so the wet output is
 * just a delayed copy of (input × gate gain). With input held below the
 * gate threshold the gate closes; after switching the gate off, the output
 * amplitude right after the delay tells us whether the gain stepped
 * (buggy: instantly at steady level) or ramped (fixed: well below it).
 *
 * Build:
 *   g++ -std=gnu++14 -g -O0 -fsanitize=address \
 *       -I../plugins/Sitar -I../dpf/distrho \
 *       test_gate_release_smooth.cpp ../plugins/Sitar/SitarPlugin.cpp \
 *       ../dpf/distrho/src/DistrhoPlugin.cpp -o test_gate_release_smooth
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

int main()
{
    d_nextBufferSize = 512;
    d_nextSampleRate = 48000.0;
    d_nextCanRequestParameterValueChanges = true;

    PluginExporter plugin(nullptr, nullptr, recordRequestCallback, nullptr);

    plugin.setParameterValue(0, 1.0f);     // num_active: only string 1
    plugin.setParameterValue(2, 0.0f);     // decay: comb is a pure delay
    plugin.setParameterValue(3, 1.0f);     // mix: all wet
    plugin.setParameterValue(10, 10.0f);   // gate threshold 0.02 linear

    plugin.activate();

    const uint32_t kBlock = 512;
    const double   omega  = 2.0 * M_PI * 130.81 / 48000.0;
    const float    amp    = 0.015f;        // below the 0.02 gate threshold

    std::vector<float> inBuf(kBlock), outL(kBlock), outR(kBlock);
    const float* inputs[1]  = { inBuf.data() };
    float*       outputs[2] = { outL.data(), outR.data() };

    uint64_t clock = 0;
    int failures = 0;

    // Let the gate settle shut on the sub-threshold sine.
    double sumSq = 0.0;
    for (uint32_t b = 0; b < 100; ++b)
    {
        for (uint32_t i = 0; i < kBlock; ++i)
            inBuf[i] = amp * static_cast<float>(std::sin(omega * static_cast<double>(clock++)));
        plugin.run(inputs, outputs, kBlock);
        if (b >= 50)
            for (uint32_t i = 0; i < kBlock; ++i)
                sumSq += static_cast<double>(outL[i]) * outL[i];
    }
    const float closedRms = static_cast<float>(std::sqrt(sumSq / (50 * kBlock)));
    if (closedRms > 1e-3f)
    {
        std::printf("FAIL: gate not closed during setup (rms %.5f)\n", closedRms);
        ++failures;
    }

    // Switch the gate off and capture the output that follows.
    plugin.setParameterValue(10, 0.0f);    // gate

    std::vector<float> captured;
    for (uint32_t b = 0; b < 8; ++b)
    {
        for (uint32_t i = 0; i < kBlock; ++i)
            inBuf[i] = amp * static_cast<float>(std::sin(omega * static_cast<double>(clock++)));
        plugin.run(inputs, outputs, kBlock);
        captured.insert(captured.end(), outL.begin(), outL.end());
    }

    // The comb delay is ~367 samples (48000 / 130.81). One full sine period
    // right after the delay reflects the gate gain over the first ~7.6 ms
    // after the switch; a window ~62 ms in reflects the settled gain.
    auto rmsWindow = [&captured](uint32_t start, uint32_t len)
    {
        double s = 0.0;
        for (uint32_t i = start; i < start + len; ++i)
            s += static_cast<double>(captured[i]) * captured[i];
        return static_cast<float>(std::sqrt(s / len));
    };

    const float earlyRms  = rmsWindow(370, 367);
    const float steadyRms = rmsWindow(3000, 367);

    if (steadyRms < 5e-3f)
    {
        std::printf("FAIL: gate did not open after being disabled (steady rms %.5f)\n", steadyRms);
        ++failures;
    }

    const float ratio = earlyRms / steadyRms;
    std::printf("gate release: early rms %.5f, steady rms %.5f, ratio %.3f\n",
                earlyRms, steadyRms, ratio);
    if (ratio > 0.6f)
    {
        std::printf("FAIL: comb feed stepped open instead of ramping (ratio %.3f)\n", ratio);
        ++failures;
    }

    if (failures != 0)
        return 1;

    std::printf("ok: disabling the gate ramps the comb feed open smoothly\n");
    return 0;
}
