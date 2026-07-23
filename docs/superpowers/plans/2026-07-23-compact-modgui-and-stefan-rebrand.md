# Compact modgui + Stefan rebrand — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Simplify the Sympathetic Sitar MOD pedal face (drop the 13 per-string knobs; regroup the rest into a compact captioned knob row) and change the vendor brand from `sitar` to `Stefan`, matching `../boreas`.

**Architecture:** Pure modgui (HTML/CSS/TTL) + plugin-identity changes. The DSP (`SitarPlugin.cpp` `run()` and everything it calls) is untouched; only `getLabel()` changes. The 48 string LV2 ports stay — they're just removed from the custom pedal face, remaining editable in MOD's gear view (generated from `sitar.ttl`, not `modgui.ttl`).

**Tech Stack:** DPF (DISTRHO Plugin Framework), LV2 modgui (Mustache HTML + CSS + jQuery-ish JS), C++14, GNU Make, Playwright/Chromium (screenshot), headless Chrome (manual PDF).

## Global Constraints

- **No DSP changes.** Do not touch `run()`, `CombFilter.hpp`, or any DSP math.
- **No port / parameter / symbol changes.** `num_active` keeps its symbol and parameter name `"Active Strings"`. Presets, saved pedalboards, and automation must keep working. The "Strings" rename is the on-face knob *title* only.
- **Brand string is exactly `Stefan`.** The plugin NAME stays `Sitar` / `Sitar (Beta)`. Bundle name (`sitar.lv2`), LV2 URI (`http://sitar.local/plugins/sitar`), CLAP id, `DISTRHO_PLUGIN_BRAND_ID`, `DISTRHO_PLUGIN_UNIQUE_ID`, and install paths are all unchanged.
- **License stays `ISC`.**
- **Keep the selector `mod-role` hooks** (`sitar-scale`, `sitar-root`, `sitar-stereo`) so `plugins/Sitar/modgui/script-sitar.js` needs zero changes.
- **Pedal height is a single shared value** used in BOTH `stylesheet-sitar.css` (`.sitar-pedal { height }`) and `render_screenshot.py` (`HEIGHT`). Start at **220px**; if on-device the lower stereo output jack overhangs the frame, raise both to ~245px in lockstep.

---

### Task 1: Plugin identity — brand=Stefan, label=Sitar (C++)

**Files:**
- Test: `tests/test_brand_identity.cpp` (create)
- Modify: `plugins/Sitar/DistrhoPluginInfo.h` (both `DISTRHO_PLUGIN_BRAND` defines)
- Modify: `plugins/Sitar/SitarPlugin.cpp` (`getLabel()`)

**Interfaces:**
- Consumes: DPF `PluginExporter` (from `dpf/distrho/src/DistrhoPluginInternal.hpp`), which exposes `const char* getMaker()` and `const char* getLabel()` forwarding to the plugin's overrides.
- Produces: `getMaker()` → `"Stefan"`, `getLabel()` → `"Sitar"`. (`getMaker()` returns `DISTRHO_PLUGIN_BRAND`; `getLabel()` returns `DISTRHO_PLUGIN_NAME`.)

- [ ] **Step 1: Write the failing test**

Create `tests/test_brand_identity.cpp`:

```cpp
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
```

- [ ] **Step 2: Run the test, verify it FAILS**

Run (from repo root):

```bash
mkdir -p build/tests && \
g++ -std=gnu++14 -g -O0 -fsanitize=address -Iplugins/Sitar -Idpf/distrho \
    tests/test_brand_identity.cpp plugins/Sitar/SitarPlugin.cpp \
    dpf/distrho/src/DistrhoPlugin.cpp -o build/tests/test_brand_identity && \
./build/tests/test_brand_identity
```

Expected: FAIL — prints `getMaker() = "sitar"` and `getLabel() = "sitar"` and exits 1 (both still return the old brand).

- [ ] **Step 3: Change the brand define**

In `plugins/Sitar/DistrhoPluginInfo.h`, change `DISTRHO_PLUGIN_BRAND` in **both** branches:

```c
#ifdef SITAR_BETA
#define DISTRHO_PLUGIN_BRAND   "Stefan"
#define DISTRHO_PLUGIN_NAME    "Sitar (Beta)"
...
#else
#define DISTRHO_PLUGIN_BRAND   "Stefan"
#define DISTRHO_PLUGIN_NAME    "Sitar"
...
#endif
```

(Only the two `DISTRHO_PLUGIN_BRAND` lines change from `"sitar"` → `"Stefan"`; leave `DISTRHO_PLUGIN_NAME`, URIs, CLAP ids, brand/unique ids as-is.)

- [ ] **Step 4: Point getLabel() at the plugin name**

In `plugins/Sitar/SitarPlugin.cpp`, the `getLabel()` override currently returns the brand. Change it to return the name:

```cpp
    const char* getLabel()       const override { return DISTRHO_PLUGIN_NAME; }
```

Leave `getMaker()` as `return DISTRHO_PLUGIN_BRAND;` (now "Stefan").

- [ ] **Step 5: Run the test, verify it PASSES**

```bash
g++ -std=gnu++14 -g -O0 -fsanitize=address -Iplugins/Sitar -Idpf/distrho \
    tests/test_brand_identity.cpp plugins/Sitar/SitarPlugin.cpp \
    dpf/distrho/src/DistrhoPlugin.cpp -o build/tests/test_brand_identity && \
./build/tests/test_brand_identity
```

Expected: PASS — `ok: getMaker()=Stefan, getLabel()=Sitar`.

- [ ] **Step 6: Run the full suite, verify nothing else broke**

Run: `make test`
Expected: `==> all tests passed` (6 tests now, including the new one).

- [ ] **Step 7: Commit**

```bash
git add tests/test_brand_identity.cpp plugins/Sitar/DistrhoPluginInfo.h plugins/Sitar/SitarPlugin.cpp
git commit -m "Rebrand vendor to Stefan (getMaker); keep plugin name Sitar (getLabel)

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 2: Brand in the build config + modgui.ttl

**Files:**
- Modify: `Makefile` (`BRAND` value; drop the `modgui:brand` sed substitution)
- Modify: `plugins/Sitar/modgui.ttl` (`modgui:brand` line)

**Interfaces:**
- Consumes: nothing from Task 1.
- Produces: built bundle `modgui.ttl` carries `modgui:brand "Stefan"` for BOTH stable and beta builds (brand no longer varies stable↔beta).

- [ ] **Step 1: Change BRAND in the Makefile**

In `Makefile`, the identity block:

```make
BRAND           := Stefan
```

(was `BRAND := sitar`; leave `PLUGIN := sitar`, `LABEL := Sympathetic Sitar`, `PLUGIN_URI_BASE` unchanged.)

- [ ] **Step 2: Drop the modgui:brand substitution**

In `Makefile`, the `modgui:` target has a three-line `sed`. Remove the middle line so the brand is emitted verbatim (it's the same "Stefan" for stable and beta now). It becomes:

```make
	sed -e 's|$(PLUGIN_URI_BASE)/$(PLUGIN)|$(PLUGIN_URI)|g' \
	    -e 's|modgui:label "$(LABEL)"|modgui:label "$(BUNDLE_LABEL)"|' \
	    $(PLUGIN_DIR)/modgui.ttl > $(BUNDLE)/modgui.ttl
```

(Delete the line `-e 's|modgui:brand "$(BRAND)"|modgui:brand "$(BUNDLE_NAME)"|' \`.)

- [ ] **Step 3: Set the brand in the source modgui.ttl**

In `plugins/Sitar/modgui.ttl`:

```ttl
        modgui:brand "Stefan" ;
```

(was `modgui:brand "sitar" ;`; leave `modgui:label "Sympathetic Sitar" ;` unchanged.)

- [ ] **Step 4: Build and verify the stable bundle brand**

```bash
make >/dev/null && grep 'modgui:brand "Stefan"' bin/sitar.lv2/modgui.ttl
```

Expected: prints the matching line (exit 0).

- [ ] **Step 5: Verify the BETA bundle also carries brand "Stefan"**

This proves the sed removal is correct (previously beta rewrote the brand to `sitar-beta`).

```bash
make clean >/dev/null && make BETA=1 >/dev/null && \
grep 'modgui:brand "Stefan"' bin/sitar-beta.lv2/modgui.ttl && \
make clean >/dev/null
```

Expected: prints `modgui:brand "Stefan"` (exit 0).

- [ ] **Step 6: Commit**

```bash
git add Makefile plugins/Sitar/modgui.ttl
git commit -m "Emit vendor brand Stefan in modgui.ttl for stable and beta builds

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: Compact modgui face (layout, Strings rename, drop string ports)

**Files:**
- Modify (replace body): `plugins/Sitar/modgui/icon-sitar.html`
- Modify (replace): `plugins/Sitar/modgui/stylesheet-sitar.css`
- Modify: `plugins/Sitar/modgui.ttl` (remove the 13 `string_1..13` `modgui:port` entries)

**Interfaces:**
- Consumes: brand/label Mustache tokens (`{{brand}}` → "Stefan", `{{label}}` → "Sympathetic Sitar"), the knob sprite at `/resources/knobs/sitar-knob.png{{{ns}}}`, and the `mod-role` selector hooks read by `script-sitar.js`.
- Produces: a ~220px-tall pedal face with a slim selector bar + a four-section knob row (Input · Pitch · Resonance · Output); the pitch knob titled "STRINGS".

- [ ] **Step 1: Replace `icon-sitar.html` with the new body**

Overwrite `plugins/Sitar/modgui/icon-sitar.html` with:

```html
<div class="mod-pedal sitar-pedal mod-pedal-sitar mod-{{color}} {{color}}">

    <div mod-role="drag-handle" class="mod-drag-handle"></div>

    <div class="mod-plugin-brand"><h1>{{brand}}</h1></div>
    <div class="mod-plugin-name"><h1>{{label}}</h1></div>

    <!-- Bypass light + footswitch (required by MOD pedalboard) -->
    <div class="mod-light on" mod-role="bypass-light"></div>
    <div class="mod-footswitch" mod-role="bypass"></div>

    <!-- Scale + Root + Stereo selects (JS-bound via mod-role) and the Test button -->
    <div class="sitar-scale-bar">
        <label class="sitar-field">
            <span class="sitar-field-label">SCALE</span>
            <select class="sitar-select" mod-role="sitar-scale">
                <optgroup label="Western">
                    <option value="major">Major</option>
                    <option value="natural-minor">Natural Minor</option>
                    <option value="harmonic-minor">Harmonic Minor</option>
                    <option value="melodic-minor">Melodic Minor</option>
                    <option value="dorian">Dorian</option>
                    <option value="phrygian">Phrygian</option>
                    <option value="lydian">Lydian</option>
                    <option value="mixolydian">Mixolydian</option>
                    <option value="locrian">Locrian</option>
                    <option value="major-pentatonic">Major Pentatonic</option>
                    <option value="minor-pentatonic">Minor Pentatonic</option>
                    <option value="blues">Blues</option>
                    <option value="pythagorean">Pythagorean</option>
                    <option value="chromatic-12tet">Chromatic (12-TET)</option>
                </optgroup>
                <optgroup label="Turkish Makams">
                    <option value="makam-rast">Rast</option>
                    <option value="makam-ussak">Uşşak</option>
                    <option value="makam-hicaz">Hicaz</option>
                    <option value="makam-saba">Saba</option>
                </optgroup>
                <optgroup label="Indian Ragas">
                    <option value="raga-yaman">Yaman</option>
                    <option value="raga-bhairav">Bhairav</option>
                    <option value="raga-bhairavi">Bhairavi</option>
                    <option value="raga-todi">Todi</option>
                    <option value="raga-marwa">Marwa</option>
                    <option value="raga-malkauns">Malkauns</option>
                </optgroup>
            </select>
        </label>

        <label class="sitar-field">
            <span class="sitar-field-label">ROOT</span>
            <select class="sitar-select" mod-role="sitar-root">
                <option value="130.81">C</option>
                <option value="138.59">C#</option>
                <option value="146.83">D</option>
                <option value="155.56">D#</option>
                <option value="164.81">E</option>
                <option value="174.61">F</option>
                <option value="185.00">F#</option>
                <option value="196.00">G</option>
                <option value="207.65">G#</option>
                <option value="220.00">A</option>
                <option value="233.08">A#</option>
                <option value="246.94">B</option>
            </select>
        </label>

        <label class="sitar-field">
            <span class="sitar-field-label">STEREO</span>
            <select class="sitar-select" mod-role="sitar-stereo">
                <option value="mono">Mono</option>
                <option value="linear-narrow">Linear Narrow</option>
                <option value="linear">Linear</option>
                <option value="wide-narrow">Wide Narrow</option>
                <option value="wide">Wide</option>
            </select>
        </label>

        <div class="sitar-field sitar-test-field">
            <span class="sitar-field-label">TEST</span>
            <div class="sitar-test-btn mod-toggle off"
                 mod-role="input-control-port"
                 mod-port-symbol="audition"
                 mod-widget="switch">
                <span class="sitar-test-icon">&#9654;</span>
            </div>
        </div>
    </div>

    <!-- Knob row grouped by signal path: Input · Pitch · Resonance · Output -->
    <div class="sitar-sections">
        <div class="sitar-sec">
            <span class="sitar-sec-title">Input</span>
            <div class="sitar-sec-knobs">
                <div class="mod-knob sitar-knob-tone">
                    <div class="mod-knob-image" mod-role="input-control-port" mod-port-symbol="gate"></div>
                    <span class="mod-knob-title">GATE</span>
                </div>
                <div class="mod-knob sitar-knob-tone">
                    <div class="mod-knob-image" mod-role="input-control-port" mod-port-symbol="sensitivity"></div>
                    <span class="mod-knob-title">SENS</span>
                </div>
            </div>
        </div>

        <div class="sitar-sec">
            <span class="sitar-sec-title">Pitch</span>
            <div class="sitar-sec-knobs">
                <div class="mod-knob sitar-knob-tone">
                    <div class="mod-knob-image" mod-role="input-control-port" mod-port-symbol="num_active"></div>
                    <span class="mod-knob-title">STRINGS</span>
                </div>
                <div class="mod-knob sitar-knob-tone">
                    <div class="mod-knob-image" mod-role="input-control-port" mod-port-symbol="octave"></div>
                    <span class="mod-knob-title">OCT</span>
                </div>
            </div>
        </div>

        <div class="sitar-sec">
            <span class="sitar-sec-title">Resonance</span>
            <div class="sitar-sec-knobs">
                <div class="mod-knob sitar-knob-tone">
                    <div class="mod-knob-image" mod-role="input-control-port" mod-port-symbol="decay"></div>
                    <span class="mod-knob-title">DECAY</span>
                </div>
                <div class="mod-knob sitar-knob-tone">
                    <div class="mod-knob-image" mod-role="input-control-port" mod-port-symbol="bloom"></div>
                    <span class="mod-knob-title">BLOOM</span>
                </div>
                <div class="mod-knob sitar-knob-tone">
                    <div class="mod-knob-image" mod-role="input-control-port" mod-port-symbol="jawari"></div>
                    <span class="mod-knob-title">JAWARI</span>
                </div>
            </div>
        </div>

        <div class="sitar-sec">
            <span class="sitar-sec-title">Output</span>
            <div class="sitar-sec-knobs">
                <div class="mod-knob sitar-knob-tone">
                    <div class="mod-knob-image" mod-role="input-control-port" mod-port-symbol="mix"></div>
                    <span class="mod-knob-title">MIX</span>
                </div>
                <div class="mod-knob sitar-knob-tone">
                    <div class="mod-knob-image" mod-role="input-control-port" mod-port-symbol="level"></div>
                    <span class="mod-knob-title">LEVEL</span>
                </div>
            </div>
        </div>
    </div>

    <!-- Audio I/O — required structure for pedalboard connections -->
    <div class="mod-pedal-input">
        {{#effect.ports.audio.input}}
        <div class="mod-input mod-input-disconnected" title="{{name}}"
             mod-role="input-audio-port" mod-port-symbol="{{symbol}}">
            <div class="mod-pedal-input-image"></div>
        </div>
        {{/effect.ports.audio.input}}
    </div>
    <div class="mod-pedal-output">
        {{#effect.ports.audio.output}}
        <div class="mod-output mod-output-disconnected" title="{{name}}"
             mod-role="output-audio-port" mod-port-symbol="{{symbol}}">
            <div class="mod-pedal-output-image"></div>
        </div>
        {{/effect.ports.audio.output}}
    </div>

</div>
```

- [ ] **Step 2: Replace `stylesheet-sitar.css`**

Overwrite `plugins/Sitar/modgui/stylesheet-sitar.css` with:

```css
/*
 * Sitar Sympathetic Resonance — MOD pedal stylesheet.
 *
 *   - The pedal is position:absolute with a fixed pixel width/height (MOD
 *     places pedals at absolute board coordinates).
 *   - Custom bands inside the pedal are absolutely positioned so MOD's
 *     injected visuals (jacks, knob images, bypass light/footswitch) don't
 *     reflow them. MOD provides the default visuals for .mod-input/.mod-output,
 *     .mod-knob-image, .mod-light, .mod-footswitch; we only place them.
 *
 * NOTE: .sitar-pedal height must stay in sync with HEIGHT in
 * modgui/render_screenshot.py.
 */

/* ============================ Pedal frame ============================ */
.sitar-pedal {
    width: 640px;
    height: 220px;
    position: absolute;
    border-radius: 14px;
    background:
        repeating-linear-gradient(
            92deg,
            rgba(255, 230, 180, 0.02) 0px,
            rgba(255, 230, 180, 0.02) 1px,
            transparent 1px,
            transparent 4px),
        radial-gradient(ellipse at 20% 0%, rgba(255, 200, 120, 0.18), transparent 60%),
        radial-gradient(ellipse at 80% 100%, rgba(120, 60, 30, 0.4), transparent 65%),
        linear-gradient(135deg, #3a1f12 0%, #5a3220 45%, #2c170c 100%);
    box-shadow:
        inset 0 0 0 2px rgba(255, 200, 130, 0.18),
        inset 0 0 32px rgba(0, 0, 0, 0.55),
        0 6px 14px rgba(0, 0, 0, 0.55);
    color: #f4e3c2;
    font-family: "nexa", "Lato", "Helvetica Neue", Helvetica, Arial, sans-serif;
}

.sitar-pedal .mod-drag-handle {
    position: absolute;
    inset: 0;
    z-index: 0;
    cursor: move;
}

.sitar-pedal .mod-plugin-brand,
.sitar-pedal .mod-plugin-name,
.sitar-pedal .sitar-scale-bar,
.sitar-pedal .sitar-sections,
.sitar-pedal .mod-light,
.sitar-pedal .mod-footswitch,
.sitar-pedal .mod-pedal-input,
.sitar-pedal .mod-pedal-output {
    z-index: 1;
}

/* ============================ Branding ============================ */
.sitar-pedal .mod-plugin-brand {
    position: absolute;
    top: 12px;
    left: 0;
    right: 0;
    text-align: center;
    pointer-events: none;
}
.sitar-pedal .mod-plugin-brand h1 {
    margin: 0;
    font-size: 11px;
    letter-spacing: 0.3em;
    text-transform: uppercase;
    color: rgba(245, 217, 154, 0.7);
    font-weight: 400;
}
.sitar-pedal .mod-plugin-name {
    position: absolute;
    top: 26px;
    left: 0;
    right: 0;
    text-align: center;
    pointer-events: none;
}
.sitar-pedal .mod-plugin-name h1 {
    margin: 0;
    font-size: 20px;
    letter-spacing: 0.18em;
    text-transform: uppercase;
    color: #f5d99a;
    font-weight: 700;
    text-shadow: 0 1px 1px rgba(0, 0, 0, 0.6);
}

/* ============================ Selector bar (inline labels) ============================ */
.sitar-pedal .sitar-scale-bar {
    position: absolute;
    top: 54px;
    left: 24px;
    right: 24px;
    height: 30px;
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 14px;
}
.sitar-pedal .sitar-field {
    display: flex;
    flex-direction: row;
    align-items: center;
    gap: 6px;
}
.sitar-pedal .sitar-field-label {
    font-size: 9px;
    letter-spacing: 0.16em;
    text-transform: uppercase;
    color: rgba(245, 217, 154, 0.65);
}
.sitar-pedal .sitar-select {
    background: #1c0e07;
    color: #f5d99a;
    border: 1px solid rgba(245, 217, 154, 0.35);
    border-radius: 4px;
    padding: 3px 6px;
    font-size: 11px;
    font-family: inherit;
    min-width: 118px;
    outline: none;
    cursor: pointer;
}
.sitar-pedal .sitar-select:hover { border-color: #f5d99a; }

/* "Test Scale" button — sequentially plucks each string. */
.sitar-pedal .sitar-test-btn {
    width: 28px;
    height: 28px;
    border-radius: 50%;
    background: linear-gradient(180deg, #4a2a18 0%, #281407 100%);
    border: 1px solid rgba(0, 0, 0, 0.7);
    box-shadow:
        inset 0 1px 2px rgba(255, 200, 130, 0.25),
        inset 0 -2px 4px rgba(0, 0, 0, 0.55),
        0 1px 2px rgba(0, 0, 0, 0.55);
    color: #f5d99a;
    display: flex;
    align-items: center;
    justify-content: center;
    cursor: pointer;
    user-select: none;
    transition: box-shadow 0.1s ease;
}
.sitar-pedal .sitar-test-btn:hover {
    box-shadow:
        inset 0 1px 2px rgba(255, 200, 130, 0.4),
        inset 0 -2px 4px rgba(0, 0, 0, 0.55),
        0 1px 3px rgba(0, 0, 0, 0.7);
}
.sitar-pedal .sitar-test-btn.on {
    background: radial-gradient(circle at 30% 30%, #ffd55a, #d27a14 70%, #6e3a04);
    box-shadow:
        inset 0 1px 2px rgba(0, 0, 0, 0.3),
        0 0 10px rgba(255, 180, 60, 0.8);
    color: #2a1505;
}
.sitar-pedal .sitar-test-icon {
    font-size: 11px;
    line-height: 1;
    margin-left: 1px;
}

/* ============================ Knob sprite ============================ */
.sitar-pedal .mod-knob-image {
    background-image: url(/resources/knobs/sitar-knob.png{{{ns}}});
    background-repeat: no-repeat;
    background-position: left center;
    display: block;
    cursor: pointer;
}

/* ============================ Sectioned knob row ============================ */
.sitar-pedal .sitar-sections {
    position: absolute;
    top: 98px;
    left: 0;
    right: 0;
    display: flex;
    justify-content: center;
    align-items: flex-start;
}
.sitar-pedal .sitar-sec {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 7px;
    padding: 0 14px;
}
.sitar-pedal .sitar-sec + .sitar-sec {
    border-left: 1px solid rgba(245, 217, 154, 0.13);
}
.sitar-pedal .sitar-sec-title {
    font-size: 8.5px;
    letter-spacing: 0.2em;
    text-transform: uppercase;
    color: rgba(245, 217, 154, 0.5);
}
.sitar-pedal .sitar-sec-knobs {
    display: flex;
    gap: 10px;
}
.sitar-pedal .sitar-knob-tone {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 3px;
    width: 52px;
}
.sitar-pedal .sitar-knob-tone .mod-knob-image {
    width: 46px;
    height: 46px;
    background-size: auto 46px;
}
.sitar-pedal .sitar-knob-tone .mod-knob-title {
    font-size: 9.5px;
    letter-spacing: 0.1em;
    color: rgba(245, 217, 154, 0.85);
    text-align: center;
    line-height: 1.15;
}

/* ============================ Bypass light + footswitch ============================ */
.sitar-pedal .mod-light {
    position: absolute;
    bottom: 12px;
    left: 26px;
    width: 14px;
    height: 14px;
    border-radius: 50%;
    background: #401a09;
    border: 1px solid rgba(0, 0, 0, 0.7);
    box-shadow: inset 0 1px 2px rgba(0, 0, 0, 0.6);
}
.sitar-pedal .mod-light.on {
    background: radial-gradient(circle at 30% 30%, #ffd55a, #d27a14 70%, #6e3a04);
    box-shadow:
        inset 0 1px 2px rgba(0, 0, 0, 0.3),
        0 0 8px rgba(255, 180, 60, 0.7);
}
.sitar-pedal .mod-footswitch {
    position: absolute;
    bottom: 8px;
    left: 56px;
    width: 24px;
    height: 24px;
    border-radius: 50%;
    background: linear-gradient(180deg, #4a2a18 0%, #281407 100%);
    border: 1px solid rgba(0, 0, 0, 0.7);
    box-shadow:
        inset 0 1px 2px rgba(255, 200, 130, 0.25),
        inset 0 -2px 4px rgba(0, 0, 0, 0.6),
        0 2px 3px rgba(0, 0, 0, 0.6);
    cursor: pointer;
}
.sitar-pedal .mod-footswitch:hover {
    box-shadow:
        inset 0 1px 2px rgba(255, 200, 130, 0.4),
        inset 0 -2px 4px rgba(0, 0, 0, 0.6),
        0 2px 4px rgba(0, 0, 0, 0.7);
}

/* ============================ Audio I/O jacks ============================
   No overrides — MOD-UI positions/styles the jacks at fixed offsets. If the
   lower stereo output jack overhangs this shorter frame, raise .sitar-pedal
   height (and render_screenshot.py HEIGHT) together rather than nudging the
   jacks (nudging drifts the cable endpoint off the visual plug). */
```

- [ ] **Step 3: Remove the 13 string ports from `modgui.ttl`**

In `plugins/Sitar/modgui.ttl`, delete the `modgui:port` entries for `string_1` through `string_13` (the first 13 blocks in the `modgui:port [ ... ] , [ ... ]` list). The list must now START at `num_active`:

```ttl
        modgui:port [
            lv2:symbol "num_active" ;
            lv2:name "Active Strings"
        ] , [
            lv2:symbol "octave" ;
            lv2:name "Octave"
        ] , [
```

Leave every non-string port entry intact (`num_active`, `octave`, `decay`, `mix`, `jawari`, `scale`, `root_note`, `audition`, `bloom`, `stereo_mode`, `gate`, `level`, `sensitivity`).

- [ ] **Step 4: Build and static-check the assembled bundle**

```bash
make >/dev/null && \
echo "-- string knobs gone from HTML:" && ! grep -q 'sitar-string' bin/sitar.lv2/modgui/icon-sitar.html && echo OK && \
echo "-- STRINGS title present:" && grep -q '>STRINGS<' bin/sitar.lv2/modgui/icon-sitar.html && echo OK && \
echo "-- section captions present:" && grep -q '>Resonance<' bin/sitar.lv2/modgui/icon-sitar.html && echo OK && \
echo "-- string ports gone from ttl:" && ! grep -q 'lv2:symbol "string_' bin/sitar.lv2/modgui.ttl && echo OK && \
echo "-- all 13 control ports still in ttl:" && [ "$(grep -c 'lv2:symbol' bin/sitar.lv2/modgui.ttl)" = "13" ] && echo OK
```

Expected: five `OK` lines (no `sitar-string`, has `STRINGS`, has `Resonance`, no `string_` ports, exactly 13 `lv2:symbol` port entries remain).

- [ ] **Step 5: Verify on device (MOD Desktop)**

```bash
make && ./install.sh
```

Then restart MOD Desktop and confirm by eye:
- Plugin appears under brand **Stefan**, name **Sympathetic Sitar**.
- Face matches the compact mockup: slim Scale/Root/Stereo/Test bar, then one knob row split Input · Pitch · Resonance · Output; the pitch knob reads **STRINGS**.
- **Both audio output jacks sit inside the frame.** If the lower one overhangs, raise `height` in `stylesheet-sitar.css` (e.g. 220 → 245) — you'll also raise `HEIGHT` in `render_screenshot.py` in Task 4 to the same value.
- Scale / Root / Stereo dropdowns and Test still work (script unchanged).
- Open the gear/detailed view and confirm the 48 `string_N` ports are still present and editable there.

- [ ] **Step 6: Commit**

```bash
git add plugins/Sitar/modgui/icon-sitar.html plugins/Sitar/modgui/stylesheet-sitar.css plugins/Sitar/modgui.ttl
git commit -m "Compact modgui: drop per-string knob row, group knobs Input/Pitch/Resonance/Output, rename N Strings->Strings

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 4: Docs + regenerated screenshot & manual

**Files:**
- Modify: `README.md` (two brand mentions)
- Modify: `docs/manual/sitar-manual.html` (three brand mentions)
- Modify: `plugins/Sitar/modgui/render_screenshot.py` (brand token + HEIGHT)
- Regenerate (commit binaries): `plugins/Sitar/modgui/screenshot-sitar.png`, `docs/manual/sitar-manual.pdf`

**Interfaces:**
- Consumes: the final pedal height chosen in Task 3 (must equal `render_screenshot.py` `HEIGHT`).
- Produces: docs and generated art that show brand "Stefan" and the new face.

- [ ] **Step 1: Update README brand text**

In `README.md`:
- The install line (~L128): `appears under brand **"sitar"** as` → `appears under brand **"Stefan"** as`.
- The beta line (~L270): `shows up as **"Sitar (Beta)"** under brand **"sitar-beta"**.` → `shows up as **"Sitar (Beta)"** under brand **"Stefan"**.`

- [ ] **Step 2: Update the manual brand text**

In `docs/manual/sitar-manual.html`, change the three brand mentions:
- `<div class="brand">sitar</div>` → `<div class="brand">Stefan</div>`
- `find it under the brand <b>sitar</b> and drag it` → `find it under the brand <b>Stefan</b> and drag it`
- `brand <b>sitar</b> as <b>Sympathetic Sitar</b>` → `brand <b>Stefan</b> as <b>Sympathetic Sitar</b>`

- [ ] **Step 3: Update render_screenshot.py brand + height**

In `plugins/Sitar/modgui/render_screenshot.py`:
- In `SUBSTITUTIONS`, `"{{brand}}": "sitar",` → `"{{brand}}": "Stefan",`.
- `HEIGHT = 320` → the height chosen in Task 3 (e.g. `HEIGHT = 220`, or 245 if the jacks forced a bump — must match `.sitar-pedal` height exactly).

- [ ] **Step 4: Regenerate the screenshot**

Requires Playwright + Chromium (`pip install playwright && playwright install chromium`).

```bash
python3 plugins/Sitar/modgui/render_screenshot.py
```

Expected: `Wrote .../screenshot-sitar.png (640x<HEIGHT>, rendered by headless Chromium)`.

**Fallback:** if Playwright/Chromium is unavailable in this environment, do NOT commit a stale PNG. Commit the source edits (Steps 1–3) and note in the commit body that `screenshot-sitar.png` regen is pending.

- [ ] **Step 5: Regenerate the manual PDF**

Requires headless Chrome (`make manual` uses `google-chrome`).

```bash
make manual
```

Expected: writes `docs/manual/sitar-manual.pdf`. Same fallback as Step 4 if Chrome is unavailable — leave the PDF regen as a flagged follow-up rather than committing a stale/mismatched PDF.

- [ ] **Step 6: Commit**

```bash
git add README.md docs/manual/sitar-manual.html plugins/Sitar/modgui/render_screenshot.py
# include these two only if regenerated in Steps 4-5:
git add plugins/Sitar/modgui/screenshot-sitar.png docs/manual/sitar-manual.pdf
git commit -m "docs: brand Stefan across README/manual; regen screenshot + manual for compact face

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Notes for the whole plan

- **Build env:** a plain `make` builds lv2 + vst3 + clap and runs the TTL + modgui steps; it needs the DPF submodule present (`git submodule update --init --recursive` if `dpf/` is empty). None of the code tasks need a browser; only Task 4's asset regen does.
- **Suggested order:** 1 → 2 → 3 → 4. Tasks 2 and 3 both edit `modgui.ttl` (different lines: brand vs. the string-port list); do 2 before 3.
- **Rollback:** every task is a single commit; `git revert` any one independently.
