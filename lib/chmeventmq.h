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
#ifndef	CHMEVENTMQ_H
#define	CHMEVENTMQ_H

#include <mqueue.h>
#include <map>

#include "chmeventbase.h"
#include "chmconf.h"
#include "chmthread.h"
#include "chmpx.h"
#include "chmstructure.tcc"
#include "chmlockmap.tcc"

//---------------------------------------------------------
// ABOUT MQUEUE MESSAGING
//
// Sending/Receiving messages on mqueue is following rule.
// * Sending data on mqueue is only sender msgid.
// * Message body is stored on k2hash by keys which are
//   "<from msgid>-<to msgid>.XXXX".
// * Message body on k2hash are two type, one is message
//   head(COMPKT), the other is real message body.
//
// Messaging on MQ is synchronous communication. This is
// very important.
// And create/destroy client side MQ by each communication.
// It is probably not good performance, but it is not large
// deteriorated.
//
// CHMPX side:
// * Create one MQ as first msgid in base msgid in CHMSHM.
//   And add and wait event from epoll.
//
// Client side:
// * Create one MQ as msgid which is not assigned yet.
//   It is doing by each attach request on client.
// * When sending, set a message with header in K2HASH and
//   send msgid which means client MQ id to CHMPX MQ.
//   After that, wait response through client MQ from CHMPX.
//   After receiving, close and remove own MQ.
//

//---------------------------------------------------------
// Symbols
//---------------------------------------------------------
#define	PXCLTPKT_AUTO_LENGTH			(-1)
#define	CHMEVENTMQ_RETRY_DEFAULT		(-1)
#define	CHMEVENTMQ_TIMEOUT_DEFAULT		(0)

//---------------------------------------------------------
// Structure
//---------------------------------------------------------
typedef struct chm_composed_msgid{
	msgid_t		msgid;
	serial_t	number;
	bool		is_ack;
	bool		is_ack_success;

	chm_composed_msgid(void) : msgid(CHM_INVALID_MSGID), number(MIN_MQ_SERIAL_NUMBER), is_ack(false), is_ack_success(false) {}

}COMPOSEDMSGID, *PCOMPOSEDMSGID;

typedef struct chm_update_data_param{
	chmpxid_t		chmpxid;					// requester chmpx
	struct timespec	startts;					// start time for update datas
	struct timespec	endts;						// end time for update datas
	chmhash_t		max_pending_hash;			// max pending hash value
	chmhash_t		pending_hash;				// new(pending) hash value
	chmhash_t		max_base_hash;				// max base hash value
	chmhash_t		base_hash;					// base hash value
	long			replica_count;				// replica count
	bool			is_expire_check;			// whether checking expire time
}CHM_UPDATA_PARAM, *PCHM_UPDATA_PARAM;

typedef std::vector<PCHM_UPDATA_PARAM>		mqmparamlist_t;

//---------------------------------------------------------
// Typedefs
//---------------------------------------------------------
typedef chm_lock_map<msgid_t, mqd_t>	msgid_mq_map_t;
typedef chm_lock_map<mqd_t, msgid_t>	mq_msgid_map_t;
typedef std::vector<mqd_t>				mqfd_list_t;

//---------------------------------------------------------
// ChmEventMq Class
//---------------------------------------------------------
class ChmEventMq : public ChmEventBase
{
	public:
		static const int	DEFAULT_RETRYCOUNT		= 3;
		static const long	DEFAULT_TIMEOUT_US		= 10 * 1000;	// 10ms
		static const int	DEFAULT_MQ_THREAD_CNT	= 0;			// MQ processing thread count
		static const long	DEFAULT_GETLTS_SLEEP	= 1000;			// 1us sleep time for checking the result of getting last update time
		static const long	DEFAULT_GETLTS_LOOP		= 1000 * 1000;	// loop count for checking the result of getting last update time
		static const time_t	DEFAULT_MERGE_TIMEOUT	= 0;			// timeout value for merging(default is no timeout)

	protected:
		msgid_mq_map_t		recv_idfd_map;				// map all MQ for receiving:			msgid -> fd
		mq_msgid_map_t		recv_fdid_map;				// map all MQ for receiving:			fd -> msgid
		msgid_mq_map_t		reserve_idfd_map;			// map disactivated MQ for receiving:	msgid -> fd
		msgid_mq_map_t		dest_idfd_map;				// map cache MQ for sending:			msgid -> fd
		mq_msgid_map_t		dest_fdid_map;				// map cache MQ for sending:			fd -> msgid
		serial_t			serial_num;					// serial number for each mq message(using autolock)
		volatile int		actmq_lockval;				// lock variable for ActivateMQ
		ChmThread			procthreads;				// processing threads
		ChmThread			mergethread;				// merge thread
		chm_merge_get_cb	mgetfunc;					// merge callback for getting each data
		chm_merge_set_cb	msetfunc;					// merge callback for setting each data
		chm_merge_lastts_cb	mlastfunc;					// merge callback for getting lastupdate time
		volatile bool		notify_merge_get;			// flag for receiving lastest update time
		struct timespec*	pres_lasttime;				// temporary buffer for receiving lastest update time
		volatile int		mparam_list_lockval;		// lock variable for merge_param_list
		mqmparamlist_t		merge_param_list;			// update data(merge) parameter list
		volatile bool		notify_merge_update;		// flag for running thread
		int					retry_count;				// cache value for ChmIMData::GetMQRetryCnt()
		long				timeout_us;					// cache value for ChmIMData::GetMQTimeout()
		time_t				timeout_merge;				// cache value for ChmIMData::GetMergeTimeout()
		bool				is_server_mode;				// cache value for ChmIMData::IsServerMode()
		bool				use_mq_ack;					// cache value for ChmIMData::IsAckMQ()

	protected:
		static bool MakeK2hashKey(msgid_t dept_msgid, msgid_t term_msgid, serial_t serial, std::string& headkey, std::string& bodykey);

		// Merge
		static bool MergeWorkerFunc(void* common_param, chmthparam_t wp_param);

		// callback functions for thread class
		static bool ReceiveWorkerProc(void* common_param, chmthparam_t wp_param);

		// callback for mapping
		static bool RcvfdIdMapCallback(const mq_msgid_map_t::iterator& iter, void* pparam);
		static bool DestfdIdMapCallback(const mq_msgid_map_t::iterator& iter, void* pparam);

		bool RawReceive(mqd_t mqfd, const COMPOSEDMSGID& composed);

		serial_t GetSerialNumber(void);
		msgid_t ComposeSerialMsgid(const COMPOSEDMSGID& composed);
		msgid_t ComposeSerialMsgid(const msgid_t msgid, const serial_t serial, bool is_ack = false, bool ack_success = false);
		bool DecomposeSerialMsgid(const msgid_t msgid, COMPOSEDMSGID& composed);

		bool BuildC2CHeadEx(PCOMHEAD pComHead, PCOMHEAD pReqComHead, chmpxid_t tochmpxid, chmhash_t hash, c2ctype_t c2ctype, msgid_t frommsgid);
		PCOMPKT AllocatePxCltPacket(pxclttype_t type, ssize_t ext_length = PXCLTPKT_AUTO_LENGTH);
		bool MakeMqPath(msgid_t msgid, std::string& path);
		msgid_t GetAssignedMsgId(int fd = CHM_INVALID_HANDLE);
		bool FreeReserveMsgIds(void);
		bool FreeMsgIds(msgid_t msgid, mqd_t mqfd);
		mqd_t OpenDestMQ(msgid_t msgid);

		bool PxCltReceiveMergeGetLastTime(PCOMHEAD pComHead, PPXCLT_ALL pCltAll);
		bool PxCltSendMergeResponseLastTime(const struct timespec& lastts);
		bool PxCltReceiveMergeResponseLastTime(PCOMHEAD pComHead, PPXCLT_ALL pCltAll);
		bool PxCltReceiveRequestUpdateData(PCOMHEAD pComHead, PPXCLT_ALL pCltAll);
		bool PxCltSendResponseUpdateData(chmpxid_t requester_chmpxid, const PCHMBIN pbindata, const struct timespec* pts);
		bool PxCltReceiveResponseUpdateData(PCOMHEAD pComHead, PPXCLT_ALL pCltAll);
		bool PxCltReceiveSetUpdateData(PCOMHEAD pComHead, PPXCLT_ALL pCltAll);
		bool PxCltSendResultUpdateData(chmpxid_t chmpxid, bool result);
		bool PxCltReceiveResultUpdateData(PCOMHEAD pComHead, PPXCLT_ALL pCltAll);
		bool PxCltReceiveAbortUpdateData(PCOMHEAD pComHead, PPXCLT_ALL pCltAll);
		bool PxCltSendCloseNotify(msgidlist_t& msgids);
		bool PxCltReceiveCloseNotify(PCOMHEAD pComHead, PPXCLT_ALL pCltAll);

	public:
		static bool InitializeMaxMqSystemSize(long maxmsg);

		explicit ChmEventMq(int eventqfd = CHM_INVALID_HANDLE, ChmCntrl* pcntrl = NULL, chm_merge_get_cb mgetfp = NULL, chm_merge_set_cb msetfp = NULL, chm_merge_lastts_cb mlastfp = NULL);
		virtual ~ChmEventMq();

		virtual bool Clean(void);
		virtual bool UpdateInternalData(void);

		int GetEventQueueFd(void);
		virtual bool GetEventQueueFds(event_fds_t& fds);
		virtual bool SetEventQueue(void);						// Make one MQ for receiver side and add it to event queue fd.
		virtual bool UnsetEventQueue(void);						// Unset mqfd from event queue fd, and destroy it.
		virtual bool IsEventQueueFd(int fd);

		virtual bool Send(PCOMPKT pComPkt, const unsigned char* pbody, size_t blength);
		virtual bool Receive(int fd);
		virtual bool NotifyHup(int fd);
		virtual bool Processing(PCOMPKT pComPkt);

		bool CheckProcessing(PCOMPKT pComPkt);					// for client on slave chmpx to process other than COM_C2C
		msgid_t ActivatedMsgId(void);							// for client on slave chmpx
		bool DisactivatedMsgId(msgid_t msgid);					// for client on slave chmpx

		// ack
		bool SendAck(const COMPOSEDMSGID& composed, bool is_success);
		bool ReceiveAck(msgid_t msgid);

		bool ReceiveComposedMsgid(int fd, COMPOSEDMSGID& composed, bool* pCanContinueWait = NULL);
		bool ReceiveHead(int fd, const COMPOSEDMSGID& composed, PCOMPKT* ppComPkt);
		bool ReceiveHeadByMsgId(msgid_t msgid, PCOMPKT* ppComPkt, bool* pCanContinueWait = NULL);
		bool ReceiveBody(PCOMPKT pComPkt, unsigned char** ppbody, size_t& blength, bool is_remove_body = true);
		PCOMPKT ReceiveBody(PCOMPKT pComPkt, bool is_remove_body);
		bool RemoveReceivedBody(PCOMPKT pComPkt);

		bool BuildC2CResponseHead(PCOMHEAD pComHead, PCOMHEAD pReqComHead);
		bool BuildC2CSendHead(PCOMHEAD pComHead, chmpxid_t tochmpxid, chmhash_t hash, c2ctype_t c2ctype = COM_C2C_ROUTING, msgid_t frommsgid = CHM_INVALID_MSGID);

		bool CloseDestMQ(msgid_t msgid);

		bool PxCltSendMergeGetLastTime(struct timespec& lastts);
		bool PxCltSendRequestUpdateData(const PPXCOMMON_MERGE_PARAM pmerge_param);
		bool PxCltSendSetUpdateData(size_t length, const unsigned char* pdata, const struct timespec* pts);
		bool PxCltSendAbortUpdateData(void);
		bool PxCltSendJoinNotify(pid_t pid);
};

#endif	// CHMEVENTMQ_H

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
