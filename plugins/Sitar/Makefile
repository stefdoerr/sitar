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
