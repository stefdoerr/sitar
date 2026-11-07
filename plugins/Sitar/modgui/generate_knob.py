#!/usr/bin/env python3
"""
Generate the knob film-strip sprite for the Sitar modgui ("Raga Night" face).

MOD-UI's `film` widget reads .mod-knob-image's background-image and steps
through frames by shifting background-position-x. Each frame must be square;
the frame count is derived from (image_width / frame_height).

The knob is the one from the design mockup (docs/superpowers/specs/
2026-11-07-raga-night-mockup.html): a knurled brass skirt, a black bakelite
cap and a gold pointer. Rather than re-draw it in Pillow, this script lays out
all frames as CSS in one HTML page and screenshots it with headless Chrome, so
the sprite matches the mockup exactly. The drop shadow is NOT baked in; the
stylesheet adds it with `filter: drop-shadow(...)`.

Output: knobs/sitar-knob.png  —  FRAMES x FRAME_SIZE x FRAME_SIZE, RGBA

Run from anywhere (needs `google-chrome`, override with $CHROME):
    python3 generate_knob.py
"""

import os
import shutil
import subprocess
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
OUT  = os.path.join(HERE, "knobs", "sitar-knob.png")
CHROME = os.environ.get("CHROME", "google-chrome")

FRAME_SIZE     = 128
FRAMES         = 65           # smooth 270-degree rotation
ROTATION_RANGE = 270.0        # degrees swept from min to max
START_ANGLE    = -135.0       # frame 0 points to "lower-left" (7:30); 0 = up
DIAMETER       = 124          # knob diameter inside the 128 px frame

# The mockup knob is 46 px; every length below is in mockup px, scaled by S.
CSS = """
html, body { margin: 0; background: transparent; }
.k { position: absolute; top: %(pad)spx; width: %(d)spx; height: %(d)spx; border-radius: 50%%;
     --s: %(s)s;
     background: repeating-conic-gradient(#d6aa58 0 5deg, #7a561d 5deg 10deg);
     box-shadow: inset 0 0 0 calc(1px * var(--s)) rgba(40,20,0,.6); }
.k::before { content: ""; position: absolute; inset: calc(6px * var(--s)); border-radius: 50%%;
     background: radial-gradient(circle at 36%% 28%%, #5a5a5a, #161616 55%%, #000);
     box-shadow: inset 0 calc(1px * var(--s)) calc(1px * var(--s)) rgba(255,255,255,.25),
                 0 calc(1px * var(--s)) calc(2px * var(--s)) rgba(0,0,0,.6); }
.k i { position: absolute; inset: 0; }
.k i::after { content: ""; position: absolute; left: 50%%;
     top: calc(7px * var(--s)); width: calc(2px * var(--s)); height: calc(13px * var(--s));
     margin-left: calc(-1px * var(--s)); border-radius: calc(1px * var(--s));
     background: #f1d797; box-shadow: 0 0 calc(3px * var(--s)) rgba(241,215,151,.6); }
"""


def build_page() -> str:
    pad = (FRAME_SIZE - DIAMETER) / 2
    css = CSS % {"pad": pad, "d": DIAMETER, "s": DIAMETER / 46}
    cells = []
    for i in range(FRAMES):
        angle = START_ANGLE + ROTATION_RANGE * i / (FRAMES - 1)
        cells.append(f'<div class="k" style="left:{i * FRAME_SIZE + pad}px">'
                     f'<i style="transform:rotate({angle:.4f}deg)"></i></div>')
    return f"<!doctype html><meta charset=utf-8><style>{css}</style>{''.join(cells)}"


def main() -> None:
    width = FRAME_SIZE * FRAMES
    with tempfile.TemporaryDirectory(prefix="sitar-knob-") as tmp:
        page = os.path.join(tmp, "knob.html")
        shot = os.path.join(tmp, "knob.png")
        with open(page, "w", encoding="utf-8") as f:
            f.write(build_page())
        subprocess.run([CHROME, "--headless=new", "--disable-gpu", "--hide-scrollbars",
                        "--force-device-scale-factor=1",
                        "--default-background-color=00000000",
                        f"--window-size={width},{FRAME_SIZE}",
                        f"--screenshot={shot}", "file://" + page],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        os.makedirs(os.path.dirname(OUT), exist_ok=True)
        shutil.copyfile(shot, OUT)
    print(f"Wrote {OUT} ({width}x{FRAME_SIZE}, {FRAMES} frames)")


if __name__ == "__main__":
    main()
