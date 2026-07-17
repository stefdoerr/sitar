/*
 * Sympathetic Resonance — Sitar-style string simulator.
 * DPF plugin info header.
 */

#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

// Build the "beta" variant by passing -DSITAR_BETA on the command line
// (top-level Makefile sets this when BETA=1). The beta build gets a
// distinct LV2 URI, bundle name, label, and DPF unique-id so it can
// install side-by-side with the stable plugin for A/B testing.
#ifdef SITAR_BETA
#define DISTRHO_PLUGIN_BRAND   "sitar-beta"
#define DISTRHO_PLUGIN_NAME    "Sitar (Beta)"
#define DISTRHO_PLUGIN_URI     "http://sitar.local/plugins/sitar-beta"
#define DISTRHO_PLUGIN_CLAP_ID "local.sitar.sitar-beta"
#define DISTRHO_PLUGIN_BRAND_ID  StBt
#define DISTRHO_PLUGIN_UNIQUE_ID dStb
#else
#define DISTRHO_PLUGIN_BRAND   "sitar"
#define DISTRHO_PLUGIN_NAME    "Sitar"
#define DISTRHO_PLUGIN_URI     "http://sitar.local/plugins/sitar"
#define DISTRHO_PLUGIN_CLAP_ID "local.sitar.sitar"
#define DISTRHO_PLUGIN_BRAND_ID  Sitr
#define DISTRHO_PLUGIN_UNIQUE_ID dStr
#endif

// Real project homepage — hosts surface it (MOD's info dialog links its
// "See online" button here). The LV2 URI above is just an identifier, not
// a web page; returning it gives users a dead link.
#define PLUGIN_HOMEPAGE "https://github.com/stefdoerr/sitar"

// LV2 plugin class -> the "Category" shown in MOD's plugin info / store
// (mod-ui maps lv2:SimulatorPlugin to its "Simulator" category — the home
// of instrument/body simulations, which a sympathetic-string sim is).
#define DISTRHO_PLUGIN_LV2_CATEGORY   "lv2:SimulatorPlugin"

#define DISTRHO_PLUGIN_HAS_UI         0
#define DISTRHO_PLUGIN_IS_RT_SAFE     1
#define DISTRHO_PLUGIN_NUM_INPUTS     1
#define DISTRHO_PLUGIN_NUM_OUTPUTS    2
#define DISTRHO_PLUGIN_WANT_PROGRAMS                          0
#define DISTRHO_PLUGIN_WANT_STATE                             0
#define DISTRHO_PLUGIN_WANT_PARAMETER_VALUE_CHANGE_REQUEST    1

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
