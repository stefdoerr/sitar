# Sympathetic Sitar — developer guide

Building from source, installing local builds, cross-compiling for the MOD
Dwarf, the release / publishing workflow, and a full control + DSP reference.

**Using the plugin needs none of this** — see the [README](README.md#install)
to download a prebuilt bundle and drop it in.

## Build from source

```bash
# Debian / Ubuntu
sudo apt install build-essential pkg-config git
```

Clone with submodules (DPF is vendored as one) and build:

```bash
git clone --recurse-submodules https://github.com/stefdoerr/sitar.git
cd sitar
make            # produces bin/sitar.{lv2,vst3,clap} with your host toolchain
```

## Install a local build

**MOD Desktop:**

```bash
./install.sh                       # installs bin/sitar.lv2 to MOD Desktop's plugin dir
```

`install.sh` defaults to `~/Documents/MOD Desktop/lv2/` — the official
user-plugin path on Linux, which survives MOD Desktop reinstalls. To install
alongside MOD Desktop's bundled plugins instead (e.g. for distribution
testing), override with:

```bash
MOD_DESKTOP_PLUGINS=/path/to/mod-desktop-X.Y.Z/mod-desktop/plugins ./install.sh
```

**Generic LV2 host** (Carla, jalv, Reaper, Ardour, …):

```bash
sudo make install                  # to /usr/lib/lv2/
# or for a user-local install:
make install PREFIX="$HOME/.lv2" LV2_DIR="$HOME/.lv2"
```

The plugin URI is `http://sitar.local/plugins/sitar` — hosts will pick it up
on the next scan.

**Desktop VST3 / CLAP:** `make` already produced `bin/sitar.vst3` and
`bin/sitar.clap` — copy them into the system plugin folders listed under
[Install → Desktop DAWs](README.md#desktop-daws--vst3--clap-linux--windows--macos)
in the README.

## Tests

```bash
make test        # host-less DSP regression tests, compiled + run under AddressSanitizer
```

Each `tests/test_*.cpp` is a plain `main()` compiled directly against the
plugin source and DPF — no host required.

## MOD Dwarf cross-compile (Docker)

The only host requirement is Docker. From a clean machine:

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

## Side-by-side beta builds

For A/B testing, the same source can be built as a second plugin with a
distinct URI / unique-id by setting `BETA=1`:

```bash
make beta                                    # builds bin/sitar-beta.lv2
MOD_DESKTOP_PLUGINS=/path/to/mod-desktop/plugins BETA=1 ./install.sh
```

The beta and stable bundles can co-exist on the same host; the beta
shows up as **"Sitar (Beta)"** under brand **"Stefan"**.

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
├── docs/manual/               — beginner PDF manual (HTML source + generated
│                                PDF; `make manual` re-renders via Chrome)
├── tests/                     — host-less DSP regression tests (`make test`,
│                                runs under AddressSanitizer)
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

## Cutting a release

`make release version=X.Y.Z` creates the GitHub release with the MOD /
Patchstorage LV2 bundles, the Dwarf build, and the PDF manual. GitHub Actions
then cross-builds the desktop **VST3 / CLAP** for Linux, Windows, and macOS and
attaches them to the same release automatically.

## Publishing to Patchstorage

`make patchstorage` cross-builds the plugin for the three targets
patchstorage.com's LV2-plugins platform supports and publishes it, reusing
Patchstorage's own prebuilt toolchain images. See
[`patchstorage-build/README.md`](patchstorage-build/README.md) for details.

| Target | Arch / ABI | glibc |
|---|---|---|
| `linux-amd64` | x86-64, SSE2 | 2.27 |
| `rpi-aarch64` | AArch64 | 2.27 |
| `patchbox-os-arm32` | 32-bit armhf + NEON hard-float | 2.31 |

**Prerequisites:** Docker, `jq`, and Python 3 with `requests`/`click`/`rdflib`.
A dedicated env is cleanest — it survives `make clean` (which wipes `build/`):

```bash
conda create -y -n patchstorage-uploader python=3.12 pip
conda run -n patchstorage-uploader pip install requests click rdflib
```

Then `conda activate patchstorage-uploader` before `make`, or pass
`PYTHON="$(conda run -n patchstorage-uploader which python)"`.

**Targets:**
- `make patchstorage-build` — build all three bundles into `build/patchstorage/`
- `make patchstorage-prepare` — assemble + generate metadata for inspection under `build/ps-upload/dist/`
- `make patchstorage PS_USER=<username>` — build + prepare + publish (prompts for the password; nothing stored)

A modgui **screenshot** must be present in the bundle (Sitar ships one), and the
repo-root `patchstorage.json` supplies `source_code_url` / `donate_url`. The three
bundles are also attached to GitHub releases (via `make release`), with
`linux-amd64` replacing the old `linux-x86_64` asset.

## Controls & DSP reference

Every control in detail, and the DSP behind it. (The [PDF
manual](docs/manual/sitar-manual.pdf) covers the same ground for players; this
is the terse engineering version.)

* **Up to 48 sympathetic strings**, each one a comb-filter resonator with
  fractional-delay tuning, per-string damping, and an in-loop tanh
  saturator so a single string can't dominate when its resonance
  frequency is excited
* **STRINGS** knob (1–48) — picks how many strings ring at once,
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
* **Custom scales** — beyond the built-ins, define your own microtonal
  tuning (ratios like `3/2` or cents like `347.4`) in the pedal's scale
  editor (the ✎ button). Up to 8 user scales, saved with the pedalboard;
  they appear in the SCALE menu alongside the presets. Per-string
  frequencies are computed internally — there are no per-string knobs or
  ports to manage. See the manual's *Make your own scale*.
* **Decay** with a curve that pushes the audible sustain range into the
  lower half of the knob (knob travel ≈ change in ring time). Capped at
  fb = 0.998 to prevent infinite resonance build-up.
* **Bloom** — shared bridge-bus cross-coupling: every active string is fed
  a small fraction of the others' output through a DC-blocked,
  tanh-saturated bus, mirroring how a real sitar's *tarafs* share one
  physical bridge and excite each other. Adds a subtle ring extension
  and harmonic shimmer; coupling is scaled by the active string count to
  stay stable across all Scale × Decay × STRINGS combinations.
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
