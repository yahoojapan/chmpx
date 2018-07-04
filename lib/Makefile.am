#
# CHMPX
#
# Copyright 2014 Yahoo Japan corporation.
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
# the license file that was distributed with this source code.
#
# AUTHOR:   Takeshi Nakatani
# CREATE:   Tue July 1 2014
# REVISION:
#

pkginclude_HEADERS	=	chmpx.h \
						chmcntrl.h \
						chmcommon.h \
						chmcomstructure.h \
						chmconf.h \
						chmconfutil.h \
						chmdbg.h \
						chmeventbase.h \
						chmeventmq.h \
						chmeventsock.h \
						chmeventshm.h \
						chmhash.h \
						chmimdata.h \
						chmnetdb.h \
						chmregex.h \
						chmsigcntrl.h \
						chmstructure.h \
						chmutil.h \
						chmopts.h \
						chmthread.h \
						chmlock.h \
						chmkvp.h \
						chmstructure.tcc \
						chmstream.h \
						chmss.h \
						chmlockmap.tcc

pkgincludedir		= $(includedir)/chmpx
pkgconfigdir		= $(libdir)/pkgconfig
pkgconfig_DATA		= libchmpx.pc
DISTCLEANFILES		= $(pkgconfig_DATA)

lib_LTLIBRARIES		= libchmpx.la
libchmpx_la_SOURCES	= chmpx.cc \
						chmcntrl.cc \
						chmconf.cc \
						chmconfutil.cc \
						chmdbg.cc \
						chmeventbase.cc \
						chmeventmq.cc \
						chmeventsock.cc \
						chmeventshm.cc \
						chmhash.cc \
						chmimdata.cc \
						chmnetdb.cc \
						chmregex.cc \
						chmsigcntrl.cc \
						chmutil.cc \
						chmopts.cc \
						chmthread.cc \
						chmlock.cc \
						chmkvp.cc \
						chmpxversion.cc

### Source code for SSL/TLS libraries
#
# [NOTE]
# The SSL/TLS library type related source code has different
# source code linked depending on the option passed to configure.
# And only object built from selected crypt source code is linked.
# The distributed source codes(tgz) by 'make dist' must have
# only selected source which is selected SSL/TLS library type.
# Because certain SSL/TLS libraries have problems that can not
# distribute the relevant source code due to licensing issues.
# Then source codes for these libraries are declared with 'nodist',
# and they are defined in 'EXTRA_DIST' macro.
# By defining 'nodist' and 'EXTRA_DIST', we realize the
# distribution of selective source code.
#
EXTRA_DIST			= $(SSL_TLS_LIB_CSOURCE) $(SSL_TLS_LIB_CHEADER)

if SSL_TLS_LIB_OPENSSL
nodist_libchmpx_la_SOURCES	= chmssopenssl.cc
nodist_pkginclude_HEADERS	= chmssopenssl.h
endif
if SSL_TLS_LIB_NSS
nodist_libchmpx_la_SOURCES	= chmssnss.cc
nodist_pkginclude_HEADERS	= chmssnss.h
endif
if SSL_TLS_LIB_GNUTLS
nodist_libchmpx_la_SOURCES	= chmssgnutls.cc
nodist_pkginclude_HEADERS	= chmssgnutls.h
endif

libchmpx_la_LDFLAGS	= -version-info $(LIB_VERSION_INFO)
libchmpx_la_LIBADD	= $(k2hash_LIBS) $(fullock_LIBS) $(SSL_TLS_LIBS) -lyaml -lrt -lpthread

ACLOCAL_AMFLAGS		= -I m4
AM_CFLAGS			= $(k2hash_CFLAGS) $(fullock_CFLAGS)
AM_CPPFLAGS			= $(k2hash_CFLAGS) $(fullock_CFLAGS)

### version(commit hash)
.PHONY:	chmpxversion

chmpxversion.cc:	chmpxversion
	@../buildutils/make_commit_hash_source.sh chmpxversion.cc chmpx_commit_hash

#
# VIM modelines
#
# vim:set ts=4 fenc=utf-8:
#
