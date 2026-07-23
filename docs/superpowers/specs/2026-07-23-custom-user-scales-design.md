# Design — Custom user scales (state-based) + internal strings

**Date:** 2026-07-23
**Scope:** Sitar plugin — DSP, LV2/DPF plumbing, MOD modgui, docs. Lets users
define and save their own microtonal scales; removes the per-string frequency
ports (strings become fully internal).

Follows the exploration in this session (MOD/DPF file & state mechanisms,
modgui `patch_set`/`patch_get`, the step-sequencer precedent).

---

## Goal & motivation

- **User-definable scales.** Beyond the 24 built-ins, a user can create their
  own scale (microtonal — ratios or cents) and save it with their pedalboard.
- **Internalize the strings.** Remove the 48 `string_N` LV2 ports; the DSP
  already computes string frequencies, so they need not be exposed. This
  retires the per-string detune knobs entirely (they were already off the
  modgui face; now they leave the port list too).
- **Keep the microtonal identity.** Custom scales are just-intonation/cents
  capable — the same expressiveness as the built-in makams/ragas. (A plain
  MIDI-note-driven approach was explicitly rejected: standard MIDI is 12-TET
  and cannot carry neutral perdes / komal swaras.)

## Decisions (locked in brainstorming)

- **State-based, per-pedalboard** for v1. Custom scales persist as DPF plugin
  **state**, saved inside the pedalboard / MOD user-preset. `.scl` import/export
  (a portable personal library) is **deferred** to a later add-on (it also
  needs an on-device test of MOD's custom-file-type support).
- **Built-in scales stay.** The 24 `kScales[]` remain; user scales are added
  alongside them.
- **Editor lives in the modgui** (sitar has no native UI). A **text-box editor**
  (name + a line of comma-separated intervals) is the v1 UX — least JS, and it
  lets users paste tunings from Scala files / theory tools. A row-by-row editor
  can come later.
- **Fixed number of user slots: 8.** Keeps the `scale` selector a normal
  automatable enum and bounds complexity. (Revisit if 8 proves too few.)

## Non-goals (v1)

- `.scl` file import/export (deferred add-on).
- MIDI input / MTS / MPE (rejected — breaks microtonality).
- A row-by-row / graphical scale editor (later polish).
- Cross-pedalboard "global library" beyond what MOD plugin-presets already give.

---

## Serialization format (the `userscales` state string)

One scale per line: `Name | i1, i2, i3, …`

- Intervals are **ratios** (`11/9`, `3/2`) or **cents** (a number containing a
  `.`, e.g. `347.4`). The tonic `1/1` is implicit as the first degree and may
  be omitted or written explicitly.
- The **number of intervals = notesPerOctave**; the bank repeats them across
  the 4-octave span exactly like the built-ins (`applyScaleAndRoot`'s existing
  `i % n` / `i / n` logic).
- Blank/short lines and unparseable intervals are skipped defensively.
- Up to 8 lines (slots) are honored; extra lines ignored.

Example value:
```
My Rast | 9/8, 347.4, 4/3, 3/2, 27/16, 1049.0
Harmonic 7 | 8/7, 5/4, 3/2, 7/4
```

Parsing runs in `setState()` (non-realtime) — never in `run()`.

## DSP (`SitarPlugin.cpp`, `CombFilter.hpp` untouched)

- Add a **user-scale table** parallel to `kScales[]`: up to 8 `ScaleDef`-shaped
  entries built from the parsed `userscales` string (label = name,
  `notesPerOctave` = interval count, `ratios[]` = parsed intervals with
  `ratios[0] = 1.0`).
- **Scale selection** (`kParamScale`): extend the enum range to `0 … 23+8`.
  Indices `0–23` → `kScales[]`; `24–31` → user slots `0–7`.
  `applyScaleAndRoot()` picks the active `ScaleDef` from whichever table the
  index falls in. An empty/undefined user slot → no populated strings (silent)
  until a scale is saved to it.
- Switching scale still only recomputes string frequencies; **all tone knobs
  are untouched** (unchanged from today — confirmed by the existing code path).
- Cents→ratio: `ratio = 2^(cents/1200)`; ratio `p/q` parsed directly.

## LV2 / DPF plumbing (`DistrhoPluginInfo.h`, Makefile/TTL)

- `DISTRHO_PLUGIN_WANT_STATE 1`. Implement `initState`, `getState`, `setState`.
- One host-writable state, key **`userscales`**, hints `kStateIsHostWritable`,
  default `""`. DPF exposes it as a `patch:writable` `atom:String` parameter
  (the channel the modgui writes/reads).
- **Remove the 48 per-string parameters** (`kParamString1 … +47`) from
  `initParameter`/`getParameterValue`/`setParameterValue` and the param enum.
  The scale/root/oct/num logic that fed them stays, now driving internal state
  only (no `requestParameterValueChange` for string ports).
- `modgui.ttl`: already carries no string ports (removed earlier). No new port
  knobs needed for the editor — it uses the patch/state channel.
- **Version bump** (VERSION) since the port layout changes.

## modgui (`icon-sitar.html`, `stylesheet-sitar.css`, `script-sitar.js`)

- **SCALE dropdown**: extend to list the 8 user slots after the built-ins,
  showing each slot's **actual name** (read from `userscales` via
  `funcs.patch_get`). Selecting a user entry sets the `scale` port to `24+slot`.
- **Scale editor overlay**: a small "✎ Scales" control on the face opens an
  absolutely-positioned panel over the pedal with: a slot picker (User 1–8), a
  **name** field, an **intervals** text field, and **Save** / **Clear**.
  - Save: the JS rebuilds the whole library string and
    `funcs.patch_set('<userscales-uri>', 's', text)`.
  - Open: `funcs.patch_get(...)` populates the slot list + editor.
- `script-sitar.js` gains the editor logic and the dynamic-dropdown population;
  the existing Scale/Root/Stereo bindings stay.

## Persistence semantics

- The `userscales` state + the `scale` selection are saved by MOD in the
  pedalboard and in MOD user-presets automatically. Reload restores both;
  switching scales retunes strings only.
- Custom scales are **per-pedalboard** in v1. To reuse a scale elsewhere: save a
  MOD plugin-preset (carries the state), or (deferred) export `.scl`.

## Breaking changes / compatibility

- Removing the `string_N` ports means any existing pedalboard/automation
  referencing them loses those references. Impact is low (they were derived and
  already hidden from the face), but it is a port-layout change → version bump,
  and the manual's current "advanced: per-string tuning in the gear view" note
  becomes obsolete and must be replaced by the custom-scales documentation.

## Risks / unknowns

1. **modgui ↔ DSP string round-trip (highest risk).** `patch_set`/`patch_get`
   for an arbitrary string on a DPF `patch:writable` String state is inferred
   from the MOD 0.0.12 code + the step-seq precedent but not yet proven for our
   setup. **De-risk first** (Phase 1) with a throwaway minimal string param
   before building the real editor.
2. **Persistence** of that state across pedalboard save/reload — verify on
   device.
3. **modgui overlay UX** on the compact pedal — verify it doesn't fight the
   board layout; iterate on device (render probe + MOD Desktop).
4. `.scl` custom-file-type support — only relevant to the deferred add-on.

## Implementation phases

1. **De-risk spike** — `WANT_STATE` + a `userscales` string state + a trivial
   modgui read/write (e.g. a text field that round-trips). Verify on MOD
   Desktop: set from UI → persists in pedalboard → reloads via `patch_get`.
   *Gate: if this doesn't work, stop and rethink the channel.*
2. **DSP: user-scale table + parser + selection** — TDD, host-less
   (`tests/`): parse the format (ratios + cents), build the table, select
   built-in vs user slot, retune, empty-slot behavior. No UI needed.
3. **Remove the 48 string ports** — internalize; adjust param enum + the three
   param methods; confirm build + existing DSP tests still pass.
4. **modgui editor + dynamic SCALE dropdown** — build the overlay + JS; verify
   on device (create → save → reload → select → hear correct tuning).
5. **Docs** — manual + README: replace the per-string-tuning material with the
   custom-scales workflow; regenerate screenshot/PDF.

## Verification

- **Host-less unit tests** (`make test`) for Phase 2: format parser
  (ratios/cents/garbage), user-slot selection, retune correctness (a known
  custom scale yields expected string Hz), empty-slot silence, and that
  switching scales leaves tone-knob state untouched.
- **On-device (MOD Desktop)** for Phases 1, 3, 4: state round-trip +
  persistence; port removal doesn't break loading; editor create/save/select;
  the dropdown shows user names; audible correctness.
- `make test` green throughout; `make` builds lv2+vst3+clap.

## Touched files (indicative)

```
plugins/Sitar/SitarPlugin.cpp            user-scale table, parser, state, selection, port removal
plugins/Sitar/DistrhoPluginInfo.h        WANT_STATE 1
plugins/Sitar/modgui/icon-sitar.html     editor overlay + trigger
plugins/Sitar/modgui/stylesheet-sitar.css  editor panel styling
plugins/Sitar/modgui/script-sitar.js     editor logic, patch_set/get, dynamic dropdown
plugins/Sitar/modgui.ttl                 (scale port range; no string ports)
tests/test_userscale_parse.cpp           new — parser + selection + retune
docs/manual/sitar-manual.html            replace per-string material; regen PDF
README.md                                document custom scales
VERSION                                  bump
```
