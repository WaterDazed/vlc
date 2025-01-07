# edge264
EDGE264_URL := $(GITHUB)/tvlabs/edge264.git
EDGE264_GITVERSION := f8809f722453a803186bbf6d7ea8eabb7fe9d86c

PKGS += edge264

DEPS_edge264 =
ifdef HAVE_WIN32
DEPS_edge264 += winpthreads $(DEPS_winpthreads)
endif

$(TARBALLS)/edge264-$(EDGE264_GITVERSION).tar.xz:
	$(call download_git,$(EDGE264_URL),,$(EDGE264_GITVERSION))

.sum-edge264: edge264-$(EDGE264_GITVERSION).tar.xz
	$(call check_githash,$(EDGE264_GITVERSION))
	touch $@

edge264: edge264-$(EDGE264_GITVERSION).tar.xz .sum-edge264
	$(UNPACK)
	$(MOVE)

EDGE264_CONF := -DBUILD_VARIANT_LOGS=OFF

.edge264: edge264 toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(EDGE264_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
