# Design — Compact modgui + "Stefan" rebrand

**Date:** 2026-07-23
**Scope:** MOD pedalboard GUI (modgui) of the Sympathetic Sitar plugin, plus a
vendor-brand change. **No DSP changes.**

Visual reference (rendered mockup, chosen layout "C1"):
<https://claude.ai/code/artifact/a4c0ee79-5630-4ec5-b114-f5a14cdf5708>

---

## Goal

Simplify the MOD pedal face and rebrand the vendor.

1. **Remove the 13 per-string fine-tune knobs** from the main face. Nobody
   retunes individual strings from the pedal; the 48 string ports remain fully
   editable in MOD's gear/detailed view (that view is generated from the
   plugin's own `sitar.ttl`, not from `modgui.ttl`, so removing the
   `modgui:port` entries does not remove the ports from the editor).
2. **Rework the remaining layout** into a compact, grouped face ("C1"): a slim
   selector bar over a single knob row split into four captioned sections that
   trace the signal path — **Input · Pitch · Resonance · Output**.
3. **Rename** the pitch knob's on-face title **"N Strings" → "Strings"**.
4. **Change the vendor brand `sitar` → `Stefan`**, mirroring exactly what was
   done in `../boreas` (brand = vendor; the plugin stays named "Sitar").

## Non-goals / unchanged

- **`SitarPlugin.cpp` DSP** — untouched. All 61 parameters and 48 string ports
  still exist and function.
- **Ports / parameters / symbols** — unchanged (`num_active` keeps its symbol
  and its parameter name "Active Strings"). Existing presets, saved
  pedalboards, and automation keep working.
- **`script-sitar.js`** — unchanged. The redesigned selector bar keeps the
  existing `mod-role="sitar-scale" / "-root" / "-stereo"` hooks the script
  binds to.
- **Bundle name (`sitar.lv2`), LV2 URI (`.../plugins/sitar`), CLAP id, unique
  ids, `DISTRHO_PLUGIN_BRAND_ID`, install paths** — all unchanged. Only the
  displayed vendor/maker string changes.

---

## Change 1 — modgui layout "C1"

### `plugins/Sitar/modgui/icon-sitar.html`

Restructure the pedal body to:

- **Header** (unchanged roles): `.mod-plugin-brand` (`{{brand}}` → renders
  "STEFAN") above `.mod-plugin-name` (`{{label}}` → "Sympathetic Sitar"),
  centered.
- **Delete** the entire `.sitar-strings` block (all 13 `.sitar-string`
  elements with `mod-port-symbol="string_1..13"`).
- **Selector bar** (`.sitar-scale-bar`, slim, inline labels): `Scale`, `Root`,
  `Stereo` `<select>`s (keep `mod-role` + option lists + values exactly as
  today) and the `Test` button (`mod-port-symbol="audition"`). Inline label
  form: a small `Scale` caption beside each select rather than stacked above.
- **Sectioned knob row** replacing `.sitar-tone`: four groups with a small
  uppercase caption and hairline dividers between them —
  - **Input:** `gate`, `sensitivity`
  - **Pitch:** `num_active` (title **"Strings"**), `octave`
  - **Resonance:** `decay`, `bloom`, `jawari`
  - **Output:** `mix`, `level`
  Each knob keeps its existing `mod-role="input-control-port"` +
  `mod-port-symbol`.
- Keep the audio I/O port blocks, `.mod-drag-handle`, `.mod-light`,
  `.mod-footswitch` exactly as today.

### `plugins/Sitar/modgui/stylesheet-sitar.css`

- Remove `.sitar-strings`, `.sitar-string`, `.sitar-knob-string*`,
  `.sitar-string-num` rules.
- Add `.sitar-sections` / `.sitar-sec` / `.sitar-sec-title` (hairline
  dividers via `.sitar-sec + .sitar-sec::before`) and inline-label selector
  styling.
- Reduce `.sitar-pedal` height from `320px` toward **~210px**. **This is the
  one value to confirm on-device** (see Risks): the two stereo output jacks are
  positioned by MOD-UI at fixed offsets, so if they render below the frame the
  height gets nudged up just enough to contain the lower jack.
- Knobs sized ~46px; keep the existing `.mod-knob-image` sprite wiring
  (`background-image: url(/resources/knobs/sitar-knob.png{{{ns}}})`).

## Change 2 — "N Strings" → "Strings"

On-face knob **title only**, in `icon-sitar.html` (the `num_active` knob's
`.mod-knob-title`). Parameter name ("Active Strings") and symbol
(`num_active`) unchanged.

## Change 3 — Vendor brand `sitar` → `Stefan` (boreas method)

### `plugins/Sitar/DistrhoPluginInfo.h`
- `DISTRHO_PLUGIN_BRAND "sitar"` → `"Stefan"` in **both** the `#ifdef
  SITAR_BETA` and `#else` branches.
- Leave `DISTRHO_PLUGIN_NAME` (`"Sitar"` / `"Sitar (Beta)"`), URIs, CLAP ids,
  `DISTRHO_PLUGIN_BRAND_ID`, `DISTRHO_PLUGIN_UNIQUE_ID` unchanged.

### `plugins/Sitar/SitarPlugin.cpp`
- `getLabel()` — return `DISTRHO_PLUGIN_NAME` (currently returns
  `DISTRHO_PLUGIN_BRAND`; the label must stay the plugin name, not the vendor).
- `getMaker()` — keep returning `DISTRHO_PLUGIN_BRAND` (now "Stefan").

### `Makefile`
- `BRAND := sitar` → `BRAND := Stefan`.
- In the `modgui:` target, **remove** the sed line
  `-e 's|modgui:brand "$(BRAND)"|modgui:brand "$(BUNDLE_NAME)"|'`. The brand no
  longer varies between stable and beta (both are "Stefan"), so only the URI
  and label substitutions remain — matching boreas.

### `plugins/Sitar/modgui.ttl`
- `modgui:brand "sitar"` → `modgui:brand "Stefan"`. Leave
  `modgui:label "Sympathetic Sitar"` and remove the 13 `string_1..13`
  `modgui:port` entries (part of Change 1).

### Docs
- `README.md`: line ~128 "appears under brand **sitar**" → **Stefan**; line
  ~270 beta note ("under brand **sitar-beta**") → beta now groups under brand
  **Stefan** with name "Sitar (Beta)".
- `docs/manual/sitar-manual.html`: brand mentions at ~294 (cover), ~334, ~597
  → "Stefan". Regenerate the PDF (`make manual`, needs headless Chrome).

**Side-effect (intended, matches boreas):** the beta build now groups under
brand **Stefan** too (only its name differs). Previously it showed as
"sitar-beta".

## Change 4 — Regenerate `screenshot-sitar.png`

Re-render via `plugins/Sitar/modgui/render_screenshot.py` so the store /
pedalboard art matches the new face (needs headless Chrome). The rendered PNG
is committed.

---

## Verification

1. `make` — builds clean (lv2 + vst3 + clap).
2. `make test` — the 5 DSP regression tests still pass (no DSP change, but
   confirms nothing in the shared build broke).
3. `./install.sh` into MOD Desktop; restart and confirm:
   - Plugin lists under brand **Stefan**, name **Sympathetic Sitar**.
   - Pedal face matches the C1 mockup; no string-knob row; sections read
     Input · Pitch · Resonance · Output; pitch knob reads **Strings**.
   - Both audio output jacks sit inside the frame at the chosen height
     (adjust height if not).
   - Scale / Root / Stereo dropdowns + Test still work (script unchanged).
   - The 48 `string_N` ports are still present and editable in the gear view.
4. Confirm the generated `screenshot-sitar.png` and manual PDF reflect the new
   face/brand.

## Risks / open items

- **Pedal height vs. fixed jack positions** — the only real unknown. Resolved
  empirically in step 3; the current stylesheet comment about jack centering
  may be stale (MOD-UI may have changed), so we start compact and nudge up only
  if needed.
- **Headless Chrome availability** for the screenshot + manual PDF regen. If
  unavailable in the working environment, flag it and leave regeneration as a
  follow-up rather than committing stale generated assets.

## Touched files

```
plugins/Sitar/modgui/icon-sitar.html        (layout, Strings rename)
plugins/Sitar/modgui/stylesheet-sitar.css   (layout, height)
plugins/Sitar/modgui.ttl                     (brand, drop string ports)
plugins/Sitar/DistrhoPluginInfo.h            (brand)
plugins/Sitar/SitarPlugin.cpp                (getLabel)
Makefile                                     (BRAND, drop modgui brand sed)
README.md                                    (brand text)
docs/manual/sitar-manual.html                (brand text) + regenerated PDF
plugins/Sitar/modgui/screenshot-sitar.png    (regenerated)
```
