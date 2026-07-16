#!/usr/bin/make -f
# Top-level Makefile for the Sitar plugin project
# -----------------------------------------------

PLUGIN_DIR := plugins/Sitar

# Set BETA=1 to produce a side-by-side beta build: distinct LV2 URI,
# bundle name, brand, and unique id. Same source, different identity —
# install with `make BETA=1 install` (or the `make beta` shortcut) and
# it'll coexist with the stable plugin in MOD Desktop. Used to A/B test
# a work-in-progress against the released plugin.
ifeq ($(BETA),1)
export SITAR_BETA := 1
BUNDLE_NAME := sitar-beta
BUNDLE_LABEL := Sympathetic Sitar (Beta)
PLUGIN_URI   := http://sitar.local/plugins/sitar-beta
else
BUNDLE_NAME := sitar
BUNDLE_LABEL := Sympathetic Sitar
PLUGIN_URI   := http://sitar.local/plugins/sitar
endif
BUNDLE := bin/$(BUNDLE_NAME).lv2

.PHONY: all plugin ttl modgui clean distclean

all: plugin ttl modgui

# ---------------------------------------------------------------------------------------------------------------------
# Build the plugin binary

plugin:
	$(MAKE) -C $(PLUGIN_DIR)

# ---------------------------------------------------------------------------------------------------------------------
# Generate manifest.ttl and sitar.ttl via DPF's TTL generator

ttl: plugin dpf/utils/lv2_ttl_generator
	@$(CURDIR)/dpf/utils/generate-ttl.sh

dpf/utils/lv2_ttl_generator:
	$(MAKE) -C dpf/utils/lv2-ttl-generator

# ---------------------------------------------------------------------------------------------------------------------
# Copy the MOD Web GUI assets and modgui.ttl into the bundle, then patch manifest.ttl to load them.

modgui: ttl
	@mkdir -p $(BUNDLE)/modgui
	cp -f $(PLUGIN_DIR)/modgui/*.html $(BUNDLE)/modgui/
	cp -f $(PLUGIN_DIR)/modgui/*.css  $(BUNDLE)/modgui/
	cp -f $(PLUGIN_DIR)/modgui/*.js   $(BUNDLE)/modgui/
	cp -f $(PLUGIN_DIR)/modgui/*.png  $(BUNDLE)/modgui/
	cp -rf $(PLUGIN_DIR)/modgui/knobs $(BUNDLE)/modgui/
	@# Patch the modgui.ttl with the current build's URI / brand / label.
	@# Source TTL contains the stable identity; sed swaps it out when BETA=1
	@# (no-op when BETA is unset, since the substitutions become identity).
	sed -e 's|http://sitar.local/plugins/sitar|$(PLUGIN_URI)|g' \
	    -e 's|modgui:brand "sitar"|modgui:brand "$(BUNDLE_NAME)"|' \
	    -e 's|modgui:label "Sympathetic Sitar"|modgui:label "$(BUNDLE_LABEL)"|' \
	    $(PLUGIN_DIR)/modgui.ttl > $(BUNDLE)/modgui.ttl
	@if ! grep -q 'modgui.ttl' $(BUNDLE)/manifest.ttl; then \
		printf '\n<%s>\n    rdfs:seeAlso <modgui.ttl> .\n' \
			"$(PLUGIN_URI)" >> $(BUNDLE)/manifest.ttl; \
	fi

# ---------------------------------------------------------------------------------------------------------------------

clean:
	$(MAKE) clean -C $(PLUGIN_DIR)
	$(MAKE) clean -C dpf/utils/lv2-ttl-generator
	rm -rf bin build

distclean: clean
	rm -rf dpf/utils/lv2_ttl_generator dpf/utils/lv2_ttl_generator.exe

# ---------------------------------------------------------------------------------------------------------------------
# install: copy the built sitar.lv2 bundle into $(DESTDIR)$(PREFIX)/lib/lv2/.
# Used by mod-plugin-builder (which sets DESTDIR=$(TARGET_DIR)) and by anyone
# wanting to install directly:  sudo make install PREFIX=/usr/local

PREFIX     ?= /usr
LV2_DIR    ?= $(DESTDIR)$(PREFIX)/lib/lv2

install: all
	@mkdir -p "$(LV2_DIR)"
	cp -rL "$(BUNDLE)" "$(LV2_DIR)/"

# ---------------------------------------------------------------------------------------------------------------------
# manual: render the beginner PDF manual from its HTML source via headless
# Chrome. The PDF is generated output — edit the HTML, re-run this, and
# commit BOTH files (the release attaches the committed PDF).

MANUAL_HTML := docs/manual/sitar-manual.html
MANUAL_PDF  := docs/manual/sitar-manual.pdf
CHROME      ?= google-chrome

manual:
	$(CHROME) --headless --disable-gpu --no-pdf-header-footer \
		--print-to-pdf=$(MANUAL_PDF) $(MANUAL_HTML)
	@echo "==> $(MANUAL_PDF)"

.PHONY: manual

# ---------------------------------------------------------------------------------------------------------------------
# test: build + run every tests/test_*.cpp under AddressSanitizer. Each test
# is a plain main() compiled directly against the plugin source and DPF's
# core — no host, no test framework. Binaries land in build/tests/
# (gitignored); each test file's header comment documents what it pins down.

TEST_CXXFLAGS := -std=gnu++14 -g -O0 -fsanitize=address \
                 -I$(PLUGIN_DIR) -Idpf/distrho
TEST_SRCS     := $(wildcard $(PLUGIN_DIR)/*.cpp) dpf/distrho/src/DistrhoPlugin.cpp

test:
	@mkdir -p build/tests
	@set -e; for t in tests/test_*.cpp; do \
		bin=build/tests/$$(basename $$t .cpp); \
		echo "==> $$t"; \
		g++ $(TEST_CXXFLAGS) $$t $(TEST_SRCS) -o $$bin; \
		$$bin; \
	done
	@echo "==> all tests passed"

.PHONY: test

# Convenience shortcuts for the side-by-side beta variant.
# `make beta`           — build bin/sitar-beta.lv2 (does NOT install)
# `make install-beta`   — build + copy to MOD Desktop's user-plugin dir
#                          (or wherever install.sh's MOD_DESKTOP_PLUGINS points)
beta:
	$(MAKE) BETA=1

install-beta:
	$(MAKE) BETA=1
	BETA=1 ./install.sh

.PHONY: install beta install-beta

# ---------------------------------------------------------------------------------------------------------------------
# MOD Dwarf cross-build — self-contained, no host workdir or external clones.
#
# Everything lives in a vendored Docker image (mod-build/Dockerfile) that
# bakes in the mod-plugin-builder cross-toolchain (aarch64, glibc 2.27,
# gcc 9.4.0 — matching Dwarf firmware). First `make dwarf-image` is slow
# (~30-60 min, one-time per machine). After that, `make dwarf-build` is
# ~10s and produces build/dwarf/sitar.lv2 ready to scp.
#
# Override on the command line as needed, e.g.
#   make dwarf-deploy DWARF_HOST=sitar.local DWARF_USER=admin
SITAR_IMAGE  ?= sitar-cross
DWARF_HOST   ?= 192.168.51.1
DWARF_USER   ?= root
DWARF_LV2DIR ?= /root/.lv2

DWARF_BUNDLE := build/dwarf/sitar.lv2

# 1. Build the cross-toolchain image. One-time, ~30-60 min, cached forever.
dwarf-image:
	docker build -t $(SITAR_IMAGE) mod-build/

# 2. Cross-build the plugin. Runs build-sitar.sh inside the image, which
#    does a native build for .ttl/modgui assets and a cross-build for the
#    aarch64 .so, dropping the assembled bundle into build/dwarf/sitar.lv2.
dwarf-build:
	@if ! docker image inspect $(SITAR_IMAGE) >/dev/null 2>&1; then \
		echo "==> $(SITAR_IMAGE) image not built yet — building (~30-60 min, one-time)"; \
		$(MAKE) dwarf-image; \
	fi
	@mkdir -p build/dwarf
	docker run --rm \
		-e HOST_UID=$$(id -u) -e HOST_GID=$$(id -g) \
		-v "$(CURDIR):/src:ro" \
		-v "$(CURDIR)/build/dwarf:/out" \
		$(SITAR_IMAGE) \
		bash /src/mod-build/build-sitar.sh

# 3. Push the bundle to a connected Dwarf via scp. The Dwarf's `/root/.lv2/`
#    is the per-user plugin dir and survives firmware updates.
#
# Note `-O`: the Dwarf runs Dropbear SSH, which has no SFTP subsystem.
# Modern OpenSSH scp (>= 9.0) defaults to SFTP and fails with "subsystem
# request failed on channel 0". `-O` forces the legacy scp protocol.
dwarf-deploy:
	@if [ ! -d "$(DWARF_BUNDLE)" ]; then \
		echo "error: no bundle at $(DWARF_BUNDLE) — run 'make dwarf-build' first."; \
		exit 1; \
	fi
	scp -O -r "$(DWARF_BUNDLE)" "$(DWARF_USER)@$(DWARF_HOST):$(DWARF_LV2DIR)/"
	@echo "==> Restarting jack2 + mod-ui so the new bundle is picked up"
	@# Two independent lilv plugin caches on the Dwarf:
	@#   - jack2 hosts mod-host internally (visible in logs as 'mod-jackd')
	@#     and caches the plugin world at startup. Without this restart the
	@#     pedalboard fails with "can't get plugin" / "Error adding effect".
	@#   - mod-ui (the web UI) has its OWN cache used to render the plugin
	@#     library, port lists, and modgui. Without this restart the UI
	@#     shows the *old* port set ("No such symbol: gate/level/...") even
	@#     though jack2 picked up the new bundle correctly.
	@# Audio drops for ~1-2 s while jack2 comes back. After the deploy you
	@# also need to hard-refresh the browser (Ctrl-Shift-R) so mod-ui's
	@# JS-side plugin metadata isn't served from the browser cache.
	ssh "$(DWARF_USER)@$(DWARF_HOST)" 'systemctl restart jack2 mod-ui'

# Convenience: build then deploy.
dwarf: dwarf-build dwarf-deploy

.PHONY: dwarf dwarf-build dwarf-image dwarf-deploy

# ---------------------------------------------------------------------------------------------------------------------
# release: build desktop + dwarf bundles locally, package them, tag the
# current commit as v$(version), push, and create a GitHub release with
# both bundles attached as downloadable assets.
#
#   make release version=0.0.4
#
# We build the artefacts locally (instead of from CI) because the Dwarf
# cross-toolchain docker image takes ~30-60 min to assemble from scratch
# and is hard to cache reliably on a fresh GH Actions runner. Locally
# the image is already there and `make dwarf-build` is ~10 s, so doing
# the publish from the developer machine is both faster and simpler.
#
# Refuses to run on a dirty tree, if the tag already exists, or if the
# bundles fail to build — so half-published releases are hard to make.
#
# The plugin version reported to hosts comes from the VERSION file (see
# plugins/Sitar/Makefile); this target bumps + commits that file to
# $(version) before building, so every release automatically embeds its
# own version — no manual source edit needed.

DIST_DIR := dist
LINUX_TARBALL := sitar-v$(version)-linux-x86_64.tar.gz
DWARF_TARBALL := sitar-v$(version)-dwarf-aarch64.tar.gz

# Build + package both bundles for a release. Doesn't tag or push; useful
# on its own for testing what the assets look like before publishing.
# Requires the VERSION file to already say $(version) so the tarball name
# can't disagree with the version baked into the binaries — `make release`
# bumps the file automatically.
release-build:
	@if [ -z "$(version)" ]; then \
		echo "error: version is required."; \
		echo "       usage: make release-build version=x.y.z"; \
		exit 1; \
	fi
	@if [ "$$(cat VERSION)" != "$(version)" ]; then \
		echo "error: VERSION file says $$(cat VERSION), but version=$(version)."; \
		echo "       'make release version=$(version)' bumps it automatically,"; \
		echo "       or update the VERSION file first."; \
		exit 1; \
	fi
	@echo "==> Building desktop bundle (Linux x86_64)"
	$(MAKE) clean all
	@mkdir -p $(DIST_DIR)
	tar -C bin -czf $(DIST_DIR)/$(LINUX_TARBALL) sitar.lv2
	@echo "==> Building Dwarf bundle (aarch64)"
	$(MAKE) dwarf-build
	tar -C build/dwarf -czf $(DIST_DIR)/$(DWARF_TARBALL) sitar.lv2
	@echo
	@echo "Built release artefacts in $(DIST_DIR)/:"
	@ls -lh $(DIST_DIR)/sitar-v$(version)-*.tar.gz

# The version flows from here into everything else: the VERSION file is
# bumped and committed BEFORE the build, so the binaries, the generated
# TTL (lv2:minorVersion / lv2:microVersion) and the tag all agree.
release:
	@if [ -z "$(version)" ]; then \
		echo "error: version is required."; \
		echo "       usage: make release version=x.y.z"; \
		exit 1; \
	fi
	@if [ -n "$$(git status --porcelain)" ]; then \
		echo "error: working tree is dirty. Commit or stash first."; \
		git status --short; \
		exit 1; \
	fi
	@if git rev-parse -q --verify "refs/tags/v$(version)" >/dev/null 2>&1; then \
		echo "error: tag v$(version) already exists locally."; \
		exit 1; \
	fi
	@if [ "$$(cat VERSION)" != "$(version)" ]; then \
		echo "==> Bumping VERSION $$(cat VERSION) -> $(version)"; \
		printf '%s\n' "$(version)" > VERSION; \
		git add VERSION; \
		git commit -m "Bump version to $(version)"; \
	fi
	$(MAKE) release-build version=$(version)
	@echo "==> Pushing branch (so the tagged commit is reachable on origin)"
	git push
	@echo "==> Tagging v$(version)"
	git tag -a "v$(version)" -m "Release v$(version)"
	git push origin "v$(version)"
	@echo "==> Creating GitHub release v$(version) with both bundles + manual attached"
	gh release create "v$(version)" \
		"$(DIST_DIR)/$(LINUX_TARBALL)" \
		"$(DIST_DIR)/$(DWARF_TARBALL)" \
		"$(MANUAL_PDF)" \
		--title "v$(version)" \
		--generate-notes
	@echo
	@echo "Release published:"
	@echo "  https://github.com/$$(git config --get remote.origin.url | sed -E 's|.*[:/]([^:/]+/[^/]+?)(\.git)?$$|\1|')/releases/tag/v$(version)"

.PHONY: release release-build
