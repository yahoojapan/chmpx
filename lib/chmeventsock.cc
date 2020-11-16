/*
 * CHMPX
 *
 * Copyright 2014 Yahoo Japan Corporation.
 *
 * CHMPX is inprocess data exchange by MQ with consistent hashing.
 * CHMPX is made for the purpose of the construction of
 * original messaging system and the offer of the client
 * library.
 * CHMPX transfers messages between the client and the server/
 * slave. CHMPX based servers are dispersed by consistent
 * hashing and are automatically laid out. As a result, it
 * provides a high performance, a high scalability.
 *
 * For the full copyright and license information, please view
 * the license file that was distributed with this source code.
 *
 * AUTHOR:   Takeshi Nakatani
 * CREATE:   Tue July 1 2014
 * REVISION:
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <poll.h>
#include <time.h>
#include <assert.h>

#include "chmcommon.h"
#include "chmeventsock.h"
#include "chmcomstructure.h"
#include "chmstructure.tcc"
#include "chmcntrl.h"
#include "chmsigcntrl.h"
#include "chmnetdb.h"
#include "chmlock.h"
#include "chmutil.h"
#include "chmdbg.h"

using namespace	std;

//---------------------------------------------------------
// Symbols
//---------------------------------------------------------
// Environment name for communication backward compatibility
#define	CHMPX_COMPROTO_VERSION				"CHMPX_COM_PROTO_VER"

#define	CHMEVSOCK_MERGE_THREAD_NAME			"ChmEventSock-Merge"
#define	CHMEVSOCK_SOCK_THREAD_NAME			"ChmEventSock-Socket"

// Control Commands
#define	CTL_COMMAND_MAX_LENGTH				8192
#define	CTL_RECEIVE_MAX_LENGTH				(CTL_COMMAND_MAX_LENGTH * 2)

#define	CTL_COMMAND_VERSION					"VERSION"
#define	CTL_COMMAND_DUMP_IMDATA				"DUMP"
#define	CTL_COMMAND_START_MERGE				"MERGE"
#define	CTL_COMMAND_STOP_MERGE				"ABORTMERGE"
#define	CTL_COMMAND_COMPLETE_MERGE			"COMPMERGE"
#define	CTL_COMMAND_SUSPEND_MERGE			"SUSPENDMERGE"
#define	CTL_COMMAND_NOSUP_MERGE				"NOSUSPENDMERGE"
#define	CTL_COMMAND_SERVICE_IN				"SERVICEIN"
#define	CTL_COMMAND_SERVICE_OUT				"SERVICEOUT"
#define	CTL_COMMAND_SELF_STATUS				"SELFSTATUS"
#define	CTL_COMMAND_ALLSVR_STATUS			"ALLSTATUS"
#define	CTL_COMMAND_UPDATE_STATUS			"UPDATESTATUS"
#define	CTL_COMMAND_TRACE_SET				"TRACE"
#define	CTL_COMMAND_TRACE_VIEW				"TRACEVIEW"

#define	CTL_COMMAND_TRACE_SET_ENABLE1		"ENABLE"
#define	CTL_COMMAND_TRACE_SET_ENABLE2		"YES"
#define	CTL_COMMAND_TRACE_SET_ENABLE3		"ON"
#define	CTL_COMMAND_TRACE_SET_DISABLE1		"DISABLE"
#define	CTL_COMMAND_TRACE_SET_DISABLE2		"NO"
#define	CTL_COMMAND_TRACE_SET_DISABLE3		"OFF"
#define	CTL_COMMAND_TRACE_VIEW_DIR			"DIR="
#define	CTL_COMMAND_TRACE_VIEW_DEV			"DEV="
#define	CTL_COMMAND_TRACE_VIEW_ALL			"ALL"
#define	CTL_COMMAND_TRACE_VIEW_IN			"IN"
#define	CTL_COMMAND_TRACE_VIEW_OUT			"OUT"
#define	CTL_COMMAND_TRACE_VIEW_SOCK			"SOCK"
#define	CTL_COMMAND_TRACE_VIEW_MQ			"MQ"

#define	CTL_RES_SUCCESS						"SUCCEED\n"
#define	CTL_RES_SUCCESS_NOSERVER			"SUCCEED: There no server on RING.\n"
#define	CTL_RES_SUCCESS_STATUS_NOTICE		"SUCCEED: Send status notice to no server on RING.\n"
#define	CTL_RES_SUCCESS_ALREADY_SERVICEOUT	"SUCCEED: Already server has been SERVICEOUT.\n"
#define	CTL_RES_ERROR						"ERROR: Something error is occured.\n"
#define	CTL_RES_ERROR_PARAMETER				"ERROR: Parameters are wrong.\n"
#define	CTL_RES_ERROR_COMMUNICATION			"ERROR: Something error is occured in sending/receiving data on RING.\n"
#define	CTL_RES_ERROR_COMMAND_ON_SERVER		"ERROR: The command can not be executed on the slave node.\n"
#define	CTL_RES_ERROR_SERVICE_OUT_PARAM		"ERROR: SERVICEOUT command must have parameter as server/ctlport.(ex: \"SERVICEOUT servername.fqdn:ctlport\")\n"
#define	CTL_RES_ERROR_MERGE_START			"ERROR: Failed to start merging.\n"
#define	CTL_RES_ERROR_MERGE_ABORT			"ERROR: Failed to stop merging.\n"
#define	CTL_RES_ERROR_MERGE_COMPLETE		"ERROR: Failed to complete merging.\n"
#define	CTL_RES_ERROR_MERGE_AUTO			"ERROR: Failed to start merging or complete merging.\n"
#define	CTL_RES_ERROR_NOT_FOUND_SVR			"ERROR: There is no such as server.\n"
#define	CTL_RES_ERROR_NOT_SERVERMODE		"ERROR: Command must run on server mode.\n"
#define	CTL_RES_ERROR_NOT_SRVIN				"ERROR: Server is not \"service in\" on RING.\n"
#define	CTL_RES_ERROR_OPERATING				"ERROR: Could not change status, because server is operating now.\n"
#define	CTL_RES_ERROR_STATUS_NOT_ALLOWED	"ERROR: Could not change status, because status is not allowed.\n"
#define	CTL_RES_ERROR_SOME_SERVER_STATUS	"ERROR: Could not change status, because some servers could not be changed its status now.\n"
#define	CTL_RES_ERROR_COULD_NOT_SUSPEND		"ERROR: Could not suspend merge automatically.\n"
#define	CTL_RES_ERROR_COULD_NOT_NOSUSPEND	"ERROR: Could not nosuspend merge automatically.\n"
#define	CTL_RES_ERROR_COULD_NOT_SERVICE_OUT	"ERROR: Server could not set SERVICE OUT on RING.\n"
#define	CTL_RES_ERROR_NOT_CHANGE_STATUS		"ERROR: Server can not be changed status.\n"
#define	CTL_RES_ERROR_CHANGE_STATUS			"ERROR: Failed to change server status.\n"
#define	CTL_RES_ERROR_TRANSFER				"ERROR: Failed to transfer command to other target server, something error occured on target server.\n"
#define	CTL_RES_ERROR_STATUS_NOTICE			"ERROR: Failed to send status notice to other servers.\n"
#define	CTL_RES_ERROR_GET_CHMPXSVR			"ERROR: Failed to get all information.\n"
#define	CTL_RES_ERROR_GET_STAT				"ERROR: Failed to get all stat.\n"
#define	CTL_RES_ERROR_UPDATE_STATUS_SENDERR	"ERROR: Could not send self status to other servers in RING.\n"
#define	CTL_RES_ERROR_UPDATE_STATUS_INTERR	"ERROR: Internal error is occured for UPDATESTATUS.\n"
#define	CTL_RES_ERROR_TRACE_SET_PARAM		"ERROR: TRACE command must have parameter as enable/disable.(ex: \"TRACE enable\")\n"
#define	CTL_RES_ERROR_TRACE_SET_ALREADY		"ERROR: TRACE is already enabled/disabled.\n"
#define	CTL_RES_ERROR_TRACE_SET_FAILED		"ERROR: Failed to set TRACE enable/disable.\n"
#define	CTL_RES_ERROR_TRACE_VIEW_PARAM		"ERROR: TRACEVIEW command parameter is \"TRACEVIEW [DIR=IN/OUT/ALL] [DEV=SOCK/MQ/ALL] [COUNT]\"\n"
#define	CTL_RES_ERROR_TRACE_VIEW_NOTRACE	"ERROR: Now trace count is 0.\n"
#define	CTL_RES_ERROR_TRACE_VIEW_NODATA		"ERROR: There is no matched trace log now.\n"
#define	CTL_RES_ERROR_TRACE_VIEW_INTERR		"ERROR: Internal error is occured for TRACEVIEW.\n"
#define	CTL_RES_INT_ERROR					"INTERNAL ERROR: Something error is occured.\n"
#define	CTL_RES_INT_ERROR_NOTGETCHMPX		"INTERNAL ERROR: Could not get chmpx servers information.\n"

#define	CTLCOM_SERVICEOUT_ERRMSG			\
	"%s command format error.\n" \
	"Specify in one of the following formats depending on the value of CHMPXIDTYPE:\n" \
	"CHMPXIDTYPE=NAME         <servername>:<control port>[:<cuk>]\n" \
	"CHMPXIDTYPE=CUK          <servername>:<control port>:<cuk>\n" \
	"CHMPXIDTYPE=CTLENDPOINTS <servername>:<control port>:<control port endpoints(*1)>\n" \
	"CHMPXIDTYPE=CUSTOM       <servername>:<control port>:<custom id seed>\n" \
	"(*1) Specify the description of control port endpoints in the same format as configuration."

//---------------------------------------------------------
// Utilities
//---------------------------------------------------------
static bool get_comproto_version_env(comver_t& comver)
{
	string	value;
	if(!k2h_getenv(CHMPX_COMPROTO_VERSION, value) || value.empty()){
		return false;
	}
	comver_t	result = 0;
	if(!cvt_string_to_number_raw(value.c_str(), &result, true)){
		return false;
	}
	if(COM_VERSION_1 != result){
		return false;
	}
	comver = COM_VERSION_1;
	return true;
}

// [NOTE]
// STRPXCOMTYPE() and DUMPCOM_COMPKT_TYPE() were originally macros defined in
// chmcomstructure.h.
// STRPXCOMTYPE() was a macro that returned a string similar to the
// SIZEOF_CHMPX_COM() macro. However, since "literalWithCharPtrCompare" warning
// in only STRPXCOMTYPE() is output by cppcheck checking, it is moved to this
// file and changed to an inline function.
// DUMPCOM_COMPKT_TYPE() is a macro that calls STRPXCOMTYPE() internally, so it
// is moved to this file as well.
//
static const char*	str_pxcom_head_type[] = {
	"CHMPX_COM_UNKNOWN",
	"CHMPX_COM_STATUS_REQ",
	"CHMPX_COM_STATUS_RES",
	"CHMPX_COM_CONINIT_REQ",
	"CHMPX_COM_CONINIT_RES",
	"CHMPX_COM_JOIN_RING",
	"CHMPX_COM_STATUS_UPDATE",
	"CHMPX_COM_STATUS_CONFIRM",
	"CHMPX_COM_STATUS_CHANGE",
	"CHMPX_COM_MERGE_START",
	"CHMPX_COM_MERGE_ABORT",
	"CHMPX_COM_MERGE_COMPLETE",
	"CHMPX_COM_SERVER_DOWN",
	"CHMPX_COM_REQ_UPDATEDATA",
	"CHMPX_COM_RES_UPDATEDATA",
	"CHMPX_COM_RESULT_UPDATEDATA",
	"CHMPX_COM_MERGE_SUSPEND",
	"CHMPX_COM_MERGE_NOSUSPEND",
	"CHMPX_COM_MERGE_SUSPEND_GET",
	"CHMPX_COM_MERGE_SUSPEND_RES",
	"CHMPX_COM_VERSION_REQ",
	"CHMPX_COM_VERSION_RES",
	"CHMPX_COM_N2_STATUS_REQ",
	"CHMPX_COM_N2_STATUS_RES",
	"CHMPX_COM_N2_CONINIT_REQ",
	"CHMPX_COM_N2_CONINIT_RES",
	"CHMPX_COM_N2_JOIN_RING",
	"CHMPX_COM_N2_STATUS_UPDATE",
	"CHMPX_COM_N2_STATUS_CONFIRM",
	"CHMPX_COM_N2_STATUS_CHANGE"
};

static inline const char* STRPXCOMTYPE(pxcomtype_t type)
{
	if(static_cast<pxcomtype_t>(sizeof(str_pxcom_head_type) / sizeof(const char*)) <= type){
		return "NOT_DEFINED_TYPE";
	}
	return str_pxcom_head_type[type];
}

static inline void DUMPCOM_COMPKT_TYPE(const char* action, PCOMPKT pComPkt)
{
	if(!action || !pComPkt){
		ERR_CHMPRN("Parameters are wrong.");
	}

	if(chm_debug_mode >= CHMDBG_DUMP){
		if(COM_PX2PX == pComPkt->head.type){
			PPXCOM_ALL	pComAll = CVT_COM_ALL_PTR_PXCOMPKT(pComPkt);
			MSG_CHMPRN("%s COMPKT type(%s) : PXCOM_ALL type(%s).", (CHMEMPTYSTR(action) ? "" : action), STRCOMTYPE(pComPkt->head.type), STRPXCOMTYPE(pComAll->val_head.type));
		}else if(COM_C2C == pComPkt->head.type){ \
			MSG_CHMPRN("%s COMPKT type(%s) : COM_C2C type(%s).", (CHMEMPTYSTR(action) ? "" : action), STRCOMTYPE(pComPkt->head.type), STRCOMC2CTYPE(pComPkt->head.c2ctype));
		}else{
			MSG_CHMPRN("%s COMPKT type(%s).", (CHMEMPTYSTR(action) ? "" : action), STRCOMTYPE(pComPkt->head.type));
		}
	}
}

//---------------------------------------------------------
// Class variables
//---------------------------------------------------------
const int			ChmEventSock::DEFAULT_SOCK_THREAD_CNT;
const int			ChmEventSock::DEFAULT_MAX_SOCK_POOL;
const time_t		ChmEventSock::DEFAULT_SOCK_POOL_TIMEOUT;
const time_t		ChmEventSock::NO_SOCK_POOL_TIMEOUT;
const int			ChmEventSock::DEFAULT_KEEPIDLE;
const int			ChmEventSock::DEFAULT_KEEPINTERVAL;
const int			ChmEventSock::DEFAULT_KEEPCOUNT;
const int			ChmEventSock::DEFAULT_LISTEN_BACKLOG;
const int			ChmEventSock::DEFAULT_RETRYCNT;
const int			ChmEventSock::DEFAULT_RETRYCNT_CONNECT;
const suseconds_t	ChmEventSock::DEFAULT_WAIT_SOCKET;
const suseconds_t	ChmEventSock::DEFAULT_WAIT_CONNECT;

//---------------------------------------------------------
// Class Methods
//---------------------------------------------------------
void ChmEventSock::AllowSelfSignedCert(void)
{
	ChmSecureSock::AllowSelfSignedCert();
}

void ChmEventSock::DenySelfSignedCert(void)
{
	ChmSecureSock::DenySelfSignedCert();
}

bool ChmEventSock::SetNonblocking(int sock)
{
	if(CHM_INVALID_SOCK == sock){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	int	flags = fcntl(sock, F_GETFL, 0);
	if(0 != (flags & O_NONBLOCK)){
		//MSG_CHMPRN("sock(%d) already set nonblocking.", sock);
		return true;
	}
	if(-1 == fcntl(sock, F_SETFL, flags | O_NONBLOCK)){
		ERR_CHMPRN("Could not set nonblocking flag to sock(%d), errno=%d.", sock, errno);
		return false;
	}
	return true;
}

int ChmEventSock::WaitForReady(int sock, int type, int retrycnt, bool is_check_so_error, suseconds_t waittime)
{
	// [NOTE] For performance
	// signal mask does not change after launching processes.
	// (because it is static member in ChmSignalCntrl)
	// So, we duplicate it's value as static value in this method.
	// And we do not lock at initializing this value.
	//
	static sigset_t	sigset;
	static bool		is_sigset_init = false;
	if(!is_sigset_init){
		ChmSigCntrl	sigcntrl;
		if(!sigcntrl.GetSignalMask(sigset)){
			ERR_CHMPRN("Could not get signal mask value.");
			return EPERM;
		}
		is_sigset_init = true;
	}

	if(CHM_INVALID_SOCK == sock || !ISSAFE_WAIT_FD(type) || (CHMEVENTSOCK_RETRY_DEFAULT != retrycnt && retrycnt < 0)){
		ERR_CHMPRN("Parameters are wrong.");
		return EPERM;
	}

	if(CHMEVENTSOCK_RETRY_DEFAULT == retrycnt){
		retrycnt = ChmEventSock::DEFAULT_RETRYCNT;
		waittime = ChmEventSock::DEFAULT_WAIT_SOCKET;
	}

	// setup fds for ppoll
	struct pollfd	fds;
	{
		fds.fd		= sock;
		fds.events	= (IS_WAIT_READ_FD(type) ? (POLLIN | POLLPRI | POLLRDHUP) : 0) | (IS_WAIT_WRITE_FD(type) ? (POLLOUT | POLLRDHUP) : 0);
		fds.revents	= 0;
	}
	struct timespec	ts;
	int				cnt;

	for(cnt = 0; cnt < (retrycnt + 1); cnt++){
		SET_TIMESPEC(&ts, 0, (waittime * 1000));	// if waittime is default(CHMEVENTSOCK_TIMEOUT_DEFAULT), no sleeping.

		// check ready
		int	rtcode = ppoll(&fds, 1, &ts, &sigset);
		if(-1 == rtcode){
			if(EINTR == errno){
				//MSG_CHMPRN("Waiting connected sock(%d) result is -1(and EINTR), try again.", sock);
				continue;
			}
			// something error
			MSG_CHMPRN("Failed to wait fd up for sock=%d by errno=%d.", sock, errno);
			return errno;

		}else if(0 != rtcode){
			if(is_check_so_error){
				int			conerr = 0;
				socklen_t	length = sizeof(int);
				if(0 > getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<void*>(&conerr), &length)){
					MSG_CHMPRN("Failed to getsockopt for sock(%d), errno=%d.", sock, errno);
					return errno;
				}
				if(EINPROGRESS == conerr || EINTR == conerr){
					MSG_CHMPRN("Waiting connected sock(%d) result is errno=%d, but this error needs to wait again.", sock, conerr);
					continue;
				}else if(0 != conerr){
					MSG_CHMPRN("Waiting connected sock(%d) result is errno=%d", sock, conerr);
					return conerr;
				}
				// OK
			}

			// [NOTE]
			// we do not check POLLERR, POLLNVAL and POLLHUP status.
			// These are not an error since it is one of the factors for canceling the wait state.
			// And if socket status is something wrong, probably it can be caught after return this
			// method.
			//

			//MSG_CHMPRN("Succeed to wait up for connected sock=%d by rtcode=%d.", sock, rtcode);
			return 0;
		}
		// timeout -> retry
		//MSG_CHMPRN("Waiting connected sock(%d) result is ETIMEOUT(%ldus), try again.", sock, waittime);
	}
	//MSG_CHMPRN("Waiting connected sock(%d) result is ETIMEOUT(%ldus * %d).", sock, waittime, retrycnt);

	return ETIMEDOUT;
}

int ChmEventSock::Listen(const char* hostname, short port)
{
	if(CHM_INVALID_PORT == port){
		ERR_CHMPRN("Parameters are wrong.");
		return CHM_INVALID_SOCK;
	}

	int	sockfd = CHM_INVALID_SOCK;
	if(CHMEMPTYSTR(hostname)){
		struct addrinfo*	paddrinfo = NULL;
		// First, test for INET6
		if(!ChmNetDb::GetAnyAddrInfo(port, &paddrinfo, true)){
			WAN_CHMPRN("Failed to get IN6ADDR_ANY_INIT addrinfo, but continue for INADDR_ANY.");
		}else{
			sockfd = ChmEventSock::RawListen(paddrinfo);
			freeaddrinfo(paddrinfo);
		}
		// check
		if(CHM_INVALID_SOCK == sockfd){
			// failed to bind/listen by INET6, so try INET4
			if(!ChmNetDb::GetAnyAddrInfo(port, &paddrinfo, false)){
				ERR_CHMPRN("Failed to get INADDR_ANY addrinfo, so both IN6ADDR_ANY_INIT and INADDR_ANY are failed.");
				return CHM_INVALID_SOCK;
			}
			sockfd = ChmEventSock::RawListen(paddrinfo);
			freeaddrinfo(paddrinfo);
		}

	}else{
		addrinfolist_t	addrinfos;
		if(!ChmNetDb::Get()->GetAddrInfoList(hostname, port, addrinfos, true)){		// if "localhost", convert fqdn.
			ERR_CHMPRN("Failed to get addrinfo for %s:%d.", hostname, port);
			return CHM_INVALID_SOCK;
		}
		sockfd = CHM_INVALID_SOCK;
		for(addrinfolist_t::const_iterator iter = addrinfos.begin(); addrinfos.end() != iter && CHM_INVALID_SOCK == sockfd; ++iter){
			struct addrinfo*	paddrinfo	= *iter;
			sockfd = ChmEventSock::RawListen(paddrinfo);
		}
		ChmNetDb::FreeAddrInfoList(addrinfos);
	}

	if(CHM_INVALID_SOCK == sockfd){
		ERR_CHMPRN("Could not make socket and listen on %s:%d.", CHMEMPTYSTR(hostname) ? "ADDR_ANY" : hostname, port);
		return CHM_INVALID_SOCK;
	}
	return sockfd;
}

int ChmEventSock::RawListen(struct addrinfo* paddrinfo)
{
	const int	opt_yes = 1;

	if(!paddrinfo){
		ERR_CHMPRN("Parameter is wrong.");
		return CHM_INVALID_SOCK;
	}

	// make socket, bind, listen
	int	sockfd = CHM_INVALID_SOCK;
	for(struct addrinfo* ptmpaddrinfo = paddrinfo; ptmpaddrinfo && CHM_INVALID_SOCK == sockfd; ptmpaddrinfo = ptmpaddrinfo->ai_next){
		if(IPPROTO_TCP != ptmpaddrinfo->ai_protocol){
			MSG_CHMPRN("protocol in addrinfo does not TCP, so check next addrinfo...");
			continue;
		}
		// socket
		if(-1 == (sockfd = socket(ptmpaddrinfo->ai_family, ptmpaddrinfo->ai_socktype, ptmpaddrinfo->ai_protocol))){
			ERR_CHMPRN("Failed to make socket by errno=%d, but continue to make next addrinfo...", errno);
			// sockfd = CHM_INVALID_SOCK;
			continue;
		}
		// sockopt(keepalive, etc)
		setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const void*>(&opt_yes), sizeof(int));
#ifdef SO_REUSEPORT
		setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, reinterpret_cast<const void*>(&opt_yes), sizeof(int));
#endif
		setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const void*>(&opt_yes), sizeof(int));
		setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const void*>(&opt_yes), sizeof(int));
		setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, reinterpret_cast<const void*>(&ChmEventSock::DEFAULT_KEEPIDLE), sizeof(int));
		setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, reinterpret_cast<const void*>(&ChmEventSock::DEFAULT_KEEPINTERVAL), sizeof(int));
		setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, reinterpret_cast<const void*>(&ChmEventSock::DEFAULT_KEEPCOUNT), sizeof(int));
		ChmEventSock::SetNonblocking(sockfd);	// NONBLOCKING

		// bind
		if(-1 == bind(sockfd, ptmpaddrinfo->ai_addr, ptmpaddrinfo->ai_addrlen)){
			ERR_CHMPRN("Failed to bind by errno=%d, but continue to make next addrinfo...", errno);
			CHM_CLOSESOCK(sockfd);
			continue;
		}

		// listen
		if(-1 == listen(sockfd, ChmEventSock::DEFAULT_LISTEN_BACKLOG)){
			ERR_CHMPRN("Failed to listen by errno=%d, but continue to make next addrinfo...", errno);
			CHM_CLOSESOCK(sockfd);
			continue;
		}
	}

	if(CHM_INVALID_SOCK == sockfd){
		ERR_CHMPRN("Could not make socket and listen.");
		return CHM_INVALID_SOCK;
	}
	return sockfd;
}

int ChmEventSock::Connect(const char* hostname, short port, bool is_blocking, int con_retrycnt, suseconds_t con_waittime)
{
	const int	opt_yes = 1;

	if(CHMEMPTYSTR(hostname) || CHM_INVALID_PORT == port){
		ERR_CHMPRN("Parameters are wrong.");
		return CHM_INVALID_SOCK;
	}

	// Get addrinfo list
	addrinfolist_t	addrinfos;
	if(!ChmNetDb::Get()->GetAddrInfoList(hostname, port, addrinfos, true)){		// if "localhost", convert fqdn.
		ERR_CHMPRN("Failed to get addrinfo for %s:%d.", hostname, port);
		return CHM_INVALID_SOCK;
	}

	// make socket, bind, listen
	int	sockfd = CHM_INVALID_SOCK;
	for(addrinfolist_t::const_iterator iter = addrinfos.begin(); addrinfos.end() != iter && CHM_INVALID_SOCK == sockfd; ++iter){
		struct addrinfo*	paddrinfo = *iter;
		for(struct addrinfo* ptmpaddrinfo = paddrinfo; ptmpaddrinfo && CHM_INVALID_SOCK == sockfd; ptmpaddrinfo = ptmpaddrinfo->ai_next){
			if(IPPROTO_TCP != ptmpaddrinfo->ai_protocol){
				MSG_CHMPRN("protocol in addrinfo which is made from %s:%d does not TCP, so check next addrinfo...", hostname, port);
				continue;
			}
			// socket
			if(CHM_INVALID_SOCK == (sockfd = socket(ptmpaddrinfo->ai_family, ptmpaddrinfo->ai_socktype, ptmpaddrinfo->ai_protocol))){
				ERR_CHMPRN("Failed to make socket for %s:%d by errno=%d, but continue to make next addrinfo...", hostname, port, errno);
				continue;
			}

			// options
			setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const void*>(&opt_yes), sizeof(int));
			setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const void*>(&opt_yes), sizeof(int));
			setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, reinterpret_cast<const void*>(&ChmEventSock::DEFAULT_KEEPIDLE), sizeof(int));
			setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, reinterpret_cast<const void*>(&ChmEventSock::DEFAULT_KEEPINTERVAL), sizeof(int));
			setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, reinterpret_cast<const void*>(&ChmEventSock::DEFAULT_KEEPCOUNT), sizeof(int));
			if(!is_blocking){
				ChmEventSock::SetNonblocking(sockfd);	// NONBLOCKING
			}

			// connect
			if(-1 == connect(sockfd, ptmpaddrinfo->ai_addr, ptmpaddrinfo->ai_addrlen)){
				if(is_blocking || EINPROGRESS != errno){
					ERR_CHMPRN("Failed to connect for %s:%d by errno=%d, but continue to make next addrinfo...", hostname, port, errno);
					CHM_CLOSESOCK(sockfd);
					continue;
				}else{
					// wait connected...(non blocking & EINPROGRESS)
					int	werr = ChmEventSock::WaitForReady(sockfd, WAIT_WRITE_FD, con_retrycnt, true, con_waittime);		// check SO_ERROR
					if(0 != werr){
						MSG_CHMPRN("Failed to connect for %s:%d by errno=%d.", hostname, port, werr);
						CHM_CLOSESOCK(sockfd);
						continue;
					}
				}
			}
			if(CHM_INVALID_SOCK != sockfd){
				break;
			}
		}
	}
	ChmNetDb::FreeAddrInfoList(addrinfos);

	if(CHM_INVALID_SOCK == sockfd){
		MSG_CHMPRN("Could not make socket and connect %s:%d.", hostname, port);
	}
	return sockfd;
}

// [TODO]
// In most case, pComPkt is allocated memory. But in case of large data it should
// be able to allocate memory by shared memory.
//
bool ChmEventSock::RawSend(int sock, ChmSSSession ssl, PCOMPKT pComPkt, bool& is_closed, bool is_blocking, int retrycnt, suseconds_t waittime)
{
	if(CHM_INVALID_SOCK == sock || !pComPkt){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	DUMPCOM_COMPKT_TYPE("Sock::RawSend", pComPkt);
	DUMPCOM_COMPKT("Sock::RawSend", pComPkt);

	// convert hton
	size_t	length = 0L;
	if(!ChmEventSock::hton(pComPkt, length)){
		ERR_CHMPRN("Failed to convert packet by hton.");
		return false;
	}
	//DUMPCOM_COMPKT("Sock::RawSend", pComPkt);

	if(0L == length){
		ERR_CHMPRN("Length(%zu) in PCOMPKT is wrong.", length);
		return false;
	}
	unsigned char*	pbydata = reinterpret_cast<unsigned char*>(pComPkt);

	// send
	return ChmEventSock::RawSend(sock, ssl, pbydata, length, is_closed, is_blocking, retrycnt, waittime);
}

bool ChmEventSock::RawSend(int sock, ChmSSSession ssl, const unsigned char* pbydata, size_t length, bool& is_closed, bool is_blocking, int retrycnt, suseconds_t waittime)
{
	if(CHM_INVALID_SOCK == sock || !pbydata || 0L == length){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}

	// send
	ssize_t	onesent = 0;
	size_t	totalsent;
	for(totalsent = 0; totalsent < length; totalsent += static_cast<size_t>(onesent)){
		if(ssl){
			// SSL
			bool is_retry 		= true;
			onesent				= 0;
			int	write_result	= ChmSecureSock::Write(ssl, &pbydata[totalsent], length - totalsent);

			if(ChmSecureSock::CheckResultSSL(sock, ssl, write_result, CHKRESULTSSL_TYPE_RW, is_retry, is_closed, retrycnt, waittime)){
				// success
				if(0 < write_result){
					onesent = static_cast<ssize_t>(write_result);
				}
			}else{
				if(!is_retry){
					// If the socket is closed, it occurs notification. so nothing to do here.
					ERR_CHMPRN("Failed to write from SSL on sock(%d), and the socket is %s.", sock, (is_closed ? "closed" : "not closed"));
					return false;
				}
			}
		}else{
			// Not SSL
			if(!is_blocking){
				int	werr = ChmEventSock::WaitForReady(sock, WAIT_WRITE_FD, retrycnt, false, waittime);		// not check SO_ERROR
				if(0 != werr){
					ERR_CHMPRN("Failed to send PCOMPKT(length:%zu), because failed to wait ready for sending on sock(%d) by errno=%d.", length, sock, werr);
					if(ETIMEDOUT != werr){
						is_closed = true;
					}
					return false;
				}
			}

			if(-1 == (onesent = send(sock, &pbydata[totalsent], length - totalsent, is_blocking ? 0 : MSG_DONTWAIT))){
				if(EINTR == errno){
					// retry assap
					MSG_CHMPRN("Interrupted signal during sending to sock(%d), errno=%d(EINTR).", sock, errno);

				}else if(EAGAIN == errno || EWOULDBLOCK == errno){
					// wait(non blocking)
					MSG_CHMPRN("sock(%d) does not ready for sending, errno=%d(EAGAIN or EWOULDBLOCK).", sock, errno);

				}else if(EACCES == errno || EBADF == errno || ECONNRESET == errno || ENOTCONN == errno || EDESTADDRREQ == errno || EISCONN == errno || ENOTSOCK == errno || EPIPE == errno){
					// something error to closing
					WAN_CHMPRN("sock(%d) does not ready for sending, errno=%d(EACCES or EBADF or ECONNRESET or ENOTCONN or EDESTADDRREQ or EISCONN or ENOTSOCK or EPIPE).", sock, errno);
					is_closed = true;
					return false;

				}else{
					// failed
					WAN_CHMPRN("Failed to send PCOMPKT(length:%zu), errno=%d.", length, errno);
					return false;
				}
				// continue...
				onesent = 0;
			}
		}
	}
	return true;
}

// [TODO]
// For large data case, pbuff is shared memory instead of allocated memory.
//
bool ChmEventSock::RawReceiveByte(int sock, ChmSSSession ssl, bool& is_closed, unsigned char* pbuff, size_t length, bool is_blocking, int retrycnt, suseconds_t waittime)
{
	if(CHM_INVALID_SOCK == sock || !pbuff || 0 == length){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	is_closed = false;

	// receive
	ssize_t	onerecv = 0;
	size_t	totalrecv;
	for(totalrecv = 0; totalrecv < length; totalrecv += static_cast<size_t>(onerecv)){
		if(ssl){
			// SSL
			bool is_retry 	= true;
			onerecv			= 0;
			int	read_result	= ChmSecureSock::Read(ssl, &pbuff[totalrecv], length - totalrecv);

			if(ChmSecureSock::CheckResultSSL(sock, ssl, read_result, CHKRESULTSSL_TYPE_RW, is_retry, is_closed, retrycnt, waittime)){
				// success
				if(0 < read_result){
					onerecv = static_cast<ssize_t>(read_result);
				}
			}else{
				if(!is_retry){
					// If the socket is closed, it occurs notification. so nothing to do here.
					ERR_CHMPRN("Failed to receive from SSL on sock(%d), and the socket is %s.", sock, (is_closed ? "closed" : "not closed"));
					return false;
				}
			}

		}else{
			// Not SSL
			if(!is_blocking){
				int	werr = ChmEventSock::WaitForReady(sock, WAIT_READ_FD, retrycnt, false, waittime);				// not check SO_ERROR
				if(0 != werr){
					ERR_CHMPRN("Failed to receive from sock(%d), because failed to wait ready for receiving, errno=%d.", sock, werr);
					if(ETIMEDOUT != werr){
						is_closed = true;
					}
					return false;
				}
			}

			if(-1 == (onerecv = recv(sock, &pbuff[totalrecv], length - totalrecv, is_blocking ? 0 : MSG_DONTWAIT))){
				if(EINTR == errno){
					// retry assap
					MSG_CHMPRN("Interrupted signal during receiving from sock(%d), errno=%d(EINTR).", sock, errno);

				}else if(EAGAIN == errno || EWOULDBLOCK == errno){
					// wait(non blocking)
					MSG_CHMPRN("There are no received data on sock(%d), errno=%d(EAGAIN or EWOULDBLOCK).", sock, errno);

				}else if(EBADF == errno || ECONNREFUSED == errno || ENOTCONN == errno || ENOTSOCK == errno){
					// error for closing
					WAN_CHMPRN("There are no received data on sock(%d), errno=%d(EBADF or ECONNREFUSED or ENOTCONN or ENOTSOCK).", sock, errno);
					is_closed = true;
					return false;

				}else{
					// failed
					WAN_CHMPRN("Failed to receive from sock(%d), errno=%d, then closing this socket.", sock, errno);
					is_closed = true;
					return false;
				}
				// continue...
				onerecv = 0;

			}else if(0 == onerecv){
				// close sock
				//
				// [NOTICE]
				// We do not specify EPOLLRDHUP for epoll, then we know the socket closed by receive length = 0.
				// Because case of specified EPOLLRDHUP, there is possibility of a problem that the packet just
				// before FIN is replaced with FIN.
				//
				MSG_CHMPRN("Receive 0 byte from sock(%d), it means socket is closed.", sock);
				is_closed = true;
				return false;
			}
		}
	}
	return true;
}

bool ChmEventSock::RawReceiveAny(int sock, bool& is_closed, unsigned char* pbuff, size_t* plength, bool is_blocking, int retrycnt, suseconds_t waittime)
{
	if(CHM_INVALID_SOCK == sock || !pbuff || !plength){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	is_closed = false;

	// receive
	while(true){
		ssize_t	onerecv;

		if(!is_blocking){
			int	werr = ChmEventSock::WaitForReady(sock, WAIT_READ_FD, retrycnt, false, waittime);		// not check SO_ERROR
			if(0 != werr){
				ERR_CHMPRN("Failed to receive from sock(%d), because failed to wait ready for receiving, errno=%d.", sock, werr);
				if(ETIMEDOUT != werr){
					is_closed = true;
				}
				return false;
			}
		}

		if(-1 == (onerecv = recv(sock, pbuff, *plength, is_blocking ? 0 : MSG_DONTWAIT))){
			if(EINTR == errno){
				// retry
				MSG_CHMPRN("Interrupted signal during receiving from sock(%d), errno=%d(EINTR).", sock, errno);

			}else if(EAGAIN == errno || EWOULDBLOCK == errno){
				// no data(return assap)
				MSG_CHMPRN("There are no received data on sock(%d), so not wait. errno=%d(EAGAIN or EWOULDBLOCK).", sock, errno);
				*plength = 0L;
				break;

			}else if(EBADF == errno || ECONNREFUSED == errno || ENOTCONN == errno || ENOTSOCK == errno){
				// error for closing
				WAN_CHMPRN("There are no received data on sock(%d), errno=%d(EBADF or ECONNREFUSED or ENOTCONN or ENOTSOCK).", sock, errno);
				is_closed = true;
				return false;

			}else{
				// failed
				WAN_CHMPRN("Failed to receive from sock(%d), errno=%d, then closing this socket.", sock, errno);
				is_closed = true;
				return false;
			}

		}else if(0 == onerecv){
			// close sock
			//
			// [NOTICE]
			// We do not specify EPOLLRDHUP for epoll, then we know the socket closed by receive *plength = 0.
			// Because case of specified EPOLLRDHUP, there is possibility of a problem that the packet just
			// before FIN is replaced with FIN.
			//
			ERR_CHMPRN("Receive 0 byte from sock(%d), it means socket is closed.", sock);
			is_closed = true;
			return false;

		}else{
			// read some bytes.
			*plength = static_cast<size_t>(onerecv);
			break;
		}
	}
	return true;
}

// [NOTICE]
// If *ppComPkt is NULL, this function returns *ppComPkt to allocated memory.
// The other(not NULL), this function set received data into *ppComPkt. Then pktlength
// means *ppComPkt buffer length.
//
bool ChmEventSock::RawReceive(int sock, ChmSSSession ssl, bool& is_closed, PCOMPKT* ppComPkt, size_t pktlength, bool is_blocking, int retrycnt, suseconds_t waittime)
{
	if(CHM_INVALID_SOCK == sock || !ppComPkt){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	is_closed = false;

	// At first, read only COMPKT
	COMPKT			ComPkt;
	unsigned char*	pbyComPkt = reinterpret_cast<unsigned char*>(&ComPkt);
	if(!ChmEventSock::RawReceiveByte(sock, ssl, is_closed, pbyComPkt, sizeof(COMPKT), is_blocking, retrycnt, waittime)){
		if(is_closed){
			MSG_CHMPRN("Failed to receive only COMPKT from sock(%d), socket is closed.", sock);
		}else{
			ERR_CHMPRN("Failed to receive only COMPKT from sock(%d), socket is not closed.", sock);
		}
		return false;
	}
	//DUMPCOM_COMPKT("Sock::RawReceive", &ComPkt);

	// get remaining length
	size_t	totallength = 0L;
	{
		// ntoh
		NTOH_PCOMPKT(&ComPkt);
		if(ComPkt.length < sizeof(COMPKT)){
			ERR_CHMPRN("The packet length(%zu) in received COMPKT from sock(%d) is too short, should be %zu byte.", ComPkt.length, sock, sizeof(COMPKT));
			return false;
		}
		totallength = ComPkt.length;
	}
	DUMPCOM_COMPKT("Sock::RawReceive", &ComPkt);

	// build buffer & copy COMPKT
	unsigned char*	pbyall;
	bool			is_alloc = false;
	if(NULL == *ppComPkt){
		if(NULL == (pbyall = reinterpret_cast<unsigned char*>(malloc(totallength)))){
			ERR_CHMPRN("Could not allocate memory(size=%zu)", totallength);
			return false;
		}
		*ppComPkt	= reinterpret_cast<PCOMPKT>(pbyall);
		is_alloc	= true;
	}else{
		if(pktlength < totallength){
			ERR_CHMPRN("Buffer length(%zu) is too small, receiving length is %zu, so COULD NOT READ REMAINING DATA.", pktlength, totallength);
			return false;
		}
		pbyall = reinterpret_cast<unsigned char*>(*ppComPkt);
	}
	memcpy(pbyall, pbyComPkt, sizeof(COMPKT));

	// Read remaining data
	if(!ChmEventSock::RawReceiveByte(sock, ssl, is_closed, &pbyall[sizeof(COMPKT)], totallength - sizeof(COMPKT), is_blocking, retrycnt, waittime)){
		if(is_closed){
			MSG_CHMPRN("Failed to receive after COMPKT from sock(%d), socket is closed.", sock);
		}else{
			ERR_CHMPRN("Failed to receive after COMPKT from sock(%d), socket is not closed.", sock);
		}
		if(is_alloc){
			CHM_Free(pbyall);
			*ppComPkt = NULL;
		}
		return false;
	}

	// ntoh
	if(!ChmEventSock::ntoh(*ppComPkt, true)){					// Already convert COMPKT
		ERR_CHMPRN("Failed to convert packet by ntoh.");
		if(is_alloc){
			CHM_Free(pbyall);
			*ppComPkt = NULL;
		}
		return false;
	}
	DUMPCOM_COMPKT_TYPE("Sock::RawReceive", *ppComPkt);

	return true;
}

//
// [NOTE]
// This method opens NEW control socket, so we do not need to lock socket for sending/receiving.
// BUT we must lock sockfd_lockval because this opens new socket.
//
bool ChmEventSock::RawSendCtlPort(const char* hostname, short ctlport, const unsigned char* pbydata, size_t length, string& strResult, volatile int& sockfd_lockval, int retrycnt, suseconds_t waittime, int con_retrycnt, suseconds_t con_waittime)
{
	strResult = "";

	if(CHMEMPTYSTR(hostname) || CHM_INVALID_PORT == ctlport || !pbydata || 0L == length){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	// try to connect to ctlport
	int	ctlsock;
	while(!fullock::flck_trylock_noshared_mutex(&sockfd_lockval));	// LOCK
	if(CHM_INVALID_SOCK == (ctlsock = ChmEventSock::Connect(hostname, ctlport, false, con_retrycnt, con_waittime))){
		// could not connect other server ctlport
		ERR_CHMPRN("Could not connect to %s:%d.", hostname, ctlport);
		fullock::flck_unlock_noshared_mutex(&sockfd_lockval);		// UNLOCK
		return false;
	}
	MSG_CHMPRN("Connected to %s:%d, then transfer command.", hostname, ctlport);
	fullock::flck_unlock_noshared_mutex(&sockfd_lockval);			// UNLOCK

	// send(does not lock for control socket)
	bool	is_closed = false;
	if(!ChmEventSock::RawSend(ctlsock, NULL, pbydata, length, is_closed, false, retrycnt, waittime)){
		ERR_CHMPRN("Could not send to %s:%d(sock:%d).", hostname, ctlport, ctlsock);
		if(!is_closed){
			CHM_CLOSESOCK(ctlsock);
		}
		return false;
	}

	// receive
	char	szReceive[CTL_RECEIVE_MAX_LENGTH];
	size_t	RecLength = CTL_RECEIVE_MAX_LENGTH - 1;
	memset(szReceive, 0, CTL_RECEIVE_MAX_LENGTH);

	if(!ChmEventSock::RawReceiveAny(ctlsock, is_closed, reinterpret_cast<unsigned char*>(szReceive), &RecLength, false, retrycnt * 10, waittime)){	// wait 10 times by normal
		ERR_CHMPRN("Failed to receive data from ctlport ctlsock(%d), ctlsock is %s.", ctlsock, is_closed ? "closed" : "not closed");
		if(!is_closed){
			CHM_CLOSESOCK(ctlsock);
		}
		return false;
	}
	// cppcheck-suppress unmatchedSuppression
	// cppcheck-suppress unreadVariable
	CHM_CLOSESOCK(ctlsock);

	strResult = szReceive;
	return true;
}

bool ChmEventSock::hton(PCOMPKT pComPkt, size_t& length)
{
	if(!pComPkt){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	// backup
	comtype_t	type	= pComPkt->head.type;
	off_t		offset	= pComPkt->offset;
	length				= pComPkt->length;

	// hton
	HTON_PCOMPKT(pComPkt);

	if(COM_PX2PX == type){
		// following datas
		PPXCOM_ALL	pComPacket	= CHM_OFFSET(pComPkt, offset, PPXCOM_ALL);

		// backup
		pxcomtype_t	comtype		= pComPacket->val_head.type;
		//size_t	comlength	= pComPacket->val_head.length;		// unused now

		// hton by type
		if(CHMPX_COM_STATUS_REQ == comtype){
			PPXCOM_STATUS_REQ	pComContents = CVT_COMPTR_STATUS_REQ(pComPacket);
			HTON_PPXCOM_STATUS_REQ(pComContents);

		}else if(CHMPX_COM_STATUS_RES == comtype){
			PPXCOM_STATUS_RES	pComContents = CVT_COMPTR_STATUS_RES(pComPacket);
			// hton each chmpxsvr
			PCHMPXSVRV1	pChmpxsvr = CHM_OFFSET(pComContents, pComContents->pchmpxsvr_offset, PCHMPXSVRV1);
			for(long cnt = 0; cnt < pComContents->count; cnt++){
				HTON_PCHMPXSVRV1(&pChmpxsvr[cnt]);
			}
			HTON_PPXCOM_STATUS_RES(pComContents);

		}else if(CHMPX_COM_CONINIT_REQ == comtype){
			PPXCOM_CONINIT_REQ	pComContents = CVT_COMPTR_CONINIT_REQ(pComPacket);
			HTON_PPXCOM_CONINIT_REQ(pComContents);

		}else if(CHMPX_COM_CONINIT_RES == comtype){
			PPXCOM_CONINIT_RES	pComContents = CVT_COMPTR_CONINIT_RES(pComPacket);
			HTON_PPXCOM_CONINIT_RES(pComContents);

		}else if(CHMPX_COM_JOIN_RING == comtype){
			PPXCOM_JOIN_RING	pComContents = CVT_COMPTR_JOIN_RING(pComPacket);
			HTON_PPXCOM_JOIN_RING(pComContents);

		}else if(CHMPX_COM_STATUS_UPDATE == comtype){
			PPXCOM_STATUS_UPDATE	pComContents = CVT_COMPTR_STATUS_UPDATE(pComPacket);
			// hton each chmpxsvr
			PCHMPXSVRV1	pChmpxsvr = CHM_OFFSET(pComContents, pComContents->pchmpxsvr_offset, PCHMPXSVRV1);
			for(long cnt = 0; cnt < pComContents->count; cnt++){
				HTON_PCHMPXSVRV1(&pChmpxsvr[cnt]);
			}
			HTON_PPXCOM_STATUS_UPDATE(pComContents);

		}else if(CHMPX_COM_STATUS_CONFIRM == comtype){
			PPXCOM_STATUS_CONFIRM	pComContents = CVT_COMPTR_STATUS_CONFIRM(pComPacket);
			// hton each chmpxsvr
			PCHMPXSVRV1	pChmpxsvr = CHM_OFFSET(pComContents, pComContents->pchmpxsvr_offset, PCHMPXSVRV1);
			for(long cnt = 0; cnt < pComContents->count; cnt++){
				HTON_PCHMPXSVRV1(&pChmpxsvr[cnt]);
			}
			HTON_PPXCOM_STATUS_CONFIRM(pComContents);

		}else if(CHMPX_COM_STATUS_CHANGE == comtype){
			PPXCOM_STATUS_CHANGE	pComContents = CVT_COMPTR_STATUS_CHANGE(pComPacket);
			HTON_PPXCOM_STATUS_CHANGE(pComContents);

		}else if(CHMPX_COM_MERGE_START == comtype){
			PPXCOM_MERGE_START	pComContents = CVT_COMPTR_MERGE_START(pComPacket);
			HTON_PPXCOM_MERGE_START(pComContents);

		}else if(CHMPX_COM_MERGE_ABORT == comtype){
			PPXCOM_MERGE_ABORT	pComContents = CVT_COMPTR_MERGE_ABORT(pComPacket);
			HTON_PPXCOM_MERGE_ABORT(pComContents);

		}else if(CHMPX_COM_MERGE_COMPLETE == comtype){
			PPXCOM_MERGE_COMPLETE	pComContents = CVT_COMPTR_MERGE_COMPLETE(pComPacket);
			HTON_PPXCOM_MERGE_COMPLETE(pComContents);

		}else if(CHMPX_COM_SERVER_DOWN == comtype){
			PPXCOM_SERVER_DOWN	pComContents = CVT_COMPTR_SERVER_DOWN(pComPacket);
			HTON_PPXCOM_SERVER_DOWN(pComContents);

		}else if(CHMPX_COM_REQ_UPDATEDATA == comtype){
			PPXCOM_REQ_UPDATEDATA	pComContents = CVT_COMPTR_REQ_UPDATEDATA(pComPacket);
			// hton each map
			PPXCOM_REQ_IDMAP	pIdMap = CHM_OFFSET(pComContents, pComContents->plist_offset, PPXCOM_REQ_IDMAP);
			for(long cnt = 0; cnt < pComContents->count; cnt++){
				HTON_PPXCOM_REQ_IDMAP(&pIdMap[cnt]);
			}
			HTON_PPXCOM_REQ_UPDATEDATA(pComContents);

		}else if(CHMPX_COM_RES_UPDATEDATA == comtype){
			PPXCOM_RES_UPDATEDATA	pComContents = CVT_COMPTR_RES_UPDATEDATA(pComPacket);
			HTON_PPXCOM_RES_UPDATEDATA(pComContents);

		}else if(CHMPX_COM_RESULT_UPDATEDATA == comtype){
			PPXCOM_RESULT_UPDATEDATA	pComContents = CVT_COMPTR_RESULT_UPDATEDATA(pComPacket);
			HTON_PPXCOM_RESULT_UPDATEDATA(pComContents);

		}else if(CHMPX_COM_MERGE_SUSPEND == comtype){
			PPXCOM_MERGE_SUSPEND	pComContents = CVT_COMPTR_MERGE_SUSPEND(pComPacket);
			HTON_PPXCOM_MERGE_SUSPEND(pComContents);

		}else if(CHMPX_COM_MERGE_NOSUSPEND == comtype){
			PPXCOM_MERGE_NOSUSPEND	pComContents = CVT_COMPTR_MERGE_NOSUSPEND(pComPacket);
			HTON_PPXCOM_MERGE_NOSUSPEND(pComContents);

		}else if(CHMPX_COM_MERGE_SUSPEND_GET == comtype){
			PPXCOM_MERGE_SUSPEND_GET	pComContents = CVT_COMPTR_MERGE_SUSPEND_GET(pComPacket);
			HTON_PPXCOM_MERGE_SUSPEND_GET(pComContents);

		}else if(CHMPX_COM_MERGE_SUSPEND_RES == comtype){
			PPXCOM_MERGE_SUSPEND_RES	pComContents = CVT_COMPTR_MERGE_SUSPEND_RES(pComPacket);
			HTON_PPXCOM_MERGE_SUSPEND_RES(pComContents);

		}else if(CHMPX_COM_VERSION_REQ == comtype){
			PPXCOM_VERSION_REQ	pComContents = CVT_COMPTR_VERSION_REQ(pComPacket);
			HTON_PPXCOM_VERSION_REQ(pComContents);

		}else if(CHMPX_COM_VERSION_RES == comtype){
			PPXCOM_VERSION_RES	pComContents = CVT_COMPTR_VERSION_RES(pComPacket);
			HTON_PPXCOM_VERSION_RES(pComContents);

		}else if(CHMPX_COM_N2_STATUS_REQ == comtype){
			PPXCOM_N2_STATUS_REQ	pComContents = CVT_COMPTR_N2_STATUS_REQ(pComPacket);
			HTON_PPXCOM_N2_STATUS_REQ(pComContents);

		}else if(CHMPX_COM_N2_STATUS_RES == comtype){
			PPXCOM_N2_STATUS_RES	pComContents = CVT_COMPTR_N2_STATUS_RES(pComPacket);
			// hton each chmpxsvr
			PCHMPXSVR	pChmpxsvr = CHM_OFFSET(pComContents, pComContents->pchmpxsvr_offset, PCHMPXSVR);
			for(long cnt = 0; cnt < pComContents->count; cnt++){
				HTON_PCHMPXSVR(&pChmpxsvr[cnt]);
			}
			HTON_PPXCOM_N2_STATUS_RES(pComContents);

		}else if(CHMPX_COM_N2_CONINIT_REQ == comtype){
			PPXCOM_N2_CONINIT_REQ	pComContents = CVT_COMPTR_N2_CONINIT_REQ(pComPacket);
			HTON_PPXCOM_N2_CONINIT_REQ(pComContents);

		}else if(CHMPX_COM_N2_CONINIT_RES == comtype){
			PPXCOM_N2_CONINIT_RES	pComContents = CVT_COMPTR_N2_CONINIT_RES(pComPacket);
			HTON_PPXCOM_N2_CONINIT_RES(pComContents);

		}else if(CHMPX_COM_N2_JOIN_RING == comtype){
			PPXCOM_N2_JOIN_RING	pComContents = CVT_COMPTR_N2_JOIN_RING(pComPacket);
			HTON_PPXCOM_N2_JOIN_RING(pComContents);

		}else if(CHMPX_COM_N2_STATUS_UPDATE == comtype){
			PPXCOM_N2_STATUS_UPDATE	pComContents = CVT_COMPTR_N2_STATUS_UPDATE(pComPacket);
			// hton each chmpxsvr
			PCHMPXSVR	pChmpxsvr = CHM_OFFSET(pComContents, pComContents->pchmpxsvr_offset, PCHMPXSVR);
			for(long cnt = 0; cnt < pComContents->count; cnt++){
				HTON_PCHMPXSVR(&pChmpxsvr[cnt]);
			}
			HTON_PPXCOM_N2_STATUS_UPDATE(pComContents);

		}else if(CHMPX_COM_N2_STATUS_CONFIRM == comtype){
			PPXCOM_N2_STATUS_CONFIRM	pComContents = CVT_COMPTR_N2_STATUS_CONFIRM(pComPacket);
			// hton each chmpxsvr
			PCHMPXSVR	pChmpxsvr = CHM_OFFSET(pComContents, pComContents->pchmpxsvr_offset, PCHMPXSVR);
			for(long cnt = 0; cnt < pComContents->count; cnt++){
				HTON_PCHMPXSVR(&pChmpxsvr[cnt]);
			}
			HTON_PPXCOM_N2_STATUS_CONFIRM(pComContents);

		}else if(CHMPX_COM_N2_STATUS_CHANGE == comtype){
			PPXCOM_N2_STATUS_CHANGE	pComContents = CVT_COMPTR_N2_STATUS_CHANGE(pComPacket);
			HTON_PPXCOM_N2_STATUS_CHANGE(pComContents);
		}else{
			WAN_CHMPRN("ComPacket type is %s(%" PRIu64 ") which is unknown. so does not convert contents.", STRPXCOMTYPE(comtype), comtype);
		}
	}else if(COM_C2C == type){
		//
		// Not implement
		//

	}else{
		WAN_CHMPRN("Packet type is %s(%" PRIu64 "), why does send to other chmpx? so does not convert contents.", STRCOMTYPE(type), type);
	}
	return true;
}

bool ChmEventSock::ntoh(PCOMPKT pComPkt, bool is_except_compkt)
{
	if(!pComPkt){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}

	// ntoh
	if(!is_except_compkt){
		NTOH_PCOMPKT(pComPkt);
	}

	if(COM_PX2PX == pComPkt->head.type){
		// following datas
		PPXCOM_ALL	pComPacket	= CHM_OFFSET(pComPkt, pComPkt->offset, PPXCOM_ALL);
		pxcomtype_t	type		= be64toh(pComPacket->val_head.type);

		// ntoh by type
		if(CHMPX_COM_STATUS_REQ == type){
			PPXCOM_STATUS_REQ	pComContents = CVT_COMPTR_STATUS_REQ(pComPacket);
			NTOH_PPXCOM_STATUS_REQ(pComContents);

		}else if(CHMPX_COM_STATUS_RES == type){
			PPXCOM_STATUS_RES	pComContents = CVT_COMPTR_STATUS_RES(pComPacket);
			NTOH_PPXCOM_STATUS_RES(pComContents);
			// ntoh each chmpx
			PCHMPXSVRV1	pChmpxsvr = CHM_OFFSET(pComContents, pComContents->pchmpxsvr_offset, PCHMPXSVRV1);
			for(long cnt = 0; cnt < pComContents->count; cnt++){
				NTOH_PCHMPXSVRV1(&pChmpxsvr[cnt]);
			}

		}else if(CHMPX_COM_CONINIT_REQ == type){
			PPXCOM_CONINIT_REQ	pComContents = CVT_COMPTR_CONINIT_REQ(pComPacket);
			NTOH_PPXCOM_CONINIT_REQ(pComContents);

		}else if(CHMPX_COM_CONINIT_RES == type){
			PPXCOM_CONINIT_RES	pComContents = CVT_COMPTR_CONINIT_RES(pComPacket);
			NTOH_PPXCOM_CONINIT_RES(pComContents);

		}else if(CHMPX_COM_JOIN_RING == type){
			PPXCOM_JOIN_RING	pComContents = CVT_COMPTR_JOIN_RING(pComPacket);
			NTOH_PPXCOM_JOIN_RING(pComContents);

		}else if(CHMPX_COM_STATUS_UPDATE == type){
			PPXCOM_STATUS_UPDATE	pComContents = CVT_COMPTR_STATUS_UPDATE(pComPacket);
			NTOH_PPXCOM_STATUS_UPDATE(pComContents);
			// ntoh each chmpx
			PCHMPXSVRV1	pChmpxsvr = CHM_OFFSET(pComContents, pComContents->pchmpxsvr_offset, PCHMPXSVRV1);
			for(long cnt = 0; cnt < pComContents->count; cnt++){
				NTOH_PCHMPXSVRV1(&pChmpxsvr[cnt]);
			}

		}else if(CHMPX_COM_STATUS_CONFIRM == type){
			PPXCOM_STATUS_CONFIRM	pComContents = CVT_COMPTR_STATUS_CONFIRM(pComPacket);
			NTOH_PPXCOM_STATUS_CONFIRM(pComContents);
			// ntoh each chmpx
			PCHMPXSVRV1	pChmpxsvr = CHM_OFFSET(pComContents, pComContents->pchmpxsvr_offset, PCHMPXSVRV1);
			for(long cnt = 0; cnt < pComContents->count; cnt++){
				NTOH_PCHMPXSVRV1(&pChmpxsvr[cnt]);
			}

		}else if(CHMPX_COM_STATUS_CHANGE == type){
			PPXCOM_STATUS_CHANGE	pComContents = CVT_COMPTR_STATUS_CHANGE(pComPacket);
			NTOH_PPXCOM_STATUS_CHANGE(pComContents);

		}else if(CHMPX_COM_MERGE_START == type){
			PPXCOM_MERGE_START	pComContents = CVT_COMPTR_MERGE_START(pComPacket);
			NTOH_PPXCOM_MERGE_START(pComContents);

		}else if(CHMPX_COM_MERGE_ABORT == type){
			PPXCOM_MERGE_ABORT	pComContents = CVT_COMPTR_MERGE_ABORT(pComPacket);
			NTOH_PPXCOM_MERGE_ABORT(pComContents);

		}else if(CHMPX_COM_MERGE_COMPLETE == type){
			PPXCOM_MERGE_COMPLETE	pComContents = CVT_COMPTR_MERGE_COMPLETE(pComPacket);
			NTOH_PPXCOM_MERGE_COMPLETE(pComContents);

		}else if(CHMPX_COM_SERVER_DOWN == type){
			PPXCOM_SERVER_DOWN	pComContents = CVT_COMPTR_SERVER_DOWN(pComPacket);
			NTOH_PPXCOM_SERVER_DOWN(pComContents);

		}else if(CHMPX_COM_REQ_UPDATEDATA == type){
			PPXCOM_REQ_UPDATEDATA	pComContents = CVT_COMPTR_REQ_UPDATEDATA(pComPacket);
			NTOH_PPXCOM_REQ_UPDATEDATA(pComContents);
			// ntoh each chmpx
			PPXCOM_REQ_IDMAP	pIdMap = CHM_OFFSET(pComContents, pComContents->plist_offset, PPXCOM_REQ_IDMAP);
			for(long cnt = 0; cnt < pComContents->count; cnt++){
				NTOH_PPXCOM_REQ_IDMAP(&pIdMap[cnt]);
			}

		}else if(CHMPX_COM_RES_UPDATEDATA == type){
			PPXCOM_RES_UPDATEDATA	pComContents = CVT_COMPTR_RES_UPDATEDATA(pComPacket);
			NTOH_PPXCOM_RES_UPDATEDATA(pComContents);

		}else if(CHMPX_COM_RESULT_UPDATEDATA == type){
			PPXCOM_RESULT_UPDATEDATA	pComContents = CVT_COMPTR_RESULT_UPDATEDATA(pComPacket);
			NTOH_PPXCOM_RESULT_UPDATEDATA(pComContents);

		}else if(CHMPX_COM_MERGE_SUSPEND == type){
			PPXCOM_MERGE_SUSPEND	pComContents = CVT_COMPTR_MERGE_SUSPEND(pComPacket);
			NTOH_PPXCOM_MERGE_SUSPEND(pComContents);

		}else if(CHMPX_COM_MERGE_NOSUSPEND == type){
			PPXCOM_MERGE_NOSUSPEND	pComContents = CVT_COMPTR_MERGE_NOSUSPEND(pComPacket);
			NTOH_PPXCOM_MERGE_NOSUSPEND(pComContents);

		}else if(CHMPX_COM_MERGE_SUSPEND_GET == type){
			PPXCOM_MERGE_SUSPEND_GET	pComContents = CVT_COMPTR_MERGE_SUSPEND_GET(pComPacket);
			NTOH_PPXCOM_MERGE_SUSPEND_GET(pComContents);

		}else if(CHMPX_COM_MERGE_SUSPEND_RES == type){
			PPXCOM_MERGE_SUSPEND_RES	pComContents = CVT_COMPTR_MERGE_SUSPEND_RES(pComPacket);
			NTOH_PPXCOM_MERGE_SUSPEND_RES(pComContents);

		}else if(CHMPX_COM_VERSION_REQ == type){
			PPXCOM_VERSION_REQ	pComContents = CVT_COMPTR_VERSION_REQ(pComPacket);
			NTOH_PPXCOM_VERSION_REQ(pComContents);

		}else if(CHMPX_COM_VERSION_RES == type){
			PPXCOM_VERSION_RES	pComContents = CVT_COMPTR_VERSION_RES(pComPacket);
			NTOH_PPXCOM_VERSION_RES(pComContents);

		}else if(CHMPX_COM_N2_STATUS_REQ == type){
			PPXCOM_N2_STATUS_REQ	pComContents = CVT_COMPTR_N2_STATUS_REQ(pComPacket);
			NTOH_PPXCOM_N2_STATUS_REQ(pComContents);

		}else if(CHMPX_COM_N2_STATUS_RES == type){
			PPXCOM_N2_STATUS_RES	pComContents = CVT_COMPTR_N2_STATUS_RES(pComPacket);
			NTOH_PPXCOM_N2_STATUS_RES(pComContents);
			// ntoh each chmpx
			PCHMPXSVR	pChmpxsvr = CHM_OFFSET(pComContents, pComContents->pchmpxsvr_offset, PCHMPXSVR);
			for(long cnt = 0; cnt < pComContents->count; cnt++){
				NTOH_PCHMPXSVR(&pChmpxsvr[cnt]);
			}

		}else if(CHMPX_COM_N2_CONINIT_REQ == type){
			PPXCOM_N2_CONINIT_REQ	pComContents = CVT_COMPTR_N2_CONINIT_REQ(pComPacket);
			NTOH_PPXCOM_N2_CONINIT_REQ(pComContents);

		}else if(CHMPX_COM_N2_CONINIT_RES == type){
			PPXCOM_N2_CONINIT_RES	pComContents = CVT_COMPTR_N2_CONINIT_RES(pComPacket);
			NTOH_PPXCOM_N2_CONINIT_RES(pComContents);

		}else if(CHMPX_COM_N2_JOIN_RING == type){
			PPXCOM_N2_JOIN_RING	pComContents = CVT_COMPTR_N2_JOIN_RING(pComPacket);
			NTOH_PPXCOM_N2_JOIN_RING(pComContents);

		}else if(CHMPX_COM_N2_STATUS_UPDATE == type){
			PPXCOM_N2_STATUS_UPDATE	pComContents = CVT_COMPTR_N2_STATUS_UPDATE(pComPacket);
			NTOH_PPXCOM_N2_STATUS_UPDATE(pComContents);
			// ntoh each chmpx
			PCHMPXSVR	pChmpxsvr = CHM_OFFSET(pComContents, pComContents->pchmpxsvr_offset, PCHMPXSVR);
			for(long cnt = 0; cnt < pComContents->count; cnt++){
				NTOH_PCHMPXSVR(&pChmpxsvr[cnt]);
			}

		}else if(CHMPX_COM_N2_STATUS_CONFIRM == type){
			PPXCOM_N2_STATUS_CONFIRM	pComContents = CVT_COMPTR_N2_STATUS_CONFIRM(pComPacket);
			NTOH_PPXCOM_N2_STATUS_CONFIRM(pComContents);
			// ntoh each chmpx
			PCHMPXSVR	pChmpxsvr = CHM_OFFSET(pComContents, pComContents->pchmpxsvr_offset, PCHMPXSVR);
			for(long cnt = 0; cnt < pComContents->count; cnt++){
				NTOH_PCHMPXSVR(&pChmpxsvr[cnt]);
			}

		}else if(CHMPX_COM_N2_STATUS_CHANGE == type){
			PPXCOM_N2_STATUS_CHANGE	pComContents = CVT_COMPTR_N2_STATUS_CHANGE(pComPacket);
			NTOH_PPXCOM_N2_STATUS_CHANGE(pComContents);
		}else{
			WAN_CHMPRN("ComPacket type is %s(%" PRIu64 ") which is unknown. so does not convert contents.", STRPXCOMTYPE(type), type);
		}
	}else if(COM_C2C == pComPkt->head.type){
		//
		// Not implement
		//

	}else{
		WAN_CHMPRN("Packet type is %s(%" PRIu64 "), why does receive from other chmpx? so does not convert contents.", STRCOMTYPE(pComPkt->head.type), pComPkt->head.type);
	}
	return true;
}

//
// If ext_length is not PXCOMPKT_AUTO_LENGTH, allocate memory length as
// COMPKT + type + ext_length size.
//
PCOMPKT ChmEventSock::AllocatePxComPacket(pxcomtype_t type, ssize_t ext_length)
{
	ssize_t	length = sizeof(COMPKT) + (PXCOMPKT_AUTO_LENGTH == ext_length ? 0L : ext_length);

	if(CHMPX_COM_STATUS_REQ == type){
		length += sizeof(PXCOM_STATUS_REQ);

	}else if(CHMPX_COM_STATUS_RES == type){
		length += sizeof(PXCOM_STATUS_RES);

	}else if(CHMPX_COM_CONINIT_REQ == type){
		length += sizeof(PXCOM_CONINIT_REQ);

	}else if(CHMPX_COM_CONINIT_RES == type){
		length += sizeof(PXCOM_CONINIT_RES);

	}else if(CHMPX_COM_JOIN_RING == type){
		length += sizeof(PXCOM_JOIN_RING);

	}else if(CHMPX_COM_STATUS_UPDATE == type){
		length += sizeof(PXCOM_STATUS_UPDATE);

	}else if(CHMPX_COM_STATUS_CONFIRM == type){
		length += sizeof(PXCOM_STATUS_CONFIRM);

	}else if(CHMPX_COM_STATUS_CHANGE == type){
		length += sizeof(PXCOM_STATUS_CHANGE);

	}else if(CHMPX_COM_MERGE_START == type){
		length += sizeof(PXCOM_MERGE_START);

	}else if(CHMPX_COM_MERGE_ABORT == type){
		length += sizeof(PXCOM_MERGE_ABORT);

	}else if(CHMPX_COM_MERGE_COMPLETE == type){
		length += sizeof(PXCOM_MERGE_COMPLETE);

	}else if(CHMPX_COM_SERVER_DOWN == type){
		length += sizeof(PXCOM_SERVER_DOWN);

	}else if(CHMPX_COM_REQ_UPDATEDATA == type){
		length += sizeof(PXCOM_REQ_UPDATEDATA);

	}else if(CHMPX_COM_RES_UPDATEDATA == type){
		length += sizeof(PXCOM_RES_UPDATEDATA);

	}else if(CHMPX_COM_RESULT_UPDATEDATA == type){
		length += sizeof(PXCOM_RESULT_UPDATEDATA);

	}else if(CHMPX_COM_MERGE_SUSPEND == type){
		length += sizeof(PXCOM_MERGE_SUSPEND);

	}else if(CHMPX_COM_MERGE_NOSUSPEND == type){
		length += sizeof(PXCOM_MERGE_NOSUSPEND);

	}else if(CHMPX_COM_MERGE_SUSPEND_GET == type){
		length += sizeof(PXCOM_MERGE_SUSPEND_GET);

	}else if(CHMPX_COM_MERGE_SUSPEND_RES == type){
		length += sizeof(PXCOM_MERGE_SUSPEND_RES);

	}else if(CHMPX_COM_VERSION_REQ == type){
		length += sizeof(PXCOM_VERSION_REQ);

	}else if(CHMPX_COM_VERSION_RES == type){
		length += sizeof(PXCOM_VERSION_RES);

	}else if(CHMPX_COM_N2_STATUS_REQ == type){
		length += sizeof(PXCOM_N2_STATUS_REQ);

	}else if(CHMPX_COM_N2_STATUS_RES == type){
		length += sizeof(PXCOM_N2_STATUS_RES);

	}else if(CHMPX_COM_N2_CONINIT_REQ == type){
		length += sizeof(PXCOM_N2_CONINIT_REQ);

	}else if(CHMPX_COM_N2_CONINIT_RES == type){
		length += sizeof(PXCOM_N2_CONINIT_RES);

	}else if(CHMPX_COM_N2_JOIN_RING == type){
		length += sizeof(PXCOM_N2_JOIN_RING);

	}else if(CHMPX_COM_N2_STATUS_UPDATE == type){
		length += sizeof(PXCOM_N2_STATUS_UPDATE);

	}else if(CHMPX_COM_N2_STATUS_CONFIRM == type){
		length += sizeof(PXCOM_N2_STATUS_CONFIRM);

	}else if(CHMPX_COM_N2_STATUS_CHANGE == type){
		length += sizeof(PXCOM_N2_STATUS_CHANGE);

	}else{
		WAN_CHMPRN("ComPacket type is %s(%" PRIu64 ") which is unknown. so does not convert contents.", STRPXCOMTYPE(type), type);
	}

	PCOMPKT	rtnptr;
	if(NULL == (rtnptr = reinterpret_cast<PCOMPKT>(malloc(length)))){
		ERR_CHMPRN("Could not allocate memory as %zd length for %s(%" PRIu64 ").", length, STRPXCOMTYPE(type), type);
	}
	return rtnptr;
}

PCOMPKT ChmEventSock::DuplicateComPkt(PCOMPKT pComPkt)
{
	if(!pComPkt){
		ERR_CHMPRN("Parameter is wrong.");
		return NULL;
	}

	// allocation
	PCOMPKT	pDupPkt;
	if(NULL == (pDupPkt = reinterpret_cast<PCOMPKT>(malloc(pComPkt->length)))){
		ERR_CHMPRN("Could not allocate memory as %zu length.", pComPkt->length);
		return NULL;
	}

	// copy compkt
	COPY_COMPKT(pDupPkt, pComPkt);

	// copy after compkt
	if(sizeof(COMPKT) < pComPkt->length){
		unsigned char*	psrc	= reinterpret_cast<unsigned char*>(pComPkt) + sizeof(COMPKT);
		unsigned char*	pdest	= reinterpret_cast<unsigned char*>(pDupPkt) + sizeof(COMPKT);
		memcpy(pdest, psrc, (pComPkt->length - sizeof(COMPKT)));
	}
	return pDupPkt;
}

//---------------------------------------------------------
// Class Methods - Processing
//---------------------------------------------------------
bool ChmEventSock::ReceiveWorkerProc(void* common_param, chmthparam_t wp_param)
{
	ChmEventSock*		pSockObj	= reinterpret_cast<ChmEventSock*>(common_param);
	int					sock		= static_cast<int>(wp_param);
	if(!pSockObj || CHM_INVALID_SOCK == sock){
		ERR_CHMPRN("Parameters are wrong.");
		return true;		// sleep thread
	}
	chmpxid_t			tgchmpxid	= pSockObj->GetAssociateChmpxid(sock);
	string				tgachname	= pSockObj->GetAssociateAcceptHostName(sock);
	bool				changed_sock= false;

	// [NOTE]
	// We use edge triggerd epoll, in this type the event is accumulated and not sent when the data to
	// the socket is left. Thus we must read more until EAGAIN. Otherwise we lost data.
	// So we checked socket for rest data here.
	//
	// cppcheck-suppress unmatchedSuppression
	// cppcheck-suppress nullPointerRedundantCheck
	suseconds_t	waittime = pSockObj->sock_wait_time;
	bool		is_closed;
	int			werr;
	while(0 == (werr = ChmEventSock::WaitForReady(sock, WAIT_READ_FD, 0, false, waittime))){		// check rest data & return assap
		// Processing
		is_closed = false;
		if(false == pSockObj->RawReceive(sock, is_closed)){
			if(!is_closed){
				ERR_CHMPRN("Failed to receive and to process for sock(%d) by event socket object.", sock);
			}else{
				MSG_CHMPRN("sock(%d) is closed while processing thread.", sock);

				chmpxid_t	nowchmpxid = pSockObj ? pSockObj->GetAssociateChmpxid(sock) : CHM_INVALID_CHMPXID;
				string		nowachname = pSockObj ? pSockObj->GetAssociateAcceptHostName(sock) : string("");
				if(tgchmpxid != nowchmpxid || tgachname != nowachname){
					MSG_CHMPRN("Associate chmpxid of target sock(%d) was changed from old chmpxid(0x%016" PRIx64 ") or accept host(%s) to now chmpxid(0x%016" PRIx64 ") or accept host(%s).", sock, tgchmpxid, tgachname.c_str(), nowchmpxid, nowachname.c_str());
					changed_sock = true;
				}
			}
			break;
		}
	}
	// [NOTE]
	// If chmpxid for socket is changed, we should not call RawNotifyHup().
	// Because it is already called for Hup from another threads.
	//
	if(!changed_sock && ((0 != werr && ETIMEDOUT != werr) || is_closed)){
		if(!pSockObj->RawNotifyHup(sock)){
			ERR_CHMPRN("Failed to closing socket(%d), but continue...", sock);
		}
	}
	return true;		// always return true for continue to work.
}

//---------------------------------------------------------
// Class Methods - Merge
//---------------------------------------------------------
// Merge logic
//
// This merge thread(chmpx server node)
//		--- [MQ] -----> get lastest update time(to client on this server node through MQ)
//		<--------------
//
//		--- [SOCK] ---> push all update datas to this chmpx(to all other chmpx)
//			Other chmpx server node(status is up/servicein/nosuspend)
//				--- [MQ] ---> send request of pushing update datas(to client on server node)
//					client chmpx library
//						--- [CB] ---> loop: get update datas/send it to original chmpx
//				<-- [MQ] ---- finish
//			Other chmpx server node(status is NOT up/servicein/nosuspend)
//				nothing to do
//		<-- [SOCK] ---- finish
//
//	Wait for all server node finished.
//
// [NOTICE]
// This method is run on another worker thread.
// If need to stop this method, change "is_run_merge" flag to false.
// So that this method exits and return worker thread proc.
// After exiting this method, the worker proc is SLEEP.
//
// "is_run_merge" flag is changed only in main thread, so do not lock it.
//
bool ChmEventSock::MergeWorkerFunc(void* common_param, chmthparam_t wp_param)
{
	if(!common_param){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	ChmEventSock*	pThis = reinterpret_cast<ChmEventSock*>(common_param);

	if(!pThis->IsDoMerge()){
		WAN_CHMPRN("Why does this method call when do merge flag is false.");
		pThis->is_run_merge = false;
		return false;
	}

	//---------------------------------
	// Make communication datas
	//---------------------------------
	// check target chmpxid map
	if(pThis->mergeidmap.empty()){
		ERR_CHMPRN("There is no merge chmpxids.");
		pThis->is_run_merge = false;
		return true;			// keep running thread for next request
	}
	// next chmpxid
	chmpxid_t	to_chmpxid	= pThis->GetNextRingChmpxId();
	if(CHM_INVALID_CHMPXID == to_chmpxid){
		ERR_CHMPRN("There is no next chmpx server.");
		pThis->is_run_merge = false;
		return true;			// keep running thread for next request
	}
	// make merge param
	PXCOMMON_MERGE_PARAM	merge_param;
	struct timespec			lastts;
	// get lastest update time.
	if(!pThis->pChmCntrl->MergeGetLastTime(lastts)){
		ERR_CHMPRN("Something error occurred during getting lastest update time.");
		pThis->is_run_merge = false;
		return true;			// keep running thread for next request
	}
	merge_param.startts.tv_sec	= lastts.tv_sec;
	merge_param.startts.tv_nsec	= lastts.tv_nsec;

	// hash values
	ChmIMData*	pImData			= pThis->pChmCntrl->GetImDataObj();
	chmhash_t	tmp_hashval		= static_cast<chmhash_t>(-1);
	chmhash_t	tmp_max_hashval	= static_cast<chmhash_t>(-1);
	merge_param.chmpxid			= pImData->GetSelfChmpxId();
	merge_param.replica_count	= pImData->GetReplicaCount() + 1;	// ImData::Replica count means copy count
	merge_param.is_expire_check	= false;							// always not check expire time
																	// [TODO] If necessary in the future, read this value from Configuration.

	if(!pImData->GetSelfPendingHash(tmp_hashval) || !pImData->GetMaxPendingHashCount(tmp_max_hashval)){
		ERR_CHMPRN("Could not get own hash and pending hash values.");
		pThis->is_run_merge = false;
		return true;			// keep running thread for next request
	}else{
		//
		// *** modify start hash value and replica count ***
		//
		// [NOTE]
		// Main hash value's data is had the servers which are assigned after main hash value with replica count.
		// Conversely, Main hash value's server has another servers data which is pointed by forwarding replica
		// count to hash value. Thus we modify start hash value(but do not change replica count(range)) at here.
		// For example)
		//		replica = 2, new hash value = 5, max hash value = 10  ---> start hash = 3, range = 2
		//		replica = 2, new hash value = 1, max hash value = 10  ---> start hash = 9, range = 2
		//
		if(static_cast<chmhash_t>(merge_param.replica_count) <= (tmp_hashval + 1)){
			tmp_hashval = (tmp_hashval + 1) - static_cast<chmhash_t>(merge_param.replica_count);
		}else{
			tmp_hashval = (tmp_hashval + tmp_max_hashval + 1) - static_cast<chmhash_t>(merge_param.replica_count);
		}
		if((tmp_max_hashval + 1) < static_cast<chmhash_t>(merge_param.replica_count)){
			// why, but set maximum
			merge_param.replica_count = static_cast<long>(tmp_max_hashval + 1);
		}
	}
	merge_param.pending_hash	= tmp_hashval;
	merge_param.max_pending_hash= tmp_max_hashval;

	//
	// [NOTE]
	// If this chmpx join at first time to ring, probably base hash value is ignored.
	// (but max base hash is not ignored.)
	//
	tmp_hashval		= static_cast<chmhash_t>(-1);
	tmp_max_hashval	= static_cast<chmhash_t>(-1);
	if(!pImData->GetSelfBaseHash(tmp_hashval) || !pImData->GetMaxBaseHashCount(tmp_max_hashval) || tmp_hashval == static_cast<chmhash_t>(-1) || tmp_max_hashval == static_cast<chmhash_t>(-1)){
		// base hash value is ignored, maybe this process run and first join to ring.
		MSG_CHMPRN("Could not own base hash(or max base hash count), on this case we need to check all hash.");
		merge_param.base_hash		= static_cast<chmhash_t>(-1);
		merge_param.max_base_hash	= static_cast<chmhash_t>(-1);
	}else{
		if(static_cast<chmhash_t>(merge_param.replica_count) <= (tmp_hashval + 1)){
			tmp_hashval = (tmp_hashval + 1) - static_cast<chmhash_t>(merge_param.replica_count);
		}else{
			tmp_hashval = (tmp_hashval + tmp_max_hashval + 1) - static_cast<chmhash_t>(merge_param.replica_count);
		}
		if((tmp_max_hashval + 1) < static_cast<chmhash_t>(merge_param.replica_count)){
			// why, but set maximum
			merge_param.replica_count = static_cast<long>(tmp_max_hashval + 1);
		}
		merge_param.base_hash		= tmp_hashval;
		merge_param.max_base_hash	= tmp_max_hashval;
	}

	//---------------------------------
	// send request to other server node
	//---------------------------------
	if(!pThis->PxComSendReqUpdateData(to_chmpxid, &merge_param)){
		ERR_CHMPRN("Failed to send request update datas command.");
		pThis->is_run_merge = false;
		return true;			// keep running thread for next request
	}

	//---------------------------------
	// Loop: wait for all chmpx
	//---------------------------------
	struct timespec	sleeptime		= {0, 100 * 1000 * 1000};		// 100ms
	bool			is_all_finish	= false;
	while(pThis->is_run_merge && !is_all_finish){
		// check exited
		is_all_finish = true;

		while(!fullock::flck_trylock_noshared_mutex(&(pThis->mergeidmap_lockval)));	// LOCK
		for(mergeidmap_t::const_iterator iter = pThis->mergeidmap.begin(); iter != pThis->mergeidmap.end(); ++iter){
			if(!IS_PXCOM_REQ_UPDATE_RESULT(iter->second)){
				is_all_finish = false;
				break;
			}
		}
		fullock::flck_unlock_noshared_mutex(&(pThis->mergeidmap_lockval));			// UNLOCK

		if(pThis->is_run_merge && !is_all_finish){
			// sleep for wait
			nanosleep(&sleeptime, NULL);
		}
	}
	if(!pThis->is_run_merge){
		// If stop merging, exit this method immediately.
		// Do not change status, changing status is done by main thread which calls abort merging.
		return true;
	}

	// Done
	// change status.
	//
	if(pThis->is_run_merge){
		if(!pThis->MergeDone()){
			ERR_CHMPRN("Failed to change status \"DONE\".");
			pThis->is_run_merge = false;
			return true;			// keep running thread for next request
		}
	}
	pThis->is_run_merge = false;

	return true;
}

//---------------------------------------------------------
// Class Methods - Lock map
//---------------------------------------------------------
bool ChmEventSock::ServerSockMapCallback(const sock_ids_map_t::iterator& iter, void* psockobj)
{
	ChmEventSock*	pSockObj = reinterpret_cast<ChmEventSock*>(psockobj);
	if(!pSockObj){
		ERR_CHMPRN("Parameter is wrong.");
		return true;										// do not stop loop.
	}
	// get socket and im object
	int				sock	= iter->first;
	ChmIMData*		pImData	= pSockObj->pChmCntrl->GetImDataObj();
	if(pImData && CHM_INVALID_SOCK != sock){
		chmpxid_t	chmpxid	= pImData->GetChmpxIdBySock(sock, CLOSETG_SERVERS);
		if(CHM_INVALID_CHMPXID != chmpxid){
			if(chmpxid != iter->second){
				ERR_CHMPRN("Not same server chmpxid(0x%016" PRIx64 " : 0x%016" PRIx64 "), by socket in imdata and by parameter", chmpxid, iter->second);
			}else{
				MSG_CHMPRN("server chmpxid(0x%016" PRIx64 ") has socket(%d), then remove it associated from chmpxid.", chmpxid, sock);
				if(!pImData->RemoveServerSock(chmpxid, sock)){
					ERR_CHMPRN("Could not remove server sock(%d) from chmpxid(0x%016" PRIx64 "), but continue...", sock, chmpxid);
				}
			}
		}
	}
	// close
	if(CHM_INVALID_SOCK != sock){
		pSockObj->UnlockSendSock(sock);						// UNLOCK SOCK(For safety)
		pSockObj->CloseSocketWithEpoll(sock);				// Delete sock from epoll, shutdown ssl, close socket
	}
	return true;
}

bool ChmEventSock::SlaveSockMapCallback(const sock_ids_map_t::iterator& iter, void* psockobj)
{
	ChmEventSock*	pSockObj = reinterpret_cast<ChmEventSock*>(psockobj);
	if(!pSockObj){
		ERR_CHMPRN("Parameter is wrong.");
		return true;										// do not stop loop.
	}
	// get socket and im object
	int				sock	= iter->first;
	ChmIMData*		pImData	= pSockObj->pChmCntrl->GetImDataObj();
	if(pImData && CHM_INVALID_SOCK != sock){
		chmpxid_t	chmpxid = pImData->GetChmpxIdBySock(sock, CLOSETG_SLAVES);
		if(CHM_INVALID_CHMPXID != chmpxid){
			if(chmpxid != iter->second){
				ERR_CHMPRN("Not same slave chmpxid(0x%016" PRIx64 " : 0x%016" PRIx64 "), by socket in imdata and by parameter", chmpxid, iter->second);
			}else{
				MSG_CHMPRN("slave chmpxid(0x%016" PRIx64 ") has socket(%d), then remove it associated from chmpxid.", chmpxid, sock);
				if(!pImData->RemoveSlaveSock(chmpxid, sock)){
					ERR_CHMPRN("Could not remove slave sock(%d) from chmpxid(0x%016" PRIx64 "), but continue...", sock, chmpxid);
				}
			}
		}
	}
	// close
	if(CHM_INVALID_SOCK != sock){
		pSockObj->UnlockSendSock(sock);						// UNLOCK SOCK(For safety)
		pSockObj->CloseSocketWithEpoll(sock);				// Delete sock from epoll, shutdown ssl, close socket
	}
	return true;
}

bool ChmEventSock::AcceptMapCallback(const sock_pending_map_t::iterator& iter, void* psockobj)
{
	ChmEventSock*	pSockObj = reinterpret_cast<ChmEventSock*>(psockobj);
	if(!pSockObj){
		ERR_CHMPRN("Parameter is wrong.");
		return true;										// do not stop loop.
	}
	// get socket and im object
	int				sock	= iter->first;
	ChmIMData*		pImData	= pSockObj->pChmCntrl->GetImDataObj();
	if(pImData && CHM_INVALID_SOCK != sock){
		// check server/slave socket
		chmpxid_t	chmpxid	= pImData->GetChmpxIdBySock(sock, CLOSETG_SERVERS);
		if(CHM_INVALID_CHMPXID == chmpxid){
			chmpxid			= pImData->GetChmpxIdBySock(sock, CLOSETG_SLAVES);
		}
		if(CHM_INVALID_CHMPXID != chmpxid){
			ERR_CHMPRN("server or slave chmpxid(0x%016" PRIx64 ") has socket(%d), then we could not remove this.", chmpxid, sock);
			return true;
		}
	}
	// close
	if(CHM_INVALID_SOCK != sock){
		pSockObj->UnlockSendSock(sock);						// UNLOCK SOCK(For safety)
		pSockObj->CloseSocketWithEpoll(sock);				// Delete sock from epoll, shutdown ssl, close socket
	}
	return true;
}

bool ChmEventSock::ControlSockMapCallback(const sock_ids_map_t::iterator& iter, void* psockobj)
{
	ChmEventSock*	pSockObj = reinterpret_cast<ChmEventSock*>(psockobj);
	if(!pSockObj){
		ERR_CHMPRN("Parameter is wrong.");
		return true;										// do not stop loop.
	}
	// get socket and im object
	int				sock	= iter->first;
	ChmIMData*		pImData	= pSockObj->pChmCntrl->GetImDataObj();
	if(pImData && CHM_INVALID_SOCK != sock){
		// check server/slave socket
		chmpxid_t	chmpxid	= pImData->GetChmpxIdBySock(sock, CLOSETG_SERVERS);
		if(CHM_INVALID_CHMPXID == chmpxid){
			chmpxid			= pImData->GetChmpxIdBySock(sock, CLOSETG_SLAVES);
		}
		if(CHM_INVALID_CHMPXID != chmpxid){
			ERR_CHMPRN("server or slave chmpxid(0x%016" PRIx64 ") has socket(%d), then we could not remove this.", chmpxid, sock);
			return true;
		}
	}
	// close
	if(CHM_INVALID_SOCK != sock){
		pSockObj->UnlockSendSock(sock);						// UNLOCK SOCK(For safety)
		pSockObj->CloseSocketWithEpoll(sock);				// Delete sock from epoll, shutdown ssl, close socket
	}
	return true;
}

bool ChmEventSock::SslSockMapCallback(const sock_ssl_map_t::iterator& iter, void* psockobj)
{
	ChmEventSock*	pSockObj = reinterpret_cast<ChmEventSock*>(psockobj);
	if(!pSockObj){
		ERR_CHMPRN("Parameter is wrong.");
		return true;										// do not stop loop.
	}
	int				sock= iter->first;
	ChmSSSession	ssl	= iter->second;

	if(CHM_INVALID_SOCK != sock){
		pSockObj->UnlockSendSock(sock);						// UNLOCK SOCK
	}
	if(ssl){
		ChmSecureSock::ShutdownSSL(sock, ssl, pSockObj->con_retry_count, pSockObj->con_wait_time);
	}
	return true;
}

bool ChmEventSock::SendLockMapCallback(const sendlockmap_t::iterator& iter, void* psockobj)
{
	ChmEventSock*	pSockObj = reinterpret_cast<ChmEventSock*>(psockobj);
	if(!pSockObj){
		ERR_CHMPRN("Parameter is wrong.");
		return true;										// do not stop loop.
	}
	//int		sock	= iter->first;
	PCHMSSSTAT	pssstat	= iter->second;
	CHM_Delete(pssstat);
	return true;
}

//---------------------------------------------------------
// Methods
//---------------------------------------------------------
ChmEventSock::ChmEventSock(int eventqfd, ChmCntrl* pcntrl, bool is_ssl) : 
	ChmEventBase(eventqfd, pcntrl), pSecureSock(NULL),
	seversockmap(CHM_INVALID_CHMPXID, ChmEventSock::ServerSockMapCallback, this),
	slavesockmap(CHM_INVALID_CHMPXID, ChmEventSock::SlaveSockMapCallback, this),
	acceptingmap(string(""), ChmEventSock::AcceptMapCallback, this),
	ctlsockmap(CHM_INVALID_CHMPXID, ChmEventSock::ControlSockMapCallback, this),
	sslmap(NULL, ChmEventSock::SslSockMapCallback, this),
	sendlockmap(NULL, ChmEventSock::SendLockMapCallback, this),
	startup_servicein(true), is_run_merge(false),
	procthreads(CHMEVSOCK_SOCK_THREAD_NAME), mergethread(CHMEVSOCK_MERGE_THREAD_NAME),
	sockfd_lockval(FLCK_NOSHARED_MUTEX_VAL_UNLOCKED), last_check_time(time(NULL)), dyna_sockfd_lockval(FLCK_NOSHARED_MUTEX_VAL_UNLOCKED),
	mergeidmap_lockval(FLCK_NOSHARED_MUTEX_VAL_UNLOCKED), is_server_mode(false), max_sock_pool(ChmEventSock::DEFAULT_MAX_SOCK_POOL),
	sock_pool_timeout(ChmEventSock::DEFAULT_SOCK_POOL_TIMEOUT), sock_retry_count(ChmEventSock::DEFAULT_RETRYCNT),
	con_retry_count(ChmEventSock::DEFAULT_RETRYCNT_CONNECT), sock_wait_time(ChmEventSock::DEFAULT_WAIT_SOCKET),
	con_wait_time(ChmEventSock::DEFAULT_WAIT_CONNECT), comproto_ver(COM_VERSION_UNINIT), is_merge_done_processing(false)
{
	assert(pChmCntrl);

	// SSL
	if(is_ssl){
		ChmIMData*	pImData	= pChmCntrl->GetImDataObj();
		CHMPXSSL	ssldata;
		if(!pImData->GetSelfSsl(ssldata)){
			ERR_CHMPRN("Failed to get SSL structure from self chmpx, but continue...");
		}else{
			// cppcheck-suppress unmatchedSuppression
			// cppcheck-suppress noOperatorEq
			// cppcheck-suppress noCopyConstructor
			pSecureSock = new ChmSecureSock((!ssldata.is_ca_file ? ssldata.capath : NULL), (ssldata.is_ca_file ? ssldata.capath : NULL), ssldata.verify_peer);

			// Set SSL/TLS minimum version
			if(!ChmSecureSock::SetSslMinVersion(pImData->GetSslMinVersion())){
				ERR_CHMPRN("Failed to set SSL/TLS minimum version to ChmSecureSock class variable, but continue...");
			}

			// For only NSS now
			if(pImData->GetNssdbDir() && !ChmSecureSock::SetExtValue(CHM_NSS_NSSDB_DIR_KEY, pImData->GetNssdbDir())){
				ERR_CHMPRN("Failed to set %s to ChmSecureSock class variable, but continue...", CHM_NSS_NSSDB_DIR_KEY);
			}
		}
	}

	// first initialize cache value and threads
	//
	// [NOTE]
	// ChmEventSock must be initialized after initialize ChmIMData
	//
	if(!ChmEventSock::UpdateInternalData()){
		ERR_CHMPRN("Could not initialize cache data for ImData, but continue...");
	}
}

ChmEventSock::~ChmEventSock()
{
	// clear all
	ChmEventSock::Clean();
}

bool ChmEventSock::Clean(void)
{
	if(IsEmpty()){
		return true;
	}
	// exit merge thread
	is_run_merge = false;
	if(!mergethread.ExitAllThreads()){
		ERR_CHMPRN("Failed to exit thread for merging.");
	}
	// exit processing thread
	if(!procthreads.ExitAllThreads()){
		ERR_CHMPRN("Failed to exit thread for processing.");
	}
	// close sockets
	if(!CloseSelfSocks()){
		ERR_CHMPRN("Failed to close self sock and ctlsock, but continue...");
	}
	if(!CloseFromSlavesSocks()){
		ERR_CHMPRN("Failed to close connection from slaves, but continue...");
	}
	if(!CloseToServersSocks()){
		ERR_CHMPRN("Failed to close connection to other servers, but continue...");
	}
	if(!CloseCtlportClientSocks()){
		ERR_CHMPRN("Failed to close control port connection from clients, but continue...");
	}
	if(!CloseFromSlavesAcceptingSocks()){
		ERR_CHMPRN("Failed to close accepting connection from slaves, but continue...");
	}
	// Here, there are no SSL connection, but check it.
	CloseAllSSL();

	// SSL contexts
	if(pSecureSock && !pSecureSock->Clean()){
		ERR_CHMPRN("Failed to clean Contexts for Secure Socket etc, but continue...");
	}
	CHM_Delete(pSecureSock);

	// all socket lock are freed
	sendlockmap.clear();		// sendlockmap does not have default cb function & not call cb here --> do nothing

	return ChmEventBase::Clean();
}

//
// initialize/reinitialize cache value and threads
//
bool ChmEventSock::UpdateInternalData(void)
{
	ChmIMData*	pImData;
	if(!pChmCntrl || NULL == (pImData = pChmCntrl->GetImDataObj())){
		ERR_CHMPRN("Object is not pre-initialized.");
		return false;
	}

	// merge thread
	if(!IsDoMerge() && is_run_merge){
		// now work to merge, but we do do not stop it.
		WAN_CHMPRN("Re-initialize by configuration. new do_merge mode is false, but merging now. be careful about merging.");
	}
	if(IsDoMerge() && !mergethread.HasThread()){
		// do_merge false to true, we need to run merge thread.
		//
		//	- parameter is this object
		//	- sleep at starting
		//	- not at once(not one shot)
		//	- sleep after every working
		//	- keep event count
		//
		if(!mergethread.CreateThreads(1, ChmEventSock::MergeWorkerFunc, NULL, this, 0, true, false, false, true)){
			ERR_CHMPRN("Failed to create thread for merging on socket, but continue...");
		}else{
			MSG_CHMPRN("start to run merge thread on socket.");
		}
	}else{
		// [NOTE]
		// if new do_merge flag is false and already run thread, but we do not stop it.
	}

	// check first service in operation flag
	//
	// [NOTE]
	// The startup_servicein flag is always set true in constructor.
	// This flag is true, change status to SERVICE IN / ADD / PENDING and do SERVICE IN logic
	// when server node is first up.
	// If the server is not automerge mode or it is slave mode, this flag is set
	// false here. And this flag is set false after first SERVICE IN operation.
	//
	if((!pImData->IsServerMode() || !IsAutoMerge()) && startup_servicein){
		startup_servicein = false;
	}

	// processing thread
	int	conf_thread_cnt	= pImData->GetSocketThreadCount();		// we do not need to cache this value.
	int	now_thread_cnt	= procthreads.GetThreadCount();
	if(conf_thread_cnt < now_thread_cnt){
		if(DEFAULT_SOCK_THREAD_CNT == conf_thread_cnt){
			// stop all
			if(!procthreads.ExitAllThreads()){
				ERR_CHMPRN("Failed to exit thread for sock processing.");
			}else{
				MSG_CHMPRN("stop all sock processing thread(%d).", now_thread_cnt);
			}
		}else{
			// need to stop some thread
			if(!procthreads.ExitThreads(now_thread_cnt - conf_thread_cnt)){
				ERR_CHMPRN("Failed to exit thread for sock processing.");
			}else{
				MSG_CHMPRN("stop sock processing thread(%d - %d = %d).", now_thread_cnt, conf_thread_cnt, now_thread_cnt - conf_thread_cnt);
			}
		}
	}else if(now_thread_cnt < conf_thread_cnt){
		// need to run new threads
		//
		//	- parameter is NULL(because thread is sleep at start)
		//	- sleep at starting
		//	- not at once(not one shot)
		//	- sleep after every working
		//	- not keep event count
		//
		if(!procthreads.CreateThreads(conf_thread_cnt - now_thread_cnt, ChmEventSock::ReceiveWorkerProc, NULL, this, 0, true, false, false, false)){
			ERR_CHMPRN("Failed to create thread for sock processing, but continue...");
		}else{
			MSG_CHMPRN("start to run sock processing thread(%d + %d = %d).", now_thread_cnt, conf_thread_cnt - now_thread_cnt, conf_thread_cnt);
		}
	}else{
		// nothing to do because of same thread count
	}

	// others
	int			tmp_sock_retry_count= pImData->GetSockRetryCnt();
	suseconds_t	tmp_sock_wait_time	= pImData->GetSockTimeout();
	suseconds_t tmp_con_wait_time	= pImData->GetConnectTimeout();

	sock_retry_count	= (CHMEVENTSOCK_RETRY_DEFAULT == tmp_sock_retry_count ? ChmEventSock::DEFAULT_RETRYCNT : tmp_sock_retry_count);
	sock_wait_time		= (CHMEVENTSOCK_TIMEOUT_DEFAULT == tmp_sock_wait_time ? ChmEventSock::DEFAULT_WAIT_SOCKET : tmp_sock_wait_time);
	con_wait_time		= (CHMEVENTSOCK_TIMEOUT_DEFAULT == tmp_con_wait_time ? ChmEventSock::DEFAULT_WAIT_CONNECT : tmp_con_wait_time);
	con_retry_count		= (CHMEVENTSOCK_RETRY_DEFAULT == tmp_sock_retry_count ? ChmEventSock::DEFAULT_RETRYCNT_CONNECT : tmp_sock_retry_count);
	is_server_mode		= pImData->IsServerMode();
	max_sock_pool		= pImData->GetMaxSockPool();
	sock_pool_timeout	= pImData->GetSockPoolTimeout();

	return true;
}

bool ChmEventSock::GetEventQueueFds(event_fds_t& fds)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	fds.clear();

	ChmIMData*	pImData	= pChmCntrl->GetImDataObj();
	int			sock	= CHM_INVALID_SOCK;
	int			ctlsock	= CHM_INVALID_SOCK;
	if(!pImData->GetSelfSocks(sock, ctlsock)){
		ERR_CHMPRN("Could not get self sock and ctlsock.");
		return false;
	}
	if(CHM_INVALID_SOCK != sock){
		fds.push_back(sock);
	}
	if(CHM_INVALID_SOCK != ctlsock){
		fds.push_back(ctlsock);
	}
	seversockmap.get_keys(fds);
	slavesockmap.get_keys(fds);
	ctlsockmap.get_keys(fds);
	acceptingmap.get_keys(fds);
	return true;
}

bool ChmEventSock::SetEventQueue(void)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// first check for SSL
	if(!IsSafeParamsForSSL()){
		ERR_CHMPRN("Something wrong about SSL parameters, thus could not make SSL context.");
		return false;
	}

	// all status update.
	//
	// Following function gets all server(on ring) status from existed another chmpx server.
	// If got status includes self status, it is merges for self.
	//
	if(!InitialAllServerStatus()){
		ERR_CHMPRN("Could not update all server status.");
		return false;
	}

	// Get self port
	short	port	= CHM_INVALID_PORT;
	short	ctlport	= CHM_INVALID_PORT;
	if(!pImData->GetSelfPorts(port, ctlport) || CHM_INVALID_PORT == ctlport || (is_server_mode && CHM_INVALID_PORT == port)){
		ERR_CHMPRN("Could not get self port and ctlport.");
		return false;
	}

	struct epoll_event	eqevent;
	int		sock	= CHM_INVALID_SOCK;
	int		ctlsock	= CHM_INVALID_SOCK;

	// on only server mode, listen port
	if(is_server_mode){
		// listen port
		if(CHM_INVALID_SOCK == (sock = ChmEventSock::Listen(NULL, port))){
			ERR_CHMPRN("Could not listen port(%d).", port);
			return false;
		}
		// add event fd
		memset(&eqevent, 0, sizeof(struct epoll_event));
		eqevent.data.fd	= sock;
		eqevent.events	= EPOLLIN | EPOLLET | EPOLLRDHUP;			// EPOLLRDHUP is set
		if(-1 == epoll_ctl(eqfd, EPOLL_CTL_ADD, sock, &eqevent)){
			ERR_CHMPRN("Failed to add sock(port %d: sock %d) into epoll event(%d), errno=%d", port, sock, eqfd, errno);
			CHM_CLOSESOCK(sock);
			return false;
		}
	}

	// listen ctlport
	if(CHM_INVALID_SOCK == (ctlsock = ChmEventSock::Listen(NULL, ctlport))){
		ERR_CHMPRN("Could not listen ctlport(%d).", ctlport);
		if(CHM_INVALID_SOCK != sock){
			epoll_ctl(eqfd, EPOLL_CTL_DEL, sock, NULL);
		}
		CHM_CLOSESOCK(sock);
		return false;
	}

	// add event fd
	memset(&eqevent, 0, sizeof(struct epoll_event));
	eqevent.data.fd	= ctlsock;
	eqevent.events	= EPOLLIN | EPOLLET | EPOLLRDHUP;				// EPOLLRDHUP is set.
	if(-1 == epoll_ctl(eqfd, EPOLL_CTL_ADD, ctlsock, &eqevent)){
		ERR_CHMPRN("Failed to add sock(port %d: sock %d) into epoll event(%d), errno=%d", ctlport, ctlsock, eqfd, errno);
		if(CHM_INVALID_SOCK != sock){
			epoll_ctl(eqfd, EPOLL_CTL_DEL, sock, NULL);
		}
		CHM_CLOSESOCK(sock);
		CHM_CLOSESOCK(ctlsock);
		return false;
	}

	// set chmshm
	if(!pImData->SetSelfSocks(sock, ctlsock)){
		ERR_CHMPRN("Could not set self sock(%d) and ctlsock(%d) into chmshm.", sock, ctlsock);
		if(CHM_INVALID_SOCK != sock){
			epoll_ctl(eqfd, EPOLL_CTL_DEL, sock, NULL);
		}
		if(CHM_INVALID_SOCK != ctlsock){
			epoll_ctl(eqfd, EPOLL_CTL_DEL, ctlsock, NULL);
		}
		// cppcheck-suppress unmatchedSuppression
		// cppcheck-suppress unreadVariable
		CHM_CLOSESOCK(sock);
		// cppcheck-suppress unmatchedSuppression
		// cppcheck-suppress unreadVariable
		CHM_CLOSESOCK(ctlsock);
		return false;
	}

	//
	// re-check self status
	//
	// Here, no client joins this new chmpx, then force set SUSPEND status
	//
	chmpxsts_t	status		= pImData->GetSelfStatus();
	chmpxsts_t	old_status	= status;								// for messaging
	if(is_server_mode){
		if(IS_CHMPXSTS_SRVIN(status)){								// [SERVICE IN]
			if(IS_CHMPXSTS_UP(status)){								// [SERVICE IN] [UP]
				if(IS_CHMPXSTS_NOACT(status)){						// [SERVICE IN] [UP] [NOACT]
					if(IS_CHMPXSTS_NOTHING(status)){				// [SERVICE IN] [UP] [NOACT] [NOTHING]
						CHANGE_CHMPXSTS_TO_SUSPEND(status);
					}else if(IS_CHMPXSTS_PENDING(status)){			// [SERVICE IN] [UP] [NOACT] [PENDING]
						status = CHMPXSTS_SRVIN_UP_NORMAL;
						CHANGE_CHMPXSTS_TO_SUSPEND(status);
					}else{											// [SERVICE IN] [UP] [NOACT] [DOING/DONE]
						// This case is impossible. but force to set(NOACT NOTHING SUSPEND)
						status = CHMPXSTS_SRVIN_UP_NORMAL;
						CHANGE_CHMPXSTS_TO_SUSPEND(status);
					}
				}else if(IS_CHMPXSTS_ADD(status)){					// [SERVICE IN] [UP] [ADD]
					// This case is impossible. but force to set(ADD PENDING SUSPEND)
					status = CHMPXSTS_SRVIN_UP_ADDPENDING;
					CHANGE_CHMPXSTS_TO_SUSPEND(status);

				}else if(IS_CHMPXSTS_DELETE(status)){				// [SERVICE IN] [UP] [DELETE]
					if(IS_CHMPXSTS_PENDING(status)){				// [SERVICE IN] [UP] [DELETE] [PENDING]
						CHANGE_CHMPXSTS_TO_SUSPEND(status);
					}else if(IS_CHMPXSTS_DOING(status)){			// [SERVICE IN] [UP] [DELETE] [DOING]
						CHANGE_CHMPXSTS_TO_SUSPEND(status);
					}else if(IS_CHMPXSTS_DONE(status)){				// [SERVICE IN] [UP] [DELETE] [DONE]
						CHANGE_CHMPXSTS_TO_SUSPEND(status);
					}
				}
			}else{													// [SERVICE IN] [DOWN]
				if(IS_CHMPXSTS_NOACT(status)){						// [SERVICE IN] [DOWN] [NOACT]
					status = CHMPXSTS_SRVIN_UP_NORMAL;
					CHANGE_CHMPXSTS_TO_SUSPEND(status);
				}else{												// [SERVICE IN] [DOWN] [DELETE]
					if(IS_CHMPXSTS_PENDING(status)){				// [SERVICE IN] [DOWN] [DELETE] [PENDING]
						status = CHMPXSTS_SRVIN_UP_DELPENDING;
						CHANGE_CHMPXSTS_TO_SUSPEND(status);
					}else{											// [SERVICE IN] [DOWN] [DELETE] [DONE]
						status = CHMPXSTS_SRVIN_UP_DELETED;
						CHANGE_CHMPXSTS_TO_SUSPEND(status);
					}
				}
			}
		}else if(IS_CHMPXSTS_SRVOUT(status)){						// [SERVICE OUT]
			status = CHMPXSTS_SRVOUT_UP_NORMAL;
			CHANGE_CHMPXSTS_TO_SUSPEND(status);
		}else{														// [SLAVE]
			status = CHMPXSTS_SLAVE_UP_NORMAL;
		}
	}else{
		status = CHMPXSTS_SLAVE_UP_NORMAL;							// [SLAVE]
	}
	MSG_CHMPRN("initial chmpx status(0x%016" PRIx64 ":%s) from initial status(0x%016" PRIx64 ":%s) which get from another server in ring.", status, STR_CHMPXSTS_FULL(status).c_str(), old_status, STR_CHMPXSTS_FULL(old_status).c_str());

	// re-set self status(set suspend in this method)
	if(!pImData->SetSelfStatus(status)){
		CloseSelfSocks();
		return false;
	}

	// Connect(join ring/connect servers)
	if(is_server_mode){
		// server mode
		if(!ConnectRing()){
			ERR_CHMPRN("Failed to join RING(mode is server as SERVICE OUT/IN).");
			return false;
		}
	}else{
		// slave mode
		if(!ConnectServers()){
			ERR_CHMPRN("Failed to connect servers on RING(mode is SLAVE).");
			return false;
		}
	}
	return true;
}

bool ChmEventSock::UnsetEventQueue(void)
{
	return Clean();
}

bool ChmEventSock::IsEventQueueFd(int fd)
{
	if(CHM_INVALID_HANDLE == fd){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}

	ChmIMData*	pImData	= pChmCntrl->GetImDataObj();
	int			sock	= CHM_INVALID_SOCK;
	int			ctlsock	= CHM_INVALID_SOCK;
	if(!pImData->GetSelfSocks(sock, ctlsock)){
		ERR_CHMPRN("Could not get self sock and ctlsock, but continue...");
	}else{
		if(fd == sock || fd == ctlsock){
			return true;
		}
	}
	if(seversockmap.find(fd) || slavesockmap.find(fd) || ctlsockmap.find(fd) || acceptingmap.find(fd)){
		return true;
	}
	return false;
}

//------------------------------------------------------
// SSL/TLS
//------------------------------------------------------
ChmSSSession ChmEventSock::GetSSL(int sock)
{
	if(IsEmpty()){
		return NULL;
	}
	if(!sslmap.find(sock)){
		return NULL;
	}
	return sslmap[sock];
}

// [NOTE]
// This method is checking whichever the parameters for SSL(CA, cert, keys, etc) is no problem.
// So that, this method makes SSL context when SSL mode.
// If could not make SSL context, it means that those parameter are wrong.
//
bool ChmEventSock::IsSafeParamsForSSL(void)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData	= pChmCntrl->GetImDataObj();

	CHMPXSSL	ssldata;
	if(!pImData->GetSelfSsl(ssldata)){
		ERR_CHMPRN("Failed to get SSL structure from self chmpx.");
		return false;
	}
	if('\0' != ssldata.server_cert[0]){
		if(!pSecureSock->IsSafeSSLContext(ssldata.server_cert, ssldata.server_prikey, ssldata.slave_cert, ssldata.slave_prikey)){
			ERR_CHMPRN("Failed to make self SSL context for server socket.");
			return false;
		}
	}
	if('\0' != ssldata.slave_cert[0]){
		if(!pSecureSock->IsSafeSSLContext(NULL, NULL, ssldata.slave_cert, ssldata.slave_prikey)){
			ERR_CHMPRN("Failed to make self SSL context for slave socket.");
			return false;
		}
	}
	return true;
}

//---------------------------------------------------------
// Lock for socket(for server/slave node)
//---------------------------------------------------------
// [NOTE]
// This method uses and manages socket lock mapping.
// Because this library supports multi-thread for sending/receiving, this class needs to
// lock socket before sending. (but the control socket does not be needed to lock.)
//
// [NOTICE]
// For locking, we use sendlockmap_t class(template), and use directly lock variable in
// sendlockmap_t here.
//
bool ChmEventSock::RawLockSendSock(int sock, bool is_lock, bool is_once)
{
	if(CHM_INVALID_SOCK == sock){
		return false;
	}
	pid_t	tid		= gettid();
	bool	result	= false;
	for(bool is_loop = true; !result && is_loop; is_loop = !is_once){
		// get trylock
		if(!fullock::flck_trylock_noshared_mutex(&sendlockmap.lockval)){
			// [NOTE]
			// no wait and not sched_yield
			continue;
		}

		sendlockmap_t::iterator	iter = sendlockmap.basemap.find(sock);
		if(sendlockmap.basemap.end() == iter || NULL == iter->second){
			// there is no sock in map(or invalid pointer), so set new status data to map
			PCHMSSSTAT	pssstat			= new CHMSSSTAT;
			pssstat->tid				= is_lock ? tid : CHM_INVALID_TID;
			pssstat->last_time			= time(NULL);
			sendlockmap.basemap[sock]	= pssstat;
			result = true;
		}else{
			// found sock status in map
			PCHMSSSTAT	pssstat = iter->second;
			if(is_lock){
				if(CHM_INVALID_TID == pssstat->tid){
					// sock is unlocked.
					pssstat->tid = tid;
					result = true;
				}else if(tid == pssstat->tid){
					// same thread locked.
					result = true;
				}else{
					// other thread is locked, so looping
				}
			}else{
				if(CHM_INVALID_TID == pssstat->tid){
					// already unlocked
				}else{
					if(tid != pssstat->tid){
						MSG_CHMPRN("thread(%d) unlocked sock(%d) locked by thread(%d).", tid, sock, pssstat->tid);
					}
					pssstat->tid		= CHM_INVALID_TID;
					pssstat->last_time	= time(NULL);			// last_time is updated only at closing!
				}
				result = true;
			}
		}
 	 	fullock::flck_unlock_noshared_mutex(&sendlockmap.lockval);		// UNLOCK
	}
	return result;
}

//---------------------------------------------------------
// Methods for Communication base
//---------------------------------------------------------
//
// Lapped RawSend method with LockSendSock
//
bool ChmEventSock::LockedSend(int sock, ChmSSSession ssl, PCOMPKT pComPkt, bool is_blocking)
{
	LockSendSock(sock);			// LOCK SOCKET

	bool	is_closed = false;
	if(!ChmEventSock::RawSend(sock, ssl, pComPkt, is_closed, is_blocking, sock_retry_count, sock_wait_time)){
		ERR_CHMPRN("Failed to send COMPKT to sock(%d).", sock);
		UnlockSendSock(sock);	// UNLOCK SOCKET
		if(is_closed){
			if(!RawNotifyHup(sock)){
				ERR_CHMPRN("Failed to closing socket(%d), but continue...", sock);
			}
		}
		return false;
	}
	UnlockSendSock(sock);		// UNLOCK SOCKET

	return true;
}

//
// Lapped RawSend method with LockSendSock
//
bool ChmEventSock::LockedSend(int sock, ChmSSSession ssl, const unsigned char* pbydata, size_t length, bool is_blocking)
{
	LockSendSock(sock);			// LOCK SOCKET

	bool	is_closed = false;
	if(!ChmEventSock::RawSend(sock, ssl, pbydata, length, is_closed, is_blocking, sock_retry_count, sock_wait_time)){
		ERR_CHMPRN("Failed to send binary data to sock(%d).", sock);
		UnlockSendSock(sock);	// UNLOCK SOCKET
		if(is_closed){
			if(!RawNotifyHup(sock)){
				ERR_CHMPRN("Failed to closing socket(%d), but continue...", sock);
			}
		}
		return false;
	}
	UnlockSendSock(sock);		// UNLOCK SOCKET

	return true;
}

//
// [NOTE]
// We check socket count in each server here, and if it is over socket pool count,
// this method tries to close it.
// This logic is valid only when server socket.
//
// When this method opens/closes a socket, this locks dyna_sockfd_lockval every time.
// If dyna_sockfd_lockval is already locked, this does not check over max pool count
// and not open new socket, and continue to loop.
//
bool ChmEventSock::GetLockedSendSock(chmpxid_t chmpxid, int& sock, bool is_check_slave)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData		= pChmCntrl->GetImDataObj();
	bool		is_server	= pImData->IsServerChmpxId(chmpxid);

	if(!is_server && !is_check_slave){
		WAN_CHMPRN("Could not find chmpxid(0x%016" PRIx64 ") sock in slave list.", chmpxid);
		return false;
	}
	socklist_t	socklist;

	// check max socket pool (if over max sock pool count, close idle sockets which is over time limit)
	if(is_server && (last_check_time + sock_pool_timeout) < time(NULL)){
		// get socket list to server
		if(pImData->GetServerSock(chmpxid, socklist)){
			// check max socket pool (if over max sock pool count, close idle sockets which is over time limit)
			if(max_sock_pool < static_cast<int>(socklist.size())){
				if(fullock::flck_trylock_noshared_mutex(&dyna_sockfd_lockval)){			// LOCK(dynamic)
					for(socklist_t::reverse_iterator riter = socklist.rbegin(); riter != socklist.rend(); ){
						if(TryLockSendSock(*riter)){									// try to lock socket
							// Succeed to lock socket, so close this.
							sendlockmap.erase(*riter);									// remove socket from send lock map(with unlock it)
							if(seversockmap.find(*riter)){
								seversockmap.erase(*riter);								// close socket
							}

							// remove socket from socklist
							if((++riter) != socklist.rend()){
								socklist.erase((riter).base());
							}else{
								socklist.erase(socklist.begin());
								break;	// over the start position.
							}

							// check over max pool count
							if(static_cast<int>(socklist.size()) <= max_sock_pool){
								break;
							}
						}else{
							++riter;
						}
					}
					fullock::flck_unlock_noshared_mutex(&dyna_sockfd_lockval);			// UNLOCK(dynamic)

					last_check_time = time(NULL);
				}
			}
		}
	}

	// loop
	for(sock = CHM_INVALID_SOCK; CHM_INVALID_SOCK == sock; ){
		size_t	slv_sockcnt;
		slv_sockcnt	= 0;

		// at first, server socket
		if(is_server){
			// get socket list to server
			socklist.clear();
			if(!pImData->GetServerSock(chmpxid, socklist)){
				socklist.clear();
			}

			// try to lock existed socket
			for(socklist_t::const_iterator iter = socklist.begin(); iter != socklist.end(); ++iter){
				if(TryLockSendSock(*iter)){												// try to lock socket
					// Succeed to lock socket!
					sock = *iter;
					break;
				}
			}

			// check for closing another idle socket which is over time limit.
			//
			// [NOTE]
			// interval for checking is 5 times by sock_pool_timeout.
			// already locked one socket(sock), so minimum socket count is guaranteed)
			//
			if(CHM_INVALID_SOCK != sock && ChmEventSock::NO_SOCK_POOL_TIMEOUT < sock_pool_timeout && (last_check_time + (sock_pool_timeout * 5) < time(NULL))){
				if(fullock::flck_trylock_noshared_mutex(&dyna_sockfd_lockval)){			// LOCK(dynamic)
					time_t	nowtime = time(NULL);

					for(socklist_t::const_iterator iter = socklist.begin(); iter != socklist.end(); ++iter){
						if(sock == *iter){
							continue;	// skip
						}
						if(TryLockSendSock(*iter)){										// try to lock socket
							// get locked sock status.
							const PCHMSSSTAT	pssstat = sendlockmap.get(*iter);
							if(pssstat && (pssstat->last_time + sock_pool_timeout) < nowtime){
								// timeouted socket.
								sendlockmap.erase(*iter);								// remove socket from send lock map(with unlock it)
								if(seversockmap.find(*iter)){
									seversockmap.erase(*iter);							// close socket
								}
								// [NOTE]
								// we do not remove socket(iter) from socklist, because this loop will be broken soon.
								//
							}else{
								UnlockSendSock(*iter);									// unlock socket
							}
						}
					}
					fullock::flck_unlock_noshared_mutex(&dyna_sockfd_lockval);			// UNLOCK(dynamic)
				}
			}
		}

		// search socket in list of slave ( for a case of other server connect to server as slave )
		if(CHM_INVALID_SOCK == sock && is_check_slave){
			socklist.clear();
			if(!pImData->GetSlaveSock(chmpxid, socklist) || socklist.empty()){
				slv_sockcnt	= 0;
				if(!is_server){
					// there is no server, so no more checking.
					WAN_CHMPRN("Could not find chmpxid(0x%016" PRIx64 ") sock in slave list.", chmpxid);
					return false;
				}
			}else{
				slv_sockcnt	= socklist.size();

				// slave has socket list. next, search unlocked socket.
				bool	need_wait_unlock = false;
				for(socklist_t::const_iterator iter = socklist.begin(); iter != socklist.end(); ++iter){
					int	tmpsock = *iter;
					if(CHM_INVALID_SOCK != tmpsock){
						need_wait_unlock = true;
						if(TryLockSendSock(tmpsock)){									// try to lock socket
							// Succeed to lock socket!
							sock = tmpsock;
							break;
						}
					}
				}
				if(!is_server && !need_wait_unlock){
					// there is no server, so no more checking.
					WAN_CHMPRN("Could not find chmpxid(0x%016" PRIx64 ") sock in slave list.", chmpxid);
					return false;
				}
			}
		}

		// try to connect new socket ( there is no (idle) socket )
		if(CHM_INVALID_SOCK == sock && is_server){
			// reget socket list, and check socket pool count
			socklist.clear();
			if(pImData->GetServerSock(chmpxid, socklist) && static_cast<int>(socklist.size()) < max_sock_pool){
				if(fullock::flck_trylock_noshared_mutex(&dyna_sockfd_lockval)){			// LOCK(dynamic)
					// can connect new sock
					if(!RawConnectServer(chmpxid, sock, false, true) && CHM_INVALID_SOCK == sock){
						if(socklist.empty() && 0 == slv_sockcnt){
							// no server socket and could not connect, and no slave socket, so no more try.
							WAN_CHMPRN("Could not connect to server chmpxid(0x%016" PRIx64 ").", chmpxid);
							fullock::flck_unlock_noshared_mutex(&dyna_sockfd_lockval);	// UNLOCK(dynamic)
							return false;
						}
						MSG_CHMPRN("Could not connect to server chmpxid(0x%016" PRIx64 "), but continue...", chmpxid);
					}else{
						// Succeed to connect new sock and lock it.
					}
					fullock::flck_unlock_noshared_mutex(&dyna_sockfd_lockval);			// UNLOCK(dynamic)
				}
			}
		}
	}
	return true;
}

//
// If you make all COMPKT before calling this method, you can set NULL
// to pbody. If pbody is not NULL, this method builds COMPKT with body data.
//
bool ChmEventSock::Send(PCOMPKT pComPkt, const unsigned char* pbody, size_t blength)
{
	if(!pComPkt){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData	= pChmCntrl->GetImDataObj();

	// for stat
	bool			is_measure 	= (COM_C2C == pComPkt->head.type);
	chmpxid_t		term_chmpxid= pComPkt->head.term_ids.chmpxid;
	struct timespec	start_time;
	if(is_measure){
		STR_MATE_TIMESPEC(&start_time);
	}

	// check & build packet
	PCOMPKT	pPacked = pComPkt;
	bool	is_pack = false;
	if(pbody && 0L < blength){
		if(NULL == (pPacked = reinterpret_cast<PCOMPKT>(malloc(sizeof(COMPKT) + blength)))){
			// cppcheck-suppress unmatchedSuppression
			// cppcheck-suppress invalidPrintfArgType_sint
			ERR_CHMPRN("Could not allocate memory as %zd length.", sizeof(COMPKT) + blength);
			return false;
		}
		COPY_COMHEAD(&(pPacked->head), &(pComPkt->head));
		pPacked->length	= sizeof(COMPKT) + blength;
		pPacked->offset	= sizeof(COMPKT);

		unsigned char*	pdest = reinterpret_cast<unsigned char*>(pPacked) + sizeof(COMPKT);
		memcpy(pdest, pbody, blength);

		is_pack = true;
	}
	// for stat
	size_t	stat_length = pPacked->length;

	// get socket
	int	sock = CHM_INVALID_SOCK;
	if(!GetLockedSendSock(pComPkt->head.term_ids.chmpxid, sock, true) || CHM_INVALID_SOCK == sock){		// LOCK SOCKET
		WAN_CHMPRN("Could not find chmpxid(0x%016" PRIx64 ") sock.", pComPkt->head.term_ids.chmpxid);
		if(is_pack){
			CHM_Free(pPacked);
		}
		return false;
	}

	// send
	bool	is_closed = false;
	if(!ChmEventSock::RawSend(sock, GetSSL(sock), pPacked, is_closed, false, sock_retry_count, sock_wait_time)){
		ERR_CHMPRN("Failed to send COMPKT to sock(%d).", sock);
		UnlockSendSock(sock);			// UNLOCK SOCKET
		if(is_closed){
			if(!RawNotifyHup(sock)){
				ERR_CHMPRN("Failed to closing socket(%d), but continue...", sock);
			}
		}
		if(is_pack){
			CHM_Free(pPacked);
		}
		return false;
	}
	UnlockSendSock(sock);				// UNLOCK SOCKET

	// stat
	if(is_measure){
		struct timespec	fin_time;
		struct timespec	elapsed_time;
		FIN_MATE_TIMESPEC2(&start_time, &fin_time, &elapsed_time);

		if(!pImData->AddStat(term_chmpxid, true, stat_length, elapsed_time)){
			ERR_CHMPRN("Failed to update stat(send).");
		}
		if(pImData->IsTraceEnable() && !pImData->AddTrace(CHMLOG_TYPE_OUT_SOCKET, stat_length, start_time, fin_time)){
			ERR_CHMPRN("Failed to add trace log.");
		}
	}

	if(is_pack){
		CHM_Free(pPacked);
	}
	return true;
}

bool ChmEventSock::Receive(int fd)
{
	if(CHM_INVALID_HANDLE == fd){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}

	bool	result = false;
	if(procthreads.HasThread()){
		// Has processing threads, so run(do work) processing thread.
		chmthparam_t	wp_param = static_cast<chmthparam_t>(fd);

		if(false == (result = procthreads.DoWorkThread(wp_param))){
			ERR_CHMPRN("Failed to wake up thread for processing(receiving).");
		}
	}else{
		// Does not have processing thread, so call directly.
		//
		// [NOTE]
		// We use edge triggerd epoll, in this type the event is accumulated and not sent when the data to
		// the socket is left. Thus we must read more until EAGAIN. Otherwise we lost data.
		// So we checked socket for rest data here.
		//
		int			rtcode		= 0;
		chmpxid_t	tgchmpxid	= GetAssociateChmpxid(fd);
		string		tgachname	= GetAssociateAcceptHostName(fd);
		suseconds_t	waittime	= sock_wait_time;
		do{
			bool		is_closed;
			chmpxid_t	chmpxid = GetAssociateChmpxid(fd);
			string		achname	= GetAssociateAcceptHostName(fd);
			if(tgchmpxid != chmpxid || tgachname != achname){
				MSG_CHMPRN("Associate chmpxid of target fd(%d) was changed from old chmpxid(0x%016" PRIx64 ") or accept host(%s) to now chmpxid(0x%016" PRIx64 ") or accept host(%s).", fd, tgchmpxid, tgachname.c_str(), chmpxid, achname.c_str());
				result = true;
				break;
			}
			is_closed = false;
			if(false == (result = RawReceive(fd, is_closed))){
				if(!is_closed){
					ERR_CHMPRN("Failed to receive processing directly.");
				}else{
					MSG_CHMPRN("sock(%d) is closed while processing directly.", fd);
					result = true;
				}
				break;
			}
		}while(0 == (rtcode = ChmEventSock::WaitForReady(fd, WAIT_READ_FD, 0, false, waittime)));		// check rest data & return assap

		if(0 != rtcode && ETIMEDOUT != rtcode){
			chmpxid_t	chmpxid = GetAssociateChmpxid(fd);
			string		achname	= GetAssociateAcceptHostName(fd);
			if(tgchmpxid != chmpxid || tgachname != achname){
				MSG_CHMPRN("Associate chmpxid of target fd(%d) was changed from old chmpxid(0x%016" PRIx64 ") or accept host(%s) to now chmpxid(0x%016" PRIx64 ") or accept host(%s).", fd, tgchmpxid, tgachname.c_str(), chmpxid, achname.c_str());
			}else{
				if(RawNotifyHup(fd)){
					ERR_CHMPRN("Failed to closing socket(%d), but continue...", fd);
				}
			}
			result = true;
		}
	}
	return result;
}

chmpxid_t ChmEventSock::GetAssociateChmpxid(int fd)
{
	chmpxid_t	chmpxid	= CHM_INVALID_CHMPXID;

	if(CHM_INVALID_HANDLE == fd){
		ERR_CHMPRN("Parameter is wrong.");
		return chmpxid;
	}
	if(seversockmap.find(fd)){
		chmpxid = seversockmap[fd];
	}else if(slavesockmap.find(fd)){
		chmpxid = slavesockmap[fd];
	}
	return chmpxid;
}

string ChmEventSock::GetAssociateAcceptHostName(int fd)
{
	string	achname("");

	if(CHM_INVALID_HANDLE == fd){
		ERR_CHMPRN("Parameter is wrong.");
		return achname;
	}
	if(acceptingmap.find(fd)){
		achname = acceptingmap[fd];
	}
	return achname;
}

//
// If fd is sock, it means connecting from other servers.
// After receiving this method is processing own work. And if it needs to dispatch
// processing event after own processing, do it by calling control object.
//
// [NOTE]
// This method calls RawReceiveByte and RawReceiveAny methods, these methods read only packet size
// from socket. We use edge triggerd epoll, thus we must read more until EAGAIN. Otherwise we
// lost data. So we checked rest data in caller method.
//
bool ChmEventSock::RawReceive(int fd, bool& is_closed)
{
	is_closed = false;

	if(CHM_INVALID_HANDLE == fd){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}

	PCOMPKT		pComPkt = NULL;
	ChmIMData*	pImData	= pChmCntrl->GetImDataObj();
	int			sock	= CHM_INVALID_SOCK;
	int			ctlsock	= CHM_INVALID_SOCK;
	if(!pImData->GetSelfSocks(sock, ctlsock)){
		ERR_CHMPRN("Could not get self sock and ctlsock, but continue...");
	}

	// Processing
	if(fd == sock){
		// self port -> new connection
		if(!Accept(sock)){
			ERR_CHMPRN("Could not accept new connection.");
			return false;
		}

	}else if(fd == ctlsock){
		// self ctlport
		if(!AcceptCtlport(ctlsock)){
			ERR_CHMPRN("Could not accept new control port connection.");
			return false;
		}

	}else if(seversockmap.find(fd)){
		// server chmpx
		chmpxid_t	tgchmpxid	= seversockmap[fd];

		// for stat
		struct timespec	start_time;
		STR_MATE_TIMESPEC(&start_time);

		if(!ChmEventSock::RawReceive(fd, GetSSL(fd), is_closed, &pComPkt, 0L, false, sock_retry_count, sock_wait_time) || !pComPkt){
			CHM_Free(pComPkt);

			if(!is_closed){
				ERR_CHMPRN("Failed to receive ComPkt from sock(%d), sock is not closed.", fd);
			}else{
				MSG_CHMPRN("Failed to receive ComPkt from sock(%d), sock is closed.", fd);

				chmpxid_t	chmpxid = GetAssociateChmpxid(fd);
				if(tgchmpxid != chmpxid){
					MSG_CHMPRN("Associate chmpxid of target fd(%d) was changed from old chmpxid(0x%016" PRIx64 ") to now chmpxid(0x%016" PRIx64 ").", fd, tgchmpxid, chmpxid);
				}else{
					// close socket
					if(!RawNotifyHup(fd)){
						ERR_CHMPRN("Failed to closing \"to server socket\" for chmpxid(0x%016" PRIx64 "), but continue...", chmpxid);
					}
				}
			}
			return false;
		}
		// stat
		if(COM_C2C == pComPkt->head.type){
			struct timespec	fin_time;
			struct timespec	elapsed_time;
			FIN_MATE_TIMESPEC2(&start_time, &fin_time, &elapsed_time);

			if(!pImData->AddStat(pComPkt->head.dept_ids.chmpxid, false, pComPkt->length, elapsed_time)){
				ERR_CHMPRN("Failed to update stat(receive).");
			}
			if(pImData->IsTraceEnable() && !pImData->AddTrace(CHMLOG_TYPE_IN_SOCKET, pComPkt->length, start_time, fin_time)){
				ERR_CHMPRN("Failed to add trace log.");
			}
		}

	}else if(slavesockmap.find(fd)){
		// slave chmpx
		chmpxid_t	tgchmpxid	= slavesockmap[fd];

		// for stat
		struct timespec	start_time;
		STR_MATE_TIMESPEC(&start_time);

		if(!ChmEventSock::RawReceive(fd, GetSSL(fd), is_closed, &pComPkt, 0L, false, sock_retry_count, sock_wait_time) || !pComPkt){
			CHM_Free(pComPkt);

			if(!is_closed){
				ERR_CHMPRN("Failed to receive ComPkt from sock(%d), sock is not closed.", fd);
			}else{
				MSG_CHMPRN("Failed to receive ComPkt from sock(%d), sock is closed.", fd);

				chmpxid_t	chmpxid = GetAssociateChmpxid(fd);
				if(tgchmpxid != chmpxid){
					MSG_CHMPRN("Associate chmpxid of target fd(%d) was changed from old chmpxid(0x%016" PRIx64 ") to now chmpxid(0x%016" PRIx64 ").", fd, tgchmpxid, chmpxid);
				}else{
					// close socket
					if(!RawNotifyHup(fd)){
						ERR_CHMPRN("Failed to closing \"from slave socket\" for chmpxid(0x%016" PRIx64 "), but continue...", chmpxid);
					}
				}
			}
			return false;
		}
		// stat
		if(COM_C2C == pComPkt->head.type){
			struct timespec	fin_time;
			struct timespec	elapsed_time;
			FIN_MATE_TIMESPEC2(&start_time, &fin_time, &elapsed_time);

			if(!pImData->AddStat(pComPkt->head.dept_ids.chmpxid, false, pComPkt->length, elapsed_time)){
				ERR_CHMPRN("Failed to update stat(receive).");
			}
			if(pImData->IsTraceEnable() && !pImData->AddTrace(CHMLOG_TYPE_IN_SOCKET, pComPkt->length, start_time, fin_time)){
				ERR_CHMPRN("Failed to add trace log.");
			}
		}

	}else if(acceptingmap.find(fd)){
		// slave socket before accepting
		string	achname = acceptingmap[fd];

		if(!ChmEventSock::RawReceive(fd, GetSSL(fd), is_closed, &pComPkt, 0L, false, sock_retry_count, sock_wait_time) || !pComPkt){
			CHM_Free(pComPkt);

			if(!is_closed){
				ERR_CHMPRN("Failed to receive ComPkt from accepting sock(%d), sock is not closed.", fd);
			}else{
				MSG_CHMPRN("Failed to receive ComPkt from accepting sock(%d), sock is closed.", fd);

				string		nowachname	= GetAssociateAcceptHostName(fd);
				chmpxid_t	chmpxid		= GetAssociateChmpxid(fd);
				if(achname != nowachname || CHM_INVALID_CHMPXID != chmpxid){
					MSG_CHMPRN("Associate accepting hostname of target fd(%d) was changed from old host(%s) to now host(%s).", fd, achname.c_str(), nowachname.c_str());
				}else{
					// remove mapping
					if(!RawNotifyHup(fd)){
						ERR_CHMPRN("Failed to closing \"accepting socket\" for %s, but continue...", achname.c_str());
					}
				}
			}
			return false;
		}

		//
		// Accepting logic needs socket, so we processing here.
		//
		// There is a possibility of receiving N2_CONINIT_REQ(or CONINIT_REQ) or VERSION_REQ
		// from sockets being accepted. The following branches.
		//

		// check COMPKT(length, type, offset)
		if(COM_PX2PX != pComPkt->head.type){
			ERR_CHMPRN("ComPkt is wrong for accepting, type(%" PRIu64 ":%s).", pComPkt->head.type, STRCOMTYPE(pComPkt->head.type));
			CHM_Free(pComPkt);
			return false;
		}

		// check N2_CONINIT_REQ(or CONINIT_REQ) or VERSION_REQ
		PPXCOM_ALL	pComAll = CHM_OFFSET(pComPkt, pComPkt->offset, PPXCOM_ALL);
		if(CHMPX_COM_VERSION_REQ == pComAll->val_head.type){
			// Receive CHMPX_COM_VERSION_REQ

			// check COMPKT(length)
			if((sizeof(COMPKT) + sizeof(PXCOM_VERSION_REQ)) != pComPkt->length){
				ERR_CHMPRN("ComPkt is wrong for accepting: length(%zu)", pComPkt->length);
				CHM_Free(pComPkt);
				return false;
			}

			// Parse and Respond
			if(!PxComReceiveVersionReq(fd, &(pComPkt->head), pComAll)){
				ERR_CHMPRN("Received CHMPXCOM type(%" PRIu64 ":%s) and responed. Something error occured.", pComAll->val_head.type, STRPXCOMTYPE(pComAll->val_head.type));
				CHM_Free(pComPkt);
				return false;
			}
			CHM_Free(pComPkt);		// For stop dispatching

		}else if(CHMPX_COM_CONINIT_REQ == pComAll->val_head.type || CHMPX_COM_N2_CONINIT_REQ == pComAll->val_head.type){
			// Receive CHMPX_COM_N2_CONINIT_REQ(or CHMPX_COM_CONINIT_REQ)

			// check COMPKT(length, offset)
			if(	((sizeof(COMPKT) + sizeof(PXCOM_N2_CONINIT_REQ)) != pComPkt->length && (sizeof(COMPKT) + sizeof(PXCOM_CONINIT_REQ)) != pComPkt->length) ||
				sizeof(COMPKT) != pComPkt->offset )
			{
				ERR_CHMPRN("ComPkt is wrong for accepting: length(%zu), offset(%jd).", pComPkt->length, static_cast<intmax_t>(pComPkt->offset));
				CHM_Free(pComPkt);
				return false;
			}

			// Parse
			PCOMPKT			pResComPkt	= NULL;	// dummy
			comver_t		comver		= COM_VERSION_2;
			string			rawslvname;
			chmpxid_t		slvchmpxid	= CHM_INVALID_CHMPXID;
			short			ctlport		= CHM_INVALID_PORT;
			string			cuk;
			string			custom_seed;
			hostport_list_t	endpoints;
			hostport_list_t	ctlendpoints;
			hostport_list_t	forward_peers;
			hostport_list_t	reverse_peers;

			if(!PxComReceiveConinitReq(&(pComPkt->head), pComAll, &pResComPkt, comver, rawslvname, slvchmpxid, ctlport, cuk, custom_seed, endpoints, ctlendpoints, forward_peers, reverse_peers)){
				ERR_CHMPRN("Received CHMPXCOM type(%" PRIu64 ":%s). Something error occured.", pComAll->val_head.type, STRPXCOMTYPE(pComAll->val_head.type));
				CHM_Free(pComPkt);
				return false;
			}
			CHM_Free(pComPkt);

			// check ACL(with ctlport/cuk and get CHMPXSSL)
			//
			// [NOTE]
			// The hostname(IP address) passed is the name recognized by the slave.
			// Reverse lookup may not work if passed by FQDN. Also, for CONINIT_REQ
			// instead of N2_CONINIT_REQ, this value is always empty.
			// In these cases, use the accepted hostname(=slvname).
			//
			CHMPXSSL	ssl;
			string		slvname;
			if(!pImData->IsAllowHostStrictly(achname.c_str(), ctlport, cuk.c_str(), slvname, &ssl)){
				// Not allow accessing from slave.
				ERR_CHMPRN("Denied %s(sock:%d):%d by not allowed.", achname.c_str(), fd, ctlport);
				PxComSendConinitRes(fd, slvchmpxid, comver, CHMPX_COM_RES_ERROR);

				// close
				if(!RawNotifyHup(fd)){
					ERR_CHMPRN("Failed to closing \"accepting socket\" for %s, but continue...", achname.c_str());
				}
				is_closed = true;
				return false;
			}

			// Set nonblock
			if(!ChmEventSock::SetNonblocking(fd)){
				WAN_CHMPRN("Failed to nonblock socket(sock:%d), but continue...", fd);
			}

			// update slave list in CHMSHM
			//
			// [NOTICE]
			// All server status which connects to this server is "SLAVE".
			// If other server connects this server for chain of RING, then this server sets that server status as "SLAVE" in slaves list.
			// See: comment in chmstructure.h
			//
			if(!pImData->SetSlaveAll(slvchmpxid, slvname.c_str(), ctlport, cuk.c_str(), custom_seed.c_str(), &endpoints, &ctlendpoints, &forward_peers, &reverse_peers, &ssl, fd, CHMPXSTS_SLAVE_UP_NORMAL)){
				ERR_CHMPRN("Failed to add/update slave list, chmpxid=0x%016" PRIx64 ", hostname=%s(%s), ctlport=%d, sock=%d", slvchmpxid, slvname.c_str(), achname.c_str(), ctlport, fd);
				PxComSendConinitRes(fd, slvchmpxid, comver, CHMPX_COM_RES_ERROR);

				// close
				if(!RawNotifyHup(fd)){
					ERR_CHMPRN("Failed to closing \"accepting socket\" for %s(%s), but continue...", slvname.c_str(), achname.c_str());
				}
				is_closed = true;
				return false;
			}

			// add internal mapping
			slavesockmap.set(fd, slvchmpxid);
			// remove mapping
			acceptingmap.erase(fd, NULL, NULL);		// do not call callback because this socket is used by slave map.

			// Send CHMPX_COM_CONINIT_RES(success)
			if(!PxComSendConinitRes(fd, slvchmpxid, comver, CHMPX_COM_RES_SUCCESS)){
				ERR_CHMPRN("Failed to send CHMPX_COM_N2_CONINIT_RES(or CHMPX_COM_CONINIT_RES), but do(could) not recover for automatic closing.");
				return false;
			}

		}else{
			// Not allowed communication type
			ERR_CHMPRN("Received CHMPXCOM type(%" PRIu64 ":%s), but allow only CHMPX_COM_N2_CONINIT_REQ(or CHMPX_COM_CONINIT_REQ) or CHMPX_COM_VERSION_REQ.", pComAll->val_head.type, STRPXCOMTYPE(pComAll->val_head.type));
			CHM_Free(pComPkt);
			return false;
		}

	}else if(ctlsockmap.find(fd)){
		// control port
		char		szReceive[CTL_COMMAND_MAX_LENGTH];
		size_t		RecLength	= CTL_COMMAND_MAX_LENGTH - 1;
		chmpxid_t	chmpxid		= ctlsockmap[fd];		// [NOTICE] sock for control is always CHM_INVALID_CHMPXID

		// receive command
		memset(szReceive, 0, CTL_COMMAND_MAX_LENGTH);
		if(!ChmEventSock::RawReceiveAny(fd, is_closed, reinterpret_cast<unsigned char*>(szReceive), &RecLength, false, sock_retry_count, sock_wait_time)){
			if(!is_closed){
				ERR_CHMPRN("Failed to receive data from ctlport ctlsock(%d), ctlsock is not closed.", fd);
			}else{
				MSG_CHMPRN("Failed to receive data from ctlport ctlsock(%d), ctlsock is closed.", fd);
				if(!RawNotifyHup(fd)){
					ERR_CHMPRN("Failed to closing \"from control socket\" for chmpxid(0x%016" PRIx64 "), but continue...", chmpxid);
				}
			}
			return false;
		}
		if(0L == RecLength){
			MSG_CHMPRN("Receive null command on control port from ctlsock(%d)", fd);
			if(!RawNotifyHup(fd)){
				ERR_CHMPRN("Failed to closing \"from control socket\" for chmpxid(0x%016" PRIx64 "), but continue...", chmpxid);
			}
			return true;
		}
		szReceive[RecLength] = '\0';		// terminate(not need this)

		// processing command.
		if(!Processing(fd, szReceive)){
			ERR_CHMPRN("Failed to do command(%s) from ctlsock(%d)", szReceive, fd);
			if(!RawNotifyHup(fd)){
				ERR_CHMPRN("Failed to closing \"from control socket\" for chmpxid(0x%016" PRIx64 "), but continue...", chmpxid);
			}
			return false;
		}

		// close ctlport
		if(!RawNotifyHup(fd)){
			ERR_CHMPRN("Failed to closing \"from control socket\" for chmpxid(0x%016" PRIx64 "), but continue...", chmpxid);
		}
		return true;

	}else{
		// from other chmpx
		WAN_CHMPRN("Received from unknown sock(%d).", fd);

		if(!ChmEventSock::RawReceive(fd, GetSSL(fd), is_closed, &pComPkt, 0L, false, sock_retry_count, sock_wait_time) || !pComPkt){
			CHM_Free(pComPkt);

			if(!is_closed){
				ERR_CHMPRN("Failed to receive ComPkt from sock(%d), sock is not closed.", fd);
			}else{
				MSG_CHMPRN("Failed to receive ComPkt from sock(%d), sock is closed.", fd);
				if(!RawNotifyHup(fd)){
					ERR_CHMPRN("Failed to closing server(sock:%d), but continue...", fd);
				}
			}
			return false;
		}
	}

	// Dispatching
	if(pComPkt){
		if(pChmCntrl && !pChmCntrl->Processing(pComPkt, ChmCntrl::EVOBJ_TYPE_EVSOCK)){
			ERR_CHMPRN("Failed processing after receiving from socket.");
			CHM_Free(pComPkt);
			return false;
		}
		CHM_Free(pComPkt);
	}
	return true;
}

//
// [NOTICE]
// We set EPOLLRDHUP for socket event, then we will catch EPOLLRDHUP at here.
// EPOLLRDHUP has possibility of a problem that the packet just before FIN is replaced with FIN.
// So we need to receive rest data and 0 byte(EOF) in Receive() for closing.
//
bool ChmEventSock::NotifyHup(int fd)
{
	if(!Receive(fd)){
		ERR_CHMPRN("Failed to receive data from event fd(%d).", fd);
		return false;
	}
	return true;
}

//
// The connection which this server connect to other servers for joining RING
// when the mode is both server and slave is set EPOLLRDHUP for epoll.
// Then the connection occurs EPOLLRDHUP event by epoll, thus this method is
// called.
// This method closes the socket and reconnect next server on RING.
//
// [NOTE]
// This method is certainly close socket and remove epoll event.
//
bool ChmEventSock::RawNotifyHup(int fd)
{
	if(CHM_INVALID_HANDLE == fd){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		// try to close for safety
		while(!fullock::flck_trylock_noshared_mutex(&sockfd_lockval));	// LOCK
		CloseSocketWithEpoll(fd);										// not check result
		fullock::flck_unlock_noshared_mutex(&sockfd_lockval);			// UNLOCK
		return false;
	}

	while(!fullock::flck_trylock_noshared_mutex(&sockfd_lockval));		// LOCK
	bool	result	= true;

	// [NOTE]
	// following method call is with locking sockfd_lockval.
	// so should not connect to new servers while locking.
	//
	bool		closed	= false;
	chmpxid_t	chmpxid	= CHM_INVALID_CHMPXID;
	if(!ServerSockNotifyHup(fd, closed, chmpxid)){
		ERR_CHMPRN("Something error occured in closing server socket(%d) by HUP.", fd);
		result = false;
	}
	if(!closed && !SlaveSockNotifyHup(fd, closed)){
		ERR_CHMPRN("Something error occured in closing slave socket(%d) by HUP.", fd);
		result = false;
	}
	if(!closed && !AcceptingSockNotifyHup(fd, closed)){
		ERR_CHMPRN("Something error occured in closing accepting socket(%d) by HUP.", fd);
		result = false;
	}
	if(!closed && !ControlSockNotifyHup(fd, closed)){
		ERR_CHMPRN("Something error occured in closing control socket(%d) by HUP.", fd);
		result = false;
	}

	fullock::flck_unlock_noshared_mutex(&sockfd_lockval);				// UNLOCK

	// do rechain RING when closed fd is for servers.
	if(closed && CHM_INVALID_CHMPXID != chmpxid){
		if(!ServerDownNotifyHup(chmpxid)){
			ERR_CHMPRN("Something error occured in setting chmpxid(0x%016" PRIx64 ") - fd(%d) to down status.", chmpxid, fd);
			result = false;
		}
	}
	return result;
}

bool ChmEventSock::RawClearSelfWorkerStatus(int fd)
{
	if(CHM_INVALID_HANDLE == fd){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	bool	result	= true;

	// Clear worker thread queue
	//
	// [NOTE]
	// Must clear before close socket.
	//
	if(procthreads.HasThread()){
		int	sock = static_cast<int>(procthreads.GetSelfWorkerParam());
		if(sock == fd){
			// fd is worker thread's socket, so clear worker loop count(queued work count).
			if(!procthreads.ClearSelfWorkerStatus()){
				ERR_CHMPRN("Something error occured in clearing worker thread loop count for sock(%d), but continue...", fd);
				result = false;
			}
		}
	}
	return result;
}

// [NOTE]
// This method is almost the same as RequestMergeAbort().
// The difference with RequestMergeAbort() is that the checking of
// operating(merging) flag and call of notification to other servers are different.
//
bool ChmEventSock::ServerDownNotifyHup(chmpxid_t chmpxid)
{
	if(CHM_INVALID_CHMPXID == chmpxid){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	ChmIMData*	pImData	= pChmCntrl->GetImDataObj();

	// check server socket count
	socklist_t	socklist;
	int			ctlsock = CHM_INVALID_SOCK;
	if(pImData->GetServerSocks(chmpxid, socklist, ctlsock) && !socklist.empty()){
		MSG_CHMPRN("Caught Server Down notify(HUP) for chmpxid(0x%016" PRIx64 "), but this server still has some socket from this.", chmpxid);
		return true;
	}

	// backup operation mode.
	bool	is_operating = pImData->IsOperating();

	// check status and set new status
	chmpxsts_t	status		= pImData->GetServerStatus(chmpxid);
	chmpxsts_t	old_status	= status;

	CHANGE_CHMPXSTS_TO_DOWN(status);
	if(status == old_status){
		MSG_CHMPRN("chmpxid(0x%016" PRIx64 ") already has status(0x%016" PRIx64 ":%s) by server down notify hup, then nothing to do.", chmpxid, status, STR_CHMPXSTS_FULL(status).c_str());
		return true;
	}
	MSG_CHMPRN("chmpxid(0x%016" PRIx64 ") is changed status(0x%016" PRIx64 ":%s) from old status(0x%016" PRIx64 ":%s) by server down notify hup.", chmpxid, status, STR_CHMPXSTS_FULL(status).c_str(), old_status, STR_CHMPXSTS_FULL(old_status).c_str());

	if(!pImData->SetServerStatus(chmpxid, status)){
		ERR_CHMPRN("Could not set status(0x%016" PRIx64 ":%s) to chmpxid(0x%016" PRIx64 "), but continue...", status, STR_CHMPXSTS_FULL(status).c_str(), chmpxid);
	}

	if(!is_server_mode){
		// do nothing no more for slave mode.
		return true;
	}

	// get next chmpxid on RING
	chmpxid_t	nextchmpxid	= GetNextRingChmpxId();
	if(CHM_INVALID_CHMPXID != nextchmpxid){
		// rechain
		bool	is_rechain	= false;
		if(!CheckRechainRing(nextchmpxid, is_rechain)){
			ERR_CHMPRN("Something error occured in rechaining RING, but continue...");
		}else{
			if(is_rechain){
				MSG_CHMPRN("Rechained RING to chmpxid(0x%016" PRIx64 ") for down chmpxid(0x%016" PRIx64 ").", nextchmpxid, chmpxid);
			}else{
				MSG_CHMPRN("Not rechained RING to chmpxid(0x%016" PRIx64 ") for down chmpxid(0x%016" PRIx64 ").", nextchmpxid, chmpxid);
			}
		}
	}

	// [NOTICE]
	// If merging, at first stop merging before sending "SERVER_DOWN".
	// Then do not care about status and pending hash when receiving "SERVER_DOWN".
	//
	if(CHM_INVALID_CHMPXID == nextchmpxid){
		// there is no server on RING.
		if(is_operating){
			if(!MergeAbort()){
				ERR_CHMPRN("Got server down notice during merging, but failed to abort merge.");
				return false;
			}
		}
		// Check automerge
		if(IsAutoMerge()){
			// Request Service out
			if(!RequestServiceOut(chmpxid)){
				ERR_CHMPRN("Could not operate to SERVICEOUT by automerge for chmpxid(0x%016" PRIx64 ").", chmpxid);
				return false;
			}
		}
	}else{
		if(is_operating){
			if(!PxComSendMergeAbort(nextchmpxid)){
				ERR_CHMPRN("Got server down notice during merging, but failed to abort merge. but continue...");
			}
		}
		// send SERVER DOWN status
		if(!PxComSendServerDown(nextchmpxid, chmpxid)){
			ERR_CHMPRN("Failed to send down chmpx server information to chmpxid(0x%016" PRIx64 "), but continue...", nextchmpxid);
			return false;
		}
	}
	return true;
}

// [NOTE]
// The method which calls this method locks sockfd_lockval.
// So this method returns assap without doing any.
//
bool ChmEventSock::ServerSockNotifyHup(int fd, bool& is_close, chmpxid_t& chmpxid)
{
	// get sock to chmpxid
	ChmIMData*	pImData;
	if(!pChmCntrl || NULL == (pImData = pChmCntrl->GetImDataObj()) || CHM_INVALID_CHMPXID == (chmpxid = pImData->GetChmpxIdBySock(fd, CLOSETG_SERVERS))){
		MSG_CHMPRN("checked sock(%d) and it is not server sock(not found server sock list).", fd);
		chmpxid	= CHM_INVALID_CHMPXID;
	}

	// [NOTE]
	// we not check fd and object empty because NotifyHup() already checks it.
	//
	if(!seversockmap.find(fd)){
		MSG_CHMPRN("checked sock(%d) and there is not in server sock map.", fd);
		is_close = false;
	}else{
		// close socket at first.(check both server and slave)
		RawClearSelfWorkerStatus(fd);
		seversockmap.erase(fd);					// call default cb function
		is_close = true;
	}
	return true;
}

// [NOTE]
// The method which calls this method locks sockfd_lockval.
// So this method returns assap without doing any.
//
bool ChmEventSock::SlaveSockNotifyHup(int fd, bool& is_close)
{
	// [NOTE]
	// we not check fd and object empty because NotifyHup() already checks it.
	//
	if(!slavesockmap.find(fd)){
		MSG_CHMPRN("checked sock(%d) and there is not in slave sock map.", fd);
		is_close = false;
	}else{
		// close socket
		RawClearSelfWorkerStatus(fd);
		slavesockmap.erase(fd);					// call default cb function
		is_close = true;
	}
	return true;
}

// [NOTE]
// The method which calls this method locks sockfd_lockval.
// So this method returns assap without doing any.
//
bool ChmEventSock::AcceptingSockNotifyHup(int fd, bool& is_close)
{
	// [NOTE]
	// we not check fd and object empty because NotifyHup() already checks it.
	//
	if(!acceptingmap.find(fd)){
		MSG_CHMPRN("checked sock(%d) and there is not in accept sock map.", fd);
		is_close = false;
	}else{
		// close socket
		RawClearSelfWorkerStatus(fd);
		acceptingmap.erase(fd);					// call default cb function
		is_close = true;
	}
	return true;
}

// [NOTE]
// The method which calls this method locks sockfd_lockval.
// So this method returns assap without doing any.
//
bool ChmEventSock::ControlSockNotifyHup(int fd, bool& is_close)
{
	// [NOTE]
	// we not check fd and object empty because NotifyHup() already checks it.
	//
	if(!ctlsockmap.find(fd)){
		MSG_CHMPRN("checked sock(%d) and there is not in control sock map.", fd);
		is_close = false;
	}else{
		// close socket
		RawClearSelfWorkerStatus(fd);
		ctlsockmap.erase(fd);					// call default cb function
		is_close = true;
	}
	return true;
}

//---------------------------------------------------------
// Methods for Communication controls
//---------------------------------------------------------
bool ChmEventSock::CloseSelfSocks(void)
{
	if(IsEmpty()){
		return true;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// close own listen sockets
	int	sock	= CHM_INVALID_SOCK;
	int	ctlsock	= CHM_INVALID_SOCK;
	if(!pImData->GetSelfSocks(sock, ctlsock)){
		ERR_CHMPRN("Could not get self sock and ctlsock.");
		return false;
	}
	if(CHM_INVALID_SOCK != sock){
		CloseSocketWithEpoll(sock);
	}
	if(CHM_INVALID_SOCK != ctlsock){
		CloseSocketWithEpoll(ctlsock);
	}

	// update sock in chmshm
	bool	result = true;
	if(!pImData->SetSelfSocks(CHM_INVALID_SOCK, CHM_INVALID_SOCK)){
		ERR_CHMPRN("Could not clean self sock and ctlsock, but continue...");
		result = false;
	}

	// update status in chmshm
	chmpxsts_t	status		= pImData->GetSelfStatus();
	chmpxsts_t	old_status	= status;

	CHANGE_CHMPXSTS_TO_DOWN(status);
	MSG_CHMPRN("Status(0x%016" PRIx64 ":%s) is changed from old status(0x%016" PRIx64 ":%s).", status, STR_CHMPXSTS_FULL(status).c_str(), old_status, STR_CHMPXSTS_FULL(old_status).c_str());
	if(!pImData->SetSelfStatus(status)){
		ERR_CHMPRN("Could not set status(0x%016" PRIx64 ":%s) for self.", status, STR_CHMPXSTS_FULL(status).c_str());
		result = false;
	}

	return result;
}

bool ChmEventSock::CloseFromSlavesSocks(void)
{
	if(IsEmpty()){
		return true;
	}
	// close sockets connecting from clients
	slavesockmap.clear();			// call default cb function

	return true;
}

bool ChmEventSock::CloseToServersSocks(void)
{
	if(IsEmpty()){
		return true;
	}
	//ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// close all server ssl
	seversockmap.clear();		// call default cb function
	return true;
}

bool ChmEventSock::CloseCtlportClientSocks(void)
{
	if(IsEmpty()){
		return true;
	}
	// close all ctlsock
	ctlsockmap.clear();			// call default cb function
	return true;
}

bool ChmEventSock::CloseFromSlavesAcceptingSocks(void)
{
	if(IsEmpty()){
		return true;
	}
	// close accepting sockets connecting from clients
	acceptingmap.clear();			// call default cb function
	return true;
}

bool ChmEventSock::CloseSSL(int sock, bool with_sock)
{
	if(IsEmpty()){
		return true;
	}
	if(!sslmap.find(sock)){
		MSG_CHMPRN("checked sock(%d) and there is not in ssl sock map.", sock);
	}else{
		MSG_CHMPRN("checked sock(%d) and it is found in ssl sock map.", sock);
		sslmap.erase(sock);
	}
	if(with_sock){
		// cppcheck-suppress unmatchedSuppression
		// cppcheck-suppress uselessAssignmentPtrArg
		// cppcheck-suppress uselessAssignmentArg
		CHM_CLOSESOCK(sock);
	}
	return true;
}

bool ChmEventSock::CloseAllSSL(void)
{
	if(IsEmpty()){
		return true;
	}
	sslmap.clear();
	return true;
}

//
// [NOTE]
// Must lock sockfd_lockval outside.
//
bool ChmEventSock::CloseSocketWithEpoll(int sock)
{
	if(CHM_INVALID_SOCK == sock){
		return false;
	}
	if(0 != epoll_ctl(eqfd, EPOLL_CTL_DEL, sock, NULL)){
		// [NOTE]
		// Sometimes epoll_ctl returns error, because socket is not add epoll yet.
		//
		MSG_CHMPRN("Failed to delete socket(%d) from epoll event, probably already remove it.", sock);
	}
	CloseSSL(sock);					// close socket automatically in this method.

	return true;
}

//
// [NOTICE] This method blocks receiving data, means waits connecting.
// (See. RawConnectServer method)
//
bool ChmEventSock::ConnectServer(chmpxid_t chmpxid, int& sock, bool without_set_imdata)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// [NOTE]
	// This method checks only existing socket to chmpxid. Do not check lock
	// status for socket and count of socket.
	//
	socklist_t	socklist;
	if(pImData->GetServerSock(chmpxid, socklist) && !socklist.empty()){
		// already connected.
		sock = socklist.front();
		return true;
	}
	return RawConnectServer(chmpxid, sock, without_set_imdata, false);		// Do not lock socket
}

//
// [NOTICE] This method blocks receiving data, means waits connecting.
//
// This method try to connect chmpxid server, and send CHMPX_COM_N2_CONINIT_REQ request
// after connecting the server and receive CHMPX_COM_N2_CONINIT_RES. This is flow for
// connecting server.
// So that this method calls RawSend and RawReceive in PxComSendConinitReq().
//
bool ChmEventSock::RawConnectServer(chmpxid_t chmpxid, int& sock, bool without_set_imdata, bool is_lock)
{
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// get hostname/port
	string			hostname;
	short			port	= CHM_INVALID_PORT;
	short			ctlport	= CHM_INVALID_PORT;
	hostport_list_t	endpoints;
	if(!pImData->GetServerBase(chmpxid, &hostname, &port, &ctlport, &endpoints, NULL)){
		ERR_CHMPRN("Could not get hostname/port/ctlport/endpoints by chmpxid(0x%016" PRIx64 ").", chmpxid);
		return false;
	}

	// try to connect
	while(!fullock::flck_trylock_noshared_mutex(&sockfd_lockval));	// LOCK

	string	connected_host;
	short	connected_port	= CHM_INVALID_PORT;
	sock					= CHM_INVALID_SOCK;
	if(!endpoints.empty()){
		// If target has endpoints, try it
		for(hostport_list_t::const_iterator iter = endpoints.begin(); endpoints.end() != iter; ++iter){
			if(CHM_INVALID_SOCK != (sock = ChmEventSock::Connect(iter->host.c_str(), iter->port, false, con_retry_count, con_wait_time))){
				// connected
				MSG_CHMPRN("Connected to %s:%d for %s:%d endpoint, set sock(%d).", iter->host.c_str(), iter->port, hostname.c_str(), port, sock);
				connected_host = iter->host;
				connected_port = iter->port;
				break;
			}else{
				// could not connect, try another
				MSG_CHMPRN("Could not connect to %s:%d which is one of endpoints for %s:%d, try to connect another host/port.", iter->host.c_str(), iter->port, hostname.c_str(), port);
			}
		}
	}
	if(CHM_INVALID_SOCK == sock){
		// If do not connect endpoints, try to main host/port
		if(CHM_INVALID_SOCK == (sock = ChmEventSock::Connect(hostname.c_str(), port, false, con_retry_count, con_wait_time))){
			// could not connect
			MSG_CHMPRN("Could not connect to %s:%d, set status to DOWN.", hostname.c_str(), port);
			fullock::flck_unlock_noshared_mutex(&sockfd_lockval);		// UNLOCK
			return false;
		}
		// connected
		MSG_CHMPRN("Connected to %s:%d, set sock(%d).", hostname.c_str(), port, sock);
		connected_host = hostname;
		connected_port = port;
	}
	fullock::flck_unlock_noshared_mutex(&sockfd_lockval);			// UNLOCK

	// SSL
	CHMPXSSL	ssldata;
	if(!pImData->GetServerBase(chmpxid, ssldata)){
		ERR_CHMPRN("Failed to get SSL structure from chmpxid(0x%" PRIx64 ").", chmpxid);
		CHM_CLOSESOCK(sock);
		return false;
	}
	if(ssldata.is_ssl){		// Check whichever the target server is ssl
		// Get own keys
		if(!pImData->GetSelfSsl(ssldata)){
			ERR_CHMPRN("Failed to get SSL structure from self chmpx.");
			CHM_CLOSESOCK(sock);
			return false;
		}
		// get SSL context
		ChmSSCtx	ctx = pSecureSock->GetSSLContext(NULL, NULL, ssldata.slave_cert, ssldata.slave_prikey);
		if(!ctx){
			ERR_CHMPRN("Failed to get SSL context.");
			CHM_CLOSESOCK(sock);
			return false;
		}

		// Accept SSL
		ChmSSSession	ssl = ChmSecureSock::ConnectSSL(ctx, sock, con_retry_count, con_wait_time);
		pSecureSock->FreeSSLContext(ctx);
		if(!ssl){
			ERR_CHMPRN("Failed to connect SSL.");
			CHM_CLOSESOCK(sock);
			return false;
		}

		// Set SSL mapping
		sslmap.set(sock, ssl, true);		// over write whether exitsts or does not.
	}

	// Check/Get communication protocol version
	if(COM_VERSION_UNINIT == comproto_ver){
		if(!InitialComVersion(sock, chmpxid, comproto_ver)){
			ERR_CHMPRN("Failed to send/receive PXCOM_VERSION_REQ.");
			CloseSSL(sock);
			return false;
		}
	}

	// Send coninit_req
	if(!PxComSendConinitReq(sock, chmpxid)){
		ERR_CHMPRN("Failed to send PCCOM_N2_CONINIT_REQ(or PCCOM_CONINIT_REQ) to sock(%d).", sock);
		CloseSSL(sock);
		return false;
	}

	// Receive response
	// 
	// [NOTICE]
	// retry count is sock_retry_count
	// 
	bool	is_closed	= false;
	PCOMPKT	pComPkt		= NULL;
	if(!ChmEventSock::RawReceive(sock, GetSSL(sock), is_closed, &pComPkt, 0L, false, sock_retry_count, sock_wait_time) || !pComPkt){
		ERR_CHMPRN("Failed to receive ComPkt from sock(%d), sock is %s.", sock, is_closed ? "closed" : "not closed");
		CloseSSL(sock);
		CHM_Free(pComPkt);
		return false;
	}

	// Check response type
	if(COM_PX2PX != pComPkt->head.type){
		ERR_CHMPRN("Received data is %s(%" PRIu64 ") type, which does not COM_PX2PX type from sock(%d).", STRCOMTYPE(pComPkt->head.type), pComPkt->head.type, sock);
		CloseSSL(sock);
		CHM_Free(pComPkt);
		return false;
	}
	PPXCOM_ALL	pChmpxCom = CHM_OFFSET(pComPkt, pComPkt->offset, PPXCOM_ALL);
	if(CHMPX_COM_CONINIT_RES != pChmpxCom->val_head.type && CHMPX_COM_N2_CONINIT_RES != pChmpxCom->val_head.type){
		ERR_CHMPRN("Received data is %s(%" PRIu64 ") type, which does not CHMPX_COM_N2_CONINIT_RES(or CHMPX_COM_CONINIT_RES) type from sock(%d).", STRPXCOMTYPE(pChmpxCom->val_head.type), pChmpxCom->val_head.type, sock);
		CloseSSL(sock);
		CHM_Free(pComPkt);
		return false;
	}

	// Check response
	PCOMPKT		pResComPkt		= NULL;	// tmp
	pxcomres_t	coninit_result	= CHMPX_COM_RES_ERROR;
	if(!PxComReceiveConinitRes(&(pComPkt->head), pChmpxCom, &pResComPkt, coninit_result)){
		ERR_CHMPRN("Received CHMPXCOM type(%" PRIu64 ":%s). Something error occured.", pChmpxCom->val_head.type, STRPXCOMTYPE(pChmpxCom->val_head.type));
		CloseSSL(sock);
		CHM_Free(pComPkt);
		CHM_Free(pResComPkt);
		return false;
	}
	CHM_Free(pComPkt);
	CHM_Free(pResComPkt);

	if(CHMPX_COM_RES_SUCCESS != coninit_result){
		if(connected_host == hostname && connected_port == port){
			ERR_CHMPRN("Connected to %s:%d, but could not allow from this server.", hostname.c_str(), port);
		}else{
			ERR_CHMPRN("Connected to %s:%d(endpoint for %s:%d), but could not allow from this server.", connected_host.c_str(), connected_port, hostname.c_str(), port);
		}
		CloseSSL(sock);
		return false;
	}

	// Lock
	if(is_lock){
		LockSendSock(sock);									// LOCK SOCKET
	}

	// Set IM data
	if(!without_set_imdata){
		// set mapping
		seversockmap.set(sock, chmpxid);

		// add event fd
		struct epoll_event	eqevent;
		memset(&eqevent, 0, sizeof(struct epoll_event));
		eqevent.data.fd	= sock;
		eqevent.events	= EPOLLIN | EPOLLET | EPOLLRDHUP;	// EPOLLRDHUP is set, connecting to server socket is needed to know server side down.
		if(-1 == epoll_ctl(eqfd, EPOLL_CTL_ADD, sock, &eqevent)){
			if(connected_host == hostname && connected_port == port){
				ERR_CHMPRN("Failed to add sock(%s:%d - sock %d) into epoll event(%d), errno=%d", hostname.c_str(), port, sock, eqfd, errno);
			}else{
				ERR_CHMPRN("Failed to add sock(%s:%d for %s:%d - sock %d) into epoll event(%d), errno=%d", connected_host.c_str(), connected_port, hostname.c_str(), port, sock, eqfd, errno);
			}
			if(is_lock){
				UnlockSendSock(sock);						// UNLOCK SOCKET
			}
			seversockmap.erase(sock, NULL, NULL);			// do not call callback
			CloseSSL(sock);
			return false;
		}
		// set sock in server list and update status
		if(!pImData->SetServerSocks(chmpxid, sock, CHM_INVALID_SOCK, SOCKTG_SOCK)){
			if(connected_host == hostname && connected_port == port){
				MSG_CHMPRN("Could not set sock(%d - %s:%d) to chmpxid(0x%016" PRIx64 ").", sock, hostname.c_str(), port, chmpxid);
			}else{
				MSG_CHMPRN("Could not set sock(%d - %s:%d for %s:%d) to chmpxid(0x%016" PRIx64 ").", sock, connected_host.c_str(), connected_port, hostname.c_str(), port, chmpxid);
			}
			if(is_lock){
				UnlockSendSock(sock);						// UNLOCK SOCKET
			}
			seversockmap.erase(sock, NULL, NULL);			// do not call callback
			CloseSSL(sock);
			return false;
		}
	}
	return true;
}

//
// This method try to connect all server, and updates chmshm status for each server
// when connecting servers.
//
// [NOTE]
// This method is called for not server mode(slave) now.
//
bool ChmEventSock::ConnectServers(void)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}

	ChmIMData*	pImData = pChmCntrl->GetImDataObj();
	if(is_server_mode){
		ERR_CHMPRN("Chmpx mode does not slave mode.");
		return false;
	}

	bool	need_reload_conf = false;
	for(chmpxpos_t pos = pImData->GetNextServerPos(CHM_INVALID_CHMPXLISTPOS, CHM_INVALID_CHMPXLISTPOS, false, false); CHM_INVALID_CHMPXLISTPOS != pos; pos = pImData->GetNextServerPos(CHM_INVALID_CHMPXLISTPOS, pos, false, false)){
		// get base information
		string		name;
		chmpxid_t	chmpxid	= CHM_INVALID_CHMPXID;
		if(!pImData->GetServerBase(pos, &name, &chmpxid, NULL, NULL, NULL, NULL) || CHM_INVALID_CHMPXID == chmpxid){
			WAN_CHMPRN("Could not get server name/chmpxid for pos(%" PRIu64 ").", pos);
			continue;
		}

		// status check
		chmpxsts_t	status		= pImData->GetServerStatus(chmpxid);
		chmpxsts_t	old_status	= status;
		if(!IS_CHMPXSTS_SERVER(status) || !IS_CHMPXSTS_UP(status)){
			WAN_CHMPRN("%s:chmpxid(0x%016" PRIx64 ")  which is pos(%" PRIu64 ") is status(0x%016" PRIx64 ":%s), not enough status to connecting.", name.c_str(), chmpxid, pos, status, STR_CHMPXSTS_FULL(status).c_str());
			if(CHMPXSTS_SRVOUT_DOWN_NORMAL == status){
				// need to recheck all configuration
				need_reload_conf = true;
			}
			continue;
		}

		// check socket to server and try to connect if not exist.
		int	sock = CHM_INVALID_SOCK;
		if(!ConnectServer(chmpxid, sock, false) || CHM_INVALID_SOCK == sock){		// connect & set epoll & chmshm
			// could not connect
			ERR_CHMPRN("Could not connect to %s:chmpxid(0x%016" PRIx64 ") which is pos(%" PRIu64 "), set status to DOWN.", name.c_str(), chmpxid, pos);

			// [NOTICE]
			// Other server status is updated, because DOWN status can not be sent by that server.
			//
			CHANGE_CHMPXSTS_TO_DOWN(status);
			MSG_CHMPRN("%s:chmpxid(0x%016" PRIx64 ") status(0x%016" PRIx64 ":%s) is changed from old status(0x%016" PRIx64 ":%s).", name.c_str(), chmpxid, status, STR_CHMPXSTS_FULL(status).c_str(), old_status, STR_CHMPXSTS_FULL(old_status).c_str());
			if(!pImData->SetServerStatus(chmpxid, status)){
				MSG_CHMPRN("Could not set status(0x%016" PRIx64 ":%s) by down to %s:chmpxid(%" PRIu64 ") which is pos(%" PRIu64 ").", status, STR_CHMPXSTS_FULL(status).c_str(), name.c_str(), chmpxid, pos);
				continue;
			}
		}
	}

	if(need_reload_conf){
		// [NOTE]
		// If there is a server in SERVICEOUT / DOWN state, it is necessary to
		// check if the server is set in Configuration. If it does not exist,
		// it will delete the entry.
		if(!pImData->ReloadConfiguration()){
			WAN_CHMPRN("Failed to reload configuration connecting servers because one server is SERVICEOUT / DOWN, but continue...");
		}
	}

	return true;
}

bool ChmEventSock::ConnectRing(void)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// Check self status, must be service out
	if(!is_server_mode){
		ERR_CHMPRN("Chmpx mode does not server-mode.");
		return false;
	}
	chmpxsts_t	selfstatus = pImData->GetSelfStatus();
	if(!IS_CHMPXSTS_SERVER(selfstatus) || !IS_CHMPXSTS_UP(selfstatus)){
		ERR_CHMPRN("This server is status(0x%016" PRIx64 ":%s), not enough of self status for connecting.", selfstatus, STR_CHMPXSTS_FULL(selfstatus).c_str());
		return false;
	}

	// Get self pos
	chmpxpos_t	selfpos = pImData->GetSelfServerPos();
	if(CHM_INVALID_CHMPXLISTPOS == selfpos){
		ERR_CHMPRN("Not found self chmpx data in list.");
		return false;
	}

	int			sock	= CHM_INVALID_SOCK;
	chmpxpos_t	pos		= selfpos;	// Start at self pos
	for(pos = pImData->GetNextServerPos(selfpos, pos, true, true); CHM_INVALID_CHMPXLISTPOS != pos; pos = pImData->GetNextServerPos(selfpos, pos, true, true)){
		// get server information
		string		name;
		chmpxid_t	chmpxid	= CHM_INVALID_CHMPXID;
		if(!pImData->GetServerBase(pos, &name, &chmpxid, NULL, NULL, NULL, NULL) || CHM_INVALID_CHMPXID == chmpxid){
			WAN_CHMPRN("Could not get serer name/chmpxid for pos(%" PRIu64 ").", pos);
			continue;
		}

		// status check
		chmpxsts_t	status = pImData->GetServerStatus(chmpxid);
		if(!IS_CHMPXSTS_SERVER(status) || !IS_CHMPXSTS_UP(status)){
			WAN_CHMPRN("%s:chmpxid(0x%016" PRIx64 ") which is pos(%" PRIu64 ") is status(0x%016" PRIx64 ":%s), not enough status to connecting.", name.c_str(), chmpxid, pos, status, STR_CHMPXSTS_FULL(status).c_str());
			continue;
		}

		// try to connect
		sock = CHM_INVALID_SOCK;
		if(!ConnectServer(chmpxid, sock, false) || CHM_INVALID_SOCK == sock){	// connect & set epoll & chmshm
			// could not connect
			WAN_CHMPRN("Could not connect to %s:chmpxid(0x%016" PRIx64 ") which is pos(%" PRIu64 "), set status to DOWN.", name.c_str(), chmpxid, pos);
		}else{
			// Update self last status updating time
			if(!pImData->UpdateSelfLastStatusTime()){
				ERR_CHMPRN("Could not update own updating status time, but continue...");
			}

			// send CHMPX_COM_JOIN_RING
			CHMPXSVR	selfchmpxsvr;
			if(!pImData->GetSelfChmpxSvr(&selfchmpxsvr)){
				ERR_CHMPRN("Could not get self chmpx information, this error can not recover.");
				return false;
			}
			if(!PxComSendJoinRing(chmpxid, &selfchmpxsvr)){
				// Failed to join.
				ERR_CHMPRN("Failed to send CHMPX_COM_N2_JOIN_RING(or CHMPX_COM_JOIN_RING) to %s:chmpxid(0x%016" PRIx64 ") on RING, so close connection to servers.", name.c_str(), chmpxid);
				if(!CloseToServersSocks()){
					ERR_CHMPRN("Failed to close connection to other servers, but continue...");
				}
				return false;
			}
			break;
		}
	}
	if(CHM_INVALID_SOCK == sock){
		//
		// This case is there is no server UP on RING.
		// It means this server is first UP on RING.
		//
		ERR_CHMPRN("There is no server UP on RING.");
		return true;
	}
	return true;
}

bool ChmEventSock::CloseRechainRing(chmpxid_t nowchmpxid)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData	= pChmCntrl->GetImDataObj();

	chmpxpos_t	selfpos	= pImData->GetSelfServerPos();
	if(CHM_INVALID_CHMPXLISTPOS == selfpos){
		ERR_CHMPRN("Not found self chmpx data in list.");
		return false;
	}

	chmpxpos_t	pos = selfpos;	// Start at self pos
	for(pos = pImData->GetNextServerPos(selfpos, pos, true, true); CHM_INVALID_CHMPXLISTPOS != pos; pos = pImData->GetNextServerPos(selfpos, pos, true, true)){
		string		name;
		chmpxid_t	chmpxid	= CHM_INVALID_CHMPXID;
		if(!pImData->GetServerBase(pos, &name, &chmpxid, NULL, NULL, NULL, NULL) || CHM_INVALID_CHMPXID == chmpxid){
			WAN_CHMPRN("Could not get server name/chmpxid for pos(%" PRIu64 ") in server list, but continue...", pos);
			continue;
		}
		if(nowchmpxid == chmpxid){
			continue;
		}

		// [NOTE]
		// Close all sockets to servers except next server.
		// 
		socklist_t	socklist;
		socklist.clear();
		if(!pImData->GetServerSock(chmpxid, socklist) || socklist.empty()){
			WAN_CHMPRN("Not have connection to %s:chmpxid(0x%016" PRIx64 ") which is pos(%" PRIu64 ").", name.c_str(), chmpxid, pos);
			continue;
		}
		// close
		for(socklist_t::iterator iter = socklist.begin(); iter != socklist.end(); iter = socklist.erase(iter)){
			seversockmap.erase(*iter);
		}
	}
	return true;
}

bool ChmEventSock::RechainRing(chmpxid_t newchmpxid)
{
	if(CHM_INVALID_CHMPXID == newchmpxid){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}

	// connect new server.
	int	newsock = CHM_INVALID_SOCK;
	if(!ConnectServer(newchmpxid, newsock, false) || CHM_INVALID_SOCK == newsock){	// update chmshm
		ERR_CHMPRN("Could not connect new server chmpxid=0x%016" PRIx64 ", and could not recover about this error.", newchmpxid);
		return false;
	}

	if(!CloseRechainRing(newchmpxid)){
		ERR_CHMPRN("Something error occured, close old chain ring except chmpxid(0x%016" PRIx64 ").", newchmpxid);
		return false;
	}
	return true;
}

bool ChmEventSock::CheckRechainRing(chmpxid_t newchmpxid, bool& is_rechain)
{
	if(CHM_INVALID_CHMPXID == newchmpxid){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	is_rechain = false;

	ChmIMData*	pImData		= pChmCntrl->GetImDataObj();
	chmpxid_t	nowchmpxid	= pImData->GetNextRingChmpxId();
	if(nowchmpxid == newchmpxid){
		socklist_t	nowsocklist;
		if(pImData->GetServerSock(nowchmpxid, nowsocklist) && !nowsocklist.empty()){
			MSG_CHMPRN("New next chmpxid(0x%016" PRIx64 ") is same as now chmpxid(0x%016" PRIx64 "), so do not need rechain RING.", newchmpxid, nowchmpxid);
			return true;
		}
	}

	chmpxpos_t	selfpos = pImData->GetSelfServerPos();
	if(CHM_INVALID_CHMPXLISTPOS == selfpos){
		ERR_CHMPRN("Not found self chmpx data in list.");
		return false;
	}

	chmpxpos_t	pos = selfpos;	// Start at self pos
	for(pos = pImData->GetNextServerPos(selfpos, pos, true, true); CHM_INVALID_CHMPXLISTPOS != pos; pos = pImData->GetNextServerPos(selfpos, pos, true, true)){
		string		name;
		chmpxid_t	chmpxid	= CHM_INVALID_CHMPXID;
		if(!pImData->GetServerBase(pos, &name, &chmpxid, NULL, NULL, NULL, NULL) || CHM_INVALID_CHMPXID == chmpxid){
			WAN_CHMPRN("Could not get server name/chmpxid for pos(%" PRIu64 ") in server list, but continue...", pos);
			continue;
		}
		if(nowchmpxid == chmpxid){
			socklist_t	nowsocklist;
			if(pImData->GetServerSock(nowchmpxid, nowsocklist) && !nowsocklist.empty()){
				MSG_CHMPRN("Found now next %s:chmpxid(0x%016" PRIx64 ") before new chmpxid(0x%016" PRIx64 "), so do not need rechain RING.", name.c_str(), nowchmpxid, newchmpxid);
			}else{
				// do rechain
				if(!RechainRing(nowchmpxid)){
					ERR_CHMPRN("Failed to rechain RING for %s:chmpxid(0x%016" PRIx64 ").", name.c_str(), nowchmpxid);
				}else{
					MSG_CHMPRN("Succeed to rechain RING for %s:chmpxid(0x%016" PRIx64 ").", name.c_str(), nowchmpxid);
					is_rechain = true;
				}
			}
			return true;

		}else if(newchmpxid == chmpxid){
			MSG_CHMPRN("Found new %s:chmpxid(0x%016" PRIx64 ") before next chmpxid(0x%016" PRIx64 "), so need rechain RING.", name.c_str(), newchmpxid, nowchmpxid);

			// do rechain
			if(!RechainRing(newchmpxid)){
				ERR_CHMPRN("Failed to rechain RING for %s:chmpxid(0x%016" PRIx64 ").", name.c_str(), newchmpxid);
			}else{
				MSG_CHMPRN("Succeed to rechain RING for %s:chmpxid(0x%016" PRIx64 ").", name.c_str(), newchmpxid);
				is_rechain = true;
			}
			return true;
		}
	}
	// why...?
	MSG_CHMPRN("There is no server UP on RING(= there is not the server of chmpxid(0x%016" PRIx64 ") in server list), so can not rechain RING.", newchmpxid);

	return true;
}

//
// If the RING from this server is broken, connect to next server in RING for recovering.
// Returns the chmpxid of next connected server in RING when TRUE.
//
chmpxid_t ChmEventSock::GetNextRingChmpxId(void)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData		= pChmCntrl->GetImDataObj();
	chmpxid_t	chmpxid;

	if(CHM_INVALID_CHMPXID != (chmpxid = pImData->GetNextRingChmpxId())){
		//MSG_CHMPRN("Not need to connect rechain RING.");
		return chmpxid;
	}

	chmpxpos_t	selfpos = pImData->GetSelfServerPos();
	if(CHM_INVALID_CHMPXLISTPOS == selfpos){
		ERR_CHMPRN("Not found self chmpx data in list.");
		return CHM_INVALID_CHMPXID;
	}

	chmpxpos_t	pos = selfpos;	// Start at self pos
	for(pos = pImData->GetNextServerPos(selfpos, pos, true, true); CHM_INVALID_CHMPXLISTPOS != pos; pos = pImData->GetNextServerPos(selfpos, pos, true, true)){
		string	name;
		chmpxid	= CHM_INVALID_CHMPXID;
		if(!pImData->GetServerBase(pos, &name, &chmpxid, NULL, NULL, NULL, NULL) || CHM_INVALID_CHMPXID == chmpxid){
			WAN_CHMPRN("Could not get server name/chmpxid for pos(%" PRIu64 ") in server list, but continue...", pos);
			continue;
		}
		// status check
		chmpxsts_t	status = pImData->GetServerStatus(chmpxid);
		if(IS_CHMPXSTS_SERVER(status) && IS_CHMPXSTS_UP(status)){
			// try to connect server.
			int	sock = CHM_INVALID_SOCK;
			if(ConnectServer(chmpxid, sock, false) && CHM_INVALID_SOCK != sock){			// update chmshm
				MSG_CHMPRN("Connect new server %s:chmpxid(0x%016" PRIx64 ") to sock(%d), and recover RING.", name.c_str(), chmpxid, sock);
				return chmpxid;
			}else{
				ERR_CHMPRN("Could not connect new server %s:chmpxid(0x%016" PRIx64 "), so try to next server.", name.c_str(), chmpxid);
			}
		}
	}
	WAN_CHMPRN("There is no connect-able server in RING. probably there is no UP server in RING.");

	return CHM_INVALID_CHMPXID;
}

// Check next chmpxid which is over step departure chmpxid in RING.
// On this case, if send packet to next chmpxid, it includes loop packet in RING.
// So we need to check next and departure chmpxid before transfer packet.
//
bool ChmEventSock::IsSafeDeptAndNextChmpxId(chmpxid_t dept_chmpxid, chmpxid_t next_chmpxid)
{
	if(CHM_INVALID_CHMPXID == dept_chmpxid || CHM_INVALID_CHMPXID ==  next_chmpxid){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	if(dept_chmpxid == next_chmpxid){
		return true;
	}

	ChmIMData*	pImData	= pChmCntrl->GetImDataObj();
	chmpxpos_t	selfpos = pImData->GetSelfServerPos();
	if(CHM_INVALID_CHMPXLISTPOS == selfpos){
		ERR_CHMPRN("Not found self chmpx data in list.");
		return false;
	}

	chmpxpos_t	pos = selfpos;	// Start at self pos
	for(pos = pImData->GetNextServerPos(selfpos, pos, true, true); CHM_INVALID_CHMPXLISTPOS != pos; pos = pImData->GetNextServerPos(selfpos, pos, true, true)){
		string		name;
		chmpxid_t	chmpxid	= CHM_INVALID_CHMPXID;
		if(!pImData->GetServerBase(pos, &name, &chmpxid, NULL, NULL, NULL, NULL) || CHM_INVALID_CHMPXID == chmpxid){
			ERR_CHMPRN("Could not get server name/chmpxid for pos(%" PRIu64 ") in server list, but continue...", pos);
			continue;
		}
		if(next_chmpxid == chmpxid){
			MSG_CHMPRN("Found next %s:chmpxid(0x%016" PRIx64 ") before departure chmpxid(0x%016" PRIx64 "), so can send this pkt.", name.c_str(), next_chmpxid, dept_chmpxid);
			return true;
		}
		if(dept_chmpxid == chmpxid){
			MSG_CHMPRN("Found departure %s:chmpxid(0x%016" PRIx64 ") before next chmpxid(0x%016" PRIx64 "), so can not send this pkt.", name.c_str(), dept_chmpxid, next_chmpxid);
			return false;
		}
	}
	// why...?
	MSG_CHMPRN("There is no server UP on RING(= there is not the server of chmpxid(0x%016" PRIx64 ") in server list), so can not send this pkt.", next_chmpxid);

	return false;
}

bool ChmEventSock::Accept(int sock)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	while(!fullock::flck_trylock_noshared_mutex(&sockfd_lockval));	// LOCK

	// accept
	int						newsock;
	struct sockaddr_storage	from;
	socklen_t				fromlen = static_cast<socklen_t>(sizeof(struct sockaddr_storage));
	if(-1 == (newsock = accept(sock, reinterpret_cast<struct sockaddr*>(&from), &fromlen))){
		if(EAGAIN == errno || EWOULDBLOCK == errno){
			MSG_CHMPRN("There is no waiting accept request for sock(%d)", sock);
			fullock::flck_unlock_noshared_mutex(&sockfd_lockval);	// UNLOCK
			return true;	// return succeed
		}
		ERR_CHMPRN("Failed to accept from sock(%d), error=%d", sock, errno);
		fullock::flck_unlock_noshared_mutex(&sockfd_lockval);		// UNLOCK
		return false;
	}
	MSG_CHMPRN("Accept new socket(%d)", newsock);
	fullock::flck_unlock_noshared_mutex(&sockfd_lockval);			// UNLOCK

	// Convert IPv4-mapped IPv6 addresses to plain IPv4.
	if(!ChmNetDb::CvtV4MappedAddrInfo(&from, fromlen)){
		ERR_CHMPRN("Something error occured during converting IPv4-mapped IPv6 addresses to plain IPv4, but continue...");
	}

	// get hostname for accessing control
	string	stripaddress;
	string	strhostname;
	if(!ChmNetDb::CvtAddrInfoToIpAddress(&from, fromlen, stripaddress)){
		ERR_CHMPRN("Failed to convert addrinfo(new sock=%d) to ipaddress.", newsock);
		CHM_CLOSESOCK(newsock);
		return false;
	}
	if(!ChmNetDb::Get()->GetHostname(stripaddress.c_str(), strhostname, true)){
		MSG_CHMPRN("Could not get hostname(FQDN) from %s, then ip address is instead of hostname.", stripaddress.c_str());
		strhostname = ChmNetDb::GetNoZoneIndexIpAddress(stripaddress);
		strhostname = ChmNetDb::CvtIPv4MappedIPv6Address(strhostname);
	}

	// add tempolary accepting socket mapping.(before adding epoll for multi threading)
	acceptingmap.set(newsock, strhostname);

	// check ACL
	//
	// [NOTE]
	// Check only hostname(IP address), then check strictly after accepting.
	//
	if(!pImData->IsAllowHost(strhostname.c_str())){
		// Not allow accessing from slave.
		ERR_CHMPRN("Denied %s(sock:%d) by not allowed.", strhostname.c_str(), newsock);
		if(!NotifyHup(newsock)){
			ERR_CHMPRN("Failed to closing \"accepting socket\" for %s, but continue...", strhostname.c_str());
		}
		return false;
	}

	// Set nonblock
	if(!ChmEventSock::SetNonblocking(newsock)){
		WAN_CHMPRN("Failed to nonblock socket(sock:%d), but continue...", newsock);
	}

	//
	// Push accepting socket which is not allowed completely yet.
	//
	MSG_CHMPRN("%s(sock:%d) is accepting tempolary, but not allowed completely yet..", strhostname.c_str(), newsock);

	// SSL
	CHMPXSSL	ssldata;
	if(!pImData->GetSelfSsl(ssldata)){
		ERR_CHMPRN("Failed to get SSL structure from self chmpx.");
		if(!NotifyHup(newsock)){
			ERR_CHMPRN("Failed to closing \"accepting socket\" for %s, but continue...", strhostname.c_str());
		}
		return false;
	}
	if(ssldata.is_ssl){
		// get SSL context
		ChmSSCtx	ctx = pSecureSock->GetSSLContext(ssldata.server_cert, ssldata.server_prikey, ssldata.slave_cert, ssldata.slave_prikey);
		if(!ctx){
			ERR_CHMPRN("Failed to get SSL context.");
			if(!NotifyHup(newsock)){
				ERR_CHMPRN("Failed to closing \"accepting socket\" for %s, but continue...", strhostname.c_str());
			}
			return false;
		}

		// Accept SSL
		ChmSSSession	ssl = ChmSecureSock::AcceptSSL(ctx, newsock, con_retry_count, con_wait_time);
		pSecureSock->FreeSSLContext(ctx);
		if(!ssl){
			ERR_CHMPRN("Failed to accept SSL.");
			if(!NotifyHup(newsock)){
				ERR_CHMPRN("Failed to closing \"accepting socket\" for %s, but continue...", strhostname.c_str());
			}
			return false;
		}

		// Set SSL mapping
		sslmap.set(newsock, ssl, true);		// over write
	}

	// add event fd
	struct epoll_event	eqevent;
	memset(&eqevent, 0, sizeof(struct epoll_event));
	eqevent.data.fd	= newsock;
	eqevent.events	= EPOLLIN | EPOLLET | EPOLLRDHUP;				// EPOLLRDHUP is set.
	if(-1 == epoll_ctl(eqfd, EPOLL_CTL_ADD, newsock, &eqevent)){
		ERR_CHMPRN("Failed to add sock(sock %d) into epoll event(%d), errno=%d", newsock, eqfd, errno);
		if(!NotifyHup(newsock)){
			ERR_CHMPRN("Failed to closing \"accepting socket\" for %s, but continue...", strhostname.c_str());
		}
		return false;
	}
	//
	// N2_CONINIT_REQ(or CONINIT_REQ) will get in main event loop.
	//
	return true;
}

// Accept socket for control port.
// The socket does not have chmpxid and not need to set chmshm.
// The sock value has only internal this class.
//
bool ChmEventSock::AcceptCtlport(int ctlsock)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// accept
	int						newctlsock;
	struct sockaddr_storage	from;
	socklen_t				fromlen = static_cast<socklen_t>(sizeof(struct sockaddr_storage));
	if(-1 == (newctlsock = accept(ctlsock, reinterpret_cast<struct sockaddr*>(&from), &fromlen))){
		if(EAGAIN == errno || EWOULDBLOCK == errno){
			MSG_CHMPRN("There is no waiting accept request for ctlsock(%d)", ctlsock);
			return false;
		}
		ERR_CHMPRN("Failed to accept from ctlsock(%d), error=%d", ctlsock, errno);
		return false;
	}
	MSG_CHMPRN("Accept new control socket(%d)", newctlsock);

	// add internal mapping
	ctlsockmap.set(newctlsock, CHM_INVALID_CHMPXID);	// control sock does not have chmpxid

	// Convert IPv4-mapped IPv6 addresses to plain IPv4.
	if(!ChmNetDb::CvtV4MappedAddrInfo(&from, fromlen)){
		ERR_CHMPRN("Something error occured during converting IPv4-mapped IPv6 addresses to plain IPv4, but continue...");
	}

	// get hostname for accessing control
	string	stripaddress;
	string	strhostname;
	if(!ChmNetDb::CvtAddrInfoToIpAddress(&from, fromlen, stripaddress)){
		ERR_CHMPRN("Failed to convert addrinfo(new ctlsock=%d) to ipaddress.", newctlsock);
		if(!NotifyHup(newctlsock)){
			ERR_CHMPRN("Failed to closing \"from control socket\" for chmpxid(0x%016" PRIx64 "), but continue...", CHM_INVALID_CHMPXID);
		}
		return false;
	}
	if(!ChmNetDb::Get()->GetHostname(stripaddress.c_str(), strhostname, true)){
		MSG_CHMPRN("Could not get hostname(FQDN) from %s, then ip address is instead of hostname.", stripaddress.c_str());
		strhostname = ChmNetDb::GetNoZoneIndexIpAddress(stripaddress);
		strhostname = ChmNetDb::CvtIPv4MappedIPv6Address(strhostname);
	}

	// check ACL
	//
	// [NOTE]
	// Check only hostname(IP address), then check strictly after accepting.
	//
	if(!pImData->IsAllowHost(strhostname.c_str())){
		// Not allow accessing from slave.
		ERR_CHMPRN("Denied %s(ctlsock:%d) by not allowed.", strhostname.c_str(), newctlsock);
		if(!NotifyHup(newctlsock)){
			ERR_CHMPRN("Failed to closing \"from control socket\" for chmpxid(0x%016" PRIx64 "), but continue...", CHM_INVALID_CHMPXID);
		}
		return false;
	}

	// Set nonblock
	if(!ChmEventSock::SetNonblocking(newctlsock)){
		WAN_CHMPRN("Failed to nonblock socket(sock:%d), but continue...", newctlsock);
	}

	// add event fd
	struct epoll_event	eqevent;
	memset(&eqevent, 0, sizeof(struct epoll_event));
	eqevent.data.fd	= newctlsock;
	eqevent.events	= EPOLLIN | EPOLLET | EPOLLRDHUP;	// EPOLLRDHUP is set.
	if(-1 == epoll_ctl(eqfd, EPOLL_CTL_ADD, newctlsock, &eqevent)){
		ERR_CHMPRN("Failed to add socket(ctlsock %d) into epoll event(%d), errno=%d", newctlsock, eqfd, errno);
		if(!NotifyHup(newctlsock)){
			ERR_CHMPRN("Failed to closing \"from control socket\" for chmpxid(0x%016" PRIx64 "), but continue...", CHM_INVALID_CHMPXID);
		}
		return false;
	}

	return true;
}

//
// Check the version of the communication protocol that can be used for backward compatibility.
//
// [NOTE]
// Due to changes in the CHMINFO structure, some communication protocols have been deprecated since v1.0.71.
// However, for backward compatibility, v1.0.71 or later can communicate with previous versions of CHMPX.
// Use CHMPX_COM_VERSION_REQ to check the compatibility of this CHMPX and communication protocol.
//
bool ChmEventSock::InitialComVersion(int sock, chmpxid_t chmpxid, comver_t& com_proto_version)
{
	if(CHM_INVALID_SOCK == sock || CHM_INVALID_CHMPXID == chmpxid){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}

	// [NOTE]
	// Prioritize environment variables over anything.
	if(get_comproto_version_env(com_proto_version)){
		MSG_CHMPRN("Found %s environment and force to use communication protocol (%" PRIu64 ").", CHMPX_COMPROTO_VERSION, com_proto_version);
		return true;
	}

	// Send version request
	if(!PxComSendVersionReq(sock, chmpxid, false)){	// do not close socket if error.
		ERR_CHMPRN("Failed to send PXCOM_VERSION_REQ.");
		return false;
	}

	// Receive response
	bool	is_closed	= false;
	PCOMPKT	pComPkt		= NULL;
	if(!ChmEventSock::RawReceive(sock, GetSSL(sock), is_closed, &pComPkt, 0L, false, sock_retry_count, sock_wait_time) || !pComPkt){
		ERR_CHMPRN("Failed to receive ComPkt from sock(%d), sock is %s.", sock, is_closed ? "closed" : "not closed");
		CHM_Free(pComPkt);
		return false;
	}

	// Check response type
	if(COM_PX2PX != pComPkt->head.type){
		ERR_CHMPRN("Received data is %s(%" PRIu64 ") type, which does not COM_PX2PX type from sock(%d).", STRCOMTYPE(pComPkt->head.type), pComPkt->head.type, sock);
		CHM_Free(pComPkt);
		return false;
	}
	PPXCOM_ALL	pChmpxCom = CHM_OFFSET(pComPkt, pComPkt->offset, PPXCOM_ALL);
	if(CHMPX_COM_VERSION_RES != pChmpxCom->val_head.type){
		ERR_CHMPRN("Received data is %s(%" PRIu64 ") type, which does not CHMPX_COM_VERSION_RES type from sock(%d).", STRPXCOMTYPE(pChmpxCom->val_head.type), pChmpxCom->val_head.type, sock);
		CHM_Free(pComPkt);
		return false;
	}

	// Check response
	PCOMPKT	pResComPkt	= NULL;						// tmp
	com_proto_version	= COM_VERSION_UNINIT;
	if(!PxComReceiveVersionRes(&(pComPkt->head), pChmpxCom, &pResComPkt, com_proto_version)){
		ERR_CHMPRN("Received CHMPXCOM type(%" PRIu64 ":%s). Something error occured.", pChmpxCom->val_head.type, STRPXCOMTYPE(pChmpxCom->val_head.type));
		CHM_Free(pComPkt);
		CHM_Free(pResComPkt);
		return false;
	}
	CHM_Free(pComPkt);
	CHM_Free(pResComPkt);

	return true;
}

// [NOTICE]
// This function is updating all server status from a server on RING.
// This method opens the port of one of servers, and sends CHMPX_COM_N2_STATUS_REQ,
// receives CHMPX_COM_N2_STATUS_RES, updates all server status by result.
// This method uses the socket for that port, the socket is connecting very short time.
// After updating all server status from the result, this method closes that socket ASSAP.
//
bool ChmEventSock::InitialAllServerStatus(void)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// Connect to ONE of servers.
	string		name;
	int			sock		= CHM_INVALID_SOCK;
	chmpxid_t	chmpxid		= CHM_INVALID_CHMPXID;
	chmpxid_t	selfchmpxid	= pImData->GetSelfChmpxId();
	for(chmpxpos_t pos = pImData->GetNextServerPos(CHM_INVALID_CHMPXLISTPOS, CHM_INVALID_CHMPXLISTPOS, true, false); CHM_INVALID_CHMPXLISTPOS != pos; pos = pImData->GetNextServerPos(CHM_INVALID_CHMPXLISTPOS, pos, true, false), sock = CHM_INVALID_SOCK){
		chmpxid	= CHM_INVALID_CHMPXID;
		if(!pImData->GetServerBase(pos, &name, &chmpxid, NULL, NULL, NULL, NULL) || CHM_INVALID_CHMPXID == chmpxid){
			WAN_CHMPRN("Could not get server name/chmpxid for pos(%" PRIu64 ").", pos);
			continue;
		}
		if(selfchmpxid == chmpxid){
			MSG_CHMPRN("chmpxid(0x%016" PRIx64 ") for pos(%" PRIu64 ") is self chmpxid.", chmpxid, pos);
			continue;
		}
		if(ConnectServer(chmpxid, sock, true)){		// connect & not set epoll & not chmshm
			// connected.
			break;
		}
		WAN_CHMPRN("Could not connect to %s:chmpxid(0x%016" PRIx64 ") which is pos(%" PRIu64 "), try to connect next server.", name.c_str(), chmpxid, pos);
	}
	if(CHM_INVALID_SOCK == sock){
		MSG_CHMPRN("There is no server to connect port, so it means any server ready up.");
		return true;		// Succeed to update
	}

	// Send request
	//
	// [NOTE]
	// This method is for initializing chmpx, so do not need to lock socket.
	//
	if(!PxComSendStatusReq(sock, chmpxid, false)){	// do not close socket if error.
		ERR_CHMPRN("Failed to send PXCOM_STATUS_REQ.");
		CloseSSL(sock);
		return false;
	}

	// Receive response
	// 
	// [NOTICE]
	// retry count is sock_retry_count
	// 
	bool	is_closed	= false;
	PCOMPKT	pComPkt		= NULL;
	if(!ChmEventSock::RawReceive(sock, GetSSL(sock), is_closed, &pComPkt, 0L, false, sock_retry_count, sock_wait_time) || !pComPkt){
		ERR_CHMPRN("Failed to receive ComPkt from sock(%d), sock is %s.", sock, is_closed ? "closed" : "not closed");
		CloseSSL(sock);
		CHM_Free(pComPkt);
		return false;
	}

	// Check response type
	if(COM_PX2PX != pComPkt->head.type){
		ERR_CHMPRN("Received data is %s(%" PRIu64 ") type, which does not COM_PX2PX type from sock(%d).", STRCOMTYPE(pComPkt->head.type), pComPkt->head.type, sock);
		CloseSSL(sock);
		CHM_Free(pComPkt);
		return false;
	}
	PPXCOM_ALL	pChmpxCom = CHM_OFFSET(pComPkt, pComPkt->offset, PPXCOM_ALL);
	if(CHMPX_COM_STATUS_RES != pChmpxCom->val_head.type && CHMPX_COM_N2_STATUS_RES != pChmpxCom->val_head.type){
		ERR_CHMPRN("Received data is %s(%" PRIu64 ") type, which does not CHMPX_COM_N2_STATUS_RES(or CHMPX_COM_STATUS_RES) type from sock(%d).", STRPXCOMTYPE(pChmpxCom->val_head.type), pChmpxCom->val_head.type, sock);
		CloseSSL(sock);
		CHM_Free(pComPkt);
		return false;
	}

	// Check response & Do merge
	PCOMPKT	pResComPkt = NULL;	// tmp
	if(!PxComReceiveStatusRes(&(pComPkt->head), pChmpxCom, &pResComPkt, true)){
		ERR_CHMPRN("Received CHMPXCOM type(%" PRIu64 ":%s). Something error occured.", pChmpxCom->val_head.type, STRPXCOMTYPE(pChmpxCom->val_head.type));
		CloseSSL(sock);
		CHM_Free(pComPkt);
		CHM_Free(pResComPkt);
		return false;
	}
	CHM_Free(pComPkt);
	CHM_Free(pResComPkt);

	//
	// [NOTICE] - added after 1.0.57
	//
	// Get suspend merging status here.
	// If all servers in the ring has suspending merging status, this node must is set
	// same status at initializing.
	// If the ring(target node) is under 1.0.57, they can not response unknown
	// communication command as MERGE_SUSPEND_GET.
	// Thus it will be timeouted here, we must check timeout.
	// If error occurred here, bu we will continue.
	//
	if(!PxComSendMergeSuspendGet(sock, chmpxid)){
		ERR_CHMPRN("Failed to send PXCOM_MERGE_SUSPEND_GET, but continue...");
	}else{
		//
		// Receive response(MERGE_SUSPEND_RES)
		// 
		// [NOTICE]
		// retry count is sock_retry_count
		// 
		is_closed	= false;	// tmp
		pComPkt		= NULL;
		if(!ChmEventSock::RawReceive(sock, GetSSL(sock), is_closed, &pComPkt, 0L, false, sock_retry_count, sock_wait_time) || !pComPkt){
			ERR_CHMPRN("Failed to receive ComPkt from sock(%d), sock is %s.", sock, is_closed ? "closed" : "not closed");
		}else{
			// Check response type
			if(COM_PX2PX != pComPkt->head.type){
				ERR_CHMPRN("Received data is %s(%" PRIu64 ") type, which does not COM_PX2PX type from sock(%d).", STRCOMTYPE(pComPkt->head.type), pComPkt->head.type, sock);
			}else{
				pChmpxCom = CHM_OFFSET(pComPkt, pComPkt->offset, PPXCOM_ALL);
				if(CHMPX_COM_MERGE_SUSPEND_RES != pChmpxCom->val_head.type){
					ERR_CHMPRN("Received data is %s(%" PRIu64 ") type, which does not CHMPX_COM_MERGE_SUSPEND_RES type from sock(%d).", STRPXCOMTYPE(pChmpxCom->val_head.type), pChmpxCom->val_head.type, sock);
				}else{
					// Check response & set suspend merging mode
					if(!PxComReceiveMergeSuspendRes(&(pComPkt->head), pChmpxCom)){
						ERR_CHMPRN("Received CHMPXCOM type(%" PRIu64 ":%s). Something error occured.", pChmpxCom->val_head.type, STRPXCOMTYPE(pChmpxCom->val_head.type));
					}
				}
			}
		}
		CHM_Free(pComPkt);
	}
	CloseSSL(sock);

	return true;
}

bool ChmEventSock::Processing(PCOMPKT pComPkt)
{
	if(!pComPkt){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();		// not used

	if(COM_PX2PX == pComPkt->head.type){
		// check length
		if(pComPkt->length <= sizeof(COMPKT) || 0 == pComPkt->offset){
			ERR_CHMPRN("ComPkt type(%" PRIu64 ":%s) has invalid values.", pComPkt->head.type, STRCOMTYPE(pComPkt->head.type));
			return false;
		}
		// following datas
		PPXCOM_ALL	pChmpxCom	= CHM_OFFSET(pComPkt, pComPkt->offset, PPXCOM_ALL);

		if(CHMPX_COM_STATUS_REQ == pChmpxCom->val_head.type || CHMPX_COM_N2_STATUS_REQ == pChmpxCom->val_head.type){
			PCOMPKT	pResComPkt = NULL;
			if(!PxComReceiveStatusReq(&(pComPkt->head), pChmpxCom, &pResComPkt) || !pResComPkt){
				ERR_CHMPRN("Received CHMPXCOM type(%" PRIu64 ":%s). Something error occured.", pChmpxCom->val_head.type, STRPXCOMTYPE(pChmpxCom->val_head.type));
				CHM_Free(pResComPkt);
				return false;
			}
			// send response
			if(!Send(pResComPkt, NULL, 0L)){
				ERR_CHMPRN("Sending response ComPkt type(%" PRIu64 ":%s) against ComPkt type(%" PRIu64 ":%s), Something error occured.", pResComPkt->head.type, STRCOMTYPE(pResComPkt->head.type), pComPkt->head.type, STRCOMTYPE(pComPkt->head.type));
				CHM_Free(pResComPkt);
				return false;
			}
			CHM_Free(pResComPkt);

		}else if(CHMPX_COM_STATUS_RES == pChmpxCom->val_head.type || CHMPX_COM_N2_STATUS_RES == pChmpxCom->val_head.type){
			// processing
			PCOMPKT	pResComPkt = NULL;
			if(!PxComReceiveStatusRes(&(pComPkt->head), pChmpxCom, &pResComPkt, false) || pResComPkt){			// should be pResComPkt=NULL
				ERR_CHMPRN("Received CHMPXCOM type(%" PRIu64 ":%s). Something error occured.", pChmpxCom->val_head.type, STRPXCOMTYPE(pChmpxCom->val_head.type));
				CHM_Free(pResComPkt);
				return false;
			}

			// If this pkt receiver is this server, check status and merging.
			//
			if(is_server_mode && pComPkt->head.term_ids.chmpxid == pImData->GetSelfChmpxId()){
				// Update self last status updating time 
				if(!pImData->UpdateSelfLastStatusTime()){
					ERR_CHMPRN("Could not update own updating status time, but continue...");
				}
				chmpxsts_t	status = pImData->GetSelfStatus();

				// If change self status after processing, so need to update status to all servers.
				chmpxid_t	to_chmpxid = GetNextRingChmpxId();
				CHMPXSVR	selfchmpxsvr;
				if(!pImData->GetSelfChmpxSvr(&selfchmpxsvr)){
					ERR_CHMPRN("Could not get self chmpx information,");
				}else{
					if(CHM_INVALID_CHMPXID == to_chmpxid){
						// no server found, finish doing so pending hash updated self.
						WAN_CHMPRN("Could not get to chmpxid, probably there is no server without self chmpx on RING. So only sending status update to slaves.");
						if(!PxComSendSlavesStatusChange(&selfchmpxsvr)){
							ERR_CHMPRN("Failed to send self status change to slaves, but continue...");
						}
					}else{
						if(!PxComSendStatusChange(to_chmpxid, &selfchmpxsvr)){
							ERR_CHMPRN("Failed to send self status change, but continue...");
						}
					}
				}

				// When this server is up and after joining ring and the status is changed pending, do merge.
				//
				// [NOTICE]
				// MergeChmpxSvrs() in PxComReceiveStatusRes() can change self status.
				// Check changed self status and do start merging.
				// 
				if(IsAutoMerge()){
					if(!IS_CHMPXSTS_NOTHING(status)){
						// start merge automatically
						if(!RequestMergeStart()){
							ERR_CHMPRN("Failed to merge or complete merge for \"server up and automatically merging\".");
							return false;
						}
					}
				}
			}
			CHM_Free(pResComPkt);

		}else if(CHMPX_COM_CONINIT_REQ == pChmpxCom->val_head.type || CHMPX_COM_N2_CONINIT_REQ == pChmpxCom->val_head.type){
			ERR_CHMPRN("Received CHMPXCOM type(%" PRIu64 ":%s). Something error occured, so exit with no processing.", pChmpxCom->val_head.type, STRPXCOMTYPE(pChmpxCom->val_head.type));
			return false;

		}else if(CHMPX_COM_CONINIT_RES == pChmpxCom->val_head.type || CHMPX_COM_N2_CONINIT_RES == pChmpxCom->val_head.type){
			ERR_CHMPRN("Received CHMPXCOM type(%" PRIu64 ":%s). Something error occured, so exit with no processing.", pChmpxCom->val_head.type, STRPXCOMTYPE(pChmpxCom->val_head.type));
			return false;

		}else if(CHMPX_COM_JOIN_RING == pChmpxCom->val_head.type || CHMPX_COM_N2_JOIN_RING == pChmpxCom->val_head.type){
			PCOMPKT	pResComPkt = NULL;
			if(!PxComReceiveJoinRing(&(pComPkt->head), pChmpxCom, &pResComPkt)){
				ERR_CHMPRN("Received CHMPXCOM type(%" PRIu64 ":%s). Something error occured.", pChmpxCom->val_head.type, STRPXCOMTYPE(pChmpxCom->val_head.type));

				// [NOTICE]
				// Close all socket, it occurs epoll event on the other servers.
				// Because other servers send server down notify, this server do 
				// nothing.
				//
				if(!CloseSelfSocks()){
					ERR_CHMPRN("Failed to close self sock and ctlsock, but continue...");
				}
				if(!CloseFromSlavesSocks()){
					ERR_CHMPRN("Failed to close connection from clients, but continue...");
				}
				if(!CloseToServersSocks()){
					ERR_CHMPRN("Failed to close connection to other servers, but continue...");
				}
				if(!CloseCtlportClientSocks()){
					ERR_CHMPRN("Failed to close control port connection from clients, but continue...");
				}
				return false;
			}
			if(pResComPkt){
				// send(transfer) compkt
				if(!Send(pResComPkt, NULL, 0L)){
					ERR_CHMPRN("Sending ComPkt type(%" PRIu64 ":%s) against ComPkt type(%" PRIu64 ":%s), Something error occured.", pResComPkt->head.type, STRCOMTYPE(pResComPkt->head.type), pComPkt->head.type, STRCOMTYPE(pComPkt->head.type));
					CHM_Free(pResComPkt);
					return false;
				}
				CHM_Free(pResComPkt);
			}else{
				// Success to join on RING, do next step(update pending hash)
				MSG_CHMPRN("Succeed to join RING, next step which is updating status and server list runs automatically.");
			}

		}else if(CHMPX_COM_STATUS_UPDATE == pChmpxCom->val_head.type || CHMPX_COM_N2_STATUS_UPDATE == pChmpxCom->val_head.type){
			PCOMPKT	pResComPkt = NULL;
			if(!PxComReceiveStatusUpdate(&(pComPkt->head), pChmpxCom, &pResComPkt)){
				// Failed Status Update
				// (recovered status update in receive function if possible)
				ERR_CHMPRN("Received CHMPXCOM type(%" PRIu64 ":%s). Something error occured.", pChmpxCom->val_head.type, STRPXCOMTYPE(pChmpxCom->val_head.type));
				return false;
			}
			if(pResComPkt){
				// Success Status Update, send(transfer) compkt to next server
				if(!Send(pResComPkt, NULL, 0L)){
					ERR_CHMPRN("Sending ComPkt type(%" PRIu64 ":%s) against ComPkt type(%" PRIu64 ":%s), Something error occured.", pResComPkt->head.type, STRCOMTYPE(pResComPkt->head.type), pComPkt->head.type, STRCOMTYPE(pComPkt->head.type));
					CHM_Free(pResComPkt);
					return false;
				}
				CHM_Free(pResComPkt);
			}else{
				// Success Status Update, wait to receive merging...(Nothing to do here)
			}

		}else if(CHMPX_COM_STATUS_CONFIRM == pChmpxCom->val_head.type || CHMPX_COM_N2_STATUS_CONFIRM == pChmpxCom->val_head.type){
			PCOMPKT	pResComPkt = NULL;
			if(!PxComReceiveStatusConfirm(&(pComPkt->head), pChmpxCom, &pResComPkt)){
				// Failed Status Confirm
				ERR_CHMPRN("Received CHMPXCOM type(%" PRIu64 ":%s). Something error occured.", pChmpxCom->val_head.type, STRPXCOMTYPE(pChmpxCom->val_head.type));
				return false;
			}
			// Success Status Confirm
			if(pResComPkt){
				// and send(transfer) compkt to next server
				if(!Send(pResComPkt, NULL, 0L)){
					ERR_CHMPRN("Sending ComPkt type(%" PRIu64 ":%s) against ComPkt type(%" PRIu64 ":%s), Something error occured.", pResComPkt->head.type, STRCOMTYPE(pResComPkt->head.type), pComPkt->head.type, STRCOMTYPE(pComPkt->head.type));
					CHM_Free(pResComPkt);
					return false;
				}
				CHM_Free(pResComPkt);
			}

		}else if(CHMPX_COM_STATUS_CHANGE == pChmpxCom->val_head.type || CHMPX_COM_N2_STATUS_CHANGE == pChmpxCom->val_head.type){
			PCOMPKT	pResComPkt = NULL;
			if(!PxComReceiveStatusChange(&(pComPkt->head), pChmpxCom, &pResComPkt)){
				// Failed
				ERR_CHMPRN("Received CHMPXCOM type(%" PRIu64 ":%s). Something error occured.", pChmpxCom->val_head.type, STRPXCOMTYPE(pChmpxCom->val_head.type));
				return false;
			}
			if(pResComPkt){
				// Success, send(transfer) compkt to next server
				if(!Send(pResComPkt, NULL, 0L)){
					ERR_CHMPRN("Sending ComPkt type(%" PRIu64 ":%s) against ComPkt type(%" PRIu64 ":%s), Something error occured.", pResComPkt->head.type, STRCOMTYPE(pResComPkt->head.type), pComPkt->head.type, STRCOMTYPE(pComPkt->head.type));
					CHM_Free(pResComPkt);
					return false;
				}
				CHM_Free(pResComPkt);
			}else{
				// Success
				MSG_CHMPRN("Succeed CHMPXCOM type(%" PRIu64 ":%s).", pChmpxCom->val_head.type, STRPXCOMTYPE(pChmpxCom->val_head.type));
			}

			//
			// If any server chmpx is up, need to check up and connect it on slave mode.
			//
			if(!is_server_mode){
				if(!ConnectServers()){
					ERR_CHMPRN("Something error occurred during connecting server after status changed on slave mode chmpx, but continue...");
				}
			}

		}else if(CHMPX_COM_MERGE_START == pChmpxCom->val_head.type){
			PCOMPKT	pResComPkt = NULL;
			if(!PxComReceiveMergeStart(&(pComPkt->head), pChmpxCom, &pResComPkt)){
				// Failed
				ERR_CHMPRN("Received CHMPXCOM type(%" PRIu64 ":%s). Something error occured.", pChmpxCom->val_head.type, STRPXCOMTYPE(pChmpxCom->val_head.type));
				return false;
			}
			if(pResComPkt){
				// Success, at first send(transfer) compkt to next server
				if(!Send(pResComPkt, NULL, 0L)){
					ERR_CHMPRN("Sending ComPkt type(%" PRIu64 ":%s) against ComPkt type(%" PRIu64 ":%s), Something error occured.", pResComPkt->head.type, STRCOMTYPE(pResComPkt->head.type), pComPkt->head.type, STRCOMTYPE(pComPkt->head.type));
					CHM_Free(pResComPkt);
					return false;
				}
				CHM_Free(pResComPkt);
			}
			// start merging
			//
			// [NOTICE]
			// must do it here after received and transferred "merge start".
			// because pending hash value must not change before receiving "merge start".
			// (MergeStart method sends "status change" in it)
			//
			if(!MergeStart()){
				WAN_CHMPRN("Failed to start merge.");
				return false;
			}

		}else if(CHMPX_COM_MERGE_ABORT == pChmpxCom->val_head.type){
			PCOMPKT	pResComPkt = NULL;
			if(!PxComReceiveMergeAbort(&(pComPkt->head), pChmpxCom, &pResComPkt)){
				// Failed Status Confirm
				ERR_CHMPRN("Received CHMPXCOM type(%" PRIu64 ":%s). Something error occured.", pChmpxCom->val_head.type, STRPXCOMTYPE(pChmpxCom->val_head.type));
				return false;
			}
			if(pResComPkt){
				// Success merge complete, at first send(transfer) compkt to next server
				if(!Send(pResComPkt, NULL, 0L)){
					ERR_CHMPRN("Sending ComPkt type(%" PRIu64 ":%s) against ComPkt type(%" PRIu64 ":%s), Something error occured.", pResComPkt->head.type, STRCOMTYPE(pResComPkt->head.type), pComPkt->head.type, STRCOMTYPE(pComPkt->head.type));
					CHM_Free(pResComPkt);
					return false;
				}
				CHM_Free(pResComPkt);
			}

			// abort merging
			//
			// [NOTICE]
			// must do it here after received and transferred "merge abort".
			// because pending hash value must not change before receiving "merge abort".
			// (MergeComplete method sends "status change" in it)
			//
			if(!MergeAbort()){
				WAN_CHMPRN("Failed to abort merge.");
				return false;
			}

			// [NOTICE]
			// If there are CHMPXSTS_SRVIN_DOWN_DELETED status server, that server can not send
			// "change status" by itself, so that if this server is start server of sending "merge abort",
			// this server send "status change" instead of down server.
			//
			if(pComPkt->head.dept_ids.chmpxid == pImData->GetSelfChmpxId()){
				chmpxid_t	nextchmpxid = GetNextRingChmpxId();
				chmpxid_t	downchmpxid;

				// [NOTICE]
				// Any DOWN status server does not have SUSPEND status.
				//
				while(CHM_INVALID_CHMPXID != (downchmpxid = pImData->GetChmpxIdByStatus(CHMPXSTS_SRVIN_DOWN_DELETED))){
					// status update
					if(!pImData->SetServerStatus(downchmpxid, CHMPXSTS_SRVIN_DOWN_DELPENDING)){
						ERR_CHMPRN("Failed to update status(CHMPXSTS_SRVIN_DOWN_DELPENDING) for down server(0x%016" PRIx64 "), but continue...", downchmpxid);
						continue;
					}
					// send status change
					CHMPXSVR	chmpxsvr;
					if(!pImData->GetChmpxSvr(downchmpxid, &chmpxsvr)){
						ERR_CHMPRN("Could not get chmpx information for down server(0x%016" PRIx64 "), but continue...", downchmpxid);
						continue;
					}
					if(CHM_INVALID_CHMPXID == nextchmpxid){
						if(!PxComSendSlavesStatusChange(&chmpxsvr)){
							ERR_CHMPRN("Failed to send self status change to slaves, but continue...");
						}
					}else{
						if(!PxComSendStatusChange(nextchmpxid, &chmpxsvr)){
							ERR_CHMPRN("Failed to send self status change, but continue...");
						}
					}
				}
			}

			// Check auto merging when it is needed.
			if(!ContinuousAutoMerge()){
				ERR_CHMPRN("Failed to start merging.");
				return true;
			}

		}else if(CHMPX_COM_MERGE_COMPLETE == pChmpxCom->val_head.type){
			PCOMPKT	pResComPkt	= NULL;
			bool	is_completed= false;
			if(!PxComReceiveMergeComplete(&(pComPkt->head), pChmpxCom, &pResComPkt, is_completed)){
				// Failed
				ERR_CHMPRN("Received CHMPXCOM type(%" PRIu64 ":%s). Something error occured.", pChmpxCom->val_head.type, STRPXCOMTYPE(pChmpxCom->val_head.type));
				return false;
			}
			if(pResComPkt){
				// Success, at first send(transfer) compkt to next server
				if(!Send(pResComPkt, NULL, 0L)){
					ERR_CHMPRN("Sending ComPkt type(%" PRIu64 ":%s) against ComPkt type(%" PRIu64 ":%s), Something error occured.", pResComPkt->head.type, STRCOMTYPE(pResComPkt->head.type), pComPkt->head.type, STRCOMTYPE(pComPkt->head.type));
					CHM_Free(pResComPkt);
					return false;
				}
				CHM_Free(pResComPkt);
			}
			if(!is_completed){
				// Not finish merging by all servers in RING, then retry to complete.
				chmpxid_t	nextchmpxid = GetNextRingChmpxId();
				if(CHM_INVALID_CHMPXID != nextchmpxid){
					MSG_CHMPRN("retry merge complete processing for recover.");

					// Update self last status updating time for updating self status
					if(!pImData->UpdateSelfLastStatusTime()){
						ERR_CHMPRN("Could not update own updating status time, but continue...");
					}
					CHMPXSVR	selfchmpxsvr;
					if(!pImData->GetSelfChmpxSvr(&selfchmpxsvr)){
						ERR_CHMPRN("Could not get self chmpx information for retry merge complete, then could not recover...");
					}else{
						if(!PxComSendStatusChange(nextchmpxid, &selfchmpxsvr)){
							ERR_CHMPRN("Failed to send self status change for retry merge complete, then could not recover...");
						}else{
							// send complete_merge
							if(!PxComSendMergeComplete(nextchmpxid)){
								ERR_CHMPRN("Failed to send CHMPX_COM_MERGE_COMPLETE for retry merge complete, then could not recover...");
							}
						}
					}
				}
				return true;
			}

			// complete merging
			//
			// [NOTICE]
			// must do it here after received and transferred "merge complete".
			// because pending hash value must not change before receiving "merge complete".
			// (MergeComplete method sends "status change" in it)
			//
			if(!MergeComplete()){
				WAN_CHMPRN("Failed to complete merge on this server because of maybe merging now, so wait complete merge after finishing merge");

			}else{
				// [NOTICE]
				// If there are CHMPXSTS_SRVIN_DOWN_DELETED/DELPENDING status server, that server can not send
				// "change status" by itself, so that if this server is start server of sending "merge complete",
				// this server send "status change" instead of down server.
				//
				if(pComPkt->head.dept_ids.chmpxid == pImData->GetSelfChmpxId()){
					chmpxid_t	nextchmpxid = pImData->GetNextRingChmpxId();
					chmpxid_t	downchmpxid;

					// [NOTICE]
					// Any DOWN status server does not have SUSPEND status.
					//
					while(CHM_INVALID_CHMPXID != (downchmpxid = pImData->GetChmpxIdByStatus(CHMPXSTS_SRVIN_DOWN_DELETED)) || CHM_INVALID_CHMPXID != (downchmpxid = pImData->GetChmpxIdByStatus(CHMPXSTS_SRVIN_DOWN_DELPENDING))){
						// hash & status update
						if(!pImData->SetServerBaseHash(downchmpxid, CHM_INVALID_HASHVAL)){
							ERR_CHMPRN("Failed to update base hash(CHM_INVALID_HASHVAL) for down server(0x%016" PRIx64 "), but continue...", downchmpxid);
							continue;
						}
						if(!pImData->SetServerStatus(downchmpxid, CHMPXSTS_SRVOUT_DOWN_NORMAL)){
							ERR_CHMPRN("Failed to update status(CHMPXSTS_SRVOUT_DOWN_NORMAL) for down server(0x%016" PRIx64 "), but continue...", downchmpxid);
							continue;
						}
						// send status change
						CHMPXSVR	chmpxsvr;
						if(!pImData->GetChmpxSvr(downchmpxid, &chmpxsvr)){
							ERR_CHMPRN("Could not get chmpx information for down server(0x%016" PRIx64 "), but continue...", downchmpxid);
							continue;
						}
						if(CHM_INVALID_CHMPXID == nextchmpxid){
							if(!PxComSendSlavesStatusChange(&chmpxsvr)){
								ERR_CHMPRN("Failed to send self status change to slaves, but continue...");
							}
						}else{
							if(!PxComSendStatusChange(nextchmpxid, &chmpxsvr)){
								ERR_CHMPRN("Failed to send self status change, but continue...");
							}
						}
					}
				}
			}
			// Check auto merging when it is needed.
			if(!ContinuousAutoMerge()){
				ERR_CHMPRN("Failed to start merging.");
				return true;
			}

		}else if(CHMPX_COM_MERGE_SUSPEND == pChmpxCom->val_head.type){
			PCOMPKT	pResComPkt	= NULL;
			if(!PxComReceiveMergeSuspend(&(pComPkt->head), pChmpxCom, &pResComPkt)){
				// Failed
				ERR_CHMPRN("Received CHMPXCOM type(%" PRIu64 ":%s). Something error occured.", pChmpxCom->val_head.type, STRPXCOMTYPE(pChmpxCom->val_head.type));
				return false;
			}
			if(pResComPkt){
				// Success, at first send(transfer) compkt to next server
				if(!Send(pResComPkt, NULL, 0L)){
					ERR_CHMPRN("Sending ComPkt type(%" PRIu64 ":%s) against ComPkt type(%" PRIu64 ":%s), Something error occured.", pResComPkt->head.type, STRCOMTYPE(pResComPkt->head.type), pComPkt->head.type, STRCOMTYPE(pComPkt->head.type));
					CHM_Free(pResComPkt);
					return false;
				}
				CHM_Free(pResComPkt);
			}else{
				// Success
				MSG_CHMPRN("Succeed CHMPXCOM type(%" PRIu64 ":%s).", pChmpxCom->val_head.type, STRPXCOMTYPE(pChmpxCom->val_head.type));
			}

		}else if(CHMPX_COM_MERGE_NOSUSPEND == pChmpxCom->val_head.type){
			PCOMPKT	pResComPkt	= NULL;
			if(!PxComReceiveMergeNoSuspend(&(pComPkt->head), pChmpxCom, &pResComPkt)){
				// Failed
				ERR_CHMPRN("Received CHMPXCOM type(%" PRIu64 ":%s). Something error occured.", pChmpxCom->val_head.type, STRPXCOMTYPE(pChmpxCom->val_head.type));
				return false;
			}
			if(pResComPkt){
				// Success, at first send(transfer) compkt to next server
				if(!Send(pResComPkt, NULL, 0L)){
					ERR_CHMPRN("Sending ComPkt type(%" PRIu64 ":%s) against ComPkt type(%" PRIu64 ":%s), Something error occured.", pResComPkt->head.type, STRCOMTYPE(pResComPkt->head.type), pComPkt->head.type, STRCOMTYPE(pComPkt->head.type));
					CHM_Free(pResComPkt);
					return false;
				}
				CHM_Free(pResComPkt);
			}else{
				// Success
				MSG_CHMPRN("Succeed CHMPXCOM type(%" PRIu64 ":%s).", pChmpxCom->val_head.type, STRPXCOMTYPE(pChmpxCom->val_head.type));
			}

		}else if(CHMPX_COM_MERGE_SUSPEND_GET == pChmpxCom->val_head.type){
			PCOMPKT	pResComPkt	= NULL;
			if(!PxComReceiveMergeSuspendGet(&(pComPkt->head), pChmpxCom, &pResComPkt)){
				// Failed
				ERR_CHMPRN("Received CHMPXCOM type(%" PRIu64 ":%s). Something error occured.", pChmpxCom->val_head.type, STRPXCOMTYPE(pChmpxCom->val_head.type));
				return false;
			}
			if(pResComPkt){
				// Success, responding compkt(suspend res) to sending server
				if(!Send(pResComPkt, NULL, 0L)){
					ERR_CHMPRN("Sending ComPkt type(%" PRIu64 ":%s) against ComPkt type(%" PRIu64 ":%s), Something error occured.", pResComPkt->head.type, STRCOMTYPE(pResComPkt->head.type), pComPkt->head.type, STRCOMTYPE(pComPkt->head.type));
					CHM_Free(pResComPkt);
					return false;
				}
				CHM_Free(pResComPkt);
			}else{
				// Success
				MSG_CHMPRN("Succeed CHMPXCOM type(%" PRIu64 ":%s).", pChmpxCom->val_head.type, STRPXCOMTYPE(pChmpxCom->val_head.type));
			}

		}else if(CHMPX_COM_MERGE_SUSPEND_RES == pChmpxCom->val_head.type){
			// Not allow this packat receiving here.
			WAN_CHMPRN("Received CHMPXCOM type(%" PRIu64 ":%s). But this packet can not be received in initializing method, because this is only used for initializing.", pChmpxCom->val_head.type, STRPXCOMTYPE(pChmpxCom->val_head.type));
			return false;

		}else if(CHMPX_COM_SERVER_DOWN == pChmpxCom->val_head.type){
			PCOMPKT	pResComPkt = NULL;
			if(!PxComReceiveServerDown(&(pComPkt->head), pChmpxCom, &pResComPkt)){
				// Failed Server Down(could not recover...)
				ERR_CHMPRN("Received CHMPXCOM type(%" PRIu64 ":%s). Something error occured.", pChmpxCom->val_head.type, STRPXCOMTYPE(pChmpxCom->val_head.type));
				return false;
			}
			if(pResComPkt){
				// Success Server Down, send(transfer) compkt to next server
				if(!Send(pResComPkt, NULL, 0L)){
					ERR_CHMPRN("Sending ComPkt type(%" PRIu64 ":%s) against ComPkt type(%" PRIu64 ":%s), Something error occured.", pResComPkt->head.type, STRCOMTYPE(pResComPkt->head.type), pComPkt->head.type, STRCOMTYPE(pComPkt->head.type));
					CHM_Free(pResComPkt);
					return false;
				}
				CHM_Free(pResComPkt);
			}

			// [NOTICE]
			// The status is already updated, because probably received "merge abort" before receiving "SERVER_DOWN",
			// so status changed at that time.
			// Thus do not care about status here.
			//

		}else if(CHMPX_COM_REQ_UPDATEDATA == pChmpxCom->val_head.type){
			PCOMPKT	pResComPkt = NULL;
			if(!PxComReceiveReqUpdateData(&(pComPkt->head), pChmpxCom, &pResComPkt)){
				// Failed request update data
				ERR_CHMPRN("Received CHMPXCOM type(%" PRIu64 ":%s). Something error occured.", pChmpxCom->val_head.type, STRPXCOMTYPE(pChmpxCom->val_head.type));
				return false;
			}
			if(pResComPkt){
				// Success Server Down, send(transfer) compkt to next server
				if(!Send(pResComPkt, NULL, 0L)){
					ERR_CHMPRN("Sending ComPkt type(%" PRIu64 ":%s) against ComPkt type(%" PRIu64 ":%s), Something error occured.", pResComPkt->head.type, STRCOMTYPE(pResComPkt->head.type), pComPkt->head.type, STRCOMTYPE(pComPkt->head.type));
					CHM_Free(pResComPkt);
					return false;
				}
				CHM_Free(pResComPkt);
			}

		}else if(CHMPX_COM_RES_UPDATEDATA == pChmpxCom->val_head.type){
			// only trans
			if(!PxComReceiveResUpdateData(&(pComPkt->head), pChmpxCom)){
				// Failed set update data
				ERR_CHMPRN("Received CHMPXCOM type(%" PRIu64 ":%s). Something error occured.", pChmpxCom->val_head.type, STRPXCOMTYPE(pChmpxCom->val_head.type));
				return false;
			}

		}else if(CHMPX_COM_RESULT_UPDATEDATA == pChmpxCom->val_head.type){
			// only trans
			if(!PxComReceiveResultUpdateData(&(pComPkt->head), pChmpxCom)){
				// Failed receive result of update data
				ERR_CHMPRN("Received CHMPXCOM type(%" PRIu64 ":%s). Something error occured.", pChmpxCom->val_head.type, STRPXCOMTYPE(pChmpxCom->val_head.type));
				return false;
			}

		}else{
			ERR_CHMPRN("Could not handle ChmpxCom type(%" PRIu64 ":%s) in ComPkt type(%" PRIu64 ":%s).", pChmpxCom->val_head.type, STRPXCOMTYPE(pChmpxCom->val_head.type), pComPkt->head.type, STRCOMTYPE(pComPkt->head.type));
			return false;
		}

	}else if(COM_C2C == pComPkt->head.type){
		// This case is received MQ data to trans that to Socket.
		// (The other case is error.)
		//
		chmpxid_t	selfchmpxid = pImData->GetSelfChmpxId();
		if(pComPkt->head.dept_ids.chmpxid != selfchmpxid){
			ERR_CHMPRN("Why does not same chmpxid? COMPKT is received from socket, it should do processing MQ event object.");
			return false;
		}
		DUMPCOM_COMPKT("Sock::Processing(COM_C2C)", pComPkt);

		// [NOTICE]
		// Come here, following pattern:
		//
		// 1) client -> MQ -> chmpx(server) -> chmpx(slave)
		// 2) client -> MQ -> chmpx(slave)  -> chmpx(server)
		//
		// Case 1) Must be set end point of chmpxid and msgid
		// Case 2) Set or not set end point of chmpxid and msgid
		//         If not set these, decide these here.
		//
		// And both type end point must be connected.
		//

		// set deliver head in COMPKT
		//
		chmpxid_t		org_chmpxid = pComPkt->head.term_ids.chmpxid;	// backup
		chmpxidlist_t	ex_chmpxids;
		if(CHM_INVALID_CHMPXID == pComPkt->head.term_ids.chmpxid){
			long	ex_chmpxcnt = pImData->GetReceiverChmpxids(pComPkt->head.hash, pComPkt->head.c2ctype, ex_chmpxids);
			if(0 > ex_chmpxcnt){
				ERR_CHMPRN("Failed to get target chmpxids by something error occurred.");
				return false;
			}else if(0 == ex_chmpxcnt){
				WAN_CHMPRN("There are no target chmpxids, but method returns with success.");
				return true;
			}
		}else{
			ex_chmpxids.push_back(pComPkt->head.term_ids.chmpxid);
		}

		// Send
		while(!ex_chmpxids.empty()){
			chmpxid_t	tmpchmpxid;
			PCOMPKT		pTmpPkt;
			bool		is_duplicate;

			tmpchmpxid = ex_chmpxids.front();

			if(1 < ex_chmpxids.size()){
				if(NULL == (pTmpPkt = ChmEventSock::DuplicateComPkt(pComPkt))){
					ERR_CHMPRN("Could not duplicate COMPKT.");
					return false;
				}
				is_duplicate = true;
			}else{
				pTmpPkt		 = pComPkt;
				is_duplicate = false;
			}

			pTmpPkt->head.mq_serial_num		= MIN_MQ_SERIAL_NUMBER;
			pTmpPkt->head.peer_dept_msgid	= CHM_INVALID_MSGID;
			pTmpPkt->head.peer_term_msgid	= CHM_INVALID_MSGID;
			pTmpPkt->head.term_ids.chmpxid	= tmpchmpxid;

			if(CHM_INVALID_CHMPXID == org_chmpxid || org_chmpxid != pTmpPkt->head.term_ids.chmpxid){
				// this case is not replying, so reset terminated msgid.
				pTmpPkt->head.term_ids.msgid= CHM_INVALID_MSGID;
			}
			ex_chmpxids.pop_front();

			// check connect
			if(is_server_mode){
				if(!pImData->IsConnectSlave(pTmpPkt->head.term_ids.chmpxid)){
					// there is no socket as slave connection for term chmpx.
					// if term chmpx is slave node, abort this processing.
					// (if server node, try to connect to it in Send() method.)
					if(!pImData->IsServerChmpxId(pTmpPkt->head.term_ids.chmpxid)){
						ERR_CHMPRN("Could not find slave socket for slave chmpx(0x%016" PRIx64 ").", pTmpPkt->head.term_ids.chmpxid);
						if(is_duplicate){
							CHM_Free(pTmpPkt);
						}
						return false;
					}
					MSG_CHMPRN("Could not find slave socket for server chmpx(0x%016" PRIx64 "), thus try to connect chmpx(server mode).", pTmpPkt->head.term_ids.chmpxid);

					// [NOTE]
					// if server node and departure and terminate chmpx id are same,
					// need to send this packet to MQ directly.
					// and if do not send packet on this case, drop it here.
					//
					if(tmpchmpxid == selfchmpxid){
						if(IS_C2CTYPE_SELF(pComPkt->head.c2ctype)){
							bool	selfresult = false;
							if(pChmCntrl){
								selfresult = pChmCntrl->Processing(pTmpPkt, ChmCntrl::EVOBJ_TYPE_EVSOCK);
							}
							if(is_duplicate){
								CHM_Free(pTmpPkt);
							}
							if(!selfresult){
								ERR_CHMPRN("Failed processing(sending packet to self) to MQ directly.");
								return false;
							}
						}
						continue;
					}
				}
			}else{
				if(!pImData->IsConnectServer(pTmpPkt->head.term_ids.chmpxid)){
					// try to connect all servers
					ConnectServers();					//  not need to check error.
				}
			}

			if(!Send(pTmpPkt, NULL, 0L)){
				// decode for error message
				ChmEventSock::ntoh(pTmpPkt);
				ERR_CHMPRN("Sending ComPkt type(%" PRIu64 ":%s), Something error occured.", pTmpPkt->head.type, STRCOMTYPE(pTmpPkt->head.type));
				if(is_duplicate){
					CHM_Free(pTmpPkt);
				}
				return false;
			}
			if(is_duplicate){
				CHM_Free(pTmpPkt);
			}
		}

	}else if(COM_C2PX == pComPkt->head.type){
		// This case is received MQ data to chmpx process(PX)
		// (The other case is error.)
		//
		// check length
		if(pComPkt->length <= sizeof(COMPKT) || 0 == pComPkt->offset){
			ERR_CHMPRN("ComPkt type(%" PRIu64 ":%s) has invalid values.", pComPkt->head.type, STRCOMTYPE(pComPkt->head.type));
			return false;
		}
		// following datas
		PPXCLT_ALL	pPxCltCom	= CVT_CLT_ALL_PTR_PXCOMPKT(pComPkt);

		if(CHMPX_CLT_JOIN_NOTIFY == pPxCltCom->val_head.type){
			if(!PxCltReceiveJoinNotify(&(pComPkt->head), pPxCltCom)){
				// Failed Join Notify
				ERR_CHMPRN("Received PXCLTCOM type(%" PRIu64 ":%s). Something error occured.", pPxCltCom->val_head.type, STRPXCLTTYPE(pPxCltCom->val_head.type));
				return false;
			}
		}else{
			ERR_CHMPRN("Could not handle PxCltCom type(%" PRIu64 ":%s) in ComPkt type(%" PRIu64 ":%s).", pPxCltCom->val_head.type, STRPXCOMTYPE(pPxCltCom->val_head.type), pComPkt->head.type, STRCOMTYPE(pComPkt->head.type));
			return false;
		}

	}else{
		ERR_CHMPRN("Could not handle ComPkt type(%" PRIu64 ":%s).", pComPkt->head.type, STRCOMTYPE(pComPkt->head.type));
		return false;
	}
	return true;
}

bool ChmEventSock::Processing(int sock, const char* pCommand)
{
	if(CHM_INVALID_SOCK == sock || !pCommand){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	strlst_t	cmdarray;
	if(!str_paeser(pCommand, cmdarray) || cmdarray.empty()){
		ERR_CHMPRN("Something wrong %s command, because could not parse it.", pCommand);
		return false;
	}
	string	strCommand = cmdarray.front();
	cmdarray.pop_front();

	string	strResponse;

	if(0 == strcasecmp(strCommand.c_str(), CTL_COMMAND_VERSION)){
		// Version
		strResponse = chmpx_get_version() + string("\n");

	}else if(0 == strcasecmp(strCommand.c_str(), CTL_COMMAND_DUMP_IMDATA)){
		// Dump
		stringstream	sstream;
		pImData->Dump(sstream);
		strResponse = sstream.str();

	}else if(0 == strcasecmp(strCommand.c_str(), CTL_COMMAND_START_MERGE)){
		// Start Merge
		//
		// The merging flow is sending CHMPX_COM_N2_STATUS_CONFIRM, and sending CHMPX_COM_MERGE_START
		// after returning CHMPX_COM_N2_STATUS_CONFIRM result on RING. After getting CHMPX_COM_MERGE_START
		// start to merge for self.
		//
		if(!CtlComMergeStart(strResponse)){
			ERR_CHMPRN("CTL_COMMAND_START_MERGE is failed, so stop merging.");
		}else{
			MSG_CHMPRN("CTL_COMMAND_START_MERGE is succeed, so do next step after receiving result.");
		}

	}else if(0 == strcasecmp(strCommand.c_str(), CTL_COMMAND_STOP_MERGE)){
		// Abort Merge
		if(!CtlComMergeAbort(strResponse)){
			ERR_CHMPRN("CTL_COMMAND_STOP_MERGE is failed, so continue merging.");
		}else{
			MSG_CHMPRN("CTL_COMMAND_STOP_MERGE is succeed, so stop merging.");
		}

	}else if(0 == strcasecmp(strCommand.c_str(), CTL_COMMAND_COMPLETE_MERGE)){
		// Complete Merge
		if(!CtlComMergeComplete(strResponse)){
			ERR_CHMPRN("CTL_COMMAND_COMPLETE_MERGE is failed, so stop merging.");
		}else{
			MSG_CHMPRN("CTL_COMMAND_COMPLETE_MERGE is succeed, so do next step after receiving result.");
		}

	}else if(0 == strcasecmp(strCommand.c_str(), CTL_COMMAND_SUSPEND_MERGE)){
		// Suspend Merge
		if(!CtlComMergeSuspend(strResponse)){
			ERR_CHMPRN("CTL_COMMAND_SUSPEND_MERGE is failed, so automatically merging is default configuration.");
		}else{
			MSG_CHMPRN("CTL_COMMAND_SUSPEND_MERGE is succeed, so automatically merging is suspended.");
		}

	}else if(0 == strcasecmp(strCommand.c_str(), CTL_COMMAND_NOSUP_MERGE)){
		// Reset Merge mode
		if(!CtlComMergeNoSuspend(strResponse)){
			ERR_CHMPRN("CTL_COMMAND_NOSUP_MERGE is failed, so automatically merging is unknown(you can see it by dump command).");
		}else{
			MSG_CHMPRN("CTL_COMMAND_NOSUP_MERGE is succeed, so automatically merging is set default(configuration).");
		}

	}else if(0 == strcasecmp(strCommand.c_str(), CTL_COMMAND_SERVICE_IN)){
		// Service IN
		//
		if(!CtlComServiceIn(strResponse)){
			ERR_CHMPRN("CTL_COMMAND_SERVICE_IN is failed.");
		}else{
			MSG_CHMPRN("CTL_COMMAND_SERVICE_IN is succeed.");
		}

	}else if(0 == strcasecmp(strCommand.c_str(), CTL_COMMAND_SERVICE_OUT)){
		// Service OUT(status is changed to delete pending)
		//
		if(cmdarray.empty()){
			ERR_CHMPRN(CTLCOM_SERVICEOUT_ERRMSG, strCommand.c_str());
			strResponse = CTL_RES_ERROR_SERVICE_OUT_PARAM;
		}else{
			CHMPXID_SEED_TYPE	type		= pImData->GetChmpxSeedType();
			string				strTmp		= cmdarray.front();
			strlst_t			paramarray;
			string				strServer;
			short				ctlport		= CHM_INVALID_PORT;
			string				strCuk;
			string				strCtleps;
			string				strCustom;

			if(CHMPXID_SEED_CUK == type){
				if(!str_paeser(strTmp.c_str(), paramarray, ":") || 3 != paramarray.size()){
					ERR_CHMPRN(CTLCOM_SERVICEOUT_ERRMSG, strCommand.c_str());
					strResponse = CTL_RES_ERROR_SERVICE_OUT_PARAM;
				}else{
					strServer			= paramarray.front();	paramarray.pop_front();
					string	strCtlPort	= paramarray.front();	paramarray.pop_front();
					ctlport				= static_cast<short>(atoi(strCtlPort.c_str()));
					strCuk				= paramarray.front();
				}

			}else if(CHMPXID_SEED_CTLENDPOINTS == type){
				string::size_type	pos;
				if(string::npos == (pos = strTmp.find(':'))){
					ERR_CHMPRN(CTLCOM_SERVICEOUT_ERRMSG, strCommand.c_str());
					strResponse = CTL_RES_ERROR_SERVICE_OUT_PARAM;
				}else{
					strServer			= strTmp.substr(0, pos);
					strTmp				= strTmp.substr(pos + 1);
					if(string::npos == (pos = strTmp.find(':'))){
						ERR_CHMPRN(CTLCOM_SERVICEOUT_ERRMSG, strCommand.c_str());
						strResponse = CTL_RES_ERROR_SERVICE_OUT_PARAM;
					}else{
						string	strCtlPort	= strTmp.substr(0, pos);
						ctlport				= static_cast<short>(atoi(strCtlPort.c_str()));
						strCtleps			= strTmp.substr(pos + 1);
						if(strCtleps.empty()){
							ERR_CHMPRN(CTLCOM_SERVICEOUT_ERRMSG, strCommand.c_str());
							strResponse = CTL_RES_ERROR_SERVICE_OUT_PARAM;
						}
					}
				}

			}else if(CHMPXID_SEED_CUSTOM == type){
				if(!str_paeser(strTmp.c_str(), paramarray, ":") || 3 != paramarray.size()){
					ERR_CHMPRN(CTLCOM_SERVICEOUT_ERRMSG, strCommand.c_str());
					strResponse = CTL_RES_ERROR_SERVICE_OUT_PARAM;
				}else{
					strServer			= paramarray.front();	paramarray.pop_front();
					string	strCtlPort	= paramarray.front();	paramarray.pop_front();
					ctlport				= static_cast<short>(atoi(strCtlPort.c_str()));
					strCustom			= paramarray.front();
				}

			}else{	// CHMPXID_SEED_NAME
				if(!str_paeser(strTmp.c_str(), paramarray, ":") || paramarray.size() < 2 || 3 < paramarray.size()){
					ERR_CHMPRN(CTLCOM_SERVICEOUT_ERRMSG, strCommand.c_str());
					strResponse = CTL_RES_ERROR_SERVICE_OUT_PARAM;
				}else{
					strServer			= paramarray.front();	paramarray.pop_front();
					string	strCtlPort	= paramarray.front();	paramarray.pop_front();
					ctlport				= static_cast<short>(atoi(strCtlPort.c_str()));
					strCuk				= paramarray.empty() ? "" : paramarray.front();
				}
			}
			if(strResponse.empty()){
				if(!CtlComServiceOut(strServer.c_str(), ctlport, strCuk.c_str(), strCtleps.c_str(), strCustom.c_str(), pCommand, strResponse)){
					ERR_CHMPRN("CTL_COMMAND_SERVICE_OUT is failed.");
				}else{
					MSG_CHMPRN("CTL_COMMAND_SERVICE_OUT is succeed.");
				}
			}
		}

	}else if(0 == strcasecmp(strCommand.c_str(), CTL_COMMAND_SELF_STATUS)){
		// Print self status
		//
		if(!CtlComSelfStatus(strResponse)){
			ERR_CHMPRN("CTL_COMMAND_SELF_STATUS is failed.");
		}else{
			MSG_CHMPRN("CTL_COMMAND_SELF_STATUS is succeed.");
		}

	}else if(0 == strcasecmp(strCommand.c_str(), CTL_COMMAND_ALLSVR_STATUS)){
		// Print all server status
		//
		if(!CtlComAllServerStatus(strResponse)){
			ERR_CHMPRN("CTL_COMMAND_SELF_STATUS is failed.");
		}else{
			MSG_CHMPRN("CTL_COMMAND_SELF_STATUS is succeed.");
		}

	}else if(0 == strcasecmp(strCommand.c_str(), CTL_COMMAND_UPDATE_STATUS)){
		// Print all server status
		//
		if(!CtlComUpdateStatus(strResponse)){
			ERR_CHMPRN("CTL_COMMAND_UPDATE_STATUS is failed.");
		}else{
			MSG_CHMPRN("CTL_COMMAND_UPDATE_STATUS is succeed.");
		}

	}else if(0 == strcasecmp(strCommand.c_str(), CTL_COMMAND_TRACE_SET)){
		// Trace dis/enable
		//
		if(cmdarray.empty()){
			ERR_CHMPRN("%s command must have parameter for enable/disable.", strCommand.c_str());
			strResponse = CTL_RES_ERROR_TRACE_SET_PARAM;
		}else{
			string	strTmp = cmdarray.front();
			bool	enable = false;
			if(0 == strcasecmp(strTmp.c_str(), CTL_COMMAND_TRACE_SET_ENABLE1) || 0 == strcasecmp(strTmp.c_str(), CTL_COMMAND_TRACE_SET_ENABLE2) || 0 == strcasecmp(strTmp.c_str(), CTL_COMMAND_TRACE_SET_ENABLE3)){
				enable = true;
			}else if(0 == strcasecmp(strTmp.c_str(), CTL_COMMAND_TRACE_SET_DISABLE1) || 0 == strcasecmp(strTmp.c_str(), CTL_COMMAND_TRACE_SET_DISABLE2) || 0 == strcasecmp(strTmp.c_str(), CTL_COMMAND_TRACE_SET_DISABLE3)){
				enable = false;
			}else{
				ERR_CHMPRN("%s command parameter(%s) must be enable or disable.", strCommand.c_str(), strTmp.c_str());
				strResponse = CTL_RES_ERROR_TRACE_SET_PARAM;
			}

			if(!CtlComAllTraceSet(strResponse, enable)){
				ERR_CHMPRN("CTL_COMMAND_TRACE_SET is failed.");
			}else{
				MSG_CHMPRN("CTL_COMMAND_TRACE_SET is succeed.");
			}
		}

	}else if(0 == strcasecmp(strCommand.c_str(), CTL_COMMAND_TRACE_VIEW)){
		// Print Trace log
		//
		long	view_count = pImData->GetTraceCount();

		if(0 == view_count){
			ERR_CHMPRN("Now trace count is 0.");
			strResponse = CTL_RES_ERROR_TRACE_VIEW_NOTRACE;
		}else{
			bool		isError	= false;
			long		count	= view_count;
			logtype_t	dirmask = CHMLOG_TYPE_UNKOWN;
			logtype_t	devmask = CHMLOG_TYPE_UNKOWN;

			if(cmdarray.empty()){
				MSG_CHMPRN("%s command does not have parameter, so use default value(dir=all, dev=all, count=all trace count).", strCommand.c_str());
			}else{
				// cppcheck-suppress unmatchedSuppression
				// cppcheck-suppress knownConditionTrueFalse
				for(string strTmp = ""; !cmdarray.empty(); cmdarray.pop_front()){
					strTmp = cmdarray.front();

					// cppcheck-suppress unmatchedSuppression
					// cppcheck-suppress stlIfStrFind
					if(0 == strTmp.find(CTL_COMMAND_TRACE_VIEW_DIR)){
						strTmp = strTmp.substr(strlen(CTL_COMMAND_TRACE_VIEW_DIR));

						if(0 == strcasecmp(strTmp.c_str(), CTL_COMMAND_TRACE_VIEW_IN)){
							dirmask |= CHMLOG_TYPE_INPUT;
						}else if(0 == strcasecmp(strTmp.c_str(), CTL_COMMAND_TRACE_VIEW_OUT)){
							dirmask |= CHMLOG_TYPE_OUTPUT;
						}else if(0 == strcasecmp(strTmp.c_str(), CTL_COMMAND_TRACE_VIEW_ALL)){
							dirmask |= (CHMLOG_TYPE_INPUT | CHMLOG_TYPE_OUTPUT);
						}else{
							ERR_CHMPRN("DIR parameter %s does not defined.", strTmp.c_str());
							isError	= true;
							break;
						}
					// cppcheck-suppress unmatchedSuppression
					// cppcheck-suppress stlIfStrFind
					}else if(0 == strTmp.find(CTL_COMMAND_TRACE_VIEW_DEV)){
						strTmp = strTmp.substr(strlen(CTL_COMMAND_TRACE_VIEW_DEV));

						if(0 == strcasecmp(strTmp.c_str(), CTL_COMMAND_TRACE_VIEW_SOCK)){
							devmask |= CHMLOG_TYPE_SOCKET;
						}else if(0 == strcasecmp(strTmp.c_str(), CTL_COMMAND_TRACE_VIEW_MQ)){
							devmask |= CHMLOG_TYPE_MQ;
						}else if(0 == strcasecmp(strTmp.c_str(), CTL_COMMAND_TRACE_VIEW_ALL)){
							devmask |= (CHMLOG_TYPE_SOCKET | CHMLOG_TYPE_MQ);
						}else{
							ERR_CHMPRN("DEV parameter %s does not defined.", strTmp.c_str());
							isError	= true;
							break;
						}
					}else{
						count = static_cast<long>(atoi(strTmp.c_str()));
						if(0 == count || view_count < count){
							ERR_CHMPRN("view count %ld is %s.", count, (0 == count ? "0" : "over trace maximum count"));
							isError	= true;
							break;
						}
					}
				}
			}
			if(isError){
				strResponse = CTL_RES_ERROR_TRACE_VIEW_PARAM;
			}else{
				if(0 == (dirmask & CHMLOG_MASK_DIRECTION)){
					dirmask |= (CHMLOG_TYPE_OUTPUT | CHMLOG_TYPE_INPUT);
				}
				if(0 == (devmask & CHMLOG_MASK_DEVICE)){
					devmask |= (CHMLOG_TYPE_SOCKET | CHMLOG_TYPE_MQ);
				}

				if(!CtlComAllTraceView(strResponse, dirmask, devmask, count)){
					ERR_CHMPRN("CTL_COMMAND_TRACE_VIEW is failed.");
				}else{
					MSG_CHMPRN("CTL_COMMAND_TRACE_VIEW is succeed.");
				}
			}
		}

	}else{
		// error
		ERR_CHMPRN("Unknown command(%s).", strCommand.c_str());
		strResponse  = "Unknown command(";
		strResponse += strCommand;
		strResponse += ")\n";
	}

	// send
	//
	// [NOTE] This socket is control port socket.
	//
	return ChmEventSock::LockedSend(sock, GetSSL(sock), reinterpret_cast<const unsigned char*>(strResponse.c_str()), strResponse.length());
}

bool ChmEventSock::ChangeStatusBeforeMergeStart(void)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// check operating(merging)
	if(pImData->IsOperating()){
		ERR_CHMPRN("Now operating(merging) now, then could not start merging.");
		return false;
	}

	// Check self status
	chmpxsts_t	status		= pImData->GetSelfStatus();
	chmpxsts_t	old_status	= status;

	if(IS_CHMPXSTS_OPERATING(status)){
		MSG_CHMPRN("Already DOING / DONE status(0x%016" PRIx64 ":%s), so not change status and nothing to do.", status, STR_CHMPXSTS_FULL(status).c_str());
		return false;
	}
	if(!IS_CHMPXSTS_SRVIN(status)){
		// [SERVICE OUT] or [SLAVE] --> nothing to do
		MSG_CHMPRN("status(0x%016" PRIx64 ":%s) is SLAVE or SERVICE OUT, so not change status and nothing to do.", status, STR_CHMPXSTS_FULL(status).c_str());
		return false;
	}
	if(IS_CHMPXSTS_UP(status)){
		if(!IS_CHMPXSTS_PENDING(status)){
			MSG_CHMPRN("Server status(0x%016" PRIx64 ":%s) is not PENDING, so could not change status.", status, STR_CHMPXSTS_FULL(status).c_str());
			return false;
		}
		if(IS_CHMPXSTS_SUSPEND(status)){
			WAN_CHMPRN("Server status(0x%016" PRIx64 ":%s) is SUSPEND, so could not change status.", status, STR_CHMPXSTS_FULL(status).c_str());
			return false;
		}

		// check whichever this server is needed to merge, it can check by hash values.
		chmhash_t	base_hash		= CHM_INVALID_HASHVAL;
		chmhash_t	pending_hash	= CHM_INVALID_HASHVAL;
		chmhash_t	max_base_hash	= CHM_INVALID_HASHVAL;
		chmhash_t	max_pending_hash= CHM_INVALID_HASHVAL;
		if(!pImData->GetSelfHash(base_hash, pending_hash) || !pImData->GetMaxHashCount(max_base_hash, max_pending_hash)){
			ERR_CHMPRN("Could not get base/pending hash value and those max value.");
			return false;
		}
		if(!IS_CHMPXSTS_NOACT(status) && (base_hash == pending_hash && max_base_hash == max_pending_hash)){
			MSG_CHMPRN("this server has status(0x%016" PRIx64 ":%s) and base/pending hash(0x%016" PRIx64 ") is same on not NOACT status, so nothing to do.", status, STR_CHMPXSTS_FULL(status).c_str(), base_hash);
			return false;
		}

		// new status(to DOING)
		SET_CHMPXSTS_DOING(status);

	}else if(IS_CHMPXSTS_DOWN(status)){
		// This case is impossible, but check it.
		if(IS_CHMPXSTS_NOACT(status) && IS_CHMPXSTS_NOTHING(status)){
			// [SERVICE IN] [DOWN] [NOACT] [NOTHING] --> nothing to do
			MSG_CHMPRN("status(0x%016" PRIx64 ":%s) does not need to merge, so not change status and nothing to do.", status, STR_CHMPXSTS_FULL(status).c_str());
			return false;
		}else if(IS_CHMPXSTS_DELETE(status) && IS_CHMPXSTS_PENDING(status)){
			// [SERVICE IN] [DOWN] [DELETE] [PENDING] --> [SERVICE IN] [DOWN] [DELETE] [DONE]
			SET_CHMPXSTS_DONE(status);
		}else{
			MSG_CHMPRN("Unknown status(0x%016" PRIx64 ":%s), so not change status and nothing to do.", status, STR_CHMPXSTS_FULL(status).c_str());
			return false;
		}
	}
	MSG_CHMPRN("Status(0x%016" PRIx64 ":%s) is changed from old status(0x%016" PRIx64 ":%s).", status, STR_CHMPXSTS_FULL(status).c_str(), old_status, STR_CHMPXSTS_FULL(old_status).c_str());

	// set new status
	if(!pImData->SetSelfStatus(status)){
		ERR_CHMPRN("Failed to change self status to 0x%016" PRIx64 ":%s", status, STR_CHMPXSTS_FULL(status).c_str());
		return false;
	}

	// send status update
	// 
	// If error occured in sending status change, but continue to merge.
	// 
	chmpxid_t	to_chmpxid = GetNextRingChmpxId();
	CHMPXSVR	selfchmpxsvr;
	if(!pImData->GetSelfChmpxSvr(&selfchmpxsvr)){
		ERR_CHMPRN("Could not get self chmpx information,");
		return false;
	}
	if(CHM_INVALID_CHMPXID == to_chmpxid){
		// no server found, finish doing so pending hash updated self.
		WAN_CHMPRN("Could not get to chmpxid, probably there is no server without self chmpx on RING. So only sending status update to slaves.");

		if(!PxComSendSlavesStatusChange(&selfchmpxsvr)){
			ERR_CHMPRN("Failed to send self status change to slaves, but continue...");
		}
	}else{
		if(!PxComSendStatusChange(to_chmpxid, &selfchmpxsvr)){
			ERR_CHMPRN("Failed to send self status change, but continue...");
		}
	}
	return true;
}

// [NOTE]
// If there are down servers, this server changes down server's status and
// send it on behalf of down server before starting merging.
//
bool ChmEventSock::ChangeDownSvrStatusByMerge(CHMDOWNSVRMERGE type)
{
	if(CHM_DOWNSVR_MERGE_START != type && CHM_DOWNSVR_MERGE_COMPLETE != type && CHM_DOWNSVR_MERGE_ABORT != type){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// get all server status
	PCHMPXSVR	pchmpxsvrs	= NULL;
	long		count		= 0L;
	if(!pImData->GetChmpxSvrs(&pchmpxsvrs, count)){
		ERR_CHMPRN("Could not get all server status, so stop update status.");
		return false;
	}

	chmpxid_t	to_chmpxid = GetNextRingChmpxId();

	// loop: check down servers and update it's status.
	for(long cnt = 0; cnt < count; ++cnt){
		// check status:	merge start		-> SERVICEIN / DOWN / DELETE / PENDING
		// 					merge complete	-> SERVICEIN / DOWN / DELETE
		//					merge abort		-> SERVICEIN / DOWN / DELETE / PENDING or DONE
		if(	(CHM_DOWNSVR_MERGE_START	== type && IS_CHMPXSTS_SRVIN(pchmpxsvrs[cnt].status) && IS_CHMPXSTS_DOWN(pchmpxsvrs[cnt].status) && IS_CHMPXSTS_DELETE(pchmpxsvrs[cnt].status) && !IS_CHMPXSTS_DONE(pchmpxsvrs[cnt].status)) ||
			(CHM_DOWNSVR_MERGE_COMPLETE	== type && IS_CHMPXSTS_SRVIN(pchmpxsvrs[cnt].status) && IS_CHMPXSTS_DOWN(pchmpxsvrs[cnt].status) && IS_CHMPXSTS_DELETE(pchmpxsvrs[cnt].status))	||
			(CHM_DOWNSVR_MERGE_ABORT	== type && IS_CHMPXSTS_SRVIN(pchmpxsvrs[cnt].status) && IS_CHMPXSTS_DOWN(pchmpxsvrs[cnt].status) && IS_CHMPXSTS_DELETE(pchmpxsvrs[cnt].status))	)
		{
			// set new status:	merge start		-> SERVICEIN / DOWN / DELETE / DONE
			//					merge complete	-> SERVICEOUT/ DOWN / NOACT  / NOTHING
			//					merge abort		-> SERVICEIN / DOWN / NOACT  / NOTHING
			chmpxsts_t	newstatus = CHM_DOWNSVR_MERGE_START == type ? CHMPXSTS_SRVIN_DOWN_DELETED : CHMPXSTS_SRVOUT_DOWN_NORMAL;

			// set new status and hash
			if(!pImData->SetServerStatus(pchmpxsvrs[cnt].chmpxid, newstatus) || (CHM_DOWNSVR_MERGE_COMPLETE == type && !pImData->SetServerBaseHash(pchmpxsvrs[cnt].chmpxid, CHM_INVALID_HASHVAL))){
				ERR_CHMPRN("Could not change server(0x%016" PRIx64 ") status to 0x%016" PRIx64 ":%s.", pchmpxsvrs[cnt].chmpxid, newstatus, STR_CHMPXSTS_FULL(newstatus).c_str());

			}else{
				// Update pending hash.
				if(!pImData->UpdateHash(HASHTG_PENDING)){
					ERR_CHMPRN("Failed to update pending hash for all servers, so stop update status.");
					return false;
				}

				// reget status
				CHMPXSVR	chmpxsvr;
				if(!pImData->GetChmpxSvr(pchmpxsvrs[cnt].chmpxid, &chmpxsvr)){
					ERR_CHMPRN("Could not get server(0x%016" PRIx64 ") information.", pchmpxsvrs[cnt].chmpxid);

				}else{
					// send status update
					if(CHM_INVALID_CHMPXID == to_chmpxid){
						// no server found, finish doing so pending hash updated self.
						WAN_CHMPRN("Could not get to chmpxid, probably there is no server without self chmpx on RING. So only sending status update to slaves.");
						if(!PxComSendSlavesStatusChange(&chmpxsvr)){
							ERR_CHMPRN("Failed to send server(0x%016" PRIx64 ") status to slaves, but continue...", pchmpxsvrs[cnt].chmpxid);
						}
					}else{
						if(!PxComSendStatusChange(to_chmpxid, &chmpxsvr)){
							ERR_CHMPRN("Failed to send server(0x%016" PRIx64 ") status to servers, but continue...", pchmpxsvrs[cnt].chmpxid);
						}
					}
				}
			}
		}
	}
	CHM_Free(pchmpxsvrs);

	return true;
}

//
// CAREFUL
// This method is called from another thread for merging worker.
//
bool ChmEventSock::MergeDone(void)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// Check status
	chmpxsts_t	status		= pImData->GetSelfStatus();
	chmpxsts_t	old_status	= status;
	if(!IS_CHMPXSTS_SRVIN(status)){
		ERR_CHMPRN("Server status(0x%016" PRIx64 ":%s) is not CHMPXSTS_VAL_SRVIN, so could not change status.", status, STR_CHMPXSTS_FULL(status).c_str());
		return false;
	}
	if(!IS_CHMPXSTS_DOING(status)){
		WAN_CHMPRN("Already status(0x%016" PRIx64 ":%s) is not \"DOING\", so not change status and nothing to do.", status, STR_CHMPXSTS_FULL(status).c_str());
		if(IS_CHMPXSTS_DONE(status)){
			return true;
		}
		return false;
	}
	if(IS_CHMPXSTS_UP(status) && IS_CHMPXSTS_SUSPEND(status)){
		ERR_CHMPRN("Server status(0x%016" PRIx64 ":%s) is SUSPEND, so could not change status.", status, STR_CHMPXSTS_FULL(status).c_str());
		return false;
	}

	// new status
	SET_CHMPXSTS_DONE(status);
	MSG_CHMPRN("Status(0x%016" PRIx64 ":%s) is changed from old status(0x%016" PRIx64 ":%s).", status, STR_CHMPXSTS_FULL(status).c_str(), old_status, STR_CHMPXSTS_FULL(old_status).c_str());

	// set new status(set suspend in this method)
	if(!pImData->SetSelfStatus(status)){
		ERR_CHMPRN("Failed to change self status to 0x%016" PRIx64 ":%s", status, STR_CHMPXSTS_FULL(status).c_str());
		return false;
	}

	// send status change
	chmpxid_t	to_chmpxid = GetNextRingChmpxId();
	CHMPXSVR	selfchmpxsvr;
	bool		need_complete = false;
	if(!pImData->GetSelfChmpxSvr(&selfchmpxsvr)){
		WAN_CHMPRN("Could not get self chmpx information, but continue...");
	}else{
		if(CHM_INVALID_CHMPXID == to_chmpxid){
			// no server found, finish doing so pending hash updated self.
			WAN_CHMPRN("Could not get to chmpxid, probably there is no server without self chmpx on RING. So only sending status update to slaves.");
			need_complete = true;

			if(!PxComSendSlavesStatusChange(&selfchmpxsvr)){
				WAN_CHMPRN("Failed to send self status change to slaves, but continue...");
			}else{
				// [NOTE]
				// If there is no slave, special processing is performed.
				//
				chmpxidlist_t	chmpxidlist;
				if(IsAutoMerge() && 0L == pImData->GetSlaveChmpxIds(chmpxidlist)){
					MSG_CHMPRN("There is no server/slave node, then start to merge complete directly here.");
					if(!RequestMergeComplete()){
						ERR_CHMPRN("Could not change status merge \"COMPLETE\", probably another server does not change status yet, hope to recover automatic or DO COMPMERGE BY MANUAL!");
						return false;
					}
				}
			}
		}else{
			// push status changed
			if(!PxComSendStatusChange(to_chmpxid, &selfchmpxsvr)){
				WAN_CHMPRN("Failed to send self status change, but continue...");
			}
		}
	}

	// if the mode is automatically merging, set flag here.
	if(IsAutoMerge()){
		if(need_complete){
			MSG_CHMPRN("Start to merge complete automatically.");
			if(!RequestMergeComplete()){
				ERR_CHMPRN("Could not change status merge \"COMPLETE\", probably another server does not change status yet, hope to recover automatic or DO COMPMERGE BY MANUAL!");
			}
		}else{
			is_merge_done_processing = true;
		}
	}
	return true;
}

bool ChmEventSock::IsAutoMerge(void)
{
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();
	return pImData->IsAutoMerge();
}

bool ChmEventSock::IsDoMerge(void)
{
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();
	return pImData->IsDoMerge();
}

bool ChmEventSock::ContinuousAutoMerge(void)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	if(!is_server_mode){
		MSG_CHMPRN("Continuous Merging automatically must be on server node.");
		return false;
	}
	if(!IsAutoMerge() && !startup_servicein){
		// Nothing to do.
		MSG_CHMPRN("Not merging automatically mode and have no pending SERVICE IN request.");
		return true;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	if(pImData->IsOperating()){
		MSG_CHMPRN("Servers are now operating, then could not start to merge.");
		return true;
	}

	// get now status
	chmpxsts_t	status = pImData->GetSelfStatus();

	// check pending service in operation 
	if(startup_servicein){
		// SERVICE IN operation is pending.
		if(IS_CHMPXSTS_SRVOUT(status) && IS_CHMPXSTS_UP(status) && IS_CHMPXSTS_NOACT(status) && IS_CHMPXSTS_NOTHING(status)){
			if(IS_CHMPXSTS_SUSPEND(status)){
				ERR_CHMPRN("Server status(0x%016" PRIx64 ":%s) is SUSPEND, then could not start to merge now.", status, STR_CHMPXSTS_FULL(status).c_str());
				return true;
			}

			// do service in
			if(!RequestServiceIn()){
				WAN_CHMPRN("Failed to request \"service in\" status before auto merging, maybe operating now.");
				return true;
			}

			// set pending flag off
			startup_servicein = false;

			// re-get status
			status = pImData->GetSelfStatus();
		}else{
			// Not [SERVICE OUT] [UP] [NOACT] [NOTHING] status
			WAN_CHMPRN("Server is status(0x%016" PRIx64 ":%s), it must be SERVICE OUT / UP / NOACT / NOTHING. but continue...", status, STR_CHMPXSTS_FULL(status).c_str());
			// for recover pending flag
			startup_servicein = false;
		}
	}

	// start merging
	if(!RequestMergeStart()){
		ERR_CHMPRN("Failed to merge or complete merge for \"service in\".");
		return false;
	}
	return true;
}

bool ChmEventSock::CheckAllStatusForMergeStart(void) const
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// check operating(merging)
	if(pImData->IsOperating()){
		ERR_CHMPRN("Now operating(merging) now, then could not start merging.");
		return false;
	}

	// get all server status
	PCHMPXSVR	pchmpxsvrs	= NULL;
	long		count		= 0L;
	if(!pImData->GetChmpxSvrs(&pchmpxsvrs, count)){
		ERR_CHMPRN("Could not get all server status, so stop update status.");
		return false;
	}

	// loop: check wrong status
	bool	result = true;
	for(long cnt = 0; cnt < count; ++cnt){
		if(IS_CHMPXSTS_SRVIN(pchmpxsvrs[cnt].status)){
			if(IS_CHMPXSTS_UP(pchmpxsvrs[cnt].status)){
				if(IS_CHMPXSTS_NOACT(pchmpxsvrs[cnt].status) && IS_CHMPXSTS_SUSPEND(pchmpxsvrs[cnt].status)){
					MSG_CHMPRN("Found SERVICEIN / UP / NOACT / NOTHING / SUSPEND server, then could not start merging.");
					result = false;
					break;
				}
				if(IS_CHMPXSTS_ADD(pchmpxsvrs[cnt].status) && IS_CHMPXSTS_SUSPEND(pchmpxsvrs[cnt].status)){
					MSG_CHMPRN("Found SERVICEIN / UP / ADD / PENDING / SUSPEND server, then could not start merging.");
					result = false;
					break;
				}
				if(IS_CHMPXSTS_DELETE(pchmpxsvrs[cnt].status) && IS_CHMPXSTS_SUSPEND(pchmpxsvrs[cnt].status)){
					MSG_CHMPRN("Found SERVICEIN / UP / DELETE / PENDING / SUSPEND server, then could not start merging.");
					result = false;
					break;
				}
			}else if(IS_CHMPXSTS_DOWN(pchmpxsvrs[cnt].status)){
				if(IS_CHMPXSTS_NOACT(pchmpxsvrs[cnt].status)){
					MSG_CHMPRN("Found SERVICEIN / DOWN / NOACT / NOTHING server, then could not start merging.");
					result = false;
					break;
				}
			}
		}else{
			// Not care for SERVICE OUT servers
		}
	}
	CHM_Free(pchmpxsvrs);

	return result;
}

bool ChmEventSock::CheckAllStatusForMergeComplete(void) const
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// get all server status
	PCHMPXSVR	pchmpxsvrs	= NULL;
	long		count		= 0L;
	if(!pImData->GetChmpxSvrs(&pchmpxsvrs, count)){
		ERR_CHMPRN("Could not get all server status, so stop update status.");
		return false;
	}

	// loop: check wrong status
	bool	result = true;
	for(long cnt = 0; cnt < count; ++cnt){
		if(IS_CHMPXSTS_SRVIN(pchmpxsvrs[cnt].status)){
			if(IS_CHMPXSTS_UP(pchmpxsvrs[cnt].status)){
				if(IS_CHMPXSTS_DOING(pchmpxsvrs[cnt].status)){
					MSG_CHMPRN("Found SERVICEIN / UP / (NOACT or ADD or DELETE) / DOING server, then could not complete merging.");
					result = false;
					break;
				}
			}else if(IS_CHMPXSTS_DOWN(pchmpxsvrs[cnt].status)){
				if(IS_CHMPXSTS_DELETE(pchmpxsvrs[cnt].status) && !IS_CHMPXSTS_PENDING(pchmpxsvrs[cnt].status) && !IS_CHMPXSTS_DONE(pchmpxsvrs[cnt].status)){
					MSG_CHMPRN("Found SERVICEIN / DOWN / DELETE / !(PENDING or DONE) server, then could not complete merging.");
					result = false;
					break;
				}
			}
		}else{
			// Not care for SERVICE OUT servers
		}
	}
	CHM_Free(pchmpxsvrs);

	return result;
}

//
// This method sends only "N2_STATUS_CONFIRM".
// After that, this process receives "STATUS_CONFIRM" result, and sends "MERGE_START" automatically.
//
bool ChmEventSock::RequestMergeStart(string* pstring)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		if(pstring){
			*pstring = CTL_RES_INT_ERROR;
		}
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// check all server status
	if(!CheckAllStatusForMergeStart()){
		if(pstring){
			*pstring = CTL_RES_ERROR_SOME_SERVER_STATUS;
		}
		return false;
	}

	// get terminated chmpxid
	chmpxid_t	chmpxid = GetNextRingChmpxId();
	if(CHM_INVALID_CHMPXID == chmpxid){
		// no server found, finish doing so pending hash updated self.
		WAN_CHMPRN("Could not get to chmpxid, probably there is no server without self chmpx on RING. So stop sending status update.");

		// Start to merge(only self on RING)
		//
		// If there is no server in RING, do start merge.
		//
		if(!MergeStart()){
			ERR_CHMPRN("Failed to start merge.");
			if(pstring){
				*pstring = CTL_RES_ERROR_MERGE_START;
			}
			return false;
		}else{
			if(pstring){
				*pstring = CTL_RES_SUCCESS_NOSERVER;
			}
			return true;
		}
	}

	// Update self last status updating time 
	if(!pImData->UpdateSelfLastStatusTime()){
		ERR_CHMPRN("Could not update own updating status time, but continue...");
	}

	// get all server status
	PCHMPXSVR	pchmpxsvrs	= NULL;
	long		count		= 0L;
	if(!pImData->GetChmpxSvrs(&pchmpxsvrs, count)){
		ERR_CHMPRN("Could not get all server status, so stop update status.");
		if(pstring){
			*pstring = CTL_RES_INT_ERROR_NOTGETCHMPX;
		}
		CHM_Free(pchmpxsvrs);
		return false;
	}

	// send status_confirm
	if(!PxComSendStatusConfirm(chmpxid, pchmpxsvrs, count)){
		ERR_CHMPRN("Failed to send CHMPX_COM_N2_STATUS_CONFIRM.");
		if(pstring){
			*pstring = CTL_RES_ERROR_COMMUNICATION;
		}
		CHM_Free(pchmpxsvrs);
		return false;
	}
	CHM_Free(pchmpxsvrs);

	if(pstring){
		*pstring = CTL_RES_SUCCESS;
	}
	return true;
}

bool ChmEventSock::MergeStart(void)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}

	// change status "DOING" before merging.
	if(!ChangeStatusBeforeMergeStart()){
		MSG_CHMPRN("Not change status to \"DOING\", because probably nothing to do now.");
		return true;
	}
	// change down server status before merging.
	if(!ChangeDownSvrStatusByMerge(CHM_DOWNSVR_MERGE_START)){
		ERR_CHMPRN("Failed to change down server status, but continue...");
	}

	// start merging.
	if(IsDoMerge()){
		if(is_run_merge){
			ERR_CHMPRN("Already to run merge.");
			return false;
		}

		if(!mergethread.HasThread()){
			ERR_CHMPRN("The thread for merging is not running, why?");
			return false;
		}

		// get chmpxids(targets) which have same basehash or replica basehash.
		ChmIMData*	pImData = pChmCntrl->GetImDataObj();
		chmhash_t	basehash;
		if(!pImData->GetSelfBaseHash(basehash)){
			// there is no server, so set status "DONE" here.
			if(!MergeDone()){
				ERR_CHMPRN("Failed to change status \"DOING to DONE to COMPLETE\".");
				return false;
			}
			return true;
		}
		// we need to collect minimum servers
		chmpxidlist_t	alllist;
		if(pImData->IsPendingExchangeData()){
			// need all servers which are up/noact/nosuspend
			pImData->GetServerChmpxIdForMerge(alllist);
		}else{
			// this case is suspend to nosuspend or down to up(servicein)
			pImData->GetServerChmpxIdByBaseHash(basehash, alllist);
		}

		while(!fullock::flck_trylock_noshared_mutex(&mergeidmap_lockval));	// LOCK

		// make chmpxid map for update datas
		chmpxid_t	selfchmpxid = pImData->GetSelfChmpxId();
		mergeidmap.clear();
		for(chmpxidlist_t::iterator iter = alllist.begin(); iter != alllist.end(); ++iter){
			if(selfchmpxid != *iter){
				mergeidmap[*iter] = CHMPX_COM_REQ_UPDATE_INIVAL;
			}
		}
		if(mergeidmap.empty()){
			// there is no server, so set status "DONE" here.
			fullock::flck_unlock_noshared_mutex(&mergeidmap_lockval);		// UNLOCK

			if(!MergeDone()){
				ERR_CHMPRN("Failed to change status \"DOING to DONE to COMPLETE\".");
				return false;
			}
			return true;
		}
		fullock::flck_unlock_noshared_mutex(&mergeidmap_lockval);			// UNLOCK

		// set "run" status for merging worker method.
		is_run_merge = true;

		// run merging worker thread method.
		if(!mergethread.DoWorkThread()){
			ERR_CHMPRN("Failed to wake up thread for merging.");
			is_run_merge = false;
			return false;
		}
	}else{
		// Do not run merging, so set status "DONE" here.
		//
		if(!MergeDone()){
			ERR_CHMPRN("Failed to change status \"DONE\".");
			return false;
		}
	}
	return true;
}

bool ChmEventSock::MergeAbort(void)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// stop data merging
	if(IsDoMerge()){
		if(!mergethread.HasThread()){
			ERR_CHMPRN("The thread for merging is not running, why? but continue...");
		}else{
			// set "stop" status for merging worker method.
			is_run_merge = false;
		}
	}

	// send abort update data to all client process
	if(!pChmCntrl->MergeAbortUpdateData()){
		ERR_CHMPRN("Failed to send abort update data, but continue...");
	}

	// status check and set new status
	chmpxsts_t	status		= pImData->GetSelfStatus();
	chmpxsts_t	old_status	= status;
	if(IS_CHMPXSTS_SRVIN(status)){
		if(IS_CHMPXSTS_UP(status)){
			if(IS_CHMPXSTS_NOACT(status)){
				if(IS_CHMPXSTS_DOING(status) || IS_CHMPXSTS_DONE(status)){
					if(IS_CHMPXSTS_NOSUP(status)){
						// [SERVICE IN] [UP] [NOACT] [DOING or DONE] [NOSUSPEND] --> [SERVICE IN] [UP] [NOACT] [PENDING] [NOSUSPEND]
						SET_CHMPXSTS_PENDING(status);
					}else{
						// [SERVICE IN] [UP] [NOACT] [DOING or DONE] [SUSPEND]	 --> [SERVICE IN] [UP] [NOACT] [NOTHING] [SUSPEND]
						// this status is impossible
						SET_CHMPXSTS_NOTHING(status);
					}
				}
				// Nothing to do at [SERVICE IN] [UP] [NOACT] [NOTHING or PENDING]

			}else if(IS_CHMPXSTS_ADD(status)){
				if(IS_CHMPXSTS_DOING(status) || IS_CHMPXSTS_DONE(status)){
					// [SERVICE IN] [UP] [ADD] [DOING or DONE]	--> [SERVICE IN] [UP] [ADD] [PENDING]
					SET_CHMPXSTS_PENDING(status);
				}
				// Nothing to do at [SERVICE IN] [UP] [ADD] [PENDING]

			}else if(IS_CHMPXSTS_DELETE(status)){
				if(IS_CHMPXSTS_NOSUP(status)){
					// [SERVICE IN] [UP] [DELETE] [NOSUSPEND] --> [SERVICE IN] [UP] [NOACT] [PENDING] [NOSUSPEND]
					SET_CHMPXSTS_NOACT(status);
					SET_CHMPXSTS_PENDING(status);
				}else{
					// [SERVICE IN] [UP] [DELETE] [SUSPEND]	 --> [SERVICE IN] [UP] [NOACT] [NOTHING] [SUSPEND]
					SET_CHMPXSTS_NOACT(status);
					SET_CHMPXSTS_NOTHING(status);
				}
			}
		}else if(IS_CHMPXSTS_DOWN(status)){
			if(IS_CHMPXSTS_DELETE(status)){
				// [SERVICE IN] [DOWN] [DELETE]	--> [SERVICE IN] [DOWN] [NOACT] [PENDING]
				SET_CHMPXSTS_NOACT(status);
				SET_CHMPXSTS_NOTHING(status);
			}
			SET_CHMPXSTS_NOSUP(status);
		}
	}
	MSG_CHMPRN("Changed status(0x%016" PRIx64 ":%s) from old status(0x%016" PRIx64 ":%s).", status, STR_CHMPXSTS_FULL(status).c_str(), old_status, STR_CHMPXSTS_FULL(old_status).c_str());

	if(!pImData->SetSelfStatus(status)){
		ERR_CHMPRN("Failed to update(rewind) status(0x%061lx:%s).", status, STR_CHMPXSTS_FULL(status).c_str());
		return false;
	}

	// Update pending hash.
	if(!pImData->UpdateHash(HASHTG_PENDING)){
		ERR_CHMPRN("Failed to update pending hash for all servers, so stop update status.");
		return false;
	}

	// send status change
	CHMPXSVR	chmpxsvr;
	if(!pImData->GetSelfChmpxSvr(&chmpxsvr)){
		ERR_CHMPRN("Could not get self chmpx information.");
		return false;
	}
	chmpxid_t	nextchmpxid = GetNextRingChmpxId();
	if(CHM_INVALID_CHMPXID == nextchmpxid){
		WAN_CHMPRN("Could not get next chmpxid, probably there is no server without self chmpx on RING.");

		if(!PxComSendSlavesStatusChange(&chmpxsvr)){
			ERR_CHMPRN("Failed to send self status change to slaves.");
			return false;
		}
	}else{
		if(!PxComSendStatusChange(nextchmpxid, &chmpxsvr)){
			ERR_CHMPRN("Failed to send self status change.");
			return false;
		}
	}

	// change down server status.
	if(!ChangeDownSvrStatusByMerge(CHM_DOWNSVR_MERGE_ABORT)){
		ERR_CHMPRN("Failed to change down server status, but continue...");
	}
	return true;
}

bool ChmEventSock::RequestMergeComplete(string* pstring)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		if(pstring){
			*pstring = CTL_RES_INT_ERROR;
		}
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// check status
	if(!CheckAllStatusForMergeComplete()){
		MSG_CHMPRN("Some server in RING have been merging yet, so could not change status.");
		if(pstring){
			*pstring = CTL_RES_ERROR_SOME_SERVER_STATUS;
		}
		return false;
	}

	// get terminated chmpxid
	chmpxid_t	chmpxid = GetNextRingChmpxId();
	if(CHM_INVALID_CHMPXID == chmpxid){
		// no server found, finish doing so pending hash updated self.
		WAN_CHMPRN("Could not get to chmpxid, probably there is no server without self chmpx on RING. So stop sending status update.");
		is_merge_done_processing = false;

		// Complete to merge
		//
		// If there is no server in RING, do complete merge.
		//
		if(!MergeComplete()){
			ERR_CHMPRN("Failed to complete merge.");
			if(pstring){
				*pstring = CTL_RES_ERROR_MERGE_COMPLETE;
			}
			return false;
		}
		if(pstring){
			*pstring = CTL_RES_SUCCESS_NOSERVER;
		}
		return true;
	}

	// Update self last status updating time for updating self status
	if(!pImData->UpdateSelfLastStatusTime()){
		ERR_CHMPRN("Could not update own updating status time, but continue...");
	}
	CHMPXSVR	chmpxsvr;
	if(!pImData->GetSelfChmpxSvr(&chmpxsvr)){
		ERR_CHMPRN("Could not get self chmpx information.");
		return false;
	}
	if(!PxComSendStatusChange(chmpxid, &chmpxsvr)){
		ERR_CHMPRN("Failed to send self status change.");
		return false;
	}

	// send complete_merge
	if(!PxComSendMergeComplete(chmpxid)){
		ERR_CHMPRN("Failed to send CHMPX_COM_MERGE_COMPLETE.");
		if(pstring){
			*pstring = CTL_RES_ERROR_COMMUNICATION;
		}
		return false;
	}
	if(pstring){
		*pstring = CTL_RES_SUCCESS;
	}
	return true;
}

bool ChmEventSock::RequestMergeSuspend(string* pstring)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		if(pstring){
			*pstring = CTL_RES_INT_ERROR;
		}
		return false;
	}
	if(!is_server_mode){
		ERR_CHMPRN("SUSPENDMERGE command need to run on server node.");
		if(pstring){
			*pstring = CTL_RES_ERROR_COMMAND_ON_SERVER;
		}
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// Do suspend
	if(!pImData->SuspendAutoMerge()){
		MSG_CHMPRN("Could not suspend auto merge to self.");
		if(pstring){
			*pstring = CTL_RES_ERROR_COULD_NOT_SUSPEND;
		}
		return false;
	}else if(startup_servicein){
		startup_servicein = false;
	}

	// get terminated chmpxid
	chmpxid_t	chmpxid = GetNextRingChmpxId();
	if(CHM_INVALID_CHMPXID == chmpxid){
		// no server found, finish doing so pending hash updated self.
		if(pstring){
			*pstring = CTL_RES_SUCCESS;
		}
		return true;
	}

	// send suspend merge
	if(!PxComSendMergeSuspend(chmpxid)){
		ERR_CHMPRN("Failed to send CHMPX_COM_MERGE_SUSPEND.");
		if(pstring){
			*pstring = CTL_RES_ERROR_COMMUNICATION;
		}
		return false;
	}
	if(pstring){
		*pstring = CTL_RES_SUCCESS;
	}
	return true;
}

bool ChmEventSock::RequestMergeNoSuspend(string* pstring)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		if(pstring){
			*pstring = CTL_RES_INT_ERROR;
		}
		return false;
	}
	if(!is_server_mode){
		ERR_CHMPRN("NOSUSPENDMERGE command need to run on server node.");
		if(pstring){
			*pstring = CTL_RES_ERROR_COMMAND_ON_SERVER;
		}
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// Reset suspend
	if(!pImData->ResetAutoMerge()){
		MSG_CHMPRN("Could not nosuspend auto merge to self.");
		if(pstring){
			*pstring = CTL_RES_ERROR_COULD_NOT_NOSUSPEND;
		}
		return false;
	}

	// get terminated chmpxid
	chmpxid_t	chmpxid = GetNextRingChmpxId();
	if(CHM_INVALID_CHMPXID == chmpxid){
		// no server found, finish doing so pending hash updated self.
		if(pstring){
			*pstring = CTL_RES_SUCCESS;
		}
		return true;
	}

	// send suspend merge
	if(!PxComSendMergeNoSuspend(chmpxid)){
		ERR_CHMPRN("Failed to send CHMPX_COM_MERGE_NOSUSPEND.");
		if(pstring){
			*pstring = CTL_RES_ERROR_COMMUNICATION;
		}
		return false;
	}
	if(pstring){
		*pstring = CTL_RES_SUCCESS;
	}
	return true;
}

bool ChmEventSock::RequestServiceIn(string* pstring)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		if(pstring){
			*pstring = CTL_RES_INT_ERROR;
		}
		return false;
	}
	if(!is_server_mode){
		ERR_CHMPRN("SERVICEIN command need to run on server node.");
		if(pstring){
			*pstring = CTL_RES_ERROR_COMMAND_ON_SERVER;
		}
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	if(pImData->IsOperating()){
		ERR_CHMPRN("Servers are now operating.");
		if(pstring){
			*pstring = CTL_RES_ERROR_OPERATING;
		}
		return false;
	}

	// check status and set new status
	chmpxsts_t	status		= pImData->GetSelfStatus();
	chmpxsts_t	old_status	= status;

	CHANGE_CHMPXSTS_TO_SRVIN(status);
	if(status == old_status){
		ERR_CHMPRN("Server is status(0x%016" PRIx64 ":%s), so could not change status.", status, STR_CHMPXSTS_FULL(status).c_str());
		if(pstring){
			*pstring = CTL_RES_ERROR_STATUS_NOT_ALLOWED;
		}
		return false;
	}
	MSG_CHMPRN("Status(0x%016" PRIx64 ":%s) is changed from old status(0x%016" PRIx64 ":%s).", status, STR_CHMPXSTS_FULL(status).c_str(), old_status, STR_CHMPXSTS_FULL(old_status).c_str());

	// set status(set suspend in this method)
	if(!pImData->SetSelfStatus(status)){
		ERR_CHMPRN("Failed to change server status(0x%016" PRIx64 ").", status);
		if(pstring){
			*pstring = CTL_RES_ERROR_CHANGE_STATUS;
		}
		return false;
	}

	// Update pending hash.
	if(!pImData->UpdateHash(HASHTG_PENDING)){
		ERR_CHMPRN("Failed to update pending hash for all servers, so stop update status.");
		if(pstring){
			*pstring = CTL_RES_ERROR_CHANGE_STATUS;
		}
		return false;
	}

	// send status update
	CHMPXSVR	chmpxsvr;
	if(!pImData->GetSelfChmpxSvr(&chmpxsvr)){
		ERR_CHMPRN("Could not get self chmpx information.");
		if(pstring){
			*pstring = CTL_RES_ERROR_STATUS_NOTICE;
		}
		return false;
	}
	chmpxid_t	nextchmpxid = GetNextRingChmpxId();
	if(CHM_INVALID_CHMPXID == nextchmpxid){
		WAN_CHMPRN("Could not get next chmpxid, probably there is no server without self chmpx on RING.");
		if(!PxComSendSlavesStatusChange(&chmpxsvr)){
			ERR_CHMPRN("Failed to send self status change to slaves.");
			if(pstring){
				*pstring = CTL_RES_ERROR_STATUS_NOTICE;
			}
			return false;
		}
		if(pstring){
			*pstring = CTL_RES_SUCCESS_STATUS_NOTICE;
		}
	}else{
		if(!PxComSendStatusChange(nextchmpxid, &chmpxsvr)){
			ERR_CHMPRN("Failed to send self status change.");
			if(pstring){
				*pstring = CTL_RES_ERROR_STATUS_NOTICE;
			}
			return false;
		}
	}
	return true;
}

bool ChmEventSock::RequestServiceOut(chmpxid_t chmpxid, string* pstring)
{
	string	strDummy = "";
	if(!pstring){
		pstring = &strDummy;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		*pstring = CTL_RES_INT_ERROR;
		return false;
	}
	if(!is_server_mode){
		ERR_CHMPRN("SERVICEOUT command need to run on server node.");
		*pstring = CTL_RES_ERROR_COMMAND_ON_SERVER;
		return false;
	}
	ChmIMData*	pImData		= pChmCntrl->GetImDataObj();
	chmpxid_t	selfchmpxid	= pImData->GetSelfChmpxId();
	if(CHM_INVALID_CHMPXID == chmpxid){
		chmpxid = selfchmpxid;
	}

	// check operating
	if(pImData->IsOperating()){
		ERR_CHMPRN("Servers are now operating.");
		*pstring = CTL_RES_ERROR_OPERATING;
		return false;
	}

	if(selfchmpxid == chmpxid){
		// check status and set new status
		chmpxsts_t	status		= pImData->GetServerStatus(chmpxid);
		chmpxsts_t	old_status	= status;

		CHANGE_CHMPXSTS_TO_SRVOUT(status);
		if(status == old_status){
			ERR_CHMPRN("chmpxid(0x%016" PRIx64 ") already has status(0x%016" PRIx64 ":%s) by service out request, then nothing to do.", chmpxid, status, STR_CHMPXSTS_FULL(status).c_str());
			*pstring = CTL_RES_ERROR_STATUS_NOT_ALLOWED;
			return false;
		}
		MSG_CHMPRN("Status(0x%016" PRIx64 ":%s) is changed from old status(0x%016" PRIx64 ":%s).", status, STR_CHMPXSTS_FULL(status).c_str(), old_status, STR_CHMPXSTS_FULL(old_status).c_str());

		// set new status(set suspend in this method)
		if(!pImData->SetSelfStatus(status)){
			ERR_CHMPRN("Failed to change server status(0x%016" PRIx64 ":%s).", status, STR_CHMPXSTS_FULL(status).c_str());
			*pstring = CTL_RES_ERROR_CHANGE_STATUS;
			return false;
		}

		// Update pending hash.
		if(!pImData->UpdateHash(HASHTG_PENDING)){
			ERR_CHMPRN("Failed to update pending hash for all servers, so stop update status.");
			*pstring = CTL_RES_ERROR_CHANGE_STATUS;
			return false;
		}

		// send status update
		CHMPXSVR	chmpxsvr;
		if(!pImData->GetSelfChmpxSvr(&chmpxsvr)){
			ERR_CHMPRN("Could not get self chmpx information.");
			*pstring = CTL_RES_ERROR_STATUS_NOTICE;
			return false;
		}

		chmpxid_t	nextchmpxid = GetNextRingChmpxId();
		if(CHM_INVALID_CHMPXID == nextchmpxid){
			WAN_CHMPRN("Could not get next chmpxid, probably there is no server without self chmpx on RING.");
			if(!PxComSendSlavesStatusChange(&chmpxsvr)){
				ERR_CHMPRN("Failed to send self status change to slaves.");
				*pstring = CTL_RES_ERROR_STATUS_NOTICE;
				return false;
			}
			*pstring = CTL_RES_SUCCESS_STATUS_NOTICE;
		}
		if(!PxComSendStatusChange(nextchmpxid, &chmpxsvr)){
			ERR_CHMPRN("Failed to send self status change.");
			*pstring = CTL_RES_ERROR_STATUS_NOTICE;
			return false;
		}

	}else{
		// set status on this server and send status update.
		//
		// check status and set new status
		chmpxsts_t	status		= pImData->GetServerStatus(chmpxid);
		chmpxsts_t	old_status	= status;

		CHANGE_CHMPXSTS_TO_SRVOUT(status);
		if(status == old_status){
			WAN_CHMPRN("chmpxid(0x%016" PRIx64 ") already has status(0x%016" PRIx64 ":%s) by service out request, then nothing to do.", chmpxid, status, STR_CHMPXSTS_FULL(status).c_str());
			*pstring = CTL_RES_SUCCESS_ALREADY_SERVICEOUT;
			return true;
		}
		MSG_CHMPRN("server(chmpxid:0x%016" PRIx64 ") status(0x%016" PRIx64 ":%s) try to change from old status(0x%016" PRIx64 ":%s).", chmpxid, status, STR_CHMPXSTS_FULL(status).c_str(), old_status, STR_CHMPXSTS_FULL(old_status).c_str());

		// set new status
		if(!pImData->SetServerStatus(chmpxid, status)){
			ERR_CHMPRN("Failed to change server(chmpxid:0x%016" PRIx64 ") status(0x%016" PRIx64 ":%s).", chmpxid, status, STR_CHMPXSTS_FULL(status).c_str());
			*pstring = CTL_RES_ERROR_CHANGE_STATUS;
			return false;
		}

		// Update pending hash.
		if(!pImData->UpdateHash(HASHTG_PENDING)){
			ERR_CHMPRN("Failed to update pending hash for all servers, so stop update status.");
			*pstring = CTL_RES_ERROR_CHANGE_STATUS;
			return false;
		}

		// send status update
		CHMPXSVR	chmpxsvr;
		if(!pImData->GetChmpxSvr(chmpxid, &chmpxsvr)){
			ERR_CHMPRN("Could not get server(0x%016" PRIx64 ") information.", chmpxid);
			*pstring = CTL_RES_ERROR_STATUS_NOTICE;
			return false;
		}

		chmpxid_t	nextchmpxid = GetNextRingChmpxId();
		if(CHM_INVALID_CHMPXID == nextchmpxid){
			WAN_CHMPRN("Could not get next chmpxid, probably there is no server without self chmpx on RING.");

			if(!PxComSendSlavesStatusChange(&chmpxsvr)){
				ERR_CHMPRN("Failed to send server(0x%016" PRIx64 ") status change to slaves.", chmpxid);
				*pstring = CTL_RES_ERROR_STATUS_NOTICE;
				return false;
			}
			*pstring = CTL_RES_SUCCESS_STATUS_NOTICE;
		}else{
			if(!PxComSendStatusChange(nextchmpxid, &chmpxsvr)){
				ERR_CHMPRN("Failed to send server(0x%016" PRIx64 ") status change.", chmpxid);
				*pstring = CTL_RES_ERROR_STATUS_NOTICE;
				return false;
			}
		}
	}

	// If do not merge(almost random deliver mode), do merging, completing it.
	if(IsAutoMerge()){
		// start merge complete automatically
		if(!RequestMergeComplete(pstring)){
			ERR_CHMPRN("Failed to complete merge for \"service out\".");
			*pstring += "\n";
			*pstring += CTL_RES_ERROR_MERGE_AUTO;
			return false;
		}
	}
	*pstring = CTL_RES_SUCCESS;
	return true;
}

bool ChmEventSock::MergeComplete(void)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// change down server status before merging.
	if(!ChangeDownSvrStatusByMerge(CHM_DOWNSVR_MERGE_COMPLETE)){
		ERR_CHMPRN("Failed to change down server status, but continue...");
	}

	// get pending hash
	chmhash_t	hash;
	if(!pImData->GetSelfPendingHash(hash)){
		ERR_CHMPRN("Failed to get pending hash value.");
		return false;
	}

	// status check & set new status
	chmpxsts_t	status		= pImData->GetSelfStatus();
	chmpxsts_t	old_status	= status;
	if(IS_CHMPXSTS_SRVIN(status)){
		if(IS_CHMPXSTS_UP(status)){
			if(IS_CHMPXSTS_NOACT(status)){
				if(IS_CHMPXSTS_SUSPEND(status)){
					WAN_CHMPRN("Server status(0x%016" PRIx64 ":%s) is SUSPEND, so could not change status.", status, STR_CHMPXSTS_FULL(status).c_str());
					return false;
				}else if(IS_CHMPXSTS_PENDING(status) || IS_CHMPXSTS_DOING(status)){
					WAN_CHMPRN("Server status(0x%016" PRIx64 ":%s) is not DONE/NOTHING, so could not change status.", status, STR_CHMPXSTS_FULL(status).c_str());
					return false;
				}
				status	= CHMPXSTS_SRVIN_UP_NORMAL;

			}else if(IS_CHMPXSTS_ADD(status)){
				if(IS_CHMPXSTS_SUSPEND(status)){
					WAN_CHMPRN("Server status(0x%016" PRIx64 ":%s) is SUSPEND, so could not change status.", status, STR_CHMPXSTS_FULL(status).c_str());
					return false;
				}else if(IS_CHMPXSTS_PENDING(status) || IS_CHMPXSTS_DOING(status)){
					WAN_CHMPRN("Server status(0x%016" PRIx64 ":%s) is not DONE, so could not change status.", status, STR_CHMPXSTS_FULL(status).c_str());
					return false;
				}
				status	= CHMPXSTS_SRVIN_UP_NORMAL;

			}else if(IS_CHMPXSTS_DELETE(status)){
				if(!IS_CHMPXSTS_DONE(status) && IS_CHMPXSTS_SUSPEND(status)){		// DONE and SUSPEND is allowed
					WAN_CHMPRN("Server status(0x%016" PRIx64 ":%s) is SUSPEND, so could not change status.", status, STR_CHMPXSTS_FULL(status).c_str());
					return false;
				}else if(IS_CHMPXSTS_PENDING(status) || IS_CHMPXSTS_DOING(status)){
					WAN_CHMPRN("Server status(0x%016" PRIx64 ":%s) is DONE yet, so could not change status.", status, STR_CHMPXSTS_FULL(status).c_str());
					return false;
				}
				if(IS_CHMPXSTS_SUSPEND(status)){
					status	= CHMPXSTS_SRVOUT_UP_NORMAL;
					CHANGE_CHMPXSTS_TO_SUSPEND(status);
				}else{
					status	= CHMPXSTS_SRVOUT_UP_NORMAL;
				}
			}
		}else if(IS_CHMPXSTS_DOWN(status)){
			if(IS_CHMPXSTS_DELETE(status) && !IS_CHMPXSTS_DONE(status)){
				WAN_CHMPRN("Server status(0x%016" PRIx64 ":%s) is not DONE, so could not change status.", status, STR_CHMPXSTS_FULL(status).c_str());
				return false;
			}
			if(IS_CHMPXSTS_DELETE(status) && IS_CHMPXSTS_DONE(status)){
				status	= CHMPXSTS_SRVOUT_DOWN_NORMAL;
			}else{
				// Not care for SERVICE IN / DOWN / NOACT servers
			}
		}
	}else{
		// Not care for SERVICE OUT servers
	}
	MSG_CHMPRN("Server status(0x%016" PRIx64 ":%s) is changed from old status(0x%016" PRIx64 ":%s).", status, STR_CHMPXSTS_FULL(status).c_str(), old_status, STR_CHMPXSTS_FULL(old_status).c_str());

	// hash update
	if(!pImData->SetSelfBaseHash(hash)){
		ERR_CHMPRN("Failed to update base hash(0x%016" PRIx64 ").", hash);
		return false;
	}

	// set new status(set suspend in this method)
	if(!pImData->SetSelfStatus(status)){
		ERR_CHMPRN("Failed to change self status to 0x%016" PRIx64 ":%s", status, STR_CHMPXSTS_FULL(status).c_str());
		return false;
	}

	// Update self last status updating time for retrying
	if(!pImData->UpdateSelfLastStatusTime()){
		ERR_CHMPRN("Could not update own updating status time, but continue...");
	}

	// [NOTE]
	// If the server node deleted from Configuration has SERVICEOUT/DOWN status, 
	// call this method to delete it from the internal data.
	//
	if(!pImData->ReloadConfiguration()){
		WAN_CHMPRN("Failed to reload configuration after merging, but continue...");
	}

	// send status change
	CHMPXSVR	chmpxsvr;
	if(!pImData->GetSelfChmpxSvr(&chmpxsvr)){
		ERR_CHMPRN("Could not get self chmpx information.");
		return false;
	}
	chmpxid_t	nextchmpxid = GetNextRingChmpxId();
	if(CHM_INVALID_CHMPXID == nextchmpxid){
		WAN_CHMPRN("Could not get next chmpxid, probably there is no server without self chmpx on RING.");

		if(!PxComSendSlavesStatusChange(&chmpxsvr)){
			ERR_CHMPRN("Failed to send self status change to slaves.");
			return false;
		}
	}else{
		if(!PxComSendStatusChange(nextchmpxid, &chmpxsvr)){
			ERR_CHMPRN("Failed to send self status change.");
			return false;
		}
	}

	return true;
}

bool ChmEventSock::RequestMergeAbort(string* pstring)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		if(pstring){
			*pstring = CTL_RES_INT_ERROR;
		}
		return false;
	}
	//ChmIMData*	pImData = pChmCntrl->GetImDataObj();		not used

	// get terminated chmpxid
	chmpxid_t	chmpxid = GetNextRingChmpxId();
	if(CHM_INVALID_CHMPXID == chmpxid){
		// no server found, finish doing so pending hash updated self.
		WAN_CHMPRN("Could not get to chmpxid, probably there is no server without self chmpx on RING. So stop sending status update.");

		// abort merge
		//
		// If there is no server in RING, do abort merge.
		//
		if(!MergeAbort()){
			MSG_CHMPRN("Failed to abort merge, maybe status does not DOING nor DONE now.");
			if(pstring){
				*pstring = CTL_RES_ERROR_MERGE_ABORT;
			}
		}else{
			if(pstring){
				*pstring = CTL_RES_SUCCESS_NOSERVER;
			}
		}
		return true;
	}

	// send merge abort
	if(!PxComSendMergeAbort(chmpxid)){
		ERR_CHMPRN("Failed to send CHMPX_COM_MERGE_ABORT.");
		if(pstring){
			*pstring = CTL_RES_ERROR_COMMUNICATION;
		}
		return false;
	}
	if(pstring){
		*pstring = CTL_RES_SUCCESS;
	}
	return true;
}

bool ChmEventSock::DoSuspend(void)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	if(!pImData->IsChmpxProcess()){
		ERR_CHMPRN("This method must be called on Chmpx process.");
		return false;
	}

	// backup operation mode.
	bool	is_operating = pImData->IsOperating();

	// status check and set new status
	chmpxsts_t	status		= pImData->GetSelfStatus();
	chmpxsts_t	old_status	= status;
	if(IS_CHMPXSTS_SRVIN(status)){
		if(IS_CHMPXSTS_UP(status)){
			if(IS_CHMPXSTS_NOACT(status)){
				// [SERVICE IN] [UP] [NOACT] [*] [NOSUSPEND / SUSPEND]		--> [SERVICE IN] [UP] [NOACT] [NOTHING] [SUSPEND]
				SET_CHMPXSTS_NOTHING(status);
				CHANGE_CHMPXSTS_TO_SUSPEND(status);

			}else if(IS_CHMPXSTS_ADD(status)){
				// [SERVICE IN] [UP] [ADD] [*] [NOSUSPEND / SUSPEND]		--> [SERVICE IN] [UP] [ADD] [PENDING] [SUSPEND]
				SET_CHMPXSTS_PENDING(status);
				CHANGE_CHMPXSTS_TO_SUSPEND(status);

			}else if(IS_CHMPXSTS_DELETE(status)){
				// [SERVICE IN] [UP] [DELETE] [*] [NOSUSPEND / SUSPEND]		--> [SERVICE IN] [UP] [DELETE] [PENDING] [SUSPEND]
				SET_CHMPXSTS_PENDING(status);
				CHANGE_CHMPXSTS_TO_SUSPEND(status);
			}
		}else if(IS_CHMPXSTS_DOWN(status)){
			// [SERVICE IN] [DOWN] [*] [*] [NOSUSPEND]
			// Should be NOSUSPEND status for DOWN servers, then do not nothing.
			CHANGE_CHMPXSTS_TO_NOSUP(status);
		}
	}else if(IS_CHMPXSTS_SRVOUT(status)){
		if(IS_CHMPXSTS_UP(status)){
			// [SERVICE OUT] [UP] [NOACT] [NOTHING] [NOSUSPEND / SUSPEND]	--> [SERVICE OUT] [UP] [NOACT] [NOTHING] [SUSPEND]
			CHANGE_CHMPXSTS_TO_SUSPEND(status);
		}else if(IS_CHMPXSTS_DOWN(status)){
			// [SERVICE OUT] [DOWN] [NOACT] [NOTHING] [NOSUSPEND]
			// Should be NOSUSPEND status for DOWN servers, then do not nothing.
			CHANGE_CHMPXSTS_TO_NOSUP(status);
		}
	}
	if(status == old_status){
		MSG_CHMPRN("Server is already set status(0x%016" PRIx64 ":%s) by DoSuspend.", status, STR_CHMPXSTS_FULL(status).c_str());
		return true;
	}
	MSG_CHMPRN("Changed status(0x%016" PRIx64 ":%s) from old status(0x%016" PRIx64 ":%s).", status, STR_CHMPXSTS_FULL(status).c_str(), old_status, STR_CHMPXSTS_FULL(old_status).c_str());

	if(!pImData->SetSelfStatus(status)){
		ERR_CHMPRN("Failed to change server status(0x%016" PRIx64 ").", status);
		return false;
	}

	// Update pending hash.
	if(!pImData->UpdateHash(HASHTG_PENDING)){
		ERR_CHMPRN("Failed to update pending hash for all servers, so stop update status.");
		return false;
	}

	// send status update
	CHMPXSVR	chmpxsvr;
	if(!pImData->GetSelfChmpxSvr(&chmpxsvr)){
		ERR_CHMPRN("Could not get self chmpx information.");
		return false;
	}
	chmpxid_t	nextchmpxid = GetNextRingChmpxId();
	if(CHM_INVALID_CHMPXID == nextchmpxid){
		WAN_CHMPRN("Could not get next chmpxid, probably there is no server without self chmpx on RING.");
		if(!PxComSendSlavesStatusChange(&chmpxsvr)){
			ERR_CHMPRN("Failed to send self status change to slaves.");
			return false;
		}
	}else{
		if(!PxComSendStatusChange(nextchmpxid, &chmpxsvr)){
			ERR_CHMPRN("Failed to send self status change.");
			return false;
		}
	}

	// If merging, stop it.
	if(is_operating){
		if(!RequestMergeAbort()){
			ERR_CHMPRN("Failed stopping merging.");
			return false;
		}
	}
	return true;
}

//---------------------------------------------------------
// Methods for Ctlport Command
//---------------------------------------------------------
bool ChmEventSock::SendCtlPort(chmpxid_t chmpxid, const unsigned char* pbydata, size_t length, string& strResult)
{
	if(CHM_INVALID_CHMPXID == chmpxid || !pbydata || 0L == length){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	ChmIMData*		pImData = pChmCntrl->GetImDataObj();

	string			hostname;
	short			ctlport	= CHM_INVALID_PORT;
	hostport_list_t	ctlendpoints;
	if(!pImData->GetServerBase(chmpxid, &hostname, NULL, &ctlport, NULL, &ctlendpoints)){
		ERR_CHMPRN("Could not find server by chmpxid(0x%016" PRIx64 ").", chmpxid);
		return false;
	}
	if(!ctlendpoints.empty()){
		// If target has control endpoints, try it
		for(hostport_list_t::const_iterator iter = ctlendpoints.begin(); ctlendpoints.end() != iter; ++iter){
			if(ChmEventSock::RawSendCtlPort(iter->host.c_str(), iter->port, pbydata, length, strResult, sockfd_lockval, sock_retry_count, sock_wait_time, con_retry_count, con_wait_time)){
				// Succeed
				return true;
			}
			MSG_CHMPRN("Could not connect(send) to %s:%d which is one of control endpoints for %s:%d, try to connect another host/port.", iter->host.c_str(), iter->port, hostname.c_str(), ctlport);
		}
	}
	// Last try main host/port
	return ChmEventSock::RawSendCtlPort(hostname.c_str(), ctlport, pbydata, length, strResult, sockfd_lockval, sock_retry_count, sock_wait_time, con_retry_count, con_wait_time);
}

bool ChmEventSock::CtlComDump(string& strResponse)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	stringstream	sstream;
	pImData->Dump(sstream);
	strResponse = sstream.str();

	return true;
}

bool ChmEventSock::CtlComMergeStart(string& strResponse)
{
	return RequestMergeStart(&strResponse);
}

bool ChmEventSock::CtlComMergeAbort(string& strResponse)
{
	return RequestMergeAbort(&strResponse);
}

bool ChmEventSock::CtlComMergeComplete(string& strResponse)
{
	return RequestMergeComplete(&strResponse);
}

bool ChmEventSock::CtlComMergeSuspend(string& strResponse)
{
	return RequestMergeSuspend(&strResponse);
}

bool ChmEventSock::CtlComMergeNoSuspend(string& strResponse)
{
	return RequestMergeNoSuspend(&strResponse);
}

bool ChmEventSock::CtlComServiceIn(string& strResponse)
{
	if(!RequestServiceIn(&strResponse)){
		return false;
	}

	// If do not merge(almost random deliver mode), do merging, completing it.
	if(IsAutoMerge()){
		if(!RequestMergeStart(&strResponse)){
			ERR_CHMPRN("Failed to start merging or complete merging for \"service in\".");
			strResponse += "\n";
			strResponse += CTL_RES_ERROR_MERGE_AUTO;
			return false;
		}
	}
	if(0 == strResponse.length()){
		strResponse = CTL_RES_SUCCESS;
	}
	return true;
}

bool ChmEventSock::CtlComServiceOut(const char* hostname, short ctlport, const char* cuk, const char* ctlendpoints, const char* custom_seed, const char* pOrgCommand, string& strResponse)
{
	if(CHMEMPTYSTR(hostname) || CHM_INVALID_PORT == ctlport || CHMEMPTYSTR(pOrgCommand)){
		ERR_CHMPRN("Parameters are wrong.");
		strResponse = CTL_RES_ERROR_PARAMETER;
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		strResponse = CTL_RES_INT_ERROR;
		return false;
	}

	ChmIMData*	pImData		= pChmCntrl->GetImDataObj();
	chmpxid_t	chmpxid;
	if(CHM_INVALID_CHMPXID == (chmpxid = pImData->GetChmpxIdByToServerName(hostname, ctlport, cuk, ctlendpoints, custom_seed))){
		ERR_CHMPRN("Could not find a server as %s:%d.", hostname, ctlport);
		strResponse = CTL_RES_ERROR_NOT_FOUND_SVR;
		return false;
	}

	// Check target server status for transfer command.
	chmpxsts_t	status		= pImData->GetServerStatus(chmpxid);
	if(IS_CHMPXSTS_SLAVE(status)){
		ERR_CHMPRN("Could not operate to SERVICEOUT for chmpxid(0x%016" PRIx64 ") as %s:%d which is slave node.", chmpxid, hostname, ctlport);
		strResponse = CTL_RES_ERROR_COMMAND_ON_SERVER;
		return false;
	}

	// check target node is alive.
	chmpxid_t	selfchmpxid	= pImData->GetSelfChmpxId();
	if(selfchmpxid != chmpxid){
		// the target server is not down, then transfer command to target server on control port.
		if(!IS_CHMPXSTS_DOWN(status)){
			// can transfer command and do on server.
			if(!ChmEventSock::RawSendCtlPort(hostname, ctlport, reinterpret_cast<const unsigned char*>(pOrgCommand), strlen(pOrgCommand) + 1, strResponse, sockfd_lockval, sock_retry_count, sock_wait_time, con_retry_count, con_wait_time)){
				ERR_CHMPRN("Failed to transfer command to %s:%d", hostname, ctlport);
				strResponse = CTL_RES_ERROR_TRANSFER;
				return false;
			}
			MSG_CHMPRN("Success to transfer SERVICEOUT command to %s:%d", hostname, ctlport);
			return true;
		}
	}

	// Request Service out
	if(!RequestServiceOut(chmpxid, &strResponse)){
		ERR_CHMPRN("Could not operate to SERVICEOUT for chmpxid(0x%016" PRIx64 ") which is specified %s:%d.", chmpxid, hostname, ctlport);
		return false;
	}
	return true;
}

bool ChmEventSock::CtlComSelfStatus(string& strResponse)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		strResponse = CTL_RES_INT_ERROR;
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// get all information for self chmpx
	long		maxmqcnt	= pImData->GetMaxMQCount();
	long		maxqppxmq	= pImData->GetMaxQueuePerChmpxMQ();
	long		maxqpcmq	= pImData->GetMaxQueuePerClientMQ();
	bool		is_server	= pImData->IsServerMode();
	long		svrcnt		= pImData->GetServerCount();
	long		slvcnt		= pImData->GetSlaveCount();
	size_t		clntcnt		= ctlsockmap.count();
	CHMPXSVR	chmpxsvr;
	string		group;
	int			sock		= CHM_INVALID_SOCK;
	int			ctlsock		= CHM_INVALID_SOCK;
	CHMSTAT		server_stat;
	CHMSTAT		slave_stat;
	if(!pImData->GetGroup(group) || !pImData->GetSelfChmpxSvr(&chmpxsvr) || !pImData->GetSelfSocks(sock, ctlsock)){
		ERR_CHMPRN("Failed to get all information for self chmpx.");
		strResponse = CTL_RES_ERROR_GET_CHMPXSVR;
		return false;
	}
	if(!pImData->GetStat(&server_stat, &slave_stat)){
		ERR_CHMPRN("Failed to get all stat data.");
		strResponse = CTL_RES_ERROR_GET_STAT;
		return false;
	}

	// set output buffer
	stringstream	ss;
	ss << "Server Name               = "	<< chmpxsvr.name									<< endl;
	ss << "Internal ID               = 0x"	<< to_hexstring(chmpxsvr.chmpxid)					<< endl;
	ss << "RING Name                 = "	<< group											<< endl;
	ss << "Mode                      = "	<< (is_server ? "Server" : "Slave")					<< endl;
	ss << "Maximum MQ Count          = "	<< to_string(maxmqcnt)								<< endl;
	ss << "Maximum Queue / Chmpx MQ  = "	<< to_string(maxqppxmq)								<< endl;
	ss << "Maximum Queue / Client MQ = "	<< to_string(maxqpcmq)								<< endl;
	ss << "Status                    = {"														<< endl;
	ss << "    Server Status         = 0x"	<< to_hexstring(chmpxsvr.status)					<< "(" << STR_CHMPXSTS_FULL(chmpxsvr.status) << ")" << endl;
	ss << "    Last Update           = "	<< CvtUpdateTimeToString(chmpxsvr.last_status_time)	<< "(" << to_string(chmpxsvr.last_status_time) << ")" << endl;
	ss << "}"																					<< endl;
	ss << "Hash                      = {"														<< endl;
	ss << "    Enable Hash Value     = 0x"	<< to_hexstring(chmpxsvr.base_hash)					<< endl;
	ss << "    Pending Hash Value    = 0x"	<< to_hexstring(chmpxsvr.pending_hash)				<< endl;
	ss << "}"																					<< endl;
	ss << "Connection                = {"														<< endl;
	ss << "    Port(socket)          = "	<< to_string(chmpxsvr.port)		<< "("				<< (CHM_INVALID_SOCK == sock ? "n/a" : to_string(sock))			<< ")" << endl;
	ss << "    Control Port(socket)  = "	<< to_string(chmpxsvr.ctlport)	<< "("				<< (CHM_INVALID_SOCK == ctlsock ? "n/a" : to_string(ctlsock))	<< ")" << endl;
	ss << "    CUK                   = "	<< chmpxsvr.cuk										<< endl;
	ss << "    Custom ID Seed        = "	<< chmpxsvr.custom_seed								<< endl;
	ss << "    Endpoints             = "	<< get_hostport_pairs_string(chmpxsvr.endpoints, EXTERNAL_EP_MAX)		<< endl;
	ss << "    Control Endppoints    = "	<< get_hostport_pairs_string(chmpxsvr.ctlendpoints, EXTERNAL_EP_MAX)	<< endl;
	ss << "    Forward Peers         = "	<< get_hostport_pairs_string(chmpxsvr.forward_peers, FORWARD_PEER_MAX)	<< endl;
	ss << "    Reverse Peers         = "	<< get_hostport_pairs_string(chmpxsvr.reverse_peers, REVERSE_PEER_MAX)	<< endl;
	ss << "    SSL Connection        = "	<< (chmpxsvr.ssl.is_ssl ? "yes" : "no")				<< endl;
	if(chmpxsvr.ssl.is_ssl){
		ss << "    SSL Parameters        = {"													<< endl;
		ss << "        Verify Peer       = "	<< (chmpxsvr.ssl.verify_peer ? "yes" : "no")	<< endl;
		ss << "        CA path type      = "	<< (chmpxsvr.ssl.is_ca_file ? "file" : "dir")	<< endl;
		ss << "        CA path           = "	<< chmpxsvr.ssl.capath							<< endl;
		ss << "        Server Cert       = "	<< chmpxsvr.ssl.server_cert						<< endl;
		ss << "        Server Private Key= "	<< chmpxsvr.ssl.server_prikey					<< endl;
		ss << "        Slave Cert        = "	<< chmpxsvr.ssl.slave_cert						<< endl;
		ss << "        Slave Private Key = "	<< chmpxsvr.ssl.slave_prikey					<< endl;
		ss << "        Slave Private Key = "	<< chmpxsvr.ssl.slave_prikey					<< endl;
		ss << "    }"																			<< endl;
	}
	ss << "}"																					<< endl;
	ss << "Connection Count          = {"														<< endl;
	ss << "    To Servers            = "	<< to_string(svrcnt)								<< endl;
	ss << "    From Slaves           = "	<< to_string(slvcnt)								<< endl;
	ss << "    On Control port       = "	<< to_string(clntcnt)								<< endl;
	ss << "}"																					<< endl;
	ss << "Stats                     = {"														<< endl;
	ss << "    To(From) Servers      = {"														<< endl;
	ss << "        send count        = "	<< to_string(server_stat.total_sent_count)			<< " count"	<< endl;
	ss << "        receive count     = "	<< to_string(server_stat.total_received_count)		<< " count"	<< endl;
	ss << "        total             = "	<< to_string(server_stat.total_body_bytes)			<< " bytes"	<< endl;
	ss << "        minimum           = "	<< to_string(server_stat.min_body_bytes)			<< " bytes"	<< endl;
	ss << "        maximum           = "	<< to_string(server_stat.max_body_bytes)			<< " bytes"	<< endl;
	ss << "        total             = "	<< to_string(server_stat.total_elapsed_time.tv_sec)	<< "s "	<< to_string(server_stat.total_elapsed_time.tv_nsec)<< "ns"	<< endl;
	ss << "        minimum           = "	<< to_string(server_stat.min_elapsed_time.tv_sec)	<< "s "	<< to_string(server_stat.min_elapsed_time.tv_nsec)	<< "ns"	<< endl;
	ss << "        maximum           = "	<< to_string(server_stat.max_elapsed_time.tv_sec)	<< "s "	<< to_string(server_stat.max_elapsed_time.tv_nsec)	<< "ns"	<< endl;
	ss << "    }"																				<< endl;
	ss << "    To(From) Slaves       = {"														<< endl;
	ss << "        send count        = "	<< to_string(slave_stat.total_sent_count)			<< " count"	<< endl;
	ss << "        receive count     = "	<< to_string(slave_stat.total_received_count)		<< " count"	<< endl;
	ss << "        total             = "	<< to_string(slave_stat.total_body_bytes)			<< " bytes"	<< endl;
	ss << "        minimum           = "	<< to_string(slave_stat.min_body_bytes)				<< " bytes"	<< endl;
	ss << "        maximum           = "	<< to_string(slave_stat.max_body_bytes)				<< " bytes"	<< endl;
	ss << "        total             = "	<< to_string(slave_stat.total_elapsed_time.tv_sec)	<< "s "	<< to_string(slave_stat.total_elapsed_time.tv_nsec)	<< "ns"	<< endl;
	ss << "        minimum           = "	<< to_string(slave_stat.min_elapsed_time.tv_sec)	<< "s "	<< to_string(slave_stat.min_elapsed_time.tv_nsec)	<< "ns"	<< endl;
	ss << "        maximum           = "	<< to_string(slave_stat.max_elapsed_time.tv_sec)	<< "s "	<< to_string(slave_stat.max_elapsed_time.tv_nsec)	<< "ns"	<< endl;
	ss << "    }"																				<< endl;
	ss << "}"																					<< endl;

	strResponse = ss.str();

	return true;
}

bool ChmEventSock::CtlComAllServerStatus(string& strResponse)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		strResponse = CTL_RES_INT_ERROR;
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// get information for all servers
	PCHMPXSVR	pchmpxsvrs	= NULL;
	long		count		= 0L;
	string		group;
	if(!pImData->GetGroup(group) || !pImData->GetChmpxSvrs(&pchmpxsvrs, count) || !pchmpxsvrs){
		ERR_CHMPRN("Failed to get information for all server.");
		strResponse = CTL_RES_ERROR_GET_CHMPXSVR;
		return false;
	}

	// set output buffer
	stringstream	ss;
	ss << "RING Name              = "	<< group											<< endl;

	for(long cnt = 0; cnt < count; cnt++){
		ss << "No."							<< to_string(cnt + 1)							<< endl;
		ss << "  Server Name          = "	<< pchmpxsvrs[cnt].name							<< endl;
		ss << "    Port               = "	<< to_string(pchmpxsvrs[cnt].port)				<< endl;
		ss << "    Control Port       = "	<< to_string(pchmpxsvrs[cnt].ctlport)			<< endl;
		ss << "    CUK                = "	<< pchmpxsvrs[cnt].cuk							<< endl;
		ss << "    Custom ID Seed     = "	<< pchmpxsvrs[cnt].custom_seed					<< endl;
		ss << "    Endpoints          = "	<< get_hostport_pairs_string(pchmpxsvrs[cnt].endpoints, EXTERNAL_EP_MAX)		<< endl;
		ss << "    Control Endppoints = "	<< get_hostport_pairs_string(pchmpxsvrs[cnt].ctlendpoints, EXTERNAL_EP_MAX)		<< endl;
		ss << "    Forward Peers      = "	<< get_hostport_pairs_string(pchmpxsvrs[cnt].forward_peers, FORWARD_PEER_MAX)	<< endl;
		ss << "    Reverse Peers      = "	<< get_hostport_pairs_string(pchmpxsvrs[cnt].reverse_peers, REVERSE_PEER_MAX)	<< endl;
		ss << "    Use SSL            = "	<< (pchmpxsvrs[cnt].ssl.is_ssl ? "yes" : "no")	<< endl;
		if(pchmpxsvrs[cnt].ssl.is_ssl){
			ss << "    Verify Peer        = "	<< (pchmpxsvrs[cnt].ssl.verify_peer ? "yes" : "no")	<< endl;
			ss << "    CA path type       = "	<< (pchmpxsvrs[cnt].ssl.is_ca_file ? "file" : "dir")<< endl;
			ss << "    CA path            = "	<< pchmpxsvrs[cnt].ssl.capath						<< endl;
			ss << "    Server Cert        = "	<< pchmpxsvrs[cnt].ssl.server_cert					<< endl;
			ss << "    Server Private Key = "	<< pchmpxsvrs[cnt].ssl.server_prikey				<< endl;
			ss << "    Slave Cert         = "	<< pchmpxsvrs[cnt].ssl.slave_cert					<< endl;
			ss << "    Slave Private Key  = "	<< pchmpxsvrs[cnt].ssl.slave_prikey					<< endl;
		}
		ss << "    Server Status      = 0x"	<< to_hexstring(pchmpxsvrs[cnt].status)						<< "(" << STR_CHMPXSTS_FULL(pchmpxsvrs[cnt].status) << ")" << endl;
		ss << "    Last Update        = "	<< CvtUpdateTimeToString(pchmpxsvrs[cnt].last_status_time)	<< "(" << to_string(pchmpxsvrs[cnt].last_status_time) << ")" << endl;
		ss << "    Enable Hash Value  = 0x"	<< to_hexstring(pchmpxsvrs[cnt].base_hash)					<< endl;
		ss << "    Pending Hash Value = 0x"	<< to_hexstring(pchmpxsvrs[cnt].pending_hash)				<< endl;
	}
	CHM_Free(pchmpxsvrs);
	strResponse = ss.str();

	return true;
}

bool ChmEventSock::CtlComUpdateStatus(string& strResponse)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		strResponse = CTL_RES_INT_ERROR;
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	chmpxsts_t	status	= pImData->GetSelfStatus();
	if(IS_CHMPXSTS_SLAVE(status)){
		ERR_CHMPRN("Could not operate to UPDATESTATUS on slave node.");
		strResponse = CTL_RES_ERROR_COMMAND_ON_SERVER;
		return false;
	}

	chmpxid_t	nextchmpxid	= GetNextRingChmpxId();
	if(CHM_INVALID_CHMPXID == nextchmpxid){
		ERR_CHMPRN("Could not operate to UPDATESTATUS, because there is no server node in RING.");
		strResponse = CTL_RES_SUCCESS_NOSERVER;
		return false;
	}

	// Update self last status updating time
	if(!pImData->UpdateSelfLastStatusTime()){
		ERR_CHMPRN("Could not update own updating status time, but continue...");
	}
	CHMPXSVR	chmpxsvr;
	if(!pImData->GetSelfChmpxSvr(&chmpxsvr)){
		ERR_CHMPRN("Could not get self chmpx information.");
		strResponse = CTL_RES_ERROR_UPDATE_STATUS_INTERR;
		return false;
	}
	if(!PxComSendStatusChange(nextchmpxid, &chmpxsvr)){
		ERR_CHMPRN("Failed to send self status change.");
		strResponse = CTL_RES_ERROR_UPDATE_STATUS_SENDERR;
		return false;
	}
	strResponse = CTL_RES_SUCCESS;

	return true;
}

bool ChmEventSock::CtlComAllTraceSet(string& strResponse, bool enable)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		strResponse = CTL_RES_ERROR;
		return false;
	}
	ChmIMData*	pImData		= pChmCntrl->GetImDataObj();
	bool		now_enable	= pImData->IsTraceEnable();

	if(enable == now_enable){
		ERR_CHMPRN("Already trace is %s.", (enable ? "enabled" : "disabled"));
		strResponse = CTL_RES_ERROR_TRACE_SET_ALREADY;
		return false;
	}

	bool	result;
	if(enable){
		result = pImData->EnableTrace();
	}else{
		result = pImData->DisableTrace();
	}
	if(!result){
		ERR_CHMPRN("Something error is occured in setting dis/enable TRACE.");
		strResponse = CTL_RES_ERROR_TRACE_SET_FAILED;
		return false;
	}

	strResponse = CTL_RES_SUCCESS;
	return true;
}

bool ChmEventSock::CtlComAllTraceView(string& strResponse, logtype_t dirmask, logtype_t devmask, long count)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		strResponse = CTL_RES_ERROR;
		return false;
	}
	ChmIMData*	pImData	= pChmCntrl->GetImDataObj();
	long		trcount	= pImData->GetTraceCount();

	if(!IS_SAFE_CHMLOG_MASK(dirmask) || !IS_SAFE_CHMLOG_MASK(devmask)){
		ERR_CHMPRN("dirmask(0x%016" PRIx64 ") or devmask(0x%016" PRIx64 ") are wrong.", dirmask, devmask);
		strResponse = CTL_RES_ERROR_TRACE_VIEW_INTERR;
		return false;
	}
	if(count <= 0 || trcount < count){
		WAN_CHMPRN("TRACEVIEW count is wrong, so set all.");
		count = trcount;
	}

	// get TRACE log.
	PCHMLOGRAW	plograwarr;
	if(NULL == (plograwarr = reinterpret_cast<PCHMLOGRAW>(malloc(sizeof(CHMLOGRAW) * count)))){
		ERR_CHMPRN("Could not allocate memory.");
		strResponse = CTL_RES_ERROR_TRACE_VIEW_INTERR;
		return false;
	}
	if(!pImData->GetTrace(plograwarr, count, dirmask, devmask)){
		ERR_CHMPRN("Failed to get trace log.");
		strResponse = CTL_RES_ERROR;
		CHM_Free(plograwarr);
		return false;
	}

	// make result string
	if(0 == count){
		MSG_CHMPRN("There is no trace log.");
		strResponse = CTL_RES_ERROR_TRACE_VIEW_NODATA;
	}else{
		for(long cnt = 0; cnt < count; ++cnt){
			strResponse += STR_CHMLOG_TYPE(plograwarr[cnt].log_type);
			strResponse += "\t";

			strResponse += to_string(plograwarr[cnt].length);
			strResponse += " byte\t";

			strResponse += "START(";
			strResponse += to_string(plograwarr[cnt].start_time.tv_sec);
			strResponse += "s ";
			strResponse += to_string(plograwarr[cnt].start_time.tv_nsec);
			strResponse += "ns) - ";

			strResponse += "FINISH(";
			strResponse += to_string(plograwarr[cnt].fin_time.tv_sec);
			strResponse += "s ";
			strResponse += to_string(plograwarr[cnt].fin_time.tv_nsec);
			strResponse += "ns)\n";
		}
	}
	CHM_Free(plograwarr);
	return true;
}

//---------------------------------------------------------
// Methods for PX2PX Command
//---------------------------------------------------------
//
// PxComSendStatusReq uses sock directly.
//
// [NOTE]
// If you need to lock the socket, you must lock it before calling this method.
//
bool ChmEventSock::PxComSendStatusReq(int sock, chmpxid_t chmpxid, bool need_sock_close)
{
	if(CHM_INVALID_SOCK == sock || CHM_INVALID_CHMPXID == chmpxid){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData		= pChmCntrl->GetImDataObj();

	// Make packet
	PCOMPKT	pComPkt;
	if(IS_COM_CURRENT_VERSION(comproto_ver)){					// check communication protocol version(backward compatibility)
		// current version
		if(NULL == (pComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_N2_STATUS_REQ))){
			ERR_CHMPRN("Could not allocate memory for COMPKT.");
			return false;
		}

		// compkt
		PPXCOM_ALL				pComAll		= CVT_COM_ALL_PTR_PXCOMPKT(pComPkt);
		PPXCOM_N2_STATUS_REQ	pStatusReq	= CVT_COMPTR_N2_STATUS_REQ(pComAll);

		// cppcheck-suppress unmatchedSuppression
		// cppcheck-suppress internalAstError
		SET_PXCOMPKT(pComPkt, COM_VERSION_2, CHMPX_COM_N2_STATUS_REQ, pImData->GetSelfChmpxId(), chmpxid, true, 0L);

		pStatusReq->head.type	= CHMPX_COM_N2_STATUS_REQ;
		pStatusReq->head.result	= CHMPX_COM_RES_SUCCESS;
		pStatusReq->head.length	= sizeof(PXCOM_N2_STATUS_REQ);
	}else{
		// old version
		if(NULL == (pComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_STATUS_REQ))){
			ERR_CHMPRN("Could not allocate memory for COMPKT.");
			return false;
		}

		// compkt
		PPXCOM_ALL			pComAll		= CVT_COM_ALL_PTR_PXCOMPKT(pComPkt);
		PPXCOM_STATUS_REQ	pStatusReq	= CVT_COMPTR_STATUS_REQ(pComAll);
		SET_PXCOMPKT(pComPkt, COM_VERSION_1, CHMPX_COM_STATUS_REQ, pImData->GetSelfChmpxId(), chmpxid, true, 0L);

		pStatusReq->head.type	= CHMPX_COM_STATUS_REQ;
		pStatusReq->head.result	= CHMPX_COM_RES_SUCCESS;
		pStatusReq->head.length	= sizeof(PXCOM_STATUS_REQ);
	}

	// Send request
	//
	// [NOTE]
	// Should lock the socket in caller if you need.
	//
	bool	is_closed = false;
	if(!ChmEventSock::RawSend(sock, GetSSL(sock), pComPkt, is_closed, false, sock_retry_count, sock_wait_time)){		// as default nonblocking
		ERR_CHMPRN("Failed to send %s to sock(%d).", (IS_COM_CURRENT_VERSION(comproto_ver) ? "CHMPX_COM_N2_STATUS_REQ" : "CHMPX_COM_STATUS_REQ"), sock);

		// [NOTE]
		// When this method is called from InitialAllServerStatus(), the sock is not mapped.
		// Then we close sock when need_sock_close is true.
		//
		if(need_sock_close && is_closed){
			if(!RawNotifyHup(sock)){
				ERR_CHMPRN("Failed to closing socket(%d), but continue...", sock);
			}
		}
		CHM_Free(pComPkt);
		return false;
	}
	CHM_Free(pComPkt);

	return true;
}

bool ChmEventSock::PxComReceiveStatusReq(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt)
{
	if(!pComHead || !pComAll || !ppResComPkt){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*				pImData		= pChmCntrl->GetImDataObj();
	//PPXCOM_STATUS_REQ		pStatusReq	= CVT_COMPTR_STATUS_REQ(pComAll);		// unused now
	//PPXCOM_N2_STATUS_REQ	pStatusReq	= CVT_COMPTR_N2_STATUS_REQ(pComAll);	// unused now
	*ppResComPkt						= NULL;

	// Update self last status updating time 
	if(!pImData->UpdateSelfLastStatusTime()){
		ERR_CHMPRN("Could not update own updating status time, but continue...");
	}

	// get all server status
	PCHMPXSVR	pchmpxsvrs	= NULL;
	long		count		= 0L;
	pxcomres_t	result		= CHMPX_COM_RES_SUCCESS;
	if(!pImData->GetChmpxSvrs(&pchmpxsvrs, count)){
		ERR_CHMPRN("Could not get all server status, but continue to response a error.");
		pchmpxsvrs	= NULL;
		count		= 0L;
		result		= CHMPX_COM_RES_ERROR;
	}

	// make response data
	PCOMPKT	pResComPkt;
	if(IS_COM_CURRENT_VERSION(pComHead->version)){				// check communication protocol version(backward compatibility)
		// current version
		if(NULL == (pResComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_N2_STATUS_RES, sizeof(CHMPXSVR) * count))){
			ERR_CHMPRN("Could not allocation memory.");
			CHM_Free(pchmpxsvrs);
			return false;
		}

		// compkt
		PPXCOM_ALL				pResComAll	= CVT_COM_ALL_PTR_PXCOMPKT(pResComPkt);
		PPXCOM_N2_STATUS_RES	pStatusRes	= CVT_COMPTR_N2_STATUS_RES(pResComAll);
		SET_PXCOMPKT(pResComPkt, COM_VERSION_2, CHMPX_COM_N2_STATUS_RES, pComHead->term_ids.chmpxid, pComHead->dept_ids.chmpxid, true, (sizeof(CHMPXSVR) * count));	// switch chmpxid

		pStatusRes->head.type			= CHMPX_COM_N2_STATUS_RES;
		pStatusRes->head.result			= result;
		pStatusRes->head.length			= sizeof(PXCOM_N2_STATUS_RES) + (sizeof(CHMPXSVR) * count);
		pStatusRes->count				= count;
		pStatusRes->pchmpxsvr_offset	= sizeof(PXCOM_N2_STATUS_RES);

		PCHMPXSVR	pchmpxsvrsv2		= CHM_OFFSET(pStatusRes, sizeof(PXCOM_N2_STATUS_RES), PCHMPXSVR);
		if(pchmpxsvrs && pchmpxsvrsv2 && 0 < count){
			for(long pos = 0; pos < count; ++pos){
				COPY_PCHMPXSVR(&pchmpxsvrsv2[pos], &pchmpxsvrs[pos]);
			}
		}
	}else{
		// old version
		if(NULL == (pResComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_STATUS_RES, sizeof(CHMPXSVRV1) * count))){
			ERR_CHMPRN("Could not allocation memory.");
			CHM_Free(pchmpxsvrs);
			return false;
		}

		// compkt
		PPXCOM_ALL			pResComAll	= CVT_COM_ALL_PTR_PXCOMPKT(pResComPkt);
		PPXCOM_STATUS_RES	pStatusRes	= CVT_COMPTR_STATUS_RES(pResComAll);
		SET_PXCOMPKT(pResComPkt, COM_VERSION_1, CHMPX_COM_STATUS_RES, pComHead->term_ids.chmpxid, pComHead->dept_ids.chmpxid, true, (sizeof(CHMPXSVRV1) * count));	// switch chmpxid

		pStatusRes->head.type			= CHMPX_COM_STATUS_RES;
		pStatusRes->head.result			= result;
		pStatusRes->head.length			= sizeof(PXCOM_STATUS_RES) + (sizeof(CHMPXSVRV1) * count);
		pStatusRes->count				= count;
		pStatusRes->pchmpxsvr_offset	= sizeof(PXCOM_STATUS_RES);

		PCHMPXSVRV1	pchmpxsvrsv1		= CHM_OFFSET(pStatusRes, sizeof(PXCOM_STATUS_RES), PCHMPXSVRV1);
		if(pchmpxsvrs && pchmpxsvrsv1 && 0 < count){
			for(long pos = 0; pos < count; ++pos){
				CVT_PCHMPXSVR_TO_PCHMPXSVRV1(&pchmpxsvrsv1[pos], &pchmpxsvrs[pos]);
			}
		}
	}
	CHM_Free(pchmpxsvrs);
	*ppResComPkt = pResComPkt;

	return true;
}

bool ChmEventSock::PxComReceiveStatusRes(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt, bool is_init_process)
{
	if(!pComHead || !pComAll || !ppResComPkt){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*		pImData		= pChmCntrl->GetImDataObj();
	chmpxid_t		selfchmpxid	= pImData->GetSelfChmpxId();
	*ppResComPkt				= NULL;

	if(pComHead->term_ids.chmpxid == selfchmpxid){
		// To me
		PPXCOM_HEAD	pPxComHead = CVT_COMPTR_HEAD(pComAll);

		if(CHMPX_COM_RES_SUCCESS != pPxComHead->result){
			// Something error occured by status response
			ERR_CHMPRN("PXCOM_N2_STATUS_RES(or PXCOM_STATUS_RES) is failed.");
			return false;
		}

		// Succeed status response
		PCHMPXSVR	pchmpxsvrs;
		PCHMPXSVR	pchmpxsvrs_tmp = NULL;
		long		count;
		if(IS_COM_CURRENT_VERSION(pComHead->version)){				// check communication protocol version(backward compatibility)
			// current version
			PPXCOM_N2_STATUS_RES	pStatusRes	= CVT_COMPTR_N2_STATUS_RES(pComAll);
			pchmpxsvrs							= CHM_OFFSET(pStatusRes, pStatusRes->pchmpxsvr_offset, PCHMPXSVR);
			if(!pchmpxsvrs){
				ERR_CHMPRN("There is no CHMPXSVR data in received PXCOM_N2_STATUS_RES.");
				return false;
			}
			count = pStatusRes->count;
		}else{
			// old version
			PPXCOM_STATUS_RES		pStatusRes	= CVT_COMPTR_STATUS_RES(pComAll);
			PCHMPXSVRV1				pchmpxsvrsv1= CHM_OFFSET(pStatusRes, pStatusRes->pchmpxsvr_offset, PCHMPXSVRV1);
			if(!pchmpxsvrsv1){
				ERR_CHMPRN("There is no CHMPXSVRV1 data in received PXCOM_STATUS_RES.");
				return false;
			}
			count = pStatusRes->count;

			if(0 < count){
				// allocation for CHMPXSVR(current)
				if(NULL == (pchmpxsvrs_tmp = reinterpret_cast<PCHMPXSVR>(calloc(count, sizeof(CHMPXSVR))))){
					ERR_CHMPRN("Could not allocation memory.");
					return false;
				}
				// convert to CHMPXSVR(current)
				for(long pos = 0; pos < count; ++pos){
					CVT_PCHMPXSVRV1_TO_PCHMPXSVR(&pchmpxsvrs_tmp[pos], &pchmpxsvrsv1[pos]);
				}
				// set pointer
				pchmpxsvrs = pchmpxsvrs_tmp;
			}
		}

		// bup status
		chmpxsts_t	bupstatus = pImData->GetSelfStatus();

		// Merge all status
		if(0 < count){
			if(!pImData->MergeChmpxSvrs(pchmpxsvrs, count, true, is_init_process, eqfd)){	// remove server chmpx data if there is not in list
				ERR_CHMPRN("Failed to merge server CHMPXLIST from CHMPXSVR list.");
				CHM_Free(pchmpxsvrs_tmp);
				return false;
			}
		}
		CHM_Free(pchmpxsvrs_tmp);

		// If server mode, update hash values
		if(is_server_mode){
			// check self status changing( normal -> merging )
			//
			// [NOTICE]
			// MergeChmpxSvrs() can change self status, then we put messaging.
			//
			chmpxsts_t	newstatus = pImData->GetSelfStatus();
			if(bupstatus != newstatus){
				WAN_CHMPRN("self status changed from 0x%016" PRIx64 ":%s to 0x%016" PRIx64 ":%s.", bupstatus, STR_CHMPXSTS_FULL(bupstatus).c_str(), newstatus, STR_CHMPXSTS_FULL(newstatus).c_str());
			}
		}
	}else{
		// To other chmpxid
		ERR_CHMPRN("Received PXCOM_N2_STATUS_RES(or PXCOM_STATUS_RES) packet, but terminal chmpxid(0x%016" PRIx64 ") is not self chmpxid(0x%016" PRIx64 ").", pComHead->term_ids.chmpxid, selfchmpxid);
		return false;
	}
	return true;
}

//
// PxComSendConinitReq uses sock directly.
//
// [NOTE]
// PPXCOM_CONINIT_REQ does not select socket.(Do not use GetLockedSendSock method).
//
bool ChmEventSock::PxComSendConinitReq(int sock, chmpxid_t chmpxid)
{
	if(CHM_INVALID_SOCK == sock || CHM_INVALID_CHMPXID == chmpxid){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// datas
	chmpxid_t		selfchmpxid = pImData->GetSelfChmpxId();
	string			name;
	short			ctlport		= CHM_INVALID_PORT;
	string			cuk;
	string			custom_seed;
	hostport_list_t	endpoints;
	hostport_list_t	ctlendpoints;
	hostport_list_t	forward_peers;
	hostport_list_t	reverse_peers;
	if(!pImData->GetSelfBase(&name, NULL, &ctlport, &cuk, &custom_seed, &endpoints, &ctlendpoints, &forward_peers, &reverse_peers)){
		ERR_CHMPRN("Could not get self chmpx information.");
		return false;
	}

	// Make packet
	PCOMPKT	pComPkt;
	if(IS_COM_CURRENT_VERSION(comproto_ver)){					// check communication protocol version(backward compatibility)
		// current version
		if(NULL == (pComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_N2_CONINIT_REQ))){
			ERR_CHMPRN("Could not allocate memory for COMPKT.");
			return false;
		}

		PPXCOM_ALL				pComAll		= CVT_COM_ALL_PTR_PXCOMPKT(pComPkt);
		PPXCOM_N2_CONINIT_REQ	pConinitReq	= CVT_COMPTR_N2_CONINIT_REQ(pComAll);
		SET_PXCOMPKT(pComPkt, COM_VERSION_2, CHMPX_COM_N2_CONINIT_REQ, selfchmpxid, chmpxid, true, 0L);

		pConinitReq->head.type		= CHMPX_COM_N2_CONINIT_REQ;
		pConinitReq->head.result	= CHMPX_COM_RES_SUCCESS;
		pConinitReq->head.length	= sizeof(PXCOM_N2_CONINIT_REQ);
		pConinitReq->chmpxid		= selfchmpxid;
		pConinitReq->ctlport		= ctlport;

		memset(pConinitReq->name,			0,					NI_MAXHOST);
		strncpy(pConinitReq->name,			name.c_str(),		NI_MAXHOST - 1);
		memset(pConinitReq->cuk,			0,					CUK_MAX);
		strncpy(pConinitReq->cuk,			cuk.c_str(),		CUK_MAX - 1);
		memset(pConinitReq->custom_seed,	0,					CUSTOM_ID_SEED_MAX);
		strncpy(pConinitReq->custom_seed,	custom_seed.c_str(),CUSTOM_ID_SEED_MAX - 1);

		cvt_hostport_pairs(pConinitReq->endpoints,		endpoints,		EXTERNAL_EP_MAX);
		cvt_hostport_pairs(pConinitReq->ctlendpoints,	ctlendpoints,	EXTERNAL_EP_MAX);
		cvt_hostport_pairs(pConinitReq->forward_peers,	forward_peers,	FORWARD_PEER_MAX);
		cvt_hostport_pairs(pConinitReq->reverse_peers,	reverse_peers,	REVERSE_PEER_MAX);

	}else{
		// old version
		if(NULL == (pComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_CONINIT_REQ))){
			ERR_CHMPRN("Could not allocate memory for COMPKT.");
			return false;
		}

		PPXCOM_ALL			pComAll		= CVT_COM_ALL_PTR_PXCOMPKT(pComPkt);
		PPXCOM_CONINIT_REQ	pConinitReq	= CVT_COMPTR_CONINIT_REQ(pComAll);
		SET_PXCOMPKT(pComPkt, COM_VERSION_1, CHMPX_COM_CONINIT_REQ, selfchmpxid, chmpxid, true, 0L);

		pConinitReq->head.type		= CHMPX_COM_CONINIT_REQ;
		pConinitReq->head.result	= CHMPX_COM_RES_SUCCESS;
		pConinitReq->head.length	= sizeof(PXCOM_CONINIT_REQ);
		pConinitReq->chmpxid		= selfchmpxid;
		pConinitReq->ctlport		= ctlport;
	}

	// Send request
	if(!ChmEventSock::LockedSend(sock, GetSSL(sock), pComPkt)){		// as default nonblocking
		ERR_CHMPRN("Failed to send CHMPX_COM_CONINIT_REQ to sock(%d).", sock);
		CHM_Free(pComPkt);
		return false;
	}
	CHM_Free(pComPkt);

	return true;
}

bool ChmEventSock::PxComReceiveConinitReq(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt, comver_t& comver, string& name, chmpxid_t& from_chmpxid, short& ctlport, string& cuk, string& custom_seed, hostport_list_t& endpoints, hostport_list_t& ctlendpoints, hostport_list_t& forward_peers, hostport_list_t& reverse_peers)
{
	comver = COM_VERSION_2;

	if(!pComHead || !pComAll || !ppResComPkt){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*			pImData		= pChmCntrl->GetImDataObj();
	chmpxid_t			selfchmpxid	= pImData->GetSelfChmpxId();
	*ppResComPkt					= NULL;

	if(pComHead->term_ids.chmpxid == selfchmpxid){
		// To me
		PPXCOM_HEAD	pPxComHead = CVT_COMPTR_HEAD(pComAll);

		if(CHMPX_COM_RES_SUCCESS != pPxComHead->result){
			// Something error occured by status response
			ERR_CHMPRN("PPXCOM_N2_CONINIT_REQ(or PPXCOM_CONINIT_REQ) is failed.");
			return false;
		}

		// Succeed response
		if(IS_COM_CURRENT_VERSION(pComHead->version)){				// check communication protocol version(backward compatibility)
			// current version
			comver = COM_VERSION_2;

			PPXCOM_N2_CONINIT_REQ	pConinitReq	= CVT_COMPTR_N2_CONINIT_REQ(pComAll);

			name		= pConinitReq->name;
			from_chmpxid= pConinitReq->chmpxid;
			ctlport		= pConinitReq->ctlport;
			cuk			= pConinitReq->cuk;
			custom_seed	= pConinitReq->custom_seed;

			rev_hostport_pairs(endpoints,		pConinitReq->endpoints,		EXTERNAL_EP_MAX);
			rev_hostport_pairs(ctlendpoints,	pConinitReq->ctlendpoints,	EXTERNAL_EP_MAX);
			rev_hostport_pairs(forward_peers,	pConinitReq->forward_peers,	FORWARD_PEER_MAX);
			rev_hostport_pairs(reverse_peers,	pConinitReq->reverse_peers,	REVERSE_PEER_MAX);

		}else{
			// old version
			comver = pComHead->version;

			PPXCOM_CONINIT_REQ	pConinitReq	= CVT_COMPTR_CONINIT_REQ(pComAll);

			name.clear();
			from_chmpxid= pConinitReq->chmpxid;
			ctlport		= pConinitReq->ctlport;
			cuk.clear();
			custom_seed.clear();
			endpoints.clear();
			ctlendpoints.clear();
			forward_peers.clear();
			reverse_peers.clear();
		}
	}else{
		// To other chmpxid
		ERR_CHMPRN("Received PPXCOM_N2_CONINIT_REQ(or PPXCOM_CONINIT_REQ) packet, but terminal chmpxid(0x%016" PRIx64 ") is not self chmpxid(0x%016" PRIx64 ").", pComHead->term_ids.chmpxid, selfchmpxid);
		return false;
	}
	return true;
}

//
// PxComSendConinitRes uses sock directly.
//
// [NOTE]
// PPXCOM_CONINIT_RES does not select socket.(Do not use GetLockedSendSock method).
//
bool ChmEventSock::PxComSendConinitRes(int sock, chmpxid_t chmpxid, comver_t comver, pxcomres_t result)
{
	if(CHM_INVALID_SOCK == sock || CHM_INVALID_CHMPXID == chmpxid){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// Make packet
	PCOMPKT	pComPkt;
	if(IS_COM_CURRENT_VERSION(comver)){							// check communication protocol version(backward compatibility)
		// current version
		if(NULL == (pComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_N2_CONINIT_RES))){
			ERR_CHMPRN("Could not allocate memory for COMPKT.");
			return false;
		}

		// compkt
		PPXCOM_ALL			pComAll			= CVT_COM_ALL_PTR_PXCOMPKT(pComPkt);
		PPXCOM_N2_CONINIT_RES	pConinitRes	= CVT_COMPTR_N2_CONINIT_RES(pComAll);
		SET_PXCOMPKT(pComPkt, COM_VERSION_2, CHMPX_COM_N2_CONINIT_RES, pImData->GetSelfChmpxId(), chmpxid, true, 0L);

		pConinitRes->head.type		= CHMPX_COM_N2_CONINIT_RES;
		pConinitRes->head.result	= result;
		pConinitRes->head.length	= sizeof(PXCOM_N2_CONINIT_RES);

	}else{
		// old version
		if(NULL == (pComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_CONINIT_RES))){
			ERR_CHMPRN("Could not allocate memory for COMPKT.");
			return false;
		}

		// compkt
		PPXCOM_ALL			pComAll		= CVT_COM_ALL_PTR_PXCOMPKT(pComPkt);
		PPXCOM_CONINIT_RES	pConinitRes	= CVT_COMPTR_CONINIT_RES(pComAll);
		SET_PXCOMPKT(pComPkt, COM_VERSION_1, CHMPX_COM_CONINIT_RES, pImData->GetSelfChmpxId(), chmpxid, true, 0L);

		pConinitRes->head.type		= CHMPX_COM_CONINIT_RES;
		pConinitRes->head.result	= result;
		pConinitRes->head.length	= sizeof(PXCOM_CONINIT_RES);
	}

	// Send request
	if(!ChmEventSock::LockedSend(sock, GetSSL(sock), pComPkt)){		// as default nonblocking
		ERR_CHMPRN("Failed to send CHMPX_COM_N2_CONINIT_RES(or CHMPX_COM_CONINIT_RES) to sock(%d).", sock);
		CHM_Free(pComPkt);
		return false;
	}
	CHM_Free(pComPkt);

	return true;
}

bool ChmEventSock::PxComReceiveConinitRes(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt, pxcomres_t& result)
{
	if(!pComHead || !pComAll || !ppResComPkt){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*			pImData		= pChmCntrl->GetImDataObj();
	chmpxid_t			selfchmpxid	= pImData->GetSelfChmpxId();
	*ppResComPkt					= NULL;

	if(pComHead->term_ids.chmpxid == selfchmpxid){
		// To me
		PPXCOM_HEAD	pPxComHead = CVT_COMPTR_HEAD(pComAll);

		if(CHMPX_COM_RES_SUCCESS != pPxComHead->result){
			// Something error occured by status response
			ERR_CHMPRN("PPXCOM_N2_CONINIT_RES(or PPXCOM_CONINIT_RES) is failed.");
		}

		if(IS_COM_CURRENT_VERSION(pComHead->version)){				// check communication protocol version(backward compatibility)
			// current version
			PPXCOM_N2_CONINIT_RES	pConinitRes	= CVT_COMPTR_N2_CONINIT_RES(pComAll);
			result = pConinitRes->head.result;
		}else{
			// old version
			PPXCOM_CONINIT_RES	pConinitRes	= CVT_COMPTR_CONINIT_RES(pComAll);
			result = pConinitRes->head.result;
		}
	}else{
		// To other chmpxid
		ERR_CHMPRN("Received PPXCOM_CONINIT_RES packet, but terminal chmpxid(0x%016" PRIx64 ") is not self chmpxid(0x%016" PRIx64 ").", pComHead->term_ids.chmpxid, selfchmpxid);
		return false;
	}
	return true;
}

bool ChmEventSock::PxComSendJoinRing(chmpxid_t chmpxid, PCHMPXSVR pserver)
{
	if(CHM_INVALID_CHMPXID == chmpxid || !pserver){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// Make packet
	PCOMPKT	pComPkt;
	if(IS_COM_CURRENT_VERSION(comproto_ver)){					// check communication protocol version(backward compatibility)
		// current version
		if(NULL == (pComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_N2_JOIN_RING))){
			ERR_CHMPRN("Could not allocate memory for COMPKT.");
			return false;
		}

		// compkt
		PPXCOM_ALL			pComAll		= CVT_COM_ALL_PTR_PXCOMPKT(pComPkt);
		PPXCOM_N2_JOIN_RING	pJoinRing	= CVT_COMPTR_N2_JOIN_RING(pComAll);
		SET_PXCOMPKT(pComPkt, COM_VERSION_2, CHMPX_COM_N2_JOIN_RING, pImData->GetSelfChmpxId(), chmpxid, true, 0L);

		pJoinRing->head.type	= CHMPX_COM_N2_JOIN_RING;
		pJoinRing->head.result	= CHMPX_COM_RES_SUCCESS;
		pJoinRing->head.length	= sizeof(PXCOM_N2_JOIN_RING);
		COPY_PCHMPXSVR(&(pJoinRing->server), pserver);

	}else{
		// old version
		if(NULL == (pComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_JOIN_RING))){
			ERR_CHMPRN("Could not allocate memory for COMPKT.");
			return false;
		}

		// compkt
		PPXCOM_ALL			pComAll		= CVT_COM_ALL_PTR_PXCOMPKT(pComPkt);
		PPXCOM_JOIN_RING	pJoinRing	= CVT_COMPTR_JOIN_RING(pComAll);
		SET_PXCOMPKT(pComPkt, COM_VERSION_1, CHMPX_COM_JOIN_RING, pImData->GetSelfChmpxId(), chmpxid, true, 0L);

		pJoinRing->head.type	= CHMPX_COM_JOIN_RING;
		pJoinRing->head.result	= CHMPX_COM_RES_SUCCESS;
		pJoinRing->head.length	= sizeof(PXCOM_JOIN_RING);
		CVT_PCHMPXSVR_TO_PCHMPXSVRV1(&(pJoinRing->server), pserver);
	}

	// Send request
	if(!Send(pComPkt, NULL, 0L)){
		ERR_CHMPRN("Failed to send CHMPX_COM_JOIN_RING to chmpxid(0x%016" PRIx64 ").", chmpxid);
		CHM_Free(pComPkt);
		return false;
	}
	CHM_Free(pComPkt);

	return true;
}

bool ChmEventSock::PxComReceiveJoinRing(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt)
{
	if(!pComHead || !pComAll || !ppResComPkt){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*		pImData		= pChmCntrl->GetImDataObj();
	chmpxid_t		selfchmpxid	= pImData->GetSelfChmpxId();
	*ppResComPkt				= NULL;

	if(pComHead->dept_ids.chmpxid == selfchmpxid){
		// around -> next step: update pending hash value
		//
		PPXCOM_HEAD	pPxComHead = CVT_COMPTR_HEAD(pComAll);

		if(CHMPX_COM_RES_SUCCESS != pPxComHead->result){
			// Failed adding this server on RING
			//
			// This function returns false, so caller gets it and close all connection.
			// So that, this function do nothing.
			//
			ERR_CHMPRN("PXCOM_N2_JOIN_RING(or PXCOM_JOIN_RING) is failed, hope to recover automatically.");
			return false;

		}

		// Succeed adding this server on RING
		//
		// Next step is update all server status.
		//
		// [NOTE]
		// PPXCOM_N2_JOIN_RING and PPXCOM_JOIN_RING are the same process.
		//
		chmpxid_t	nextchmpxid	= GetNextRingChmpxId();
		if(CHM_INVALID_CHMPXID == nextchmpxid){
			ERR_CHMPRN("Could not get next server chmpxid, maybe there is no server on RING...But WHY?");
			return true;
		}

		int	nextsock = CHM_INVALID_SOCK;
		if(!GetLockedSendSock(nextchmpxid, nextsock, false) || CHM_INVALID_SOCK == nextsock){		// LOCK SOCKET
			ERR_CHMPRN("Could not get socket for chmpxid(0x%016" PRIx64 ").", nextchmpxid);
			return false;
		}

		// Send request
		if(!PxComSendStatusReq(nextsock, nextchmpxid, true)){	// if error, need to close sock in method.
			ERR_CHMPRN("Failed to send PXCOM_STATUS_REQ.");
			UnlockSendSock(nextsock);			// UNLOCK SOCKET
			return false;
		}
		UnlockSendSock(nextsock);				// UNLOCK SOCKET

	}else{
		// update own chmshm & transfer packet.
		//

		CHMPXSVR	chmpxsvr_tmp;
		PCHMPXSVR	pchmpxsvr;
		pxcomres_t	ReceiveResultCode = CHMPX_COM_RES_SUCCESS;
		if(IS_COM_CURRENT_VERSION(pComHead->version)){				// check communication protocol version(backward compatibility)
			// current version
			PPXCOM_N2_JOIN_RING	pJoinRing	= CVT_COMPTR_N2_JOIN_RING(pComAll);
			pchmpxsvr						= &(pJoinRing->server);

			if(CHMPX_COM_RES_SUCCESS != pJoinRing->head.result){
				MSG_CHMPRN("Already error occured before this server.");
				ReceiveResultCode = CHMPX_COM_RES_ERROR;
			}
		}else{
			// old version
			PPXCOM_JOIN_RING	pJoinRing	= CVT_COMPTR_JOIN_RING(pComAll);
			pchmpxsvr						= &chmpxsvr_tmp;

			// convert CHMPXSVRV1 to CHMPXSVR
			CVT_PCHMPXSVRV1_TO_PCHMPXSVR(pchmpxsvr, &(pJoinRing->server));

			if(CHMPX_COM_RES_SUCCESS != pJoinRing->head.result){
				MSG_CHMPRN("Already error occured before this server.");
				ReceiveResultCode = CHMPX_COM_RES_ERROR;
			}
		}

		// update chmshm
		pxcomres_t	ResultCode;
		if(!pImData->MergeChmpxSvrs(pchmpxsvr, 1, false, false, eqfd)){				// not remove other
			// error occured, so transfer packet.
			ERR_CHMPRN("Could not update server chmpx information.");
			ResultCode = CHMPX_COM_RES_ERROR;
		}else{
			// succeed.
			ResultCode = CHMPX_COM_RES_SUCCESS;
		}
		if(CHMPX_COM_RES_SUCCESS != ReceiveResultCode){
			MSG_CHMPRN("Already error occured before this server.");
			ResultCode = CHMPX_COM_RES_ERROR;
		}

		// check & rechain
		bool	is_rechain = false;
		if(!CheckRechainRing(pchmpxsvr->chmpxid, is_rechain)){
			ERR_CHMPRN("Something error occured in rechaining RING, but continue...");
			ResultCode = CHMPX_COM_RES_ERROR;
		}else{
			if(is_rechain){
				MSG_CHMPRN("Rechained RING after joining chmpxid(0x%016" PRIx64 ").", pchmpxsvr->chmpxid);
			}else{
				MSG_CHMPRN("Not rechained RING after joining chmpxid(0x%016" PRIx64 ").", pchmpxsvr->chmpxid);
			}
		}

		// next chmpxid(after rechaining)
		chmpxid_t	nextchmpxid;
		if(is_rechain){
			// After rechaining, new server does not in server list.
			// So get old next server by GetNextRingChmpxId(), force set new server chmpxid.
			//
			nextchmpxid = pchmpxsvr->chmpxid;
		}else{
			nextchmpxid	= GetNextRingChmpxId();
		}
		if(CHM_INVALID_CHMPXID == nextchmpxid){
			ERR_CHMPRN("Could not get next server chmpxid.");
			return false;
		}

		if(!IsSafeDeptAndNextChmpxId(pComHead->dept_ids.chmpxid, nextchmpxid)){
			// Departure chmpx maybe DOWN!
			//
			// [NOTICE]
			// This case is very small case, departure server sends this packet but that server could not
			// be connected from in RING server.(ex. down after sending, or FQDN is wrong, etc)
			// So we do not transfer this packet, because it could not be stopped in RING.
			//
			ERR_CHMPRN("Departure chmpxid(0x%016" PRIx64 ") maybe down, so stop transferring this packet.", pComHead->dept_ids.chmpxid);
			*ppResComPkt = NULL;

		}else{
			// make response data buffer
			PCOMPKT	pResComPkt;
			if(IS_COM_CURRENT_VERSION(pComHead->version)){				// check communication protocol version(backward compatibility)
				// current version
				if(NULL == (pResComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_N2_JOIN_RING))){
					ERR_CHMPRN("Could not allocation memory.");
					return false;
				}

				// compkt
				PPXCOM_ALL			pResComAll	= CVT_COM_ALL_PTR_PXCOMPKT(pResComPkt);
				PPXCOM_N2_JOIN_RING	pResJoinRing= CVT_COMPTR_N2_JOIN_RING(pResComAll);

				SET_PXCOMPKT(pResComPkt, COM_VERSION_2, CHMPX_COM_N2_JOIN_RING, pComHead->dept_ids.chmpxid, nextchmpxid, false, 0L);	// dept chmpxid is not changed
				COPY_TIMESPEC(&(pResComPkt->head.reqtime), &(pComHead->reqtime));	// not change

				// join_ring(copy)
				pResJoinRing->head.type				= CHMPX_COM_N2_JOIN_RING;
				pResJoinRing->head.result			= ResultCode;
				pResJoinRing->head.length			= sizeof(PXCOM_N2_JOIN_RING);
				COPY_PCHMPXSVR(&(pResJoinRing->server), pchmpxsvr);

			}else{
				// old version
				if(NULL == (pResComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_JOIN_RING))){
					ERR_CHMPRN("Could not allocation memory.");
					return false;
				}

				// compkt
				PPXCOM_ALL			pResComAll	= CVT_COM_ALL_PTR_PXCOMPKT(pResComPkt);
				PPXCOM_JOIN_RING	pResJoinRing= CVT_COMPTR_JOIN_RING(pResComAll);

				SET_PXCOMPKT(pResComPkt, COM_VERSION_1, CHMPX_COM_JOIN_RING, pComHead->dept_ids.chmpxid, nextchmpxid, false, 0L);	// dept chmpxid is not changed
				COPY_TIMESPEC(&(pResComPkt->head.reqtime), &(pComHead->reqtime));	// not change

				// join_ring(copy)
				pResJoinRing->head.type				= CHMPX_COM_JOIN_RING;
				pResJoinRing->head.result			= ResultCode;
				pResJoinRing->head.length			= sizeof(PXCOM_JOIN_RING);
				CVT_PCHMPXSVR_TO_PCHMPXSVRV1(&(pResJoinRing->server), pchmpxsvr);
			}
			*ppResComPkt = pResComPkt;
		}
	}
	return true;
}

//
// [NOTICE]
// This packet is not used now. If you need to use this packet, you MUST call
// UpdateSelfLastStatusTime() function before this function for pchmpxsvrs.
//
bool ChmEventSock::PxComSendStatusUpdate(chmpxid_t chmpxid, PCHMPXSVR pchmpxsvrs, long count)
{
	if(CHM_INVALID_CHMPXID == chmpxid || !pchmpxsvrs || count <= 0L){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*		pImData = pChmCntrl->GetImDataObj();

	// make data
	PCOMPKT	pComPkt;
	if(IS_COM_CURRENT_VERSION(comproto_ver)){					// check communication protocol version(backward compatibility)
		// current version
		if(NULL == (pComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_N2_STATUS_UPDATE, sizeof(CHMPXSVR) * count))){
			ERR_CHMPRN("Could not allocation memory.");
			return false;
		}

		// compkt
		PPXCOM_ALL				pComAll		= CVT_COM_ALL_PTR_PXCOMPKT(pComPkt);
		PPXCOM_N2_STATUS_UPDATE	pStsUpdate	= CVT_COMPTR_N2_STATUS_UPDATE(pComAll);
		SET_PXCOMPKT(pComPkt, COM_VERSION_2, CHMPX_COM_N2_STATUS_UPDATE, pImData->GetSelfChmpxId(), chmpxid, true, (sizeof(CHMPXSVR) * count));

		// status_update
		pStsUpdate->head.type			= CHMPX_COM_N2_STATUS_UPDATE;
		pStsUpdate->head.result			= CHMPX_COM_RES_SUCCESS;
		pStsUpdate->head.length			= sizeof(PXCOM_N2_STATUS_UPDATE) + (sizeof(CHMPXSVR) * count);
		pStsUpdate->count				= count;
		pStsUpdate->pchmpxsvr_offset	= sizeof(PXCOM_N2_STATUS_UPDATE);

		// extra area
		PCHMPXSVR	pStsUpChmsvr = CHM_OFFSET(pStsUpdate, sizeof(PXCOM_N2_STATUS_UPDATE), PCHMPXSVR);
		for(long cnt = 0; cnt < count; cnt++){
			COPY_PCHMPXSVR(&pStsUpChmsvr[cnt], &pchmpxsvrs[cnt]);
		}

	}else{
		// old version
		if(NULL == (pComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_STATUS_UPDATE, sizeof(CHMPXSVRV1) * count))){
			ERR_CHMPRN("Could not allocation memory.");
			return false;
		}

		// compkt
		PPXCOM_ALL				pComAll		= CVT_COM_ALL_PTR_PXCOMPKT(pComPkt);
		PPXCOM_STATUS_UPDATE	pStsUpdate	= CVT_COMPTR_STATUS_UPDATE(pComAll);
		SET_PXCOMPKT(pComPkt, COM_VERSION_1, CHMPX_COM_STATUS_UPDATE, pImData->GetSelfChmpxId(), chmpxid, true, (sizeof(CHMPXSVRV1) * count));

		// status_update
		pStsUpdate->head.type			= CHMPX_COM_STATUS_UPDATE;
		pStsUpdate->head.result			= CHMPX_COM_RES_SUCCESS;
		pStsUpdate->head.length			= sizeof(PXCOM_STATUS_UPDATE) + (sizeof(CHMPXSVRV1) * count);
		pStsUpdate->count				= count;
		pStsUpdate->pchmpxsvr_offset	= sizeof(PXCOM_STATUS_UPDATE);

		// extra area
		PCHMPXSVRV1	pStsUpChmsvr = CHM_OFFSET(pStsUpdate, sizeof(PXCOM_STATUS_UPDATE), PCHMPXSVRV1);
		for(long cnt = 0; cnt < count; cnt++){
			CVT_PCHMPXSVR_TO_PCHMPXSVRV1(&pStsUpChmsvr[cnt], &pchmpxsvrs[cnt]);
		}
	}

	// Send request
	if(!Send(pComPkt, NULL, 0L)){
		ERR_CHMPRN("Failed to send CHMPX_COM_N2_STATUS_UPDATE(or CHMPX_COM_STATUS_UPDATE) to chmpxid(0x%016" PRIx64 ").", chmpxid);
		CHM_Free(pComPkt);
		return false;
	}
	CHM_Free(pComPkt);

	return true;
}

bool ChmEventSock::PxComReceiveStatusUpdate(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt)
{
	if(!pComHead || !pComAll || !ppResComPkt){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*		pImData			= pChmCntrl->GetImDataObj();
	chmpxid_t		selfchmpxid		= pImData->GetSelfChmpxId();
	*ppResComPkt					= NULL;

	if(pComHead->dept_ids.chmpxid == selfchmpxid){
		// around
		//
		PPXCOM_HEAD	pPxComHead = CVT_COMPTR_HEAD(pComAll);

		if(CHMPX_COM_RES_SUCCESS != pPxComHead->result){
			// Failed updating status, for recovering...
			ERR_CHMPRN("PXCOM_N2_STATUS_UPDATE(or PXCOM_STATUS_UPDATE) is failed, NEED to check all servers and recover MANUALLY.");
			return false;
		}

		// Succeed updating status
		//
		// [NOTE]
		// PXCOM_N2_STATUS_UPDATE and PXCOM_STATUS_UPDATE are the same process.
		//

	}else{
		// update own chmshm & transfer packet.
		//
		chmpxid_t	nextchmpxid	= GetNextRingChmpxId();
		if(CHM_INVALID_CHMPXID == nextchmpxid){
			ERR_CHMPRN("Could not get next server chmpxid.");
			return false;
		}

		PCHMPXSVR	pResultChmsvrs		= NULL;
		PCHMPXSVR	pResultChmsvrs_tmp	= NULL;						// [NOTICE] need to free
		pxcomres_t	ResultCode;
		long		ResultCount;
		if(IS_COM_CURRENT_VERSION(pComHead->version)){				// check communication protocol version(backward compatibility)
			// current version
			PPXCOM_N2_STATUS_UPDATE	pReqStsUpdate	= CVT_COMPTR_N2_STATUS_UPDATE(pComAll);
			pResultChmsvrs							= CHM_OFFSET(pReqStsUpdate, sizeof(PXCOM_N2_STATUS_UPDATE), PCHMPXSVR);
			ResultCode								= pReqStsUpdate->head.result;
			ResultCount								= pReqStsUpdate->count;

		}else{
			// old version
			PPXCOM_STATUS_UPDATE	pReqStsUpdate	= CVT_COMPTR_STATUS_UPDATE(pComAll);
			PCHMPXSVRV1				pReqChmsvrsv1	= CHM_OFFSET(pReqStsUpdate, sizeof(PXCOM_STATUS_UPDATE), PCHMPXSVRV1);
			ResultCode								= pReqStsUpdate->head.result;
			ResultCount								= pReqStsUpdate->count;

			if(0 < ResultCount){
				// allocation for CHMPXSVR(current)
				if(NULL == (pResultChmsvrs_tmp = reinterpret_cast<PCHMPXSVR>(calloc(ResultCount, sizeof(CHMPXSVR))))){
					ERR_CHMPRN("Could not allocation memory.");
					return false;
				}
				// convert to CHMPXSVR(current)
				for(long pos = 0; pos < ResultCount; ++pos){
					CVT_PCHMPXSVRV1_TO_PCHMPXSVR(&pResultChmsvrs_tmp[pos], &pReqChmsvrsv1[pos]);
				}
				// set pointer
				pResultChmsvrs = pResultChmsvrs_tmp;
			}
		}

		// Already error occured, skip merging
		if(CHMPX_COM_RES_SUCCESS != ResultCode){
			MSG_CHMPRN("Already error occured before this server.");
			ResultCode = CHMPX_COM_RES_ERROR;

		}else{
			// update status into chmshm
			if(!pImData->MergeChmpxSvrsForStatusUpdate(pResultChmsvrs, ResultCount, eqfd)){		// not remove other
				// error occured.
				ERR_CHMPRN("Could not update all server status, maybe recovering in function...");
				ResultCode = CHMPX_COM_RES_ERROR;
			}else{
				MSG_CHMPRN("Succeed update status.");
				ResultCode = CHMPX_COM_RES_SUCCESS;
			}
		}

		if(!IsSafeDeptAndNextChmpxId(pComHead->dept_ids.chmpxid, nextchmpxid)){
			// Departure chmpx maybe DOWN!
			//
			// [NOTICE]
			// This case is very small case, departure server sends this packet but that server could not
			// be connected from in RING server.(ex. down after sending, or FQDN is wrong, etc)
			// So we do not transfer this packet, because it could not be stopped in RING.
			//
			ERR_CHMPRN("Departure chmpxid(0x%016" PRIx64 ") maybe down, so stop transferring this packet.", pComHead->dept_ids.chmpxid);
			*ppResComPkt = NULL;

		}else{
			// make response data buffer
			PCOMPKT	pResComPkt;
			if(IS_COM_CURRENT_VERSION(pComHead->version)){				// check communication protocol version(backward compatibility)
				// current version
				if(NULL == (pResComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_N2_STATUS_UPDATE, sizeof(CHMPXSVR) * ResultCount))){
					ERR_CHMPRN("Could not allocation memory.");
					CHM_Free(pResultChmsvrs_tmp);
					return false;
				}

				// compkt
				PPXCOM_ALL				pResComAll		= CVT_COM_ALL_PTR_PXCOMPKT(pResComPkt);
				PPXCOM_N2_STATUS_UPDATE	pResStsUpdate	= CVT_COMPTR_N2_STATUS_UPDATE(pResComAll);

				SET_PXCOMPKT(pResComPkt, COM_VERSION_2, CHMPX_COM_N2_STATUS_UPDATE, pComHead->dept_ids.chmpxid, nextchmpxid, false, (sizeof(CHMPXSVR) * ResultCount));	// dept chmpxid is not changed.
				COPY_TIMESPEC(&(pResComPkt->head.reqtime), &(pComHead->reqtime));	// not change

				// status_update(copy)
				pResStsUpdate->head.type				= CHMPX_COM_N2_STATUS_UPDATE;
				pResStsUpdate->head.result				= ResultCode;
				pResStsUpdate->head.length				= sizeof(PXCOM_N2_STATUS_UPDATE) + (sizeof(CHMPXSVR) * ResultCount);
				pResStsUpdate->count					= ResultCount;
				pResStsUpdate->pchmpxsvr_offset			= sizeof(PXCOM_N2_STATUS_UPDATE);

				// extra area
				PCHMPXSVR	pResChmsvrs = CHM_OFFSET(pResStsUpdate, sizeof(PXCOM_N2_STATUS_UPDATE), PCHMPXSVR);
				for(long cnt = 0; cnt < ResultCount; cnt++){
					COPY_PCHMPXSVR(&pResChmsvrs[cnt], &pResultChmsvrs[cnt]);
				}

			}else{
				// old version
				if(NULL == (pResComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_STATUS_UPDATE, sizeof(CHMPXSVRV1) * ResultCount))){
					ERR_CHMPRN("Could not allocation memory.");
					CHM_Free(pResultChmsvrs_tmp);
					return false;
				}

				// compkt
				PPXCOM_ALL				pResComAll		= CVT_COM_ALL_PTR_PXCOMPKT(pResComPkt);
				PPXCOM_STATUS_UPDATE	pResStsUpdate	= CVT_COMPTR_STATUS_UPDATE(pResComAll);

				SET_PXCOMPKT(pResComPkt, COM_VERSION_1, CHMPX_COM_STATUS_UPDATE, pComHead->dept_ids.chmpxid, nextchmpxid, false, (sizeof(CHMPXSVRV1) * ResultCount));	// dept chmpxid is not changed.
				COPY_TIMESPEC(&(pResComPkt->head.reqtime), &(pComHead->reqtime));	// not change

				// status_update(copy)
				pResStsUpdate->head.type				= CHMPX_COM_STATUS_UPDATE;
				pResStsUpdate->head.result				= ResultCode;
				pResStsUpdate->head.length				= sizeof(PXCOM_N2_STATUS_UPDATE) + (sizeof(CHMPXSVRV1) * ResultCount);
				pResStsUpdate->count					= ResultCount;
				pResStsUpdate->pchmpxsvr_offset			= sizeof(PXCOM_STATUS_UPDATE);

				// extra area
				PCHMPXSVRV1	pResChmsvrsv1 = CHM_OFFSET(pResStsUpdate, sizeof(PXCOM_STATUS_UPDATE), PCHMPXSVRV1);
				for(long cnt = 0; cnt < ResultCount; cnt++){
					CVT_PCHMPXSVR_TO_PCHMPXSVRV1(&pResChmsvrsv1[cnt], &pResultChmsvrs[cnt]);
				}
			}
			*ppResComPkt = pResComPkt;
		}
		CHM_Free(pResultChmsvrs_tmp);
	}
	return true;
}

bool ChmEventSock::PxComSendStatusConfirm(chmpxid_t chmpxid, PCHMPXSVR pchmpxsvrs, long count)
{
	if(CHM_INVALID_CHMPXID == chmpxid || !pchmpxsvrs || count <= 0L){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*		pImData = pChmCntrl->GetImDataObj();

	// make data
	PCOMPKT	pComPkt;
	if(IS_COM_CURRENT_VERSION(comproto_ver)){					// check communication protocol version(backward compatibility)
		// current version
		if(NULL == (pComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_N2_STATUS_CONFIRM, sizeof(CHMPXSVR) * count))){
			ERR_CHMPRN("Could not allocation memory.");
			return false;
		}

		// compkt
		PPXCOM_ALL					pComAll		= CVT_COM_ALL_PTR_PXCOMPKT(pComPkt);
		PPXCOM_N2_STATUS_CONFIRM	pStsConfirm	= CVT_COMPTR_N2_STATUS_CONFIRM(pComAll);
		SET_PXCOMPKT(pComPkt, COM_VERSION_2, CHMPX_COM_N2_STATUS_CONFIRM, pImData->GetSelfChmpxId(), chmpxid, true, (sizeof(CHMPXSVR) * count));

		// status_update
		pStsConfirm->head.type			= CHMPX_COM_N2_STATUS_CONFIRM;
		pStsConfirm->head.result		= CHMPX_COM_RES_SUCCESS;
		pStsConfirm->head.length		= sizeof(PXCOM_N2_STATUS_CONFIRM) + (sizeof(CHMPXSVR) * count);
		pStsConfirm->count				= count;
		pStsConfirm->pchmpxsvr_offset	= sizeof(PXCOM_N2_STATUS_CONFIRM);

		// extra area
		PCHMPXSVR	pStsChmsvr = CHM_OFFSET(pStsConfirm, sizeof(PXCOM_N2_STATUS_CONFIRM), PCHMPXSVR);
		for(long cnt = 0; cnt < count; cnt++){
			COPY_PCHMPXSVR(&pStsChmsvr[cnt], &pchmpxsvrs[cnt]);
		}

	}else{
		// old version
		if(NULL == (pComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_STATUS_CONFIRM, sizeof(CHMPXSVRV1) * count))){
			ERR_CHMPRN("Could not allocation memory.");
			return false;
		}

		// compkt
		PPXCOM_ALL				pComAll		= CVT_COM_ALL_PTR_PXCOMPKT(pComPkt);
		PPXCOM_STATUS_CONFIRM	pStsConfirm	= CVT_COMPTR_STATUS_CONFIRM(pComAll);
		SET_PXCOMPKT(pComPkt, COM_VERSION_1, CHMPX_COM_STATUS_CONFIRM, pImData->GetSelfChmpxId(), chmpxid, true, (sizeof(CHMPXSVRV1) * count));

		// status_update
		pStsConfirm->head.type			= CHMPX_COM_STATUS_CONFIRM;
		pStsConfirm->head.result		= CHMPX_COM_RES_SUCCESS;
		pStsConfirm->head.length		= sizeof(PXCOM_STATUS_CONFIRM) + (sizeof(CHMPXSVRV1) * count);
		pStsConfirm->count				= count;
		pStsConfirm->pchmpxsvr_offset	= sizeof(PXCOM_STATUS_CONFIRM);

		// extra area
		PCHMPXSVRV1	pStsChmsvrv1 = CHM_OFFSET(pStsConfirm, sizeof(PXCOM_STATUS_CONFIRM), PCHMPXSVRV1);
		for(long cnt = 0; cnt < count; cnt++){
			CVT_PCHMPXSVR_TO_PCHMPXSVRV1(&pStsChmsvrv1[cnt], &pchmpxsvrs[cnt]);
		}
	}

	// Send request
	if(!Send(pComPkt, NULL, 0L)){
		ERR_CHMPRN("Failed to send CHMPX_COM_N2_STATUS_CONFIRM(or CHMPX_COM_STATUS_CONFIRM) to chmpxid(0x%016" PRIx64 ").", chmpxid);
		CHM_Free(pComPkt);
		return false;
	}
	CHM_Free(pComPkt);

	return true;
}

bool ChmEventSock::PxComReceiveStatusConfirm(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt)
{
	if(!pComHead || !pComAll || !ppResComPkt){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*		pImData		= pChmCntrl->GetImDataObj();
	chmpxid_t		selfchmpxid	= pImData->GetSelfChmpxId();
	*ppResComPkt				= NULL;

	if(pComHead->dept_ids.chmpxid == selfchmpxid){
		// around
		//
		chmpxid_t	nextchmpxid	= GetNextRingChmpxId();
		if(CHM_INVALID_CHMPXID == nextchmpxid){
			ERR_CHMPRN("Could not get next server chmpxid.");
			return false;
		}

		PPXCOM_HEAD	pPxComHead = CVT_COMPTR_HEAD(pComAll);

		if(CHMPX_COM_RES_SUCCESS == pPxComHead->result){
			// Succeed status confirm, do next step
			//

			// If it is merging now, we do not start merging.
			if(!CheckAllStatusForMergeStart()){
				WAN_CHMPRN("Now merging, then could not start to new merge. but it will start after finishing merging.");
				return true;
			}

			// Send start merge
			if(!PxComSendMergeStart(nextchmpxid)){
				ERR_CHMPRN("Failed to send CHMPX_COM_MERGE_START to next chmpxid(0x%016" PRIx64 ").", nextchmpxid);
				return false;
			}

		}else{
			// Failed status confirm, Thus retry to send status confirm
			WAN_CHMPRN("PXCOM_N2_STATUS_CONFIRM(or PXCOM_STATUS_CONFIRM) is failed, thus update self status and retry send status confirm.");

			// Update self last status updating time for retrying
			if(!pImData->UpdateSelfLastStatusTime()){
				ERR_CHMPRN("Could not update own updating status time, but continue...");
			}

			CHMPXSVR	chmpxsvr;
			if(!pImData->GetSelfChmpxSvr(&chmpxsvr)){
				ERR_CHMPRN("Could not get self chmpx information.");
				return false;
			}
			if(!PxComSendStatusChange(nextchmpxid, &chmpxsvr)){
				ERR_CHMPRN("Failed to send self status change.");
				return false;
			}

			PCHMPXSVR	pchmpxsvrs	= NULL;
			long		count		= 0L;
			if(!pImData->GetChmpxSvrs(&pchmpxsvrs, count)){
				ERR_CHMPRN("Could not get all server status, so stop retry confirm status.");
				return false;
			}
			// send status_confirm
			if(!PxComSendStatusConfirm(nextchmpxid, pchmpxsvrs, count)){
				ERR_CHMPRN("Failed to send CHMPX_COM_STATUS_CONFIRM.");
				CHM_Free(pchmpxsvrs);
				return false;
			}
			CHM_Free(pchmpxsvrs);

			// Not start to merge, but retry to get confirm
		}

	}else{
		// check chmshm & transfer packet.
		//
		chmpxid_t	nextchmpxid	= GetNextRingChmpxId();
		if(CHM_INVALID_CHMPXID == nextchmpxid){
			ERR_CHMPRN("Could not get next server chmpxid.");
			return false;
		}

		PCHMPXSVR	pReqChmsvrs		= NULL;
		PCHMPXSVR	pReqChmsvrs_tmp	= NULL;							// [NOTICE] need to free
		pxcomres_t	ResultCode;
		long		ResultCount;
		if(IS_COM_CURRENT_VERSION(pComHead->version)){				// check communication protocol version(backward compatibility)
			// current version
			PPXCOM_N2_STATUS_CONFIRM	pReqStsConfirm	= CVT_COMPTR_N2_STATUS_CONFIRM(pComAll);
			pReqChmsvrs									= CHM_OFFSET(pReqStsConfirm, sizeof(PXCOM_N2_STATUS_CONFIRM), PCHMPXSVR);
			ResultCode									= pReqStsConfirm->head.result;
			ResultCount									= pReqStsConfirm->count;

		}else{
			// old version
			PPXCOM_STATUS_CONFIRM	pReqStsConfirm	= CVT_COMPTR_STATUS_CONFIRM(pComAll);
			PCHMPXSVRV1				pReqChmsvrsv1	= CHM_OFFSET(pReqStsConfirm, sizeof(PXCOM_STATUS_CONFIRM), PCHMPXSVRV1);
			ResultCode								= pReqStsConfirm->head.result;
			ResultCount								= pReqStsConfirm->count;

			if(0 < ResultCount){
				// allocation for CHMPXSVR(current)
				if(NULL == (pReqChmsvrs_tmp = reinterpret_cast<PCHMPXSVR>(calloc(ResultCount, sizeof(CHMPXSVR))))){
					ERR_CHMPRN("Could not allocation memory.");
					return false;
				}
				// convert to CHMPXSVR(current)
				for(long pos = 0; pos < ResultCount; ++pos){
					CVT_PCHMPXSVRV1_TO_PCHMPXSVR(&pReqChmsvrs_tmp[pos], &pReqChmsvrsv1[pos]);
				}
				// set pointer
				pReqChmsvrs = pReqChmsvrs_tmp;
			}
		}

		// Already error occured, skip merging
		if(CHMPX_COM_RES_SUCCESS != ResultCode){
			MSG_CHMPRN("Already error occured before this server.");
			ResultCode = CHMPX_COM_RES_ERROR;
		}else{
			// Compare
			if(!pImData->CompareChmpxSvrs(pReqChmsvrs, ResultCount)){
				MSG_CHMPRN("Status(%ld count status) could not confirm.", ResultCount);
				ResultCode = CHMPX_COM_RES_ERROR;
			}else{
				ResultCode = CHMPX_COM_RES_SUCCESS;
			}
		}

		if(!IsSafeDeptAndNextChmpxId(pComHead->dept_ids.chmpxid, nextchmpxid)){
			// Departure chmpx maybe DOWN!
			//
			// [NOTICE]
			// This case is very small case, departure server sends this packet but that server could not
			// be connected from in RING server.(ex. down after sending, or FQDN is wrong, etc)
			// So we do not transfer this packet, because it could not be stopped in RING.
			//
			ERR_CHMPRN("Departure chmpxid(0x%016" PRIx64 ") maybe down, so stop transferring this packet.", pComHead->dept_ids.chmpxid);
			*ppResComPkt = NULL;

		}else{
			// make response data buffer
			PCOMPKT	pResComPkt;
			if(IS_COM_CURRENT_VERSION(pComHead->version)){				// check communication protocol version(backward compatibility)
				// current version
				if(NULL == (pResComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_N2_STATUS_CONFIRM, sizeof(CHMPXSVR) * ResultCount))){
					ERR_CHMPRN("Could not allocation memory.");
					CHM_Free(pReqChmsvrs_tmp);
					return false;
				}

				// compkt
				PPXCOM_ALL					pResComAll		= CVT_COM_ALL_PTR_PXCOMPKT(pResComPkt);
				PPXCOM_N2_STATUS_CONFIRM	pResStsConfirm	= CVT_COMPTR_N2_STATUS_CONFIRM(pResComAll);

				SET_PXCOMPKT(pResComPkt, COM_VERSION_2, CHMPX_COM_N2_STATUS_CONFIRM, pComHead->dept_ids.chmpxid, nextchmpxid, false, (sizeof(CHMPXSVR) * ResultCount));	// dept chmpxid is not changed.
				COPY_TIMESPEC(&(pResComPkt->head.reqtime), &(pComHead->reqtime));	// not change

				// status_confirm(copy)
				pResStsConfirm->head.type				= CHMPX_COM_N2_STATUS_CONFIRM;
				pResStsConfirm->head.result				= ResultCode;
				pResStsConfirm->head.length				= sizeof(PXCOM_N2_STATUS_CONFIRM) + (sizeof(CHMPXSVR) * ResultCount);
				pResStsConfirm->count					= ResultCount;
				pResStsConfirm->pchmpxsvr_offset		= sizeof(PXCOM_N2_STATUS_CONFIRM);

				// extra area
				PCHMPXSVR	pResChmsvrs = CHM_OFFSET(pResStsConfirm, sizeof(PXCOM_N2_STATUS_CONFIRM), PCHMPXSVR);
				for(long cnt = 0; cnt < ResultCount; cnt++){
					COPY_PCHMPXSVR(&pResChmsvrs[cnt], &pReqChmsvrs[cnt]);
				}

			}else{
				// old version
				if(NULL == (pResComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_STATUS_CONFIRM, sizeof(CHMPXSVRV1) * ResultCount))){
					ERR_CHMPRN("Could not allocation memory.");
					CHM_Free(pReqChmsvrs_tmp);
					return false;
				}

				// compkt
				PPXCOM_ALL				pResComAll		= CVT_COM_ALL_PTR_PXCOMPKT(pResComPkt);
				PPXCOM_STATUS_CONFIRM	pResStsConfirm	= CVT_COMPTR_STATUS_CONFIRM(pResComAll);

				SET_PXCOMPKT(pResComPkt, COM_VERSION_1, CHMPX_COM_STATUS_CONFIRM, pComHead->dept_ids.chmpxid, nextchmpxid, false, (sizeof(CHMPXSVRV1) * ResultCount));	// dept chmpxid is not changed.
				COPY_TIMESPEC(&(pResComPkt->head.reqtime), &(pComHead->reqtime));	// not change

				// status_confirm(copy)
				pResStsConfirm->head.type				= CHMPX_COM_STATUS_CONFIRM;
				pResStsConfirm->head.result				= ResultCode;
				pResStsConfirm->head.length				= sizeof(PXCOM_N2_STATUS_CONFIRM) + (sizeof(CHMPXSVRV1) * ResultCount);
				pResStsConfirm->count					= ResultCount;
				pResStsConfirm->pchmpxsvr_offset		= sizeof(PXCOM_STATUS_CONFIRM);

				// extra area
				PCHMPXSVRV1	pResChmsvrsv1 = CHM_OFFSET(pResStsConfirm, sizeof(PXCOM_STATUS_CONFIRM), PCHMPXSVRV1);
				for(long cnt = 0; cnt < ResultCount; cnt++){
					CVT_PCHMPXSVR_TO_PCHMPXSVRV1(&pResChmsvrsv1[cnt], &pReqChmsvrs[cnt]);
				}
			}
			*ppResComPkt = pResComPkt;
		}
		CHM_Free(pReqChmsvrs_tmp);
	}
	return true;
}

//
// Send server status to all servers(on ring), and if is_send_slaves, sends packet to slaves.
//
bool ChmEventSock::PxComSendStatusChange(chmpxid_t chmpxid, PCHMPXSVR pserver, bool is_send_slaves)
{
	if(CHM_INVALID_CHMPXID == chmpxid || !pserver){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// Make packet
	PCOMPKT	pComPkt;
	if(IS_COM_CURRENT_VERSION(comproto_ver)){					// check communication protocol version(backward compatibility)
		// current version
		if(NULL == (pComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_N2_STATUS_CHANGE))){
			ERR_CHMPRN("Could not allocate memory for COMPKT.");
			return false;
		}

		// compkt
		PPXCOM_ALL				pComAll			= CVT_COM_ALL_PTR_PXCOMPKT(pComPkt);
		PPXCOM_N2_STATUS_CHANGE	pStatusChange	= CVT_COMPTR_N2_STATUS_CHANGE(pComAll);
		SET_PXCOMPKT(pComPkt, COM_VERSION_2, CHMPX_COM_N2_STATUS_CHANGE, pImData->GetSelfChmpxId(), chmpxid, true, 0L);

		pStatusChange->head.type	= CHMPX_COM_N2_STATUS_CHANGE;
		pStatusChange->head.result	= CHMPX_COM_RES_SUCCESS;
		pStatusChange->head.length	= sizeof(PXCOM_N2_STATUS_CHANGE);
		COPY_PCHMPXSVR(&(pStatusChange->server), pserver);

	}else{
		// old version
		if(NULL == (pComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_STATUS_CHANGE))){
			ERR_CHMPRN("Could not allocate memory for COMPKT.");
			return false;
		}

		// compkt
		PPXCOM_ALL				pComAll			= CVT_COM_ALL_PTR_PXCOMPKT(pComPkt);
		PPXCOM_STATUS_CHANGE	pStatusChange	= CVT_COMPTR_STATUS_CHANGE(pComAll);
		SET_PXCOMPKT(pComPkt, COM_VERSION_1, CHMPX_COM_STATUS_CHANGE, pImData->GetSelfChmpxId(), chmpxid, true, 0L);

		pStatusChange->head.type	= CHMPX_COM_STATUS_CHANGE;
		pStatusChange->head.result	= CHMPX_COM_RES_SUCCESS;
		pStatusChange->head.length	= sizeof(PXCOM_STATUS_CHANGE);
		CVT_PCHMPXSVR_TO_PCHMPXSVRV1(&(pStatusChange->server), pserver);
	}

	// Send request
	if(!Send(pComPkt, NULL, 0L)){
		ERR_CHMPRN("Failed to send CHMPX_COM_STATUS_CHANGE to chmpxid(0x%016" PRIx64 ").", chmpxid);
		CHM_Free(pComPkt);
		return false;
	}

	// send to slaves
	if(is_send_slaves){
		if(!PxComSendSlavesStatusChange(pserver)){
			ERR_CHMPRN("Failed to send CHMPX_COM_N2_STATUS_CHANGE(or CHMPX_COM_STATUS_CHANGE) to slaves.");
		}
	}
	CHM_Free(pComPkt);

	return true;
}

//
// Only send slaves
//
bool ChmEventSock::PxComSendSlavesStatusChange(PCHMPXSVR pserver)
{
	if(!pserver){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// get all slave chmpxid.
	chmpxid_t		selfchmpxid = pImData->GetSelfChmpxId();
	chmpxidlist_t	chmpxidlist;
	if(0L == pImData->GetSlaveChmpxIds(chmpxidlist)){
		MSG_CHMPRN("There is no slave, so not need to send STATUS_CHANGE.");
		return true;
	}

	// Make packet
	PCOMPKT	pComPkt;
	if(IS_COM_CURRENT_VERSION(comproto_ver)){					// check communication protocol version(backward compatibility)
		// current version
		if(NULL == (pComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_N2_STATUS_CHANGE))){
			ERR_CHMPRN("Could not allocate memory for COMPKT.");
			return false;
		}

		// compkt
		PPXCOM_ALL				pComAll			= CVT_COM_ALL_PTR_PXCOMPKT(pComPkt);
		PPXCOM_N2_STATUS_CHANGE	pStatusChange	= CVT_COMPTR_N2_STATUS_CHANGE(pComAll);

		// loop: send to slaves
		for(chmpxidlist_t::const_iterator iter = chmpxidlist.begin(); iter != chmpxidlist.end(); ++iter){
			// Slave chmpxid list has server mode chmpxid.
			// Because the server connects other server for RING as SLAVE.
			// Then if server chmpxid, skip it.
			//
			if(pImData->IsServerChmpxId(*iter)){
				continue;
			}

			// set status change struct data
			SET_PXCOMPKT(pComPkt, COM_VERSION_2, CHMPX_COM_N2_STATUS_CHANGE, selfchmpxid, (*iter), true, 0L);

			pStatusChange->head.type	= CHMPX_COM_N2_STATUS_CHANGE;
			pStatusChange->head.result	= CHMPX_COM_RES_SUCCESS;
			pStatusChange->head.length	= sizeof(PXCOM_N2_STATUS_CHANGE);
			COPY_PCHMPXSVR(&(pStatusChange->server), pserver);

			// send
			if(!Send(pComPkt, NULL, 0L)){
				ERR_CHMPRN("Failed to send CHMPX_COM_N2_STATUS_CHANGE to slave chmpxid(0x%016" PRIx64 "), but continue...", *iter);
			}
		}

	}else{
		// old version
		if(NULL == (pComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_STATUS_CHANGE))){
			ERR_CHMPRN("Could not allocate memory for COMPKT.");
			return false;
		}

		// compkt
		PPXCOM_ALL				pComAll			= CVT_COM_ALL_PTR_PXCOMPKT(pComPkt);
		PPXCOM_STATUS_CHANGE	pStatusChange	= CVT_COMPTR_STATUS_CHANGE(pComAll);

		// loop: send to slaves
		for(chmpxidlist_t::const_iterator iter = chmpxidlist.begin(); iter != chmpxidlist.end(); ++iter){
			// Slave chmpxid list has server mode chmpxid.
			// Because the server connects other server for RING as SLAVE.
			// Then if server chmpxid, skip it.
			//
			if(pImData->IsServerChmpxId(*iter)){
				continue;
			}

			// set status change struct data
			SET_PXCOMPKT(pComPkt, COM_VERSION_1, CHMPX_COM_STATUS_CHANGE, selfchmpxid, (*iter), true, 0L);

			pStatusChange->head.type	= CHMPX_COM_STATUS_CHANGE;
			pStatusChange->head.result	= CHMPX_COM_RES_SUCCESS;
			pStatusChange->head.length	= sizeof(PXCOM_STATUS_CHANGE);
			CVT_PCHMPXSVR_TO_PCHMPXSVRV1(&(pStatusChange->server), pserver);

			// send
			if(!Send(pComPkt, NULL, 0L)){
				ERR_CHMPRN("Failed to send CHMPX_COM_STATUS_CHANGE to slave chmpxid(0x%016" PRIx64 "), but continue...", *iter);
			}
		}
	}
	CHM_Free(pComPkt);

	return true;
}

bool ChmEventSock::PxComReceiveStatusChange(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt)
{
	if(!pComHead || !pComAll || !ppResComPkt){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*		pImData		= pChmCntrl->GetImDataObj();
	chmpxid_t		selfchmpxid	= pImData->GetSelfChmpxId();
	*ppResComPkt				= NULL;

	if(pComHead->dept_ids.chmpxid == selfchmpxid){
		// around
		//
		PPXCOM_HEAD	pPxComHead = CVT_COMPTR_HEAD(pComAll);

		bool	result	= true;
		bool	is_retry= false;
		if(CHMPX_COM_RES_SUCCESS != pPxComHead->result){
			// Failed change status
			WAN_CHMPRN("PXCOM_N2_STATUS_CHANGE(or PXCOM_STATUS_CHANGE) is failed, try to recover status");
			result	= false;
			is_retry= true;
		}else{
			// Succeed change status
			MSG_CHMPRN("PXCOM_N2_STATUS_CHANGE(or PXCOM_STATUS_CHANGE) is around the RING with success.");

			// get chmpx data in packet
			CHMPXSVR	tgchmpxsvr_tmp;
			PCHMPXSVR	ptgchmpxsvr;
			if(IS_COM_CURRENT_VERSION(pComHead->version)){				// check communication protocol version(backward compatibility)
				// current version
				PPXCOM_N2_STATUS_CHANGE	pStatusChange	= CVT_COMPTR_N2_STATUS_CHANGE(pComAll);
				ptgchmpxsvr								= &(pStatusChange->server);
			}else{
				// old version
				PPXCOM_STATUS_CHANGE	pStatusChange	= CVT_COMPTR_STATUS_CHANGE(pComAll);
				ptgchmpxsvr								= &tgchmpxsvr_tmp;

				// convert CHMPXSVRV1 to CHMPXSVR
				CVT_PCHMPXSVRV1_TO_PCHMPXSVR(ptgchmpxsvr, &(pStatusChange->server));
			}

			// check chmpxid in packet is own
			if(selfchmpxid == ptgchmpxsvr->chmpxid){
				// check for processing to auto merge "done"
				if(	IS_CHMPXSTS_SRVIN(ptgchmpxsvr->status)	&&
					IS_CHMPXSTS_UP(ptgchmpxsvr->status)		&&
					IS_CHMPXSTS_DONE(ptgchmpxsvr->status)	&&
					IS_CHMPXSTS_NOSUP(ptgchmpxsvr->status)	&&
					IsAutoMerge()							&&
					is_merge_done_processing				)		// set in ChmEventSock::MergeDone()
				{
					MSG_CHMPRN("Start to merge complete automatically.");

					if(!RequestMergeComplete()){
						ERR_CHMPRN("Could not change status merge \"COMPLETE\", probably another server does not change status yet, hope to recover automatic or DO COMPMERGE BY MANUAL!");
						if(is_merge_done_processing){
							is_retry = true;
						}
					}
				}
			}
		}

		if(is_retry){
			// Update self last status updating time
			if(!pImData->UpdateSelfLastStatusTime()){
				ERR_CHMPRN("Could not update own updating status time, but continue...");
			}
			chmpxid_t	nextchmpxid = GetNextRingChmpxId();
			CHMPXSVR	selfchmpxsvr;
			if(!pImData->GetSelfChmpxSvr(&selfchmpxsvr)){
				ERR_CHMPRN("Could not get self chmpx information for retry updating status, then could not recover...");
			}else{
				if(CHM_INVALID_CHMPXID == nextchmpxid){
					// no server found, finish doing so pending hash updated self.
					if(!PxComSendSlavesStatusChange(&selfchmpxsvr)){
						ERR_CHMPRN("Failed to send self status change to slaves, then could not recover...");
					}
				}else{
					if(!PxComSendStatusChange(nextchmpxid, &selfchmpxsvr)){
						ERR_CHMPRN("Failed to send self status change, then could not recover...");
					}
				}
			}
		}
		if(!result){
			return false;
		}

	}else{
		// update own chmshm & transfer packet.
		//
		CHMPXSVR	chmpxsvr_tmp;
		PCHMPXSVR	pchmpxsvr;
		pxcomres_t	ReceiveResultCode = CHMPX_COM_RES_SUCCESS;
		if(IS_COM_CURRENT_VERSION(pComHead->version)){				// check communication protocol version(backward compatibility)
			// current version
			PPXCOM_N2_STATUS_CHANGE	pStatusChange	= CVT_COMPTR_N2_STATUS_CHANGE(pComAll);
			pchmpxsvr								= &(pStatusChange->server);

			if(CHMPX_COM_RES_SUCCESS != pStatusChange->head.result){
				MSG_CHMPRN("Already error occured before this server.");
				ReceiveResultCode = CHMPX_COM_RES_ERROR;
			}
		}else{
			// old version
			PPXCOM_STATUS_CHANGE	pStatusChange	= CVT_COMPTR_STATUS_CHANGE(pComAll);
			pchmpxsvr								= &chmpxsvr_tmp;

			// convert CHMPXSVRV1 to CHMPXSVR
			CVT_PCHMPXSVRV1_TO_PCHMPXSVR(pchmpxsvr, &(pStatusChange->server));

			if(CHMPX_COM_RES_SUCCESS != pStatusChange->head.result){
				MSG_CHMPRN("Already error occured before this server.");
				ReceiveResultCode = CHMPX_COM_RES_ERROR;
			}
		}

		// update chmshm
		pxcomres_t	ResultCode;
		if(!pImData->MergeChmpxSvrs(pchmpxsvr, 1, false, false, eqfd)){	// not remove other
			// error occured, so transfer packet.
			ERR_CHMPRN("Could not update server chmpx information.");
			ResultCode = CHMPX_COM_RES_ERROR;
		}else{
			// succeed.
			ResultCode = CHMPX_COM_RES_SUCCESS;
		}
		if(CHMPX_COM_RES_SUCCESS != ReceiveResultCode){
			MSG_CHMPRN("Already error occured before this server.");
			ResultCode = CHMPX_COM_RES_ERROR;
		}

		// [NOTICE]
		// This N2_STATUS_CHANGE(or STATUS_CHANGE) is only received by Slave and Server.
		// If server mode, transfer packet to RING, if slave mode, do not transfer it.
		//
		if(is_server_mode){
			// always, new updated status to send to slaves
			//
			if(!PxComSendSlavesStatusChange(pchmpxsvr)){
				ERR_CHMPRN("Failed to send CHMPX_COM_N2_STATUS_CHANGE(or CHMPX_COM_STATUS_CHANGE) to slaves, but continue...");
			}

			// check & rechain
			//
			bool	is_rechain = false;
			if(!CheckRechainRing(pchmpxsvr->chmpxid, is_rechain)){
				ERR_CHMPRN("Something error occured in rechaining RING, but continue...");
				ResultCode = CHMPX_COM_RES_ERROR;
			}else{
				if(is_rechain){
					MSG_CHMPRN("Rechained RING after joining chmpxid(0x%016" PRIx64 ").", pchmpxsvr->chmpxid);
				}else{
					MSG_CHMPRN("Not rechained RING after joining chmpxid(0x%016" PRIx64 ").", pchmpxsvr->chmpxid);
				}
			}

			// next chmpxid(after rechaining)
			chmpxid_t	nextchmpxid	= GetNextRingChmpxId();
			if(CHM_INVALID_CHMPXID == nextchmpxid){
				ERR_CHMPRN("Could not get next server chmpxid.");
				return false;
			}

			if(!IsSafeDeptAndNextChmpxId(pComHead->dept_ids.chmpxid, nextchmpxid)){
				// Departure chmpx maybe DOWN!
				//
				// [NOTICE]
				// This case is very small case, departure server sends this packet but that server could not
				// be connected from in RING server.(ex. down after sending, or FQDN is wrong, etc)
				// So we do not transfer this packet, because it could not be stopped in RING.
				//
				ERR_CHMPRN("Departure chmpxid(0x%016" PRIx64 ") maybe down, so stop transferring this packet.", pComHead->dept_ids.chmpxid);
				*ppResComPkt = NULL;

			}else{
				// make response data buffer
				PCOMPKT	pResComPkt;
				if(IS_COM_CURRENT_VERSION(pComHead->version)){				// check communication protocol version(backward compatibility)
					// current version
					if(NULL == (pResComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_N2_STATUS_CHANGE))){
						ERR_CHMPRN("Could not allocation memory.");
						return false;
					}

					// compkt
					PPXCOM_ALL				pResComAll		= CVT_COM_ALL_PTR_PXCOMPKT(pResComPkt);
					PPXCOM_N2_STATUS_CHANGE	pResStatusChange= CVT_COMPTR_N2_STATUS_CHANGE(pResComAll);

					SET_PXCOMPKT(pResComPkt, COM_VERSION_2, CHMPX_COM_N2_STATUS_CHANGE, pComHead->dept_ids.chmpxid, nextchmpxid, false, 0L);	// dept chmpxid is not changed
					COPY_TIMESPEC(&(pResComPkt->head.reqtime), &(pComHead->reqtime));	// not change

					// change_status(copy)
					pResStatusChange->head.type			= CHMPX_COM_N2_STATUS_CHANGE;
					pResStatusChange->head.result		= ResultCode;
					pResStatusChange->head.length		= sizeof(PXCOM_N2_STATUS_CHANGE);
					COPY_PCHMPXSVR(&(pResStatusChange->server), pchmpxsvr);

				}else{
					// old version
					if(NULL == (pResComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_STATUS_CHANGE))){
						ERR_CHMPRN("Could not allocation memory.");
						return false;
					}

					// compkt
					PPXCOM_ALL				pResComAll		= CVT_COM_ALL_PTR_PXCOMPKT(pResComPkt);
					PPXCOM_STATUS_CHANGE	pResStatusChange= CVT_COMPTR_STATUS_CHANGE(pResComAll);

					SET_PXCOMPKT(pResComPkt, COM_VERSION_1, CHMPX_COM_STATUS_CHANGE, pComHead->dept_ids.chmpxid, nextchmpxid, false, 0L);	// dept chmpxid is not changed
					COPY_TIMESPEC(&(pResComPkt->head.reqtime), &(pComHead->reqtime));	// not change

					// change_status(copy)
					pResStatusChange->head.type			= CHMPX_COM_STATUS_CHANGE;
					pResStatusChange->head.result		= ResultCode;
					pResStatusChange->head.length		= sizeof(PXCOM_STATUS_CHANGE);
					CVT_PCHMPXSVR_TO_PCHMPXSVRV1(&(pResStatusChange->server), pchmpxsvr);
				}
				*ppResComPkt = pResComPkt;
			}
		}
	}
	return true;
}

bool ChmEventSock::PxComSendMergeStart(chmpxid_t chmpxid)
{
	if(CHM_INVALID_CHMPXID == chmpxid){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// Make packet
	PCOMPKT	pComPkt;
	if(NULL == (pComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_MERGE_START))){
		ERR_CHMPRN("Could not allocate memory for COMPKT.");
		return false;
	}

	// compkt
	PPXCOM_ALL			pComAll		= CVT_COM_ALL_PTR_PXCOMPKT(pComPkt);
	PPXCOM_MERGE_START	pMergeStart	= CVT_COMPTR_MERGE_START(pComAll);
	SET_PXCOMPKT(pComPkt, COM_VERSION_2, CHMPX_COM_MERGE_START, pImData->GetSelfChmpxId(), chmpxid, true, 0L);

	pMergeStart->head.type		= CHMPX_COM_MERGE_START;
	pMergeStart->head.result	= CHMPX_COM_RES_SUCCESS;
	pMergeStart->head.length	= sizeof(PXCOM_MERGE_START);

	// Send request
	if(!Send(pComPkt, NULL, 0L)){
		ERR_CHMPRN("Failed to send CHMPX_COM_MERGE_START to chmpxid(0x%016" PRIx64 ").", chmpxid);
		CHM_Free(pComPkt);
		return false;
	}
	CHM_Free(pComPkt);

	return true;
}

bool ChmEventSock::PxComReceiveMergeStart(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt)
{
	if(!pComHead || !pComAll || !ppResComPkt){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*			pImData		= pChmCntrl->GetImDataObj();
	PPXCOM_MERGE_START	pMergeStart	= CVT_COMPTR_MERGE_START(pComAll);
	chmpxid_t			selfchmpxid	= pImData->GetSelfChmpxId();
	*ppResComPkt					= NULL;

	if(pComHead->dept_ids.chmpxid == selfchmpxid){
		// around -> next step: do merging
		//
		if(CHMPX_COM_RES_SUCCESS == pMergeStart->head.result){
			// Succeed
			MSG_CHMPRN("PXCOM_MERGE_START is succeed, All server start to merge.");

			// If there is DELETE Pending server, do DELETE done status here!
			chmpxid_t	nextchmpxid = GetNextRingChmpxId();
			if(CHM_INVALID_CHMPXID == nextchmpxid){
				MSG_CHMPRN("Could not get next chmpxid, probably there is no server without self chmpx on RING.");
			}
			chmpxid_t	downchmpxid;
			while(CHM_INVALID_CHMPXID != (downchmpxid = pImData->GetChmpxIdByStatus(CHMPXSTS_SRVIN_DOWN_DELPENDING, false))){
				// change down server status
				if(!pImData->SetServerStatus(downchmpxid, CHMPXSTS_SRVIN_DOWN_DELETED)){
					ERR_CHMPRN("Failed to change server(0x%016" PRIx64 ") status to CHMPXSTS_SRVIN_DOWN_DELETED.", downchmpxid);
					return false;
				}
				// send down server status update
				CHMPXSVR	chmpxsvr;
				if(!pImData->GetChmpxSvr(downchmpxid, &chmpxsvr)){
					ERR_CHMPRN("Could not get server(0x%016" PRIx64 ") information.", downchmpxid);
					return false;
				}
				if(CHM_INVALID_CHMPXID == nextchmpxid){
					if(!PxComSendSlavesStatusChange(&chmpxsvr)){
						ERR_CHMPRN("Failed to send server(0x%016" PRIx64 ") status change to slaves.", downchmpxid);
						return false;
					}
				}else{
					if(!PxComSendStatusChange(nextchmpxid, &chmpxsvr)){
						ERR_CHMPRN("Failed to send server(0x%016" PRIx64 ") status change.", downchmpxid);
						return false;
					}
				}
			}
		}else{
			// Something error occured on RING.
			ERR_CHMPRN("PXCOM_MERGE_START is failed, could not recover, but returns true.");
			return true;
		}
	}else{
		// update own chmshm & transfer packet.
		//
		chmpxid_t	nextchmpxid	= GetNextRingChmpxId();
		if(CHM_INVALID_CHMPXID == nextchmpxid){
			ERR_CHMPRN("Could not get next server chmpxid.");
			return false;
		}

		if(!IsSafeDeptAndNextChmpxId(pComHead->dept_ids.chmpxid, nextchmpxid)){
			// Departure chmpx maybe DOWN!
			//
			// [NOTICE]
			// This case is very small case, departure server sends this packet but that server could not
			// be connected from in RING server.(ex. down after sending, or FQDN is wrong, etc)
			// So we do not transfer this packet, because it could not be stopped in RING.
			//
			ERR_CHMPRN("Departure chmpxid(0x%016" PRIx64 ") maybe down, so stop transferring this packet.", pComHead->dept_ids.chmpxid);
			*ppResComPkt = NULL;

		}else{
			// make response data buffer
			PCOMPKT	pResComPkt;
			if(NULL == (pResComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_MERGE_START))){
				ERR_CHMPRN("Could not allocation memory.");
				return false;
			}

			// compkt
			PPXCOM_ALL			pResComAll		= CVT_COM_ALL_PTR_PXCOMPKT(pResComPkt);
			PPXCOM_MERGE_START	pResMergeStart	= CVT_COMPTR_MERGE_START(pResComAll);

			SET_PXCOMPKT(pResComPkt, COM_VERSION_2, CHMPX_COM_MERGE_START, pComHead->dept_ids.chmpxid, nextchmpxid, false, 0L);	// dept chmpxid is not changed.
			COPY_TIMESPEC(&(pResComPkt->head.reqtime), &(pComHead->reqtime));	// not change

			// merge_start(copy)
			pResMergeStart->head.type			= pMergeStart->head.type;
			pResMergeStart->head.result			= pMergeStart->head.result;		// always no error.
			pResMergeStart->head.length			= pMergeStart->head.length;

			if(CHMPX_COM_RES_SUCCESS != pMergeStart->head.result){
				// already error occured, so ignore this packet.
				WAN_CHMPRN("Already error occured before this server, but doing.");
			}
			*ppResComPkt = pResComPkt;
		}
	}
	return true;
}

bool ChmEventSock::PxComSendMergeAbort(chmpxid_t chmpxid)
{
	if(CHM_INVALID_CHMPXID == chmpxid){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// Make packet
	PCOMPKT	pComPkt;
	if(NULL == (pComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_MERGE_ABORT))){
		ERR_CHMPRN("Could not allocate memory for COMPKT.");
		return false;
	}

	// compkt
	PPXCOM_ALL			pComAll		= CVT_COM_ALL_PTR_PXCOMPKT(pComPkt);
	PPXCOM_MERGE_ABORT	pMergeAbort	= CVT_COMPTR_MERGE_ABORT(pComAll);
	SET_PXCOMPKT(pComPkt, COM_VERSION_2, CHMPX_COM_MERGE_ABORT, pImData->GetSelfChmpxId(), chmpxid, true, 0L);

	pMergeAbort->head.type		= CHMPX_COM_MERGE_ABORT;
	pMergeAbort->head.result	= CHMPX_COM_RES_SUCCESS;
	pMergeAbort->head.length	= sizeof(PXCOM_MERGE_ABORT);

	// Send request
	if(!Send(pComPkt, NULL, 0L)){
		ERR_CHMPRN("Failed to send CHMPX_COM_MERGE_ABORT to chmpxid(0x%016" PRIx64 ").", chmpxid);
		CHM_Free(pComPkt);
		return false;
	}
	CHM_Free(pComPkt);

	return true;
}

bool ChmEventSock::PxComReceiveMergeAbort(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt)
{
	if(!pComHead || !pComAll || !ppResComPkt){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*			pImData		= pChmCntrl->GetImDataObj();
	PPXCOM_MERGE_ABORT	pMergeAbort	= CVT_COMPTR_MERGE_ABORT(pComAll);
	chmpxid_t			selfchmpxid	= pImData->GetSelfChmpxId();
	*ppResComPkt					= NULL;

	if(pComHead->dept_ids.chmpxid == selfchmpxid){
		// around -> next step: do merging
		//
		if(CHMPX_COM_RES_SUCCESS == pMergeAbort->head.result){
			// Succeed
			MSG_CHMPRN("PXCOM_MERGE_ABORT is succeed, All server abort to merge.");
		}else{
			// Something error occured on RING.
			ERR_CHMPRN("PXCOM_MERGE_ABORT is failed, could not recover, but returns true.");
			return true;
		}
	}else{
		// update own chmshm & transfer packet.
		//
		chmpxid_t	nextchmpxid	= GetNextRingChmpxId();
		if(CHM_INVALID_CHMPXID == nextchmpxid){
			ERR_CHMPRN("Could not get next server chmpxid.");
			return false;
		}

		if(!IsSafeDeptAndNextChmpxId(pComHead->dept_ids.chmpxid, nextchmpxid)){
			// Departure chmpx maybe DOWN!
			//
			// [NOTICE]
			// This case is very small case, departure server sends this packet but that server could not
			// be connected from in RING server.(ex. down after sending, or FQDN is wrong, etc)
			// So we do not transfer this packet, because it could not be stopped in RING.
			//
			ERR_CHMPRN("Departure chmpxid(0x%016" PRIx64 ") maybe down, so stop transferring this packet.", pComHead->dept_ids.chmpxid);
			*ppResComPkt = NULL;

		}else{
			// make response data buffer
			PCOMPKT	pResComPkt;
			if(NULL == (pResComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_MERGE_ABORT))){
				ERR_CHMPRN("Could not allocation memory.");
				return false;
			}

			// compkt
			PPXCOM_ALL			pResComAll		= CVT_COM_ALL_PTR_PXCOMPKT(pResComPkt);
			PPXCOM_MERGE_ABORT	pResMergeAbort	= CVT_COMPTR_MERGE_ABORT(pResComAll);

			SET_PXCOMPKT(pResComPkt, COM_VERSION_2, CHMPX_COM_MERGE_ABORT, pComHead->dept_ids.chmpxid, nextchmpxid, false, 0L);	// dept chmpxid is not changed.
			COPY_TIMESPEC(&(pResComPkt->head.reqtime), &(pComHead->reqtime));	// not change

			// merge_start(copy)
			pResMergeAbort->head.type		= pMergeAbort->head.type;
			pResMergeAbort->head.result		= pMergeAbort->head.result;			// always no error.
			pResMergeAbort->head.length		= pMergeAbort->head.length;

			if(CHMPX_COM_RES_SUCCESS != pMergeAbort->head.result){
				// already error occured, so ignore this packet.
				WAN_CHMPRN("Already error occured before this server, but doing.");
			}
			*ppResComPkt = pResComPkt;
		}
	}
	return true;
}

bool ChmEventSock::PxComSendMergeComplete(chmpxid_t chmpxid)
{
	if(CHM_INVALID_CHMPXID == chmpxid){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// Make packet
	PCOMPKT	pComPkt;
	if(NULL == (pComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_MERGE_COMPLETE))){
		ERR_CHMPRN("Could not allocate memory for COMPKT.");
		return false;
	}

	// compkt
	PPXCOM_ALL				pComAll			= CVT_COM_ALL_PTR_PXCOMPKT(pComPkt);
	PPXCOM_MERGE_COMPLETE	pMergeComplete	= CVT_COMPTR_MERGE_COMPLETE(pComAll);
	SET_PXCOMPKT(pComPkt, COM_VERSION_2, CHMPX_COM_MERGE_COMPLETE, pImData->GetSelfChmpxId(), chmpxid, true, 0L);

	pMergeComplete->head.type	= CHMPX_COM_MERGE_COMPLETE;
	pMergeComplete->head.result	= CHMPX_COM_RES_SUCCESS;
	pMergeComplete->head.length	= sizeof(PXCOM_MERGE_COMPLETE);

	// Send request
	if(!Send(pComPkt, NULL, 0L)){
		ERR_CHMPRN("Failed to send CHMPX_COM_MERGE_COMPLETE to chmpxid(0x%016" PRIx64 ").", chmpxid);
		CHM_Free(pComPkt);
		return false;
	}
	CHM_Free(pComPkt);

	return true;
}

bool ChmEventSock::PxComReceiveMergeComplete(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt, bool& is_completed)
{
	is_completed = false;

	if(!pComHead || !pComAll || !ppResComPkt){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*				pImData			= pChmCntrl->GetImDataObj();
	PPXCOM_MERGE_COMPLETE	pMergeComplete	= CVT_COMPTR_MERGE_COMPLETE(pComAll);
	chmpxid_t				selfchmpxid		= pImData->GetSelfChmpxId();
	*ppResComPkt							= NULL;

	if(pComHead->dept_ids.chmpxid == selfchmpxid){
		// around -> next step: do merging
		//
		if(CHMPX_COM_RES_SUCCESS == pMergeComplete->head.result){
			// Succeed
			MSG_CHMPRN("PXCOM_MERGE_COMPLETE is succeed, All server start to merge.");
			is_completed = true;

			// If there is DELETE Done server, do SERVICEOUT status here!
			chmpxid_t	nextchmpxid = GetNextRingChmpxId();
			if(CHM_INVALID_CHMPXID == nextchmpxid){
				MSG_CHMPRN("Could not get next chmpxid, probably there is no server without self chmpx on RING.");
			}

			chmpxid_t	downchmpxid;
			while(CHM_INVALID_CHMPXID != (downchmpxid = pImData->GetChmpxIdByStatus(CHMPXSTS_SRVIN_DOWN_DELETED, false))){
				// change down server status
				if(!pImData->SetServerStatus(downchmpxid, CHMPXSTS_SRVOUT_DOWN_NORMAL)){
					ERR_CHMPRN("Failed to change server(0x%016" PRIx64 ") status to CHMPXSTS_SRVOUT_DOWN_NORMAL.", downchmpxid);
					return false;
				}
				// send down server status update
				CHMPXSVR	chmpxsvr;
				if(!pImData->GetChmpxSvr(downchmpxid, &chmpxsvr)){
					ERR_CHMPRN("Could not get server(0x%016" PRIx64 ") information.", downchmpxid);
					return false;
				}
				if(CHM_INVALID_CHMPXID == nextchmpxid){
					if(!PxComSendSlavesStatusChange(&chmpxsvr)){
						ERR_CHMPRN("Failed to send server(0x%016" PRIx64 ") status change to slaves.", downchmpxid);
						return false;
					}
				}else{
					if(!PxComSendStatusChange(nextchmpxid, &chmpxsvr)){
						ERR_CHMPRN("Failed to send server(0x%016" PRIx64 ") status change.", downchmpxid);
						return false;
					}
				}
			}
		}else{
			// Something error occured on RING.
			WAN_CHMPRN("PXCOM_MERGE_COMPLETE is failed, maybe other servers are merging now, thus returns true for retry.");
			return true;
		}
	}else{
		// update own chmshm & transfer packet.
		//
		chmpxid_t	nextchmpxid	= GetNextRingChmpxId();
		if(CHM_INVALID_CHMPXID == nextchmpxid){
			ERR_CHMPRN("Could not get next server chmpxid.");
			return false;
		}

		if(!IsSafeDeptAndNextChmpxId(pComHead->dept_ids.chmpxid, nextchmpxid)){
			// Departure chmpx maybe DOWN!
			//
			// [NOTICE]
			// This case is very small case, departure server sends this packet but that server could not
			// be connected from in RING server.(ex. down after sending, or FQDN is wrong, etc)
			// So we do not transfer this packet, because it could not be stopped in RING.
			//
			ERR_CHMPRN("Departure chmpxid(0x%016" PRIx64 ") maybe down, so stop transferring this packet.", pComHead->dept_ids.chmpxid);
			*ppResComPkt = NULL;

		}else{
			// make response data buffer
			PCOMPKT	pResComPkt;
			if(NULL == (pResComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_MERGE_COMPLETE))){
				ERR_CHMPRN("Could not allocation memory.");
				return false;
			}

			// compkt
			PPXCOM_ALL				pResComAll			= CVT_COM_ALL_PTR_PXCOMPKT(pResComPkt);
			PPXCOM_MERGE_COMPLETE	pResMergeComplete	= CVT_COMPTR_MERGE_COMPLETE(pResComAll);

			SET_PXCOMPKT(pResComPkt, COM_VERSION_2, CHMPX_COM_MERGE_COMPLETE, pComHead->dept_ids.chmpxid, nextchmpxid, false, 0L);	// dept chmpxid is not changed.
			COPY_TIMESPEC(&(pResComPkt->head.reqtime), &(pComHead->reqtime));	// not change

			// merge_start(copy)
			pResMergeComplete->head.type		= pMergeComplete->head.type;
			pResMergeComplete->head.result		= pMergeComplete->head.result;	// always no error.
			pResMergeComplete->head.length		= pMergeComplete->head.length;

			if(CHMPX_COM_RES_SUCCESS != pMergeComplete->head.result){
				// already error occured, so ignore this packet.
				WAN_CHMPRN("Already error occured before this server, but doing.");
				pResMergeComplete->head.result = pMergeComplete->head.result;
			}else{
				// check self status
				chmpxsts_t	status = pImData->GetSelfStatus();
				if(IS_CHMPXSTS_SRVIN(status)){
					if(IS_CHMPXSTS_UP(status)){
						if(IS_CHMPXSTS_NOACT(status)){
							if(IS_CHMPXSTS_SUSPEND(status)){
								WAN_CHMPRN("Server status(0x%016" PRIx64 ":%s) is SUSPEND, so could not change status.", status, STR_CHMPXSTS_FULL(status).c_str());
								pResMergeComplete->head.result = CHMPX_COM_RES_ERROR;
							}else if(IS_CHMPXSTS_DOING(status)){
								WAN_CHMPRN("Server status(0x%016" PRIx64 ":%s) is DOING yet, so could not change status.", status, STR_CHMPXSTS_FULL(status).c_str());
								pResMergeComplete->head.result = CHMPX_COM_RES_ERROR;
							}else{
								is_completed = true;
							}

						}else if(IS_CHMPXSTS_ADD(status)){
							if(!IS_CHMPXSTS_PENDING(status) && IS_CHMPXSTS_SUSPEND(status)){	// only suspend status is set to PENDING, it is allowed
								WAN_CHMPRN("Server status(0x%016" PRIx64 ":%s) is SUSPEND, so could not change status.", status, STR_CHMPXSTS_FULL(status).c_str());
								pResMergeComplete->head.result = CHMPX_COM_RES_ERROR;
							}else if(IS_CHMPXSTS_DOING(status)){
								WAN_CHMPRN("Server status(0x%016" PRIx64 ":%s) is DOING yet, so could not change status.", status, STR_CHMPXSTS_FULL(status).c_str());
								pResMergeComplete->head.result = CHMPX_COM_RES_ERROR;
							}else{
								is_completed = true;
							}

						}else if(IS_CHMPXSTS_DELETE(status)){
							if(!IS_CHMPXSTS_DONE(status) && IS_CHMPXSTS_SUSPEND(status)){		// DONE and SUSPEND is allowed
								WAN_CHMPRN("Server status(0x%016" PRIx64 ":%s) is SUSPEND, so could not change status.", status, STR_CHMPXSTS_FULL(status).c_str());
								pResMergeComplete->head.result = CHMPX_COM_RES_ERROR;
							}else if(IS_CHMPXSTS_DOING(status)){
								WAN_CHMPRN("Server status(0x%016" PRIx64 ":%s) is DOING yet, so could not change status.", status, STR_CHMPXSTS_FULL(status).c_str());
								pResMergeComplete->head.result = CHMPX_COM_RES_ERROR;
							}else{
								is_completed = true;
							}
						}
					}else if(IS_CHMPXSTS_DOWN(status)){
						// Allow all DOWN status
						is_completed = true;
					}
				}else{
					// Not care for SERVICE OUT servers
					is_completed = true;
				}
			}
			*ppResComPkt = pResComPkt;
		}
	}
	return true;
}

bool ChmEventSock::PxComSendMergeSuspend(chmpxid_t chmpxid)
{
	if(CHM_INVALID_CHMPXID == chmpxid){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// Make packet
	PCOMPKT	pComPkt;
	if(NULL == (pComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_MERGE_SUSPEND))){
		ERR_CHMPRN("Could not allocate memory for COMPKT.");
		return false;
	}

	// compkt
	PPXCOM_ALL				pComAll			= CVT_COM_ALL_PTR_PXCOMPKT(pComPkt);
	PPXCOM_MERGE_SUSPEND	pMergeSuspend	= CVT_COMPTR_MERGE_SUSPEND(pComAll);
	SET_PXCOMPKT(pComPkt, COM_VERSION_2, CHMPX_COM_MERGE_SUSPEND, pImData->GetSelfChmpxId(), chmpxid, true, 0L);

	pMergeSuspend->head.type	= CHMPX_COM_MERGE_SUSPEND;
	pMergeSuspend->head.result	= CHMPX_COM_RES_SUCCESS;
	pMergeSuspend->head.length	= sizeof(PXCOM_MERGE_SUSPEND);

	// Send request
	if(!Send(pComPkt, NULL, 0L)){
		ERR_CHMPRN("Failed to send CHMPX_COM_MERGE_SUSPEND to chmpxid(0x%016" PRIx64 ").", chmpxid);
		CHM_Free(pComPkt);
		return false;
	}
	CHM_Free(pComPkt);

	return true;
}

bool ChmEventSock::PxComReceiveMergeSuspend(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt)
{
	if(!pComHead || !pComAll || !ppResComPkt){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*				pImData			= pChmCntrl->GetImDataObj();
	PPXCOM_MERGE_SUSPEND	pMergeSuspend	= CVT_COMPTR_MERGE_SUSPEND(pComAll);
	chmpxid_t				selfchmpxid		= pImData->GetSelfChmpxId();
	*ppResComPkt							= NULL;

	if(pComHead->dept_ids.chmpxid == selfchmpxid){
		// around -> next step: do merging
		//
		if(CHMPX_COM_RES_SUCCESS == pMergeSuspend->head.result){
			// Succeed
			MSG_CHMPRN("PXCOM_MERGE_SUSPEND is succeed, All server suspend auto merging.");
		}else{
			// Error
			ERR_CHMPRN("PXCOM_MERGE_SUSPEND is failed, Some server could not suspend auto merging. please check all server.");
			return false;
		}
	}else{
		// update own chmshm & transfer packet.
		//
		chmpxid_t	nextchmpxid	= GetNextRingChmpxId();
		if(CHM_INVALID_CHMPXID == nextchmpxid){
			ERR_CHMPRN("Could not get next server chmpxid.");
			return false;
		}

		if(!IsSafeDeptAndNextChmpxId(pComHead->dept_ids.chmpxid, nextchmpxid)){
			// Departure chmpx maybe DOWN!
			//
			// [NOTICE]
			// This case is very small case, departure server sends this packet but that server could not
			// be connected from in RING server.(ex. down after sending, or FQDN is wrong, etc)
			// So we do not transfer this packet, because it could not be stopped in RING.
			//
			ERR_CHMPRN("Departure chmpxid(0x%016" PRIx64 ") maybe down, so stop transferring this packet.", pComHead->dept_ids.chmpxid);
			*ppResComPkt = NULL;

		}else{
			// make response data buffer
			PCOMPKT	pResComPkt;
			if(NULL == (pResComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_MERGE_SUSPEND))){
				ERR_CHMPRN("Could not allocation memory.");
				return false;
			}

			// compkt
			PPXCOM_ALL				pResComAll			= CVT_COM_ALL_PTR_PXCOMPKT(pResComPkt);
			PPXCOM_MERGE_SUSPEND	pResMergeSuspend	= CVT_COMPTR_MERGE_SUSPEND(pResComAll);

			SET_PXCOMPKT(pResComPkt, COM_VERSION_2, CHMPX_COM_MERGE_SUSPEND, pComHead->dept_ids.chmpxid, nextchmpxid, false, 0L);	// dept chmpxid is not changed.
			COPY_TIMESPEC(&(pResComPkt->head.reqtime), &(pComHead->reqtime));	// not change

			// merge_start(copy)
			pResMergeSuspend->head.type		= pMergeSuspend->head.type;
			pResMergeSuspend->head.result	= pMergeSuspend->head.result;		// always no error.
			pResMergeSuspend->head.length	= pMergeSuspend->head.length;

			if(CHMPX_COM_RES_SUCCESS != pMergeSuspend->head.result){
				// already error occured, so ignore this packet.
				WAN_CHMPRN("Already error occured before this server, but doing.");
				pResMergeSuspend->head.result = pMergeSuspend->head.result;
			}else{
				// Change suspend flag
				if(!pImData->SuspendAutoMerge()){
					WAN_CHMPRN("Could not change auto merging flag to suspend.");
					pResMergeSuspend->head.result = CHMPX_COM_RES_ERROR;
				}else if(startup_servicein){
					startup_servicein = false;
				}
			}
			*ppResComPkt = pResComPkt;
		}
	}
	return true;
}

bool ChmEventSock::PxComSendMergeNoSuspend(chmpxid_t chmpxid)
{
	if(CHM_INVALID_CHMPXID == chmpxid){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// Make packet
	PCOMPKT	pComPkt;
	if(NULL == (pComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_MERGE_NOSUSPEND))){
		ERR_CHMPRN("Could not allocate memory for COMPKT.");
		return false;
	}

	// compkt
	PPXCOM_ALL				pComAll			= CVT_COM_ALL_PTR_PXCOMPKT(pComPkt);
	PPXCOM_MERGE_NOSUSPEND	pMergeNoSuspend	= CVT_COMPTR_MERGE_NOSUSPEND(pComAll);
	SET_PXCOMPKT(pComPkt, COM_VERSION_2, CHMPX_COM_MERGE_NOSUSPEND, pImData->GetSelfChmpxId(), chmpxid, true, 0L);

	pMergeNoSuspend->head.type		= CHMPX_COM_MERGE_NOSUSPEND;
	pMergeNoSuspend->head.result	= CHMPX_COM_RES_SUCCESS;
	pMergeNoSuspend->head.length	= sizeof(PXCOM_MERGE_NOSUSPEND);

	// Send request
	if(!Send(pComPkt, NULL, 0L)){
		ERR_CHMPRN("Failed to send CHMPX_COM_MERGE_NOSUSPEND to chmpxid(0x%016" PRIx64 ").", chmpxid);
		CHM_Free(pComPkt);
		return false;
	}
	CHM_Free(pComPkt);

	return true;
}

bool ChmEventSock::PxComReceiveMergeNoSuspend(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt)
{
	if(!pComHead || !pComAll || !ppResComPkt){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*				pImData			= pChmCntrl->GetImDataObj();
	PPXCOM_MERGE_NOSUSPEND	pMergeNoSuspend	= CVT_COMPTR_MERGE_NOSUSPEND(pComAll);
	chmpxid_t				selfchmpxid		= pImData->GetSelfChmpxId();
	*ppResComPkt							= NULL;

	if(pComHead->dept_ids.chmpxid == selfchmpxid){
		// around -> next step: do merging
		//
		if(CHMPX_COM_RES_SUCCESS == pMergeNoSuspend->head.result){
			// Succeed
			MSG_CHMPRN("PXCOM_MERGE_NOSUSPEND is succeed, All server reset suspend auto merging.");
		}else{
			// Error
			ERR_CHMPRN("PXCOM_MERGE_NOSUSPEND is failed, Some server could not reset suspend auto merging. please check all server.");
			return false;
		}
	}else{
		// update own chmshm & transfer packet.
		//
		chmpxid_t	nextchmpxid	= GetNextRingChmpxId();
		if(CHM_INVALID_CHMPXID == nextchmpxid){
			ERR_CHMPRN("Could not get next server chmpxid.");
			return false;
		}

		if(!IsSafeDeptAndNextChmpxId(pComHead->dept_ids.chmpxid, nextchmpxid)){
			// Departure chmpx maybe DOWN!
			//
			// [NOTICE]
			// This case is very small case, departure server sends this packet but that server could not
			// be connected from in RING server.(ex. down after sending, or FQDN is wrong, etc)
			// So we do not transfer this packet, because it could not be stopped in RING.
			//
			ERR_CHMPRN("Departure chmpxid(0x%016" PRIx64 ") maybe down, so stop transferring this packet.", pComHead->dept_ids.chmpxid);
			*ppResComPkt = NULL;

		}else{
			// make response data buffer
			PCOMPKT	pResComPkt;
			if(NULL == (pResComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_MERGE_NOSUSPEND))){
				ERR_CHMPRN("Could not allocation memory.");
				return false;
			}

			// compkt
			PPXCOM_ALL				pResComAll			= CVT_COM_ALL_PTR_PXCOMPKT(pResComPkt);
			PPXCOM_MERGE_NOSUSPEND	pResMergeNoSuspend	= CVT_COMPTR_MERGE_NOSUSPEND(pResComAll);

			SET_PXCOMPKT(pResComPkt, COM_VERSION_2, CHMPX_COM_MERGE_NOSUSPEND, pComHead->dept_ids.chmpxid, nextchmpxid, false, 0L);	// dept chmpxid is not changed.
			COPY_TIMESPEC(&(pResComPkt->head.reqtime), &(pComHead->reqtime));	// not change

			// merge_start(copy)
			pResMergeNoSuspend->head.type	= pMergeNoSuspend->head.type;
			pResMergeNoSuspend->head.result	= pMergeNoSuspend->head.result;		// always no error.
			pResMergeNoSuspend->head.length	= pMergeNoSuspend->head.length;

			if(CHMPX_COM_RES_SUCCESS != pMergeNoSuspend->head.result){
				// already error occured, so ignore this packet.
				WAN_CHMPRN("Already error occured before this server, but doing.");
				pResMergeNoSuspend->head.result = pMergeNoSuspend->head.result;
			}else{
				// Change suspend flag
				if(!pImData->ResetAutoMerge()){
					WAN_CHMPRN("Could not change auto merging flag to not suspend.");
					pResMergeNoSuspend->head.result = CHMPX_COM_RES_ERROR;
				}
			}
			*ppResComPkt = pResComPkt;
		}
	}
	return true;
}

bool ChmEventSock::PxComSendMergeSuspendGet(int sock, chmpxid_t chmpxid)
{
	if(CHM_INVALID_SOCK == sock || CHM_INVALID_CHMPXID == chmpxid){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// Make packet
	PCOMPKT	pComPkt;
	if(NULL == (pComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_MERGE_SUSPEND_GET))){
		ERR_CHMPRN("Could not allocate memory for COMPKT.");
		return false;
	}

	// compkt
	PPXCOM_ALL					pComAll			= CVT_COM_ALL_PTR_PXCOMPKT(pComPkt);
	PPXCOM_MERGE_SUSPEND_GET	pMergeSuspendGet= CVT_COMPTR_MERGE_SUSPEND_GET(pComAll);
	SET_PXCOMPKT(pComPkt, COM_VERSION_2, CHMPX_COM_MERGE_SUSPEND_GET, pImData->GetSelfChmpxId(), chmpxid, true, 0L);

	pMergeSuspendGet->head.type		= CHMPX_COM_MERGE_SUSPEND_GET;
	pMergeSuspendGet->head.result	= CHMPX_COM_RES_SUCCESS;
	pMergeSuspendGet->head.length	= sizeof(PXCOM_MERGE_SUSPEND_GET);

	// Send request(direct)
	bool	is_closed = false;
	if(!ChmEventSock::RawSend(sock, GetSSL(sock), pComPkt, is_closed, false, sock_retry_count, sock_wait_time)){		// as default nonblocking
		ERR_CHMPRN("Failed to send CHMPX_COM_MERGE_SUSPEND_GET to sock(%d).", sock);

		// [NOTE]
		// This method is called from only InitialAllServerStatus(), then the sock is not mapped.
		// Then we do not close sock here, it will be closed in InitialAllServerStatus().
		//
		CHM_Free(pComPkt);
		return false;
	}
	CHM_Free(pComPkt);

	return true;
}

bool ChmEventSock::PxComReceiveMergeSuspendGet(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt)
{
	if(!pComHead || !pComAll || !ppResComPkt){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*					pImData			= pChmCntrl->GetImDataObj();
//	PPXCOM_MERGE_SUSPEND_GET	pMergeSuspendGet= CVT_COMPTR_MERGE_SUSPEND_GET(pComAll);
	*ppResComPkt								= NULL;

	// make response data
	PCOMPKT	pResComPkt;
	if(NULL == (pResComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_MERGE_SUSPEND_RES))){
		ERR_CHMPRN("Could not allocation memory.");
		return false;
	}

	// compkt
	PPXCOM_ALL					pResComAll		= CVT_COM_ALL_PTR_PXCOMPKT(pResComPkt);
	PPXCOM_MERGE_SUSPEND_RES	pMergeSuspendRes= CVT_COMPTR_MERGE_SUSPEND_RES(pResComAll);
	SET_PXCOMPKT(pResComPkt, COM_VERSION_2, CHMPX_COM_MERGE_SUSPEND_RES, pComHead->term_ids.chmpxid, pComHead->dept_ids.chmpxid, true, 0);	// switch chmpxid

	pMergeSuspendRes->head.type		= CHMPX_COM_MERGE_SUSPEND_RES;
	pMergeSuspendRes->head.result	= CHMPX_COM_RES_SUCCESS;
	pMergeSuspendRes->head.length	= sizeof(PXCOM_MERGE_SUSPEND_RES);
	pMergeSuspendRes->is_suspend	= pImData->GetAutoMergeMode();		// get merge suspend mode

	*ppResComPkt = pResComPkt;

	return true;
}

bool ChmEventSock::PxComReceiveMergeSuspendRes(PCOMHEAD pComHead, PPXCOM_ALL pComAll)
{
	if(!pComHead || !pComAll){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*					pImData			= pChmCntrl->GetImDataObj();
	PPXCOM_MERGE_SUSPEND_RES	pMergeSuspendRes= CVT_COMPTR_MERGE_SUSPEND_RES(pComAll);
	chmpxid_t					selfchmpxid		= pImData->GetSelfChmpxId();

	if(pComHead->term_ids.chmpxid == selfchmpxid){
		// To me
		if(CHMPX_COM_RES_SUCCESS != pMergeSuspendRes->head.result){
			WAN_CHMPRN("PXCOM_MERGE_SUSPEND_RES is failed, something wrong or target ring does not support this command.");
			return true;
		}

		// set suspend merging mode
		if(pMergeSuspendRes->is_suspend){
			MSG_CHMPRN("PXCOM_MERGE_SUSPEND_RES is received, and set suspending merging mode.");
			pImData->SuspendAutoMerge();
		}else{
			MSG_CHMPRN("PXCOM_MERGE_SUSPEND_RES is received, and reset suspending merging mode.");
			pImData->ResetAutoMerge();
		}

	}else{
		// To other chmpxid
		ERR_CHMPRN("Received PXCOM_MERGE_SUSPEND_RES packet, but terminal chmpxid(0x%016" PRIx64 ") is not self chmpxid(0x%016" PRIx64 ").", pComHead->term_ids.chmpxid, selfchmpxid);
		return false;
	}
	return true;
}

bool ChmEventSock::PxComSendServerDown(chmpxid_t chmpxid, chmpxid_t downchmpxid)
{
	if(CHM_INVALID_CHMPXID == chmpxid || CHM_INVALID_CHMPXID == downchmpxid){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// Make packet
	PCOMPKT	pComPkt;
	if(NULL == (pComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_SERVER_DOWN))){
		ERR_CHMPRN("Could not allocate memory for COMPKT.");
		return false;
	}

	// compkt
	PPXCOM_ALL			pComAll		= CVT_COM_ALL_PTR_PXCOMPKT(pComPkt);
	PPXCOM_SERVER_DOWN	pServerDown	= CVT_COMPTR_SERVER_DOWN(pComAll);
	SET_PXCOMPKT(pComPkt, COM_VERSION_2, CHMPX_COM_SERVER_DOWN, pImData->GetSelfChmpxId(), chmpxid, true, 0L);

	pServerDown->head.type		= CHMPX_COM_SERVER_DOWN;
	pServerDown->head.result	= CHMPX_COM_RES_SUCCESS;
	pServerDown->head.length	= sizeof(PXCOM_SERVER_DOWN);
	pServerDown->chmpxid		= downchmpxid;

	// Send request
	if(!Send(pComPkt, NULL, 0L)){
		ERR_CHMPRN("Failed to send CHMPX_COM_SERVER_DOWN to chmpxid(0x%016" PRIx64 ").", chmpxid);
		CHM_Free(pComPkt);
		return false;
	}
	CHM_Free(pComPkt);

	return true;
}

bool ChmEventSock::PxComReceiveServerDown(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt)
{
	if(!pComHead || !pComAll || !ppResComPkt){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*			pImData		= pChmCntrl->GetImDataObj();
	PPXCOM_SERVER_DOWN	pServerDown	= CVT_COMPTR_SERVER_DOWN(pComAll);
	chmpxid_t			selfchmpxid	= pImData->GetSelfChmpxId();
	*ppResComPkt					= NULL;

	if(pComHead->dept_ids.chmpxid == selfchmpxid){
		// around -> next step: update pending hash value
		//
		if(CHMPX_COM_RES_SUCCESS == pServerDown->head.result){
			// Succeed notify server down & status update on RING
			if(CHM_INVALID_CHMPXID == pServerDown->chmpxid){
				ERR_CHMPRN("PXCOM_SERVER_DOWN target chmpxid is something wrong.");
				return false;
			}
			// Check automerge
			if(IsAutoMerge()){
				// Request Service out
				if(!RequestServiceOut(pServerDown->chmpxid)){
					ERR_CHMPRN("Could not operate to SERVICEOUT by automerge for chmpxid(0x%016" PRIx64 ").", pServerDown->chmpxid);
					return false;
				}
			}
			return true;

		}else{
			// Something error occured on RING, but nothing to do for recovering.
			ERR_CHMPRN("PXCOM_SERVER_DOWN is failed, could not recover...");
			return false;
		}
	}else{
		// update own chmshm & transfer packet.
		//
		// [NOTICE]
		// Before receiving this "SERVER_DOWN", this server got "merge abort" if merging.
		// Thus do not care about merge abort.
		//
		pxcomres_t	ResultCode = CHMPX_COM_RES_SUCCESS;;

		// close down server sock (probably all sockets are already closed)
		socklist_t	socklist;
		if(pImData->GetServerSock(pServerDown->chmpxid, socklist) && !socklist.empty()){
			for(socklist_t::iterator iter = socklist.begin(); iter != socklist.end(); iter = socklist.erase(iter)){
				seversockmap.erase(*iter);
			}
		}

		// check status and set new status
		chmpxsts_t	status		= pImData->GetServerStatus(pServerDown->chmpxid);
		chmpxsts_t	old_status	= status;

		CHANGE_CHMPXSTS_TO_DOWN(status);
		if(status == old_status){
			MSG_CHMPRN("chmpxid(0x%016" PRIx64 ") already has status(0x%016" PRIx64 ":%s) by server down notify.", pServerDown->chmpxid, status, STR_CHMPXSTS_FULL(status).c_str());
		}else{
			MSG_CHMPRN("chmpxid(0x%016" PRIx64 ") is changed status(0x%016" PRIx64 ":%s) from old status(0x%016" PRIx64 ":%s) by server down notify.", pServerDown->chmpxid, status, STR_CHMPXSTS_FULL(status).c_str(), old_status, STR_CHMPXSTS_FULL(old_status).c_str());

			if(!pImData->SetServerStatus(pServerDown->chmpxid, status)){
				ERR_CHMPRN("Could not set status(0x%016" PRIx64 ":%s) to chmpxid(0x%016" PRIx64 "), but continue...", status, STR_CHMPXSTS_FULL(status).c_str(), pServerDown->chmpxid);
				ResultCode = CHMPX_COM_RES_ERROR;
			}
		}
		if(CHMPX_COM_RES_SUCCESS != pServerDown->head.result){
			MSG_CHMPRN("Already error occured before this server.");
			ResultCode = CHMPX_COM_RES_ERROR;
		}

		// get next chmpxid on RING
		chmpxid_t	nextchmpxid	= GetNextRingChmpxId();

		if(CHM_INVALID_CHMPXID != nextchmpxid){
			if(!IsSafeDeptAndNextChmpxId(pComHead->dept_ids.chmpxid, nextchmpxid)){
				// Departure chmpx maybe DOWN!
				//
				// [NOTICE]
				// This case is very small case, departure server sends this packet but that server could not
				// be connected from in RING server.(ex. down after sending, or FQDN is wrong, etc)
				// So we do not transfer this packet, because it could not be stopped in RING.
				//
				ERR_CHMPRN("Departure chmpxid(0x%016" PRIx64 ") maybe down, so stop transferring this packet.", pComHead->dept_ids.chmpxid);
				*ppResComPkt = NULL;

			}else{
				// make response data buffer
				PCOMPKT	pResComPkt;
				if(NULL == (pResComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_SERVER_DOWN))){
					ERR_CHMPRN("Could not allocation memory.");
					return false;
				}

				// compkt
				PPXCOM_ALL			pResComAll		= CVT_COM_ALL_PTR_PXCOMPKT(pResComPkt);
				PPXCOM_SERVER_DOWN	pResServerDown	= CVT_COMPTR_SERVER_DOWN(pResComAll);

				SET_PXCOMPKT(pResComPkt, COM_VERSION_2, CHMPX_COM_SERVER_DOWN, pComHead->dept_ids.chmpxid, nextchmpxid, false, 0L);	// dept chmpxid is not changed.
				COPY_TIMESPEC(&(pResComPkt->head.reqtime), &(pComHead->reqtime));	// not change

				// server_down(copy)
				pResServerDown->head.type			= pServerDown->head.type;
				pResServerDown->head.result			= ResultCode;
				pResServerDown->head.length			= pServerDown->head.length;
				pResServerDown->chmpxid				= pServerDown->chmpxid;

				*ppResComPkt = pResComPkt;
			}
		}
	}
	return true;
}

bool ChmEventSock::PxCltReceiveJoinNotify(PCOMHEAD pComHead, PPXCLT_ALL pComAll)
{
	if(!pComHead || !pComAll){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*			pImData		= pChmCntrl->GetImDataObj();
//	PPXCLT_JOIN_NOTIFY	pJoinNotify	= CVT_CLTPTR_JOIN_NOTIFY(pComAll);

	if(!pImData->IsChmpxProcess()){
		ERR_CHMPRN("CHMPX_CLT_JOIN_NOTIFY must be received on Chmpx process.");
		return false;
	}

	// check processes
	//
	// [MEMO]
	// We should check pJoinNotify->pid in pidlist, but we do not it.
	// After receiving this command, we only check client count and change status.
	// 
	if(!pImData->IsClientPids()){
		ERR_CHMPRN("There is no client process.");
		return false;
	}

	// Check status
	chmpxsts_t	status		= pImData->GetSelfStatus();
	chmpxsts_t	old_status	= status;
	if(IS_CHMPXSTS_NOSUP(status)){
		MSG_CHMPRN("Already self status(0x%016" PRIx64 ":%s) is NO SUSPEND.", status, STR_CHMPXSTS_FULL(status).c_str());
		return true;
	}

	// new status
	if(IS_CHMPXSTS_SRVIN(status) && IS_CHMPXSTS_UP(status) && IS_CHMPXSTS_NOACT(status) && IS_CHMPXSTS_NOTHING(status)){
		// [NOTICE]
		// CHMPXSTS_SRVIN_UP_PENDING is "pending" status, so we need to start merging.
		//
		status = CHMPXSTS_SRVIN_UP_PENDING;
	}
	CHANGE_CHMPXSTS_TO_NOSUP(status);
	MSG_CHMPRN("Status(0x%016" PRIx64 ":%s) is changed from old status(0x%016" PRIx64 ":%s).", status, STR_CHMPXSTS_FULL(status).c_str(), old_status, STR_CHMPXSTS_FULL(old_status).c_str());

	// set status(set suspend in this method)
	if(!pImData->SetSelfStatus(status)){
		ERR_CHMPRN("Failed to change server status(0x%016" PRIx64 ").", status);
		return false;
	}

	// Update pending hash.
	if(!pImData->UpdateHash(HASHTG_PENDING)){
		ERR_CHMPRN("Failed to update pending hash for all servers, so stop update status.");
		return false;
	}

	// send status update
	CHMPXSVR	chmpxsvr;
	if(!pImData->GetSelfChmpxSvr(&chmpxsvr)){
		ERR_CHMPRN("Could not get self chmpx information.");
		return false;
	}
	chmpxid_t	nextchmpxid = GetNextRingChmpxId();
	if(CHM_INVALID_CHMPXID == nextchmpxid){
		WAN_CHMPRN("Could not get next chmpxid, probably there is no server without self chmpx on RING.");
		if(!PxComSendSlavesStatusChange(&chmpxsvr)){
			ERR_CHMPRN("Failed to send self status change to slaves.");
			return false;
		}
	}else{
		if(!PxComSendStatusChange(nextchmpxid, &chmpxsvr)){
			ERR_CHMPRN("Failed to send self status change.");
			return false;
		}
	}

	// If do not merge(almost random deliver mode), do merging, completing it.
	if(!ContinuousAutoMerge()){
		ERR_CHMPRN("Failed to start merging.");
		return false;
	}
	return true;
}

bool ChmEventSock::PxComSendReqUpdateData(chmpxid_t chmpxid, const PPXCOMMON_MERGE_PARAM pmerge_param)
{
	if(CHM_INVALID_CHMPXID == chmpxid || !pmerge_param){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	while(!fullock::flck_trylock_noshared_mutex(&mergeidmap_lockval));	// LOCK

	// Make packet
	PCOMPKT	pComPkt;
	if(NULL == (pComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_REQ_UPDATEDATA, (sizeof(PXCOM_REQ_IDMAP) * mergeidmap.size())))){
		ERR_CHMPRN("Could not allocate memory for COMPKT.");
		fullock::flck_unlock_noshared_mutex(&mergeidmap_lockval);		// UNLOCK
		return false;
	}

	// compkt
	PPXCOM_ALL				pComAll			= CVT_COM_ALL_PTR_PXCOMPKT(pComPkt);
	PPXCOM_REQ_UPDATEDATA	pReqUpdateData	= CVT_COMPTR_REQ_UPDATEDATA(pComAll);
	SET_PXCOMPKT(pComPkt, COM_VERSION_2, CHMPX_COM_REQ_UPDATEDATA, pImData->GetSelfChmpxId(), chmpxid, true, (sizeof(PXCOM_REQ_IDMAP) * mergeidmap.size()));

	pReqUpdateData->head.type			= CHMPX_COM_REQ_UPDATEDATA;
	pReqUpdateData->head.result			= CHMPX_COM_RES_SUCCESS;
	pReqUpdateData->head.length			= sizeof(PXCOM_REQ_UPDATEDATA) + (sizeof(PXCOM_REQ_IDMAP) * mergeidmap.size());
	pReqUpdateData->count				= static_cast<long>(mergeidmap.size());
	pReqUpdateData->plist_offset		= sizeof(PXCOM_REQ_UPDATEDATA);
	COPY_COMMON_MERGE_PARAM(&(pReqUpdateData->merge_param), pmerge_param);

	// extra area
	PPXCOM_REQ_IDMAP	pIdMap	= CHM_OFFSET(pReqUpdateData, sizeof(PXCOM_REQ_UPDATEDATA), PPXCOM_REQ_IDMAP);
	int					idcnt	= 0;
	for(mergeidmap_t::const_iterator iter = mergeidmap.begin(); iter != mergeidmap.end(); ++iter, ++idcnt){
		pIdMap[idcnt].chmpxid	= iter->first;
		pIdMap[idcnt].req_status= iter->second;
	}

	fullock::flck_unlock_noshared_mutex(&mergeidmap_lockval);			// UNLOCK

	// Send request
	if(!Send(pComPkt, NULL, 0L)){
		ERR_CHMPRN("Failed to send CHMPX_COM_REQ_UPDATEDATA to chmpxid(0x%016" PRIx64 ").", chmpxid);
		CHM_Free(pComPkt);
		return false;
	}
	CHM_Free(pComPkt);

	return true;
}

bool ChmEventSock::PxComReceiveReqUpdateData(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt)
{
	if(!pComHead || !pComAll || !ppResComPkt){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*				pImData			= pChmCntrl->GetImDataObj();
	PPXCOM_REQ_UPDATEDATA	pReqUpdateData	= CVT_COMPTR_REQ_UPDATEDATA(pComAll);
	chmpxid_t				selfchmpxid		= pImData->GetSelfChmpxId();
	*ppResComPkt							= NULL;

	if(pComHead->dept_ids.chmpxid == selfchmpxid){
		// around -> next step: update pending hash value
		//
		if(CHMPX_COM_RES_SUCCESS == pReqUpdateData->head.result){
			// Succeed request update data
			while(!fullock::flck_trylock_noshared_mutex(&mergeidmap_lockval));	// LOCK

			// copy all update data request status
			PPXCOM_REQ_IDMAP	pReqIdMap	= CHM_OFFSET(pReqUpdateData, sizeof(PXCOM_REQ_UPDATEDATA), PPXCOM_REQ_IDMAP);
			for(long cnt = 0; cnt < pReqUpdateData->count; ++cnt){
				mergeidmap_t::iterator	iter = mergeidmap.find(pReqIdMap[cnt].chmpxid);

				if(mergeidmap.end() == iter){
					// why? there is not chmpxid, but set data
					WAN_CHMPRN("There is not chmpxid(0x%016" PRIx64 ") in map.", pReqIdMap[cnt].chmpxid);
					mergeidmap[pReqIdMap[cnt].chmpxid] = pReqIdMap[cnt].req_status;
				}else{
					if(IS_PXCOM_REQ_UPDATE_RESULT(mergeidmap[pReqIdMap[cnt].chmpxid])){
						WAN_CHMPRN("Already result(%s) for chmpxid(0x%016" PRIx64 ") in map, so not set result(%s)",
							STRPXCOM_REQ_UPDATEDATA(mergeidmap[pReqIdMap[cnt].chmpxid]), pReqIdMap[cnt].chmpxid, STRPXCOM_REQ_UPDATEDATA(pReqIdMap[cnt].req_status));
					}else{
						mergeidmap[pReqIdMap[cnt].chmpxid] = pReqIdMap[cnt].req_status;
					}
				}
			}
			fullock::flck_unlock_noshared_mutex(&mergeidmap_lockval);			// UNLOCK
			return true;

		}else{
			// Something error occured on RING, but nothing to do for recovering.
			ERR_CHMPRN("PXCOM_REQ_UPDATEDATA is failed, could not recover...");
			return false;
		}
	}else{
		// get next chmpxid on RING
		chmpxid_t	nextchmpxid	= GetNextRingChmpxId();

		if(CHM_INVALID_CHMPXID != nextchmpxid){
			if(!IsSafeDeptAndNextChmpxId(pComHead->dept_ids.chmpxid, nextchmpxid)){
				// Departure chmpx maybe DOWN!
				//
				// [NOTICE]
				// This case is very small case, departure server sends this packet but that server could not
				// be connected from in RING server.(ex. down after sending, or FQDN is wrong, etc)
				// So we do not transfer this packet, because it could not be stopped in RING.
				//
				ERR_CHMPRN("Departure chmpxid(0x%016" PRIx64 ") maybe down, so stop transferring this packet.", pComHead->dept_ids.chmpxid);

			}else{
				// Make packet
				PCOMPKT	pResComPkt;
				if(NULL == (pResComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_REQ_UPDATEDATA, (sizeof(PXCOM_REQ_IDMAP) * pReqUpdateData->count)))){
					ERR_CHMPRN("Could not allocate memory for COMPKT.");
					return false;
				}

				// compkt(copy)
				PPXCOM_ALL				pComAllSub		= CVT_COM_ALL_PTR_PXCOMPKT(pResComPkt);
				PPXCOM_REQ_UPDATEDATA	pResUpdateData	= CVT_COMPTR_REQ_UPDATEDATA(pComAllSub);

				SET_PXCOMPKT(pResComPkt, COM_VERSION_2, CHMPX_COM_REQ_UPDATEDATA, pComHead->dept_ids.chmpxid, nextchmpxid, false, (sizeof(PXCOM_REQ_IDMAP) * pReqUpdateData->count));	// dept chmpxid is not changed.
				COPY_TIMESPEC(&(pResComPkt->head.reqtime), &(pComHead->reqtime));	// not change

				pResUpdateData->head.type			= CHMPX_COM_REQ_UPDATEDATA;
				pResUpdateData->head.result			= CHMPX_COM_RES_SUCCESS;
				pResUpdateData->head.length			= sizeof(PXCOM_REQ_UPDATEDATA) + (sizeof(PXCOM_REQ_IDMAP) * pReqUpdateData->count);
				pResUpdateData->count				= pReqUpdateData->count;
				pResUpdateData->plist_offset		= sizeof(PXCOM_REQ_UPDATEDATA);
				COPY_COMMON_MERGE_PARAM(&(pResUpdateData->merge_param), &(pReqUpdateData->merge_param));

				// extra area(copy)
				PPXCOM_REQ_IDMAP	pOwnMap		= NULL;
				PPXCOM_REQ_IDMAP	pReqIdMap	= CHM_OFFSET(pReqUpdateData, sizeof(PXCOM_REQ_UPDATEDATA), PPXCOM_REQ_IDMAP);
				PPXCOM_REQ_IDMAP	pResIdMap	= CHM_OFFSET(pResUpdateData, sizeof(PXCOM_REQ_UPDATEDATA), PPXCOM_REQ_IDMAP);
				for(long cnt = 0; cnt < pReqUpdateData->count; ++cnt){
					pResIdMap[cnt].chmpxid		= pReqIdMap[cnt].chmpxid;
					pResIdMap[cnt].req_status	= pReqIdMap[cnt].req_status;

					if(selfchmpxid == pResIdMap[cnt].chmpxid){
						pOwnMap = &pResIdMap[cnt];
					}
				}

				// own chmpxid in target chmpxid list?
				if(pOwnMap){
					// get own status
					chmpxsts_t	status = pImData->GetSelfStatus();

					// check status
					//
					// [NOTE]
					// Transfer the request to client, if this server is UP/DOWN & NOSUSPEND status.
					// Because we merge data from minimum server which has a valid data.
					//
					if((IS_CHMPXSTS_NOACT(status) || IS_CHMPXSTS_DELETE(status)) && IS_CHMPXSTS_NOSUP(status)){
						// this server needs to update data, so transfer request to client.

						// Make parameter
						PXCOMMON_MERGE_PARAM	MqMergeParam;
						COPY_COMMON_MERGE_PARAM(&MqMergeParam, &(pResUpdateData->merge_param));

						// transfer command to MQ
						if(!pChmCntrl->MergeRequestUpdateData(&MqMergeParam)){
							ERR_CHMPRN("Something error occurred during the request is sent to MQ.");
							pOwnMap->req_status = CHMPX_COM_REQ_UPDATE_FAIL;
						}else{
							pOwnMap->req_status = CHMPX_COM_REQ_UPDATE_DO;
						}
					}else{
						// this server does not need to update, so result is success.
						pOwnMap->req_status = CHMPX_COM_REQ_UPDATE_NOACT;
					}
				}else{
					// this chmpx is not target chmpx.
				}
				// set response data.
				*ppResComPkt = pResComPkt;
			}
		}else{
			// there is no next chmpxid.
		}
	}
	return true;
}

bool ChmEventSock::PxComSendResUpdateData(chmpxid_t chmpxid, size_t length, const unsigned char* pdata, const struct timespec* pts)
{
	if(CHM_INVALID_CHMPXID == chmpxid || 0 == length || !pdata || !pts){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// Make packet
	PCOMPKT	pComPkt;
	if(NULL == (pComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_RES_UPDATEDATA, length))){
		ERR_CHMPRN("Could not allocate memory for COMPKT.");
		return false;
	}

	// compkt
	PPXCOM_ALL				pComAll			= CVT_COM_ALL_PTR_PXCOMPKT(pComPkt);
	PPXCOM_RES_UPDATEDATA	pResUpdateData	= CVT_COMPTR_RES_UPDATEDATA(pComAll);
	SET_PXCOMPKT(pComPkt, COM_VERSION_2, CHMPX_COM_RES_UPDATEDATA, pImData->GetSelfChmpxId(), chmpxid, true, length);

	pResUpdateData->head.type		= CHMPX_COM_RES_UPDATEDATA;
	pResUpdateData->head.result		= CHMPX_COM_RES_SUCCESS;
	pResUpdateData->head.length		= sizeof(PXCOM_RES_UPDATEDATA) + length;
	pResUpdateData->ts.tv_sec		= pts->tv_sec;
	pResUpdateData->ts.tv_nsec		= pts->tv_nsec;
	pResUpdateData->length			= length;
	pResUpdateData->pdata_offset	= sizeof(PXCOM_RES_UPDATEDATA);

	unsigned char*	psetbase		= CHM_OFFSET(pResUpdateData, sizeof(PXCOM_RES_UPDATEDATA), unsigned char*);
	memcpy(psetbase, pdata, length);

	// Send request
	if(!Send(pComPkt, NULL, 0L)){
		ERR_CHMPRN("Failed to send CHMPX_COM_RES_UPDATEDATA to chmpxid(0x%016" PRIx64 ").", chmpxid);
		CHM_Free(pComPkt);
		return false;
	}
	CHM_Free(pComPkt);

	return true;
}

bool ChmEventSock::PxComReceiveResUpdateData(PCOMHEAD pComHead, PPXCOM_ALL pComAll)
{
	if(!pComHead || !pComAll){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}

	PPXCOM_RES_UPDATEDATA	pResUpdateData	= CVT_COMPTR_RES_UPDATEDATA(pComAll);

	// check data
	if(0 == pResUpdateData->length || 0 == pResUpdateData->pdata_offset){
		ERR_CHMPRN("Received CHMPX_COM_RES_UPDATEDATA command is something wrong.");
		return false;
	}
	unsigned char*	pdata = CHM_OFFSET(pResUpdateData, pResUpdateData->pdata_offset, unsigned char*);

	// transfer client
	return pChmCntrl->MergeSetUpdateData(pResUpdateData->length, pdata, &(pResUpdateData->ts));
}

bool ChmEventSock::PxComSendResultUpdateData(chmpxid_t chmpxid, reqidmapflag_t result_updatedata)
{
	if(CHM_INVALID_CHMPXID == chmpxid){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// Make packet
	PCOMPKT	pComPkt;
	if(NULL == (pComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_RESULT_UPDATEDATA))){
		ERR_CHMPRN("Could not allocate memory for COMPKT.");
		return false;
	}

	// compkt
	PPXCOM_ALL					pComAll				= CVT_COM_ALL_PTR_PXCOMPKT(pComPkt);
	PPXCOM_RESULT_UPDATEDATA	pResultUpdateData	= CVT_COMPTR_RESULT_UPDATEDATA(pComAll);
	SET_PXCOMPKT(pComPkt, COM_VERSION_2, CHMPX_COM_RESULT_UPDATEDATA, pImData->GetSelfChmpxId(), chmpxid, true, 0);

	pResultUpdateData->head.type	= CHMPX_COM_RESULT_UPDATEDATA;
	pResultUpdateData->head.result	= CHMPX_COM_RES_SUCCESS;
	pResultUpdateData->chmpxid		= pImData->GetSelfChmpxId();
	pResultUpdateData->result		= result_updatedata;

	// Send request
	if(!Send(pComPkt, NULL, 0L)){
		ERR_CHMPRN("Failed to send CHMPX_COM_RESULT_UPDATEDATA to chmpxid(0x%016" PRIx64 ").", chmpxid);
		CHM_Free(pComPkt);
		return false;
	}
	CHM_Free(pComPkt);

	return true;
}

bool ChmEventSock::PxComReceiveResultUpdateData(PCOMHEAD pComHead, PPXCOM_ALL pComAll)
{
	if(!pComHead || !pComAll){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	PPXCOM_RESULT_UPDATEDATA	pResultUpdateData = CVT_COMPTR_RESULT_UPDATEDATA(pComAll);

	// check data
	if(CHM_INVALID_CHMPXID == pResultUpdateData->chmpxid || !IS_PXCOM_REQ_UPDATE_RESULT(pResultUpdateData->result)){
		ERR_CHMPRN("Received CHMPX_COM_RESULT_UPDATEDATA command is something wrong.");
		return false;
	}

	while(!fullock::flck_trylock_noshared_mutex(&mergeidmap_lockval));	// LOCK

	// search map
	mergeidmap_t::iterator	iter = mergeidmap.find(pResultUpdateData->chmpxid);
	if(mergeidmap.end() == iter){
		ERR_CHMPRN("The chmpxid(0x%016" PRIx64 ") in received CHMPX_COM_RESULT_UPDATEDATA does not find in merge id map.", pResultUpdateData->chmpxid);
		fullock::flck_unlock_noshared_mutex(&mergeidmap_lockval);		// UNLOCK
		return false;
	}
	if(!IS_PXCOM_REQ_UPDATE_RESULT(pResultUpdateData->result)){
		ERR_CHMPRN("received CHMPX_COM_RESULT_UPDATEDATA result(%s) for chmpxid(0x%016" PRIx64 ") is wrong.", STRPXCOM_REQ_UPDATEDATA(pResultUpdateData->result), pResultUpdateData->chmpxid);
		return false;
	}
	// set result
	mergeidmap[pResultUpdateData->chmpxid] = pResultUpdateData->result;

	fullock::flck_unlock_noshared_mutex(&mergeidmap_lockval);			// UNLOCK
	return true;
}

//
// PxComSendVersionReq uses sock directly.
//
// [NOTE]
// If you need to lock the socket, you must lock it before calling this method.
//
bool ChmEventSock::PxComSendVersionReq(int sock, chmpxid_t chmpxid, bool need_sock_close)
{
	if(CHM_INVALID_SOCK == sock || CHM_INVALID_CHMPXID == chmpxid){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	// Make packet
	PCOMPKT	pComPkt;
	if(NULL == (pComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_VERSION_REQ))){
		ERR_CHMPRN("Could not allocate memory for COMPKT.");
		return false;
	}

	// compkt
	PPXCOM_ALL				pComAll		= CVT_COM_ALL_PTR_PXCOMPKT(pComPkt);
	PPXCOM_VERSION_REQ		pVersionReq	= CVT_COMPTR_VERSION_REQ(pComAll);
	SET_PXCOMPKT(pComPkt, COM_VERSION_2, CHMPX_COM_VERSION_REQ, pImData->GetSelfChmpxId(), chmpxid, true, 0L);

	pVersionReq->head.type		= CHMPX_COM_VERSION_REQ;
	pVersionReq->head.result	= CHMPX_COM_RES_SUCCESS;
	pVersionReq->head.length	= sizeof(PXCOM_VERSION_REQ);

	// Send request
	//
	// [NOTE]
	// Should lock the socket in caller if you need.
	//
	bool	is_closed = false;
	if(!ChmEventSock::RawSend(sock, GetSSL(sock), pComPkt, is_closed, false, sock_retry_count, sock_wait_time)){		// as default nonblocking
		ERR_CHMPRN("Failed to send CHMPX_COM_VERSION_REQ to sock(%d).", sock);

		// [NOTE]
		// When this method is called from InitialAllServerVersion(), the sock is not mapped.
		// Then we close sock when need_sock_close is true.
		//
		if(need_sock_close && is_closed){
			if(!RawNotifyHup(sock)){
				ERR_CHMPRN("Failed to closing socket(%d), but continue...", sock);
			}
		}
		CHM_Free(pComPkt);
		return false;
	}
	CHM_Free(pComPkt);

	return true;
}

// [NOTE]
// CHMPX_COM_VERSION_REQ can only be accepted from a socket that is being accepted.
// Therefore, instead of processing in the reception main loop, CHMPX_COM_VERSION_RES is
// returned directly in this method.
//
bool ChmEventSock::PxComReceiveVersionReq(int sock, PCOMHEAD pComHead, PPXCOM_ALL pComAll)
{
	if(CHM_INVALID_SOCK == sock || !pComHead || !pComAll){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*				pImData		= pChmCntrl->GetImDataObj();
	//PPXCOM_VERSION_REQ	pVersionReq	= CVT_COMPTR_VERSION_REQ(pComAll);	// unused now

	// Update self last status updating time 
	if(!pImData->UpdateSelfLastStatusTime()){
		ERR_CHMPRN("Could not update own updating status time, but continue...");
	}

	// get version information
	uint64_t	chmpx_com_version	= COM_VERSION_2;
	uint64_t	chmpx_cur_version	= IS_COM_CURRENT_VERSION(comproto_ver) ? COM_VERSION_2 : COM_VERSION_1;
	uint64_t	chmpx_bin_version	= chmpx_get_bin_version();
	string		chmpx_str_version	= chmpx_get_raw_version(false);
	string		commit_hash			= chmpx_get_raw_version(true);

	// make response data
	PCOMPKT	pResComPkt;
	if(NULL == (pResComPkt = ChmEventSock::AllocatePxComPacket(CHMPX_COM_VERSION_RES))){
		ERR_CHMPRN("Could not allocation memory.");
		return false;
	}

	// compkt
	PPXCOM_ALL			pResComAll	= CVT_COM_ALL_PTR_PXCOMPKT(pResComPkt);
	PPXCOM_VERSION_RES	pVersionRes	= CVT_COMPTR_VERSION_RES(pResComAll);
	SET_PXCOMPKT(pResComPkt, COM_VERSION_2, CHMPX_COM_VERSION_RES, pComHead->term_ids.chmpxid, pComHead->dept_ids.chmpxid, true, 0L);	// switch chmpxid

	pVersionRes->head.type			= CHMPX_COM_VERSION_RES;
	pVersionRes->head.result		= CHMPX_COM_RES_SUCCESS;					// always success
	pVersionRes->head.length		= sizeof(PXCOM_VERSION_RES);
	pVersionRes->chmpx_com_version	= chmpx_com_version;
	pVersionRes->chmpx_cur_version	= chmpx_cur_version;
	pVersionRes->chmpx_bin_version	= chmpx_bin_version;
	memset(pVersionRes->chmpx_str_version,	0,							CHMPX_VERSION_MAX);
	strncpy(pVersionRes->chmpx_str_version,	chmpx_str_version.c_str(),	CHMPX_VERSION_MAX - 1);
	memset(pVersionRes->commit_hash,		0,							COMMIT_HASH_MAX);
	strncpy(pVersionRes->commit_hash,		commit_hash.c_str(),		COMMIT_HASH_MAX - 1);

	// Send request
	if(!ChmEventSock::LockedSend(sock, GetSSL(sock), pResComPkt)){				// as default nonblocking
		ERR_CHMPRN("Failed to send CHMPX_COM_VERSION_RES to sock(%d).", sock);
		CHM_Free(pResComPkt);
		return false;
	}
	CHM_Free(pResComPkt);

	return true;
}

bool ChmEventSock::PxComReceiveVersionRes(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt, comver_t& com_proto_version)
{
	if(!pComHead || !pComAll || !ppResComPkt){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*			pImData		= pChmCntrl->GetImDataObj();
	PPXCOM_VERSION_RES	pVersionRes	= CVT_COMPTR_VERSION_RES(pComAll);
	chmpxid_t			selfchmpxid	= pImData->GetSelfChmpxId();
	*ppResComPkt					= NULL;

	if(pComHead->term_ids.chmpxid == selfchmpxid){
		// To me
		if(CHMPX_COM_RES_SUCCESS != pVersionRes->head.result){
			// Something error occured by status response
			ERR_CHMPRN("PXCOM_VERSION_RES is failed, probably responser does not have CHMPX communication version, it measn CHMPX older than v1.0.71");
			com_proto_version = COM_VERSION_1;
			return true;
		}
		// Succeed version response

		// set communication protocol version
		com_proto_version = pVersionRes->chmpx_com_version;
		if(pVersionRes->chmpx_cur_version < com_proto_version){
			com_proto_version = pVersionRes->chmpx_cur_version;
			if(COM_VERSION_2 < com_proto_version){
				com_proto_version = COM_VERSION_2;
			}
		}

		// for messages
		MSG_CHMPRN("PXCOM_VERSION_RES succeed : CHMPX version %s(0x%016" PRIx64 ") : commit(%s) - Communication protocol version 0x%016" PRIx64 ".", pVersionRes->chmpx_str_version, pVersionRes->chmpx_bin_version, pVersionRes->commit_hash, pVersionRes->chmpx_com_version);
	}else{
		// To other chmpxid
		ERR_CHMPRN("Received PXCOM_VERSION_RES packet, but terminal chmpxid(0x%016" PRIx64 ") is not self chmpxid(0x%016" PRIx64 ").", pComHead->term_ids.chmpxid, selfchmpxid);
		return false;
	}
	return true;
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
