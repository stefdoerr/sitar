/*
 * Regression test: STEREO MODE changes must not touch string tunings.
 *
 * Changing the stereo layout only needs the pan table rebuilt. It must not
 * recompute the string frequencies from the scale table — that silently
 * reverts any user fine-tune, and (because the stereo path doesn't notify
 * the host) leaves the host UI showing detunes the DSP no longer plays.
 *
 * SCALE / ROOT / OCT / NUM changes, by contrast, are *supposed* to reset
 * fine-tunes (detunes are relative to a specific scale's pitches), so the
 * second half of the test pins that behavior down too.
 *
 * Build:
 *   g++ -std=gnu++14 -g -O0 -fsanitize=address \
 *       -I../plugins/Sitar -I../dpf/distrho \
 *       test_stereo_mode_detune.cpp ../plugins/Sitar/SitarPlugin.cpp \
 *       ../dpf/distrho/src/DistrhoPlugin.cpp -o test_stereo_mode_detune
 */

#include "src/DistrhoPluginInternal.hpp"

#include <cmath>
#include <cstdio>

USE_NAMESPACE_DISTRHO

static bool recordRequestCallback(void*, uint32_t, float)
{
    return true;
}

static int gFailures = 0;

static void expectNear(const char* what, float actual, float expected)
{
    if (std::fabs(actual - expected) > 0.01f)
    {
        std::printf("FAIL: %s — expected %.2f, got %.2f\n", what, expected, actual);
        ++gFailures;
    }
}

int main()
{
    d_nextBufferSize = 512;
    d_nextSampleRate = 48000.0;
    d_nextCanRequestParameterValueChanges = true;

    PluginExporter plugin(nullptr, nullptr, recordRequestCallback, nullptr);

    const uint32_t kString3    = 2;   // string_3 port, 0-based param index
    const uint32_t kScale      = 53;
    const uint32_t kStereoMode = 57;

    // Default: C major, root C3 (130.81 Hz). String 3 is the major third.
    expectNear("string_3 default (C major 3rd)",
               plugin.getParameterValue(kString3), 130.81f * 5.0f / 4.0f);

    // User fine-tunes string 3.
    plugin.setParameterValue(kString3, 440.0f);
    expectNear("string_3 after user detune",
               plugin.getParameterValue(kString3), 440.0f);

    // Stereo mode Wide Narrow (default, 3) -> Wide (4): pan-only change,
    // the detune must survive.
    plugin.setParameterValue(kStereoMode, 4.0f);
    expectNear("string_3 after stereo mode change",
               plugin.getParameterValue(kString3), 440.0f);

    // Scale change Major (0) -> Phrygian (5): detunes are scale-relative,
    // so this MUST reset string 3 to the phrygian minor third.
    plugin.setParameterValue(kScale, 5.0f);
    expectNear("string_3 after scale change (reset to Phrygian 3rd)",
               plugin.getParameterValue(kString3), 130.81f * 6.0f / 5.0f);

    if (gFailures != 0)
    {
        std::printf("%d failure(s)\n", gFailures);
        return 1;
    }

    std::printf("ok: stereo mode preserves detunes, scale change resets them\n");
    return 0;
}
