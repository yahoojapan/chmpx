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
# hashing and are automatically layouted. As a result, it
# provides a high performance, a high scalability.
#
# For the full copyright and license information, please view
# the license file that was distributed with this source code.
#
# AUTHOR:   Takeshi Nakatani
# CREATE:   Fri May 8 2015
# REVISION:
#

##############################################################
## library path & programs path
##
MYSCRIPTDIR=`dirname $0`
if [ "X${SRCTOP}" = "X" ]; then
	SRCTOP=`cd ${MYSCRIPTDIR}/..; pwd`
fi
cd ${MYSCRIPTDIR}
if [ "X${OBJDIR}" = "X" ]; then
	LD_LIBRARY_PATH="${SRCTOP}/lib/.lib"
	TESTPROGDIR=${MYSCRIPTDIR}
else
	LD_LIBRARY_PATH="${SRCTOP}/lib/${OBJDIR}"
	TESTPROGDIR=${MYSCRIPTDIR}/${OBJDIR}
fi
export LD_LIBRARY_PATH
export TESTPROGDIR

###########################################################
# Initialize
###########################################################
DATE=`date`
PROCID=$$
COUNT=10

#
# Base directory
#
RUNDIR=`pwd`
TESTSCRIPTDIR=`dirname $0`
if [ "X$TESTSCRIPTDIR" = "X" -o "X$TESTSCRIPTDIR" = "X." ]; then
	TMP_BASENAME=`basename $0`
	TMP_FIRSTWORD=`echo $0 | awk -F"/" '{print $1}'`

	if [ "X$TMP_BASENAME" = "X$TMP_FIRSTWORD" ]; then
		# search path
		TESTSCRIPTDIR=`which $0`
		TESTSCRIPTDIR=`dirname $TESTSCRIPTDIR`
	else
		TESTSCRIPTDIR=.
	fi
fi
BASEDIR=`cd -P ${RUNDIR}/${TESTSCRIPTDIR}; pwd`
CHMPXDIR=`cd -P ${BASEDIR}/../src; pwd`

#
# Binary
#
if [ -f ${CHMPXDIR}/${PLATFORM_CURRENT}/chmpx ]; then
	CHMPXBIN=${CHMPXDIR}/${PLATFORM_CURRENT}/chmpx
elif [ -f ${CHMPXDIR}/${PLATFORM_CURRENT}/chmmain ]; then
	CHMPXBIN=${CHMPXDIR}/${PLATFORM_CURRENT}/chmmain
else
	echo "ERROR: there is no chmpx binary"
	echo "RESULT --> FAILED"
	exit 1
fi
if [ -f ${BASEDIR}/${PLATFORM_CURRENT}/chmpxbench ]; then
	CHMPXBENCHBIN=${BASEDIR}/${PLATFORM_CURRENT}/chmpxbench
else
	echo "ERROR: there is no chmpxbench binary"
	echo "RESULT --> FAILED"
	exit 1
fi
if [ -f ${BASEDIR}/${PLATFORM_CURRENT}/chmpxconftest ]; then
	CHMCONFTEST=${BASEDIR}/${PLATFORM_CURRENT}/chmpxconftest
else
	echo "ERROR: there is no chmpxconftest binary"
	echo "RESULT --> FAILED"
	exit 1
fi

###########################################################
# Start configuration test
###########################################################
#
# make parameter for JSON
#
JSON_SERVER_PARAM=`grep 'SERVER=' ${BASEDIR}/test_json_string.data 2>/dev/null | sed 's/SERVER=//g' 2>/dev/null`
JSON_SLAVE_PARAM=`grep 'SLAVE=' ${BASEDIR}/test_json_string.data 2>/dev/null | sed 's/SLAVE=//g' 2>/dev/null`

#
# Test for conf loading
#
echo "------ LOAD INI file for server ----------------------------"
${CHMCONFTEST} -conf ${BASEDIR}/test_server.ini | grep -v NAME | grep -v DATE > /tmp/conftest_svr_ini_${PROCID}.log 2>&1
if [ $? -ne 0 ]; then
	echo "ERROR: Failed test configuration for server INI."
	echo "RESULT --> FAILED"
	exit 1
fi
echo "RESULT --> SUCCEED"
echo ""

echo "------ LOAD INI file for slave -----------------------------"
${CHMCONFTEST} -conf ${BASEDIR}/test_slave.ini | grep -v NAME | grep -v DATE > /tmp/conftest_slv_ini_${PROCID}.log 2>&1
if [ $? -ne 0 ]; then
	echo "ERROR: Failed test configuration for slave INI."
	echo "RESULT --> FAILED"
	exit 1
fi
echo "RESULT --> SUCCEED"
echo ""

echo "------ LOAD YAML file for server ---------------------------"
${CHMCONFTEST} -conf ${BASEDIR}/test_server.yaml | grep -v NAME | grep -v DATE > /tmp/conftest_svr_yaml_${PROCID}.log 2>&1
if [ $? -ne 0 ]; then
	echo "ERROR: Failed test configuration for server YAML."
	echo "RESULT --> FAILED"
	exit 1
fi
echo "RESULT --> SUCCEED"
echo ""

echo "------ LOAD YAML file for slave ----------------------------"
${CHMCONFTEST} -conf ${BASEDIR}/test_slave.yaml | grep -v NAME | grep -v DATE > /tmp/conftest_slv_yaml_${PROCID}.log 2>&1
if [ $? -ne 0 ]; then
	echo "ERROR: Failed test configuration for slave YAML."
	echo "RESULT --> FAILED"
	exit 1
fi
echo "RESULT --> SUCCEED"
echo ""

echo "------ LOAD JSON file for server ---------------------------"
${CHMCONFTEST} -conf ${BASEDIR}/test_server.json | grep -v NAME | grep -v DATE > /tmp/conftest_svr_json_${PROCID}.log 2>&1
if [ $? -ne 0 ]; then
	echo "ERROR: Failed test configuration for server JSON."
	echo "RESULT --> FAILED"
	exit 1
fi
echo "RESULT --> SUCCEED"
echo ""

echo "------ LOAD JSON file for slave ----------------------------"
${CHMCONFTEST} -conf ${BASEDIR}/test_slave.json | grep -v NAME | grep -v DATE > /tmp/conftest_slv_json_${PROCID}.log 2>&1
if [ $? -ne 0 ]; then
	echo "ERROR: Failed test configuration for slave JSON."
	echo "RESULT --> FAILED"
	exit 1
fi
echo "RESULT --> SUCCEED"
echo ""

echo "------ LOAD JSON param for server --------------------------"
${CHMCONFTEST} -json "${JSON_SERVER_PARAM}" | grep -v NAME | grep -v DATE > /tmp/conftest_svr_sjson_${PROCID}.log 2>&1
if [ $? -ne 0 ]; then
	echo "ERROR: Failed test configuration for server JSON param."
	echo "RESULT --> FAILED"
	exit 1
fi
echo "RESULT --> SUCCEED"
echo ""

echo "------ LOAD JSON param for slave ---------------------------"
${CHMCONFTEST} -json "${JSON_SLAVE_PARAM}" | grep -v NAME | grep -v DATE > /tmp/conftest_slv_sjson_${PROCID}.log 2>&1
if [ $? -ne 0 ]; then
	echo "ERROR: Failed test configuration for slave JSON param."
	echo "RESULT --> FAILED"
	exit 1
fi
echo "RESULT --> SUCCEED"
echo ""

echo "------ LOAD INI conf by env for server ---------------------"
CHMCONFFILE=${BASEDIR}/test_server.ini ${CHMCONFTEST} | grep -v NAME | grep -v DATE > /tmp/conftest_svr_inienv_${PROCID}.log 2>&1
if [ $? -ne 0 ]; then
	echo "ERROR: Failed test configuration for server INI on ENV."
	echo "RESULT --> FAILED"
	exit 1
fi
echo "RESULT --> SUCCEED"
echo ""

echo "------ LOAD JSON param by env for server ------------------"
CHMJSONCONF="${JSON_SERVER_PARAM}" ${CHMCONFTEST} | grep -v NAME | grep -v DATE > /tmp/conftest_svr_jsonenv_${PROCID}.log 2>&1
if [ $? -ne 0 ]; then
	echo "ERROR: Failed test configuration for server JSON on ENV."
	echo "RESULT --> FAILED"
	exit 1
fi
echo "RESULT --> SUCCEED"
echo ""

#
# Compare result
#
echo "------ Compare LOAD result for server ----------------------"
diff /tmp/conftest_svr_ini_${PROCID}.log /tmp/conftest_svr_yaml_${PROCID}.log
if [ $? -ne 0 ]; then
	echo "RESULT --> FAILED"
	exit 1
fi

diff /tmp/conftest_svr_ini_${PROCID}.log /tmp/conftest_svr_json_${PROCID}.log
if [ $? -ne 0 ]; then
	echo "RESULT --> FAILED"
	exit 1
fi

diff /tmp/conftest_svr_ini_${PROCID}.log /tmp/conftest_svr_sjson_${PROCID}.log
if [ $? -ne 0 ]; then
	echo "RESULT --> FAILED"
	exit 1
fi

diff /tmp/conftest_svr_ini_${PROCID}.log /tmp/conftest_svr_inienv_${PROCID}.log
if [ $? -ne 0 ]; then
	echo "RESULT --> FAILED"
	exit 1
fi

diff /tmp/conftest_svr_ini_${PROCID}.log /tmp/conftest_svr_jsonenv_${PROCID}.log
if [ $? -ne 0 ]; then
	echo "RESULT --> FAILED"
	exit 1
fi
echo "RESULT --> SUCCEED"
echo ""

echo "------ Compare LOAD result for slave -----------------------"
diff /tmp/conftest_slv_ini_${PROCID}.log /tmp/conftest_slv_yaml_${PROCID}.log
if [ $? -ne 0 ]; then
	echo "RESULT --> FAILED"
	exit 1
fi

diff /tmp/conftest_slv_ini_${PROCID}.log /tmp/conftest_slv_json_${PROCID}.log
if [ $? -ne 0 ]; then
	echo "RESULT --> FAILED"
	exit 1
fi

diff /tmp/conftest_slv_ini_${PROCID}.log /tmp/conftest_slv_sjson_${PROCID}.log
if [ $? -ne 0 ]; then
	echo "RESULT --> FAILED"
	exit 1
fi
echo "RESULT --> SUCCEED"
echo ""

###########################################################
# Start test
###########################################################
#
# chmpx for server node
#
echo "------ RUN chmpx server node -------------------------------"
${CHMPXBIN} -conf ${BASEDIR}/test_server.ini -d silent &
CHMPXSVRPID=$!
echo "chmpx server node pid = ${CHMPXSVRPID}"
sleep 2

#
# test client process on server node
#
echo "------ RUN test server process on server side --------------"
${CHMPXBENCHBIN} -s -f ${BASEDIR}/test_server.ini -l 0 -proccnt 1 -threadcnt 1 -ta -dl 128 -pr -g err -dummykey ${PROCID} > /tmp/testsvr_${PROCID}.log 2>&1 &
TESTSVRPID=$!
echo "test client process pid(on server node) = ${TESTSVRPID}"
sleep 2

#
# server chmpx service in
#
echo "------ SET chmpx mode to SERVICEIN -------------------------"
(sleep 1; echo SERVICEIN) | telnet localhost 8021
echo "chmpx server node status to SERVICEIN"
sleep 2

#
# chmpx for slave node
#
echo "------ RUN chmpx slave node --------------------------------"
${CHMPXBIN} -conf ${BASEDIR}/test_slave.ini -d silent &
CHMPXSLVPID=$!
echo "chmpx slave node pid = ${CHMPXSLVPID}"
sleep 2

#
# test client process on slave node
#
echo "------ RUN client process on slave node & START test -------"
${CHMPXBENCHBIN} -c -f ${BASEDIR}/test_slave.ini -l ${COUNT} -proccnt 1 -threadcnt 1 -ta -dl 128 -pr -g err -dummykey ${PROCID} > /tmp/testslv_${PROCID}.log 2>&1
echo "finish test client process on slave node"
sleep 2

###########################################################
# Stop all process
###########################################################
kill -HUP ${TESTSVRPID} > /dev/null 2>&1
sleep 1
kill -9 ${TESTSVRPID} > /dev/null 2>&1
sleep 1

TESTCLIENTPIDS=`ps w | grep chmpxbench | grep dummykey | grep ${PROCID} | grep -v grep | awk '{print $1}'`
if [ "X$TESTCLIENTPIDS" != "X" ]; then
	sleep 1
	kill -HUP ${TESTCLIENTPIDS} > /dev/null 2>&1
	sleep 1
	kill -9 ${TESTCLIENTPIDS} > /dev/null 2>&1
fi

kill -HUP ${CHMPXSLVPID} > /dev/null 2>&1
sleep 1
kill -HUP ${CHMPXSVRPID} > /dev/null 2>&1
sleep 1
kill -9 ${CHMPXSLVPID} ${CHMPXSVRPID} > /dev/null 2>&1

##############
# Check
##############
echo "------ RESULT ----------------------------------------------"
SVRRESULT=`cat /tmp/testsvr_${PROCID}.log`
SLVRESULTERRLINE=`grep 'Total error count' /tmp/testslv_${PROCID}.log`
SLVRESULTERRCNT=`echo $SLVRESULTERRLINE | awk '{print $4}'`

RESULT=0
if [ "X$SVRRESULT" != "X" -o "X$SLVRESULTERRLINE" = "X" -o "X$SLVRESULTERRCNT" != "X0" ]; then
	RESULT=1
fi

#
# Cleanup files
#
rm -f /tmp/conftest_svr_ini_${PROCID}.log
rm -f /tmp/conftest_svr_yaml_${PROCID}.log
rm -f /tmp/conftest_svr_json_${PROCID}.log
rm -f /tmp/conftest_svr_sjson_${PROCID}.log
rm -f /tmp/conftest_svr_inienv_${PROCID}.log
rm -f /tmp/conftest_svr_jsonenv_${PROCID}.log
rm -f /tmp/conftest_slv_ini_${PROCID}.log
rm -f /tmp/conftest_slv_yaml_${PROCID}.log
rm -f /tmp/conftest_slv_json_${PROCID}.log
rm -f /tmp/conftest_slv_sjson_${PROCID}.log
rm -f /tmp/testsvr_${PROCID}.log
rm -f /tmp/testslv_${PROCID}.log
rm -f /tmp/TESTSCRPT-8021.chmpx
rm -f /tmp/TESTSCRPT-8021.k2h
rm -f /tmp/TESTSCRPT-8022.chmpx
rm -f /tmp/TESTSCRPT-8022.k2h
rm -f /tmp/.k2h_*
rm -f /tmp/chmpxbench-*.dat

if [ $RESULT -eq 1 ]; then
	echo "RESULT --> FAILED"
else
	echo "RESULT --> SUCCEED"
fi
exit $RESULT

#
# VIM modelines
#
# vim:set ts=4 fenc=utf-8:
#
