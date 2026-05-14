/*
 * Sitar Sympathetic Resonator — Feedback comb filter with fractional delay.
 */

#ifndef SITAR_COMB_FILTER_HPP_INCLUDED
#define SITAR_COMB_FILTER_HPP_INCLUDED

#include <cmath>
#include <cstdint>
#include <cstring>

class CombFilter
{
public:
    CombFilter() = default;

    ~CombFilter()
    {
        delete[] fBuffer;
    }

    // Allocate the delay line. Called when the sample rate changes.
    // maxDelaySamples must accommodate the lowest expected frequency.
    void setSampleRate(double sampleRate, uint32_t maxDelaySamples)
    {
        fSampleRate = sampleRate;

        delete[] fBuffer;
        fBufferSize = maxDelaySamples + 4; // small headroom for interpolation
        fBuffer = new float[fBufferSize];
        clear();
    }

    // Zero out the delay line and damping state.
    void clear()
    {
        if (fBuffer != nullptr)
            std::memset(fBuffer, 0, sizeof(float) * fBufferSize);
        fWritePos = 0;
        fLastOut = 0.0f;
    }

    // Set the resonant frequency in Hz. The delay length becomes fs / freq.
    void setFrequency(float freq)
    {
        if (freq < 1.0f)
            freq = 1.0f;

        float delay = static_cast<float>(fSampleRate) / freq;

        // Clamp to buffer bounds (leave headroom for the interpolator).
        const float maxDelay = static_cast<float>(fBufferSize) - 2.0f;
        if (delay > maxDelay) delay = maxDelay;
        if (delay < 2.0f)     delay = 2.0f;

        fDelaySamples = delay;
    }

    // Feedback coefficient in [0, ~1). Controls decay length.
    void setFeedback(float fb)
    {
        if (fb < 0.0f)     fb = 0.0f;
        if (fb > 0.99999f) fb = 0.99999f;
        fFeedback = fb;
    }

    // One-pole low-pass damping in the feedback loop (0 = full damping, 1 = none).
    // Models the natural high-frequency loss in a real vibrating string.
    void setDamping(float d)
    {
        if (d < 0.0f) d = 0.0f;
        if (d > 1.0f) d = 1.0f;
        fDamping = d;
    }

    // Process a single sample.
    inline float process(float input)
    {
        // Read pointer: writePos - delay, wrapped into the ring buffer.
        float readPos = static_cast<float>(fWritePos) - fDelaySamples;
        while (readPos < 0.0f)
            readPos += static_cast<float>(fBufferSize);

        const uint32_t idx0 = static_cast<uint32_t>(readPos);
        const uint32_t idx1 = (idx0 + 1) % fBufferSize;
        const float frac = readPos - static_cast<float>(idx0);

        const float delayed = fBuffer[idx0] + frac * (fBuffer[idx1] - fBuffer[idx0]);

        // One-pole damping inside the loop.
        fLastOut = fDamping * delayed + (1.0f - fDamping) * fLastOut;

        const float fbSample = fLastOut * fFeedback;
        fBuffer[fWritePos] = input + fbSample;

        fWritePos = (fWritePos + 1) % fBufferSize;

        return delayed;
    }

private:
    float* fBuffer = nullptr;
    uint32_t fBufferSize = 0;
    uint32_t fWritePos = 0;

    double fSampleRate = 48000.0;
    float fDelaySamples = 100.0f;
    float fFeedback = 0.9f;
    float fDamping = 0.5f;
    float fLastOut = 0.0f;
};

#endif // SITAR_COMB_FILTER_HPP_INCLUDED
