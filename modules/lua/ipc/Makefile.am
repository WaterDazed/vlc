luaipcdir = $(pluginsdir)/lua/ipc

libluaipc_plugin_la_SOURCES = \
	lua/ipc/luaipc.c

protocoldir=lua/ipc/protocol

$(protocoldir)/%_ipc.c: $(protocoldir)/%.ipc $(top_builddir)/ipc/vlc-ipc-codegen$(BUILDEXEEXT)
	$(AM_V_GEN)$(top_builddir)/ipc/vlc-ipc-codegen$(BUILDEXEEXT) $< $@ $(patsubst %.c, %.h, $@)

BUILT_SOURCES += $(protocoldir)/lua_ipc.c
libluaipc_plugin_la_SOURCES += $(protocoldir)/lua_ipc.c

libluaipc_plugin_la_CFLAGS = $(AM_CFLAGS) $(LUA_CFLAGS) -I$(srcdir)/lua -I$(srcdir)/lua/ipc -I$(top_srcdir)/ipc -I$(protocoldir)
libluaipc_plugin_la_LIBADD = $(LUA_LIBS) $(SOCKET_LIBS) $(LIBDL) $(LIBM) $(top_builddir)/ipc/libvlc_ipc.la

if BUILD_LUA
lua_LTLIBRARIES += libluaipc_plugin.la
endif
