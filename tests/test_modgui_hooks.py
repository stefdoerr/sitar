#!/usr/bin/env python3
"""
Regression guard for the MOD pedal face (plugins/Sitar/modgui/icon-sitar.html).

The face can be restyled freely, but mod-ui and script-sitar.js find every
control by its `mod-role` / `mod-port-symbol` attributes, and the dropdowns'
option values are looked up in parallel arrays in script-sitar.js. Dropping or
renaming one of those silently breaks a control (or, for ports, saved
pedalboards). This pins them.

Run: python3 tests/test_modgui_hooks.py   (also part of `make test`)
"""

import os
import re

ROOT   = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MODGUI = os.path.join(ROOT, "plugins", "Sitar", "modgui")
html   = open(os.path.join(MODGUI, "icon-sitar.html"), encoding="utf-8").read()
js     = open(os.path.join(MODGUI, "script-sitar.js"), encoding="utf-8").read()
# The stylesheet is a Mustache template: swap its {{{cns}}}/{{{ns}}} tokens out so
# their braces don't confuse the rule parser below.
css    = re.sub(r"/\*.*?\*/", "", open(os.path.join(MODGUI, "stylesheet-sitar.css"),
              encoding="utf-8").read(), flags=re.DOTALL) \
           .replace("{{{cns}}}", "__CNS__").replace("{{{ns}}}", "")
SCOPE  = ".sitar-pedal__CNS__"     # mod-ui renders {{{cns}}} per plugin URI + version

CONTROL_PORTS = {
    "gate", "sensitivity", "num_active", "octave", "decay", "bloom", "jawari",
    "lead_jawari", "mix", "level", "strum_time", "strum_level",
    "audition", "strum_up", "strum_down",
}
ROLES = {
    "drag-handle", "bypass", "bypass-light", "sitar-edit-open", "sitar-scale",
    "sitar-root", "sitar-stereo", "sitar-editor", "sitar-edit-close",
    "sitar-edit-slot", "sitar-edit-name", "sitar-edit-ivals", "sitar-edit-save",
    "sitar-edit-clear", "input-audio-port", "output-audio-port",
}


def tags(attr_re):
    """Every opening tag whose attributes match attr_re."""
    return [t for t in re.findall(r"<[a-zA-Z][^>]*>", html) if re.search(attr_re, t)]


def attr(tag, name):
    m = re.search(name + r'="([^"]*)"', tag)
    return m.group(1) if m else None


def css_rules():
    """(selector, body) of every plain rule (inside @media too; @font-face skipped)."""
    return [(sel.strip(), body) for sel, body in re.findall(r"([^{}]+)\{([^{}]*)\}", css)
            if not sel.strip().startswith("@")]


def z_index(selector_part):
    zs = [int(m.group(1)) for sel, body in css_rules() if selector_part in sel
          for m in [re.search(r"z-index:\s*(-?\d+)", body)] if m]
    return zs[-1] if zs else None


def js_array(name):
    body = re.search(r"var " + name + r"\s*=\s*\[(.*?)\];", js, re.DOTALL).group(1)
    return re.findall(r"'([^']*)'", body)


def select_options(role):
    m = re.search(r'<select[^>]*mod-role="' + role + r'"[^>]*>(.*?)</select>', html, re.DOTALL)
    assert m, f"no <select mod-role={role}>"
    return re.findall(r'<option value="([^"]*)"', m.group(1))


def test_control_ports():
    ports = {attr(t, "mod-port-symbol") for t in tags(r'mod-role="input-control-port"')}
    assert ports == CONTROL_PORTS, f"missing {CONTROL_PORTS - ports}, extra {ports - CONTROL_PORTS}"


def test_roles():
    roles = {attr(t, "mod-role") for t in tags(r"mod-role=")}
    assert ROLES <= roles, f"missing roles {ROLES - roles}"


def test_dropdown_values_match_script():
    assert select_options("sitar-scale") == js_array("SCALE_KEYS")
    assert select_options("sitar-root") == js_array("ROOT_VALUES")
    assert select_options("sitar-stereo") == js_array("STEREO_KEYS")


def test_styles_scoped_to_this_plugin_version():
    # mod-ui keeps every loaded plugin version's stylesheet in <head>, so a
    # stable and a beta Sitar on one page would restyle each other unless every
    # selector (and the face's root class) carries the per-version {{{cns}}}.
    root = re.search(r'<div class="([^"]*)"', html).group(1).split()
    assert "sitar-pedal{{{cns}}}" in root and "sitar-pedal" not in root, root
    for sel, _ in css_rules():
        for part in sel.split(","):
            assert part.strip().startswith(SCOPE), f"unscoped selector: {part.strip()!r}"


def test_fonts_beat_modui_important():
    # mod-ui's main.css has *{font-family:"cooper hewitt",Sans-serif !important};
    # a font-family without !important never applies on a real pedalboard.
    decls = [(sel, d) for sel, body in css_rules() for d in re.findall(r"font-family:[^;]*", body)]
    assert decls, "no font-family declarations"
    for sel, d in decls:
        assert d.rstrip().endswith("!important"), f"{sel}: {d}"
    joined = " ".join(d for _, d in decls)
    assert "SitarRakkas" in joined and "SitarLato" in joined


def test_footswitch_below_editor():
    # pedals.css gives .mod-footswitch z-index 20; ours must stay under the open
    # scale editor or its invisible hit area bypasses the effect.
    fs, ed = z_index(".mod-footswitch"), z_index(".sitar-editor")
    assert fs is not None and ed is not None and fs < ed, (fs, ed)


if __name__ == "__main__":
    import sys
    failed = 0
    for name, fn in list(globals().items()):
        if name.startswith("test_"):
            try:
                fn()
                print("ok  ", name)
            except AssertionError as e:
                failed += 1
                print("FAIL", name, "-", e)
    if failed:
        sys.exit(f"modgui hooks: {failed} failed")
    print("modgui hooks: all passed")
