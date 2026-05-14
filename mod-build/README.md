# Building Sitar for the MOD Dwarf

This directory contains the `mod-plugin-builder` (MPB) recipe and the workflow
for producing a `.lv2` bundle that runs on the MOD Dwarf (aarch64 hardware).

The recipe is set up for **local-source builds**: the top-level Makefile
bind-mounts this repo into the MPB Docker container, and `mod-plugin-builder`
copies the current working tree as the package source. **No git push or SHA
bump is needed for local iteration** — edit, save, `make dwarf-build`, done.

DPF is already vendored as a git submodule at `dpf/`, so it travels with the
source via the bind mount.

For a fully reproducible *distribution* build (pinned git revision, downloaded
fresh by buildroot), see the comment at the top of `sitar.mk` and swap the
`SITAR_SITE_*` block to git mode.

---

## One-time setup

The only thing you'll ever run by hand is **`make dwarf-bootstrap`** — that
clones `mod-plugin-builder`, builds the Docker build-environment image, and
bootstraps the cross-toolchain (`aarch64-modaudio-linux-gnu-*`) into
`~/mod-workdir/moddwarf-new/`. About 30–60 minutes total. Cached afterwards.

```bash
make dwarf-bootstrap
```

That's it. You don't need to clone mod-plugin-builder, run `docker build`,
or memorise any `docker run` invocations — the Makefile does all of that.

---

## Daily workflow

From the repo root:

```bash
make dwarf-build           # cross-build the .lv2 in Docker  (~20s after bootstrap)
make dwarf-deploy          # POST to the Dwarf at 192.168.51.1
make dwarf                 # both, in sequence
```

Override defaults on the command line:

| Variable      | Default                       | Purpose |
|---------------|-------------------------------|---------|
| `MPB_DIR`     | `~/mod-plugin-builder`        | Where you cloned mod-plugin-builder. |
| `MPB_WORKDIR` | `~/mod-workdir`               | MPB's per-platform output directory. |
| `MPB_IMAGE`   | `mpb-moddwarf`                | Tag of the Docker image you built. |
| `MPB_PLATFORM`| `moddwarf-new`                | Target. Use `moddwarf` for older Dwarf firmware. |
| `DWARF_HOST`  | `192.168.51.1`                | Where to POST the bundle. |

Example:

```bash
make dwarf MPB_DIR=/opt/mpb DWARF_HOST=sitar.local
```

The resulting bundle lands at `~/mod-workdir/moddwarf-new/plugins/sitar.lv2/`.

`make dwarf-build` always issues a `sitar-dirclean` first; this is needed
because buildroot caches the extracted source from the previous run and
won't re-copy your edits otherwise. The dirclean only nukes the per-package
build dir, not the toolchain, so a full rebuild is still ~10–20 seconds.

---

## Install on a MOD Dwarf

`make dwarf-deploy` runs the canonical install flow:

```bash
cd ~/mod-workdir/moddwarf-new/plugins
tar czf - sitar.lv2 | base64 \
  | curl -F 'package=@-' http://192.168.51.1/sdk/install
```

The `/sdk/install` endpoint accepts a base64-encoded tar.gz of the bundle(s)
and installs them on the device. The Dwarf web UI shows the "Sympathetic
Sitar" plugin immediately, no reboot needed.

---

## Switching to a reproducible distribution build

When you're ready to ship a fixed version, switch `mod-build/sitar.mk` to
pull from your published git repo:

```mk
SITAR_VERSION = <git sha>
SITAR_SITE    = https://github.com/YOUR_USER/sitar.git
SITAR_SITE_METHOD = git
SITAR_PRE_DOWNLOAD_HOOKS += MOD_PLUGIN_BUILDER_DOWNLOAD_WITH_SUBMODULES
```

(Delete the `local`-mode lines.) Now `make dwarf-build` will tell MPB to
clone the pinned revision from GitHub (along with the DPF submodule) and
build that. The bind-mount of `$(CURDIR):/home/builder/sitar-src` in the
Makefile becomes inert — harmless to leave in.

---

## Troubleshooting

- **Docker image build fails with apt errors**: the Dockerfile uses
  `dpkg --add-architecture i386/armhf/arm64`. On some hosts you need
  `--network=host` to reach apt mirrors:
  `docker build --network=host --build-arg platform=moddwarf-new -t mpb-moddwarf docker/`
- **Source edits don't show up in the build**: should never happen now that
  `dwarf-build` does `sitar-dirclean` automatically. If it does anyway, run
  `make dwarf-clean` manually and try again.
- **TTL generation fails inside the build**: the cross-built
  `lv2_ttl_generator` runs under `qemu-aarch64-static` via DPF's
  `$EXE_WRAPPER`. The Dockerfile installs `qemu-user-static`; if you skipped
  that line, plugins won't be able to emit their TTL.
- **Plugin shows in the Dwarf list but won't drag onto the pedalboard**:
  same modgui/knob-sprite issue we hit in MOD Desktop. Check
  `~/mod-workdir/moddwarf-new/plugins/sitar.lv2/modgui/` and make sure all
  the assets (icon-sitar.html, stylesheet-sitar.css, script-sitar.js,
  screenshot-sitar.png, thumbnail-sitar.png, knobs/sitar-knob.png) ended up
  there.
