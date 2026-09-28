/*
 * Regression test: the STRUM triggers.
 *
 *   1. Even strum: every string's pluck carries the same energy whatever its
 *      pitch. A raw noise period's harmonics land at random (~4 dB spread
 *      across strings), so each pluck is normalised to unit energy. The one
 *      deviation left is the comb's linear fractional-delay interpolation,
 *      which averages adjacent samples and takes 0..3 dB off white noise
 *      depending on each string's fractional delay (almost all of it above
 *      5 kHz). It shapes every excitation alike, your guitar too, so it is
 *      not the strum's to undo: the spread over C2..B6 must stay within it.
 *   2. Direction: an up strum reaches the lowest string first, a down strum
 *      the highest (seen as the delay before the first output sample, which
 *      is one period of the string that fired first).
 *   3. STRENGTH scales the pluck, and nothing sounds without a trigger.
 *   4. No thump: the pluck must be zero-mean. DC rings every comb's 0 Hz
 *      mode — the slowest-decaying one, since the in-loop damping LPF passes
 *      DC — and the strum sounds like a sub-bass thud.
 *   5. Fresh noise: two strums in a row are not sample-identical, so a
 *      repeated strum sounds played, not looped.
 *   6. Ease-in: the first pluck of a strum is half strength (-6 dB) and the
 *      last is full, either direction — so an up strum doesn't open on its
 *      loudest string at full force out of silence (a kick).
 *
 * Tests 1, 3 and 6 run at DECAY 0 (no feedback): the output is then exactly the
 * pluck, once, so its energy is the pluck's energy.
 *
 * Build:
 *   g++ -std=gnu++14 -g -O0 -fsanitize=address \
 *       -I../plugins/Sitar -I../dpf/distrho \
 *       test_strum.cpp ../plugins/Sitar/SitarPlugin.cpp \
 *       ../dpf/distrho/src/DistrhoPlugin.cpp -o test_strum
 */

#include "src/DistrhoPluginInternal.hpp"

#include <algorithm>
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

struct Setup
{
    float    strength = 0.5f;
    uint32_t strings  = 13;
    float    octave   = 3.0f;
    float    root     = 0.0f;
    float    decay    = 0.5f;
};

// Silent input, all wet, mono, no bloom/jawari/gate: whatever comes out is
// the strum.
static void configure(PluginExporter& p, const Setup& s)
{
    p.setParameterValue(idx(p, "mix"),         1.0f);
    p.setParameterValue(idx(p, "stereo_mode"), 0.0f);
    p.setParameterValue(idx(p, "jawari"),      0.0f);
    p.setParameterValue(idx(p, "bloom"),       0.0f);
    p.setParameterValue(idx(p, "gate"),        0.0f);
    p.setParameterValue(idx(p, "num_active"),  static_cast<float>(s.strings));
    p.setParameterValue(idx(p, "octave"),      s.octave);
    p.setParameterValue(idx(p, "root_note"),   s.root);
    p.setParameterValue(idx(p, "decay"),       s.decay);
    p.setParameterValue(idx(p, "strum_level"), s.strength);
    p.setParameterValue(idx(p, "strum_time"),  500.0f);
    p.activate();
}

static std::vector<float> runFor(PluginExporter& p, float seconds)
{
    const uint32_t kBlock = 256;
    std::vector<float> in(kBlock, 0.0f), outL(kBlock), outR(kBlock), all;
    const float* inputs[1]  = { in.data() };
    float*       outputs[2] = { outL.data(), outR.data() };
    const uint32_t blocks = static_cast<uint32_t>(seconds * 48000.0f / kBlock);
    for (uint32_t b = 0; b < blocks; ++b)
    {
        p.run(inputs, outputs, kBlock);
        all.insert(all.end(), outL.begin(), outL.end());
    }
    return all;
}

static std::vector<float> strum(const char* trigger, const Setup& s, float seconds = 0.1f)
{
    PluginExporter p(nullptr, nullptr, recordRequestCallback, nullptr);
    configure(p, s);
    if (trigger != nullptr)
        p.setParameterValue(idx(p, trigger), 1.0f);
    return runFor(p, seconds);
}

static float energy(const std::vector<float>& x)
{
    double e = 0.0;
    for (float v : x) e += static_cast<double>(v) * v;
    return static_cast<float>(e);
}

static float peak(const std::vector<float>& x)
{
    float m = 0.0f;
    for (float v : x) m = std::max(m, std::fabs(v));
    return m;
}

static int firstSample(const std::vector<float>& x)
{
    for (size_t i = 0; i < x.size(); ++i)
        if (std::fabs(x[i]) > 1e-6f) return static_cast<int>(i);
    return -1;
}

// Sub-bass share: four cascaded 20 Hz one-poles (-24 dB/oct, so even a C2
// fundamental leaks in below -40 dB) vs the whole signal.
static float subDb(const std::vector<float>& x)
{
    const float a = 1.0f - std::exp(-2.0f * 3.14159265f * 20.0f / 48000.0f);
    float lpf[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    double sub = 0.0;
    for (float v : x)
    {
        for (float& st : lpf) { st += a * (v - st); v = st; }
        sub += static_cast<double>(v) * v;
    }
    return static_cast<float>(10.0 * std::log10(sub / energy(x) + 1e-20));
}

int main()
{
    d_nextBufferSize = 256;
    d_nextSampleRate = 48000.0;
    d_nextCanRequestParameterValueChanges = true;

    int failures = 0;

    // 1. One string at a time across C2..B6: equal pluck energy. Low strength
    //    keeps the wet tanh near-linear so it can't mask a level spread.
    std::vector<float> levelsDb;
    for (int oct = 2; oct <= 6; ++oct)
        for (int root = 0; root < 12; ++root)
        {
            Setup s;
            s.strength = 0.1f; s.strings = 1; s.decay = 0.0f;
            s.octave = static_cast<float>(oct); s.root = static_cast<float>(root);
            // Energy per second of pluck: a pluck is one period long.
            const float hz = 65.41f * std::pow(2.0f, (oct - 2) + root / 12.0f);
            levelsDb.push_back(10.0f * std::log10(energy(strum("strum_up", s)) * hz));
        }
    const auto range = std::minmax_element(levelsDb.begin(), levelsDb.end());
    std::printf("pluck level over %zu pitches: %.2f .. %.2f dB\n",
                levelsDb.size(), *range.first, *range.second);
    if (*range.second - *range.first > 3.5f)
    {
        std::printf("FAIL: strum pluck level spreads beyond the comb's 3 dB interpolation loss\n");
        ++failures;
    }

    // 2. Direction. 13 strings of C major from C3: lowest C3 (period ≈ 367
    //    samples), highest A4 (≈ 109 samples).
    const int up   = firstSample(strum("strum_up",   Setup()));
    const int down = firstSample(strum("strum_down", Setup()));
    std::printf("first output sample: up %d, down %d\n", up, down);
    if (up < 300)
    {
        std::printf("FAIL: up strum did not start on the lowest string\n");
        ++failures;
    }
    if (down < 0 || down > 150)
    {
        std::printf("FAIL: down strum did not start on the highest string\n");
        ++failures;
    }

    // 3. STRENGTH scales the pluck (double strength = 4x energy); silence
    //    without a trigger.
    {
        Setup s;
        s.strings = 1; s.decay = 0.0f;
        s.strength = 0.05f; const float soft = energy(strum("strum_up", s));
        s.strength = 0.10f; const float hard = energy(strum("strum_up", s));
        std::printf("energy ratio for 2x STRENGTH: %.2f\n", hard / soft);
        if (hard / soft < 3.6f || hard / soft > 4.4f)
        {
            std::printf("FAIL: doubling STRENGTH did not double the pluck\n");
            ++failures;
        }
    }
    const float idle = peak(strum(nullptr, Setup()));
    if (idle > 1e-6f)
    {
        std::printf("FAIL: output %.6f with no strum triggered\n", idle);
        ++failures;
    }

    // 4. No thump, one string at C3 and at C5, over 2 s of ring.
    for (float oct : { 3.0f, 5.0f })
    {
        Setup s;
        s.strings = 1; s.octave = oct;
        const float sub = subDb(strum("strum_up", s, 2.0f));
        std::printf("sub-20 Hz energy at octave %.0f: %.1f dB\n", oct, sub);
        // The rejected thumpy plucks measured -5..-13 dB; this is also a
        // leak-prone measure (onset transient, and equal ring time lets a
        // C5's slow 0 Hz comb mode ring longer), so it sits at -20 dB.
        if (sub > -20.0f)
        {
            std::printf("FAIL: strum pluck carries DC — the strings thump\n");
            ++failures;
        }
    }

    // 5. Fresh noise: strum the same string twice in one instance.
    {
        Setup s;
        s.strings = 1; s.decay = 0.0f;
        PluginExporter p(nullptr, nullptr, recordRequestCallback, nullptr);
        configure(p, s);
        p.setParameterValue(idx(p, "strum_up"), 1.0f);
        const std::vector<float> first = runFor(p, 0.05f);
        p.setParameterValue(idx(p, "strum_up"), 1.0f);
        const std::vector<float> second = runFor(p, 0.05f);
        double diff = 0.0;
        for (size_t i = 0; i < first.size(); ++i)
            diff += std::fabs(first[i] - second[i]);
        std::printf("repeat strum difference: %.3f\n", diff);
        if (diff < 1e-3)
        {
            std::printf("FAIL: two strums are identical — the noise is not fresh\n");
            ++failures;
        }
    }

    // 6. Ease-in. 13 strings, 500 ms sweep: plucks ~42 ms apart, and at
    //    DECAY 0 each is heard once, alone. Compare pluck power (energy per
    //    period) of the first and last string of each strum.
    for (const char* dir : { "strum_up", "strum_down" })
    {
        Setup s;
        s.decay = 0.0f;
        const std::vector<float> o = strum(dir, s, 0.6f);
        const bool up = std::strcmp(dir, "strum_up") == 0;
        const float lowHz = 130.81f, highHz = 440.0f;   // C3 .. A4
        const size_t step = static_cast<size_t>(0.5f * 48000.0f / 12.0f);
        auto power = [&](size_t start, float hz) {
            const std::vector<float> w(o.begin() + start, o.begin() + start + step - 1);
            return 10.0f * std::log10(energy(w) * hz);
        };
        const float first = power(0,         up ? lowHz : highHz);
        const float last  = power(12 * step, up ? highHz : lowHz);
        std::printf("%s: first pluck %.1f dB vs last\n", dir, first - last);
        if (first - last < -8.0f || first - last > -4.0f)
        {
            std::printf("FAIL: %s does not ease in at half strength\n", dir);
            ++failures;
        }
    }

    if (failures == 0)
        std::printf("PASS\n");
    return failures == 0 ? 0 : 1;
}
