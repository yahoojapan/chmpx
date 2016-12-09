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

bin_PROGRAMS = chmpxbench chmpxstatus
noinst_PROGRAMS = chmconftest chmstreamtest

#
# -lrt: for mqueue
#
chmconftest_SOURCES = chmconftest.cc
chmconftest_LDADD = -L../lib/.libs -lchmpx $(k2hash_LIBS) $(fullock_LIBS)

chmpxbench_SOURCES = chmpxbench.cc
chmpxbench_LDADD = -L../lib/.libs -lchmpx $(k2hash_LIBS) $(fullock_LIBS) -lpthread

chmstreamtest_SOURCES = chmstreamtest.cc
chmstreamtest_LDADD = -L../lib/.libs -lchmpx $(k2hash_LIBS) $(fullock_LIBS)

chmpxstatus_SOURCES = chmpxstatus.cc
chmpxstatus_LDADD = -L../lib/.libs -lchmpx $(k2hash_LIBS) $(fullock_LIBS)

ACLOCAL_AMFLAGS = -I m4
AM_CFLAGS = -I../lib $(k2hash_CFLAGS) $(fullock_CFLAGS)
AM_CPPFLAGS = -I../lib $(k2hash_CFLAGS) $(fullock_CFLAGS)

TESTS = test.sh

EXTRA_DIST = test.sh test_server.ini test_slave.ini test_slave.yaml test_server.yaml test_server.json test_slave.json test_json_string.data

#
# VIM modelines
#
# vim:set ts=4 fenc=utf-8:
#
