#!/bin/sh
#
# CHMPX
#
# Copyright 2015 Yahoo Japan Corporation.
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
# CREATE:   Fri May 8 2015
# REVISION:
#

#--------------------------------------------------------------
# Common Variables
#--------------------------------------------------------------
PRGNAME=$(basename "${0}")
SCRIPTDIR=$(dirname "${0}")
SCRIPTDIR=$(cd "${SCRIPTDIR}" || exit 1; pwd)
SRCTOP=$(cd "${SCRIPTDIR}/.." || exit 1; pwd)

#
# Directories / Files
#
SRCDIR="${SRCTOP}/src"
TESTDIR="${SRCTOP}/tests"
LIBOBJDIR="${SRCTOP}/lib/.libs"
#TESTOBJDIR="${TESTDIR}/.libs"

#
# Process ID
#
PROCID=$$

#
# Test data/result files
#
TEST_SVR_INI_FILE="${TESTDIR}/test_server.ini"
TEST_SLV_INI_FILE="${TESTDIR}/test_slave.ini"

CFG_TEST_JSONSTRING_FILE="${TESTDIR}/cfg_test_json_string.data"
CFG_TEST_SVR_INI_FILE="${TESTDIR}/cfg_test_server.ini"
CFG_TEST_SVR_YAML_FILE="${TESTDIR}/cfg_test_server.yaml"
CFG_TEST_SVR_JSON_FILE="${TESTDIR}/cfg_test_server.json"
CFG_TEST_SLV_INI_FILE="${TESTDIR}/cfg_test_slave.ini"
CFG_TEST_SLV_YAML_FILE="${TESTDIR}/cfg_test_slave.yaml"
CFG_TEST_SLV_JSON_FILE="${TESTDIR}/cfg_test_slave.json"

CONFTEST_SVR_RESULT_FILE="${TESTDIR}/cfg_test_server.result"
CONFTEST_SLV_RESULT_FILE="${TESTDIR}/cfg_test_slave.result"

#
# Log files
#
STOP_PROC_LOGFILE="/tmp/.${PRGNAME}_stop_proc.log"

CONFTEST_SVR_INI_LOGFILE="/tmp/conftest_svr_ini_${PROCID}.log"
CONFTEST_SVR_YAML_LOGFILE="/tmp/conftest_svr_yaml_${PROCID}.log"
CONFTEST_SVR_JSON_LOGFILE="/tmp/conftest_svr_json_${PROCID}.log"
CONFTEST_SVR_SJSON_LOGFILE="/tmp/conftest_svr_sjson_${PROCID}.log"
CONFTEST_SVR_INIENV_LOGFILE="/tmp/conftest_svr_inienv_${PROCID}.log"
CONFTEST_SVR_JSONENV_LOGFILE="/tmp/conftest_svr_jsonenv_${PROCID}.log"
CONFTEST_SLV_INI_LOGFILE="/tmp/conftest_slv_ini_${PROCID}.log"
CONFTEST_SLV_YAML_LOGFILE="/tmp/conftest_slv_yaml_${PROCID}.log"
CONFTEST_SLV_JSON_LOGFILE="/tmp/conftest_slv_json_${PROCID}.log"
CONFTEST_SLV_SJON_LOGFILE="/tmp/conftest_slv_sjson_${PROCID}.log"
CONFTEST_SLV_INIENV_LOGFILE="/tmp/conftest_slv_inienv_${PROCID}.log"
CONFTEST_SLV_SJONENV_LOGFILE="/tmp/conftest_slv_jsonenv_${PROCID}.log"

TEST_SVR_PROC_LOGFILE="/tmp/testsvr_${PROCID}.log"
TEST_SLV_PROC_LOGFILE="/tmp/testslv_${PROCID}.log"

#
# LD_LIBRARY_PATH / TESTPROGDIR
#
LD_LIBRARY_PATH="${LIBOBJDIR}"
export LD_LIBRARY_PATH

TESTPROGDIR="${TESTDIR}"
export TESTPROGDIR

#
# Debug flag
#
CHMPX_DBG_FLAG="silent"

#--------------------------------------------------------------
# Variables and Utility functions
#--------------------------------------------------------------
#
# Escape sequence
#
if [ -t 1 ]; then
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

#--------------------------------------------------------------
# Message functions
#--------------------------------------------------------------
PRNTITLE()
{
	echo ""
	echo "${CGRN}---------------------------------------------------------------------${CDEF}"
	echo "${CGRN}${CREV}[TITLE]${CDEF} ${CGRN}$*${CDEF}"
	echo "${CGRN}---------------------------------------------------------------------${CDEF}"
}

PRNERR()
{
	echo "${CBLD}${CRED}[ERROR]${CDEF} ${CRED}$*${CDEF}"
}

PRNWARN()
{
	echo "${CYEL}${CREV}[WARNING]${CDEF} $*"
}

PRNMSG()
{
	echo "${CYEL}${CREV}[MSG]${CDEF} ${CYEL}$*${CDEF}"
}

PRNINFO()
{
	echo "${CREV}[INFO]${CDEF} $*"
}

PRNSUCCEED()
{
	echo "${CREV}[SUCCEED]${CDEF} $*"
}

#--------------------------------------------------------------
# Utilitiy functions
#--------------------------------------------------------------
stop_process()
{
	if [ -z "$1" ]; then
		return 1
	fi
	_STOP_PIDS="$1"

	for _ONE_PID in ${_STOP_PIDS}; do
		_MAX_TRYCOUNT=10

		while [ "${_MAX_TRYCOUNT}" -gt 0 ]; do
			#
			# Try HUP
			#
			kill -HUP "${_ONE_PID}" > /dev/null 2>&1
			sleep 1

			# shellcheck disable=SC2009
			if ! ( ps -o pid,stat ax 2>/dev/null | grep -v 'PID' | awk '$2~/^[^Z]/ { print $1 }' | grep -q "^${_ONE_PID}$" || exit 1 && exit 0 ); then
				break
			fi

			#
			# Try KILL
			#
			kill -KILL "${_ONE_PID}" > /dev/null 2>&1
			sleep 1

			# shellcheck disable=SC2009
			if ! ( ps -o pid,stat ax 2>/dev/null | grep -v 'PID' | awk '$2~/^[^Z]/ { print $1 }' | grep -q "^${_ONE_PID}$" || exit 1 && exit 0 ); then
				break
			fi
			echo "[FAILED] kill ${_ONE_PID} process" >> "${STOP_PROC_LOGFILE}"

			_MAX_TRYCOUNT=$((_MAX_TRYCOUNT - 1))
		done

		if [ "${_MAX_TRYCOUNT}" -le 0 ]; then
			# shellcheck disable=SC2009
			if ( ps -o pid,stat ax 2>/dev/null | grep -v 'PID' | awk '$2~/^[^Z]/ { print $1 }' | grep -q "^${_ONE_PID}$" || exit 1 && exit 0 ); then
				PRNWARN "Could not stop ${_ONE_PID} process, because it has defunct status. So assume we were able to stop it."
			else
				return 1
			fi
		fi
	done

	return 0
}

cleanup_files()
{
	#
	# Cleanup log files
	#
	rm -f "${CONFTEST_SVR_INI_LOGFILE}"
	rm -f "${CONFTEST_SVR_YAML_LOGFILE}"
	rm -f "${CONFTEST_SVR_JSON_LOGFILE}"
	rm -f "${CONFTEST_SVR_SJSON_LOGFILE}"
	rm -f "${CONFTEST_SVR_INIENV_LOGFILE}"
	rm -f "${CONFTEST_SVR_JSONENV_LOGFILE}"
	rm -f "${CONFTEST_SLV_INI_LOGFILE}"
	rm -f "${CONFTEST_SLV_YAML_LOGFILE}"
	rm -f "${CONFTEST_SLV_JSON_LOGFILE}"
	rm -f "${CONFTEST_SLV_SJON_LOGFILE}"
	rm -f "${CONFTEST_SLV_INIENV_LOGFILE}"
	rm -f "${CONFTEST_SLV_SJONENV_LOGFILE}"
	rm -f "${TEST_SVR_PROC_LOGFILE}"
	rm -f "${TEST_SLV_PROC_LOGFILE}"

	#
	# Cleanup files created by processes
	#
	rm -f /tmp/TESTSCRPT-8021.chmpx
	rm -f /tmp/TESTSCRPT-8021.k2h
	rm -f /tmp/TESTSCRPT-8022.chmpx
	rm -f /tmp/TESTSCRPT-8022.k2h
	rm -f /tmp/.k2h_*
	rm -f /tmp/chmpxbench-*.dat

	return 0
}

#==============================================================
# Set and Check variables for test
#==============================================================
BENCH_COUNT=10

#
# Check binary path
#
if [ -f "${SRCDIR}/chmpx" ]; then
	CHMPXBIN="${SRCDIR}/chmpx"
elif [ -f "${SRCDIR}/chmmain" ]; then
	CHMPXBIN="${SRCDIR}/chmmain"
else
	PRNERR "Not found chmpx binary"
	exit 1
fi
if [ -f "${TESTDIR}/chmpxbench" ]; then
	CHMPXBENCHBIN="${TESTDIR}/chmpxbench"
else
	PRNERR "Not found chmpxbench binary"
	exit 1
fi
if [ -f "${TESTDIR}/chmpxconftest" ]; then
	CHMCONFTEST="${TESTDIR}/chmpxconftest"
else
	PRNERR "Not found chmpxconftest binary"
	exit 1
fi

#==============================================================
# Configuration test
#==============================================================
#
# make parameter for JSON
#
JSON_SERVER_PARAM=$(grep 'SERVER=' "${CFG_TEST_JSONSTRING_FILE}" 2>/dev/null | sed -e 's/SERVER=//g')
JSON_SLAVE_PARAM=$(grep 'SLAVE=' "${CFG_TEST_JSONSTRING_FILE}" 2>/dev/null | sed -e 's/SLAVE=//g')

#--------------------------------------------------------------
# Test for conf loading
#--------------------------------------------------------------
#
# LOAD INI file for server
#
PRNTITLE "LOAD INI file for server"

if ! "${CHMCONFTEST}" -conf "${CFG_TEST_SVR_INI_FILE}" -no_check_update 2>&1 | grep -v NAME | grep -v DATE > "${CONFTEST_SVR_INI_LOGFILE}" 2>&1; then
	PRNERR "Failed test configuration for server INI."
	cleanup_files
	exit 1
fi
if ! diff "${CONFTEST_SVR_RESULT_FILE}" "${CONFTEST_SVR_INI_LOGFILE}" >/dev/null 2>&1; then
	PRNERR "Failed test configuration for server INI."
	cleanup_files
	exit 1
fi
PRNSUCCEED "LOAD INI file for server"

#
# LOAD INI file for slave
#
PRNTITLE "LOAD INI file for slave"

if ! "${CHMCONFTEST}" -conf "${CFG_TEST_SLV_INI_FILE}" -no_check_update 2>&1 | grep -v NAME | grep -v DATE > "${CONFTEST_SLV_INI_LOGFILE}" 2>&1; then
	PRNERR "Failed test configuration for slave INI."
	cleanup_files
	exit 1
fi
if ! diff "${CONFTEST_SLV_RESULT_FILE}" "${CONFTEST_SLV_INI_LOGFILE}" >/dev/null 2>&1; then
	PRNERR "Failed test configuration for slave INI."
	cleanup_files
	exit 1
fi
PRNSUCCEED "LOAD INI file for slave"

#
# LOAD YAML file for server
#
PRNTITLE "LOAD YAML file for server"

if ! "${CHMCONFTEST}" -conf "${CFG_TEST_SVR_YAML_FILE}" -no_check_update 2>&1 | grep -v NAME | grep -v DATE > "${CONFTEST_SVR_YAML_LOGFILE}" 2>&1; then
	PRNERR "Failed test configuration for server YAML."
	cleanup_files
	exit 1
fi
if ! diff "${CONFTEST_SVR_RESULT_FILE}" "${CONFTEST_SVR_YAML_LOGFILE}" >/dev/null 2>&1; then
	PRNERR "Failed test configuration for server YAML."
	cleanup_files
	exit 1
fi
PRNSUCCEED "LOAD YAML file for server"

#
# LOAD YAML file for slave
#
PRNTITLE "LOAD YAML file for slave"

if ! "${CHMCONFTEST}" -conf "${CFG_TEST_SLV_YAML_FILE}" -no_check_update 2>&1 | grep -v NAME | grep -v DATE > "${CONFTEST_SLV_YAML_LOGFILE}" 2>&1; then
	PRNERR "Failed test configuration for slave YAML."
	cleanup_files
	exit 1
fi
if ! diff "${CONFTEST_SLV_RESULT_FILE}" "${CONFTEST_SLV_YAML_LOGFILE}" >/dev/null 2>&1; then
	PRNERR "Failed test configuration for slave YAML."
	cleanup_files
	exit 1
fi
PRNSUCCEED "LOAD YAML file for slave"

#
# LOAD JSON file for server
#
PRNTITLE "LOAD JSON file for server"

if ! "${CHMCONFTEST}" -conf "${CFG_TEST_SVR_JSON_FILE}" -no_check_update 2>&1 | grep -v NAME | grep -v DATE > "${CONFTEST_SVR_JSON_LOGFILE}" 2>&1; then
	PRNERR "Failed test configuration for server JSON."
	cleanup_files
	exit 1
fi
if ! diff "${CONFTEST_SVR_RESULT_FILE}" "${CONFTEST_SVR_JSON_LOGFILE}" >/dev/null 2>&1; then
	PRNERR "Failed test configuration for server JSON."
	cleanup_files
	exit 1
fi
PRNSUCCEED "LOAD JSON file for server"

#
# LOAD JSON file for slave
#
PRNTITLE "LOAD JSON file for slave"

if ! "${CHMCONFTEST}" -conf "${CFG_TEST_SLV_JSON_FILE}" -no_check_update 2>&1 | grep -v NAME | grep -v DATE > "${CONFTEST_SLV_JSON_LOGFILE}" 2>&1; then
	PRNERR "Failed test configuration for slave JSON."
	cleanup_files
	exit 1
fi
if ! diff "${CONFTEST_SLV_RESULT_FILE}" "${CONFTEST_SLV_JSON_LOGFILE}" >/dev/null 2>&1; then
	PRNERR "Failed test configuration for slave JSON."
	cleanup_files
	exit 1
fi
PRNSUCCEED "LOAD JSON file for slave"

#
# LOAD JSON param for server
#
PRNTITLE "LOAD JSON param for server"

if ! "${CHMCONFTEST}" -json "${JSON_SERVER_PARAM}" -no_check_update 2>&1 | grep -v NAME | grep -v DATE > "${CONFTEST_SVR_SJSON_LOGFILE}" 2>&1; then
	PRNERR "Failed test configuration for server JSON param."
	cleanup_files
	exit 1
fi
if ! diff "${CONFTEST_SVR_RESULT_FILE}" "${CONFTEST_SVR_SJSON_LOGFILE}" >/dev/null 2>&1; then
	PRNERR "Failed test configuration for server JSON param."
	cleanup_files
	exit 1
fi
PRNSUCCEED "LOAD JSON param for server"

#
# LOAD JSON param for slave
#
PRNTITLE "LOAD JSON param for slave"

if ! "${CHMCONFTEST}" -json "${JSON_SLAVE_PARAM}" -no_check_update 2>&1 | grep -v NAME | grep -v DATE > "${CONFTEST_SLV_SJON_LOGFILE}" 2>&1; then
	PRNERR "Failed test configuration for slave JSON param."
	cleanup_files
	exit 1
fi
if ! diff "${CONFTEST_SLV_RESULT_FILE}" "${CONFTEST_SLV_SJON_LOGFILE}" >/dev/null 2>&1; then
	PRNERR "Failed test configuration for slave JSON param."
	cleanup_files
	exit 1
fi
PRNSUCCEED "LOAD JSON param for slave"

#
# LOAD INI conf by env for server
#
PRNTITLE "LOAD INI conf by env for server"

if ! CHMCONFFILE="${CFG_TEST_SVR_INI_FILE}" "${CHMCONFTEST}" -no_check_update 2>&1 | grep -v NAME | grep -v DATE > "${CONFTEST_SVR_INIENV_LOGFILE}" 2>&1; then
	PRNERR "Failed test configuration for server INI on ENV."
	cleanup_files
	exit 1
fi
if ! diff "${CONFTEST_SVR_RESULT_FILE}" "${CONFTEST_SVR_INIENV_LOGFILE}" >/dev/null 2>&1; then
	PRNERR "Failed test configuration for server INI on ENV."
	cleanup_files
	exit 1
fi
PRNSUCCEED "LOAD INI conf by env for server"

#
# LOAD INI conf by env for slave
#
PRNTITLE "LOAD INI conf by env for slave"

if ! CHMCONFFILE="${CFG_TEST_SLV_INI_FILE}" "${CHMCONFTEST}" -no_check_update 2>&1 | grep -v NAME | grep -v DATE > "${CONFTEST_SLV_INIENV_LOGFILE}" 2>&1; then
	PRNERR "Failed test configuration for slave INI on ENV."
	cleanup_files
	exit 1
fi
if ! diff "${CONFTEST_SLV_RESULT_FILE}" "${CONFTEST_SLV_INIENV_LOGFILE}" >/dev/null 2>&1; then
	PRNERR "Failed test configuration for slave INI on ENV."
	cleanup_files
	exit 1
fi
PRNSUCCEED "LOAD INI conf by env for slave"

#
# LOAD JSON param by env for server
#
PRNTITLE "LOAD JSON param by env for server"

if ! CHMJSONCONF="${JSON_SERVER_PARAM}" "${CHMCONFTEST}" -no_check_update 2>&1 | grep -v NAME | grep -v DATE > "${CONFTEST_SVR_JSONENV_LOGFILE}" 2>&1; then
	PRNERR "Failed test configuration for server JSON on ENV."
	cleanup_files
	exit 1
fi
if ! diff "${CONFTEST_SVR_RESULT_FILE}" "${CONFTEST_SVR_JSONENV_LOGFILE}" >/dev/null 2>&1; then
	PRNERR "Failed test configuration for server JSON on ENV."
	cleanup_files
	exit 1
fi
PRNSUCCEED "LOAD JSON param by env for server"

#
# LOAD JSON param by env for slave
#
PRNTITLE "LOAD JSON param by env for slave"

if ! CHMJSONCONF="${JSON_SLAVE_PARAM}" "${CHMCONFTEST}" -no_check_update 2>&1 | grep -v NAME | grep -v DATE > "${CONFTEST_SLV_SJONENV_LOGFILE}" 2>&1; then
	PRNERR "Failed test configuration for slave JSON on ENV."
	cleanup_files
	exit 1
fi
if ! diff "${CONFTEST_SLV_RESULT_FILE}" "${CONFTEST_SLV_SJONENV_LOGFILE}" >/dev/null 2>&1; then
	PRNERR "Failed test configuration for slave JSON on ENV."
	cleanup_files
	exit 1
fi
PRNSUCCEED "LOAD JSON param by env for slave"

#--------------------------------------------------------------
# Compare result
#--------------------------------------------------------------
#
# Server
#
PRNTITLE "Compare LOAD result for server"

if ! diff "${CONFTEST_SVR_INI_LOGFILE}" "${CONFTEST_SVR_YAML_LOGFILE}"; then
	PRNERR "Detected a difference ${CONFTEST_SVR_INI_LOGFILE} and ${CONFTEST_SVR_YAML_LOGFILE}"
	cleanup_files
	exit 1
fi

if ! diff "${CONFTEST_SVR_INI_LOGFILE}" "${CONFTEST_SVR_JSON_LOGFILE}"; then
	PRNERR "Detected a difference ${CONFTEST_SVR_INI_LOGFILE} and ${CONFTEST_SVR_JSON_LOGFILE}"
	cleanup_files
	exit 1
fi

if ! diff "${CONFTEST_SVR_INI_LOGFILE}" "${CONFTEST_SVR_SJSON_LOGFILE}"; then
	PRNERR "Detected a difference ${CONFTEST_SVR_INI_LOGFILE} and ${CONFTEST_SVR_SJSON_LOGFILE}"
	cleanup_files
	exit 1
fi

if ! diff "${CONFTEST_SVR_INI_LOGFILE}" "${CONFTEST_SVR_INIENV_LOGFILE}"; then
	PRNERR "Detected a difference ${CONFTEST_SVR_INI_LOGFILE} and ${CONFTEST_SVR_INIENV_LOGFILE}"
	cleanup_files
	exit 1
fi

if ! diff "${CONFTEST_SVR_INI_LOGFILE}" "${CONFTEST_SVR_JSONENV_LOGFILE}"; then
	PRNERR "Detected a difference ${CONFTEST_SVR_INI_LOGFILE} and ${CONFTEST_SVR_JSONENV_LOGFILE}"
	cleanup_files
	exit 1
fi
PRNSUCCEED "Compare LOAD result for server"

#
# Slave
#
PRNTITLE "Compare LOAD result for slave"

if ! diff "${CONFTEST_SLV_INI_LOGFILE}" "${CONFTEST_SLV_YAML_LOGFILE}"; then
	PRNERR "Detected a difference ${CONFTEST_SLV_INI_LOGFILE} and ${CONFTEST_SLV_YAML_LOGFILE}"
	cleanup_files
	exit 1
fi

if ! diff "${CONFTEST_SLV_INI_LOGFILE}" "${CONFTEST_SLV_JSON_LOGFILE}"; then
	PRNERR "Detected a difference ${CONFTEST_SLV_INI_LOGFILE} and ${CONFTEST_SLV_JSON_LOGFILE}"
	cleanup_files
	exit 1
fi

if ! diff "${CONFTEST_SLV_INI_LOGFILE}" "${CONFTEST_SLV_SJON_LOGFILE}"; then
	PRNERR "Detected a difference ${CONFTEST_SLV_INI_LOGFILE} and ${CONFTEST_SLV_SJON_LOGFILE}"
	cleanup_files
	exit 1
fi
PRNSUCCEED "Compare LOAD result for slave"

#==============================================================
# Start Process test
#==============================================================
PRNTITLE "Start Process test"

#
# chmpx for server node
#
PRNMSG "RUN chmpx server node"

"${CHMPXBIN}" -conf "${TEST_SVR_INI_FILE}" -d "${CHMPX_DBG_FLAG}" &
CHMPXSVRPID=$!
PRNINFO "chmpx server node pid = ${CHMPXSVRPID}"
sleep 2

#
# Test client process on server node
#
PRNMSG "RUN test server process on server side"

"${CHMPXBENCHBIN}" -s -f "${TEST_SVR_INI_FILE}" -l 0 -proccnt 1 -threadcnt 1 -ta -dl 128 -pr -g err -dummykey "${PROCID}" > "${TEST_SVR_PROC_LOGFILE}" 2>&1 &
TESTSVRPID=$!
PRNINFO "test client process pid(on server node) = ${TESTSVRPID}"
sleep 2

#
# chmpx for slave node
#
PRNMSG "RUN chmpx slave node"

"${CHMPXBIN}" -conf "${TEST_SLV_INI_FILE}" -d "${CHMPX_DBG_FLAG}" &
CHMPXSLVPID=$!
PRNINFO "chmpx slave node pid = ${CHMPXSLVPID}"
sleep 2

#
# Test client process on slave node
#
PRNMSG "RUN client process on slave node & START test"

if ! "${CHMPXBENCHBIN}" -c -f "${TEST_SLV_INI_FILE}" -l "${BENCH_COUNT}" -proccnt 1 -threadcnt 1 -ta -dl 128 -pr -g err -dummykey "${PROCID}" > "${TEST_SLV_PROC_LOGFILE}" 2>&1; then
	PRNERR "Failed to run chmpxbench, but need to stop processes so continue..."
	IS_FAILURE_STARTING_PROCS=1
else
	PRNINFO "Finish test client process on slave node"
	PRNSUCCEED "Start Process test"
	IS_FAILURE_STARTING_PROCS=0
	sleep 2
fi

#==============================================================
# Stop all processes
#==============================================================
PRNTITLE "Stop all processes"

IS_FAILURE_STOPPING_PROCS=0

# [NOTE]
# The order in which processes are stopped is important.
#
if [ -n "${CHMPXSLVPID}" ]; then
	if ! stop_process "${CHMPXSLVPID}"; then
		IS_FAILURE_STOPPING_PROCS=1
	fi
fi
if [ -n "${TESTSVRPID}" ]; then
	if ! stop_process "${TESTSVRPID}"; then
		IS_FAILURE_STOPPING_PROCS=1
	fi
fi
OTHER_CHMPXBENCH_PIDS=$(pgrep -a 'chmpxbench' | grep  '\-s' | awk '{print $1}')
if [ -n "${OTHER_CHMPXBENCH_PIDS}" ]; then
	if ! stop_process "${OTHER_CHMPXBENCH_PIDS}"; then
		IS_FAILURE_STOPPING_PROCS=1
	fi
fi
if [ -n "${CHMPXSVRPID}" ]; then
	if ! stop_process "${CHMPXSVRPID}"; then
		IS_FAILURE_STOPPING_PROCS=1
	fi
fi

if [ "${IS_FAILURE_STOPPING_PROCS}" -ne 0 ]; then
	#
	# Retry to stop
	#
	PRNWARN "Failed to stop all processes, retry to force stop."
	kill -KILL "${TESTSVRPID}" "${TESTCLIENTPIDS}" "${CHMPXSLVPID}" "${CHMPXSVRPID}" >/dev/null 2>&1

	#
	# Re-check processes
	#
	sleep 2
	IS_FAILURE_STOPPING_PROCS=0

	# shellcheck disable=SC2009
	if ( ps -o pid,stat ax 2>/dev/null | grep -v 'PID' | awk '$2~/^[^Z]/ { print $1 }' | grep -q "^${TESTSVRPID}$" || exit 1 && exit 0 ); then
		IS_FAILURE_STOPPING_PROCS=1
	fi

	# shellcheck disable=SC2009
	if ( ps -o pid,stat ax 2>/dev/null | grep -v 'PID' | awk '$2~/^[^Z]/ { print $1 }' | grep -q "^${CHMPXSLVPID}$" || exit 1 && exit 0 ); then
		IS_FAILURE_STOPPING_PROCS=1
	fi

	# shellcheck disable=SC2009
	if ( ps -o pid,stat ax 2>/dev/null | grep -v 'PID' | awk '$2~/^[^Z]/ { print $1 }' | grep -q "^${CHMPXSVRPID}$" || exit 1 && exit 0 ); then
		IS_FAILURE_STOPPING_PROCS=1
	fi

	for _ONE_PID in ${TESTCLIENTPIDS}; do
		# shellcheck disable=SC2009
		if ( ps -o pid,stat ax 2>/dev/null | grep -v 'PID' | awk '$2~/^[^Z]/ { print $1 }' | grep -q "^${_ONE_PID}$" || exit 1 && exit 0 ); then
			IS_FAILURE_STOPPING_PROCS=1
		fi
	done

	if [ "${IS_FAILURE_STOPPING_PROCS}" -ne 0 ]; then
		PRNERR "Failed to stop all processes, but contiune..."
	else
		PRNSUCCEED "Stop all processes"
	fi
else
	PRNSUCCEED "Stop all processes"
fi

#==============================================================
# Confirmation of communication results
#==============================================================
PRNTITLE "Confirmation of communication results"

SVRRESULT=$(grep -v "^Receive : " "${TEST_SVR_PROC_LOGFILE}")
SLVRESULTERRCNT=$(grep 'Total error count' "${TEST_SLV_PROC_LOGFILE}" | awk '{print $4}')

if [ -n "${SVRRESULT}" ] || [ "${SLVRESULTERRCNT}" != "0" ]; then
	PRNERR "Failed to send/receive data, but contiune..."
	IS_FAILURE_COMMUNICATION=1
else
	PRNSUCCEED "Confirmation of communication results"
	IS_FAILURE_COMMUNICATION=0
fi

#==============================================================
# Finish
#==============================================================
#
# Cleanup files
#
cleanup_files

#
# Result
#
PRNTITLE "Test summary"

RESULT_CODE=0
if [ "${IS_FAILURE_STARTING_PROCS}" -eq 1 ] || [ "${IS_FAILURE_STOPPING_PROCS}" -eq 1 ] || [ "${IS_FAILURE_COMMUNICATION}" -eq 1 ]; then
	PRNERR "The test summary is a Failure"
	RESULT_CODE=1
else
	PRNSUCCEED "The test summary is a Success"
fi

exit "${RESULT_CODE}"

#
# Local variables:
# tab-width: 4
# c-basic-offset: 4
# End:
# vim600: noexpandtab sw=4 ts=4 fdm=marker
# vim<600: noexpandtab sw=4 ts=4
#
