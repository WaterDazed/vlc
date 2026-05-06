# uavs3d
# UAVS3D_VERSION := 1.2
UAVS3D_VERSION := 0e20d2c291853f196c68922a264bcd8471d75b68
UAVS3D_URL := $(GITHUB)/uavs3/uavs3d/archive/refs/tags/$(UAVS3D_VERSION).tar.gz
UAVS3D_GITURL := $(GITHUB)/uavs3/uavs3d.git

PKGS += uavs3d
ifeq ($(call need_pkg,"uavs3d"),)
PKGS_FOUND += uavs3d
endif

DEPS_uavs3d =
ifdef HAVE_WIN32
DEPS_uavs3d += winpthreads $(DEPS_winpthreads)
endif

# $(TARBALLS)/uavs3d-$(UAVS3D_VERSION).tar.gz:
# 	$(call download_pkg,$(UAVS3D_URL),uavs3d)

# .sum-uavs3d: uavs3d-$(UAVS3D_VERSION).tar.gz

$(TARBALLS)/uavs3d-$(UAVS3D_VERSION).tar.xz:
	$(call download_git,$(UAVS3D_GITURL),,$(UAVS3D_VERSION))

.sum-uavs3d: uavs3d-$(UAVS3D_VERSION).tar.xz
	$(call check_githash,$(UAVS3D_VERSION))
	touch $@

# uavs3d: uavs3d-$(UAVS3D_VERSION).tar.gz .sum-uavs3d
uavs3d: uavs3d-$(UAVS3D_VERSION).tar.xz .sum-uavs3d
	$(UNPACK)
	$(MOVE)

.uavs3d: uavs3d toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) -DCMAKE_POLICY_VERSION_MINIMUM=3.5
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
