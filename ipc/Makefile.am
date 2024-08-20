###############################################################################
# Automake targets and declarations
###############################################################################

AUTOMAKE_OPTIONS =

###############################################################################
# VLC IPC framework
###############################################################################
libvlc_ipc_la_SOURCES = \
	ipc.h ipc.c

libvlc_ipc_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/ipc
libvlc_ipc_la_LIBADD = ../src/libvlccore.la ../compat/libcompat.la
libvlc_ipc_la_LDFLAGS = -static
noinst_LTLIBRARIES = libvlc_ipc.la
