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

bin_PROGRAMS = chmpx

#
# -lrt: for mqueue
#
chmpx_SOURCES = chmmain.cc
chmpx_LDADD = $(k2hash_LIBS) $(fullock_LIBS) -L../lib/.libs -lchmpx

ACLOCAL_AMFLAGS = -I m4
AM_CFLAGS = -I../lib $(k2hash_CFLAGS) $(fullock_CFLAGS)
AM_CPPFLAGS = -I../lib $(k2hash_CFLAGS) $(fullock_CFLAGS)

#
# VIM modelines
#
# vim:set ts=4 fenc=utf-8:
#
