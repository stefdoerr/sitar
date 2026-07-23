# Custom user scales — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let users define/save their own microtonal scales (stored as plugin state, per-pedalboard) and remove the 48 per-string frequency ports so strings are fully internal — while keeping the 24 built-in scales.

**Architecture:** A self-contained parser (`UserScale.hpp`) turns the serialized `userscales` state string into up to 8 scale definitions. The DSP selects built-in *or* user scales through the existing `applyScaleAndRoot()` path (unchanged math). The MOD modgui edits the library via the already-validated `patch_set`/`patch_get` string channel (Phase 1 spike) and shows user scales in the SCALE dropdown. The 48 `string_N` LV2 ports are removed.

**Tech Stack:** C++14, DPF (state + patch), MOD modgui (HTML/CSS/JS), GNU Make.

**Spec:** `docs/superpowers/specs/2026-07-23-custom-user-scales-design.md`

## Status

- **Phase 1 (de-risk spike): DONE & validated on device.** `WANT_STATE` +
  `userscales` string state round-trips through the modgui and persists in the
  pedalboard (commit `5a955de`). The throwaway spike UI (a bottom text input in
  `icon-sitar.html`, its CSS, and the spike JS) is **replaced** by the real
  editor in Task 4.

## Global Constraints

- **Keep the 24 built-in scales** (`kScales[]`); user scales are additive.
- **Microtonal**: intervals are ratios (`p/q`) or cents (a number with a `.`),
  Scala-compatible (bare integer = ratio `n/1`). Tonic `1/1` is implicit.
- **8 fixed user slots.** SCALE selector: indices `0–23` built-ins, `24–31`
  user slots (`24+slot`).
- **Switching scale must not reset tone knobs** — only string tunings recompute
  (unchanged behavior; do not route scale changes through tone-knob state).
- **No per-string ports** after Task 3; strings are internal.
- **Patch parameter URI** (confirmed from generated TTL):
  `http://sitar.local/plugins/sitar#userscales`, `atom:String`, `patch:writable`.
  The modgui must discover it from the params list (not hardcode) so beta works.
- Parsing/state handling runs in `setState()` (non-realtime) — never in `run()`.
- License ISC; bundle name / plugin URI base / CLAP id / unique ids unchanged.

---

### Task 1: `UserScale.hpp` — the scale-library parser (TDD)

**Files:**
- Create: `plugins/Sitar/UserScale.hpp`
- Test: `tests/test_userscale_parse.cpp`

**Interfaces:**
- Produces (used by Task 2):
  - `struct sitar::UserScale { bool valid; char name[32]; uint32_t notesPerOctave; float ratios[12]; };`
  - `bool sitar::parseInterval(const char* tok, float& ratioOut)`
  - `void sitar::parseUserScales(const char* text, sitar::UserScale* out, uint32_t maxOut)`
    — line *i* fills slot *i* (positional, slot-stable); empty/`|`-less lines → `valid=false`.

- [ ] **Step 1: Write the failing test**

Create `tests/test_userscale_parse.cpp`:

```cpp
/*
 * Regression test: the user-scale library parser (UserScale.hpp).
 *   - parseInterval: ratios (p/q), cents (has '.'), bare int (n/1), garbage.
 *   - parseUserScales: positional slot fill, name trim, implicit/explicit
 *     tonic, cents->ratio, note-count and slot clamping, blank/malformed lines.
 *
 * Build (host-less, no DPF needed):
 *   g++ -std=gnu++14 -g -O0 -fsanitize=address -I../plugins/Sitar \
 *       test_userscale_parse.cpp -o test_userscale_parse && ./test_userscale_parse
 */
#include "UserScale.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

using namespace sitar;

static int gFail = 0;
static void near(const char* what, float a, float b)
{
    if (std::fabs(a - b) > 1e-3f) { std::printf("FAIL: %s: got %.5f want %.5f\n", what, a, b); ++gFail; }
}
static void eqi(const char* what, long a, long b)
{
    if (a != b) { std::printf("FAIL: %s: got %ld want %ld\n", what, a, b); ++gFail; }
}
static void istrue(const char* what, bool c)
{
    if (!c) { std::printf("FAIL: %s\n", what); ++gFail; }
}

int main()
{
    // ---- parseInterval ----
    float r = 0.0f;
    istrue("ratio parses",       parseInterval("3/2", r));    near("3/2", r, 1.5f);
    istrue("cents parses",       parseInterval("1200.0", r)); near("1200c=2.0", r, 2.0f);
    istrue("cents neutral 3rd",  parseInterval("347.4", r));  near("347.4c", r, std::pow(2.0f, 347.4f/1200.0f));
    istrue("bare int = n/1",     parseInterval("2", r));      near("2 -> 2/1", r, 2.0f);
    istrue("leading space ok",   parseInterval("  5/4", r));  near("sp 5/4", r, 1.25f);
    istrue("empty rejected",    !parseInterval("", r));
    istrue("garbage rejected",  !parseInterval("abc", r));
    istrue("zero denom rejected",!parseInterval("1/0", r));
    istrue("zero int rejected",  !parseInterval("0", r));

    // ---- parseUserScales ----
    UserScale s[8];

    // implicit tonic; ratios + cents mixed; name trimmed
    parseUserScales("  My Rast | 9/8, 347.4, 4/3, 3/2, 27/16, 1049.0  \n", s, 8);
    istrue("slot0 valid", s[0].valid);
    istrue("name trimmed", std::strcmp(s[0].name, "My Rast") == 0);
    eqi("rast notes = 7 (tonic+6)", s[0].notesPerOctave, 7);
    near("rast[0]=1", s[0].ratios[0], 1.0f);
    near("rast[1]=9/8", s[0].ratios[1], 1.125f);
    near("rast[4]=3/2", s[0].ratios[4], 1.5f);
    istrue("slot1 empty", !s[1].valid);

    // explicit tonic (1/1 first) must not double-count
    parseUserScales("Ex | 1/1, 5/4, 3/2\n", s, 8);
    eqi("explicit tonic -> 3 notes", s[0].notesPerOctave, 3);
    near("ex[0]=1", s[0].ratios[0], 1.0f);
    near("ex[1]=5/4", s[0].ratios[1], 1.25f);

    // positional slots: blank + malformed lines leave slots invalid
    parseUserScales("A | 9/8\n\nnobar line\nD | 6/5\n", s, 8);
    istrue("slot0 A valid", s[0].valid && std::strcmp(s[0].name, "A") == 0);
    istrue("slot1 blank invalid", !s[1].valid);
    istrue("slot2 nobar invalid", !s[2].valid);
    istrue("slot3 D valid", s[3].valid && std::strcmp(s[3].name, "D") == 0);

    // clamps: >12 notes and >8 slots
    parseUserScales("Big | 2,3,4,5,6,7,8,9,10,11,12,13,14,15\n", s, 8);
    istrue("notes clamped <= 12", s[0].notesPerOctave <= 12);
    parseUserScales("1|2\n2|2\n3|2\n4|2\n5|2\n6|2\n7|2\n8|2\n9|2\n", s, 8);
    istrue("slot7 valid", s[7].valid);  // 9th line ignored (maxOut=8)

    if (gFail) { std::printf("%d failure(s)\n", gFail); return 1; }
    std::printf("ok: user-scale parser (intervals, slots, tonic, cents, clamps)\n");
    return 0;
}
```

- [ ] **Step 2: Run it, verify it FAILS (no header yet)**

Run (from repo root):
```bash
mkdir -p build/tests && \
g++ -std=gnu++14 -g -O0 -fsanitize=address -Iplugins/Sitar \
    tests/test_userscale_parse.cpp -o build/tests/test_userscale_parse
```
Expected: FAIL to compile — `UserScale.hpp: No such file`.

- [ ] **Step 3: Write `plugins/Sitar/UserScale.hpp`**

```cpp
/*
 * Sitar — user-defined scale library parser.
 *
 * Turns the serialized "userscales" state string into up to 8 scale slots.
 * Format: one scale per line, "Name | i1, i2, ...". Line i fills slot i
 * (positional, so slot indices are stable across edits). Intervals are:
 *   p/q            -> ratio p/q
 *   number with .  -> cents, ratio = 2^(cents/1200)   (Scala convention)
 *   bare integer   -> ratio n/1                        (Scala convention)
 * The tonic 1/1 is implicit (ratios[0]); a listed value ~1.0 is not doubled.
 * Header-only so both the plugin and the host-less tests link it directly.
 */
#ifndef SITAR_USER_SCALE_HPP_INCLUDED
#define SITAR_USER_SCALE_HPP_INCLUDED

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace sitar {

static constexpr uint32_t kMaxUserScales    = 8;
static constexpr uint32_t kMaxScaleNotes    = 12;   // ratios[] capacity
static constexpr uint32_t kUserScaleNameCap = 32;

struct UserScale {
    bool     valid;
    char     name[kUserScaleNameCap];
    uint32_t notesPerOctave;                 // >= 1; ratios[0..notesPerOctave-1]
    float    ratios[kMaxScaleNotes];         // ratios[0] == 1.0 (tonic)
};

// Parse one interval token to a frequency ratio (> 0). Returns false and
// leaves ratioOut untouched for empty / unparseable / non-positive tokens.
inline bool parseInterval(const char* tok, float& ratioOut)
{
    while (*tok == ' ' || *tok == '\t') ++tok;
    if (*tok == '\0') return false;

    if (const char* slash = std::strchr(tok, '/'))
    {
        const double num = std::atof(tok);
        const double den = std::atof(slash + 1);
        if (num <= 0.0 || den <= 0.0) return false;
        ratioOut = static_cast<float>(num / den);
        return true;
    }
    if (std::strchr(tok, '.') != nullptr)              // cents
    {
        const double cents = std::atof(tok);
        const float  ratio = static_cast<float>(std::pow(2.0, cents / 1200.0));
        if (ratio <= 0.0f) return false;
        ratioOut = ratio;
        return true;
    }
    const double n = std::atof(tok);                   // bare int -> n/1
    if (n <= 0.0) return false;
    ratioOut = static_cast<float>(n);
    return true;
}

// Parse the library string into out[0..maxOut-1] positionally (line i -> slot
// i). Slots whose line has no name or no '|' are marked invalid.
inline void parseUserScales(const char* text, UserScale* out, uint32_t maxOut)
{
    for (uint32_t i = 0; i < maxOut; ++i) { out[i].valid = false; out[i].name[0] = '\0'; out[i].notesPerOctave = 0; }
    if (text == nullptr) return;

    const char* line = text;
    uint32_t slot = 0;
    while (*line != '\0' && slot < maxOut)
    {
        const char* eol     = std::strchr(line, '\n');
        const char* lineEnd = (eol != nullptr) ? eol : (line + std::strlen(line));

        const char* bar = nullptr;
        for (const char* c = line; c < lineEnd; ++c) { if (*c == '|') { bar = c; break; } }

        if (bar != nullptr)
        {
            UserScale& s = out[slot];

            const char* nb = line;
            const char* ne = bar;
            while (nb < ne && (*nb == ' ' || *nb == '\t')) ++nb;
            while (ne > nb && (*(ne - 1) == ' ' || *(ne - 1) == '\t')) --ne;
            uint32_t ni = 0;
            for (const char* c = nb; c < ne && ni < kUserScaleNameCap - 1; ++c) s.name[ni++] = *c;
            s.name[ni] = '\0';

            s.ratios[0]      = 1.0f;
            s.notesPerOctave = 1;

            char buf[64];
            const char* p = bar + 1;
            while (p < lineEnd && s.notesPerOctave < kMaxScaleNotes)
            {
                const char* comma = p;
                while (comma < lineEnd && *comma != ',') ++comma;
                uint32_t bi = 0;
                for (const char* c = p; c < comma && bi < sizeof(buf) - 1; ++c) buf[bi++] = *c;
                buf[bi] = '\0';

                float ratio;
                if (parseInterval(buf, ratio) && std::fabs(ratio - 1.0f) > 1e-4f)
                    s.ratios[s.notesPerOctave++] = ratio;

                p = (comma < lineEnd) ? comma + 1 : lineEnd;
            }

            s.valid = (s.name[0] != '\0');
        }

        ++slot;
        if (eol == nullptr) break;
        line = eol + 1;
    }
}

} // namespace sitar

#endif // SITAR_USER_SCALE_HPP_INCLUDED
```

- [ ] **Step 4: Run the test, verify it PASSES**

```bash
g++ -std=gnu++14 -g -O0 -fsanitize=address -Iplugins/Sitar \
    tests/test_userscale_parse.cpp -o build/tests/test_userscale_parse && \
./build/tests/test_userscale_parse
```
Expected: PASS — `ok: user-scale parser (...)`.

- [ ] **Step 5: Full suite**

Run: `make test` → `==> all tests passed`.

- [ ] **Step 6: Commit**

```bash
git add plugins/Sitar/UserScale.hpp tests/test_userscale_parse.cpp
git commit -m "feat: user-scale library parser (UserScale.hpp) + tests"
```

---

### Task 2: Wire user scales into the DSP (table, selection, apply, live edit)

**Files:**
- Modify: `plugins/Sitar/SitarPlugin.cpp`
- Test: `tests/test_userscale_apply.cpp`

**Interfaces:**
- Consumes: `sitar::UserScale`, `sitar::parseUserScales`, `sitar::kMaxUserScales` (Task 1).
- Produces: SCALE param range `0..(kNumScales-1 + kMaxUserScales)`; a private
  `const ScaleDef* activeScaleDef() const` returning the built-in or a
  synthesized user `ScaleDef`; `setState` rebuilds the table and retunes when
  the active slot is edited.

- [ ] **Step 1: Write the failing test**

Create `tests/test_userscale_apply.cpp`:

```cpp
/*
 * Regression test: selecting a user scale tunes the strings from the parsed
 * library, and editing the active user scale retunes live. Observed through
 * AUDIO (resonance RMS) so it survives the later removal of the string ports.
 *
 * Build:
 *   g++ -std=gnu++14 -g -O0 -fsanitize=address -Iplugins/Sitar -Idpf/distrho \
 *       test_userscale_apply.cpp ../plugins/Sitar/SitarPlugin.cpp \
 *       ../dpf/distrho/src/DistrhoPlugin.cpp -o test_userscale_apply
 */
#include "src/DistrhoPluginInternal.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

USE_NAMESPACE_DISTRHO

static bool cb(void*, uint32_t, float) { return true; }

// Drive `blocks` of a sine at `freq` and return output RMS.
static float feed(PluginExporter& p, float freq, float amp, uint32_t blocks, uint64_t& clk)
{
    const uint32_t B = 512; const double w = 2.0 * M_PI * freq / 48000.0;
    std::vector<float> in(B), l(B), r(B);
    const float* ins[1] = { in.data() }; float* outs[2] = { l.data(), r.data() };
    double sum = 0.0;
    for (uint32_t b = 0; b < blocks; ++b) {
        for (uint32_t i = 0; i < B; ++i) in[i] = amp * (float) std::sin(w * (double) clk++);
        p.run(ins, outs, B);
        for (uint32_t i = 0; i < B; ++i) sum += (double) l[i] * l[i];
    }
    return (float) std::sqrt(sum / (blocks * B));
}

// Param indices are resolved by symbol so this test is robust to the port
// renumbering in Task 3.
static uint32_t idx(PluginExporter& p, const char* sym)
{
    for (uint32_t i = 0; i < p.getParameterCount(); ++i)
        if (std::strcmp(p.getParameterSymbol(i), sym) == 0) return i;
    std::printf("FAIL: no param symbol '%s'\n", sym); return 0;
}

int main()
{
    d_nextBufferSize = 512; d_nextSampleRate = 48000.0; d_nextCanRequestParameterValueChanges = true;
    PluginExporter p(nullptr, nullptr, cb, nullptr);

    const uint32_t SCALE = idx(p, "scale"), ROOT = idx(p, "root_note"),
                   OCT = idx(p, "octave"), MIX = idx(p, "mix"), GATE = idx(p, "gate"),
                   NUM = idx(p, "num_active");
    int fail = 0;
    uint64_t clk = 0;

    // A user scale whose tonic (1/1) at ROOT=C, OCT=3 is 130.81 Hz.
    p.setState("userscales", "Mine | 3/2\n");
    p.setParameterValue(ROOT, 0.0f);      // C
    p.setParameterValue(OCT,  3.0f);
    p.setParameterValue(MIX,  1.0f);      // all wet
    p.setParameterValue(GATE, 0.0f);      // gate off
    p.setParameterValue(NUM,  13.0f);
    p.setParameterValue(SCALE, 24.0f);    // user slot 0
    p.activate();

    // String 1 (tonic) should ring at 130.81 Hz.
    const float ring = feed(p, 130.81f, 0.02f, 120, clk);
    if (ring < 0.03f) { std::printf("FAIL: user-scale tonic not resonating (rms %.5f)\n", ring); ++fail; }

    // Its second degree (3/2) -> 196.22 Hz should also ring.
    const float fifth = feed(p, 130.81f * 1.5f, 0.02f, 120, clk);
    if (fifth < 0.03f) { std::printf("FAIL: user-scale 3/2 degree not resonating (rms %.5f)\n", fifth); ++fail; }

    // Selecting an EMPTY user slot -> silence (no populated strings).
    p.setParameterValue(SCALE, 25.0f);    // user slot 1 (empty)
    p.activate();
    const float silent = feed(p, 130.81f, 0.02f, 60, clk);
    if (silent > 0.02f) { std::printf("FAIL: empty user slot still resonates (rms %.5f)\n", silent); ++fail; }

    if (fail) return 1;
    std::printf("ok: user scale applies + retunes; empty slot is silent\n");
    return 0;
}
```

- [ ] **Step 2: Run it, verify it FAILS**

```bash
mkdir -p build/tests && \
g++ -std=gnu++14 -g -O0 -fsanitize=address -Iplugins/Sitar -Idpf/distrho \
    tests/test_userscale_apply.cpp plugins/Sitar/SitarPlugin.cpp \
    dpf/distrho/src/DistrhoPlugin.cpp -o build/tests/test_userscale_apply && \
./build/tests/test_userscale_apply
```
Expected: FAIL — SCALE clamps to 23 (max built-in) so slot 24 never selects the user scale; the tonic won't be at 130.81 unless the user scale is applied.

- [ ] **Step 3: Include the parser + add the user-scale table**

In `SitarPlugin.cpp`, add near the top includes:
```cpp
#include "UserScale.hpp"
```
Add members (next to `String fUserScales;` from Phase 1):
```cpp
    // Parsed user-scale library (from the "userscales" state). Slot i is
    // selectable as SCALE index kNumScales + i.
    sitar::UserScale fUserScaleTable[sitar::kMaxUserScales] {};
```

- [ ] **Step 4: Add an active-scale accessor + a user-slot ScaleDef synthesizer**

Add these private helpers (near `applyScaleAndRoot`):
```cpp
    // Total selectable scales = built-ins + user slots.
    static constexpr uint32_t kNumSelectableScales = kNumScales + sitar::kMaxUserScales;

    // Scratch ScaleDef for the active user slot (label unused by the DSP math).
    ScaleDef fUserScaleDef {};

    // Returns the ScaleDef for the current fScaleIdx, or nullptr if it points
    // at an empty user slot (→ no populated strings).
    const ScaleDef* activeScaleDef()
    {
        if (fScaleIdx < kNumScales)
            return &kScales[fScaleIdx];

        const uint32_t slot = fScaleIdx - kNumScales;
        if (slot >= sitar::kMaxUserScales || !fUserScaleTable[slot].valid)
            return nullptr;

        const sitar::UserScale& u = fUserScaleTable[slot];
        fUserScaleDef.label          = "User";
        fUserScaleDef.notesPerOctave = u.notesPerOctave;
        for (uint32_t i = 0; i < u.notesPerOctave && i < 12; ++i)
            fUserScaleDef.ratios[i] = u.ratios[i];
        return &fUserScaleDef;
    }
```

- [ ] **Step 5: Use the active scale in `applyScaleAndRoot()`**

In `applyScaleAndRoot()`, replace the two lines that read `kScales[fScaleIdx]`:
```cpp
        const ScaleDef& scale  = kScales[fScaleIdx];
        ...
        const uint32_t  n      = scale.notesPerOctave;
```
with:
```cpp
        const ScaleDef* activeScale = activeScaleDef();
        // Empty user slot → nothing rings until the user saves a scale to it.
        const uint32_t  n = (activeScale != nullptr) ? activeScale->notesPerOctave : 0;
```
and change the per-string frequency line to guard on `activeScale`:
```cpp
            if (i < effective && activeScale != nullptr)
            {
                const uint32_t scaleIdx = i % n;
                const uint32_t octaves  = i / n;
                freq = rootHz * activeScale->ratios[scaleIdx]
                              * std::pow(2.0f, static_cast<float>(octaves))
                              * octMul;
                ...
            }
```
(`rebuildPanTable()` reads `kScales[fScaleIdx].notesPerOctave`; make it use
`activeScaleDef()` the same way — `n = active ? active->notesPerOctave : 0`.)

- [ ] **Step 6: Extend the SCALE param range + clamp**

In `initParameter` `case kParamScale`, change the max and the enum count so user
slots are selectable:
```cpp
            parameter.ranges.max = static_cast<float>(kNumSelectableScales - 1);
            parameter.enumValues.count = static_cast<uint8_t>(kNumSelectableScales);
            ParameterEnumerationValue* const ev = new ParameterEnumerationValue[kNumSelectableScales];
            for (uint32_t i = 0; i < kNumScales; ++i) { ev[i].value = (float) i; ev[i].label = kScales[i].label; }
            static const char* const kUserLabels[8] =
                { "User 1","User 2","User 3","User 4","User 5","User 6","User 7","User 8" };
            for (uint32_t i = 0; i < sitar::kMaxUserScales; ++i)
                { ev[kNumScales+i].value = (float)(kNumScales+i); ev[kNumScales+i].label = kUserLabels[i]; }
```
In `setParameterValue` `case kParamScale`, clamp to `kNumSelectableScales - 1`
instead of `kNumScales - 1`.

- [ ] **Step 7: Parse the library + retune on live edit in `setState`**

Replace the Phase-1 `setState` body:
```cpp
    void setState(const char* key, const char* value) override
    {
        if (std::strcmp(key, "userscales") == 0)
        {
            fUserScales = value;
            sitar::parseUserScales(fUserScales, fUserScaleTable, sitar::kMaxUserScales);
            // If a user slot is the active scale, re-apply so an edit is heard.
            if (fScaleIdx >= kNumScales)
                applyScaleAndRoot(/*notifyHost=*/ true);
        }
    }
```

- [ ] **Step 8: Run the apply test → PASS, then full suite**

```bash
g++ -std=gnu++14 -g -O0 -fsanitize=address -Iplugins/Sitar -Idpf/distrho \
    tests/test_userscale_apply.cpp plugins/Sitar/SitarPlugin.cpp \
    dpf/distrho/src/DistrhoPlugin.cpp -o build/tests/test_userscale_apply && \
./build/tests/test_userscale_apply
make test
```
Expected: apply test PASS; `==> all tests passed`. Also `make >/dev/null && echo OK` builds.

- [ ] **Step 9: Commit**

```bash
git add plugins/Sitar/SitarPlugin.cpp tests/test_userscale_apply.cpp
git commit -m "feat: select + apply user scales (SCALE slots 24-31); retune on state edit"
```

---

### Task 3: Remove the 48 per-string ports (strings fully internal)

**Files:**
- Modify: `plugins/Sitar/SitarPlugin.cpp`
- Modify/replace: `tests/test_string_zero_clear.cpp`, `tests/test_stereo_mode_detune.cpp`
  (they exercise per-string ports that no longer exist)

**Interfaces:**
- Consumes: Task 2's DSP.
- Produces: param enum starts at `num_active` (index 0); `kNumParams` drops by
  48; no `string_N` ports; `applyScaleAndRoot`/`setParameterValue` no longer
  call `requestParameterValueChange` for strings.

- [ ] **Step 1: Remove string params from the enum**

In `enum ParamIndex`, remove `kParamString1` and the "up to +47" comment; make
`kParamNumActive = 0` the first entry (the rest follow). `kNumStrings` (the
DSP's internal string count = 48) **stays** — only the *parameters* go.

- [ ] **Step 2: Drop the `index < kNumStrings` branches**

In `initParameter`, `getParameterValue`, and `setParameterValue`, delete the
leading `if (index < kNumStrings) { … }` blocks (the per-string port handling).
The `switch (index)` on the remaining params stays.

- [ ] **Step 3: Stop notifying string ports**

In `applyScaleAndRoot`, delete `if (notifyHost) requestParameterValueChange(i, freq);`
(no string ports to notify). Keep computing `fStringFreqs[]`/`fStringActive[]`
internally. The `notifyHost` parameter becomes unused — remove it and its call
sites (all callers pass a bool), or keep the signature and ignore it; simplest
is to drop the parameter.

- [ ] **Step 4: Retire the two obsolete tests**

`test_string_zero_clear.cpp` and `test_stereo_mode_detune.cpp` drive
`setParameterValue(0/2, …)` / `getParameterValue` on string ports that no
longer exist. Delete both files (the behavior they covered — per-string port
writes/detune — is removed by design). `test_userscale_apply` now covers scale
application; `test_combfilter_oob`, `test_gate_release_smooth`,
`test_jawari_gain`, `test_brand_identity`, `test_userscale_parse` remain.

```bash
git rm tests/test_string_zero_clear.cpp tests/test_stereo_mode_detune.cpp
```

- [ ] **Step 5: Build + full suite + confirm no string ports**

```bash
make >/dev/null && echo BUILD_OK
make test
grep -c 'lv2:symbol "string_' bin/sitar.lv2/sitar.ttl   # expect 0
```
Expected: builds; remaining tests pass; zero `string_` ports in the generated TTL.

- [ ] **Step 6: Commit**

```bash
git add -A plugins/Sitar/SitarPlugin.cpp tests/
git commit -m "feat: remove the 48 per-string ports; strings are now fully internal"
```

---

### Task 4: modgui scale editor + dynamic SCALE dropdown (replaces the spike)

**Files:**
- Modify: `plugins/Sitar/modgui/icon-sitar.html`
- Modify: `plugins/Sitar/modgui/stylesheet-sitar.css`
- Modify: `plugins/Sitar/modgui/script-sitar.js`

**Interfaces:**
- Consumes: the `userscales` patch String param (discovered from the params
  list); the `scale` port (0–23 built-ins, 24–31 user slots).
- Produces: an editor overlay + user entries in the SCALE dropdown.

> This task is UI and is verified on device (like the modgui redesign). The
> code below is a complete first cut; expect to tune spacing/behavior against
> MOD Desktop. Build with `make`, install, and iterate.

- [ ] **Step 1: HTML — remove the spike input; add the editor overlay + trigger**

In `icon-sitar.html`, delete the Phase-1 spike `<input class="sitar-spike-input" …>`
block. Add a trigger in the selector bar (after the TEST field) and an overlay
before the Audio I/O block:

```html
        <div class="sitar-field">
            <span class="sitar-field-label">SCALES</span>
            <div class="sitar-editbtn" mod-role="sitar-edit-open" title="Edit user scales">&#9998;</div>
        </div>
```
```html
    <!-- User-scale editor overlay (hidden until opened) -->
    <div class="sitar-editor" mod-role="sitar-editor" style="display:none;">
        <div class="sitar-editor-row">
            <span class="sitar-editor-title">USER SCALES</span>
            <div class="sitar-editor-close" mod-role="sitar-edit-close">&#10005;</div>
        </div>
        <div class="sitar-editor-row">
            <select class="sitar-select" mod-role="sitar-edit-slot"></select>
            <input class="sitar-editor-name" mod-role="sitar-edit-name" type="text" placeholder="scale name" />
        </div>
        <input class="sitar-editor-ivals" mod-role="sitar-edit-ivals" type="text"
               placeholder="intervals: 9/8, 347.4, 4/3, 3/2 …  (ratios or cents)" />
        <div class="sitar-editor-row">
            <div class="sitar-editor-btn" mod-role="sitar-edit-save">Save</div>
            <div class="sitar-editor-btn" mod-role="sitar-edit-clear">Clear slot</div>
        </div>
    </div>
```

- [ ] **Step 2: CSS — remove the spike rule; add editor styling**

In `stylesheet-sitar.css`, delete the `.sitar-spike-input` rule. Add:

```css
/* User-scale editor trigger + overlay */
.sitar-pedal .sitar-editbtn {
    width: 26px; height: 26px; border-radius: 5px;
    background: linear-gradient(180deg, #4a2a18, #281407);
    border: 1px solid rgba(0,0,0,0.7); color: #f5d99a;
    display: flex; align-items: center; justify-content: center;
    cursor: pointer; font-size: 13px;
    box-shadow: inset 0 1px 2px rgba(255,200,130,0.25), 0 1px 2px rgba(0,0,0,0.55);
}
.sitar-pedal .sitar-editor {
    position: absolute; top: 44px; left: 40px; right: 40px; z-index: 3;
    padding: 10px 12px; border-radius: 8px;
    background: linear-gradient(180deg, #2c170c, #1a0d05);
    border: 1px solid rgba(245,217,154,0.35);
    box-shadow: 0 6px 18px rgba(0,0,0,0.6);
    display: flex; flex-direction: column; gap: 8px;
}
.sitar-pedal .sitar-editor-row { display: flex; align-items: center; gap: 8px; justify-content: space-between; }
.sitar-pedal .sitar-editor-title { font-size: 9px; letter-spacing: 0.2em; color: rgba(245,217,154,0.7); }
.sitar-pedal .sitar-editor-close { cursor: pointer; color: #f5d99a; font-size: 12px; }
.sitar-pedal .sitar-editor-name,
.sitar-pedal .sitar-editor-ivals {
    background: #1c0e07; color: #f5d99a; border: 1px solid rgba(245,217,154,0.35);
    border-radius: 4px; padding: 3px 6px; font-size: 11px; font-family: inherit; outline: none;
}
.sitar-pedal .sitar-editor-name { flex: 1; }
.sitar-pedal .sitar-editor-ivals { width: 100%; box-sizing: border-box; }
.sitar-pedal .sitar-editor-btn {
    background: linear-gradient(180deg, #4a2a18, #281407); color: #f5d99a;
    border: 1px solid rgba(0,0,0,0.7); border-radius: 4px; padding: 4px 12px;
    font-size: 11px; cursor: pointer;
}
```

- [ ] **Step 3: JS — replace spike code with editor logic + dynamic dropdown**

In `script-sitar.js`, remove the Phase-1 spike blocks (the `$spike` wiring in
`start` and the `#userscales` branch in `change`). Add the editor. Key pieces
(full handler in the built bundle):

```js
    // --- user-scale library helpers (8 positional slots) ---
    var NUSER = 8, BUILTINS = SCALE_KEYS.length;

    function parseLibrary(text) {           // -> array[8] of {name, ivals} or null
        var slots = new Array(NUSER); for (var i=0;i<NUSER;i++) slots[i]=null;
        (text||'').split('\n').forEach(function(line, i){
            if (i>=NUSER) return; var bar=line.indexOf('|'); if (bar<0) return;
            var name=line.slice(0,bar).trim(); if (!name) return;
            slots[i]={name:name, ivals:line.slice(bar+1).trim()};
        });
        return slots;
    }
    function serializeLibrary(slots) {
        return slots.map(function(s){ return s ? (s.name+' | '+s.ivals) : ''; })
                    .join('\n').replace(/\n+$/,'');
    }
    function refreshScaleDropdown(icon, slots) {
        var $scale = icon.find('[mod-role="sitar-scale"]');
        $scale.find('optgroup[label="User"], option.sitar-user-opt').remove();
        var $g = jQuery('<optgroup class="sitar-user-grp" label="User"></optgroup>');
        for (var i=0;i<NUSER;i++) if (slots[i])
            $g.append('<option class="sitar-user-opt" value="user'+i+'">'+slots[i].name+'</option>');
        if ($g.children().length) $scale.append($g);
    }
```
- In `start`: discover the `userscales` URI from `event.parameters`, store on
  `icon`, `parseLibrary` the current value → `icon.data('sitar-lib', slots)`,
  populate the slot `<select>` (User 1–8) and `refreshScaleDropdown`. Wire:
  open/close overlay; slot-select fills name/ivals fields; **Save** updates the
  slot in the lib, `funcs.patch_set(uri,'s',serializeLibrary(lib))`, refreshes
  the dropdown; **Clear** nulls the slot and patch_sets.
- Extend `scaleKeyToIndex`: a `user<i>` value → `BUILTINS + i`; the `change`
  handler for `scale` maps index `>=BUILTINS` back to `user<idx-BUILTINS>` and
  selects it (or shows the built-in otherwise).
- In `change` with `event.uri` ending `#userscales`: re-`parseLibrary` and
  refresh the dropdown + editor (external/preset recall).

- [ ] **Step 4: Build + static check**

```bash
make >/dev/null && echo BUILD_OK
! grep -q 'sitar-spike' bin/sitar.lv2/modgui/icon-sitar.html && echo "spike removed"
grep -q 'sitar-editor' bin/sitar.lv2/modgui/icon-sitar.html && echo "editor present"
```

- [ ] **Step 5: On-device verification (MOD Desktop)**

`./install.sh`, restart. Confirm: the ✎ opens the editor; entering a name +
`9/8, 347.4, 4/3, 3/2` and Save makes "User 1: <name>" appear in the SCALE
dropdown; selecting it retunes (TEST plays the custom tuning); tone knobs
unaffected; save + reload pedalboard restores the scale and selection; Clear
removes a slot. (Render-probe the overlay first if layout needs tuning.)

- [ ] **Step 6: Commit**

```bash
git add plugins/Sitar/modgui/icon-sitar.html plugins/Sitar/modgui/stylesheet-sitar.css plugins/Sitar/modgui/script-sitar.js
git commit -m "feat: modgui user-scale editor + user scales in the SCALE dropdown"
```

---

### Task 5: Docs + regenerated assets + version bump

**Files:**
- Modify: `docs/manual/sitar-manual.html` (+ regen `sitar-manual.pdf`)
- Modify: `README.md`
- Modify: `VERSION`
- Regen: `plugins/Sitar/modgui/screenshot-sitar.png`

- [ ] **Step 1: Manual** — replace the "Per-string tuning is advanced" tip and
  the "My per-string tuning disappeared" FAQ (added earlier) with custom-scales
  content: how to open the editor, the interval format (ratios/cents, tonic
  implicit), that scales save with the pedalboard, and that SCALE/ROOT/OCT
  changes retune. Add a short "Custom scales" section near the controls.

- [ ] **Step 2: README** — add a "Custom scales" subsection under Features:
  define your own microtonal scales in the modgui editor (ratios or cents),
  saved per-pedalboard; note the 48 string ports are gone (strings internal).

- [ ] **Step 3: VERSION** — bump (port layout changed). e.g. `0.2.0`.

- [ ] **Step 4: Regenerate assets** (browser tooling — see prior session):
```bash
uv run --with 'playwright==1.59.0' python plugins/Sitar/modgui/render_screenshot.py
make manual
```
(If the editor overlay should appear in the screenshot, that's optional — the
default render shows the closed face.) Commit the regenerated PNG/PDF only if
regenerated this run.

- [ ] **Step 5: Commit**

```bash
git add docs/manual/sitar-manual.html docs/manual/sitar-manual.pdf README.md VERSION plugins/Sitar/modgui/screenshot-sitar.png
git commit -m "docs: document custom scales; note internal strings; regen assets; bump version"
```

---

## Notes for the whole plan

- **Order:** 1 → 2 → 3 → 4 → 5. Tasks 1–3 are host-less testable; 4 needs
  on-device verification; 5 needs the browser tooling.
- **Phase 1 spike** is already on this branch (`custom-user-scales`, `5a955de`);
  Task 4 removes its throwaway UI.
- Every task is one commit; `git revert` any independently.
- `make test` stays green after Tasks 1, 2, 3 (with the test set updated in 3).
