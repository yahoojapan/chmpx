#
# CHMPX
#
# Utility tools for building configure/packages by AntPickax
#
# Copyright 2018 Yahoo Japan corporation.
#
# AntPickax provides utility tools for supporting autotools
# builds.
#
# These tools retrieve the necessary information from the
# repository and appropriately set the setting values of
# configure, Makefile, spec,etc file and so on.
# These tools were recreated to reduce the number of fixes and
# reduce the workload of developers when there is a change in
# the project configuration.
# 
# For the full copyright and license information, please view
# the license file that was distributed with this source code.
#
# AUTHOR:   Takeshi Nakatani
# CREATE:   Fri, Apr 13 2018
# REVISION:
#

#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
#
AC_PREREQ([2.63])
AC_INIT(chmpx, m4_esyscmd([tr -d '\n' < $(pwd)/RELEASE_VERSION]))
AM_INIT_AUTOMAKE()
LT_INIT()

AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])

#
# Checks for programs.
#
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB

#
# Checks for libraries.
#
AC_CHECK_LIB(dl, dlopen)

#
# Checks for typedefs, structures, and compiler characteristics.
#
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT64_T
AC_TYPE_UINT32_T
AC_TYPE_MODE_T

#
# Checks for library functions.
#
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_FUNC_MKTIME
AC_FUNC_FORK
AC_FUNC_ERROR_AT_LINE
AC_CHECK_FUNCS([ftruncate gethostname memset regcomp setlocale strcasecmp strncasecmp strdup strrchr clock_gettime munmap select socket uname gettimeofday strstr inet_ntoa])
AC_CHECK_HEADERS([locale.h netdb.h fcntl.h sys/socket.h sys/time.h endian.h sys/endian.h netinet/in.h termios.h arpa/inet.h inttypes.h strings.h unistd.h])
AC_CONFIG_MACRO_DIR([m4])
AC_HEADER_RESOLV

#
# Load customizable variables
#
AC_CHECK_FILE([configure.custom],
	[
		configure_custom_file="configure.custom"
		custom_git_domain="$(grep '^\s*GIT_DOMAIN\s*=' configure.custom | sed -e 's|^\s*GIT_DOMAIN\s*=\s*||g' -e 's|^\s*||g' -e 's|\s*$||g')"
		custom_git_org="$(grep '^\s*GIT_ORG\s*=' configure.custom | sed -e 's|^\s*GIT_ORG\s*=\s*||g' -e 's|^\s*||g' -e 's|\s*$||g')"
		custom_git_repo="$(grep '^\s*GIT_REPO\s*=' configure.custom | sed -e 's|^\s*GIT_REPO\s*=\s*||g' -e 's|^\s*||g' -e 's|\s*$||g')"
		custom_git_endpoint="$(grep '^\s*GIT_EP_V3_REPO\s*=' configure.custom | sed -e 's|^\s*GIT_EP_V3_REPO\s*=\s*||g' -e 's|^\s*||g' -e 's|\s*$||g')"
		custom_dev_email="$(grep '^\s*DEV_EMAIL\s*=' configure.custom | sed -e 's|^\s*DEV_EMAIL\s*=\s*||g' -e 's|^\s*||g' -e 's|\s*$||g')"
		custom_dev_name="$(grep '^\s*DEB_NAME\s*=' configure.custom | sed -e 's|^\s*DEB_NAME\s*=\s*||g' -e 's|^\s*||g' -e 's|\s*$||g')"
	],
	[
		configure_custom_file=""
		custom_git_domain="github.com"
		custom_git_org="yahoojapan"
		custom_git_repo="chmpx"
		custom_git_endpoint="https://api.github.com/repos/"
		custom_dev_email="antpickax-support@mail.yahoo.co.jp"
		custom_dev_name="CHMPX_DEVELOPER"
	]
)

#
# Symbols for buildutils
#
AC_SUBST([CONFIGURECUSTOM], "${configure_custom_file}")
AC_SUBST([GIT_DOMAIN], "${custom_git_domain}")
AC_SUBST([GIT_ORG], "${custom_git_org}")
AC_SUBST([GIT_REPO], "${custom_git_repo}")
AC_SUBST([CURRENTREV], "$($(pwd)/buildutils/make_commit_hash.sh -o "${custom_git_org}" -r "${custom_git_repo}" -ep "${custom_git_endpoint}" -s 2>/dev/null)")
AC_SUBST([DEV_EMAIL], "$(echo ${DEBEMAIL:-"${custom_dev_email}"})")
AC_SUBST([DEV_NAME], "$(echo ${DEBFULLNAME:-"${custom_dev_name}"})")
#
AC_SUBST([RPMCHANGELOG], "`$(pwd)/buildutils/make_rpm_changelog.sh $(pwd)/ChangeLog`")
AC_SUBST([SHORTDESC], "`$(pwd)/buildutils/make_description.sh $(pwd)/docs/chmpx.1 --short`")
AC_SUBST([LONGDESC], "`$(pwd)/buildutils/make_description.sh $(pwd)/docs/chmpx.1 --long`")
AC_SUBST([ESCLONGDESC], "`$(pwd)/buildutils/make_description.sh $(pwd)/docs/chmpx.1 --esclong`")
AC_SUBST([DEBLONGDESC], "`$(pwd)/buildutils/make_description.sh $(pwd)/docs/chmpx.1 --deblong`")
AC_SUBST([PKG_VERSION], "`$(pwd)/buildutils/make_variables.sh --pkg_version`")
AC_SUBST([LIB_VERSION_INFO], "`$(pwd)/buildutils/make_variables.sh --lib_version_info`")
AC_SUBST([LIB_VERSION_LINK], "`$(pwd)/buildutils/make_variables.sh --lib_version_for_link`")
AC_SUBST([LIB_MAJOR_NUMBER], "`$(pwd)/buildutils/make_variables.sh --major_number`")
AC_SUBST([PKGLICENSE], "`grep License COPYING | sed 's/ License//g'`")
AC_SUBST([DEBCOPYING], "`tail -n +5 COPYING | sed 's/^$/./g' | sed 's/^/ /g'`")
AC_SUBST([DEBHELPER_DEP], "`$(pwd)/buildutils/make_variables.sh --debhelper_dep_with_systemd`")
AC_SUBST([RPMPKG_GROUP], ["`$(pwd)/buildutils/make_variables.sh --rpmpkg_group | sed 's#NEEDRPMGROUP#Group: Applications/Communications#g'`"])
AM_SUBST_NOTMAKE([CURRENTREV])
AM_SUBST_NOTMAKE([RPMCHANGELOG])
AM_SUBST_NOTMAKE([SHORTDESC])
AM_SUBST_NOTMAKE([LONGDESC])
AM_SUBST_NOTMAKE([ESCLONGDESC])
AM_SUBST_NOTMAKE([DEBLONGDESC])
AM_SUBST_NOTMAKE([DEBCOPYING])
AM_SUBST_NOTMAKE([DEBHELPER_DEP])
AM_SUBST_NOTMAKE([RPMPKG_GROUP])

#
# pkg-config for old version
#
AC_PATH_PROG(PKG_CONFIG, pkg-config, [AC_MSG_RESULT(no)])
AS_IF([test "$PKG_CONFIG" = "no"], [AC_MSG_ERROR(You have to install pkg-config to compile $PACKAGE_NAME v$PACKAGE_VERSION)])

#
# Option for SSL/TLS libraries
#
ssl_lib=openssl
is_openssl3=no

AC_MSG_CHECKING([for SSL/TLS library option with GnuTLS])
AC_ARG_WITH(gnutls,
	[AS_HELP_STRING([--with-gnutls], [build with GnuTLS])],
	[case "${withval}" in
		yes)	ssl_lib=gnutls;;
		*)		;;
	esac]
)
AS_IF([test ${ssl_lib} = gnutls], [AC_MSG_RESULT(yes)], [AC_MSG_RESULT(no)])

AC_MSG_CHECKING([for SSL/TLS library option with NSS])
AC_ARG_WITH(nss,
	[AS_HELP_STRING([--with-nss], [build with NSS])],
	[case "${withval}" in
		yes)	ssl_lib=nss;;
		*)		;;
	esac]
)
AS_IF([test ${ssl_lib} = nss], [AC_MSG_RESULT(yes)], [AC_MSG_RESULT(no)])

AC_MSG_CHECKING([for SSL/TLS library option with OpenSSL(default)])
AC_ARG_WITH(openssl,
	[AS_HELP_STRING([--with-openssl], [build with OpenSSL(default)])],
	[case "${withval}" in
		yes)	ssl_lib=openssl;;
		*)		;;
	esac]
)
AS_IF([test ${ssl_lib} = openssl], [AC_MSG_RESULT(yes)], [AC_MSG_RESULT(no)])

#
# Checking crypt libraries
#
# [NOTE]
# Although gcrypt does not provide a pkg-config file, it provides
# libgcrypt-config and we check it with libgcrypt-config.
#
AC_MSG_CHECKING([for using SSL/TLS library type])
case "${ssl_lib}" in
	openssl)
		AC_MSG_RESULT(OpenSSL)
		PKG_CHECK_MODULES([SSL_TLS], [openssl >= 1.0.0 ])
		AC_DEFINE(SSL_TLS_TYPE_OPENSSL, 1, [Defined for linking OpenSSL library])
		AC_SUBST([SSL_TLS_LIB_CSOURCE], "chmssopenssl.cc")
		AC_SUBST([SSL_TLS_LIB_CHEADER], "chmssopenssl.h")
		AC_SUBST([CONFIGUREWITHOPT], "--with-openssl")
		AM_SUBST_NOTMAKE([CONFIGUREWITHOPT])

		#
		# Check openssl 3
		#
		AC_MSG_CHECKING([openssl 3.0 or later])
		AC_COMPILE_IFELSE([
			AC_LANG_PROGRAM([[
				#include <openssl/opensslv.h>
				#if !defined(LIBRESSL_VERSION_NUMBER) && OPENSSL_VERSION_NUMBER >= 0x30000000L
				#error "found openssl is 3.0 or later(so compiling is stopped with error)"
				#endif
			]], [[]])],
			[AC_MSG_RESULT(no)],
			[AC_MSG_RESULT(yes); is_openssl3=yes])

		#
		# Depends TLS library for packaging
		#
		AC_SUBST([DEB_DEPS_TLS_PKG],     "libssl3")
		AC_SUBST([DEB_DEPS_TLS_DEV_PKG], "libssl-dev")
		AC_SUBST([RPM_DEPS_TLS_PKG],     "openssl-libs")
		AC_SUBST([RPM_DEPS_TLS_DEV_PKG], "openssl-devel")
		AC_SUBST([ALP_DEPS_TLS_DEV_PKG], "openssl-dev")

		;;
	nss)
		AC_MSG_RESULT(NSS)
		PKG_CHECK_MODULES([SSL_TLS], [nss >= 3.28.0 ])
		AC_DEFINE(SSL_TLS_TYPE_NSS, 1, [Defined for linking NSS library])
		AC_SUBST([SSL_TLS_LIB_CSOURCE], "chmssnss.cc")
		AC_SUBST([SSL_TLS_LIB_CHEADER], "chmssnss.h")
		AC_SUBST([CONFIGUREWITHOPT], "--with-nss")
		AM_SUBST_NOTMAKE([CONFIGUREWITHOPT])

		#
		# Depends TLS library for packaging
		#
		AC_SUBST([RPM_DEPS_TLS_PKG],     "nss")
		AC_SUBST([RPM_DEPS_TLS_DEV_PKG], "nss-devel")

		;;
	gnutls)
		AC_MSG_RESULT(gnutls)
		PKG_CHECK_MODULES([SSL_TLS], [gnutls >= 3.3.6 ])
		AC_DEFINE(SSL_TLS_TYPE_GNUTLS, 1, [Defined for linking GnuTLS library])
		AC_SUBST([SSL_TLS_LIB_CSOURCE], "chmssgnutls.cc")
		AC_SUBST([SSL_TLS_LIB_CHEADER], "chmssgnutls.h")
		AC_SUBST([CONFIGUREWITHOPT], "--with-gnutls")
		AM_SUBST_NOTMAKE([CONFIGUREWITHOPT])

		#
		# Depends TLS library for packaging
		#
		AC_SUBST([DEB_DEPS_TLS_PKG],     "libgnutls30")
		AC_SUBST([DEB_DEPS_TLS_DEV_PKG], "gnutls-dev")

		;;
	*)
		AC_MSG_ERROR([unknown SSL/TLS library type.])
		;;
esac

AM_CONDITIONAL([SSL_TLS_LIB_OPENSSL],	[test "$ssl_lib" = openssl])
AM_CONDITIONAL([SSL_TLS_LIB_NSS],		[test "$ssl_lib" = nss])
AM_CONDITIONAL([SSL_TLS_LIB_GNUTLS],	[test "$ssl_lib" = gnutls])
AM_CONDITIONAL([IS_OPENSSL3],			[test "$is_openssl3" = yes])

#
# Version list for Libraries
#
AC_SUBST([LIB_MINVER_LIBK2HASH], "1.0.99")
AC_SUBST([LIB_MINVER_LIBFULLOCK], "1.0.63")

#
# Check for k2hash + libfullock
#
check_depend_libs=1
AC_MSG_CHECKING([for checking dependent libraries])
AC_ARG_ENABLE(check-depend-libs,
	[AS_HELP_STRING([--disable-check-depend-libs], [disable checking dependent libraries])],
	[case "${enableval}" in
		yes)	check_depend_libs=1;;
		*)		check_depend_libs=0;;
	esac]
)
AS_IF([test ${check_depend_libs} = 1], [AC_MSG_RESULT(yes)], [AC_MSG_RESULT(no)])
AS_IF([test ${check_depend_libs} = 1], [PKG_CHECK_MODULES([k2hash], [libk2hash >= 1.0.99], [], [AC_MSG_ERROR(not found k2hash package)])])
AS_IF([test ${check_depend_libs} = 1], [PKG_CHECK_MODULES([fullock], [libfullock >= 1.0.63], [], [AC_MSG_ERROR(not found libfullock package)])])

#
# CFLAGS/CXXFLAGS
#
CFLAGS="-Wall $CFLAGS $SSL_TLS_CFLAGS"
CXXFLAGS="-Wall $CXXFLAGS $SSL_TLS_CFLAGS"

#
# Config files
#
AC_CONFIG_FILES([Makefile
				docs/Makefile
				lib/Makefile
				lib/libchmpx.pc
				src/Makefile
				tests/Makefile
				buildutils/Makefile
				buildutils/control
				buildutils/copyright
				buildutils/rules
				buildutils/chmpx.spec
				buildutils/Dockerfile.templ
				buildutils/APKBUILD.templ])

AC_OUTPUT

#
# Local variables:
# tab-width: 4
# c-basic-offset: 4
# End:
# vim600: noexpandtab sw=4 ts=4 fdm=marker
# vim<600: noexpandtab sw=4 ts=4
#
