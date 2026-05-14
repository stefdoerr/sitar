#!/usr/bin/env python3
"""
Generate screenshot-sitar.png and thumbnail-sitar.png for the MOD pedalboard view.

These are static images used to render the pedal in MOD's pedalboard scene.
The live interactive view is driven by icon-sitar.html + stylesheet-sitar.css,
so this script only needs to produce a recognizable still that matches the
visual identity of the pedal (dark teak wood, brass title, 13 string knobs).

Run from this directory:
    python3 generate_images.py

Output: screenshot-sitar.png (640x280), thumbnail-sitar.png (64x90)
"""

import math
import os
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))

# --- Palette (matches stylesheet-sitar.css) ---
WOOD_TOP        = (90, 50, 32)
WOOD_BOTTOM     = (44, 23, 12)
WOOD_HIGHLIGHT  = (255, 200, 120, 46)   # warm rim light
INK             = (244, 227, 194)
INK_DIM         = (244, 227, 194, 175)
BRASS           = (245, 217, 154)
KNOB_RIM        = (90, 60, 28)
KNOB_FACE_TOP   = (210, 175, 110)
KNOB_FACE_BOT   = (138, 99, 47)
KNOB_INDICATOR  = (40, 22, 8)
PANEL_DARK      = (24, 12, 6, 180)


def _find_font(size, bold=False):
    """Return a TrueType font of the requested size, falling back gracefully."""
    candidates_bold = [
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
    ]
    candidates = [
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
    ]
    for path in (candidates_bold if bold else candidates):
        if os.path.exists(path):
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


def _wood_background(width, height):
    """A diagonal teak-toned gradient with warm rim highlight + subtle grain."""
    img = Image.new("RGBA", (width, height), WOOD_BOTTOM + (255,))
    px = img.load()
    for y in range(height):
        for x in range(width):
            # Diagonal gradient parameter in [0,1].
            t = (x / max(1, width) + y / max(1, height)) / 2.0
            r = int(WOOD_TOP[0]   * (1 - t) + WOOD_BOTTOM[0]   * t)
            g = int(WOOD_TOP[1]   * (1 - t) + WOOD_BOTTOM[1]   * t)
            b = int(WOOD_TOP[2]   * (1 - t) + WOOD_BOTTOM[2]   * t)
            px[x, y] = (r, g, b, 255)

    # Warm highlight glow in the upper-left.
    glow = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    cx, cy = width * 0.22, height * 0.0
    rx, ry = width * 0.55, height * 0.85
    gd.ellipse([cx - rx, cy - ry, cx + rx, cy + ry], fill=(255, 200, 120, 55))
    glow = glow.filter(ImageFilter.GaussianBlur(radius=width * 0.08))
    img.alpha_composite(glow)

    # Subtle wood grain — thin vertical strokes with slight angle.
    grain = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    gd = ImageDraw.Draw(grain)
    for x in range(-height, width, 4):
        gd.line([(x, 0), (x + height * 0.03, height)], fill=(255, 230, 180, 8), width=1)
    img.alpha_composite(grain)

    return img


def _rounded_rect_mask(width, height, radius):
    mask = Image.new("L", (width, height), 0)
    md = ImageDraw.Draw(mask)
    md.rounded_rectangle([0, 0, width - 1, height - 1], radius=radius, fill=255)
    return mask


def _draw_knob(draw, center, radius, indicator_angle_deg):
    """Draw a small round knob with a dark indicator notch."""
    cx, cy = center
    # Outer rim (slightly larger, dark).
    draw.ellipse(
        [cx - radius - 1, cy - radius - 1, cx + radius + 1, cy + radius + 1],
        fill=KNOB_RIM,
    )
    # Face — radial gradient via two ellipses.
    draw.ellipse(
        [cx - radius, cy - radius, cx + radius, cy + radius],
        fill=KNOB_FACE_BOT,
    )
    inner = max(2, int(radius * 0.7))
    draw.ellipse(
        [cx - inner, cy - inner + 1, cx + inner, cy + inner + 1],
        fill=KNOB_FACE_TOP,
    )
    # Indicator dot.
    rad = math.radians(indicator_angle_deg)
    ix = cx + math.cos(rad) * (radius * 0.62)
    iy = cy + math.sin(rad) * (radius * 0.62)
    dot = max(1, int(radius * 0.22))
    draw.ellipse(
        [ix - dot, iy - dot, ix + dot, iy + dot],
        fill=KNOB_INDICATOR,
    )


def render_screenshot(path, width=640, height=320):
    bg = _wood_background(width, height)
    draw = ImageDraw.Draw(bg)

    # Rim highlight (thin brass outline).
    draw.rounded_rectangle(
        [1, 1, width - 2, height - 2],
        radius=14,
        outline=(255, 200, 130, 60),
        width=2,
    )

    # Title.
    title_font  = _find_font(20, bold=True)
    sub_font    = _find_font(10, bold=False)
    draw.text((width / 2, 24), "SYMPATHETIC SITAR",
              fill=BRASS, anchor="mm", font=title_font)
    draw.text((width / 2, 42), "13-STRING MICROTONAL RESONATOR",
              fill=INK_DIM[:3], anchor="mm", font=sub_font)

    # --- Strings panel (the 13 knobs) ---
    panel_y0, panel_y1 = 80, 170
    panel_x0, panel_x1 = 32, width - 32
    panel = Image.new("RGBA", (panel_x1 - panel_x0, panel_y1 - panel_y0), (0, 0, 0, 0))
    pd = ImageDraw.Draw(panel)
    pd.rounded_rectangle(
        [0, 0, panel.width - 1, panel.height - 1],
        radius=8,
        fill=PANEL_DARK,
        outline=(245, 217, 154, 30),
        width=1,
    )
    bg.alpha_composite(panel, (panel_x0, panel_y0))

    # 13 string knobs in a row.
    knob_radius = 14
    spacing = (panel_x1 - panel_x0 - 24) / 12
    string_label_font = _find_font(9, bold=False)
    for i in range(13):
        cx = panel_x0 + 12 + i * spacing
        cy = panel_y0 + (panel_y1 - panel_y0) / 2 - 6
        # Simulate "tuned" angles — sweep across as if a scale is loaded.
        angle = -200 + (i / 12) * 220  # roughly -200..20deg
        _draw_knob(draw, (cx, cy), knob_radius, angle)
        draw.text((cx, cy + knob_radius + 10), str(i + 1),
                  fill=INK_DIM[:3], anchor="mm", font=string_label_font)

    # --- Tone knobs at the bottom: Decay / Jawari / Mix ---
    tone_y = height - 50
    tone_radius = 18
    tone_labels = ["DECAY", "JAWARI", "MIX"]
    tone_angles = [-30, -110, 30]
    tone_spacing = 110
    tone_cx_start = width / 2 - tone_spacing
    tone_font = _find_font(9, bold=True)
    for i, label in enumerate(tone_labels):
        cx = tone_cx_start + i * tone_spacing
        _draw_knob(draw, (cx, tone_y), tone_radius, tone_angles[i])
        draw.text((cx, tone_y + tone_radius + 11), label,
                  fill=BRASS, anchor="mm", font=tone_font)

    # Mask to rounded corners.
    rounded = _rounded_rect_mask(width, height, 14)
    out = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    out.paste(bg, (0, 0), rounded)

    out.save(path, "PNG", optimize=True)
    print(f"Wrote {path} ({width}x{height})")


def render_thumbnail(path, width=64, height=90):
    """Plugin-browser thumbnail. Built from thumbnail-source.png (a CC-licensed
    cartoon sitar provided by the user) — scaled to fit width x height while
    preserving aspect ratio and centred on a transparent canvas. The MOD plugin
    browser shows this at small size; keep the silhouette readable."""
    src_path = os.path.join(HERE, "src", "thumbnail-source.png")
    src = Image.open(src_path).convert("RGBA")

    # Fit the source into the target box, preserving aspect ratio.
    scale = min(width / src.width, height / src.height)
    new_w = max(1, round(src.width  * scale))
    new_h = max(1, round(src.height * scale))
    src = src.resize((new_w, new_h), Image.LANCZOS)

    out = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    out.paste(src, ((width - new_w) // 2, (height - new_h) // 2), src)
    out.save(path, "PNG", optimize=True)
    print(f"Wrote {path} ({width}x{height}, source {src_path})")


if __name__ == "__main__":
    render_screenshot(os.path.join(HERE, "screenshot-sitar.png"))
    render_thumbnail(os.path.join(HERE, "thumbnail-sitar.png"))
