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

#
# Utility for CHMPX with NSS
#
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

#
# Global
#
PRGNAME=`basename $0`
MYSCRIPTDIR=`dirname $0`

#
# Load options
#
MODE=""
PEMPATH=""
PKEYFILE=""
PCKS12FILE=""
NSSDB_DIR=""
NICKNAME=""
PASSPHRASE=""
CANOTRUSTED=""

while [ $# -ne 0 ]; do
	if [ "X$1" = "X" ]; then
		break

	elif [ "X$1" = "X-h" -o "X$1" = "X-help" ]; then
		func_usage $PRGNAME
		exit 0

	elif [ "X$1" = "Xinit" ]; then
		if [ "X${MODE}" != "X" ]; then
			echo "ERROR: already specified mode ${MODE}." 1>&2
			exit 1
		fi
		MODE="INIT"

	elif [ "X$1" = "Xpkcs12" ]; then
		if [ "X${MODE}" != "X" ]; then
			echo "ERROR: already specified mode ${MODE}." 1>&2
			exit 1
		fi
		MODE="PKCS12"

	elif [ "X$1" = "Ximport" ]; then
		if [ "X${MODE}" != "X" ]; then
			echo "ERROR: already specified mode ${MODE}." 1>&2
			exit 1
		fi
		MODE="IMPORT"

	elif [ "X$1" = "X-pem" ]; then
		if [ "X${PEMPATH}" != "X" ]; then
			echo "ERROR: already specified PEM file path." 1>&2
			exit 1
		fi
		shift
		if [ $# -eq 0 ]; then
			echo "ERROR: -pem option needs parameter." 1>&2
			exit 1
		fi
		if [ ! -f $1 ]; then
			if [ ! -d $1 ]; then
				echo "ERROR: $1 file does not exist." 1>&2
				exit 1
			fi
		fi
		PEMPATH=$1

	elif [ "X$1" = "X-pkey" ]; then
		if [ "X${PKEYFILE}" != "X" ]; then
			echo "ERROR: already specified private key file path." 1>&2
			exit 1
		fi
		shift
		if [ $# -eq 0 ]; then
			echo "ERROR: -pkey option needs parameter." 1>&2
			exit 1
		fi
		if [ ! -f $1 ]; then
			echo "ERROR: $1 file does not exist." 1>&2
			exit 1
		fi
		PKEYFILE=$1

	elif [ "X$1" = "X-pkcs12" ]; then
		if [ "X${PCKS12FILE}" != "X" ]; then
			echo "ERROR: already specified pkcs#12 file path." 1>&2
			exit 1
		fi
		shift
		if [ $# -eq 0 ]; then
			echo "ERROR: -pkcs12 option needs parameter." 1>&2
			exit 1
		fi
		if [ ! -f $1 ]; then
			echo "ERROR: $1 file does not exist." 1>&2
			exit 1
		fi
		PCKS12FILE=$1

	elif [ "X$1" = "X-out" ]; then
		if [ "X${PCKS12FILE}" != "X" ]; then
			echo "ERROR: already specified output file path." 1>&2
			exit 1
		fi
		shift
		if [ $# -eq 0 ]; then
			echo "ERROR: -out option needs parameter." 1>&2
			exit 1
		fi
		if [ -f $1 ]; then
			echo "ERROR: $1 file exists." 1>&2
			exit 1
		fi
		PCKS12FILE=$1

	elif [ "X$1" = "X-nssdir" ]; then
		if [ "X${NSSDB_DIR}" != "X" ]; then
			echo "ERROR: already specified NSSDB directory path." 1>&2
			exit 1
		fi
		shift
		if [ $# -eq 0 ]; then
			echo "ERROR: -nssidr option needs parameter." 1>&2
			exit 1
		fi
		if [ ! -d $1 ]; then
			echo "ERROR: $1 directory does not exist." 1>&2
			exit 1
		fi
		NSSDB_DIR=$1

	elif [ "X$1" = "X-nickname" ]; then
		if [ "X${NICKNAME}" != "X" ]; then
			echo "ERROR: already specified nickname." 1>&2
			exit 1
		fi
		shift
		if [ $# -eq 0 ]; then
			echo "ERROR: -nickname option needs parameter." 1>&2
			exit 1
		fi
		NICKNAME=$1

	elif [ "X$1" = "X-pass" ]; then
		if [ "X${PASSPHRASE}" != "X" ]; then
			echo "ERROR: already specified passphrase." 1>&2
			exit 1
		fi
		shift
		if [ $# -eq 0 ]; then
			echo "ERROR: -pass option needs parameter." 1>&2
			exit 1
		fi
		PASSPHRASE=$1

	elif [ "X$1" = "X-notrusted" ]; then
		if [ "X${CANOTRUSTED}" != "X" ]; then
			echo "ERROR: already specified no CA trusted flag." 1>&2
			exit 1
		fi
		CANOTRUSTED="YES"

	else
		echo "ERROR: unknown parameter $1" 1>&2
		exit 1
	fi
	shift
done

#
# Check options
#
if [ "X${MODE}" = "XINIT" ]; then
	if [ "X${PEMPATH}" != "X" -o "X${PKEYFILE}" != "X" -o "X${PCKS12FILE}" != "X" -o "X${NICKNAME}" != "X" -o "X${CANOTRUSTED}" != "X" ]; then
		echo "ERROR: unnecessary option is specified for \"init\" mode." 1>&2
		exit 1
	fi
	if [ "X${NSSDB_DIR}" = "X" ]; then
		NSSDB_DIR=${SSL_DIR}
		if [ "X${NSSDB_DIR}" = "X" ]; then
			echo "ERROR: not specify \"-nssdir\" option and \"SSL_DIR\" environment is not specified too." 1>&2
			exit 1
		fi
		if [ ! -d ${NSSDB_DIR} ]; then
			echo "ERROR: nssdb directory ${NSSDB_DIR} is not directory." 1>&2
			exit 1
		fi
	fi

elif [ "X${MODE}" = "XPKCS12" ]; then
	if [ "X${NSSDB_DIR}" != "X" -o "X${CANOTRUSTED}" != "X" ]; then
		echo "ERROR: unnecessary option is specified for \"pkcs12\" mode." 1>&2
		exit 1
	fi
	if [ "X${PEMPATH}" = "X" ]; then
		echo "ERROR: \"-pem\" option is not specified." 1>&2
		exit 1
	fi
	if [ ! -f ${PEMPATH} ]; then
		echo "ERROR: ${PEMPATH} file dose not exist." 1>&2
		exit 1
	fi
	if [ "X${NICKNAME}" = "X" ]; then
		NICKNAME=`basename ${PEMPATH} 2>/dev/null | sed 's/\.[0-9a-zA-Z]*$//g' 2>/dev/null`
	fi
	if [ "X${PCKS12FILE}" = "X" ]; then
		PCKS12FILE=`basename ${PEMPATH} 2>/dev/null | sed 's/\.[0-9a-zA-Z]*$//g' 2>/dev/null`
		PCKS12FILE=${PCKS12FILE}.p12
	fi
	if [ -f ${PCKS12FILE} ]; then
		echo "ERROR: ${PCKS12FILE} file exists." 1>&2
		exit 1
	fi

elif [ "X${MODE}" = "XIMPORT" ]; then
	if [ "X${PEMPATH}" != "X" ]; then
		if [ "X${PCKS12FILE}" != "X" ]; then
			echo "ERROR: \"-pem\" or \"-pkcs12\" options can not be specified at the same time." 1>&2
			exit 1
		fi
		if [ -f ${PEMPATH} ]; then
			if [ "X${CANOTRUSTED}" != "X" ]; then
				echo "ERROR: unnecessary option is specified for \"import\" mode with PEM file." 1>&2
				exit 1
			fi
			if [ "X${NICKNAME}" = "X" ]; then
				NICKNAME=`basename ${PEMPATH} 2>/dev/null | sed 's/\.[0-9a-zA-Z]*$//g' 2>/dev/null`
			fi
			MODE="IMPORT_PEMFILE"

		elif [ -d ${PEMPATH} ]; then
			if [ "X${PKEYFILE}" != "X" -o "X${NICKNAME}" != "X" -o "X${PASSPHRASE}" != "X" ]; then
				echo "ERROR: unnecessary option is specified for \"import\" mode with PEM directory." 1>&2
				exit 1
			fi
			MODE="IMPORT_PEMDIR"

		else
			echo "ERROR: ${PEMPATH} is not file nor directory." 1>&2
			exit 1
		fi

	elif [ "X${PCKS12FILE}" != "X" ]; then
		if [ "X${PKEYFILE}" != "X" -o "X${NICKNAME}" != "X" -o "X${CANOTRUSTED}" != "X" ]; then
			echo "ERROR: unnecessary option is specified for \"import\" mode with PKCS#12 file." 1>&2
			exit 1
		fi
		if [ ! -f ${PCKS12FILE} ]; then
			echo "ERROR: ${PCKS12FILE} file does not exists." 1>&2
			exit 1
		fi
		MODE="IMPORT_PKCS12"

	else
		echo "ERROR: for \"import\" mode, you need to specify \"-pem\" or \"-pkcs12\" option." 1>&2
		exit 1
	fi

	if [ "X${NSSDB_DIR}" = "X" ]; then
		NSSDB_DIR=${SSL_DIR}
		if [ "X${NSSDB_DIR}" = "X" ]; then
			echo "ERROR: not specify \"-nssdir\" option and \"SSL_DIR\" environment is not specified too." 1>&2
			exit 1
		fi
		if [ ! -d ${NSSDB_DIR} ]; then
			echo "ERROR: nssdb directory ${NSSDB_DIR} is not directory." 1>&2
			exit 1
		fi
	fi

elif [ "X${MODE}" = "X" ]; then
	echo "ERROR: not specify mode(init/pkcs12/import)." 1>&2
	exit 1
else
	echo "ERROR: internal error(unknown mode is set)." 1>&2
	exit 1
fi

#
# Check programs
#
if [ "X${MODE}" = "XINIT" -o "X${MODE}" = "XIMPORT_PEMFILE" -o "X${MODE}" = "XIMPORT_PEMDIR" ]; then
	if ! command -v  certutil >/dev/null 2>&1; then
		echo "ERROR: not found \"certutil\" program, you can need to install nss util package." 1>&2
		exit 1
	fi

elif [ "X${MODE}" = "XPKCS12" ]; then
	if ! command -v openssl >/dev/null 2>&1; then
		echo "ERROR: not found \"openssl\" program, you can need to install openssl util package." 1>&2
		exit 1
	fi

elif [ "X${MODE}" = "XIMPORT_PKCS12" ]; then
	if ! command -v pk12util >/dev/null 2>&1; then
		echo "ERROR: not found \"pk12util\" program, you can need to install nss util package." 1>&2
		exit 1
	fi
fi

#
# Run
#
if [ "X${MODE}" = "XINIT" ]; then
	ls ${NSSDB_DIR}/cert*.db >/dev/null 2>&1
	if [ $? -eq 0 ]; then
		rm ${NSSDB_DIR}/cert*.db
	fi
	ls ${NSSDB_DIR}/key*.db >/dev/null 2>&1
	if [ $? -eq 0 ]; then
		rm ${NSSDB_DIR}/key*.db
	fi
	ls ${NSSDB_DIR}/secmod*.db >/dev/null 2>&1
	if [ $? -eq 0 ]; then
		rm ${NSSDB_DIR}/secmod*.db
	fi

	if [ "X${PASSPHRASE}" = "X" ]; then
		certutil -N --empty-password -d ${NSSDB_DIR}
	else
		echo ${PASSPHRASE} > ${PRGNAME}.$$.tmp
		certutil -N -f ${PRGNAME}.$$.tmp -d ${NSSDB_DIR}
	fi
	if [ $? -ne 0 ]; then
		echo "ERROR: failed to initialize nssdb with passphrase." 1>&2
		rm -f ${PRGNAME}.$$.tmp
		exit 1
	fi
	rm -f ${PRGNAME}.$$.tmp

	chmod +r ${NSSDB_DIR}/*.db

	echo "SUCCEED: initialize nssdb with passphrase."

elif [ "X${MODE}" = "XPKCS12" ]; then
	if [ "X${PKEYFILE}" = "X" ]; then
		openssl pkcs12 -export -in ${PEMPATH} -name "${NICKNAME}" -passout pass:${PASSPHRASE} -out ${PCKS12FILE}
	else
		openssl pkcs12 -export -in ${PEMPATH} -inkey ${PKEYFILE} -name "${NICKNAME}" -passout pass:${PASSPHRASE} -out ${PCKS12FILE}
	fi
	if [ $? -ne 0 ]; then
		echo "ERROR: failed to convert PEM file(with private key file) to PKCS#12 file." 1>&2
		exit 1
	fi

	echo "SUCCEED: convert PEM file(with private key file) to PKCS#12 file."

elif [ "X${MODE}" = "XIMPORT_PEMFILE" ]; then
	if [ "X${PKEYFILE}" = "X" ]; then
		openssl pkcs12 -export -in ${PEMPATH} -name "${NICKNAME}" -passout pass:${PASSPHRASE} -out ${PRGNAME}.$$.tmp.p12
	else
		openssl pkcs12 -export -in ${PEMPATH} -inkey ${PKEYFILE} -name "${NICKNAME}" -passout pass:${PASSPHRASE} -out ${PRGNAME}.$$.tmp.p12
	fi
	if [ $? -ne 0 ]; then
		echo "ERROR: failed to convert PEM file(with private key file) to PKCS#12 file." 1>&2
		rm -f ${PRGNAME}.$$.tmp.p12
		exit 1
	fi

	pk12util -i ${PRGNAME}.$$.tmp.p12 -d ${NSSDB_DIR} -W "${PASSPHRASE}"
	if [ $? -ne 0 ]; then
		echo "ERROR: failed to import PKCS#12 file converted from PEM file(with private key file)" 1>&2
		rm -f ${PRGNAME}.$$.tmp.p12
		exit 1
	fi
	rm -f ${PRGNAME}.$$.tmp.p12

	echo "SUCCEED: import PKCS#12 file converted from PEM file(with private key file)."

elif [ "X${MODE}" = "XIMPORT_PEMDIR" ]; then
	TOTAL=0
	COUNT=0
	PEMFILES=`cd ${PEMPATH}; ls -1 *.pem 2>/dev/null`
	CATRUSTED="T"
	if [ "X${CANOTRUSTED}" = "XYES" ]; then
		CATRUSTED=""
	fi

	for pemfile in ${PEMFILES}; do
		NICKNAME=`echo ${pemfile} | sed 's/\.pem$//g' 2>/dev/null`

		certutil -A -n "${NICKNAME}" -t "C${CATRUSTED},," -i ${PEMPATH}/${pemfile} -d ${NSSDB_DIR}
		if [ $? -eq 0 ]; then
			echo "Importing ${NICKNAME} ... done"
			COUNT=`expr ${COUNT} + 1`
		else
			echo "Importing ${NICKNAME} ... failed"
		fi
		TOTAL=`expr ${TOTAL} + 1`
	done
	echo "FINISH: Imported PEM file(${COUNT}) / total file(${TOTAL})"

elif [ "X${MODE}" = "XIMPORT_PKCS12" ]; then
	pk12util -i ${PCKS12FILE} -d ${NSSDB_DIR} -W "${PASSPHRASE}"
	if [ $? -ne 0 ]; then
		echo "ERROR: failed to import PKCS#12 ${PCKS12FILE} file." 1>&2
		exit 1
	fi

	echo "SUCCEED: import PKCS#12 file."
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
