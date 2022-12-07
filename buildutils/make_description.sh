#!/bin/sh
#
# Helper Script for Systemd Service based by AntPickax
#
# Copyright 2020 Yahoo Japan Corporation.
#
# AntPickax provides base utility script for systemd service.
# The script for each component is created based on the
# helper scripts provided by AntPickax for building systemd
# services.
# By changing some variables and functions of the script,
# it corresponds to the script of each component.
# 
# For the full copyright and license information, please view
# the license file that was distributed with this source code.
#
# AUTHOR:   Takeshi Nakatani
# CREATE:   Fri, Dec 18 2020
# REVISION:
#

#---------------------------------------------------------------------
# Common variables
#---------------------------------------------------------------------
# [NOTE]
# The script name is important, it specifies the following format.
#    <sub process name>-service-helper
# This name determines the PID file name and log file name.
# If you want to change these names, customize the assignment formulas
# and values of the following variables.
#

#--------------------------------------------------------------
# Common Variables
#--------------------------------------------------------------
PRGNAME=$(basename "${0}")
SCRIPTDIR=$(dirname "${0}")
SCRIPTDIR=$(cd "${SCRIPTDIR}" || exit 1; pwd)
#SRCTOP=$(cd "${SCRIPTDIR}/../.." || exit 1; pwd)

#
# Create variables
#
BASE_PRGNAME=$(echo "${PRGNAME}" | sed -e 's/\./ /g' | awk '{print $1}')
BASE_SUBPROCNAME=$(echo "${BASE_PRGNAME}" | sed -e 's/-/ /g' | awk '{print $1}')
BASE_UPPER_SUBPROCNAME=$(echo "${BASE_SUBPROCNAME}" | tr '[:lower:]' '[:upper:]')

#--------------------------------------------------------------
# Variables
#--------------------------------------------------------------
CAUGHT_SIGNAL=0

#
# Default configuration values
#
SCRIPT_DIR_PART_NAME="antpickax"

CONFDIR="/etc/${SCRIPT_DIR_PART_NAME}"
SERVICE_CONF_FILE="${BASE_PRGNAME}.conf"
OVERRIDE_FILE="override.conf"

INI_CONF_FILE="${CONFDIR}/${BASE_SUBPROCNAME}.ini"
INI_CONF_FILE_KEYWORD="${BASE_UPPER_SUBPROCNAME}_INI_CONF_FILE"

PIDDIR="/var/run/${SCRIPT_DIR_PART_NAME}"
SERVICE_PIDFILE="${BASE_PRGNAME}.pid"
SUBPROCESS_PIDFILE="${BASE_SUBPROCNAME}.pid"
SUBPROCESS_BIN="${BASE_SUBPROCNAME}"
SUBPROCESS_USER=$(id -un | tr -d '\n')
SUBPROCESS_OPTIONS=""
BEFORE_RUN_SUBPROCESS=""

WAIT_DEPENDPROC_PIDFILE=""
WAIT_SEC_AFTER_DEPENDPROC_UP=15
WAIT_SEC_STARTUP=10
WAIT_SEC_AFTER_SUBPROCESS_UP=15
INTERVAL_SEC_FOR_LOOP=10

TRYCOUNT_STOP_SUBPROC=10

# [NOTE]
# The following are the variables used to specify this helper script
# and the log file for the subprocess.
# Normally, the log is left to jounald or syslog related to systemd,
# so it is not specified.
# If you want to collect logs yourself, set the values in the following
# variables. For reference, it can be set as follows:
#
#	LOGDIR="/var/log/${SCRIPT_DIR_PART_NAME}"
#	SERVICE_LOGFILE="${BASE_PRGNAME}.log"
#	SUBPROCESS_LOGFILE="${BASE_SUBPROCNAME}.log"
#
# These values can be specified in configuration file or override.conf.
#
LOGDIR=""
SERVICE_LOGFILE=""
SUBPROCESS_LOGFILE=""

#---------------------------------------------------------------------
# Utilitiy functions
#---------------------------------------------------------------------
#
# Usage
#
func_usage()
{
	echo ""
	echo "Usage:  $1 {-h | --help} {start | stop}"
	echo ""
}

#
# Message
#
# $1 	:	1(stdout) or 2(stderr)
# $2 	:	Level
# $3... :	Messages
#
print_message()
{
	if [ -n "$1" ] && [ "$1" -eq 2 ]; then
		_PRINT_STDERR=1
	else
		_PRINT_STDERR=0
	fi
	if [ -z "$2" ]; then
		_PRINT_LEVEL="ERROR"
	else
		_PRINT_LEVEL=$2
	fi
	_PRINT_DATE=$(date '+%FT%T%z')
	shift
	shift

	if [ -n "${LOGDIR}" ] && [ -n "${SERVICE_LOGFILE}" ]; then
		echo "${_PRINT_DATE} ${PRGNAME} [${_PRINT_LEVEL}] $*" >> "${LOGDIR}/${SERVICE_LOGFILE}"
	else
		if [ "${_PRINT_STDERR}" -eq 1 ]; then
			echo "${_PRINT_DATE} ${PRGNAME} [${_PRINT_LEVEL}] $*" 1>&2
		else
			echo "${_PRINT_DATE} ${PRGNAME} [${_PRINT_LEVEL}] $*"
		fi
	fi
}

log_err()
{
	print_message 1 "ERROR" "$*"
}

log_warn()
{
	print_message 1 "WARNING" "$*"
}

log_info()
{
	print_message 1 "INFO" "$*"
}

prn_err()
{
	print_message 2 "ERROR" "$*"
}

prn_warn()
{
	print_message 2 "WARNING" "$*"
}

prn_info()
{
	print_message 2 "INFO" "$*"
}

#
# Check file
#
# $1 :	File name/path
#
is_safe_file()
{
	if [ $# -eq 0 ]; then
		echo "There are no arguments(is_safe_file)"
		return 1
	fi
	if [ -z "$1" ] || [ ! -f "$1" ]; then
		echo "$1 is not safe file."
		return 1
	fi
	return 0
}

#
# Get value by key name from file
#
# $1 :	Key name
# $2 :	File name/path
#
extract_value()
{
	#
	# Check parameters
	#
	if [ $# -lt 2 ]; then
		echo "Not enough arguments - $*"
		return 1
	fi

	_EXTRACTVALUE_KEY="$1"
	_EXTRACTVALUE_FILE="$2"
	if ! _ERROR_MSG=$(is_safe_file "${_EXTRACTVALUE_FILE}"); then
		echo "${_ERROR_MSG}"
		return 1
	fi

	#
	# Get latest matching line
	#
	_EXTRACTVALUE_LINE=$(grep -v "^[[:space:]]*#" "${_EXTRACTVALUE_FILE}" | grep "^[[:space:]]*${_EXTRACTVALUE_KEY}[[:space:]]*=" | tail -1)
	if [ -z "${_EXTRACTVALUE_LINE}" ]; then
		echo ""
		return 0
	fi

	#
	# Get value(trimed head/tail spaces)
	#
	_EXTRACTVALUE_VALUE=$(echo "${_EXTRACTVALUE_LINE}" | sed -e "s/^[[:space:]]*${_EXTRACTVALUE_KEY}[[:space:]]*=//g" -e "s/^[[:space:]]*//g" -e "s/[[:space:]]*$//g")

	echo "${_EXTRACTVALUE_VALUE}"
	return 0
}

#
# Get value by key name from file
#
# $1 :	Key name
# $2 :	File name/path
# $3 :	Override file name/path(allow empty)
#
get_value()
{
	#
	# Check parameters
	#
	if [ $# -lt 2 ]; then
		echo "Not enough arguments - $*"
		return 1
	fi

	_GETVALUE_KEY="$1"
	_GETVALUE_FILE="$2"
	if ! _ERROR_MSG=$(is_safe_file "${_GETVALUE_FILE}"); then
		echo "${_ERROR_MSG}"
		return 1
	fi

	if [ $# -ge 3 ]; then
		_GETVALUE_OVERRIDE_FILE="$3"
		if ! _ERROR_MSG=$(is_safe_file "${_GETVALUE_OVERRIDE_FILE}"); then
			#
			# Skip this file becase the file is optinal file
			#
			prn_warn "${_ERROR_MSG}"
			_GETVALUE_OVERRIDE_FILE=""
		fi
	else
		_GETVALUE_OVERRIDE_FILE=""
	fi

	#
	# Check override file and switch file and key
	#
	# [NOTE]
	# The override file is used to replace the file and key with different values
	# in the following formats(file and key pair):
	#
	# <file name>:<key name> = <replace file name/path>:<replace key name>
	#
	if [ -n "${_GETVALUE_OVERRIDE_FILE}" ]; then
		#
		# Make key for override file
		#
		_GETVALUE_FILE_NAME=$(basename "${_GETVALUE_FILE}")
		_GETVALUE_OVERRIDE_PAIRKEY="${_GETVALUE_FILE_NAME}:${_GETVALUE_KEY}"

		#
		# Search key in override configutation file
		#
		if _GETVALUE_OVERRIDE_VALUE=$(extract_value "${_GETVALUE_OVERRIDE_PAIRKEY}" "${_GETVALUE_OVERRIDE_FILE}"); then
			#
			# Found key and parse it
			#
			# [NOTE]
			# The value is "<replace file/path>:<replace key name>" or "value"
			#
			if ! echo "${_GETVALUE_OVERRIDE_VALUE}" | grep -q ':'; then
				#
				# Case : "value" directly
				#
				echo "${_GETVALUE_OVERRIDE_VALUE}"
				return 0

			else
				#
				# Case : "<replace file/path>:<replace key name>"
				#
				_GETVALUE_REPLACE_FILE=$(echo "${_GETVALUE_OVERRIDE_VALUE}" | sed -e 's/:/ /g' | awk '{print $1}')
				_GETVALUE_REPLACE_KEY=$(echo "${_GETVALUE_OVERRIDE_VALUE}" | sed -e 's/:/ /g' | awk '{print $2}')

				if [ -n "${_GETVALUE_REPLACE_FILE}" ] && [ -n "${_GETVALUE_REPLACE_KEY}" ]; then
					#
					# Check override configuration file
					#
					if ! echo "${_GETVALUE_REPLACE_FILE}" | grep -q '^/'; then
						#
						# file is relative file path, convert to absolute path
						#
						_GETVALUE_TMP_FILENAME=$(basename "${_GETVALUE_REPLACE_FILE}")
						_GETVALUE_TMP_RELATIVE=$(dirname "${_GETVALUE_REPLACE_FILE}")
						_GETVALUE_TMP_CURRENT=$(dirname "${_GETVALUE_OVERRIDE_FILE}")
						_GETVALUE_TMP_ABSOLUTE=$(cd "${_GETVALUE_TMP_CURRENT}/${_GETVALUE_TMP_RELATIVE}" || exit 1; pwd)
						_GETVALUE_REPLACE_FILE="${_GETVALUE_TMP_ABSOLUTE}/${_GETVALUE_TMP_FILENAME}"
					fi
					if [ -f "${_GETVALUE_REPLACE_FILE}" ]; then
						#
						# file is safe, switch file and key
						#
						_GETVALUE_KEY="${_GETVALUE_REPLACE_KEY}"
						_GETVALUE_FILE="${_GETVALUE_REPLACE_FILE}"
					fi
				fi
			fi
		fi
	fi

	#
	# Search key in configuration file
	#
	if ! _GETVALUE_VALUE=$(extract_value "${_GETVALUE_KEY}" "${_GETVALUE_FILE}"); then
		echo "Not found ${_GETVALUE_KEY} in ${_GETVALUE_FILE}"
		return 1
	fi

	#
	# Found key and return value
	#
	echo "${_GETVALUE_VALUE}"
	return 0
}

#---------------------------------------------------------------------
# Utilitiy control functions
#---------------------------------------------------------------------
#
# Check file exists and wait
#
# $1 :	file
# $2 :	interval second for loop
#
wait_file()
{
	#
	# Check parameters
	#
	if [ $# -ne 2 ]; then
		echo "Argument is wrong - $*"
		return 1
	fi
	_WAITFILE_FILE="$1"
	_WAITFILE_INTERVAL="$2"

	#
	# Loop
	#
	while [ ! -f "${_WAITFILE_FILE}" ]; do
		prn_info "${_WAITFILE_FILE} file is not exsted, thus wait to create it."
		sleep "${_WAITFILE_INTERVAL}"
	done

	if [ ! -f "${_WAITFILE_FILE}" ]; then
		echo "Something is wrong after waiting file(${_WAITFILE_FILE}) loop"
		return 1
	fi

	echo "Found ${_WAITFILE_FILE} file."
	return 0
}

#
# Wait process up
#
# $1 :	PID file
# $2 :	interval second for loop
# $3 :	wait second after process up(allow 0 or to be omitted)
#
wait_process_up()
{
	#
	# Check parameters
	#
	if [ $# -lt 2 ]; then
		echo "Argument is wrong - $*"
		return 1
	fi
	_WAITPROC_PID_FILE="$1"

	if [ -z "$2" ]; then
		echo "The second argument is empty."
		return 1
	elif echo "$1" | grep -q '[^0-9]'; then
		echo "The second argument($2) must be number."
		return 1
	fi
	_WAITPROC_INTERVAL="$2"

	if [ $# -ge 3 ]; then
		_WAITPROC_AFTER_SEC="$3"
	else
		_WAITPROC_AFTER_SEC=0
	fi

	#
	# Loop
	#
	while true; do
		#
		# Check PID file
		#
		if [ -f "${_WAITPROC_PID_FILE}" ]; then
			#
			# Get PID
			#
			if ! _WAITPROC_PID=$(tr -d '\n' < "${_WAITPROC_PID_FILE}"); then
				prn_info "Could not read PID file(${_WAITPROC_PID_FILE}), it may be not started."
			else
				#
				# Check process
				#
				# shellcheck disable=SC2009
				if ps -p "${_WAITPROC_PID}" | grep -v PID | grep -q -v -i 'defunct'; then
					#
					# OK(single checking)
					#
					if [ "${_WAITPROC_AFTER_SEC}" -le 0 ]; then
						prn_info "PID(${_WAITPROC_PID}) process running."
						return 0
					fi

					#
					# Sleep
					#
					sleep "${_WAITPROC_AFTER_SEC}"

					#
					# Re-check process
					#
					# shellcheck disable=SC2009
					if ps -p "${_WAITPROC_PID}" | grep -v PID | grep -q -v -i 'defunct'; then
						#
						# OK(double checking)
						#
						prn_info "PID(${_WAITPROC_PID}) process running."
						return 0
					fi
					prn_info "Found PID file(${_WAITPROC_PID_FILE}) but PID(${_WAITPROC_PID}) is not running."
				else
					prn_info "Found PID file(${_WAITPROC_PID_FILE}) but PID(${_WAITPROC_PID}) is not running."
				fi
			fi
		else
			prn_info "Not found PID file(${_WAITPROC_PID_FILE}), it may be not started."
		fi

		#
		# Sleep
		#
		if [ "${_WAITPROC_INTERVAL}" -gt 0 ]; then
			sleep "${_WAITPROC_INTERVAL}"
		fi
	done

	echo "Something is wrong after waiting process(${_WAITPROC_PID_FILE}) loop."
	return 1
}

#
# Start process
#
# $1   :	Process name
# $2   :	Process owner
# $3   :	PID file
# $4   :	Wait second after process launch
# $5   :	Redirect file path or "NO"
# $6   :	ini file
# $7.. :	Process options
#
# [NOTE]
# Since this function executes wait internally, it is prohibited to call
# it in another shell.
# If you call it in a different shell, you will not be able to trap the
# signal.
#
start_process()
{
	#
	# Check parameters
	#
	if [ $# -lt 5 ]; then
		log_err "Argument is wrong - $*"
		return 1
	fi
	_STARTPROC_PROCNAME="$1"
	_STARTPROC_OWNER="$2"
	_STARTPROC_PIDFILE="$3"
	_STARTPROC_WAITSEC="$4"
	_STARTPROC_REDIRECTFILE="$5"
	_STARTPROC_INIFILE="$6"
	shift; shift; shift; shift; shift; shift

	_STARTPROC_OSTIONS="-conf ${_STARTPROC_INIFILE} $*"

	if ! _STARTPROC_BIN=$(command -v "${_STARTPROC_PROCNAME}" 2>/dev/null); then
		log_err "Not found execute file(${_STARTPROC_PROCNAME})."
		return 1
	fi

	if [ -f "${_STARTPROC_PIDFILE}" ]; then
		if _STARTPROC_PID=$(cat "${_STARTPROC_PIDFILE}"); then

			# shellcheck disable=SC2009
			if ps -p "${_STARTPROC_PID}" | grep -v PID | grep -q -v -i 'defunct'; then
				log_err "Could not start a new process because PID file(${_STARTPROC_PIDFILE}) exists and the process(${_STARTPROC_PID}) is running."
				return 1
			fi
		fi
		rm -f "${_STARTPROC_PIDFILE}"
	fi

	_STARTPROC_CUR_USER=$(id -un | tr -d '\n')
	if [ -z "${_STARTPROC_CUR_USER}" ] || [ -z "${_STARTPROC_OWNER}" ] || [ "${_STARTPROC_CUR_USER}" != "${_STARTPROC_OWNER}" ]; then
		if [ -z "${_STARTPROC_CUR_USER}" ] || [ "${_STARTPROC_CUR_USER}" != "root" ]; then
			log_err "The execution user(${_STARTPROC_OWNER}) of the process was specified, but the execution user of this script is not root."
			return 1
		fi
		_STARTPROC_USE_SUDO=1
	else
		_STARTPROC_USE_SUDO=0
	fi

	#
	# Convert options to an array to avoid using "/bin/sh -c"
	#
	# [NOTE]
	# This processing destroy argements, for using "$@"
	#
	# shellcheck disable=SC2086
	set ${_STARTPROC_BIN} ${_STARTPROC_OSTIONS}

	#
	# Run process
	#
	log_info "Launch a new process."

	if [ "${_STARTPROC_USE_SUDO}" -eq 1 ]; then
		if [ -n "${_STARTPROC_REDIRECTFILE}" ] && [ "${_STARTPROC_REDIRECTFILE}" = "NO" ]; then
			# shellcheck disable=SC2068
			sudo -u "${_STARTPROC_OWNER}" $@ &
		else
			# shellcheck disable=SC2068
			sudo -u "${_STARTPROC_OWNER}" $@ 2>&1 | sudo tee "${_STARTPROC_REDIRECTFILE}" >/dev/null 2>&1 &
		fi

		# [NOTE]
		# Detects the PID of sudo's child process because it is running a
		# subprocess via the sudo command.
		#
		sleep 1

		# shellcheck disable=SC2009
		_STARTPROC_PROCESS_ID=$(ps --ppid $! | grep "${_STARTPROC_PROCNAME}" | grep -v sudo | awk '{print $1}')
	else
		if [ -n "${_STARTPROC_REDIRECTFILE}" ] && [ "${_STARTPROC_REDIRECTFILE}" = "NO" ]; then
			# shellcheck disable=SC2068
			$@ &
		else
			# shellcheck disable=SC2068
			$@ >"${_STARTPROC_REDIRECTFILE}" 2>&1 &
		fi
		_STARTPROC_PROCESS_ID=$!
	fi
	if [ -z "${_STARTPROC_PROCESS_ID}" ]; then
		log_err "Could not start a new process, it was launched but exited soon."
		return 1
	fi
	echo "${_STARTPROC_PROCESS_ID}" > "${_STARTPROC_PIDFILE}"

	#
	# Sleep
	#
	if [ "${_STARTPROC_WAITSEC}" -gt 0 ]; then
		sleep "${_STARTPROC_WAITSEC}"
	fi

	#
	# Check process running
	#
	# shellcheck disable=SC2009
	if ! ps -p "${_STARTPROC_PROCESS_ID}" | grep -v PID | grep -q -v -i 'defunct'; then
		log_err "Could not start a new process, it was launched but exited soon."
		return 1
	fi
	log_info "Success launching a new process, and wait it exiting."

	#
	# wait
	#
	# [NOTE]
	# Do not execute the wait command directly for _STARTPROC_PROCESS_ID.
	# If it is started via the sudo command, the wait command cannot be
	# executed because it is not a direct child process of this script.
	# Therefore, the tail command waits for the end of _STARTPROC_PROCESS_ID.
	# However, if you wait with the tail command, the signal will also be
	# blocked, so execute the tail command itself in the background and
	# wait for the tail command's PID with the wait command.
	#
	tail -f --pid="${_STARTPROC_PROCESS_ID}" /dev/null &
	wait $!

	log_warn "Process(${_STARTPROC_PROCESS_ID}) exited."
	return 0
}

#
# Stop process
#
# $1 :	PID file
# $2 :	interval second for loop
# $3 :	Maximum try count
#
stop_process()
{
	#
	# Check parameters
	#
	if [ $# -ne 3 ]; then
		echo "Argument is wrong - $*"
		return 1
	fi
	_STOPPROC_PID_FILE="$1"
	_STOPPROC_INTERVAL="$2"
	_STOPPROC_TRYCOUNT="$3"

	if [ -z "${_STOPPROC_PID_FILE}" ] || [ ! -f "${_STOPPROC_PID_FILE}" ]; then
		echo "Already process has probabry stopped, PID file(${_STOPPROC_PID_FILE}) is not existed."
		return 0
	fi

	#
	# Loop
	#
	while [ "${_STOPPROC_TRYCOUNT}" -gt 0 ]; do
		#
		# Get PID
		#
		if ! _STOPPROC_PID=$(tr -d '\n' < "${_STOPPROC_PID_FILE}"); then
			echo "Could not read PID file(${_STOPPROC_PID_FILE}), it may be stopped."
			rm -f "${_STOPPROC_PID_FILE}"
			return 0
		fi

		#
		# Check process
		#
		# shellcheck disable=SC2009
		if ! ps -p "${_STOPPROC_PID}" | grep -v PID | grep -q -v -i 'defunct'; then
			echo "Stopped PID(${_STOPPROC_PID}) process."
			rm -f "${_STOPPROC_PID_FILE}"
			return 0
		fi

		#
		# Send signal HUP
		#
		prn_info "Send signal HUP to PID(${_STOPPROC_PID}) process."
		if kill -HUP "${_STOPPROC_PID}"; then
			sleep "${_STOPPROC_INTERVAL}"
		fi

		#
		# Check process
		#
		# shellcheck disable=SC2009
		if ! ps -p "${_STOPPROC_PID}" | grep -v PID | grep -q -v -i 'defunct'; then
			echo "Stopped PID(${_STOPPROC_PID}) process."
			rm -f "${_STOPPROC_PID_FILE}"
			return 0
		fi
		prn_info "Could not stop PID(${_STOPPROC_PID}) process by signal HUP."

		#
		# Send signal KILL
		#
		prn_info "Send signal KILL to PID(${_STOPPROC_PID}) process."
		if kill -KILL "${_STOPPROC_PID}"; then
			sleep "${_STOPPROC_INTERVAL}"
		fi

		#
		# Check process
		#
		# shellcheck disable=SC2009
		if ! ps -p "${_STOPPROC_PID}" | grep -v PID | grep -q -v -i 'defunct'; then
			echo "Stopped PID(${_STOPPROC_PID}) process."
			rm -f "${_STOPPROC_PID_FILE}"
			return 0
		fi
		prn_info "Could not stop PID(${_STOPPROC_PID}) process by signal KILL."

		_STOPPROC_TRYCOUNT=$((_STOPPROC_TRYCOUNT - 1))
	done

	echo "Could not stop PID(${_STOPPROC_PID}) process."
	return 1
}

#
# Exit script
#
# $1 :	Exit code
#
exit_main()
{
	#
	# Check parameters
	#
	if [ $# -lt 1 ]; then
		_EXIT_CODE=1
	else
		_EXIT_CODE="$1"
	fi

	#
	# Stop subprocess and remove pid file
	#
	if _TMP_MSG=$(stop_process "${PIDDIR}/${SUBPROCESS_PIDFILE}" "${INTERVAL_SEC_FOR_LOOP}" "${TRYCOUNT_STOP_SUBPROC}"); then
		log_info "${_TMP_MSG}"
	else
		log_err "${_TMP_MSG}"
	fi

	#
	# Remove pid file
	#
	if [ -f "${PIDDIR}/${SERVICE_PIDFILE}" ]; then
		rm -f "${PIDDIR}/${SERVICE_PIDFILE}"
	fi

	exit "${_EXIT_CODE}"
}

#
# Load and Set global variables
#
# Set Variables
#	INI_CONF_FILE
#	PIDDIR
#	SERVICE_PIDFILE
#	SUBPROCESS_PIDFILE
#	SUBPROCESS_USER
#	LOGDIR
#	SERVICE_LOGFILE
#	SUBPROCESS_LOGFILE
#	WAIT_DEPENDPROC_PIDFILE
#	WAIT_SEC_AFTER_DEPENDPROC_UP
#	WAIT_SEC_STARTUP
#	WAIT_SEC_AFTER_SUBPROCESS_UP
#	INTERVAL_SEC_FOR_LOOP
#	TRYCOUNT_STOP_SUBPROC
#	SUBPROCESS_OPTIONS
#	BEFORE_RUN_SUBPROCESS
#
# Input parameters
#	$1: exit	Last command when finish processing(defaut: return).
#
load_variables()
{
	_TMP_FIN_CMD="return"
	if [ $# -gt 0 ]; then
		if [ -n "$1" ] && [ "$1" = "exit" ]; then
			_TMP_FIN_CMD="exit"
		fi
	fi

	#
	# INI_CONF_FILE
	#
	if ! _TMP_VALUE=$(get_value "${INI_CONF_FILE_KEYWORD}" "${CONFDIR}/${SERVICE_CONF_FILE}" "${CONFDIR}/${OVERRIDE_FILE}"); then
		log_err "Failed loading the configuration(${INI_CONF_FILE_KEYWORD}) value : ${_TMP_VALUE}"
		"${_TMP_FIN_CMD}" 1
	elif [ -n "${_TMP_VALUE}" ]; then
		if ! echo "${_TMP_VALUE}" | grep -q '^/'; then
			#
			# file is relative file path, convert to absolute path
			#
			_TMP_RELATIVE=$(dirname "${_TMP_VALUE}")
			_TMP_ABSOLUTE=$(cd "${CONFDIR}/${_TMP_RELATIVE}" || exit 1; pwd)
			INI_CONF_FILE="${_TMP_ABSOLUTE}/${_TMP_VALUE}"
		else
			INI_CONF_FILE="${_TMP_VALUE}"
		fi
	fi

	#
	# PIDDIR
	#
	if ! _TMP_VALUE=$(get_value "PIDDIR" "${CONFDIR}/${SERVICE_CONF_FILE}" "${CONFDIR}/${OVERRIDE_FILE}"); then
		log_err "Failed loading the configuration(PIDDIR) value : ${_TMP_VALUE}"
		"${_TMP_FIN_CMD}" 1
	elif [ -n "${_TMP_VALUE}" ]; then
		PIDDIR="${_TMP_VALUE}"
	fi

	if [ -n "${PIDDIR}" ]; then
		if [ ! -d "${PIDDIR}" ]; then
			if ! mkdir -p "${PIDDIR}"; then
				log_err "Could not make directory ${PIDDIR} for PID file."
				"${_TMP_FIN_CMD}" 1
			fi
		fi
	fi

	#
	# SERVICE_PIDFILE
	#
	if ! _TMP_VALUE=$(get_value "SERVICE_PIDFILE" "${CONFDIR}/${SERVICE_CONF_FILE}" "${CONFDIR}/${OVERRIDE_FILE}"); then
		log_err "Failed loading the configuration(SERVICE_PIDFILE) value : ${_TMP_VALUE}"
		"${_TMP_FIN_CMD}" 1
	elif [ -n "${_TMP_VALUE}" ]; then
		SERVICE_PIDFILE="${_TMP_VALUE}"
	fi

	#
	# SUBPROCESS_PIDFILE
	#
	if ! _TMP_VALUE=$(get_value "SUBPROCESS_PIDFILE" "${CONFDIR}/${SERVICE_CONF_FILE}" "${CONFDIR}/${OVERRIDE_FILE}"); then
		log_err "Failed loading the configuration(SUBPROCESS_PIDFILE) value : ${_TMP_VALUE}"
		"${_TMP_FIN_CMD}" 1
	elif [ -n "${_TMP_VALUE}" ]; then
		SUBPROCESS_PIDFILE="${_TMP_VALUE}"
	fi

	#
	# SUBPROCESS_USER
	#
	if ! _TMP_VALUE=$(get_value "SUBPROCESS_USER" "${CONFDIR}/${SERVICE_CONF_FILE}" "${CONFDIR}/${OVERRIDE_FILE}"); then
		log_err "Failed loading the subprocess user(SUBPROCESS_USER) value : ${_TMP_VALUE}"
		"${_TMP_FIN_CMD}" 1
	elif [ -n "${_TMP_VALUE}" ]; then
		SUBPROCESS_USER="${_TMP_VALUE}"
	fi

	#
	# LOGDIR
	#
	if ! _TMP_VALUE=$(get_value "LOGDIR" "${CONFDIR}/${SERVICE_CONF_FILE}" "${CONFDIR}/${OVERRIDE_FILE}"); then
		log_err "Failed loading the configuration(LOGDIR) value : ${_TMP_VALUE}"
		"${_TMP_FIN_CMD}" 1
	elif [ -n "${_TMP_VALUE}" ]; then
		LOGDIR="${_TMP_VALUE}"
	fi

	if [ -n "${LOGDIR}" ]; then
		if [ ! -d "${LOGDIR}" ]; then
			if ! mkdir -p "${LOGDIR}"; then
				log_err "Could not make directory ${LOGDIR} for log files."
				"${_TMP_FIN_CMD}" 1
			fi
		fi
		# [NOTE]
		# If you start a subprocess other than the user of this script, you must allow permissions.
		#
		if ! chmod 0777 "${LOGDIR}"; then
			log_warn "Could not change attributes for directory ${LOGDIR}, but it is not critical so continue..."
		fi
	fi

	#
	# SERVICE_LOGFILE
	#
	if ! _TMP_VALUE=$(get_value "SERVICE_LOGFILE" "${CONFDIR}/${SERVICE_CONF_FILE}" "${CONFDIR}/${OVERRIDE_FILE}"); then
		log_err "Failed loading the configuration(SERVICE_LOGFILE) value : ${_TMP_VALUE}"
		"${_TMP_FIN_CMD}" 1
	elif [ -n "${_TMP_VALUE}" ]; then
		SERVICE_LOGFILE="${_TMP_VALUE}"
	fi

	#
	# SUBPROCESS_LOGFILE
	#
	if ! _TMP_VALUE=$(get_value "SUBPROCESS_LOGFILE" "${CONFDIR}/${SERVICE_CONF_FILE}" "${CONFDIR}/${OVERRIDE_FILE}"); then
		log_err "Failed loading the configuration(SUBPROCESS_LOGFILE) value : ${_TMP_VALUE}"
		"${_TMP_FIN_CMD}" 1
	elif [ -n "${_TMP_VALUE}" ]; then
		SUBPROCESS_LOGFILE="${_TMP_VALUE}"
	fi

	#
	# WAIT_DEPENDPROC_PIDFILE
	#
	if ! _TMP_VALUE=$(get_value "WAIT_DEPENDPROC_PIDFILE" "${CONFDIR}/${SERVICE_CONF_FILE}" "${CONFDIR}/${OVERRIDE_FILE}"); then
		log_err "Failed loading the configuration(WAIT_DEPENDPROC_PIDFILE) value : ${_TMP_VALUE}"
		"${_TMP_FIN_CMD}" 1
	elif [ -n "${_TMP_VALUE}" ]; then
		WAIT_DEPENDPROC_PIDFILE="${_TMP_VALUE}"
	fi

	#
	# WAIT_SEC_AFTER_DEPENDPROC_UP
	#
	if ! _TMP_VALUE=$(get_value "WAIT_SEC_AFTER_DEPENDPROC_UP" "${CONFDIR}/${SERVICE_CONF_FILE}" "${CONFDIR}/${OVERRIDE_FILE}"); then
		log_err "Failed loading the configuration(WAIT_SEC_AFTER_DEPENDPROC_UP) value : ${_TMP_VALUE}"
		"${_TMP_FIN_CMD}" 1
	elif [ -n "${_TMP_VALUE}" ]; then
		if echo "${_TMP_VALUE}" | grep -q '[^0-9]'; then
			log_err "The configuration(WAIT_SEC_AFTER_DEPENDPROC_UP) value must be number : ${_TMP_VALUE}"
			"${_TMP_FIN_CMD}" 1
		fi
		WAIT_SEC_AFTER_DEPENDPROC_UP="${_TMP_VALUE}"
	fi

	#
	# WAIT_SEC_STARTUP
	#
	if ! _TMP_VALUE=$(get_value "WAIT_SEC_STARTUP" "${CONFDIR}/${SERVICE_CONF_FILE}" "${CONFDIR}/${OVERRIDE_FILE}"); then
		log_err "Failed loading the configuration(WAIT_SEC_STARTUP) value : ${_TMP_VALUE}"
		"${_TMP_FIN_CMD}" 1
	elif [ -n "${_TMP_VALUE}" ]; then
		if echo "${_TMP_VALUE}" | grep -q '[^0-9]'; then
			log_err "The configuration(WAIT_SEC_STARTUP) value must be number : ${_TMP_VALUE}"
			"${_TMP_FIN_CMD}" 1
		fi
		WAIT_SEC_STARTUP="${_TMP_VALUE}"
	fi

	#
	# WAIT_SEC_AFTER_SUBPROCESS_UP
	#
	if ! _TMP_VALUE=$(get_value "WAIT_SEC_AFTER_SUBPROCESS_UP" "${CONFDIR}/${SERVICE_CONF_FILE}" "${CONFDIR}/${OVERRIDE_FILE}"); then
		log_err "Failed loading the configuration(WAIT_SEC_AFTER_SUBPROCESS_UP) value : ${_TMP_VALUE}"
		"${_TMP_FIN_CMD}" 1
	elif [ -n "${_TMP_VALUE}" ]; then
		if echo "${_TMP_VALUE}" | grep -q '[^0-9]'; then
			log_err "The configuration(WAIT_SEC_AFTER_SUBPROCESS_UP) value must be number : ${_TMP_VALUE}"
			"${_TMP_FIN_CMD}" 1
		fi
		WAIT_SEC_AFTER_SUBPROCESS_UP="${_TMP_VALUE}"
	fi

	#
	# INTERVAL_SEC_FOR_LOOP
	#
	if ! _TMP_VALUE=$(get_value "INTERVAL_SEC_FOR_LOOP" "${CONFDIR}/${SERVICE_CONF_FILE}" "${CONFDIR}/${OVERRIDE_FILE}"); then
		log_err "Failed loading the configuration(INTERVAL_SEC_FOR_LOOP) value : ${_TMP_VALUE}"
		"${_TMP_FIN_CMD}" 1
	elif [ -n "${_TMP_VALUE}" ]; then
		if echo "${_TMP_VALUE}" | grep -q '[^0-9]'; then
			log_err "The configuration(INTERVAL_SEC_FOR_LOOP) value must be number : ${_TMP_VALUE}"
			"${_TMP_FIN_CMD}" 1
		fi
		INTERVAL_SEC_FOR_LOOP="${_TMP_VALUE}"
	fi

	#
	# TRYCOUNT_STOP_SUBPROC
	#
	if ! _TMP_VALUE=$(get_value "TRYCOUNT_STOP_SUBPROC" "${CONFDIR}/${SERVICE_CONF_FILE}" "${CONFDIR}/${OVERRIDE_FILE}"); then
		log_err "Failed loading the configuration(TRYCOUNT_STOP_SUBPROC) value : ${_TMP_VALUE}"
		"${_TMP_FIN_CMD}" 1
	elif [ -n "${_TMP_VALUE}" ]; then
		if echo "${_TMP_VALUE}" | grep -q '[^0-9]'; then
			log_err "The configuration(TRYCOUNT_STOP_SUBPROC) value must be number : ${_TMP_VALUE}"
			"${_TMP_FIN_CMD}" 1
		fi
		TRYCOUNT_STOP_SUBPROC="${_TMP_VALUE}"
	fi

	#
	# SUBPROCESS_OPTIONS
	#
	if ! _TMP_VALUE=$(get_value "SUBPROCESS_OPTIONS" "${CONFDIR}/${SERVICE_CONF_FILE}" "${CONFDIR}/${OVERRIDE_FILE}"); then
		log_err "Failed loading the configuration(SUBPROCESS_OPTIONS) value : ${_TMP_VALUE}"
		"${_TMP_FIN_CMD}" 1
	elif [ -n "${_TMP_VALUE}" ]; then
		SUBPROCESS_OPTIONS="${_TMP_VALUE}"
	fi

	#
	# BEFORE_RUN_SUBPROCESS
	#
	if ! _TMP_VALUE=$(get_value "BEFORE_RUN_SUBPROCESS" "${CONFDIR}/${SERVICE_CONF_FILE}" "${CONFDIR}/${OVERRIDE_FILE}"); then
		log_err "Failed loading the configuration(BEFORE_RUN_SUBPROCESS) value : ${_TMP_VALUE}"
		"${_TMP_FIN_CMD}" 1
	elif [ -n "${_TMP_VALUE}" ]; then
		BEFORE_RUN_SUBPROCESS="${_TMP_VALUE}"
	fi

	return 0
}

#---------------------------------------------------------------------
# Options
#---------------------------------------------------------------------
SCRIPT_MODE=""

while [ $# -ne 0 ]; do
	if [ -z "$1" ]; then
		break;

	elif [ "$1" = "-h" ] || [ "$1" = "-H" ] || [ "$1" = "--help" ] || [ "$1" = "--HELP" ]; then
		func_usage "${PRGNAME}"
		exit 0

	elif [ "$1" = "start" ] || [ "$1" = "START" ]; then
		if [ -n "${SCRIPT_MODE}" ]; then
			log_err "Option $1 is conflicted, already set \"${SCRIPT_MODE}\" mode."
			exit 1
		fi
		SCRIPT_MODE="START"

	elif [ "$1" = "stop" ] || [ "$1" = "STOP" ]; then
		if [ -n "${SCRIPT_MODE}" ]; then
			log_err "Option $1 is conflicted, already set \"${SCRIPT_MODE}\" mode."
			exit 1
		fi
		SCRIPT_MODE="STOP"

	elif [ "$1" = "restart" ] || [ "$1" = "RESTART" ]; then
		if [ -n "${SCRIPT_MODE}" ]; then
			log_err "Option $1 is conflicted, already set \"${SCRIPT_MODE}\" mode."
			exit 1
		fi
		SCRIPT_MODE="RESTART"

	else
		log_err "Unknown option: $1, check usage with the -h option."
		exit 1
	fi
	shift
done

if [ -z "${SCRIPT_MODE}" ]; then
	log_err "Option \"start\", \"stop\" or \"restart\" must be specified."
	exit 1
fi

#---------------------------------------------------------------------
# Signales
#---------------------------------------------------------------------
#
# Signal handler
#
SigHandle()
{
	CAUGHT_SIGNAL=1

	#
	# Try count is set 1 for emergency
	#
	TRYCOUNT_STOP_SUBPROC=1

	log_info "Caught signal $1, try to stop subprocess and exit."
	exit_main 0
}

#
# Set trap signals
#
# SIGHUP(1) / SIGINT(2) / SIGQUIT(3) / SIGABRT(6) / SIGTERM(15)
#
trap 'SigHandle 1'	1
trap 'SigHandle 2'	2
trap 'SigHandle 3'	3
trap 'SigHandle 6'	6
trap 'SigHandle 15'	15

#---------------------------------------------------------------------
# Main
#---------------------------------------------------------------------
log_info "Start processing"

#
# Load and Set variables
#
load_variables "exit"

if [ "${SCRIPT_MODE}" = "STOP" ]; then
	#
	# Check old process PID file
	#
	if [ ! -f "${PIDDIR}/${SERVICE_PIDFILE}" ]; then
		log_info "There is not PID file(${PIDDIR}/${SERVICE_PIDFILE}), then the process has stopped."
		exit 0
	fi
	OLD_PROCESS_PID=$(tr -d '\n' < "${PIDDIR}/${SERVICE_PIDFILE}")

	#
	# Check old process
	#
	# shellcheck disable=SC2009
	if ! ps -p "${OLD_PROCESS_PID}" | grep -v PID | grep -q -v -i 'defunct'; then
		log_info "PID(${OLD_PROCESS_PID}) process is not exusted, then the process has stopped."
		rm -f "${PIDDIR}/${SERVICE_PIDFILE}"
		exit 0
	fi

	#
	# Send signal HUP
	#
	log_info "Try to stop PID(${OLD_PROCESS_PID}) process."
	kill -HUP "${OLD_PROCESS_PID}"

	#
	# Wait old process stopping
	#
	_LOOP_COUNT=10
	while [ "${_LOOP_COUNT}" -gt 0 ]; do
		#
		# Sleep
		#
		sleep "${INTERVAL_SEC_FOR_LOOP}"

		#
		# Check process running
		#
		# shellcheck disable=SC2009
		if ! ps -p "${OLD_PROCESS_PID}" | grep -v PID | grep -q -v -i 'defunct'; then
			#
			# Stopped
			#
			break
		fi
		_LOOP_COUNT=$((_LOOP_COUNT - 1))
	done

	#
	# Force stop if not stopped
	#
	if [ "${_LOOP_COUNT}" -le 0 ]; then
		#
		# Send signal KILL
		#
		kill -KILL "${OLD_PROCESS_PID}"
		sleep "${INTERVAL_SEC_FOR_LOOP}"

		# shellcheck disable=SC2009
		if ps -p "${OLD_PROCESS_PID}" | grep -v PID | grep -q -v -i 'defunct'; then
			log_err "Could not stop PID(${OLD_PROCESS_PID}) process."
			exit 1
		fi
	fi

	#
	# Success
	#
	rm -f "${PIDDIR}/${SERVICE_PIDFILE}"

else
	#
	# Check old process
	#
	if [ -f "${PIDDIR}/${SERVICE_PIDFILE}" ]; then
		OLD_PROCESS_PID=$(tr -d '\n' < "${PIDDIR}/${SERVICE_PIDFILE}")
		if [ $$ -ne "${OLD_PROCESS_PID}" ]; then
			# shellcheck disable=SC2009
			if ps -p "${OLD_PROCESS_PID}" | grep -v PID | grep -q -v -i 'defunct'; then
				log_err "Process PID(${OLD_PROCESS_PID}) has not stopped yet."
				exit 1
			fi
			#
			# Over write Process id to PID FILE
			#
			echo $$ > "${PIDDIR}/${SERVICE_PIDFILE}"
		fi
	else
		#
		# Write Process id to PID FILE
		#
		echo $$ > "${PIDDIR}/${SERVICE_PIDFILE}"
	fi

	#
	# Wait after startup
	#
	sleep "${WAIT_SEC_STARTUP}"
	log_info "Since the waiting time after startup has expired, processing will continue."

	#
	# Loop
	#
	while true; do
		#
		# Relaad variables(without error exiting)
		#
		if ! load_variables; then
			if [ "${INTERVAL_SEC_FOR_LOOP}" -gt 0 ]; then
				sleep "${INTERVAL_SEC_FOR_LOOP}"
			else
				sleep 1
			fi
			continue
		fi

		#
		# Wait ini file
		#
		if ! _TMP_MSG=$(wait_file "${INI_CONF_FILE}" "${INTERVAL_SEC_FOR_LOOP}"); then
			log_err "Failed waiting/checking the ini file(${INI_CONF_FILE}) : ${_TMP_MSG}"
			exit_main 1
		fi

		#
		# Wait depended process up
		#
		if [ -n "${WAIT_DEPENDPROC_PIDFILE}" ]; then
			if ! _TMP_MSG=$(wait_process_up "${WAIT_DEPENDPROC_PIDFILE}" "${INTERVAL_SEC_FOR_LOOP}" "${WAIT_SEC_AFTER_DEPENDPROC_UP}"); then
				log_err "Failed waiting/checking process(${WAIT_DEPENDPROC_PIDFILE}) : ${_TMP_MSG}"
				exit_main 1
			fi
		fi

		#
		# Execute before launch subprocess
		#
		if [ -n "${BEFORE_RUN_SUBPROCESS}" ]; then
			if ! /bin/sh -c "${BEFORE_RUN_SUBPROCESS}"; then
				log_warn "Failed to execute (${BEFORE_RUN_SUBPROCESS}) before lauching subprocess, but continue..."
			fi
		fi

		#
		# Start subprocess and wait
		#
		# [NOTE]
		# Since the start_process function executes wait internally, it is prohibited to
		# call it in a different shell.
		#
		_TMP_SUBPROCESS_OPTIONS="${SUBPROCESS_OPTIONS}"
		if [ -n "${LOGDIR}" ] && [ -n "${SUBPROCESS_LOGFILE}" ]; then
			_TMP_SUBPROCESS_REDIRECTFILE="${LOGDIR}/${SUBPROCESS_LOGFILE}"
		else
			_TMP_SUBPROCESS_REDIRECTFILE="NO"
		fi

		if ! start_process "${SUBPROCESS_BIN}" "${SUBPROCESS_USER}" "${PIDDIR}/${SUBPROCESS_PIDFILE}" "${WAIT_SEC_AFTER_SUBPROCESS_UP}" "${_TMP_SUBPROCESS_REDIRECTFILE}" "${INI_CONF_FILE}" "${_TMP_SUBPROCESS_OPTIONS}"; then
			log_err "Failed launching subprocess(${SUBPROCESS_BIN}) : ${_TMP_MSG}"
			exit_main 1
		fi
		if [ "${CAUGHT_SIGNAL}" -eq 1 ]; then
			break;
		fi

		#
		# Sleep
		#
		if [ "${INTERVAL_SEC_FOR_LOOP}" -gt 0 ]; then
			sleep "${INTERVAL_SEC_FOR_LOOP}"
		else
			sleep 1
		fi
	done
fi

#---------------------------------------------------------------------
# Finish
#---------------------------------------------------------------------
exit 0

#
# Local variables:
# tab-width: 4
# c-basic-offset: 4
# End:
# vim600: noexpandtab sw=4 ts=4 fdm=marker
# vim<600: noexpandtab sw=4 ts=4
#
