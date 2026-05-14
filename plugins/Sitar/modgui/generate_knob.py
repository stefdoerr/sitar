#!/usr/bin/env python3
"""
Generate a brass/gold knob film-strip sprite for the Sitar modgui.

MOD-UI's `film` widget reads .mod-knob-image's background-image and rotates
through frames by shifting background-position-x. Each frame must be square.
The frame count is derived from (image_width / frame_height).

Output: knobs/sitar-knob.png  —  FRAMES x FRAME_SIZE x FRAME_SIZE

Run inside any Python env that has Pillow installed:
    python3 generate_knob.py
"""

import math
import os
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
OUT  = os.path.join(HERE, "knobs", "sitar-knob.png")

FRAME_SIZE     = 128
FRAMES         = 65           # smooth 270-degree rotation
ROTATION_RANGE = 270.0        # degrees swept from min to max
START_ANGLE    = -135.0       # frame 0 points to "lower-left" (8 o'clock)

# Palette (matches our pedal CSS: brass + dark teak)
BRASS_HI   = (255, 224, 158)
BRASS_MID  = (216, 168,  82)
BRASS_LOW  = (122,  78,  24)
RIM_DARK   = ( 38,  20,   8)
INDICATOR  = ( 24,  12,   4)
GROOVE     = ( 90,  56,  20, 150)


def draw_knob(frame_size: int, angle_deg: float) -> Image.Image:
    """Render a single knob frame with the indicator at the given angle."""
    img  = Image.new("RGBA", (frame_size, frame_size), (0, 0, 0, 0))
    cx   = cy = frame_size / 2
    r_outer = frame_size * 0.46
    r_inner = frame_size * 0.40
    r_face  = frame_size * 0.34

    # Outer rim (dark, slightly larger than the face).
    draw = ImageDraw.Draw(img)
    draw.ellipse(
        [cx - r_outer, cy - r_outer, cx + r_outer, cy + r_outer],
        fill=RIM_DARK + (255,),
    )

    # Brass body — fake radial gradient by stacking shrinking discs.
    steps = 24
    for i in range(steps):
        t = i / (steps - 1)
        # Blend BRASS_HI (top-left highlight) → BRASS_MID → BRASS_LOW.
        if t < 0.5:
            k = t * 2
            col = tuple(int(BRASS_HI[c] * (1 - k) + BRASS_MID[c] * k) for c in range(3))
        else:
            k = (t - 0.5) * 2
            col = tuple(int(BRASS_MID[c] * (1 - k) + BRASS_LOW[c] * k) for c in range(3))
        r = r_inner * (1 - t) + r_face * t * 0.4
        # Offset toward upper-left so the highlight sits at ~10 o'clock.
        off_x = (1 - t) * frame_size * 0.07
        off_y = (1 - t) * frame_size * 0.07
        draw.ellipse(
            [cx - r - off_x, cy - r - off_y, cx + r - off_x, cy + r - off_y],
            fill=col + (255,),
        )

    # Concentric groove for sitar bridge feel.
    for r in (r_inner * 0.92, r_inner * 0.78):
        draw.ellipse(
            [cx - r, cy - r, cx + r, cy + r],
            outline=GROOVE, width=1,
        )

    # Indicator: dark wedge from centre toward the angle.
    ind_layer = Image.new("RGBA", (frame_size, frame_size), (0, 0, 0, 0))
    ind_draw  = ImageDraw.Draw(ind_layer)
    # Compass convention: angle 0 = up, +90 = right. (sin, -cos) gives the
    # direction in image coordinates (y grows downward).
    rad   = math.radians(angle_deg)
    tip_x = cx + math.sin(rad) * r_inner * 0.92
    tip_y = cy - math.cos(rad) * r_inner * 0.92
    base_x = cx + math.sin(rad) * r_inner * 0.18
    base_y = cy - math.cos(rad) * r_inner * 0.18

    # Draw a thin line + a dot at the tip — readable at small sizes.
    ind_draw.line(
        [(base_x, base_y), (tip_x, tip_y)],
        fill=INDICATOR + (255,),
        width=max(2, frame_size // 30),
    )
    dot_r = frame_size * 0.05
    ind_draw.ellipse(
        [tip_x - dot_r, tip_y - dot_r, tip_x + dot_r, tip_y + dot_r],
        fill=INDICATOR + (255,),
    )

    img.alpha_composite(ind_layer)

    # Subtle bevel highlight at the top-left edge of the brass body.
    bevel = Image.new("RGBA", (frame_size, frame_size), (0, 0, 0, 0))
    bd    = ImageDraw.Draw(bevel)
    bd.ellipse(
        [cx - r_inner * 1.02, cy - r_inner * 1.02,
         cx + r_inner * 0.6,  cy + r_inner * 0.6],
        outline=(255, 240, 200, 110), width=2,
    )
    bevel = bevel.filter(ImageFilter.GaussianBlur(radius=1.2))
    img.alpha_composite(bevel)

    return img


def build_filmstrip(frame_size: int, frames: int, out_path: str) -> None:
    strip = Image.new("RGBA", (frame_size * frames, frame_size), (0, 0, 0, 0))
    for i in range(frames):
        t = i / (frames - 1)
        angle = START_ANGLE + ROTATION_RANGE * t
        strip.paste(draw_knob(frame_size, angle), (i * frame_size, 0))

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    strip.save(out_path, "PNG", optimize=True)
    print(f"Wrote {out_path} ({strip.size[0]}x{strip.size[1]}, {frames} frames)")


if __name__ == "__main__":
    build_filmstrip(FRAME_SIZE, FRAMES, OUT)
