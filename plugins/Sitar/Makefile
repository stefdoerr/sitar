#!/usr/bin/make -f
# Makefile for the Sitar Sympathetic Resonance plugin
# ---------------------------------------------------

# --------------------------------------------------------------
# Project name (used for binary names and the .lv2 bundle dir)

# Set BETA=1 (or SITAR_BETA=1) on the make command line to build the
# side-by-side beta variant: distinct LV2 URI, bundle name, brand and
# unique id. Useful for A/B testing a feature branch alongside the
# stable plugin in MOD Desktop. The top-level Makefile wraps this in a
# `make beta` convenience target.
ifeq ($(BETA),1)
SITAR_BETA = 1
endif
ifeq ($(SITAR_BETA),1)
NAME = sitar-beta
# DPF's Makefile.base.mk hard-resets BUILD_CXX_FLAGS but appends
# $(CXXFLAGS) at the end, so user-visible flags must go through CXXFLAGS
# to actually reach the compiler.
CXXFLAGS += -DSITAR_BETA=1
else
NAME = sitar
endif

# --------------------------------------------------------------
# Plugin version — the top-level VERSION file is the single source of
# truth (`make release` keeps it in sync with the release tag). Injected
# as preprocessor defines so getVersion() and the generated LV2 TTL
# always report the released version. Goes through CXXFLAGS for the same
# reason as SITAR_BETA above.

SITAR_VERSION := $(strip $(shell cat ../../VERSION 2>/dev/null))
ifeq ($(SITAR_VERSION),)
$(error VERSION file not found at ../../VERSION)
endif
SITAR_VERSION_PARTS := $(subst ., ,$(SITAR_VERSION))
CXXFLAGS += -DSITAR_VERSION_MAJOR=$(word 1,$(SITAR_VERSION_PARTS)) \
            -DSITAR_VERSION_MINOR=$(word 2,$(SITAR_VERSION_PARTS)) \
            -DSITAR_VERSION_MICRO=$(word 3,$(SITAR_VERSION_PARTS))

# --------------------------------------------------------------
# Extra flags injected by the Patchstorage cross-build (CPU/opt flags matching
# patchstorage's defconfig). Kept in a separate variable so passing them on the
# make command line doesn't clobber the version defines above — a command-line
# CXXFLAGS=... would override the makefile's `CXXFLAGS +=` lines entirely.
CFLAGS   += $(EXTRA_CFLAGS)
CXXFLAGS += $(EXTRA_CXXFLAGS)

# --------------------------------------------------------------
# Files to build

FILES_DSP = \
	SitarPlugin.cpp

# --------------------------------------------------------------
# Pull in DPF's shared build rules

include ../../dpf/Makefile.plugins.mk

# --------------------------------------------------------------
# Targets — LV2 only for MOD Dwarf

TARGETS += lv2

all: $(TARGETS)

# --------------------------------------------------------------
