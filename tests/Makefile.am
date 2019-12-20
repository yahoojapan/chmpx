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
# hashing and are automatically laid out. As a result, it
# provides a high performance, a high scalability.
#
# For the full copyright and license information, please view
# the license file that was distributed with this source code.
#
# AUTHOR:   Takeshi Nakatani
# CREATE:   Tue July 1 2014
# REVISION:
#

bin_PROGRAMS = chmpxbench chmpxstatus chmpxlinetool chmpxconftest
noinst_PROGRAMS = chmpxstreamtest

#
# -lrt: for mqueue
#
chmpxconftest_SOURCES	= chmpxconftest.cc
chmpxconftest_LDADD		= $(k2hash_LIBS) $(fullock_LIBS) -L../lib/.libs -lchmpx

chmpxbench_SOURCES		= chmpxbench.cc
chmpxbench_LDADD		= $(k2hash_LIBS) $(fullock_LIBS) -L../lib/.libs -lchmpx -lpthread

chmpxstreamtest_SOURCES	= chmpxstreamtest.cc
chmpxstreamtest_LDADD	= $(k2hash_LIBS) $(fullock_LIBS) -L../lib/.libs -lchmpx

chmpxstatus_SOURCES		= chmpxstatus.cc
chmpxstatus_LDADD		= $(k2hash_LIBS) $(fullock_LIBS) -L../lib/.libs -lchmpx

chmpxlinetool_SOURCES	= chmpxlinetool.cc
chmpxlinetool_LDADD		= $(k2hash_LIBS) $(fullock_LIBS) -L../lib/.libs -lchmpx -lpthread

ACLOCAL_AMFLAGS	= -I m4
AM_CFLAGS		= $(k2hash_CFLAGS) $(fullock_CFLAGS) -I../lib
AM_CPPFLAGS		= $(k2hash_CFLAGS) $(fullock_CFLAGS) -I../lib

TESTS = test.sh

EXTRA_DIST =	test.sh \
				chmpxnssutil.sh \
				test_server.ini \
				test_slave.ini \
				cfg_test_server.ini \
				cfg_test_server.json \
				cfg_test_server.result \
				cfg_test_server.yaml \
				cfg_test_slave.ini \
				cfg_test_slave.json \
				cfg_test_slave.result \
				cfg_test_slave.yaml \
				cfg_test_json_string.data

#
# VIM modelines
#
# vim:set ts=4 fenc=utf-8:
#
