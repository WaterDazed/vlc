# Microhttpd

# TODO: build libmicrohttpd out of tree once support for it is added
# TODO: MICROHTTPD_URL should refer to a stable release after the release of libmicrohttpd 2.0

MICROHTTPD_VERSION := 7f9b410904b4e93c2c20071396a745198ca63ea1
MICROHTTPD_URL = https://git.gnunet.org/libmicrohttpd2.git/snapshot/libmicrohttpd2-$(MICROHTTPD_VERSION).tar.gz

PKGS += libmicrohttpd2

$(TARBALLS)/libmicrohttpd2-$(MICROHTTPD_VERSION).tar.gz:
	$(call download_pkg,$(MICROHTTPD_URL),libmicrohttpd2)

.sum-microhttpd: libmicrohttpd2-$(MICROHTTPD_VERSION).tar.gz

microhttpd: libmicrohttpd2-$(MICROHTTPD_VERSION).tar.gz .sum-microhttpd
	$(UNPACK)
	$(APPLY) $(SRC)/microhttpd/mhd-vlc-tls.patch
	$(MOVE)

MICROHTTPD_CONF = --enable-external-tls  \
                  --with-gnutls=no  \
			      --with-openssl=no

.microhttpd: microhttpd
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(MICROHTTPD_CONF)
	$(MAKE) -C $<
	$(MAKE) -C $< install
	touch $@
