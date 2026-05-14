#!/usr/bin/make -f
# Makefile for the Sitar Sympathetic Resonance plugin
# ---------------------------------------------------

# --------------------------------------------------------------
# Project name (used for binary names and the .lv2 bundle dir)

NAME = sitar

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
