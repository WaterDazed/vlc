# libndi

NDI_HASH := 930234549f96bcf7442795180f8d1fb43ed23af1
NDI_BRANCH := refactor-codebase
NDI_GITURL := https://code.videolan.org/AhmedHamed3699/libndi.git

PKGS += libndi
ifeq ($(call need_pkg,"libndi"),)
PKGS_FOUND += libndi
endif

NDI_CONF := -Dmicrodns=disabled

$(TARBALLS)/libndi-$(NDI_BRANCH).tar.xz:
	$(call download_git,$(NDI_GITURL),$(NDI_BRANCH),$(NDI_HASH))

.sum-libndi: $(TARBALLS)/libndi-$(NDI_BRANCH).tar.xz
	$(call check_githash,$(NDI_HASH))
	touch $@

libndi: libndi-$(NDI_BRANCH).tar.xz .sum-libndi
	$(UNPACK)
	$(MOVE)

.libndi: libndi crossfile.meson
	$(MESONCLEAN)
	$(MESON) $(NDI_CONF)
	+$(MESONBUILD)
	touch $@
