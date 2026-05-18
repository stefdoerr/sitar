#!/bin/bash
# Self-contained sitar cross-build, run *inside* the sitar-cross image.
#
# Invoked by `make dwarf-build`. The host passes us:
#   /src   — the sitar source tree, mounted read-only
#   /out   — host-side output dir; we drop sitar.lv2 here
#
# What this script does:
#   1. Stage the source into a writable scratch dir.
#   2. Native (x86_64) build to produce the LV2 .ttl metadata + modgui
#      bundle layout. DPF's lv2_ttl_generator dlopen()s the plugin .so
#      to introspect ports, so it has to run against an x86_64 build —
#      it can't introspect an aarch64 .so on an x86_64 host.
#   3. Cross-compile the .so against the moddwarf-new toolchain baked
#      into the image, overwriting the x86_64 .so in the bundle.
#   4. Sanity-check that the resulting bundle has an aarch64 .so, then
#      copy it to /out.

set -euo pipefail

if [ ! -d /src ]; then
    echo "build-sitar.sh: /src not mounted. Run via 'make dwarf-build'." >&2
    exit 1
fi
if [ ! -d /out ]; then
    echo "build-sitar.sh: /out not mounted." >&2
    exit 1
fi

TOOLCHAIN_BIN=/root/mod-workdir/moddwarf-new/host/usr/bin
TOOL_PREFIX=aarch64-modaudio-linux-gnu

if [ ! -x "$TOOLCHAIN_BIN/${TOOL_PREFIX}-gcc" ]; then
    echo "build-sitar.sh: cross-toolchain missing at $TOOLCHAIN_BIN." >&2
    echo "                Re-build the docker image: 'make dwarf-image'." >&2
    exit 1
fi

WORK=/work/sitar
rm -rf "$WORK"
mkdir -p "$WORK"
# rsync gives us a writable copy without touching the read-only mount.
# Exclude build artefacts so the staging dir is clean.
rsync -a --exclude bin --exclude build --exclude '.git' /src/ "$WORK/"
cd "$WORK"

echo "==> [1/3] Native build (for .ttl metadata + modgui assets)"
make -s all
# Stash the populated bundle (.ttl + modgui assets). DPF's clean between
# the native and cross builds wipes bin/, so we have to set this aside.
BUNDLE_STASH=/tmp/sitar-bundle-stash
rm -rf "$BUNDLE_STASH"
cp -rL bin/sitar.lv2 "$BUNDLE_STASH"

echo "==> [2/3] Cross-compiling sitar.so for aarch64"
make -s -C plugins/Sitar clean
make -s -C plugins/Sitar \
    CC="$TOOLCHAIN_BIN/${TOOL_PREFIX}-gcc" \
    CXX="$TOOLCHAIN_BIN/${TOOL_PREFIX}-g++" \
    AR="$TOOLCHAIN_BIN/${TOOL_PREFIX}-ar" \
    LD="$TOOLCHAIN_BIN/${TOOL_PREFIX}-ld" \
    STRIP="$TOOLCHAIN_BIN/${TOOL_PREFIX}-strip" \
    NOOPT=false
"$TOOLCHAIN_BIN/${TOOL_PREFIX}-strip" bin/sitar.lv2/sitar.so

if ! file bin/sitar.lv2/sitar.so | grep -q 'ARM aarch64'; then
    echo "build-sitar.sh: cross-compile did not produce an aarch64 binary." >&2
    file bin/sitar.lv2/sitar.so >&2
    exit 1
fi

echo "==> [3/3] Publishing bundle to /out/sitar.lv2"
# Use the stashed bundle (has the .ttl + modgui) and overlay the aarch64 .so
cp -f bin/sitar.lv2/sitar.so "$BUNDLE_STASH/sitar.so"
rm -rf /out/sitar.lv2
cp -rL "$BUNDLE_STASH" /out/sitar.lv2

# Container runs as root; chown the output back to the host user so it's
# editable / deletable from outside Docker.
if [ -n "${HOST_UID:-}" ] && [ -n "${HOST_GID:-}" ]; then
    chown -R "$HOST_UID:$HOST_GID" /out
fi

echo "==> Done. $(file -b /out/sitar.lv2/sitar.so)"
