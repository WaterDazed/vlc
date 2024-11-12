wasm_rsdir = $(pluginsdir)/wasm-rs

libwasm_rs.la:
	@$(LIBTOOL_CARGO) $(srcdir)/wasm-rs/ $@

libwasm_rs_plugin_la_SOURCES = \
	wasm-rs/Cargo.toml \
	wasm-rs/src/lib.rs 
libwasm_rs_plugin_la_LIBADD = libwasm_rs.la

if HAVE_RUST
wasm_rs_LTLIBRARIES = libwasm_rs_plugin.la
endif
