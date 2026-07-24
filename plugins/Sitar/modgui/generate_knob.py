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
from PIL import Image, ImageDraw, ImageFilter, ImageChops

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
RIM_DARK   = ( 36,  18,   6)
INDICATOR  = ( 24,  16,  10)
GROOVE     = ( 90,  56,  20, 150)


def _blend(a, b, t):
    """Linear RGB blend a→b at t in [0, 1]."""
    return tuple(int(a[c] * (1 - t) + b[c] * t) for c in range(3))


def draw_knob(frame_size: int, angle_deg: float) -> Image.Image:
    """Render a single knob frame with the indicator at the given angle.

    The brass face is an offset radial gradient — bright at the ~10 o'clock
    highlight, quickly into mid-brass, then dark at the edge — drawn as rings
    concentric with the HIGHLIGHT point but clipped to the centred face circle.
    So only the *light* is off-centre; the body edge, grooves and indicator
    pivot all share the true centre that MOD-UI's film widget rotates about (the
    knob no longer drifts up-left inside its frame).
    """
    fs   = frame_size
    img  = Image.new("RGBA", (fs, fs), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx   = cy = fs / 2
    r_outer = fs * 0.46
    r_face  = fs * 0.435

    # Dark rim — concentric with the frame centre.
    draw.ellipse(
        [cx - r_outer, cy - r_outer, cx + r_outer, cy + r_outer],
        fill=RIM_DARK + (255,),
    )

    # Brass face — an offset radial gradient matching the modgui mock-up:
    # HI at the ~10 o'clock focal point, into MID by ~46% of the radius, then
    # LOW at the edge. Drawn as discs concentric with the highlight point (so the
    # bright spot is small and up-left), then clipped to the centred face circle
    # so the body edge itself stays concentric with the frame.
    face = Image.new("RGBA", (fs, fs), (0, 0, 0, 0))
    fd   = ImageDraw.Draw(face)
    hx, hy = cx - fs * 0.12, cy - fs * 0.155           # highlight focal point
    R = r_face + math.hypot(cx - hx, cy - hy)           # reaches the far edge
    steps = 56
    for i in range(steps):
        u = i / (steps - 1)          # 0 = outer edge (LOW, large) -> 1 = focal (HI)
        s = 1.0 - u                  # gradient stop, measured from the focal point
        if s < 0.46:
            col = _blend(BRASS_HI, BRASS_MID, s / 0.46)
        else:
            col = _blend(BRASS_MID, BRASS_LOW, (s - 0.46) / 0.54)
        r = R * (1 - u)
        fd.ellipse([hx - r, hy - r, hx + r, hy + r], fill=col + (255,))
    face = face.filter(ImageFilter.GaussianBlur(fs * 0.012))   # smooth the banding
    mask = Image.new("L", (fs, fs), 0)
    ImageDraw.Draw(mask).ellipse(
        [cx - r_face, cy - r_face, cx + r_face, cy + r_face], fill=255)
    face.putalpha(ImageChops.multiply(face.getchannel("A"), mask))
    img.alpha_composite(face)

    # Concentric grooves for the sitar-bridge feel — centred.
    for r in (r_face * 0.80, r_face * 0.62):
        draw.ellipse(
            [cx - r, cy - r, cx + r, cy + r],
            outline=GROOVE, width=max(1, fs // 128),
        )

    # Indicator: a dark line + tip dot pivoting at the TRUE centre, so it tracks
    # MOD's film rotation exactly and always lands on the face.
    ind = Image.new("RGBA", (fs, fs), (0, 0, 0, 0))
    idr = ImageDraw.Draw(ind)
    # Compass convention: angle 0 = up, +90 = right. (sin, -cos) gives the
    # direction in image coordinates (y grows downward).
    rad   = math.radians(angle_deg)
    reach = r_face * 0.86
    tip   = (cx + math.sin(rad) * reach,       cy - math.cos(rad) * reach)
    base  = (cx + math.sin(rad) * reach * 0.2, cy - math.cos(rad) * reach * 0.2)
    idr.line([base, tip], fill=INDICATOR + (255,), width=max(2, fs // 30))
    dot = fs * 0.05
    idr.ellipse(
        [tip[0] - dot, tip[1] - dot, tip[0] + dot, tip[1] + dot],
        fill=INDICATOR + (255,),
    )
    img.alpha_composite(ind)

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
