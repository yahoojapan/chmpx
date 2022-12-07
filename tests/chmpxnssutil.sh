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

#--------------------------------------------------------------
# Common Variables
#--------------------------------------------------------------
PRGNAME=$(basename "${0}")
SCRIPTDIR=$(dirname "${0}")
SCRIPTDIR=$(cd "${SCRIPTDIR}" || exit 1; pwd)
#SRCTOP=$(cd "${SCRIPTDIR}/../.." || exit 1; pwd)

#--------------------------------------------------------------
# Usage
#--------------------------------------------------------------
func_usage()
{
	echo ""
	echo "This script is a simple tool for loading PEM format CA certificates and server certificates used in OpenSSL etc into NSSDB."
	echo ""
	echo "Usage:   $1 init [-pass <pass phrase>] [-nssdir <dir>]"
	echo "         $1 pkcs12 -pem <file> [-pkey <private key>] [-nickname <nickname>] [-pass <pass phrase>] [-out <filename>]"
	echo "         $1 import -pem <file> [-pkey <private key>] [-nickname <nickname>] [-pass <pass phrase>] [-nssdir <dir>]"
	echo "         $1 import -pem <dir> [-nssdir <dir>] [-notrusted]"
	echo "         $1 import -pkcs12 <file> [-pass <pass phrase>] [-nssdir <dir>]"
	echo ""
	echo "Modes:   init                 initialize nss db files(be careful)"
	echo "         pkcs12               convert pem file and private key file to PKCS#12 file"
	echo "         import               import PKCS#12 certs or PEM CA certs to nssdb"
	echo ""
	echo "Options: -pass <pass phrase>  pass phrase for nssdb or PCKS#12 file"
	echo "         -pem <file | dir>    specify PEM format file or files in directory path"
	echo "         -pkcs12 <file>       specify PCKS#12 cert file"
	echo "         -pkey <private key>  private key file for PEM file"
	echo "         -nickname <nickname> nickname for PCKS#12 cert"
	echo "         -out <filename>      output PKCS#12 file path"
	echo "         -notrusted           add CA certifications as untrusted CA"
	echo "         -nssdir <dir>        specify nssdb directory path(if this is not specified, using SSL_DIR environment)"
	echo ""
	echo "Notes:   If you need to reference/operate NSSDB, you can use the following command."
	echo "         certutil -L -d <nssdb dir>"
	echo "         certutil -L -d sql:<nssdb dir>"
	echo "         certutil -U -d <nssdb dir>"
	echo "         certutil -K -d <nssdb dir>"
	echo "         certutil -L -n <nickname> -d <nssdb dir>"
	echo "         certutil -L -a -n <nickname> -d <nssdb dir>"
	echo "         certutil -D -n <nickname> -d <nssdb dir>"
	echo "         certutil -V -u [V|C|A|Y] -n <nickname> -d <nssdb dir>"
	echo "         certutil -O -n <nickname> -d <nssdb dir>"
	echo ""
}

#--------------------------------------------------------------
# Parse options
#--------------------------------------------------------------
#
# Variables
#
SCRIPT_MODE=""
PEMPATH=""
PKEYFILE=""
PCKS12FILE=""
NSSDB_DIR=""
NICKNAME=""
PASSPHRASE=""
CANOTRUSTED=""

while [ $# -ne 0 ]; do
	if [ -z "$1" ]; then
		break

	elif [ "$1" = "-h" ] || [ "$1" = "-H" ] || [ "$1" = "--help" ] || [ "$1" = "--HELP" ]; then
		func_usage "${PRGNAME}"
		exit 0

	elif [ "$1" = "init" ]; then
		if [ -n "${SCRIPT_MODE}" ]; then
			echo "[ERROR] Already specified init, pkcs12 or import option." 1>&2
			exit 1
		fi
		SCRIPT_MODE="INIT"

	elif [ "$1" = "pkcs12" ]; then
		if [ -n "${SCRIPT_MODE}" ]; then
			echo "[ERROR] Already specified init, pkcs12 or import option." 1>&2
			exit 1
		fi
		SCRIPT_MODE="PKCS12"

	elif [ "$1" = "import" ]; then
		if [ -n "${SCRIPT_MODE}" ]; then
			echo "[ERROR] Already specified init, pkcs12 or import option." 1>&2
			exit 1
		fi
		SCRIPT_MODE="IMPORT"

	elif [ "$1" = "-pem" ]; then
		if [ -n "${PEMPATH}" ]; then
			echo "[ERROR] Already specified -pem option." 1>&2
			exit 1
		fi
		shift
		if [ $# -eq 0 ]; then
			echo "[ERROR] -pem option needs parameter." 1>&2
			exit 1
		fi
		if [ ! -f "$1" ]; then
			if [ ! -d "$1" ]; then
				echo "[ERROR] $1 file does not exist." 1>&2
				exit 1
			fi
		fi
		PEMPATH="$1"

	elif [ "$1" = "-pkey" ]; then
		if [ -n "${PKEYFILE}" ]; then
			echo "[ERROR] Already specified -pkey option." 1>&2
			exit 1
		fi
		shift
		if [ $# -eq 0 ]; then
			echo "[ERROR] -pkey option needs parameter." 1>&2
			exit 1
		fi
		if [ ! -f "$1" ]; then
			echo "[ERROR] $1 file does not exist." 1>&2
			exit 1
		fi
		PKEYFILE="$1"

	elif [ "$1" = "-pkcs12" ]; then
		if [ -n "${PCKS12FILE}" ]; then
			echo "[ERROR] Already specified -pkcs12 option." 1>&2
			exit 1
		fi
		shift
		if [ $# -eq 0 ]; then
			echo "[ERROR] -pkcs12 option needs parameter." 1>&2
			exit 1
		fi
		if [ ! -f "$1" ]; then
			echo "[ERROR] $1 file does not exist." 1>&2
			exit 1
		fi
		PCKS12FILE="$1"

	elif [ "$1" = "-out" ]; then
		if [ -n "${PCKS12FILE}" ]; then
			echo "[ERROR] Already specified -out option." 1>&2
			exit 1
		fi
		shift
		if [ $# -eq 0 ]; then
			echo "[ERROR] -out option needs parameter." 1>&2
			exit 1
		fi
		if [ -f "$1" ]; then
			echo "[ERROR] $1 file exists." 1>&2
			exit 1
		fi
		PCKS12FILE="$1"

	elif [ "$1" = "-nssdir" ]; then
		if [ -n "${NSSDB_DIR}" ]; then
			echo "[ERROR] Already specified -nssdir option." 1>&2
			exit 1
		fi
		shift
		if [ $# -eq 0 ]; then
			echo "[ERROR] -nssidr option needs parameter." 1>&2
			exit 1
		fi
		if [ ! -d "$1" ]; then
			echo "[ERROR] $1 directory does not exist." 1>&2
			exit 1
		fi
		NSSDB_DIR="$1"

	elif [ "$1" = "-nickname" ]; then
		if [ -n "${NICKNAME}" ]; then
			echo "[ERROR] Already specified -nickname option." 1>&2
			exit 1
		fi
		shift
		if [ $# -eq 0 ]; then
			echo "[ERROR] -nickname option needs parameter." 1>&2
			exit 1
		fi
		NICKNAME="$1"

	elif [ "$1" = "-pass" ]; then
		if [ -n "${PASSPHRASE}" ]; then
			echo "[ERROR] Already specified -pass option." 1>&2
			exit 1
		fi
		shift
		if [ $# -eq 0 ]; then
			echo "[ERROR] -pass option needs parameter." 1>&2
			exit 1
		fi
		PASSPHRASE="$1"

	elif [ "$1" = "-notrusted" ]; then
		if [ -n "${CANOTRUSTED}" ]; then
			echo "[ERROR] Already specified -notrusted option." 1>&2
			exit 1
		fi
		CANOTRUSTED="YES"

	else
		echo "[ERROR] Unknown parameter $1" 1>&2
		exit 1
	fi
	shift
done

#
# Check options
#
if [ -z "${SCRIPT_MODE}" ]; then
	echo "[ERROR] Not specify mode(init, pkcs12 or import)." 1>&2
	exit 1

elif [ "${SCRIPT_MODE}" = "INIT" ]; then
	if [ -n "${PEMPATH}" ] || [ -n "${PKEYFILE}" ] || [ -n "${PCKS12FILE}" ] || [ -n "${NICKNAME}" ] || [ -n "${CANOTRUSTED}" ]; then
		echo "[ERROR] Unnecessary option is specified for \"init\" mode." 1>&2
		exit 1
	fi
	if [ -z "${NSSDB_DIR}" ]; then
		NSSDB_DIR="${SSL_DIR}"
		if [ -z "${NSSDB_DIR}" ]; then
			echo "[ERROR] not specify \"-nssdir\" option and \"SSL_DIR\" environment is not specified too." 1>&2
			exit 1
		fi
		if [ ! -d "${NSSDB_DIR}" ]; then
			echo "[ERROR] nssdb directory ${NSSDB_DIR} is not directory." 1>&2
			exit 1
		fi
	fi

elif [ "${SCRIPT_MODE}" = "PKCS12" ]; then
	if [ -n "${NSSDB_DIR}" ] || [ -n "${CANOTRUSTED}" ]; then
		echo "[ERROR] Unnecessary option is specified for \"pkcs12\" mode." 1>&2
		exit 1
	fi
	if [ -z "${PEMPATH}" ]; then
		echo "[ERROR] \"-pem\" option is not specified." 1>&2
		exit 1
	fi
	if [ ! -f "${PEMPATH}" ]; then
		echo "[ERROR] ${PEMPATH} file dose not exist." 1>&2
		exit 1
	fi
	if [ -z "${NICKNAME}" ]; then
		NICKNAME=$(basename "${PEMPATH}" | sed -e 's/\.[0-9a-zA-Z]*$//g')
	fi
	if [ -z "${PCKS12FILE}" ]; then
		PCKS12FILE=$(basename "${PEMPATH}" | sed -e 's/\.[0-9a-zA-Z]*$//g')
		PCKS12FILE="${PCKS12FILE}.p12"
	fi
	if [ -f "${PCKS12FILE}" ]; then
		echo "[ERROR] ${PCKS12FILE} file exists." 1>&2
		exit 1
	fi

elif [ "${SCRIPT_MODE}" = "IMPORT" ]; then
	if [ -n "${PEMPATH}" ]; then
		if [ -n "${PCKS12FILE}" ]; then
			echo "[ERROR] \"-pem\" or \"-pkcs12\" options can not be specified at the same time." 1>&2
			exit 1
		fi

		if [ -f "${PEMPATH}" ]; then
			if [ -n "${CANOTRUSTED}" ]; then
				echo "[ERROR] Unnecessary option is specified for \"import\" mode with PEM file." 1>&2
				exit 1
			fi
			if [ -z "${NICKNAME}" ]; then
				NICKNAME=$(basename "${PEMPATH}" | sed -e 's/\.[0-9a-zA-Z]*$//g')
			fi
			SCRIPT_MODE="IMPORT_PEMFILE"

		elif [ -d "${PEMPATH}" ]; then
			if [ -n "${PKEYFILE}" ] || [ -n "${NICKNAME}" ] || [ -n "${PASSPHRASE}" ]; then
				echo "[ERROR] Unnecessary option is specified for \"import\" mode with PEM directory." 1>&2
				exit 1
			fi
			SCRIPT_MODE="IMPORT_PEMDIR"

		else
			echo "[ERROR] ${PEMPATH} is not file nor directory." 1>&2
			exit 1
		fi

	elif [ -n "${PCKS12FILE}" ]; then
		if [ -n "${PKEYFILE}" ] || [ -n "${NICKNAME}" ] || [ -n "${CANOTRUSTED}" ]; then
			echo "[ERROR] Unnecessary option is specified for \"import\" mode with PKCS#12 file." 1>&2
			exit 1
		fi
		if [ ! -f "${PCKS12FILE}" ]; then
			echo "[ERROR] ${PCKS12FILE} file does not exists." 1>&2
			exit 1
		fi
		SCRIPT_MODE="IMPORT_PKCS12"

	else
		echo "[ERROR] For \"import\" mode, you need to specify \"-pem\" or \"-pkcs12\" option." 1>&2
		exit 1
	fi

	if [ -z "${NSSDB_DIR}" ]; then
		NSSDB_DIR="${SSL_DIR}"
		if [ -z "${NSSDB_DIR}" ]; then
			echo "[ERROR] Not specify \"-nssdir\" option and \"SSL_DIR\" environment is not specified too." 1>&2
			exit 1
		fi
		if [ ! -d "${NSSDB_DIR}" ]; then
			echo "[ERROR] nssdb directory ${NSSDB_DIR} is not directory." 1>&2
			exit 1
		fi
	fi
else
	echo "[ERROR] Internal error(unknown mode is set)." 1>&2
	exit 1
fi

#
# Check programs
#
if [ "${SCRIPT_MODE}" = "INIT" ] || [ "${SCRIPT_MODE}" = "IMPORT_PEMFILE" ] || [ "${SCRIPT_MODE}" = "IMPORT_PEMDIR" ]; then
	if ! command -v  certutil >/dev/null 2>&1; then
		echo "[ERROR] Not found \"certutil\" program, you can need to install nss util package." 1>&2
		exit 1
	fi

elif [ "${SCRIPT_MODE}" = "PKCS12" ]; then
	if ! command -v openssl >/dev/null 2>&1; then
		echo "[ERROR] Not found \"openssl\" program, you can need to install openssl util package." 1>&2
		exit 1
	fi

elif [ "${SCRIPT_MODE}" = "IMPORT_PKCS12" ]; then
	if ! command -v pk12util >/dev/null 2>&1; then
		echo "[ERROR] Not found \"pk12util\" program, you can need to install nss util package." 1>&2
		exit 1
	fi
fi

#==============================================================
# Main
#==============================================================
if [ "${SCRIPT_MODE}" = "INIT" ]; then
	if stat "${NSSDB_DIR}"/cert*.db >/dev/null 2>&1; then
		rm -f "${NSSDB_DIR}"/cert*.db
	fi
	if stat "${NSSDB_DIR}"/key*.db >/dev/null 2>&1; then
		rm -f "${NSSDB_DIR}"/key*.db
	fi
	if stat "${NSSDB_DIR}"/secmod*.db >/dev/null 2>&1; then
		rm -f "${NSSDB_DIR}"/secmod*.db
	fi

	if [ -z "${PASSPHRASE}" ]; then
		if ! certutil -N --empty-password -d "${NSSDB_DIR}"; then
			echo "[ERROR] Failed to initialize nssdb with passphrase." 1>&2
			rm -f "${PRGNAME}.$$.tmp"
			exit 1
		fi
	else
		echo "${PASSPHRASE}" > "${PRGNAME}.$$.tmp"
		if ! certutil -N -f "${PRGNAME}.$$.tmp" -d "${NSSDB_DIR}"; then
			echo "[ERROR] Failed to initialize nssdb with passphrase." 1>&2
			rm -f "${PRGNAME}.$$.tmp"
			exit 1
		fi
	fi
	rm -f "${PRGNAME}.$$.tmp"

	if ! chmod +r "${NSSDB_DIR}"/*.db; then
		echo "[ERROR] Failed to change permission to ${NSSDB_DIR}/*.db files." 1>&2
		exit 1
	fi
	echo "[SUCCEED] Initialized nssdb with passphrase."

elif [ "${SCRIPT_MODE}" = "PKCS12" ]; then
	if [ -z "${PKEYFILE}" ]; then
		if ! openssl pkcs12 -export -in "${PEMPATH}" -name "${NICKNAME}" -passout pass:"${PASSPHRASE}" -out "${PCKS12FILE}"; then
			echo "[ERROR] Failed to convert PEM file(with private key file) to PKCS#12 file." 1>&2
			exit 1
		fi
	else
		if ! openssl pkcs12 -export -in "${PEMPATH}" -inkey "${PKEYFILE}" -name "${NICKNAME}" -passout pass:"${PASSPHRASE}" -out "${PCKS12FILE}"; then
			echo "[ERROR] Failed to convert PEM file(with private key file) to PKCS#12 file." 1>&2
			exit 1
		fi
	fi
	echo "[SUCCEED] Converted PEM file(with private key file) to PKCS#12 file."

elif [ "${SCRIPT_MODE}" = "IMPORT_PEMFILE" ]; then
	if [ -z "${PKEYFILE}" ]; then
		if ! openssl pkcs12 -export -in "${PEMPATH}" -name "${NICKNAME}" -passout pass:"${PASSPHRASE}" -out "${PRGNAME}.$$.tmp.p12"; then
			echo "[ERROR] Failed to convert PEM file(with private key file) to PKCS#12 file." 1>&2
			rm -f "${PRGNAME}.$$.tmp.p12"
			exit 1
		fi
	else
		if ! openssl pkcs12 -export -in "${PEMPATH}" -inkey "${PKEYFILE}" -name "${NICKNAME}" -passout pass:"${PASSPHRASE}" -out "${PRGNAME}.$$.tmp.p12"; then
			echo "[ERROR] Failed to convert PEM file(with private key file) to PKCS#12 file." 1>&2
			rm -f "${PRGNAME}.$$.tmp.p12"
			exit 1
		fi
	fi

	if ! pk12util -i "${PRGNAME}.$$.tmp.p12" -d "${NSSDB_DIR}" -W "${PASSPHRASE}"; then
		echo "[ERROR] Failed to import PKCS#12 file converted from PEM file(with private key file)" 1>&2
		rm -f "${PRGNAME}.$$.tmp.p12"
		exit 1
	fi
	rm -f "${PRGNAME}.$$.tmp.p12"

	echo "[SUCCEED] Imported PKCS#12 file converted from PEM file(with private key file)."

elif [ "${SCRIPT_MODE}" = "IMPORT_PEMDIR" ]; then
	TOTAL=0
	COUNT=0
	CATRUSTED="T"
	if [ -n "${CANOTRUSTED}" ] && [ "${CANOTRUSTED}" = "YES" ]; then
		CATRUSTED=""
	fi

	for PEM_FILEPATH in "${PEMPATH}"/*.pem; do
		PEM_FILENAME=$(basename "${PEM_FILEPATH}")
		NICKNAME=$(echo "${PEM_FILENAME}" | sed -e 's/\.pem$//g' 2>/dev/null)

		if certutil -A -n "${NICKNAME}" -t "C${CATRUSTED},," -i "${PEMPATH}/${PEM_FILENAME}" -d "${NSSDB_DIR}"; then
			echo "Importing ${NICKNAME} ... done"
			COUNT=$((COUNT + 1))
		else
			echo "Importing ${NICKNAME} ... failed"
		fi
		TOTAL=$((TOTAL + 1))
	done

	echo "[FINISH] Imported PEM file(${COUNT}) / total file(${TOTAL})"

elif [ "${SCRIPT_MODE}" = "IMPORT_PKCS12" ]; then
	if ! pk12util -i "${PCKS12FILE}" -d "${NSSDB_DIR}" -W "${PASSPHRASE}"; then
		echo "[ERROR] failed to import PKCS#12 ${PCKS12FILE} file." 1>&2
		exit 1
	fi

	echo "[SUCCEED] import PKCS#12 file."
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
