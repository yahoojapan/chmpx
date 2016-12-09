#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
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

AC_PREREQ([2.63])
AC_INIT(chmpx, m4_esyscmd([tr -d '\n' < $(pwd)/RELEASE_VERSION]))
AM_INIT_AUTOMAKE()
LT_INIT()

AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB
AC_PROG_LIBTOOL

# Checks for libraries.
#AC_CHECK_LIB([chmpx], [main])

# Checks for header files.
AC_CHECK_HEADERS([locale.h netdb.h fcntl.h sys/socket.h sys/time.h endian.h sys/endian.h netinet/in.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT64_T
AC_TYPE_UINT32_T
AC_TYPE_MODE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_FUNC_MKTIME
AC_FUNC_FORK
AC_CHECK_FUNCS([ftruncate gethostname memset regcomp setlocale strcasecmp strdup strrchr clock_gettime munmap select socket uname])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_FILES([Makefile
		lib/Makefile
		lib/libchmpx.pc
		src/Makefile
		tests/Makefile
		docs/Makefile
		docs/examples/Makefile])

# Check for k2hash + libfullock
if test "x${CHECK_K2HASH}" != "xno"; then
	PKG_CHECK_MODULES([k2hash], [libk2hash >= 1.0.52], [have_k2hash=yes], [have_k2hash=no])
	if test "x${have_k2hash}" = "xno"; then
		AC_MSG_ERROR([not found k2hash package.])
	fi
	PKG_CHECK_MODULES([fullock], [libfullock >= 1.0.21], [have_fullock=yes], [have_fullock=no])
	if test "x${have_fullock}" = "xno"; then
		AC_MSG_ERROR([not found fullock package.])
	fi
fi

# CFLAGS/CXXFLAGS
CFLAGS="-Wall $CFLAGS"
CXXFLAGS="-Wall $CXXFLAGS"

# LIB_VERSION_INFO for libtool library version info
LIB_VERSION_INFO=`$(pwd)/make_valiables.sh -lib_version_info`
AC_SUBST([LIB_VERSION_INFO])

AC_OUTPUT

#
# VIM modelines
#
# vim:set ts=4 fenc=utf-8:
#
