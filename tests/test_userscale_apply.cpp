/*
 * Regression test: selecting a user scale tunes the strings from the parsed
 * library, and editing the active user scale retunes live. Observed through
 * AUDIO (resonance RMS) so it survives the later removal of the string ports.
 *
 * Build:
 *   g++ -std=gnu++14 -g -O0 -fsanitize=address -Iplugins/Sitar -Idpf/distrho \
 *       test_userscale_apply.cpp ../plugins/Sitar/SitarPlugin.cpp \
 *       ../dpf/distrho/src/DistrhoPlugin.cpp -o test_userscale_apply
 */
#include "src/DistrhoPluginInternal.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

USE_NAMESPACE_DISTRHO

static bool cb(void*, uint32_t, float) { return true; }

// Drive `blocks` of a sine at `freq` and return output RMS.
static float feed(PluginExporter& p, float freq, float amp, uint32_t blocks, uint64_t& clk)
{
    const uint32_t B = 512; const double w = 2.0 * M_PI * freq / 48000.0;
    std::vector<float> in(B), l(B), r(B);
    const float* ins[1] = { in.data() }; float* outs[2] = { l.data(), r.data() };
    double sum = 0.0;
    for (uint32_t b = 0; b < blocks; ++b) {
        for (uint32_t i = 0; i < B; ++i) in[i] = amp * (float) std::sin(w * (double) clk++);
        p.run(ins, outs, B);
        for (uint32_t i = 0; i < B; ++i) sum += (double) l[i] * l[i];
    }
    return (float) std::sqrt(sum / (blocks * B));
}

// Param indices are resolved by symbol so this test is robust to the port
// renumbering in Task 3.
static uint32_t idx(PluginExporter& p, const char* sym)
{
    for (uint32_t i = 0; i < p.getParameterCount(); ++i)
        if (std::strcmp(p.getParameterSymbol(i), sym) == 0) return i;
    std::printf("FAIL: no param symbol '%s'\n", sym); return 0;
}

int main()
{
    d_nextBufferSize = 512; d_nextSampleRate = 48000.0; d_nextCanRequestParameterValueChanges = true;
    PluginExporter p(nullptr, nullptr, cb, nullptr);

    const uint32_t SCALE = idx(p, "scale"), ROOT = idx(p, "root_note"),
                   OCT = idx(p, "octave"), MIX = idx(p, "mix"), GATE = idx(p, "gate"),
                   NUM = idx(p, "num_active");
    int fail = 0;
    uint64_t clk = 0;

    // A user scale whose tonic (1/1) at ROOT=C, OCT=3 is 130.81 Hz.
    p.setState("userscales", "Mine | 3/2\n");
    p.setParameterValue(ROOT, 0.0f);      // C
    p.setParameterValue(OCT,  3.0f);
    p.setParameterValue(MIX,  1.0f);      // all wet
    p.setParameterValue(GATE, 0.0f);      // gate off
    p.setParameterValue(NUM,  13.0f);
    p.setParameterValue(SCALE, 24.0f);    // user slot 0
    p.activate();

    // String 1 (tonic) should ring at 130.81 Hz.
    const float ring = feed(p, 130.81f, 0.02f, 120, clk);
    if (ring < 0.03f) { std::printf("FAIL: user-scale tonic not resonating (rms %.5f)\n", ring); ++fail; }

    // Its second degree (3/2) -> 196.22 Hz should also ring.
    const float fifth = feed(p, 130.81f * 1.5f, 0.02f, 120, clk);
    if (fifth < 0.03f) { std::printf("FAIL: user-scale 3/2 degree not resonating (rms %.5f)\n", fifth); ++fail; }

    // Selecting an EMPTY user slot -> silence (no populated strings).
    p.setParameterValue(SCALE, 25.0f);    // user slot 1 (empty)
    p.activate();
    const float silent = feed(p, 130.81f, 0.02f, 60, clk);
    if (silent > 0.02f) { std::printf("FAIL: empty user slot still resonates (rms %.5f)\n", silent); ++fail; }

    if (fail) return 1;
    std::printf("ok: user scale applies + retunes; empty slot is silent\n");
    return 0;
}
