#!/bin/sh
#
# CHMPX
#
# Copyright 2018 Yahoo Japan Corporation.
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
# CREATE:   Tue Jun 5 2018
# REVISION:
#

#==========================================================
# Common setting
#==========================================================
#
# Instead of pipefail(for shells not support "set -o pipefail")
#
PIPEFAILURE_FILE="/tmp/.pipefailure.$(od -An -tu4 -N4 /dev/random | tr -d ' \n')"

#
# Set Locate
#
if command -v locale >/dev/null 2>&1; then
	if locale -a | grep -q -i '^[[:space:]]*C.utf8[[:space:]]*$'; then
		LANG=$(locale -a | grep -i '^[[:space:]]*C.utf8[[:space:]]*$' | sed -e 's/^[[:space:]]*//g' -e 's/[[:space:]]*$//g' | tr -d '\n')
		LC_ALL="${LANG}"
		export LANG
		export LC_ALL
	elif locale -a | grep -q -i '^[[:space:]]*en_US.utf8[[:space:]]*$'; then
		LANG=$(locale -a | grep -i '^[[:space:]]*en_US.utf8[[:space:]]*$' | sed -e 's/^[[:space:]]*//g' -e 's/[[:space:]]*$//g' | tr -d '\n')
		LC_ALL="${LANG}"
		export LANG
		export LC_ALL
	fi
fi

#==========================================================
# Common Variables
#==========================================================
PRGNAME=$(basename "${0}")
SCRIPTDIR=$(dirname "${0}")
SCRIPTDIR=$(cd "${SCRIPTDIR}" || exit 1; pwd)

#
# Other variables
#
RUN_MODE=""
FORCE_INIT_NSSDB=0
NSSDB_DIR=""
NSSDB_PW=""
CERT_FILE=""
KEY_FILE=""
CA_CERT_FILE=""
PKCS12_FILE=""
PKCS12_PW=""

#
# Static values
#
AUTO_PKCS12_FILE="AUTO CREATE PKCS12 FILENAME"
CA_CERT_NICKNAME="Antpickax_CHMPX_CA"

#==========================================================
# Utility functions and variables for messaging
#==========================================================
#
# Utilities for message
#
if [ -t 1 ] || { [ -n "${CI}" ] && [ "${CI}" = "true" ]; }; then
	CBLD=$(printf '\033[1m')
	CREV=$(printf '\033[7m')
	CRED=$(printf '\033[31m')
	CYEL=$(printf '\033[33m')
	CGRN=$(printf '\033[32m')
	CDEF=$(printf '\033[0m')
else
	CBLD=""
	CREV=""
	CRED=""
	CYEL=""
	CGRN=""
	CDEF=""
fi

PRNTITLE()
{
	echo "${GHAGRP_START}${CBLD}${CGRN}${CREV}[TITLE]${CDEF} ${CGRN}$*${CDEF}"
}
PRNINFO()
{
	echo "${CBLD}${CREV}[INFO]${CDEF} $*"
}
PRNWARN()
{
	echo "${CBLD}${CYEL}${CREV}[WARNING]${CDEF} ${CYEL}$*${CDEF}"
}
PRNERR()
{
	echo "${CBLD}${CRED}${CREV}[ERROR]${CDEF} ${CRED}$*${CDEF}"
}
PRNSUCCESS()
{
	echo "${CBLD}${CGRN}${CREV}[SUCCEED]${CDEF} ${CGRN}$*${CDEF}"
}
PRNFAILURE()
{
	echo "${CBLD}${CRED}${CREV}[FAILURE]${CDEF} ${CRED}$*${CDEF}"
}

#==========================================================
# Utility functions
#==========================================================
#
# Basic common utility functions.
#
#----------------------------------------------------------
# Utility : Initialize NSSDB
#----------------------------------------------------------
init_nssdb()
{
	PRNINFO "Initialize NSSDB : ${NSSDB_DIR}"

	#
	# Make password file in temporary
	#
	NSSDB_PW_FILE="/tmp/${PRGNAME}.$$.pw"
	echo "${NSSDB_PW}" > "${NSSDB_PW_FILE}"

	#
	# Initialize NSSDB
	#
	# ex) certutil -N -d sql:/etc/pki/nssdb --empty-password -f /tmp/nssdb.pw -@ /tmp/nssdb.pw
	#
	if({ "${CERTUTIL_BIN}" -N -d sql:"${NSSDB_DIR}" --empty-password -f "${NSSDB_PW_FILE}" -@ "${NSSDB_PW_FILE}" || echo > "${PIPEFAILURE_FILE}"; } | sed -e 's/^/    /g') && rm "${PIPEFAILURE_FILE}" >/dev/null 2>&1; then
		PRNERR "Failed to initialize NSSDB(${NSSDB_DIR})"
		rm -f "${NSSDB_PW_FILE}"
		return 1
	fi
	rm -f "${NSSDB_PW_FILE}"

	return 0
}

check_init_nssdb()
{
	if [ -z "${NSSDB_DIR}" ]; then
		PRNERR "Internal Error : NSSDB_DIR variable is empty."
		return 1
	fi
	if [ ! -d "${NSSDB_DIR}" ]; then
		PRNERR "${NSSDB_DIR} nssdb directory is not existed."
		return 1
	fi

	#
	# Check whether NSSDB needs to be initialized.
	#
	# ex) certutil -L -d sql:<nssdb dir>
	#
	if [ "${FORCE_INIT_NSSDB}" -eq 0 ]; then
		#
		# Make password file in temporary
		#
		NSSDB_PW_FILE="/tmp/${PRGNAME}.$$.pw"
		echo "${NSSDB_PW}" > "${NSSDB_PW_FILE}"

		#
		# Check
		#
		if "${CERTUTIL_BIN}" -L -d sql:"${NSSDB_DIR}" -f "${NSSDB_PW_FILE}" -@ "${NSSDB_PW_FILE}" >/dev/null 2>&1; then
			#
			# Already initialized NSSSDB, so nothing to do.
			#
			rm -f "${NSSDB_PW_FILE}"
			return 0
		fi
		rm -f "${NSSDB_PW_FILE}"
	fi

	#
	# Initialize nssdb
	#
	if ! init_nssdb; then
		return 1
	fi

	return 0
}

#----------------------------------------------------------
# Utility : Create PKCS#12 Certification
#----------------------------------------------------------
create_pkcs12()
{
	# [NOTE]
	# The PKCS#12 file name is optional; if omitted, it will be created in
	# the same directory as the certificate in a format that includes the
	# Subject's CN name.
	#
	# ex)	Cert file name : mycert.crt ---> <CN name>_mycert.p12
	#
	if [ "${PKCS12_FILE}" = "${AUTO_PKCS12_FILE}" ]; then
		CERT_FILENAME=$(basename "${CERT_FILE}" | sed -e 's#[\.].*$##g')

		CERT_DIR=$(dirname "${CERT_FILE}")
		CERT_DIR=$(cd "${CERT_DIR}" || exit 1; pwd)

		CERT_CN_NAME=$(grep 'Subject[[:space:]]*:' "${CERT_FILE}" | sed -e 's#^.*CN=##g' -e 's#,.*##g')

		PKCS12_FILE="${CERT_DIR}/${CERT_CN_NAME}_${CERT_FILENAME}.p12"

		if [ -f "${PKCS12_FILE}" ] || [ -d "${PKCS12_FILE}" ]; then
			PRNERR "The PKCS#12 file(${PKCS12_FILE}) to be output already exists."
			return 1
		fi
	fi

	if [ -n "${CA_CERT_FILE}" ]; then
		PARAM_CA_CERT="-certfile ${CA_CERT_FILE}"
	else
		PARAM_CA_CERT=""
	fi

	PRNINFO "Create PKCS#12 Certification : ${PKCS12_FILE}"

	#
	# Create PKCS#12 certification
	#
	# ex) openssl pkcs12 -export -inkey cert.key -in cert.crt -out cert.p12 -certfile ca.crt -passout pass:
	#
	if({ /bin/sh -c "${OPENSSL_BIN} pkcs12 -export -in ${CERT_FILE} -inkey ${KEY_FILE} ${PARAM_CA_CERT} -out ${PKCS12_FILE} -passout pass:${PKCS12_PW}" || echo > "${PIPEFAILURE_FILE}"; } | sed -e 's/^/    /g') && rm "${PIPEFAILURE_FILE}" >/dev/null 2>&1; then
		PRNERR "Failed to create PKCS#12 Certification(${PKCS12_FILE})"
		return 1
	fi
	if [ ! -f "${PKCS12_FILE}" ]; then
		PRNERR "Failed to create PKCS#12 Certification(${PKCS12_FILE}) which file is not exsted."
		return 1
	fi

	return 0
}

#----------------------------------------------------------
# Utility : Import CA Certification
#----------------------------------------------------------
import_ca_cert()
{
	PRNINFO "Import CA Certification : ${CA_CERT_FILE}"

	#
	# Make password file in temporary
	#
	NSSDB_PW_FILE="/tmp/${PRGNAME}.$$.pw"
	echo "${NSSDB_PW}" > "${NSSDB_PW_FILE}"

	#
	# Import CA Certification
	#
	# ex) certutil -A -n "Antpickax_CHMPX_CA" -i ca.crt -a -t "CT,," -d sql:<nssdb dir> -f /tmp/nssdb.pw -@ /tmp/nssdb.pw
	#
	if({ "${CERTUTIL_BIN}" -A -n "${CA_CERT_NICKNAME}" -i "${CA_CERT_FILE}" -a -t "CT,," -d sql:"${NSSDB_DIR}" -f "${NSSDB_PW_FILE}" -@ "${NSSDB_PW_FILE}" || echo > "${PIPEFAILURE_FILE}"; } | sed -e 's/^/    /g') && rm "${PIPEFAILURE_FILE}" >/dev/null 2>&1; then
		PRNERR "Failed to import CA Certification(${CA_CERT_FILE}) to NSSDB(${NSSDB_DIR})"
		rm -f "${NSSDB_PW_FILE}"
		return 1
	fi
	rm -f "${NSSDB_PW_FILE}"

	return 0
}

#----------------------------------------------------------
# Utility : Import PKCS#12 Certification
#----------------------------------------------------------
import_pkcs12()
{
	PRNINFO "Import PKCS#12 Certification : ${PKCS12_FILE}"

	#
	# Import PKCS#12 Certification
	#
	# ex) pk12util -i <pkcs#12 file> -d sql:<nssdb dir> -W "pass"
	#
	if({ "${PK12UTIL_BIN}" -i "${PKCS12_FILE}" -d sql:"${NSSDB_DIR}" -W "${PKCS12_PW}" || echo > "${PIPEFAILURE_FILE}"; } | sed -e 's/^/    /g') && rm "${PIPEFAILURE_FILE}" >/dev/null 2>&1; then
		PRNERR "Failed to import PKCS#12 Certification(${PKCS12_FILE}) to NSSDB(${NSSDB_DIR})"
		return 1
	fi

	return 0
}

#----------------------------------------------------------
# Print Usage
#----------------------------------------------------------
func_usage()
{
	echo "This script is a tool for setting the certificate in NSSDB used by CHMPX with NSS"
	echo "library."
	echo "By using this script, you can wrap the nss-tools(certutil, pk12util) and openssl"
	echo "command and easily configure certificates to NSSDB."
	echo ""
	echo "Usage:"
	echo "  $1 init [--nssdb <dir>] [--nssdb-pw <pass>]"
	echo "  $1 pkcs12 --cert <file> --key <file> [--ca-cert <file>] [--pkcs12 <file>] [--pkcs-pw <pass>]"
	echo "  $1 import --ca-cert <file> [--force-init-nssdb] [--nssdb <dir>] [--nssdb-pw <pass>]"
	echo "  $1 import --pkcs12 <file> [--pkcs-pw <pass>] [--force-init-nssdb] [--nssdb <dir>] [--nssdb-pw <pass>]"
	echo "  $1 all --cert <file> --key <file> [--ca-cert <file>] [--pkcs12 <file>] [--pkcs-pw <pass>] [--force-init-nssdb] [--nssdb <dir>] [--nssdb-pw <pass>]"
	echo ""
	echo "Mode and Options:"
	echo "  <init>"
	echo "    Initialize NSSDB(be careful)."
	echo "      --nssdb <dir>         Specify the NSSDB directory path."
	echo "                            If omitted, the value of the SSL_DIR environment"
	echo "                            variable is used instead. If the environment is also"
	echo "                            not specified, /etc/pki/nssdb is used as the default"
	echo "                            value."
	echo "      --nssdb-pw <pass>     Specify the NSSDB password. If omitted, an empty"
	echo "                            password will be used."
	echo ""
	echo "  <pkcs12>"
	echo "    Convert PEM files (CRT, KEY) to PKCS#12 certificates."
	echo "      --cert <file>         Specify the PEM certificate. This option is required."
	echo "      --key <file>          Specify the PEM private key. This option is required."
	echo "      --ca-cert <file>      Specify the CA certificate that signed the certificate."
	echo "                            It can be omitted if it has already been imported into"
	echo "                            NSSDB."
	echo "      --pkcs12 <file>       Specify the file path of the PKCS#12 certificate to be"
	echo "                            created."
	echo "                            If omitted, the file name will be a combination of"
	echo "                            Subject's CN(Common Name) and the certificate file name"
	echo "                            in the same directory as the PEM certificate."
	echo "                            (<CN>_<cert name>.p12)"
	echo "      --pkcs-pw <pass>      Specify the passphrase for the PKCS#12 certificate that"
	echo "                            will be created."
	echo "                            If omitted, an empty passphrase will be used."
	echo ""
	echo "  <import CA cert>"
	echo "    Import the CA certificate as Trusted CA into NSSDB."
	echo "      --ca-cert <file>      Specify the file path of the CA certificate."
	echo "                            This option is required."
	echo "      --force-init-nssdb    Force initialization of NSSDB. This is optional."
	echo "      --nssdb <dir>         Specify the NSSDB directory path."
	echo "                            If omitted, the value of the SSL_DIR environment"
	echo "                            variable is used instead. If the environment is also"
	echo "                            not specified, /etc/pki/nssdb is used as the default"
	echo "                            value."
	echo "      --nssdb-pw <pass>     Specify the NSSDB password. If omitted, an empty"
	echo "                            password will be used."
	echo ""
	echo "  <import PKCS#12>"
	echo "    Import the PKCS#12 certificate into NSSDB."
	echo "      --pkcs12 <file>       Specify the PKCS#12 certificate."
	echo "                            This option is required."
	echo "      --pkcs-pw <pass>      Specify the passphrase for the PKCS#12 certificate"
	echo "                            that will be created."
	echo "                            If omitted, an empty passphrase will be used."
	echo "      --force-init-nssdb    Force initialization of NSSDB. This is optional."
	echo "      --nssdb <dir>         Specify the NSSDB directory path."
	echo "                            If omitted, the value of the SSL_DIR environment"
	echo "                            variable is used instead. If the environment is also"
	echo "                            not specified, /etc/pki/nssdb is used as the default"
	echo "                            value."
	echo "      --nssdb-pw <pass>     Specify the NSSDB password. If omitted, an empty"
	echo "                            password will be used."
	echo ""
	echo "  <all>"
	echo "    Performs NSSDB initialization(init), import CA certificate, PKCS#12 certificate"
	echo "    conversion and import it as a series of processes."
	echo "      --cert <file>         Specify the PEM certificate. This option is required."
	echo "      --key <file>          Specify the PEM private key. This option is required."
	echo ""
	echo "      --ca-cert <file>      Specify the CA certificate that signed the certificate."
	echo "                            It can be omitted if it has already been imported into"
	echo "                            NSSDB."
	echo "      --pkcs12 <file>       Specify the file path of the PKCS#12 certificate to be"
	echo "                            created."
	echo "                            If omitted, the file name will be a combination of"
	echo "                            Subject's CN(Common Name) and the certificate file name"
	echo "                            in the same directory as the PEM certificate."
	echo "                            (<CN>_<cert name>.p12)"
	echo "      --pkcs-pw <pass>      Specify the passphrase for the PKCS#12 certificate"
	echo "                            that will be created."
	echo "                            If omitted, an empty passphrase will be used."
	echo "      --force-init-nssdb    Force initialization of NSSDB. This is optional."
	echo "      --nssdb <dir>         Specify the NSSDB directory path."
	echo "                            If omitted, the value of the SSL_DIR environment"
	echo "                            variable is used instead. If the environment is also"
	echo "                            not specified, /etc/pki/nssdb is used as the default"
	echo "                            value."
	echo "      --nssdb-pw <pass>     Specify the NSSDB password. If omitted, an empty"
	echo "                            password will be used."
	echo ""
	echo "Notes:"
	echo "  In some mode(<import CA cert>, <import PKCS#12> and <all>), if NSSDB is not"
	echo "  initialized, it will be initialized automatically."
	echo ""
	echo "Reference:"
	echo "  The following certutil, pk12util, openssl commands(examples) are used."
	echo "  <certutil>"
	echo "    certutil -L -d sql:<nssdb dir>"
	echo "    certutil -U -d sql:<nssdb dir>"
	echo "    certutil -K -d sql:<nssdb dir>"
	echo "    certutil -L -n <nickname> -d sql:<nssdb dir>"
	echo "    certutil -L -a -n <nickname> -d sql:<nssdb dir>"
	echo "    certutil -D -n <nickname> -d sql:<nssdb dir>"
	echo "    certutil -V -u [V|C|A|Y] -n <nickname> -d sql:<nssdb dir>"
	echo "    certutil -O -n <nickname> -d sql:<nssdb dir>"
	echo ""
	echo "  <pk12util>"
	echo "    pk12util -i <pkcs#12 file> -d sql:<nssdb dir> -W \"<pass>\""
	echo ""
	echo "  <openssl>"
	echo "    openssl pkcs12 -export -in <PEM file> -inkey <PEM private key>"
	echo "                  -out <pkcs#12 file> -certfile <ca cert> -passout pass:<pass>"
	echo ""
}

#==========================================================
# Parse options
#==========================================================
#
# Variables
#
OPT_RUN_MODE=""
OPT_FORCE_INIT_NSSDB=0
OPT_NSSDB_DIR=""
OPT_NSSDB_PW=""
OPT_CERT_FILE=""
OPT_KEY_FILE=""
OPT_CA_CERT_FILE=""
OPT_PKCS12_FILE=""
OPT_PKCS12_PW=""

while [ $# -ne 0 ]; do
	if [ -z "$1" ]; then
		break

	elif [ "$1" = "-h" ] || [ "$1" = "-H" ] || [ "$1" = "--help" ] || [ "$1" = "--HELP" ]; then
		func_usage "${PRGNAME}"
		exit 0

	elif [ "$1" = "--force-init-nssdb" ] || [ "$1" = "--FORCE-INIT-NSSDB" ]; then
		if [ "${OPT_FORCE_INIT_NSSDB}" -ne 0 ]; then
			PRNERR "already specify \"--force-init-nssdb\" option."
			exit 1
		fi
		OPT_FORCE_INIT_NSSDB=1

	elif [ "$1" = "--nssdb" ] || [ "$1" = "--NSSDB" ]; then
		if [ -n "${OPT_NSSDB_DIR}" ]; then
			PRNERR "already set \"--nssdb\" option(${OPT_NSSDB_DIR})."
			exit 1
		fi
		shift
		if [ $# -eq 0 ]; then
			PRNERR "\"--nssdb\" option is specified without parameter."
			exit 1
		fi
		OPT_NSSDB_DIR="$1"

	elif [ "$1" = "--nssdb-pw" ] || [ "$1" = "--NSSDB-PW" ]; then
		if [ -n "${OPT_NSSDB_PW}" ]; then
			PRNERR "already set \"--nssdb-pw\" option(********)."
			exit 1
		fi
		shift
		if [ $# -eq 0 ]; then
			PRNERR "\"--nssdb-pw\" option is specified without parameter."
			exit 1
		fi
		OPT_NSSDB_PW="$1"

	elif [ "$1" = "--cert" ] || [ "$1" = "--CERT" ]; then
		if [ -n "${OPT_CERT_FILE}" ]; then
			PRNERR "already set \"--cert\" option(${OPT_CERT_FILE})."
			exit 1
		fi
		shift
		if [ $# -eq 0 ]; then
			PRNERR "\"--cert\" option is specified without parameter."
			exit 1
		fi
		if [ ! -f "$1" ]; then
			PRNERR "$1 file does not exist."
			exit 1
		fi
		OPT_CERT_FILE="$1"

	elif [ "$1" = "--key" ] || [ "$1" = "--KEY" ]; then
		if [ -n "${OPT_KEY_FILE}" ]; then
			PRNERR "already set \"--key\" option(${OPT_KEY_FILE})."
			exit 1
		fi
		shift
		if [ $# -eq 0 ]; then
			PRNERR "\"--key\" option is specified without parameter."
			exit 1
		fi
		if [ ! -f "$1" ]; then
			PRNERR "$1 file does not exist."
			exit 1
		fi
		OPT_KEY_FILE="$1"

	elif [ "$1" = "--ca-cert" ] || [ "$1" = "--CA-CERT" ]; then
		if [ -n "${OPT_CA_CERT_FILE}" ]; then
			PRNERR "already set \"--ca-cert\" option(${OPT_CA_CERT_FILE})."
			exit 1
		fi
		shift
		if [ $# -eq 0 ]; then
			PRNERR "\"--ca-cert\" option is specified without parameter."
			exit 1
		fi
		if [ ! -f "$1" ]; then
			PRNERR "$1 file does not exist."
			exit 1
		fi
		OPT_CA_CERT_FILE="$1"

	elif [ "$1" = "--pkcs12" ] || [ "$1" = "--PKCS12" ]; then
		if [ -n "${OPT_PKCS12_FILE}" ]; then
			PRNERR "already set \"--pkcs12\" option(${OPT_PKCS12_FILE})."
			exit 1
		fi
		shift
		if [ $# -eq 0 ]; then
			PRNERR "\"--pkcs12\" option is specified without parameter."
			exit 1
		fi
		# [NOTE]
		# Confirming the existence or non-existence of the PKCS#12 file will
		# be checked later depending on the program execution mode, so we
		# will not check it here.
		#
		OPT_PKCS12_FILE="$1"

	elif [ "$1" = "--pkcs12-pw" ] || [ "$1" = "--PKCS12-PW" ]; then
		if [ -n "${OPT_PKCS12_PW}" ]; then
			PRNERR "already set \"--pkcs12-pw\" option(********)."
			exit 1
		fi
		shift
		if [ $# -eq 0 ]; then
			PRNERR "\"--pkcs12-pw\" option is specified without parameter."
			exit 1
		fi
		OPT_PKCS12_PW="$1"

	elif [ "$1" = "init" ] || [ "$1" = "INIT" ]; then
		if [ -n "${OPT_RUN_MODE}" ]; then
			PRNERR "already set program run mode(${OPT_RUN_MODE})."
			exit 1
		fi
		OPT_RUN_MODE="init"

	elif [ "$1" = "pkcs12" ] || [ "$1" = "PKCS12" ]; then
		if [ -n "${OPT_RUN_MODE}" ]; then
			PRNERR "already set program run mode(${OPT_RUN_MODE})."
			exit 1
		fi
		OPT_RUN_MODE="pkcs12"

	elif [ "$1" = "import" ] || [ "$1" = "IMPORT" ]; then
		if [ -n "${OPT_RUN_MODE}" ]; then
			PRNERR "already set program run mode(${OPT_RUN_MODE})."
			exit 1
		fi
		OPT_RUN_MODE="import"

	elif [ "$1" = "all" ] || [ "$1" = "ALL" ]; then
		if [ -n "${OPT_RUN_MODE}" ]; then
			PRNERR "already set program run mode(${OPT_RUN_MODE})."
			exit 1
		fi
		OPT_RUN_MODE="all"

	else
		PRNERR "Unknown option or parameter: $1"
		exit 1
	fi
	shift
done

#
# Check options/parameters
#
if [ -z "${OPT_RUN_MODE}" ]; then
	PRNERR "Program run mode(init/pkcs12/import/all) is not specified."
	exit 1
fi

if [ "${OPT_RUN_MODE}" = "init" ]; then
	#
	# Initialize NSSDB
	#
	if [ "${OPT_FORCE_INIT_NSSDB}" -ne 0 ] || [ -n "${OPT_CERT_FILE}" ] || [ -n "${OPT_KEY_FILE}" ] || [ -n "${OPT_CA_CERT_FILE}" ] || [ -n "${OPT_PKCS12_FILE}" ] || [ -n "${OPT_PKCS12_PW}" ]; then
		PRNERR "Some unnecessary options were specified for this mode(init)."
		exit 1
	fi

elif [ "${OPT_RUN_MODE}" = "pkcs12" ]; then
	#
	# Create PKCS#12 certification
	#
	if [ -n "${OPT_NSSDB_DIR}" ] || [ -n "${OPT_NSSDB_PW}" ] || [ "${OPT_FORCE_INIT_NSSDB}" -ne 0 ]; then
		PRNERR "Some unnecessary options were specified for this mode(pkcs12)."
		exit 1
	fi
	if [ -z "${OPT_CERT_FILE}" ] || [ -z "${OPT_KEY_FILE}" ]; then
		PRNERR "Some required options for this mode(pkcs12) were not specified."
		exit 1
	fi
	if [ ! -f "${OPT_CERT_FILE}" ] || [ ! -f "${OPT_KEY_FILE}" ]; then
		PRNERR "Cert file(${OPT_CERT_FILE}) or Key file(${OPT_KEY_FILE}) were not specified."
		exit 1
	fi

	# [NOTE]
	# The PKCS#12 file name is optional; if omitted, it will be created in
	# the same directory as the certificate in a format that includes the
	# Subject's CN name.
	#
	if [ -z "${OPT_PKCS12_FILE}" ]; then
		OPT_PKCS12_FILE="${AUTO_PKCS12_FILE}"
	else
		if [ -f "${OPT_PKCS12_FILE}" ] || [ -d "${OPT_PKCS12_FILE}" ]; then
			PRNERR "The PKCS#12 file(${OPT_PKCS12_FILE}) to be output already exists."
			exit 1
		fi
	fi

elif [ "${OPT_RUN_MODE}" = "import" ]; then
	#
	# Import
	#
	if [ -n "${OPT_CA_CERT_FILE}" ]; then
		#
		# import CA cert
		#
		if [ -n "${OPT_CERT_FILE}" ] || [ -n "${OPT_KEY_FILE}" ] || [ -n "${OPT_PKCS12_FILE}" ] || [ -n "${OPT_PKCS12_PW}" ]; then
			PRNERR "Some unnecessary options were specified for this mode(import ca cert)."
			exit 1
		fi
		if [ ! -f "${OPT_CA_CERT_FILE}" ]; then
			PRNERR "The specified CA certification file(${OPT_CA_CERT_FILE}) does not exist."
			exit 1
		fi
		OPT_RUN_MODE="import_ca"
	else
		#
		# import PKCS#12 cert
		#
		if [ -n "${OPT_CERT_FILE}" ] || [ -n "${OPT_KEY_FILE}" ]; then
			PRNERR "Some unnecessary options were specified for this mode(import pkcs#12 cert)."
			exit 1
		fi
		if [ ! -f "${OPT_PKCS12_FILE}" ]; then
			PRNERR "The specified PKCS#12 file(${OPT_PKCS12_FILE}) does not exist."
			exit 1
		fi
		OPT_RUN_MODE="import_pkcs12"
	fi

elif [ "${OPT_RUN_MODE}" = "all" ]; then
	#
	# Do all
	#
	if [ -z "${OPT_CERT_FILE}" ] || [ -z "${OPT_KEY_FILE}" ]; then
		PRNERR "Some required options for this mode(${OPT_RUN_MODE}) were not specified."
		exit 1
	fi
	if [ ! -f "${OPT_CERT_FILE}" ] || [ ! -f "${OPT_KEY_FILE}" ]; then
		PRNERR "Cert file(${OPT_CERT_FILE}) or Key file(${OPT_KEY_FILE}) were not specified."
		exit 1
	fi

	# [NOTE]
	# The PKCS#12 file name is optional; if omitted, it will be created in
	# the same directory as the certificate in a format that includes the
	# Subject's CN name.
	#
	if [ -z "${OPT_PKCS12_FILE}" ]; then
		OPT_PKCS12_FILE="${AUTO_PKCS12_FILE}"
	else
		if [ -f "${OPT_PKCS12_FILE}" ] || [ -d "${OPT_PKCS12_FILE}" ]; then
			PRNERR "The PKCS#12 file(${OPT_PKCS12_FILE}) to be output already exists."
			exit 1
		fi
	fi

else
	PRNERR "Fatal internal error."
	exit 1
fi

#
# NSSDB
#
# [NOTE]
# Required for cases other than Create PKCS#12 Cert.
# It can be specified as an option(--nssdb), or set to the SSL_DIR
# environment variable or default value(/etc/pki/nssdb).
# If the NSSDB directory does not exist, it will be created here and
# initialized when the process runs.
#
if [ "${OPT_RUN_MODE}" != "pkcs12" ]; then
	if [ -z "${OPT_NSSDB_DIR}" ]; then
		if [ -n "${SSL_DIR}" ]; then
			if [ ! -d "${SSL_DIR}" ]; then
				PRNWARN "Since the \"--nssdb\" option is not specified, it uses the \"SSL_DIR\" environment variable, but the directory(${SSL_DIR}) specified by the \"SSL_DIR\" variable does not exist, so create it."
				if ! mkdir -p "${SSL_DIR}"; then
					PRNERR "Failed to create ${SSL_DIR} directory."
					exit 1
				fi
				OPT_FORCE_INIT_NSSDB=1
			fi
			OPT_NSSDB_DIR="${SSL_DIR}"
		else
			# [NOTE]
			# The default SSL_DIR path is usually ~/.netscape,
			# but this tool uses /etc/pki/nssdb.
			#
			if [ ! -d /etc/pki/nssdb ]; then
				PRNWARN "The /etc/pki/nssdb directory does not exist, so try to create it."
				if ! mkdir -p /etc/pki/nssdb; then
					PRNERR "Failed to create /etc/pki/nssdb directory."
					exit 1
				fi
				OPT_FORCE_INIT_NSSDB=1
			fi
			OPT_NSSDB_DIR="/etc/pki/nssdb"
		fi
	else
		if [ ! -d "${OPT_NSSDB_DIR}" ]; then
			PRNWARN "The ${OPT_NSSDB_DIR} directory does not exist, so try to create it."
			if ! mkdir -p "${OPT_NSSDB_DIR}"; then
				PRNERR "Failed to create ${OPT_NSSDB_DIR} directory."
				exit 1
			fi
			OPT_FORCE_INIT_NSSDB=1
		fi
	fi

	#
	# Clear SSL_DIR environment
	#
	unset SSL_DIR
fi

#
# Set all option
#
RUN_MODE="${OPT_RUN_MODE}"
FORCE_INIT_NSSDB=${OPT_FORCE_INIT_NSSDB}
NSSDB_DIR="${OPT_NSSDB_DIR}"
NSSDB_PW="${OPT_NSSDB_PW}"
CERT_FILE="${OPT_CERT_FILE}"
KEY_FILE="${OPT_KEY_FILE}"
CA_CERT_FILE="${OPT_CA_CERT_FILE}"
PKCS12_FILE="${OPT_PKCS12_FILE}"
PKCS12_PW="${OPT_PKCS12_PW}"

#----------------------------------------------------------
# Main Processing
#----------------------------------------------------------
#
# Check Utility Command
#
if [ "${RUN_MODE}" = "pkcs12" ] || [ "${RUN_MODE}" = "all" ]; then
	OPENSSL_BIN=$(command -v openssl | tr -d '\n')
	if [ -z "${OPENSSL_BIN}" ]; then
		PRNERR "openssl command is not found, you must install openssl package."
		exit 1
	fi
fi
if [ "${RUN_MODE}" != "pkcs12" ]; then
	CERTUTIL_BIN=$(command -v certutil | tr -d '\n')
	if [ -z "${CERTUTIL_BIN}" ]; then
		PRNERR "certutil command is not found, you must install nss-tools package."
		exit 1
	fi
	PK12UTIL_BIN=$(command -v pk12util | tr -d '\n')
	if [ -z "${PK12UTIL_BIN}" ]; then
		PRNERR "pk12util command is not found, you must install nss-tools package."
		exit 1
	fi
fi

#
# Processing by Mode
#
if [ "${RUN_MODE}" = "init" ]; then
	#
	# Initialize NSSDB
	#
	PRNTITLE "Initialize NSSDB"

	if ! init_nssdb; then
		PRNFAILURE "Initialize NSSDB"
		exit 1
	fi
	PRNSUCCESS "Initialized NSSDB"

elif [ "${RUN_MODE}" = "pkcs12" ]; then
	#
	# Create PKCS#12 certification
	#
	PRNTITLE "Create PKCS#12 certification"

	if ! create_pkcs12; then
		PRNFAILURE "Create PKCS#12 certification"
		exit 1
	fi
	PRNSUCCESS "Created PKCS#12 certification"

elif [ "${RUN_MODE}" = "import_ca" ]; then
	#
	# Import CA Certification
	#
	PRNTITLE "Import CA Certification"

	if ! check_init_nssdb; then
		PRNFAILURE "Import CA Certification"
		exit 1
	fi
	if ! import_ca_cert; then
		PRNFAILURE "Import CA Certification"
		exit 1
	fi
	PRNSUCCESS "Imported CA Certification"

elif [ "${RUN_MODE}" = "import_pkcs12" ]; then
	#
	# Import PKCS#12 Certification
	#
	PRNTITLE "Import PKCS#12 Certification"

	if ! check_init_nssdb; then
		PRNFAILURE "Import PKCS#12 Certification"
		exit 1
	fi
	if ! import_pkcs12; then
		PRNFAILURE "Import PKCS#12 Certification"
		exit 1
	fi
	PRNSUCCESS "Imported PKCS#12 Certification"

elif [ "${RUN_MODE}" = "all" ]; then
	#
	# Do all processing
	#
	PRNTITLE "Do all processing"

	if ! check_init_nssdb; then
		PRNFAILURE "Do all processing"
		exit 1
	fi
	if [ -n "${CA_CERT_FILE}" ]; then
		if ! import_ca_cert; then
			PRNFAILURE "Do all processing"
			exit 1
		fi
	fi
	if ! create_pkcs12; then
		PRNFAILURE "Do all processing"
		exit 1
	fi
	if ! import_pkcs12; then
		PRNFAILURE "Do all processing"
		exit 1
	fi
	PRNSUCCESS "Done all processing"

else
	PRNERR "Fatal internal error."
	exit 1
fi

exit 0

#
# Local variables:
# tab-width: 4
# c-basic-offset: 4
# End:
# vim600: noexpandtab sw=4 ts=4 fdm=marker
# vim<600: noexpandtab sw=4 ts=4
#
