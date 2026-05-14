#!/usr/bin/env bash
#
# Install the built sitar.lv2 bundle into MOD Desktop's plugin directory.
# The default is the user-data path ($XDG_DOCUMENTS/MOD Desktop/lv2), which
# is the official location for user-installed plugins on Linux and survives
# MOD Desktop reinstalls. To install alongside MOD Desktop's bundled set
# instead, override with:
#   MOD_DESKTOP_PLUGINS=/path/to/mod-desktop/plugins ./install.sh
#

set -euo pipefail

REPO_DIR="$(cd "$(dirname "$0")" && pwd)"
BUNDLE_SRC="$REPO_DIR/bin/sitar.lv2"
DEST="${MOD_DESKTOP_PLUGINS:-$HOME/Documents/MOD Desktop/lv2}"

if [[ ! -d "$BUNDLE_SRC" ]]; then
    echo "error: $BUNDLE_SRC not found." >&2
    echo "       Build the plugin first:  make" >&2
    exit 1
fi

if [[ ! -d "$DEST" ]]; then
    echo "error: destination directory does not exist: $DEST" >&2
    echo "       Either create it, or override the location with MOD_DESKTOP_PLUGINS=..." >&2
    exit 1
fi

DEST_BUNDLE="$DEST/sitar.lv2"

# Remove the previous install so stale TTL or modgui files from older builds
# don't linger alongside fresh ones.
if [[ -e "$DEST_BUNDLE" ]]; then
    echo "Removing previous install: $DEST_BUNDLE"
    rm -rf "$DEST_BUNDLE"
fi

echo "Installing $BUNDLE_SRC"
echo "        -> $DEST_BUNDLE"
cp -r "$BUNDLE_SRC" "$DEST_BUNDLE"

echo
echo "Done. Restart MOD Desktop so it rescans plugins."
