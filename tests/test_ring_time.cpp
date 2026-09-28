/*
 * Regression test: string ring time is independent of pitch.
 *
 *   1. Equal ring: a C3 and a C5 string plucked alone at the same DECAY take
 *      the same time (±25%) to fall 30 dB. A comb loses a fixed fraction per
 *      trip round its loop, so with one shared feedback a string an octave up
 *      makes twice the trips and rings half as long — the low strings then
 *      outlast and drown the high shimmer. Each string gets its own feedback
 *      so every string rings like a C4 one.
 *   2. No Bloom runaway: after the input stops, the output must fall, not
 *      hold or grow. Two ways it broke (both ran away in v0.3.0):
 *        - per-string feedback puts the high strings closer to 1, so the
 *          bound must follow the most resonant string (48 chromatic, OCT 4);
 *        - just intonation stacks exact shared partials: in Major at OCT 2,
 *          five of seven strings resonate at 981 Hz, past the bound's
 *          ~3-strings assumption. The per-string detune splits them.
 *
 * Build:
 *   g++ -std=gnu++14 -g -O0 -fsanitize=address \
 *       -I../plugins/Sitar -I../dpf/distrho \
 *       test_ring_time.cpp ../plugins/Sitar/SitarPlugin.cpp \
 *       ../dpf/distrho/src/DistrhoPlugin.cpp -o test_ring_time
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

static uint32_t idx(PluginExporter& p, const char* sym)
{
    for (uint32_t i = 0; i < p.getParameterCount(); ++i)
        if (std::strcmp(p.getParameterSymbol(i), sym) == 0) return i;
    std::printf("FAIL: no param symbol '%s'\n", sym);
    return 0;
}

static const uint32_t kBlock = 256;

// Run `blocks` blocks of `input(sampleIndex)`, returning the left output.
template <typename F>
static std::vector<float> run(PluginExporter& p, uint32_t blocks, F input, uint32_t& t)
{
    std::vector<float> in(kBlock), outL(kBlock), outR(kBlock), all;
    const float* inputs[1]  = { in.data() };
    float*       outputs[2] = { outL.data(), outR.data() };
    for (uint32_t b = 0; b < blocks; ++b)
    {
        for (uint32_t i = 0; i < kBlock; ++i) in[i] = input(t++);
        p.run(inputs, outputs, kBlock);
        all.insert(all.end(), outL.begin(), outL.end());
    }
    return all;
}

static float rmsDb(const std::vector<float>& x, size_t from, size_t len)
{
    double e = 0.0;
    for (size_t i = from; i < from + len && i < x.size(); ++i) e += static_cast<double>(x[i]) * x[i];
    return static_cast<float>(10.0 * std::log10(e / len + 1e-30));
}

// Seconds for one string, plucked by the strum, to fall 30 dB below its
// loudest 20 ms window.
static float ringSeconds(float octave)
{
    PluginExporter p(nullptr, nullptr, recordRequestCallback, nullptr);
    p.setParameterValue(idx(p, "mix"),         1.0f);
    p.setParameterValue(idx(p, "stereo_mode"), 0.0f);
    p.setParameterValue(idx(p, "jawari"),      0.0f);
    p.setParameterValue(idx(p, "bloom"),       0.0f);
    p.setParameterValue(idx(p, "gate"),        0.0f);
    p.setParameterValue(idx(p, "num_active"),  1.0f);
    p.setParameterValue(idx(p, "octave"),      octave);
    p.setParameterValue(idx(p, "decay"),       0.7f);
    p.setParameterValue(idx(p, "strum_level"), 0.1f);   // keep the tanh stages linear
    p.activate();
    p.setParameterValue(idx(p, "strum_up"), 1.0f);

    uint32_t t = 0;
    const std::vector<float> o = run(p, 12 * 48000 / kBlock, [](uint32_t) { return 0.0f; }, t);
    const size_t win = 960;   // 20 ms
    float peak = -300.0f;
    for (size_t i = 0; i + win < o.size(); i += win) peak = std::fmax(peak, rmsDb(o, i, win));
    size_t last = 0;
    for (size_t i = 0; i + win < o.size(); i += win)
        if (rmsDb(o, i, win) > peak - 30.0f) last = i;
    return static_cast<float>(last) / 48000.0f;
}

int main()
{
    d_nextBufferSize = kBlock;
    d_nextSampleRate = 48000.0;
    d_nextCanRequestParameterValueChanges = true;

    int failures = 0;

    // 1. Equal ring time.
    const float low  = ringSeconds(3.0f);   // C3
    const float high = ringSeconds(5.0f);   // C5
    std::printf("30 dB ring time at DECAY 0.7: C3 %.2f s, C5 %.2f s (ratio %.2f)\n",
                low, high, low / high);
    if (low / high < 0.8f || low / high > 1.25f)
    {
        std::printf("FAIL: string ring time depends on pitch\n");
        ++failures;
    }

    // 2. No Bloom runaway.
    struct Case { const char* name; float scale, strings, octave, decay; };
    const Case cases[] = {
        { "48 chromatic, OCT 4, DECAY 1",  13.0f, 48.0f, 4.0f, 1.0f },
        { "7 Major, OCT 2, DECAY 0.6",      0.0f,  7.0f, 2.0f, 0.6f },
        { "48 Major, OCT 2, DECAY 0.5",     0.0f, 48.0f, 2.0f, 0.5f },
        { "48 chromatic, OCT 6, DECAY 0.5", 13.0f, 48.0f, 6.0f, 0.5f },
    };
    for (const Case& c : cases)
    {
        PluginExporter p(nullptr, nullptr, recordRequestCallback, nullptr);
        p.setParameterValue(idx(p, "scale"),      c.scale);
        p.setParameterValue(idx(p, "num_active"), c.strings);
        p.setParameterValue(idx(p, "octave"),     c.octave);
        p.setParameterValue(idx(p, "decay"),      c.decay);
        p.setParameterValue(idx(p, "bloom"),      1.0f);
        p.setParameterValue(idx(p, "gate"),       0.0f);
        p.setParameterValue(idx(p, "mix"),        1.0f);
        p.activate();

        uint32_t t = 0, seed = 1u;
        auto noise = [&](uint32_t) {
            seed = seed * 1664525u + 1013904223u;
            return 0.3f * static_cast<float>(static_cast<int32_t>(seed)) / 2147483648.0f;
        };
        run(p, 48000 / kBlock, noise, t);                                // 1 s of input
        const std::vector<float> tail = run(p, 3 * 48000 / kBlock, [](uint32_t) { return 0.0f; }, t);
        bool finite = true;
        for (float v : tail) if (!std::isfinite(v)) finite = false;
        const float early = rmsDb(tail, 0,          24000);             // 0.0-0.5 s after
        const float late  = rmsDb(tail, 2 * 48000,  24000);             // 2.0-2.5 s after
        std::printf("bloom max, %s: tail %.1f dB -> %.1f dB two seconds later\n", c.name, early, late);
        if (!finite || late > early - 3.0f)
        {
            std::printf("FAIL: strings sustain or grow on their own with BLOOM up — runaway\n");
            ++failures;
        }
    }

    if (failures == 0)
        std::printf("PASS\n");
    return failures == 0 ? 0 : 1;
}
