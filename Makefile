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
# MOD Dwarf cross-build via mod-plugin-builder + Docker.
#
# Variables (override on the command line, e.g.  make dwarf-build MPB_DIR=/opt/mpb):
#   MPB_DIR       — path to a clone of moddevices/mod-plugin-builder
#   MPB_WORKDIR   — where MPB stores toolchain + built plugins
#   MPB_IMAGE     — name of the docker image built from MPB's Dockerfile
#   MPB_PLATFORM  — target platform (moddwarf-new for current Dwarf hardware)
#   DWARF_HOST    — IP/hostname of the connected Dwarf for deployment

MPB_DIR      ?= $(HOME)/mod-plugin-builder
MPB_WORKDIR  ?= $(HOME)/mod-workdir
MPB_IMAGE    ?= mpb-moddwarf
MPB_PLATFORM ?= moddwarf-new
DWARF_HOST   ?= 192.168.51.1

DWARF_BUNDLE = $(MPB_WORKDIR)/$(MPB_PLATFORM)/plugins/sitar.lv2

# Bind-mount this repo into the build container so the recipe (which uses
# SITAR_SITE_METHOD=local) can copy the current working tree as source. No
# git push or SHA bump is needed for local iteration — edits in this repo
# show up in the very next dwarf-build.
MPB_DOCKER_RUN = docker run --rm -i \
	-v "$(MPB_DIR):/home/builder/mod-plugin-builder" \
	-v "$(MPB_WORKDIR):/root/mod-workdir" \
	-v "$(CURDIR):/home/builder/sitar-src:ro" \
	-w /home/builder/mod-plugin-builder \
	$(MPB_IMAGE)

# Marker files for the one-time setup steps. Using marker files lets Make
# skip them automatically once they're done.
MPB_CLONE_STAMP      := $(MPB_DIR)/Makefile
MPB_IMAGE_STAMP      := $(MPB_WORKDIR)/.image-built-$(MPB_PLATFORM)
MPB_BOOTSTRAP_STAMP  := $(MPB_WORKDIR)/.bootstrapped-$(MPB_PLATFORM)

# 1. Clone mod-plugin-builder automatically on first use.
$(MPB_CLONE_STAMP):
	@echo "==> Cloning mod-plugin-builder into $(MPB_DIR)"
	git clone https://github.com/moddevices/mod-plugin-builder $(MPB_DIR)

# 2. Build the build-environment Docker image on first use.
$(MPB_IMAGE_STAMP): $(MPB_CLONE_STAMP)
	@echo "==> Building Docker image $(MPB_IMAGE)"
	@mkdir -p $(MPB_WORKDIR)
	docker build --build-arg platform=$(MPB_PLATFORM) -t $(MPB_IMAGE) $(MPB_DIR)/docker
	@touch $@

# 3. Bootstrap the cross-toolchain inside the image. This is the slow
#    step (~30-60 min) and is gated behind an explicit target so it never
#    runs by surprise. Subsequent rebuilds reuse the cached toolchain.
dwarf-bootstrap: $(MPB_IMAGE_STAMP)
	@echo "==> Bootstrapping the $(MPB_PLATFORM) cross-toolchain (~30-60 min, one-time)"
	$(MPB_DOCKER_RUN) ./bootstrap.sh $(MPB_PLATFORM)
	@touch $(MPB_BOOTSTRAP_STAMP)

# Sync our recipe into MPB's package tree before each build, so local
# edits to mod-build/sitar.mk are picked up immediately.
mpb-sync-recipe: $(MPB_CLONE_STAMP)
	@mkdir -p "$(MPB_DIR)/plugins/package/sitar"
	cp -f mod-build/sitar.mk "$(MPB_DIR)/plugins/package/sitar/"

# Build the aarch64 .lv2 bundle inside the MPB Docker image. The dirclean
# is needed because buildroot caches the extracted source from the previous
# run and won't re-copy the working tree otherwise. The dirclean is cheap
# (only nukes the per-package build dir, not the toolchain), so a full
# rebuild is still ~10-20s.
dwarf-build: $(MPB_IMAGE_STAMP) mpb-sync-recipe
	@if [ ! -f "$(MPB_BOOTSTRAP_STAMP)" ]; then \
		echo "error: $(MPB_PLATFORM) cross-toolchain not bootstrapped yet"; \
		echo "       Run 'make dwarf-bootstrap' first (~30-60 min, one-time)."; \
		exit 1; \
	fi
	$(MPB_DOCKER_RUN) ./build $(MPB_PLATFORM) sitar-dirclean
	$(MPB_DOCKER_RUN) ./build $(MPB_PLATFORM) sitar

# Just the dirclean, in case something is wedged.
dwarf-clean: mpb-sync-recipe
	$(MPB_DOCKER_RUN) ./build $(MPB_PLATFORM) sitar-dirclean

# Upload the most recently built bundle to a connected Dwarf.
# Override DWARF_HOST=<ip> if your device isn't on the default 192.168.51.1.
dwarf-deploy:
	@if [ ! -d "$(DWARF_BUNDLE)" ]; then \
		echo "error: no built bundle at $(DWARF_BUNDLE)"; \
		echo "       Run 'make dwarf-build' first."; \
		exit 1; \
	fi
	@echo "Uploading sitar.lv2 to http://$(DWARF_HOST)/sdk/install"
	cd "$(MPB_WORKDIR)/$(MPB_PLATFORM)/plugins" && \
		tar czf - sitar.lv2 | base64 | \
		curl --silent --show-error --fail \
			-F 'package=@-' "http://$(DWARF_HOST)/sdk/install"
	@echo

# Convenience: build then deploy.
dwarf: dwarf-build dwarf-deploy

.PHONY: dwarf dwarf-build dwarf-clean dwarf-deploy dwarf-bootstrap mpb-sync-recipe
