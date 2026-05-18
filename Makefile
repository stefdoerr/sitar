#!/usr/bin/make -f
# Top-level Makefile for the Sitar plugin project
# -----------------------------------------------

PLUGIN_DIR := plugins/Sitar
BUNDLE     := bin/sitar.lv2

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
	cp -f $(PLUGIN_DIR)/modgui.ttl    $(BUNDLE)/modgui.ttl
	@if ! grep -q 'modgui.ttl' $(BUNDLE)/manifest.ttl; then \
		printf '\n<%s>\n    rdfs:seeAlso <modgui.ttl> .\n' \
			"http://sitar.local/plugins/sitar" >> $(BUNDLE)/manifest.ttl; \
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

.PHONY: install

# ---------------------------------------------------------------------------------------------------------------------
# MOD Dwarf cross-build — self-contained, no host workdir or external clones.
#
# Everything lives in a vendored Docker image (mod-build/Dockerfile) that
# bakes in the mod-plugin-builder cross-toolchain (aarch64, glibc 2.27,
# gcc 9.4.0 — matching Dwarf firmware). First `make dwarf-image` is slow
# (~30-60 min, one-time per machine). After that, `make dwarf-build` is
# ~10s and produces bin/dwarf/sitar.lv2 ready to scp.
#
# Override on the command line as needed, e.g.
#   make dwarf-deploy DWARF_HOST=sitar.local DWARF_USER=admin
SITAR_IMAGE  ?= sitar-cross
DWARF_HOST   ?= 192.168.51.1
DWARF_USER   ?= root
DWARF_LV2DIR ?= /root/.lv2

DWARF_BUNDLE := bin/dwarf/sitar.lv2

# 1. Build the cross-toolchain image. One-time, ~30-60 min, cached forever.
dwarf-image:
	docker build -t $(SITAR_IMAGE) mod-build/

# 2. Cross-build the plugin. Runs build-sitar.sh inside the image, which
#    does a native build for .ttl/modgui assets and a cross-build for the
#    aarch64 .so, dropping the assembled bundle into bin/dwarf/sitar.lv2.
dwarf-build:
	@if ! docker image inspect $(SITAR_IMAGE) >/dev/null 2>&1; then \
		echo "==> $(SITAR_IMAGE) image not built yet — building (~30-60 min, one-time)"; \
		$(MAKE) dwarf-image; \
	fi
	@mkdir -p bin/dwarf
	docker run --rm \
		-e HOST_UID=$$(id -u) -e HOST_GID=$$(id -g) \
		-v "$(CURDIR):/src:ro" \
		-v "$(CURDIR)/bin/dwarf:/out" \
		$(SITAR_IMAGE) \
		bash /src/mod-build/build-sitar.sh

# 3. Push the bundle to a connected Dwarf via scp. The Dwarf's `/root/.lv2/`
#    is the per-user plugin dir and survives firmware updates.
dwarf-deploy:
	@if [ ! -d "$(DWARF_BUNDLE)" ]; then \
		echo "error: no bundle at $(DWARF_BUNDLE) — run 'make dwarf-build' first."; \
		exit 1; \
	fi
	scp -r "$(DWARF_BUNDLE)" "$(DWARF_USER)@$(DWARF_HOST):$(DWARF_LV2DIR)/"

# Convenience: build then deploy.
dwarf: dwarf-build dwarf-deploy

.PHONY: dwarf dwarf-build dwarf-image dwarf-deploy

# ---------------------------------------------------------------------------------------------------------------------
# release: tag the current commit as v$(version) and push the tag, which
# triggers the GitHub Actions release workflow.
#
#   make release version=0.1.0
#
# Refuses to run on a dirty tree or if the tag already exists, so accidental
# releases are hard to make.

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
	@echo "==> Pushing branch (so the tagged commit is reachable on origin)"
	git push
	@echo "==> Tagging v$(version)"
	git tag -a "v$(version)" -m "Release v$(version)"
	git push origin "v$(version)"
	@echo
	@echo "Tag v$(version) pushed. CI will build and publish the release shortly:"
	@echo "  https://github.com/$$(git config --get remote.origin.url | sed -E 's|.*[:/]([^:/]+/[^/]+?)(\.git)?$$|\1|')/actions"

.PHONY: release
