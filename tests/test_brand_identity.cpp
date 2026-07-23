/*
 * Regression test: plugin identity strings after the boreas-style rebrand.
 *   - getMaker() is the VENDOR brand -> "Stefan"
 *   - getLabel() is the PLUGIN name  -> "Sitar" (must NOT be the brand)
 *
 * Build (mirrors the `make test` recipe):
 *   g++ -std=gnu++14 -g -O0 -fsanitize=address -Iplugins/Sitar -Idpf/distrho \
 *       tests/test_brand_identity.cpp plugins/Sitar/SitarPlugin.cpp \
 *       dpf/distrho/src/DistrhoPlugin.cpp -o build/tests/test_brand_identity
 */

#include "src/DistrhoPluginInternal.hpp"

#include <cstdio>
#include <cstring>

USE_NAMESPACE_DISTRHO

int main()
{
    d_nextBufferSize = 512;
    d_nextSampleRate = 48000.0;
    d_nextCanRequestParameterValueChanges = true;

    PluginExporter plugin(nullptr, nullptr, nullptr, nullptr);

    int failures = 0;

    if (std::strcmp(plugin.getMaker(), "Stefan") != 0)
    {
        std::printf("FAIL: getMaker() = \"%s\", expected \"Stefan\"\n", plugin.getMaker());
        ++failures;
    }
    if (std::strcmp(plugin.getLabel(), "Sitar") != 0)
    {
        std::printf("FAIL: getLabel() = \"%s\", expected \"Sitar\"\n", plugin.getLabel());
        ++failures;
    }

    if (failures != 0)
        return 1;

    std::printf("ok: getMaker()=Stefan, getLabel()=Sitar\n");
    return 0;
}
