/*
 * Regression test: LEAD jawari, the vowel sweep on the dry sound.
 *
 *   1. Off is off: at LEAD 0 the dry path is bit-identical to the input, so
 *      existing pedalboards sound exactly as before.
 *   2. Level-neutral on plucked-string notes (a Karplus-Strong phrase, like
 *      a guitar): LEAD 1 keeps the RMS within ±1.5 dB of LEAD 0. How much the
 *      formant adds depends on the input spectrum (white noise: -1.9 dB).
 *   3. The sweep: over a note decaying 30 dB in a second, the frequency LEAD
 *      boosts most (LEAD 1 over LEAD 0, per window) starts high (an "ee",
 *      > 2.5 kHz) and ends low (an "oo", < 1.2 kHz).
 *
 * Build:
 *   g++ -std=gnu++14 -g -O0 -fsanitize=address \
 *       -I../plugins/Sitar -I../dpf/distrho \
 *       test_lead_jawari.cpp ../plugins/Sitar/SitarPlugin.cpp \
 *       ../dpf/distrho/src/DistrhoPlugin.cpp -o test_lead_jawari
 */

#include "src/DistrhoPluginInternal.hpp"

#include <cmath>
#include <complex>
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

// Dry only (MIX 0), so the output is exactly the (LEAD-processed) input.
static std::vector<float> process(const std::vector<float>& x, float lead)
{
    PluginExporter p(nullptr, nullptr, recordRequestCallback, nullptr);
    p.setParameterValue(idx(p, "mix"),         0.0f);
    p.setParameterValue(idx(p, "lead_jawari"), lead);
    p.activate();
    const uint32_t B = 256;
    std::vector<float> in(B), outL(B), outR(B), all;
    const float* inputs[1]  = { in.data() };
    float*       outputs[2] = { outL.data(), outR.data() };
    for (size_t b = 0; b + B <= x.size(); b += B)
    {
        std::copy(x.begin() + b, x.begin() + b + B, in.begin());
        p.run(inputs, outputs, B);
        all.insert(all.end(), outL.begin(), outL.end());
    }
    return all;
}

static double rms(const std::vector<float>& x)
{
    double e = 0.0;
    for (float v : x) e += static_cast<double>(v) * v;
    return std::sqrt(e / x.size());
}

// Magnitude of a Hann-windowed DFT bin at frequency f over [from, from+n).
static double mag(const std::vector<float>& x, size_t from, size_t n, double f)
{
    std::complex<double> acc = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos(2.0 * M_PI * i / (n - 1));
        acc += w * x[from + i] * std::polar(1.0, -2.0 * M_PI * f * i / 48000.0);
    }
    return std::abs(acc);
}

// Frequency (200 Hz..6 kHz, 1/6-octave steps) where LEAD adds the most.
static double formant(const std::vector<float>& on, const std::vector<float>& off, double t)
{
    const size_t from = static_cast<size_t>(t * 48000.0), n = 3840;   // 80 ms
    double best = 0.0, bestRatio = 0.0;
    for (double f = 200.0; f < 6000.0; f *= std::pow(2.0, 1.0 / 6.0))
    {
        const double r = mag(on, from, n, f) / (mag(off, from, n, f) + 1e-12);
        if (r > bestRatio) { bestRatio = r; best = f; }
    }
    return best;
}

int main()
{
    d_nextBufferSize = 256;
    d_nextSampleRate = 48000.0;
    d_nextCanRequestParameterValueChanges = true;

    int failures = 0;

    // A "note": white noise decaying 30 dB over one second, then silence.
    std::vector<float> note(static_cast<size_t>(1.5 * 48000), 0.0f);
    uint32_t seed = 3u;
    for (size_t i = 0; i < 48000; ++i)
    {
        seed = seed * 1664525u + 1013904223u;
        const float n = static_cast<float>(static_cast<int32_t>(seed)) / 2147483648.0f;
        note[i] = 0.3f * n * std::pow(10.0f, -1.5f * static_cast<float>(i) / 48000.0f);
    }

    const std::vector<float> off = process(note, 0.0f);
    const std::vector<float> on  = process(note, 1.0f);

    // 1. Off is off.
    size_t diffs = 0;
    for (size_t i = 0; i < off.size(); ++i)
        if (off[i] != note[i]) ++diffs;
    std::printf("LEAD 0: %zu samples differ from the input\n", diffs);
    if (diffs != 0)
    {
        std::printf("FAIL: LEAD 0 changes the dry sound\n");
        ++failures;
    }

    // 2. Level-neutral, on a plucked phrase.
    std::vector<float> phrase(static_cast<size_t>(4 * 48000), 0.0f);
    const float hz[] = { 130.81f, 196.0f, 261.63f, 329.63f, 392.0f };
    for (int k = 0; k < 5; ++k)
    {
        const int period = static_cast<int>(48000.0f / hz[k] + 0.5f);
        std::vector<float> buf(period);
        for (float& v : buf) { seed = seed * 1664525u + 1013904223u; v = static_cast<float>(static_cast<int32_t>(seed)) / 2147483648.0f; }
        float prev = 0.0f;
        for (size_t i = 0, t0 = static_cast<size_t>(k * 0.6 * 48000); t0 + i < phrase.size(); ++i)
        {
            float& v = buf[i % period];
            const float o = v;
            v = 0.996f * 0.5f * (v + prev);
            prev = o;
            phrase[t0 + i] += 0.25f * o;
        }
    }
    const double dB = 20.0 * std::log10(rms(process(phrase, 1.0f)) / rms(process(phrase, 0.0f)));
    std::printf("LEAD 1 level vs LEAD 0: %+.2f dB\n", dB);
    if (std::fabs(dB) > 1.5)
    {
        std::printf("FAIL: LEAD 1 is not level-neutral\n");
        ++failures;
    }

    // 3. The sweep.
    const double early = formant(on, off, 0.02);
    const double late  = formant(on, off, 0.75);
    std::printf("formant: %.0f Hz early in the note, %.0f Hz late\n", early, late);
    if (early < 2500.0 || late > 1200.0)
    {
        std::printf("FAIL: no downward vowel sweep\n");
        ++failures;
    }

    if (failures == 0)
        std::printf("PASS\n");
    return failures == 0 ? 0 : 1;
}
