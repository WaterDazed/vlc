WHISPER_GITHASH := 1558ec5a16cb2b2a0bf54815df1d41f83dc3815b
WHISPER_VERSION := git-$(WHISPER_GITHASH)
WHISPER_GITURL := $(GITHUB)/ggerganov/whisper.cpp


# Whisper need std::thread that is provided by llvm
ifneq ($(findstring clang, $(shell $(CC) --version 2>/dev/null)),)
HAVE_CLANG := 1
endif

ifndef HAVE_WIN32
  ifeq ($(ARCH),aarch64)
    PKGS += whisper
  else ifeq ($(ARCH),x86_64)
    PKGS += whisper
  endif
else
  ifdef HAVE_CLANG
    PKGS += whisper
  endif
endif

ifdef HAVE_LINUX
  ifeq ($(ARCH),i386)
    PKGS += whisper
  endif
endif

ifeq ($(call need_pkg,"whisper"),)
  PKGS_FOUND += whisper
endif

$(TARBALLS)/whisper-$(WHISPER_GITHASH).tar.xz:
	$(call download_git,$(WHISPER_GITURL),,$(WHISPER_GITHASH))

.sum-whisper: whisper-$(WHISPER_GITHASH).tar.xz
	$(call check_githash,$(WHISPER_GITHASH))
	touch $@

whisper: whisper-$(WHISPER_GITHASH).tar.xz .sum-whisper
	$(UNPACK)
	$(APPLY) $(SRC)/whisper/0001-Add-pkg-config-support.patch
	$(call pkg_static,"whisper.pc.in")
	$(MOVE)

WHISPER_CONF := -DWHISPER_BUILD_EXAMPLES=OFF -DWHISPER_BUILD_TESTS=OFF

# Activate COREML only when is aarch64
ifdef HAVE_DARWIN_OS
  WHISPER_CONF += -DWHISPER_NO_ACCELERATE=OFF
  ifeq ($(ARCH),aarch64)
    WHISPER_CONF += -DWHISPER_COREML=ON
  endif
  WHISPER_CONF += -DWHISPER_METAL_EMBED_LIBRARY=ON
endif


.whisper: whisper toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS) $(CMAKE) $(WHISPER_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
