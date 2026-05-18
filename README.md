# Sympathetic Sitar

A sympathetic-resonance LV2 plugin for [MOD](https://mod.audio) devices
(MOD Dwarf, MOD Desktop) and any standard LV2 host. Drives 13 tuned
feedback comb-filter strings from the input signal to produce sitar-like
ringing, microtonal accompaniment — the same idea as a real sitar's
*taraf* (sympathetic strings beneath the frets).

![Pedalboard rendering of the Sympathetic Sitar plugin](plugins/Sitar/modgui/screenshot-sitar.png)

## What it does

Send a guitar, synth, voice, or anything else through the plugin. Each of
the 13 internal "strings" is a tuned resonator that rings whenever the
input has energy at (or near) the string's frequency or one of its
harmonics. The result is a shimmering, slowly-decaying halo of pitched
overtones that follow whatever you play, harmonized against the scale you
pick.

### Features

* **13 sympathetic strings**, each one a comb-filter resonator with
  fractional-delay tuning + per-string damping
* **Scale + Root selectors** (LV2 enum parameters — also appear in the
  cogwheel settings of any LV2 host)
  * 14 Western scales (major / minor / modes / pentatonics / blues /
    pythagorean / chromatic)
  * 4 Turkish makams (Rast, Uşşak, Hicaz, Saba) with proper neutral
    intervals (11-limit + 13-limit Just intonation)
  * 6 Hindustani ragas (Yaman, Bhairav, Bhairavi, Todi, Marwa, Malkauns)
* **Octave knob** (-1 to +3) that non-destructively transposes all 13
  strings. Strings that fall outside the piano range are muted, not
  clipped — moving the knob back unclips them losslessly.
* **Decay** with a perceptually-linear curve (knob travel ≈ change in
  ring time)
* **Bloom** — shared bridge-bus cross-coupling: every active string is fed
  a small fraction of the others' output through a DC-blocked, tanh-saturated
  bus, mirroring how a real sitar's *tarafs* share one physical bridge and
  excite each other. Adds a subtle ring extension and harmonic shimmer;
  deliberately scaled to stay stable across all Scale × Decay combinations.
* **Jawari** — tanh soft-saturation on the wet sum, emulating the gentle
  buzz of a real sitar's *jawari* bridge
* **Mix** — dry/wet blend
* **Test Scale** button — plucks each string in turn for 2 seconds so you
  can hear the tuning without needing to play anything
* **MOD pedalboard GUI** — proper drag-handle, brass-knob film-strip,
  audio jack rendering; renders correctly in MOD Desktop and on Dwarf
  hardware

## Installation

### Prerequisites

```bash
# Debian / Ubuntu
sudo apt install build-essential pkg-config git
```

### MOD Desktop (Linux, x86_64)

```bash
git clone --recurse-submodules https://github.com/YOUR_USER/sitar.git
cd sitar
make
./install.sh                       # installs to MOD Desktop's plugin dir
```

The `install.sh` script defaults to `~/Documents/MOD Desktop/lv2/` — the
official user-plugin path on Linux, which survives MOD Desktop reinstalls.
To install alongside MOD Desktop's bundled plugins instead (e.g. for
distribution testing), override with:

```bash
MOD_DESKTOP_PLUGINS=/path/to/mod-desktop-X.Y.Z/mod-desktop/plugins ./install.sh
```

Restart MOD Desktop and the plugin appears under brand **"sitar"** as
**"Sympathetic Sitar"**.

### MOD Dwarf (hardware, aarch64)

Cross-compile via Docker — fully self-contained. The only host requirement
is Docker. From a clean machine:

```bash
make dwarf-image          # one-time, ~30-60 min — builds Docker image with the aarch64 cross-toolchain inline
make dwarf                # cross-build + scp to Dwarf at 192.168.51.1
```

`make dwarf` is `make dwarf-build && make dwarf-deploy`. The bundle lands
locally at `bin/dwarf/sitar.lv2` (also pushed to `/root/.lv2/` on the
device). Override defaults on the command line:

```bash
make dwarf DWARF_HOST=sitar.local DWARF_USER=admin DWARF_LV2DIR=/usr/lib/lv2
```

The vendored Docker setup (toolchain targets glibc 2.27 + gcc 9.4.0, the
Dwarf's exact ABI) lives in [`mod-build/`](mod-build/README.md).

### Generic LV2 host (Carla, jalv, Reaper, Ardour, …)

```bash
git clone --recurse-submodules https://github.com/YOUR_USER/sitar.git
cd sitar
make
sudo make install               # to /usr/lib/lv2/
# or for a user-local install:
make install PREFIX="$HOME/.lv2" LV2_DIR="$HOME/.lv2"
```

The plugin URI is `http://sitar.local/plugins/sitar` — hosts will pick it up
on the next scan.

## Usage tips

* The plugin is a *resonator*, not a generator: it needs audio in to ring.
  Pluck a guitar, hum into a mic, or feed a synth into it.
* **Decay** below 0.3 is for percussive plucks; 0.5–0.7 for natural
  sympathetic feel; 0.9+ becomes a sustained drone.
* **Bloom** at 0 is fully independent strings (input-driven only).
  Around 0.3–0.6 adds a subtle ring extension as strings nudge each other
  through the shared bridge; at the top of the knob you get a ~2× boost
  to sustained resonance. Bloom is deliberately conservative — full
  cross-coupling at high Decay would run away into self-oscillation
  because feedback combs have resonance peaks at every harmonic of their
  tuned frequency, so multiple strings line up at the same pitch. The
  current setting trades drone-like wash for predictable stability.
* The default **Mix** is 0.5 (dry + wet). For a pure sympathetic effect
  put it in a parallel chain at Mix = 1.0 and blend manually.
* **Test Scale** sequences all 13 strings as a 26-second pluck pattern, so
  you can audibly verify scale and root before playing.
* For sitar-authentic feel, try **Raga Yaman** with root **A2** and
  **Jawari ≈ 0.3** with a clean guitar input.

## Project layout

```
.
├── plugins/Sitar/             — the DPF plugin (C++ DSP + modgui)
│   ├── SitarPlugin.cpp        — main DSP, scale tables, audition
│   ├── CombFilter.hpp         — fractional-delay feedback comb filter
│   ├── DistrhoPluginInfo.h    — DPF plugin metadata
│   ├── modgui/                — MOD pedalboard GUI (HTML/CSS/JS/sprite)
│   ├── modgui.ttl             — MOD GUI declaration
│   └── Makefile               — DPF build glue
├── dpf/                       — DISTRHO Plugin Framework (git submodule)
├── mod-build/                 — Cross-compile recipe for MOD Dwarf
│   ├── sitar.mk               — mod-plugin-builder package recipe
│   └── README.md              — Dwarf cross-build walkthrough
├── Makefile                   — top-level build + install + Dwarf targets
└── install.sh                 — convenience installer for MOD Desktop
```

## License

The plugin source is released under [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/)
— effectively public domain, no attribution required. Use it however
you want.

DPF is ISC/MIT depending on the file (see [`dpf/LICENSE`](dpf/LICENSE)).

The MOD modgui knob sprite at `plugins/Sitar/modgui/knobs/sitar-knob.png`
is generated procedurally (see `generate_knob.py` in that folder) and is
also CC0.

The thumbnail cartoon sitar (`plugins/Sitar/modgui/thumbnail-sitar.png`)
is **not** CC0 — it's from Flaticon and carries the Flaticon free-license
attribution requirement:
[Sitar icons created by Freepik – Flaticon](https://www.flaticon.com/free-icons/sitar).

## Acknowledgements

* [DISTRHO Plugin Framework](https://github.com/DISTRHO/DPF) — the cross-platform LV2/VST/CLAP framework
* [MOD Audio](https://mod.audio) — for the Dwarf, MOD Desktop, mod-plugin-builder, and the modgui design
* Microtonal interval references: [microtonaltheory.com](https://www.microtonaltheory.com/microtonal-ethnography/turkish-makams), [Sethares Akkoc 2014](https://sethares.engr.wisc.edu/paperspdf/MP3204_02_Akkoc.pdf), [narenratan/scale-library](https://github.com/narenratan/scale-library), and Bhatkhande's Just-intonation swara ratios for the Hindustani ragas
