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


if __name__ == "__main__":
    for name, fn in list(globals().items()):
        if name.startswith("test_"):
            fn()
            print("ok ", name)
    print("modgui hooks: all passed")
