#
# CHMPX
#
# Copyright 2014 Yahoo! JAPAN corporation.
#
# CHMPX is inprocess data exchange by MQ with consistent hashing.
# CHMPX is made for the purpose of the construction of
# original messaging system and the offer of the client
# library.
# CHMPX transfers messages between the client and the server/
# slave. CHMPX based servers are dispersed by consistent
# hashing and are automatically layouted. As a result, it
# provides a high performance, a high scalability.
#
# For the full copyright and license information, please view
# the LICENSE file that was distributed with this source code.
#
# AUTHOR:   Takeshi Nakatani
# CREATE:   Tue July 1 2014
# REVISION:
#

## AUTOMAKE_OPTIONS =

pkginclude_HEADERS = chmpx.h chmcntrl.h chmcommon.h chmcomstructure.h chmconf.h chmconfutil.h chmdbg.h chmeventbase.h chmeventmq.h chmeventsock.h chmeventshm.h chmhash.h chmimdata.h chmnetdb.h chmregex.h chmsigcntrl.h chmstructure.h chmutil.h chmopts.h chmthread.h chmlock.h chmkvp.h chmstructure.tcc chmstream.h chmlockmap.tcc
pkgincludedir = $(includedir)/chmpx

EXTRA_DIST =

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libchmpx.pc
DISTCLEANFILES = $(pkgconfig_DATA)

lib_LTLIBRARIES = libchmpx.la
libchmpx_la_SOURCES = chmpx.cc chmcntrl.cc chmconf.cc chmconfutil.cc chmdbg.cc chmeventbase.cc chmeventmq.cc chmeventsock.cc chmeventshm.cc chmhash.cc chmimdata.cc chmnetdb.cc chmregex.cc chmsigcntrl.cc chmutil.cc chmopts.cc chmthread.cc chmlock.cc chmkvp.cc chmpxversion.cc
libchmpx_la_LDFLAGS = -version-info $(LIB_VERSION_INFO)
libchmpx_la_LIBADD = $(k2hash_LIBS) $(fullock_LIBS) -lrt -lpthread -lssl -lcrypto -lyaml

ACLOCAL_AMFLAGS = -I m4
AM_CFLAGS = $(k2hash_CFLAGS) $(fullock_CFLAGS)
AM_CPPFLAGS = $(k2hash_CFLAGS) $(fullock_CFLAGS)

### version(commit hash)
.PHONY: chmpxversion

chmpxversion.cc: chmpxversion
	@../make_rev.sh chmpxversion.cc chmpx_commit_hash

#
# VIM modelines
#
# vim:set ts=4 fenc=utf-8:
#
