# Sympathetic Sitar

A sympathetic-resonance LV2 plugin for [MOD](https://mod.audio) devices
(MOD Dwarf, MOD Desktop) and any standard LV2 host. Drives up to 48
tuned feedback comb-filter strings from the input signal to produce
sitar-like ringing, microtonal accompaniment — the same idea as a real
sitar's *taraf* (sympathetic strings beneath the frets).

![Pedalboard rendering of the Sympathetic Sitar plugin](plugins/Sitar/modgui/screenshot-sitar.png)

## What it does

Send a guitar, synth, voice, or anything else through the plugin. Each
internal "string" is a tuned resonator that rings whenever the input has
energy at (or near) the string's frequency or one of its harmonics. The
result is a shimmering, slowly-decaying halo of pitched overtones that
follow whatever you play, harmonized against the scale you pick. The
48-string bank spans roughly four octaves above the root, and
`N STRINGS` cursors over how many of them ring at once.

### Features

* **Up to 48 sympathetic strings**, each one a comb-filter resonator with
  fractional-delay tuning, per-string damping, and an in-loop tanh
  saturator so a single string can't dominate when its resonance
  frequency is excited
* **N STRINGS** knob (1–48) — picks how many strings ring at once,
  starting from the root upward through the scale
* **Scale + Root selectors** (LV2 enum parameters — also appear in the
  cogwheel settings of any LV2 host)
  * 14 Western scales (major / minor / modes / pentatonics / blues /
    pythagorean / chromatic)
  * 4 Turkish makams (Rast, Uşşak, Hicaz, Saba) with proper neutral
    intervals (11-limit + 13-limit Just intonation)
  * 6 Hindustani ragas (Yaman, Bhairav, Bhairavi, Todi, Marwa, Malkauns)
* **Root** as a pitch class (C / C# / D / … / B) — paired with the OCT
  knob it covers C2 through C8
* **OCT** (2–6) — absolute octave for the root pitch class. Default 3
  puts the root at C3 (≈131 Hz); higher octaves leave only upper
  harmonics of the input to excite the strings, for shimmer-only effects.
* **13 fine-tune knobs** for the first 13 populated string slots — edit
  them to detune individual strings. Detunes are performance tweaks
  relative to the current scale's pitches: any SCALE / ROOT / OCT /
  N STRINGS change recomputes them from scratch. That includes reloading
  a saved pedalboard whose SCALE / ROOT / OCT / N STRINGS differ from the
  defaults — detunes are deliberately not carried across, since they'd
  land on different notes.
* **Decay** with a curve that pushes the audible sustain range into the
  lower half of the knob (knob travel ≈ change in ring time). Capped at
  fb = 0.998 to prevent infinite resonance build-up.
* **Bloom** — shared bridge-bus cross-coupling: every active string is fed
  a small fraction of the others' output through a DC-blocked,
  tanh-saturated bus, mirroring how a real sitar's *tarafs* share one
  physical bridge and excite each other. Adds a subtle ring extension
  and harmonic shimmer; coupling is scaled by the active string count to
  stay stable across all Scale × Decay × N STRINGS combinations.
* **Jawari** — tanh soft-saturation on the wet sum, emulating the gentle
  buzz of a real sitar's *jawari* bridge. Level-neutral: the saturated
  path is normalized for small-signal unity and blended in by the knob,
  so JAWARI adds harmonics without boosting the wet level or stepping
  when it engages. A ~3.5 kHz one-pole tilt-down LPF fades in with the
  knob to tame the brittle top-end harmonics the saturator generates.
* **Mix** — equal-power dry/wet crossfade (`dry·cos(mix·π/2) +
  wet·sin(mix·π/2)`) so the output stays at roughly constant power
  across the knob.
* **Level** — output trim, ±12 dB, applied post-mix to the L/R bus.
  For the last bit of input↔output matching by ear.
* **Sens** (sensitivity) — input trim into the comb bank, 0..1. Lower
  values cause strings to excite more slowly and quietly at any given
  input level. Sits between the noise gate and the strings.
* **Gate** — input noise gate. Peak-envelope follower with instant
  attack / 80 ms release; mutes only the signal feeding the combs, so
  existing rings keep decaying at the user-set rate. Knob 0..10 maps
  internally to a linear-amplitude threshold (10 ≈ -34 dBFS).
* **Stereo** — 5-mode layout: Mono / Linear Narrow / Linear / Wide
  Narrow / Wide. *Linear* spreads strings monotonically low→left
  high→right; *Wide* alternates edges-to-centre so adjacent scale
  degrees end up on opposite sides; *Narrow* variants halve the pan
  magnitude. Defaults to Wide Narrow.
* **Test Scale** button — plucks each populated string in turn for
  ~2 seconds so you can hear the tuning without needing to play anything.
  Output bypasses dry so only the wet plucks are audible.
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

**Prebuilt bundle** (recommended): grab
`sitar-vX.Y.Z-linux-x86_64.tar.gz` from the
[Releases page](https://github.com/stefdoerr/sitar/releases),
extract it, and copy the `sitar.lv2/` directory into your MOD Desktop
plugins directory (default `~/Documents/MOD Desktop/lv2/`). Restart
MOD Desktop.

**Or build from source:**

```bash
git clone --recurse-submodules https://github.com/stefdoerr/sitar.git
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

**Prebuilt bundle** (recommended): grab
`sitar-vX.Y.Z-dwarf-aarch64.tar.gz` from the
[Releases page](https://github.com/stefdoerr/sitar/releases), extract,
and copy the `sitar.lv2/` directory into the Dwarf's `/root/.lv2/`
(via `scp -O -r sitar.lv2 root@192.168.51.1:/root/.lv2/`). Then SSH in
and run `systemctl restart jack2 mod-ui` so the new plugin world is
picked up.

**Or cross-compile from source** via Docker (the only host requirement is
Docker). From a clean machine:

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
* **N STRINGS** at 13 (default) covers a single octave of a 7-note scale
  or two octaves of a pentatonic. Push to 28+ for the full 4-octave
  span, or drop to 4-7 for a sparse tambura-style drone.
* **OCT** at 3 (default) puts the root at C3-ish; OCT 5–6 leaves only the
  upper harmonics of bassy input to excite the strings, for "shimmer
  only" effects with no muddy fundamentals.
* **Decay** below 0.3 is for percussive plucks; 0.5–0.7 for natural
  sympathetic feel; 0.9+ becomes a sustained drone.
* **Bloom** at 0 is fully independent strings (input-driven only).
  Around 0.3–0.6 adds a subtle ring extension as strings nudge each
  other through the shared bridge; at the top of the knob you get a ~2×
  boost to sustained resonance. Bloom is deliberately conservative —
  full cross-coupling at high Decay would run away into self-oscillation
  because feedback combs have resonance peaks at every harmonic of their
  tuned frequency, so multiple strings line up at the same pitch. The
  current setting trades drone-like wash for predictable stability.
* The default **Mix** is 0.5 (dry + wet, equal-power). For a pure
  sympathetic effect put it in a parallel chain at Mix = 1.0 and blend
  manually.
* **Sens** lower (≈ 0.3–0.6) is the cure if singing the root note (or any
  scale pitch) makes a single string dominate. The per-string limiter
  already caps absolute amplitude, but lowering Sens slows the build-up.
* **Level** (±12 dB) is the last bit of input↔output matching. After
  setting Mix and Sens, dial Level by ear so plugin-on and plugin-off
  feel the same volume.
* **Gate** at 0 disables the gate (good for studio-clean signals); 1.0
  (default) catches typical mic / guitar noise floors. Push higher if
  your input is genuinely noisy.
* **Stereo** at *Wide Narrow* (default) is the safe musical choice. *Wide*
  is the most theatrical (adjacent scale degrees on opposite sides);
  *Mono* is right for a single-speaker live rig.
* **Test Scale** sequences every populated string as a ~2-second pluck
  pattern (so 13 strings = ~26 s, 28 strings = ~56 s), so you can audibly
  verify scale × root × OCT before playing.
* For sitar-authentic feel, try **Raga Yaman** with root **A**, **OCT 2**,
  and **Jawari ≈ 0.3** with a clean guitar input.

## Project layout

```
.
├── plugins/Sitar/             — the DPF plugin (C++ DSP + modgui)
│   ├── SitarPlugin.cpp        — main DSP, scale tables, audition
│   ├── CombFilter.hpp         — fractional-delay feedback comb filter
│   │                            with in-loop tanh saturator
│   ├── DistrhoPluginInfo.h    — DPF plugin metadata; conditional
│   │                            stable / beta identity via SITAR_BETA
│   ├── modgui/                — MOD pedalboard GUI (HTML/CSS/JS/sprite)
│   ├── modgui.ttl             — MOD GUI declaration
│   └── Makefile               — DPF build glue (BETA=1 retags here)
├── dpf/                       — DISTRHO Plugin Framework (git submodule)
├── mod-build/                 — Self-contained Dwarf cross-build setup
│   ├── Dockerfile             — vendored mod-plugin-builder Dockerfile,
│   │                            inline aarch64 toolchain build
│   ├── build-sitar.sh         — runs inside the container; rsyncs source,
│   │                            does the host TTL pass + aarch64 .so build
│   └── README.md              — Dwarf cross-build walkthrough
├── Makefile                   — top-level build + install + Dwarf targets;
│                                BETA=1 builds the side-by-side beta bundle
└── install.sh                 — convenience installer for MOD Desktop
                                 (BETA=1 to install sitar-beta.lv2)
```

### Side-by-side beta builds

For A/B testing, the same source can be built as a second plugin with a
distinct URI / brand / unique-id by setting `BETA=1`:

```bash
make beta                                    # builds bin/sitar-beta.lv2
MOD_DESKTOP_PLUGINS=/path/to/mod-desktop/plugins BETA=1 ./install.sh
```

The beta and stable bundles can co-exist on the same host; the beta
shows up as **"Sitar (Beta)"** under brand **"sitar-beta"**.

## License

The plugin source is released under [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/)
— effectively public domain, no attribution required. Use it however
you want.

DPF is ISC-licensed (see [`dpf/LICENSE`](dpf/LICENSE)).

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
