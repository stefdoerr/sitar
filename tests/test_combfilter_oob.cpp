/*
 * Regression test: CombFilter::process() must never read past the end of
 * its delay buffer.
 *
 * The fractional read position is computed as float(writePos) - delaySamples
 * and wrapped into [0, bufferSize) by adding bufferSize while negative. When
 * delaySamples has a tiny fractional part (smaller than half an ulp at
 * bufferSize magnitude, ~6e-5 for a 1750-sample buffer), the wrap of a tiny
 * negative value rounds to exactly float(bufferSize), so idx0 == bufferSize
 * reads one float past the end of the allocation.
 *
 * This test scans for tunings whose float delay reproduces that rounding,
 * then runs the filter through a full buffer cycle for each. Build with
 * AddressSanitizer so the out-of-bounds read aborts the run:
 *
 *   g++ -std=c++11 -g -O0 -fsanitize=address -I../plugins/Sitar \
 *       test_combfilter_oob.cpp -o test_combfilter_oob && ./test_combfilter_oob
 */

#include "CombFilter.hpp"

#include <cmath>
#include <cstdio>

int main()
{
    const double   sampleRate = 48000.0;
    // Mirrors SitarPlugin::sampleRateChanged: ceil(fs / 27.5) = 1746,
    // + 4 samples of interpolation headroom inside CombFilter = 1750.
    const uint32_t maxDelay   = static_cast<uint32_t>(std::ceil(sampleRate / 27.5));
    const float    bufferSize = static_cast<float>(maxDelay + 4);

    int candidates = 0;

    // Walk the float grid over a plausible string-frequency range and pick
    // tunings whose delay length lands on the rounding edge. The precheck
    // replicates process()'s exact arithmetic: readPos = float(writePos) -
    // delay for writePos = floor(delay), wrapped once by + bufferSize.
    for (float freq = 100.0f; freq < 1000.0f && candidates < 5;
         freq = std::nextafterf(freq, 2000.0f))
    {
        const float delay = static_cast<float>(sampleRate) / freq;
        const float fl    = std::floor(delay);
        const float frac  = delay - fl;
        if (frac <= 0.0f)
            continue;

        float readPos = fl - delay;      // tiny negative, exact (Sterbenz)
        readPos += bufferSize;           // the wrap process() performs
        if (readPos < bufferSize)
            continue;                    // rounds safely, not an edge case

        ++candidates;
        std::printf("edge tuning %d: freq=%.9g Hz, delay=%.9g samples (frac=%.3g)\n",
                    candidates, freq, delay, frac);

        CombFilter comb;
        comb.setSampleRate(sampleRate, maxDelay);
        comb.setFrequency(freq);
        comb.setFeedback(0.99f);
        comb.setDamping(0.9f);

        // One impulse, then run past a full buffer wrap so writePos passes
        // floor(delay) — the position where the read lands out of bounds.
        for (uint32_t i = 0; i < maxDelay + 4 + 250; ++i)
        {
            const float out = comb.process(i == 0 ? 1.0f : 0.0f);
            if (!std::isfinite(out))
            {
                std::printf("FAIL: non-finite output at sample %u\n", i);
                return 1;
            }
        }
    }

    if (candidates == 0)
    {
        std::printf("FAIL: no edge tunings found in scan range — test is broken\n");
        return 1;
    }

    std::printf("ok: %d edge tunings processed without out-of-bounds access\n", candidates);
    return 0;
}
