#!/usr/bin/env python3
"""
Render screenshot-sitar.png via headless Chromium so it matches MOD-UI's
actual rendering of the pedal (real fonts, real knob sprites, real CSS gradients).

Pipeline:
  1. Read icon-sitar.html and resolve the Mustache template tokens MOD-UI would
     normally fill in at runtime (brand, label, color, knob, audio-port loops).
  2. Read stylesheet-sitar.css and rewrite the `/resources/...{{{ns}}}` knob URL
     to a file:// path so chromium can load the sprite locally.
  3. Inline the stylesheet, give the body a 640×HEIGHT viewport with overflow:hidden
     so the audio jacks (which stick out at -44px) are cropped away — matching
     real MOD plugin screenshots that show only the pedal frame.
  4. Write a self-contained render.html to a temp dir, launch Chromium headless,
     screenshot it at 640×HEIGHT, and copy the PNG to screenshot-sitar.png.

Run inside any Python env that has Playwright + Chromium installed:
    pip install playwright && playwright install chromium
    python3 render_screenshot.py
"""

import os
import re
import shutil
import tempfile

from playwright.sync_api import sync_playwright

HERE   = os.path.dirname(os.path.abspath(__file__))
ICON   = os.path.join(HERE, "icon-sitar.html")
STYLE  = os.path.join(HERE, "stylesheet-sitar.css")
SPRITE = os.path.join(HERE, "knobs", "sitar-knob.png")
OUT    = os.path.join(HERE, "screenshot-sitar.png")
OUT_EDITOR = os.path.join(HERE, "screenshot-editor.png")

WIDTH  = 640
HEIGHT = 220

# SITAR_EDITOR=1 renders the user-scale editor overlay (open + populated with an
# example scale) to screenshot-editor.png instead of the closed face. Used by
# the beginner manual's "Make your own scale" page.
EDITOR = bool(os.environ.get("SITAR_EDITOR"))


# ---------------------------------------------------------------------------
# Mustache substitution (matches what MOD-UI does in modgui.js at runtime)
# ---------------------------------------------------------------------------

# Same values you put in modgui.ttl.
SUBSTITUTIONS = {
    "{{brand}}":   "Stefan",
    "{{label}}":   "Sympathetic Sitar",
    "{{color}}":   "wood3",
    "{{knob}}":    "gold",
    "{{{cns}}}":   "",   # cache-bust class suffix; harmless to drop
    "{{{ns}}}":    "",   # cache-bust query string; not needed for local files
    "{{instancename}}": "sitar-render",
}

# The audio port iteration blocks in icon-sitar.html. Replace each block with
# the rendered HTML for our actual ports. Single mono input, stereo outputs.
AUDIO_INPUT_HTML = (
    '<div class="mod-input mod-input-disconnected" title="Input" '
    'mod-role="input-audio-port" mod-port-symbol="in">'
    '<div class="mod-pedal-input-image"></div>'
    '</div>'
)
AUDIO_OUTPUT_HTML = (
    '<div class="mod-output mod-output-disconnected" title="Output Left" '
    'mod-role="output-audio-port" mod-port-symbol="out_l">'
    '<div class="mod-pedal-output-image"></div>'
    '</div>'
    '<div class="mod-output mod-output-disconnected" title="Output Right" '
    'mod-role="output-audio-port" mod-port-symbol="out_r">'
    '<div class="mod-pedal-output-image"></div>'
    '</div>'
)


def render_template(html: str) -> str:
    for k, v in SUBSTITUTIONS.items():
        html = html.replace(k, v)

    # {{#effect.ports.audio.input}}…{{/effect.ports.audio.input}}
    html = re.sub(
        r"\{\{#effect\.ports\.audio\.input\}\}.*?\{\{/effect\.ports\.audio\.input\}\}",
        AUDIO_INPUT_HTML, html, flags=re.DOTALL,
    )
    html = re.sub(
        r"\{\{#effect\.ports\.audio\.output\}\}.*?\{\{/effect\.ports\.audio\.output\}\}",
        AUDIO_OUTPUT_HTML, html, flags=re.DOTALL,
    )
    # Drop midi/cv iteration blocks (we don't have any of those ports).
    for kind in ("midi", "cv"):
        for direction in ("input", "output"):
            html = re.sub(
                r"\{\{#effect\.ports\." + kind + r"\." + direction + r"\}\}.*?\{\{/effect\.ports\." + kind + r"\." + direction + r"\}\}",
                "", html, flags=re.DOTALL,
            )
    return html


def render_stylesheet(css: str, sprite_url: str) -> str:
    # Apply the same substitutions as the HTML (mostly to handle {{{ns}}}).
    for k, v in SUBSTITUTIONS.items():
        css = css.replace(k, v)
    # Rewrite the knob sprite URL to point at our local file.
    css = css.replace("/resources/knobs/sitar-knob.png", sprite_url)
    return css


# ---------------------------------------------------------------------------
# Render
# ---------------------------------------------------------------------------

def main():
    with open(ICON,  "r", encoding="utf-8") as f: icon_html = f.read()
    with open(STYLE, "r", encoding="utf-8") as f: css       = f.read()

    sprite_url = "file://" + SPRITE
    css        = render_stylesheet(css, sprite_url)
    pedal_html = render_template(icon_html)

    if EDITOR:
        # No modgui JS here, so open + populate the overlay by hand for the shot.
        pedal_html = pedal_html.replace(
            'mod-role="sitar-editor" style="display:none;"',
            'mod-role="sitar-editor" style="display:flex;"')
        pedal_html = pedal_html.replace(
            'mod-role="sitar-edit-slot"></select>',
            'mod-role="sitar-edit-slot"><option>User 1</option></select>')
        pedal_html = pedal_html.replace(
            'mod-role="sitar-edit-name" type="text"',
            'mod-role="sitar-edit-name" type="text" value="My Rast"')
        pedal_html = pedal_html.replace(
            'mod-role="sitar-edit-ivals" type="text"',
            'mod-role="sitar-edit-ivals" type="text" value="9/8, 347.4, 4/3, 3/2, 27/16, 1049.0"')

    page = f"""<!doctype html>
<html><head>
<meta charset="utf-8">
<style>
  html, body {{
    margin: 0; padding: 0;
    width: {WIDTH}px; height: {HEIGHT}px;
    overflow: hidden;
    background: transparent;
  }}
  /* MOD-UI hosts the pedal inside a positioned container; mirror that so
     position:absolute on .sitar-pedal anchors to (0,0) of the viewport. */
  .pedal-host {{ position: relative; width: {WIDTH}px; height: {HEIGHT}px; }}
{css}
</style></head>
<body><div class="pedal-host">{pedal_html}</div></body></html>"""

    with tempfile.TemporaryDirectory(prefix="sitar-render-") as tmp:
        page_path = os.path.join(tmp, "render.html")
        with open(page_path, "w", encoding="utf-8") as f:
            f.write(page)

        with sync_playwright() as p:
            browser = p.chromium.launch()
            context = browser.new_context(
                viewport={"width": WIDTH, "height": HEIGHT},
                device_scale_factor=1,
            )
            tab = context.new_page()
            tab.goto("file://" + page_path)
            # Give the browser a moment to lay out and load the sprite.
            tab.wait_for_load_state("networkidle")

            tmp_out = os.path.join(tmp, "screenshot.png")
            tab.screenshot(path=tmp_out, omit_background=False, full_page=False)
            browser.close()

        out = OUT_EDITOR if EDITOR else OUT
        shutil.copyfile(tmp_out, out)
        print(f"Wrote {out} ({WIDTH}x{HEIGHT}, rendered by headless Chromium)")


if __name__ == "__main__":
    main()
