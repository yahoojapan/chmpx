#!/bin/sh
#
# Utility tools for building configure/packages by AntPickax
#
# Copyright 2018 Yahoo Japan Corporation.
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

#
# Puts project version/revision/age/etc variables for building
#
func_usage()
{
	echo ""
	echo "Usage:  $1 [-pkg_version | -lib_version_info | -lib_version_for_link | -major_number | -debhelper_dep | -rpmpkg_group]"
	echo "	-pkg_version            returns package version."
	echo "	-lib_version_info       returns library libtools revision"
	echo "	-lib_version_for_link   return library version for symbolic link"
	echo "	-major_number           return major version number"
	echo "	-debhelper_dep          return debhelper dependency string"
	echo "	-rpmpkg_group           return group string for rpm package"
	echo "	-h(help)                print help."
	echo ""
}

PRGNAME=$(basename "$0")
MYSCRIPTDIR=$(dirname "$0")
SRCTOP=$(cd "${MYSCRIPTDIR}/.." || exit 1; pwd)
RELEASE_VERSION_FILE="${SRCTOP}/RELEASE_VERSION"

#
# Check options
#
PRGMODE=""
while [ $# -ne 0 ]; do
	if [ -z "$1" ]; then
		break;

	elif [ "$1" = "-h" ] || [ "$1" = "-help" ]; then
		func_usage "${PRGNAME}"
		exit 0

	elif [ "$1" = "-pkg_version" ]; then
		PRGMODE="PKG"

	elif [ "$1" = "-lib_version_info" ]; then
		PRGMODE="LIB"

	elif [ "$1" = "-lib_version_for_link" ]; then
		PRGMODE="LINK"

	elif [ "$1" = "-major_number" ]; then
		PRGMODE="MAJOR"

	elif [ "$1" = "-debhelper_dep" ]; then
		PRGMODE="DEBHELPER"

	elif [ "$1" = "-rpmpkg_group" ]; then
		PRGMODE="RPMGROUP"

	else
		echo "ERROR: unknown option $1" 1>&2
		printf '0'
		exit 1
	fi
	shift
done
if [ -z "${PRGMODE}" ]; then
	echo "ERROR: option is not specified." 1>&2
	printf '0'
	exit 1
fi

#
# Make result
#
if [ "${PRGMODE}" = "PKG" ]; then
	RESULT=$(cat "${RELEASE_VERSION_FILE}")

elif [ "${PRGMODE}" = "LIB" ] || [ "${PRGMODE}" = "LINK" ]; then
	MAJOR_VERSION=$(sed -e 's/["|\.]/ /g' "${RELEASE_VERSION_FILE}" | awk '{print $1}')
	MID_VERSION=$(sed -e 's/["|\.]/ /g' "${RELEASE_VERSION_FILE}" | awk '{print $2}')
	LAST_VERSION=$(sed -e 's/["|\.]/ /g' "${RELEASE_VERSION_FILE}" | awk '{print $3}')

	# check version number
	# shellcheck disable=SC2003
	expr "${MAJOR_VERSION}" + 1 >/dev/null 2>&1
	if [ $? -ge 2 ]; then
		echo "ERROR: wrong version number in RELEASE_VERSION file" 1>&2
		printf '0'
		exit 1
	fi
	# shellcheck disable=SC2003
	expr "${MID_VERSION}" + 1 >/dev/null 2>&1
	if [ $? -ge 2 ]; then
		echo "ERROR: wrong version number in RELEASE_VERSION file" 1>&2
		printf '0'
		exit 1
	fi
	# shellcheck disable=SC2003
	expr "${LAST_VERSION}" + 1 >/dev/null 2>&1
	if [ $? -ge 2 ]; then
		echo "ERROR: wrong version number in RELEASE_VERSION file" 1>&2
		printf '0'
		exit 1
	fi

	# make library revision number
	if [ "${MID_VERSION}" -gt 0 ]; then
		REV_VERSION=$((MID_VERSION * 100))
		REV_VERSION=$((MID_VERSION * 100))
		REV_VERSION=$((LAST_VERSION + REV_VERSION))
	else
		REV_VERSION="${LAST_VERSION}"
	fi

	if [ "${PRGMODE}" = "LIB" ]; then
		RESULT="${MAJOR_VERSION}:${REV_VERSION}:0"
	else
		RESULT="${MAJOR_VERSION}.0.${REV_VERSION}"
	fi

elif [ "${PRGMODE}" = "MAJOR" ]; then
	RESULT=$(sed 's/["|\.]/ /g' "${RELEASE_VERSION_FILE}" | awk '{print $1}')

elif [ "${PRGMODE}" = "DEBHELPER" ]; then
	# [NOTE]
	# This option returns debhelper dependency string in control file for debian package.
	# That string is depended debhelper package version and os etc.
	# (if not ubuntu/debian os, returns default string)
	#
	OS_ID_STRING=$(grep '^ID[[:space:]]*=[[:space:]]*' /etc/os-release | sed -e 's|^ID[[:space:]]*=[[:space:]]*||g' -e 's|^[[:space:]]*||g' -e 's|[[:space:]]*$||g')

	DEBHELPER_MAJOR_VER=$(apt-cache show debhelper 2>/dev/null | grep Version 2>/dev/null | awk '{print $2}' 2>/dev/null | sed 's/\..*/ /g' 2>/dev/null)
	# shellcheck disable=SC2003
	expr "${DEBHELPER_MAJOR_VER}" + 1 >/dev/null 2>&1
	if [ $? -ne 0 ]; then
		DEBHELPER_MAJOR_VER=0
	fi

	if [ -n "${OS_ID_STRING}" ]; then
		if [ "${OS_ID_STRING}" = "debian" ]; then
			RESULT="debhelper (>= 9.20160709) | dh-systemd, autotools-dev"

		elif [ "${OS_ID_STRING}" = "ubuntu" ]; then
			if [ ${DEBHELPER_MAJOR_VER} -lt 10 ]; then
				RESULT="debhelper (>= 9.20160709) | dh-systemd, autotools-dev"
			else
				RESULT="debhelper (>= 9.20160709) | dh-systemd"
			fi
		else
			# Not debian/ubuntu, set default
			RESULT="debhelper (>= 9.20160709) | dh-systemd, autotools-dev"
		fi
	else
		# Unknown OS, set default
		RESULT="debhelper (>= 9.20160709) | dh-systemd, autotools-dev"
	fi

elif [ "${PRGMODE}" = "RPMGROUP" ]; then
	# [NOTE]
	# Fedora rpm does not need "Group" key in spec file.
	# If not fedora, returns "NEEDRPMGROUP", and you must replace this string in configure.ac
	#
	if grep -q '^ID[[:space:]]*=[[:space:]]*["]*fedora["]*[[:space:]]*$' /etc/os-release; then
		RESULT=""
	else
		RESULT="NEEDRPMGROUP"
	fi
fi

#
# Output result
#
printf '%s' "${RESULT}"

exit 0

#
# Local variables:
# tab-width: 4
# c-basic-offset: 4
# End:
# vim600: noexpandtab sw=4 ts=4 fdm=marker
# vim<600: noexpandtab sw=4 ts=4
#
