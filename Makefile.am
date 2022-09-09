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

SUBDIRS=lib src tests docs buildutils
ACLOCAL_AMFLAGS = -I m4
EXTRA_DIST=RELEASE_VERSION @CONFIGURECUSTOM@

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
# Building chmpx on Github Actions(CI) is running in several OS.
# Depending on the OS(CentOS, ubuntu, debian, fedora), different
# cppcheck versions are installed. Certain versions of cppcheck
# are incompatible with the chmpx template source code and are
# very poor performance on the VM or container(CI).
# In the problematic version, cppcheck runs very slowly. To avoid
# this we skip only specific versions of cppcheck. Even if we
# skip a specific version of the cppcheck, it will be tested
# with a combination of other versions and OS.
# When the process runs on Github Actions, the CI environment is
# set true.
# 
# Currently, old debian and centos use cppcheck 1.7x(or 1.6x),
# there is a problem with these old version and skipping these.
# If the other will support 1.8x or later versions in the
# future, this skipped process will be unnecessary.
#
CPPCHECK_NGVER=	-e 1\\.7 -e 1\\.6

cppcheck:
	echo "$(CPPCHECK_CMD) $(CPPCHECK_OPT) $(CPPCHECK_DEF) $(CPPCHECK_IGN) $(SUBDIRS)"
	if test "X$$CI" = "Xtrue"; then \
		if ($(CPPCHECK_CMD) --version | grep $(CPPCHECK_NGVER)) >/dev/null 2>&1; then \
			echo " --> Skip, because this old $(CPPCHECK_CMD) version is very poor performance on VM(CI)"; \
		else \
			$(CPPCHECK_CMD) $(CPPCHECK_OPT) $(CPPCHECK_DEF) $(CPPCHECK_IGN) $(SUBDIRS); \
		fi; \
	else \
		$(CPPCHECK_CMD) $(CPPCHECK_OPT) $(CPPCHECK_DEF) $(CPPCHECK_IGN) $(SUBDIRS); \
	fi

#
# Local variables:
# tab-width: 4
# c-basic-offset: 4
# End:
# vim600: noexpandtab sw=4 ts=4 fdm=marker
# vim<600: noexpandtab sw=4 ts=4
#
