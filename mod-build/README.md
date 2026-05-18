# Building Sitar for the MOD Dwarf

This directory contains everything needed to cross-compile a `.lv2` bundle
that runs on the MOD Dwarf (aarch64 hardware). It is **self-contained**:
the only host-side dependency is Docker. No global mod-plugin-builder
clone, no `~/mod-workdir`, no bootstrap of host state.

## Files

- **`Dockerfile`** — vendored from `moddevices/mod-plugin-builder`. Builds
  a Debian Bookworm image, then internally clones MPB and runs its
  bootstrap to produce a buildroot-based aarch64 cross-toolchain
  targeting `glibc 2.27` + `gcc 9.4.0`. Tag: `sitar-cross:latest`.
- **`build-sitar.sh`** — runs *inside* the container. Does a native
  (x86_64) build of the plugin to produce DPF's introspected `.ttl`
  metadata, then a cross-build of the `.so`, then copies the assembled
  bundle to `/out` (which the Makefile mounts to `bin/dwarf/`).

## One-time setup

```bash
make dwarf-image          # ~30-60 min, cached forever after
```

Builds the image. Most of the time is spent inside MPB's bootstrap
scripts compiling the aarch64 cross-toolchain via crosstool-ng. After
this completes, every cross-build is ~10s.

## Daily workflow

```bash
make dwarf-build          # produce bin/dwarf/sitar.lv2 (aarch64)
make dwarf-deploy         # scp to a connected Dwarf
make dwarf                # both, in sequence
```

`make dwarf-build` auto-builds the image if it doesn't exist, so on a
fresh machine `make dwarf` alone is enough — it'll just take an hour the
first time.

Override defaults on the command line:

| Variable        | Default          | Purpose |
|-----------------|------------------|---------|
| `SITAR_IMAGE`   | `sitar-cross`    | Docker image tag. |
| `DWARF_HOST`    | `192.168.51.1`   | Hostname/IP of the connected Dwarf. |
| `DWARF_USER`    | `root`           | SSH user on the Dwarf. |
| `DWARF_LV2DIR`  | `/root/.lv2`     | Plugin install dir on the Dwarf. |

Example:

```bash
make dwarf DWARF_HOST=sitar.local
```

## Why two builds (native + cross)?

DPF's `lv2_ttl_generator` is a helper binary that `dlopen()`s the plugin
to introspect its ports and emit `manifest.ttl` / `sitar.ttl`. It has to
run on the same architecture as the plugin. Inside the container we have
a native x86_64 toolchain (from Debian) *and* an aarch64 cross-toolchain
(from MPB). We use the native one to generate the TTLs, then swap in the
cross-built `.so` to assemble an aarch64 bundle.

## Resyncing with upstream MPB

The Dockerfile is a near-verbatim copy of
`moddevices/mod-plugin-builder/docker/Dockerfile`, hard-locked to
`moddwarf-new`. If upstream MPB changes the bootstrap, `diff` against the
upstream file and cherry-pick. The platform-specific `if test ...` blocks
have been collapsed in our copy; if you ever need to target `modduo` or
`modduox` instead, restore them from upstream.

## Troubleshooting

- **`docker build` fails with apt errors**: try `--network=host`:
  `docker build --network=host -t sitar-cross mod-build/`
- **Image build runs out of disk**: the toolchain is ~5 GB and the
  buildroot tree is another ~5 GB. Free up at least 15 GB before starting.
- **`build-sitar.sh` complains the cross-toolchain is missing**: the
  image was built but MPB's bootstrap didn't complete. Rebuild with
  `docker build --no-cache -t sitar-cross mod-build/`.
- **Plugin loads but won't drag onto the pedalboard**: check that
  `bin/dwarf/sitar.lv2/modgui/` has all the assets (icon, stylesheet,
  script, screenshot, knobs/). The native build in step 1 of
  `build-sitar.sh` should have populated them via the top-level
  `modgui` target — if they're missing, the host `make all` step is
  silently failing.
