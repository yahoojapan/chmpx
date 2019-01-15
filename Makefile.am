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

SUBDIRS=lib src tests docs buildutils
ACLOCAL_AMFLAGS = -I m4
EXTRA_DIST=RELEASE_VERSION

CPPCHECK_CMD=	cppcheck
CPPCHECK_OPT=	--quiet \
				--error-exitcode=1 \
				--inline-suppr \
				-j 4 \
				--std=c++03 \
				--xml-version=2 \
				--enable=warning,style,information,missingInclude
CPPCHECK_DEF=	-D SO_REUSEPORT
CPPCHECK_IGN=

# [NOTE]
# Building chmpx on Travis CI is running in several OS.
# Depending on the OS(CentOS, ubuntu, debian, fedora), different
# cppcheck versions are installed. Certain versions of cppcheck
# are incompatible with the chmpx template source code and are
# very poor performance on the VM or container(Travis CI).
# On problematic versions, timeout error on Travis CI.
# To avoid this we skip only specific versions of cppcheck on
# travis. Even if we skip a specific version of the cppcheck,
# it will be tested with a combination of other versions and OS.
# When running Travis, the TRAVIS_TAG environment variable is
# set, and this environment variable is used for judgment.
# 
# Currently, debian uses cppcheck 1.76, and CentOS6 uses 1.63,
# there is a problem with this old version, skipping this.
# If debian/etc will support 1.8x or later versions in the
# future, this skipped process will be unnecessary.
#
CPPCHECK_NGVER=	-e 1\\.7 -e 1\\.6

cppcheck:
	@echo "$(CPPCHECK_CMD) $(CPPCHECK_OPT) $(CPPCHECK_DEF) $(CPPCHECK_IGN) $(SUBDIRS)"
	@((((env | grep TRAVIS_TAG >/dev/null 2>&1) && echo -n TRAVISENVFOUND || echo -n NOTFOUND) && $(CPPCHECK_CMD) --version 2>/dev/null) | grep TRAVISENVFOUND | grep $(CPPCHECK_NGVER) >/dev/null 2>&1) && \
		echo " --> Skip, because this old $(CPPCHECK_CMD) version is very poor performance on VM(CI)" || \
		$(CPPCHECK_CMD) $(CPPCHECK_OPT) $(CPPCHECK_DEF) $(CPPCHECK_IGN) $(SUBDIRS)

#
# VIM modelines
#
# vim:set ts=4 fenc=utf-8:
#
