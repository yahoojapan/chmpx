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
#ifndef	CHMEVENTSOCK_H
#define	CHMEVENTSOCK_H

#include <pthread.h>
#include <string>
#include <map>

#include "chmeventbase.h"
#include "chmthread.h"
#include "chmlockmap.tcc"

//---------------------------------------------------------
// Symbols (must be defined before chmss.h)
//---------------------------------------------------------
#define	PXCOMPKT_AUTO_LENGTH			(-1)

#define	WAIT_READ_FD					1
#define	WAIT_WRITE_FD					2
#define	WAIT_BOTH_FD					(WAIT_READ_FD | WAIT_WRITE_FD)

#define	ISSAFE_WAIT_FD(type)			(0 != (type & WAIT_BOTH_FD))
#define	IS_WAIT_READ_FD(type)			(0 != (type & WAIT_READ_FD))
#define	IS_WAIT_WRITE_FD(type)			(0 != (type & WAIT_WRITE_FD))

#define	CHMEVENTSOCK_RETRY_DEFAULT		(-1)
#define	CHMEVENTSOCK_TIMEOUT_DEFAULT	(0)

//
// Include SSL/TLS implement header
//
#include "chmss.h"

//---------------------------------------------------------
// Structure
//---------------------------------------------------------
typedef struct chm_send_sock_status{
	pid_t		tid;					// thread id of locking socket
	time_t		last_time;				// last using(locking) time
}CHMSSSTAT, *PCHMSSSTAT;

//---------------------------------------------------------
// Typedefs
//---------------------------------------------------------
typedef std::map<chmpxid_t, reqidmapflag_t>	mergeidmap_t;
typedef chm_lock_map<int, chmpxid_t>		sock_ids_map_t;
typedef chm_lock_map<int, std::string>		sock_pending_map_t;
typedef chm_lock_map<int, ChmSSSession>		sock_ssl_map_t;
typedef chm_lock_map<int, PCHMSSSTAT>		sendlockmap_t;

//---------------------------------------------------------
// ChmEventSock Class
//---------------------------------------------------------
// [NOTE] About multi-threading
//
// This class should have exclusion control for socket on multi-threading.
// Then this class controls closing/opening/accepting socket as exclusion,
// thus these processing can not run at same time.
// And this class can send data to particular socket by only one thread at
// same time because the socket is locked by that thread.
// These two exclusion control are independent of each other, so we can
// open new socket during sending data as an example.
//
// And contrary to the above, this class does not lock the socket for
// receiving. The reason is that the receiving from the same socket is
// processed sequentially, thus we do not need to lock the socket for
// receiving. However the processing such as open(close) is caught during
// we are receiving from the socket, but we can probably fail to receive
// by closing the socket.
// Thus we can stop the processing to receive by getting error.
//
class ChmEventSock : public ChmEventBase
{
	public:
		static const int			DEFAULT_SOCK_THREAD_CNT		= 0;		// socket processing thread count
		static const int			DEFAULT_MAX_SOCK_POOL		= 1;		// max socket count for each chmpx
		static const time_t			DEFAULT_SOCK_POOL_TIMEOUT	= 60;		// timeout value till closing for not used socket in pool
		static const time_t			NO_SOCK_POOL_TIMEOUT		= 0;		// no timeout for socket pool
		static const int			DEFAULT_KEEPIDLE			= 60;		// time until sending keep alive
		static const int			DEFAULT_KEEPINTERVAL		= 10;		// interval for sending keep alive
		static const int			DEFAULT_KEEPCOUNT			= 3;		// maximum count of sending keep alive
		static const int			DEFAULT_LISTEN_BACKLOG		= 256;
		static const int			DEFAULT_RETRYCNT			= 500;		// retry count for send and receive(total default wait = 200us * 500 = 100ms)
		static const int			DEFAULT_RETRYCNT_CONNECT	= 2500;		// retry count for connect.( = (500 * 1000) / 200 )
		static const suseconds_t	DEFAULT_WAIT_SOCKET			= 200;		// timeout/count for send and receive.(200us)
		static const suseconds_t	DEFAULT_WAIT_CONNECT		= 500 * 1000;	// total timeout for connect.(500ms)

	protected:
		typedef enum chm_downsvr_merge_type{								// for ChangeDownSvrStatusBeforeMerge method
			CHM_DOWNSVR_MERGE_START,
			CHM_DOWNSVR_MERGE_COMPLETE,
			CHM_DOWNSVR_MERGE_ABORT
		}CHMDOWNSVRMERGE;

		ChmSecureSock*				pSecureSock;							// SSL/TLS Library wrapper object
		sock_ids_map_t				seversockmap;							// to server sockets
		sock_ids_map_t				slavesockmap;							// from slave sockets
		sock_pending_map_t			acceptingmap;							// from slave sockets before accepting
		sock_ids_map_t				ctlsockmap;								// from client sockets to ctlport
		sock_ssl_map_t				sslmap;									// to server/from slave ssl object mapping
		sendlockmap_t				sendlockmap;							// lock socket map for sending
		bool						startup_servicein;						// SERVICE IN pending at start when automerge mode is true
		volatile bool				is_run_merge;							// whichever run or abort in merging
		ChmThread					procthreads;							// processing threads
		ChmThread					mergethread;							// merge thread
		volatile int				sockfd_lockval;							// lock variable for open/close/accept socket
		time_t						last_check_time;						// last unix time at checking socket pool timeout
		volatile int				dyna_sockfd_lockval;					// lock variable for new connection dynamic
		volatile int				mergeidmap_lockval;						// lock variable for chmpxid mapping
		mergeidmap_t				mergeidmap;								// chmpxid mapping for update datas
		bool						is_server_mode;							// cache value for ChmIMData::IsServerMode()
		int							max_sock_pool;							// cache value for ChmIMData::GetMaxSockPool()
		time_t						sock_pool_timeout;						// cache value for ChmIMData::GetSockPoolTimeout()
		int							sock_retry_count;						// cache value for retry count for send and receive
		int							con_retry_count;						// cache value for retry count for connect
		suseconds_t					sock_wait_time;							// cache value for timeout/count for send and receive
		suseconds_t					con_wait_time;							// cache value for total timeout for connect

	protected:
		static bool SetNonblocking(int sock);
		static int Listen(const char* hostname, short port);
		static int RawListen(struct addrinfo* paddrinfo);
		static int Connect(const char* hostname, short port, bool is_blocking = false, int con_retrycnt = CHMEVENTSOCK_RETRY_DEFAULT, suseconds_t con_waittime = CHMEVENTSOCK_TIMEOUT_DEFAULT);
		static bool RawSend(int sock, ChmSSSession ssl, PCOMPKT pComPkt, bool& is_closed, bool is_blocking = false, int retrycnt = CHMEVENTSOCK_RETRY_DEFAULT, suseconds_t waittime = CHMEVENTSOCK_TIMEOUT_DEFAULT);
		static bool RawSend(int sock, ChmSSSession ssl, const unsigned char* pbydata, size_t length, bool& is_closed, bool is_blocking = false, int retrycnt = CHMEVENTSOCK_RETRY_DEFAULT, suseconds_t waittime = CHMEVENTSOCK_TIMEOUT_DEFAULT);
		static bool RawReceiveByte(int sock, ChmSSSession ssl, bool& is_closed, unsigned char* pbuff, size_t length, bool is_blocking = false, int retrycnt = CHMEVENTSOCK_RETRY_DEFAULT, suseconds_t waittime = CHMEVENTSOCK_TIMEOUT_DEFAULT);
		static bool RawReceiveAny(int sock, bool& is_closed, unsigned char* pbuff, size_t* plength, bool is_blocking = false, int retrycnt = CHMEVENTSOCK_RETRY_DEFAULT, suseconds_t waittime = CHMEVENTSOCK_TIMEOUT_DEFAULT);
		static bool RawReceive(int sock, ChmSSSession ssl, bool& is_closed, PCOMPKT* ppComPkt, size_t pktlength = 0L, bool is_blocking = false, int retrycnt = CHMEVENTSOCK_RETRY_DEFAULT, suseconds_t waittime = CHMEVENTSOCK_TIMEOUT_DEFAULT);
		static bool RawSendCtlPort(const char* hostname, short ctlport, const unsigned char* pbydata, size_t length, std::string& strResult, volatile int& sockfd_lockval, int retrycnt = CHMEVENTSOCK_RETRY_DEFAULT, suseconds_t waittime = CHMEVENTSOCK_TIMEOUT_DEFAULT, int con_retrycnt = CHMEVENTSOCK_RETRY_DEFAULT, suseconds_t con_waittime = CHMEVENTSOCK_TIMEOUT_DEFAULT);

		static bool hton(PCOMPKT pComPkt, size_t& length);
		static bool ntoh(PCOMPKT pComPkt, bool is_except_compkt = false);
		static PCOMPKT AllocatePxComPacket(pxcomtype_t type, ssize_t ext_length = PXCOMPKT_AUTO_LENGTH);
		static PCOMPKT DuplicateComPkt(PCOMPKT pComPkt);

		// callback functions for thread class
		static bool ReceiveWorkerProc(void* common_param, chmthparam_t wp_param);

		// callback functions for merging
		static bool MergeWorkerFunc(void* common_param, chmthparam_t wp_param);

		// callback functions for locked map class
		static bool ServerSockMapCallback(sock_ids_map_t::iterator& iter, void* psockobj);
		static bool SlaveSockMapCallback(sock_ids_map_t::iterator& iter, void* psockobj);
		static bool AcceptMapCallback(sock_pending_map_t::iterator& iter, void* psockobj);
		static bool ControlSockMapCallback(sock_ids_map_t::iterator& iter, void* psockobj);
		static bool SslSockMapCallback(sock_ssl_map_t::iterator& iter, void* psockobj);
		static bool SendLockMapCallback(sendlockmap_t::iterator& iter, void* psockobj);

		// close connection
		bool CloseSelfSocks(void);
		bool CloseFromSlavesSocks(void);
		bool CloseCtlportClientSocks(void);
		bool CloseToServersSocks(void);
		bool CloseFromSlavesAcceptingSocks(void);
		bool CloseSSL(int sock, bool with_sock = true);
		bool CloseAllSSL(void);
		bool CloseSocketWithEpoll(int sock);

		// SSL
		ChmSSSession GetSSL(int sock);
		bool IsSafeParamsForSSL(void);

		// Socket lock for sending
		bool LockSendSock(int sock) { return RawLockSendSock(sock, true, false); }
		bool TryLockSendSock(int sock) { return RawLockSendSock(sock, true, true); }
		bool UnlockSendSock(int sock) { return RawLockSendSock(sock, false, false); }
		bool RawLockSendSock(int sock, bool is_lock, bool is_once);
		bool GetLockedSendSock(chmpxid_t chmpxid, int& sock, bool is_check_slave);

		// Send with locking
		bool LockedSend(int sock, ChmSSSession ssl, PCOMPKT pComPkt, bool is_blocking = false);
		bool LockedSend(int sock, ChmSSSession ssl, const unsigned char* pbydata, size_t length, bool is_blocking = false);

		// Utility
		chmpxid_t GetAssociateChmpxid(int fd);
		std::string GetAssociateAcceptHostName(int fd);
		bool RawClearSelfWorkerStatus(int fd);

		// Receive
		bool RawReceive(int fd, bool& is_closed);

		// connect/accept
		bool ConnectServer(chmpxid_t chmpxid, int& sock, bool without_set_imdata);
		bool RawConnectServer(chmpxid_t chmpxid, int& sock, bool without_set_imdata, bool is_lock);
		bool ConnectRing(void);
		bool CloseRechainRing(chmpxid_t nowchmpxid);
		bool RechainRing(chmpxid_t newchmpxid);
		bool CheckRechainRing(chmpxid_t newchmpxid, bool& is_rechain);
		chmpxid_t GetNextRingChmpxId(void);
		bool IsSafeDeptAndNextChmpxId(chmpxid_t dept_chmpxid, chmpxid_t next_chmpxid);
		bool Accept(int sock);
		bool AcceptCtlport(int ctlsock);
		bool Processing(int sock, const char* pCommand);

		// for merge
		bool IsAutoMerge(void);
		bool IsDoMerge(void);
		bool ContinuousAutoMerge(void);
		bool CheckAllStatusForMergeStart(void) const;
		bool CheckAllStatusForMergeComplete(void) const;
		bool ChangeStatusBeforeMergeStart(void);
		bool ChangeDownSvrStatusByMerge(CHMDOWNSVRMERGE mode);
		bool MergeStart(void);
		bool MergeAbort(void);
		bool MergeDone(void);
		bool MergeComplete(void);
		bool RequestMergeStart(std::string* pstring = NULL);
		bool RequestMergeAbort(std::string* pstring = NULL);
		bool RequestMergeComplete(std::string* pstring = NULL);
		bool RequestMergeSuspend(std::string* pstring = NULL);
		bool RequestMergeNoSuspend(std::string* pstring = NULL);
		bool RequestServiceIn(std::string* pstring = NULL);
		bool RequestServiceOut(chmpxid_t chmpxid = CHM_INVALID_CHMPXID, std::string* pstring = NULL);

		// notification
		bool RawNotifyHup(int fd);
		bool ServerDownNotifyHup(chmpxid_t chmpxid);
		bool ServerSockNotifyHup(int fd, bool& is_close, chmpxid_t& chmpxid);
		bool SlaveSockNotifyHup(int fd, bool& is_close);
		bool AcceptingSockNotifyHup(int fd, bool& is_close);
		bool ControlSockNotifyHup(int fd, bool& is_close);

		// ctlport commands
		bool SendCtlPort(chmpxid_t chmpxid, const unsigned char* pbydata, size_t length, std::string& strResult);
		bool CtlComDump(std::string& strResponse);
		bool CtlComMergeStart(std::string& strResponse);
		bool CtlComMergeAbort(std::string& strResponse);
		bool CtlComMergeComplete(std::string& strResponse);
		bool CtlComMergeSuspend(std::string& strResponse);
		bool CtlComMergeNoSuspend(std::string& strResponse);
		bool CtlComServiceIn(std::string& strResponse);
		bool CtlComServiceOut(const char* hostname, short ctlport, const char* pOrgCommand, std::string& strResponse);
		bool CtlComSelfStatus(std::string& strResponse);
		bool CtlComAllServerStatus(std::string& strResponse);
		bool CtlComUpdateStatus(std::string& strResponse);
		bool CtlComAllTraceSet(std::string& strResponse, bool enable);
		bool CtlComAllTraceView(std::string& strResponse, logtype_t dirmask, logtype_t devmask, long count);

		// px2px commands
		bool PxComSendStatusReq(int sock, chmpxid_t chmpxid, bool need_sock_close);
		bool PxComReceiveStatusReq(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt);
		bool PxComReceiveStatusRes(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt, bool is_init_process = false);
		bool PxComSendConinitReq(int sock, chmpxid_t chmpxid);
		bool PxComReceiveConinitReq(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt, chmpxid_t& from_chmpxid, short& ctlport);
		bool PxComSendConinitRes(int sock, chmpxid_t chmpxid, pxcomres_t result);
		bool PxComReceiveConinitRes(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt, pxcomres_t& result);
		bool PxComSendJoinRing(chmpxid_t chmpxid, PCHMPXSVR pserver);
		bool PxComReceiveJoinRing(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt);
		bool PxComSendStatusUpdate(chmpxid_t chmpxid, PCHMPXSVR pchmpxsvrs, long count);
		bool PxComReceiveStatusUpdate(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt);
		bool PxComSendStatusConfirm(chmpxid_t chmpxid, PCHMPXSVR pchmpxsvrs, long count);
		bool PxComReceiveStatusConfirm(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt);
		bool PxComSendStatusChange(chmpxid_t chmpxid, PCHMPXSVR pserver, bool is_send_slaves = true);
		bool PxComSendSlavesStatusChange(PCHMPXSVR pserver);
		bool PxComReceiveStatusChange(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt);
		bool PxComSendMergeStart(chmpxid_t chmpxid);
		bool PxComReceiveMergeStart(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt);
		bool PxComSendMergeAbort(chmpxid_t chmpxid);
		bool PxComReceiveMergeAbort(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt);
		bool PxComSendMergeComplete(chmpxid_t chmpxid);
		bool PxComReceiveMergeComplete(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt, bool& is_completed);
		bool PxComSendMergeSuspend(chmpxid_t chmpxid);
		bool PxComReceiveMergeSuspend(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt);
		bool PxComSendMergeNoSuspend(chmpxid_t chmpxid);
		bool PxComReceiveMergeNoSuspend(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt);
		bool PxComSendMergeSuspendGet(int sock, chmpxid_t chmpxid);
		bool PxComReceiveMergeSuspendGet(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt);
		bool PxComReceiveMergeSuspendRes(PCOMHEAD pComHead, PPXCOM_ALL pComAll);
		bool PxComSendServerDown(chmpxid_t chmpxid, chmpxid_t downchmpxid);
		bool PxComReceiveServerDown(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt);

		bool PxCltReceiveJoinNotify(PCOMHEAD pComHead, PPXCLT_ALL pComAll);

		bool PxComSendReqUpdateData(chmpxid_t chmpxid, const PPXCOMMON_MERGE_PARAM pmerge_param);
		bool PxComReceiveReqUpdateData(PCOMHEAD pComHead, PPXCOM_ALL pComAll, PCOMPKT* ppResComPkt);
		bool PxComReceiveResUpdateData(PCOMHEAD pComHead, PPXCOM_ALL pComAll);
		bool PxComReceiveResultUpdateData(PCOMHEAD pComHead, PPXCOM_ALL pComAll);

	public:
		static void AllowSelfSignedCert(void);
		static void DenySelfSignedCert(void);
		static int WaitForReady(int sock, int type, int retrycnt, bool is_check_so_error = false, suseconds_t waittime = CHMEVENTSOCK_TIMEOUT_DEFAULT);

		ChmEventSock(int eventqfd = CHM_INVALID_HANDLE, ChmCntrl* pcntrl = NULL, bool is_ssl = false);
		virtual ~ChmEventSock();

		bool InitialAllServerStatus(void);
		bool ConnectServers(void);
		bool DoSuspend(void);

		virtual bool Clean(void);
		virtual bool UpdateInternalData(void);
		virtual bool GetEventQueueFds(event_fds_t& fds);
		virtual bool SetEventQueue(void);						// Make Socket for receiver side and add it to event queue fd.
		virtual bool UnsetEventQueue(void);						// Unset mqfd from event queue fd, and destroy it.
		virtual bool IsEventQueueFd(int fd);

		virtual bool Send(PCOMPKT pComPkt, const unsigned char* pbody, size_t blength);
		virtual bool Receive(int fd);
		virtual bool NotifyHup(int fd);
		virtual bool Processing(PCOMPKT pComPkt);

		bool PxComSendResUpdateData(chmpxid_t chmpxid, size_t length, const unsigned char* pdata, const struct timespec* pts);
		bool PxComSendResultUpdateData(chmpxid_t chmpxid, reqidmapflag_t result_updatedata);
};

#endif	// CHMEVENTSOCK_H

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
