# Sympathetic Sitar

A sympathetic-resonance plugin — **LV2, VST3, and CLAP** — for
[MOD](https://mod.audio) devices (MOD Dwarf, MOD Desktop), desktop DAWs on
Linux / Windows / macOS, and any standard LV2 host. Drives up to 48 tuned
feedback comb-filter strings from the input signal to produce sitar-like
ringing, microtonal accompaniment — the same idea as a real sitar's *taraf*
(sympathetic strings beneath the frets).

**[Install](#install) · [Manual (PDF)](docs/manual/sitar-manual.pdf) · [Releases](https://github.com/stefdoerr/sitar/releases/latest) · [Building & contributing](DEVELOPERS.md)**

![Pedalboard rendering of the Sympathetic Sitar plugin](plugins/Sitar/modgui/screenshot-sitar.png)

## What it does

Send a guitar, synth, voice, or anything else through the plugin. Each
internal "string" is a tuned resonator that rings whenever the input has
energy at (or near) the string's frequency or one of its harmonics. The
result is a shimmering, slowly-decaying halo of pitched overtones that
follow whatever you play, harmonized against the scale you pick. The
48-string bank spans roughly four octaves above the root, and
`STRINGS` cursors over how many of them ring at once.

The [PDF manual](docs/manual/sitar-manual.pdf) walks through every control
with pictures — the fastest way in.

## Install

Every release ships ready-to-run bundles — **no build tools, no developer
setup**. Grab the latest for your platform from the
**[Releases page](https://github.com/stefdoerr/sitar/releases/latest)** and
follow the matching steps below.

> Replace `X.Y.Z` below with the version you downloaded (e.g. `0.2.0`). The
> plugin always appears under brand **Stefan** as **Sympathetic Sitar**.

### MOD Desktop (Linux)

Download `sitar-vX.Y.Z-linux-amd64.tar.gz`, then copy the `sitar.lv2/` folder
into your MOD Desktop plugin directory (default `~/Documents/MOD Desktop/lv2/`):

```bash
tar xf sitar-vX.Y.Z-linux-amd64.tar.gz
cp -r sitar.lv2 ~/"Documents/MOD Desktop/lv2/"
```

Restart MOD Desktop and the plugin shows up in the plugin store.

### MOD Dwarf (hardware)

Download `sitar-vX.Y.Z-dwarf-aarch64.tar.gz`, copy `sitar.lv2/` onto the device
over the network, and restart its audio stack:

```bash
tar xf sitar-vX.Y.Z-dwarf-aarch64.tar.gz
scp -O -r sitar.lv2 root@192.168.51.1:/root/.lv2/
ssh root@192.168.51.1 'systemctl restart jack2 mod-ui'
```

The plugin then appears in the Dwarf's plugin store.

### Desktop DAWs — VST3 / CLAP (Linux · Windows · macOS)

Download the bundle for your OS and put the plugin in your system plugin folder:

| OS | Download | Copy the `.vst3` / `.clap` into |
|---|---|---|
| **Linux** | `sitar-vX.Y.Z-linux-x86_64.tar.xz` | `~/.vst3/` and `~/.clap/` |
| **Windows** | `sitar-vX.Y.Z-win64.zip` | `%COMMONPROGRAMFILES%\VST3\` and `…\CLAP\` |
| **macOS** | `sitar-vX.Y.Z-macos-universal.pkg` | run the installer |

The macOS package is **unsigned** — right-click it and choose **Open** to get
past Gatekeeper. Rescan plugins in your DAW afterwards.

> **Raspberry Pi / Patchbox OS:** LV2 builds for `rpi-aarch64` and
> `patchbox-os-arm32` are on the Releases page too, and the plugin is published
> on [Patchstorage](https://patchstorage.com/) for one-tap install on supported
> devices.

## Usage tips

* The plugin is a *resonator*, not a generator: it needs audio in to ring.
  Pluck a guitar, hum into a mic, or feed a synth into it.
* **STRINGS** at 13 (default) covers a single octave of a 7-note scale
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

## Manual

A beginner-friendly **PDF manual** walks through every control, sound recipes,
and installation — no developer knowledge needed. **[Read the
manual](docs/manual/sitar-manual.pdf).** It's bundled in every
[release](https://github.com/stefdoerr/sitar/releases/latest) as
`sitar-manual.pdf`, and on a MOD device it's the *documentation* button in the
plugin's info dialog.

## Building & contributing

Building from source, installing local builds, cross-compiling for the MOD
Dwarf, the release / publishing workflow, and the full control + DSP reference
all live in **[DEVELOPERS.md](DEVELOPERS.md)**.

## License

The plugin source is ISC-licensed. DPF is ISC-licensed (see [`dpf/LICENSE`](dpf/LICENSE)).

The MOD modgui knob sprite at `plugins/Sitar/modgui/knobs/sitar-knob.png`
is generated procedurally (see `generate_knob.py` in that folder) and is
released as CC0 / public domain.

The thumbnail cartoon sitar (`plugins/Sitar/modgui/thumbnail-sitar.png`)
is **not** CC0 — it's from Flaticon and carries the Flaticon free-license
attribution requirement:
[Sitar icons created by Freepik – Flaticon](https://www.flaticon.com/free-icons/sitar).

## Acknowledgements

* [DISTRHO Plugin Framework](https://github.com/DISTRHO/DPF) — the cross-platform LV2/VST/CLAP framework
* [MOD Audio](https://mod.audio) — for the Dwarf, MOD Desktop, mod-plugin-builder, and the modgui design
* Microtonal interval references: [microtonaltheory.com](https://www.microtonaltheory.com/microtonal-ethnography/turkish-makams), [Sethares Akkoc 2014](https://sethares.engr.wisc.edu/paperspdf/MP3204_02_Akkoc.pdf), [narenratan/scale-library](https://github.com/narenratan/scale-library), and Bhatkhande's Just-intonation swara ratios for the Hindustani ragas
