###############################################################################
# Automake targets and declarations
###############################################################################

AUTOMAKE_OPTIONS = subdir-objects
BUILT_SOURCES=

###############################################################################
# VLC IPC framework
###############################################################################
# Generate IPC code generator
vlc-ipc-codegen$(BUILDEXEEXT): codegen/lexicon.l codegen/grammar.y
	$(AM_V_YACC) $(YACC) $(AM_YFLAGS) $(YFLAGS) -d -b ipccodegen.grammar $(srcdir)/codegen/grammar.y
	$(AM_V_LEX) $(LEX) $(AM_LFLAGS) $(LFLAGS) $(srcdir)/codegen/lexicon.l
	$(AM_V_CC) $(BUILDCC) -D_POSIX_C_SOURCE=200809L -o $@ ipccodegen.grammar.tab.c ipccodegen.lex.yy.c $(LEXLIB)

MOSTLYCLEANFILES= vlc-ipc-codegen$(BUILDEXEEXT)

protocoldir=protocol

$(protocoldir)/%_ipc.c: $(protocoldir)/%.ipc vlc-ipc-codegen$(BUILDEXEEXT)
	$(AM_V_GEN)./vlc-ipc-codegen$(BUILDEXEEXT) $< $@ $(patsubst %.c, %.h, $@)

libvlc_ipc_la_SOURCES = \
	ipc.h ipc.c \
	client.h client.c \
	server.h server.c \
	logs.h logs.c \
	runner.h runner_internal.h runnerlib.c

libvlc_ipc_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/ipc -I$(protocoldir)
libvlc_ipc_la_LIBADD = ../src/libvlccore.la ../compat/libcompat.la
libvlc_ipc_la_LDFLAGS = -static
noinst_LTLIBRARIES = libvlc_ipc.la
BUILT_SOURCES += $(protocoldir)/log_ipc.c
libvlc_ipc_la_SOURCES += $(protocoldir)/log_ipc.c

# Runner
pkglibexec_PROGRAMS= vlc-ipc-runner

vlc_ipc_runner_SOURCES = \
	runner.h \
	runner_internal.h \
	runner.c

vlc_ipc_runner_CFLAGS = $(AM_CFLAGS) -I$(srcdir) -fPIC
vlc_ipc_runner_LDADD = ../src/libvlccore.la ../lib/libvlc.la ../compat/libcompat.la libvlc_ipc.la

# Tests
testdir=test

$(testdir)/test_ipc.c: $(testdir)/test.ipc vlc-ipc-codegen$(BUILDEXEEXT)
	$(AM_V_GEN) ./vlc-ipc-codegen$(BUILDEXEEXT) $< $@ $(patsubst %.c, %.h, $@)

BUILT_SOURCES += $(testdir)/test_ipc.c
check_PROGRAMS = test_ipc

test_ipc_SOURCES = test/test.c
test_ipc_SOURCES += $(testdir)/test_ipc.c
test_ipc_CPPFLAGS = $(AM_CPPFLAGS) -I../include -I$(srcdir) -I$(testdir)
test_ipc_LDADD = ../lib/libvlc.la ../src/libvlccore.la ../compat/libcompat.la libvlc_ipc.la

TESTS = test_ipc
