# rlottie

RLOTTIE_VERSION := 0.2
RLOTTIE_URL := $(GITHUB)/Samsung/rlottie/archive/refs/tags/v$(RLOTTIE_VERSION).tar.gz

ifdef HAVE_MACOSX
PKGS += rlottie
endif

ifdef HAVE_WIN32
PKGS += rlottie
endif

ifdef HAVE_LINUX
ifndef HAVE_ANDROID
PKGS += rlottie
endif
endif

ifeq ($(call need_pkg,"rlottie >= 0.2"),)
PKGS_FOUND += rlottie
endif

$(TARBALLS)/rlottie-$(RLOTTIE_VERSION).tar.gz:
	$(call download,$(RLOTTIE_URL))

.sum-rlottie: rlottie-$(RLOTTIE_VERSION).tar.gz

RLOTTIE_CONFIG := -Dexample=false -Dmodule=false

rlottie: rlottie-$(RLOTTIE_VERSION).tar.gz .sum-rlottie
	$(UNPACK)
	$(APPLY) $(SRC)/rlottie/0001-Add-missing-include.patch
	$(MOVE)

.rlottie: rlottie crossfile.meson
	$(MESONCLEAN)
	$(MESON) $(RLOTTIE_CONFIG)
	+$(MESONBUILD)
	touch $@
