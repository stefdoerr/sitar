######################################
#
# sitar — sympathetic resonance for MOD devices
#
######################################

# This recipe builds from a *local* source tree mounted into the container
# at /home/builder/sitar-src by the parent project's `make dwarf-build`
# target. No GitHub push or SHA pinning is needed for local iteration —
# edit, save, `make dwarf-build`, done.
#
# To switch to a reproducible GitHub-based build instead, swap the SITE_*
# vars below for:
#
#   SITAR_VERSION = <git sha>
#   SITAR_SITE    = https://github.com/USER/sitar.git
#   SITAR_SITE_METHOD = git
#   SITAR_PRE_DOWNLOAD_HOOKS += MOD_PLUGIN_BUILDER_DOWNLOAD_WITH_SUBMODULES

SITAR_VERSION     = local
SITAR_SITE        = /home/builder/sitar-src
SITAR_SITE_METHOD = local
SITAR_BUNDLES     = sitar.lv2

SITAR_TARGET_MAKE = $(TARGET_MAKE_ENV) $(TARGET_CONFIGURE_OPTS) $(MAKE) -C $(@D)

define SITAR_BUILD_CMDS
	$(SITAR_TARGET_MAKE)
endef

define SITAR_INSTALL_TARGET_CMDS
	$(SITAR_TARGET_MAKE) install DESTDIR=$(TARGET_DIR) PREFIX=/usr
endef

$(eval $(generic-package))
