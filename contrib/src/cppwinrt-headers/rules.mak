CPPWINRT_VERSION := 2.0.221221.0
CPPWINRT_VERSION_EXTRA := -beta.1
CPPWINRT_VERSION_FULL := $(CPPWINRT_VERSION)$(CPPWINRT_VERSION_EXTRA)
CPPWINRT_URL := $(GITHUB)/alvinhochun/mingw-w64-cppwinrt/releases/download/$(CPPWINRT_VERSION_FULL)/mingw-w64-cppwinrt-$(CPPWINRT_VERSION_FULL)-headers.tar.gz

ifdef HAVE_WIN32
ifndef HAVE_WINSTORE
PKGS += cppwinrt-headers
endif
endif

$(TARBALLS)/mingw-w64-cppwinrt-$(CPPWINRT_VERSION_FULL)-headers.tar.gz:
	$(call download_pkg,$(CPPWINRT_URL),cppwinrt)

.sum-cppwinrt-headers: mingw-w64-cppwinrt-$(CPPWINRT_VERSION_FULL)-headers.tar.gz

cppwinrt-headers: mingw-w64-cppwinrt-$(CPPWINRT_VERSION_FULL)-headers.tar.gz .sum-cppwinrt-headers
	$(UNPACK)
	$(APPLY) $(SRC)/cppwinrt-headers/workaround-llvm13-libc++.patch
	$(MOVE)

.cppwinrt-headers: cppwinrt-headers
	@mkdir -p $(PREFIX)/include
	cp -R cppwinrt-headers/include/cppwinrt-$(CPPWINRT_VERSION) $(PREFIX)/include
	cp cppwinrt-headers/share/licenses/cppwinrt/cppwinrt/LICENSE $(PREFIX)/include/cppwinrt-$(CPPWINRT_VERSION)
	@mkdir -p $(PREFIX)/lib/pkgconfig
	sed 's,^prefix=.*,prefix=$(PREFIX),' cppwinrt-headers/share/pkgconfig/CppWinRT.pc > $(PREFIX)/lib/pkgconfig/CppWinRT.pc
	touch $@
