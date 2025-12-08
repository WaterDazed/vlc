# lcms2

LCMS2_VERSION := 2.15
LCMS2_ARCHIVE = lcms2-$(LCMS2_VERSION).tar.gz
LCMS2_URL := $(GITHUB)/mm2/Little-CMS/releases/download/lcms$(LCMS2_VERSION)/$(LCMS2_ARCHIVE)

PKGS += lcms2
ifeq ($(call need_pkg,"lcms2"),)
PKGS_FOUND += lcms2
endif

$(TARBALLS)/$(LCMS2_ARCHIVE):
	$(call download_pkg,$(LCMS2_URL),lcms2)

.sum-lcms2: $(LCMS2_ARCHIVE)

lcms2: $(LCMS2_ARCHIVE) .sum-lcms2
	$(UNPACK)
	sed -i.orig -e "s/<Windows.h>/<windows.h>/" "$(UNPACK_DIR)/src/lcms2.rc.in"
	sed -i.orig -e "s/<Windows.h>/<windows.h>/" "$(UNPACK_DIR)/plugins/fast_float/src/lcms2_fast_float.rc.in"
	sed -i.orig -e "s/<Windows.h>/<windows.h>/" "$(UNPACK_DIR)/plugins/threaded/src/lcms2_threaded.rc.in"
	$(MOVE)

.lcms2: lcms2 crossfile.meson
	$(MESONCLEAN)
	cd $< && $(HOSTVARS_MESON) $(MESON) build
	cd $< && cd build && ninja install
	touch $@
