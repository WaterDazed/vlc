# tremor (fixed-point Vorbis)

TREMOR_HASH := b56ffce0c0773ec5ca04c466bc00b1bbcaf65aef
TREMOR_URL := https://gitlab.xiph.org/xiph/tremor/-/archive/$(TREMOR_HASH)/tremor-$(TREMOR_HASH).tar.gz

ifndef HAVE_FPU
PKGS += tremor
endif

$(TARBALLS)/tremor-$(TREMOR_HASH).tar.gz:
	$(call download_pkg,$(TREMOR_URL),tremor)

.sum-tremor: tremor-$(TREMOR_HASH).tar.gz

tremor: tremor-$(TREMOR_HASH).tar.gz .sum-tremor
	# Stuff that does not depend on libogg
	$(UNPACK)
	# $(call update_autoconfig,.)
	$(APPLY) $(SRC)/tremor/tremor.patch
	$(MOVE)

DEPS_tremor = ogg $(DEPS_ogg)

.tremor: tremor
	# Stuff that depends on libogg
	$(RECONF)
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE)
	+$(MAKEBUILD)
	+$(MAKEBUILD) install
	touch $@
