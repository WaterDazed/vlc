###############################################################################
# Automake targets and declarations
###############################################################################

AUTOMAKE_OPTIONS = subdir-objects

###############################################################################
# VLC IPC framework
###############################################################################
# Generate IPC code generator
vlc-ipc-codegen$(BUILDEXEEXT): codegen/lexicon.l codegen/grammar.y
	$(AM_V_YACC) $(YACC) $(AM_YFLAGS) $(YFLAGS) -d -b ipccodegen.grammar $(srcdir)/codegen/grammar.y
	$(AM_V_LEX) $(LEX) $(AM_LFLAGS) $(LFLAGS) $(srcdir)/codegen/lexicon.l
	$(AM_V_CC) $(BUILDCC) -D_POSIX_C_SOURCE=200809L -o $@ ipccodegen.grammar.tab.c ipccodegen.lex.yy.c $(LEXLIB)

MOSTLYCLEANFILES= vlc-ipc-codegen$(BUILDEXEEXT)

libvlc_ipc_la_SOURCES = \
	ipc.h ipc.c \
	client.h client.c \
	server.h server.c

libvlc_ipc_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/ipc
libvlc_ipc_la_LIBADD = ../src/libvlccore.la ../compat/libcompat.la
libvlc_ipc_la_LDFLAGS = -static
noinst_LTLIBRARIES = libvlc_ipc.la
