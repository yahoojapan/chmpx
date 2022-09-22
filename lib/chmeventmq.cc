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
#include <mqueue.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/epoll.h>
#include <assert.h>
#include <string>

#include "chmcommon.h"
#include "chmeventmq.h"
#include "chmcntrl.h"
#include "chmimdata.h"
#include "chmutil.h"
#include "chmlock.h"
#include "chmdbg.h"
#include "chmstructure.tcc"

using namespace	std;

//---------------------------------------------------------
// Symbols
//---------------------------------------------------------
#define	PROCFILE_FOR_MQ					"/proc/sys/fs/mqueue/msg_max"

#define	PARSER_K2HASH_MSGIDS			"-"
#define	SUFFIX_K2HASH_HEADKEY			".HEAD"
#define SUFFIX_K2HASH_BODYKEY			".BODY"

#define	CHMEVMQ_PROC_THREAD_NAME		"ChmEventMq-Proc"
#define	CHMEVMQ_MERGE_THREAD_NAME		"ChmEventMq-Merge"

//---------------------------------------------------------
// Class variables
//---------------------------------------------------------
const int	ChmEventMq::DEFAULT_RETRYCOUNT;
const long	ChmEventMq::DEFAULT_TIMEOUT_US;
const int	ChmEventMq::DEFAULT_MQ_THREAD_CNT;
const long	ChmEventMq::DEFAULT_GETLTS_SLEEP;
const long	ChmEventMq::DEFAULT_GETLTS_LOOP;
const long	ChmEventMq::DEFAULT_MERGE_TIMEOUT;

//---------------------------------------------------------
// Utility (for performance)
//---------------------------------------------------------
// [NOTE]
// For convert binary(uint64_t) data to string.
// We can use to_hexstring(in fullock) function, but we need performance.
// So should use this low level function for msgid_t and serial_t.
// BE CAREFUL ABOUT this function does not check input variables.
// (buff MUST be over 17 bytes)
//
inline char* cvt_hexstring(char* buff, uint64_t bin)
{
	sprintf(buff, "%016" PRIx64 , bin);
	return buff;
}

//---------------------------------------------------------
// Class Methods
//---------------------------------------------------------
bool ChmEventMq::MakeK2hashKey(msgid_t dept_msgid, msgid_t term_msgid, serial_t serial, string& headkey, string& bodykey)
{
	if(CHM_INVALID_MSGID == dept_msgid || CHM_INVALID_MSGID == term_msgid){
		ERR_CHMPRN("Invalid MSGID.");
		return false;
	}
	char	szBuff[24];		// need over 17 bytes
	headkey = cvt_hexstring(szBuff, dept_msgid);
	headkey += PARSER_K2HASH_MSGIDS;
	headkey += cvt_hexstring(szBuff, term_msgid);
	headkey += PARSER_K2HASH_MSGIDS;
	headkey += cvt_hexstring(szBuff, serial);

	bodykey = headkey;

	headkey += SUFFIX_K2HASH_HEADKEY;
	bodykey += SUFFIX_K2HASH_BODYKEY;
	return true;
}

//
// Try to set rlimit(if user is root) for request msg size.
// And if not root user, check current msg size.
//
bool ChmEventMq::InitializeMaxMqSystemSize(long maxmsg)
{
	if(0 == geteuid()){
		// root
		struct rlimit	mylimit;
		memset(&mylimit, 0, sizeof(struct rlimit));

		if(-1 == getrlimit(RLIMIT_MSGQUEUE, &mylimit)){
			ERR_CHMPRN("Could not get rlimit. errno=%d", errno);
			return false;
		}
		MSG_CHMPRN("NOW: hard limit = %jd, soft limit = %jd", static_cast<intmax_t>(mylimit.rlim_max), static_cast<intmax_t>(mylimit.rlim_cur));

		// check current value
		if(mylimit.rlim_cur < (maxmsg * (sizeof(struct msg_msg*) + 8/*mqattr.mq_msgsize*/))){
			// set new current value
			mylimit.rlim_cur = maxmsg * (sizeof(struct msg_msg*) + 8/*mqattr.mq_msgsize*/);
			if(-1 == setrlimit(RLIMIT_MSGQUEUE, &mylimit)){
				ERR_CHMPRN("Could not set rlimit. errno=%d", errno);
				return false;
			}
			// check to read
			if(-1 == getrlimit(RLIMIT_MSGQUEUE, &mylimit)){
				ERR_CHMPRN("Could not get rlimit. errno=%d", errno);
				return false;
			}
			MSG_CHMPRN("NEW: hard limit = %zd, soft limit = %zd", mylimit.rlim_max, mylimit.rlim_cur);

			// check new value
			if(mylimit.rlim_cur < (maxmsg * (sizeof(struct msg_msg*) + 8/*mqattr.mq_msgsize*/))){
				ERR_CHMPRN("Could not set rlimit for request.");
				return false;
			}
		}
	}else{
		// not root
		int		fd;
		if(CHM_INVALID_HANDLE == (fd = open(PROCFILE_FOR_MQ, O_RDONLY))){
			ERR_CHMPRN("Could not open file %s. errno=%d", PROCFILE_FOR_MQ, errno);
			return false;
		}
		// read current limit
		char*	pbuff;
		size_t	length = 0L;
		if(NULL == (pbuff = reinterpret_cast<char*>(chm_read(fd, &length)))){
			ERR_CHMPRN("Could not read file %s", PROCFILE_FOR_MQ);
			// cppcheck-suppress unmatchedSuppression
			// cppcheck-suppress unreadVariable
			CHM_CLOSE(fd);
			return false;
		}
		long	current = static_cast<long>(atoi(pbuff));
		CHM_Free(pbuff);
		// cppcheck-suppress unmatchedSuppression
		// cppcheck-suppress unreadVariable
		CHM_CLOSE(fd);

		if(current < maxmsg){
			ERR_CHMPRN("Current msg %ld is too small for specified msg %ld(MAXCLIENT in GLOBAL section).", current, maxmsg);
			ERR_CHMPRN("\n\nYou should set current value(%ld) to MAXCLIENT, or MAXCLIENT(%ld) to %s(ex. # echo %ld > %s ).", current, maxmsg, PROCFILE_FOR_MQ, maxmsg, PROCFILE_FOR_MQ);
			return false;
		}
	}
	return true;
}

//---------------------------------------------------------
// Class Methods - Processing
//---------------------------------------------------------
bool ChmEventMq::MergeWorkerFunc(void* common_param, chmthparam_t wp_param)
{
	if(!common_param){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	ChmEventMq*	pThis = reinterpret_cast<ChmEventMq*>(common_param);

	if(!pThis->mgetfunc){
		ERR_CHMPRN("Why, does not set mgetfunc.");
		return false;
	}
	MSG_CHMPRN("Start merge thread in client(MQ).");

	// Loop
	chmpx_h	handle = reinterpret_cast<chmpx_h>(pThis->pChmCntrl);
	while(pThis->notify_merge_update){
		PCHM_UPDATA_PARAM	pUpdateDataParam;
		time_t				start_time;
		bool				result;

		// get parameter
		while(!fullock::flck_trylock_noshared_mutex(&(pThis->mparam_list_lockval)));	// LOCK
		pUpdateDataParam = NULL;
		if(0 < pThis->merge_param_list.size()){
			pUpdateDataParam = pThis->merge_param_list.front();
			pThis->merge_param_list.erase(pThis->merge_param_list.begin());
		}
		fullock::flck_unlock_noshared_mutex(&(pThis->mparam_list_lockval));				// UNLOCK

		if(!pUpdateDataParam){
			break;
		}
		// copy param
		CHM_MERGE_GETPARAM	getparam;
		// cppcheck-suppress unmatchedSuppression
		// cppcheck-suppress unreadVariable
		getparam.starthash			= 0;									// must be start position 0
		getparam.startts.tv_nsec	= pUpdateDataParam->startts.tv_nsec;
		getparam.startts.tv_sec		= pUpdateDataParam->startts.tv_sec;
		getparam.endts.tv_nsec		= pUpdateDataParam->endts.tv_nsec;
		getparam.endts.tv_sec		= pUpdateDataParam->endts.tv_sec;
		getparam.target_hash		= pUpdateDataParam->pending_hash;
		getparam.target_max_hash	= pUpdateDataParam->max_pending_hash;
		getparam.old_hash			= pUpdateDataParam->base_hash;
		getparam.old_max_hash		= pUpdateDataParam->max_base_hash;
		getparam.target_hash_range	= pUpdateDataParam->replica_count;
		getparam.is_expire_check	= pUpdateDataParam->is_expire_check;

		// Loop for each hash
		chmhash_t	nexthash	= 0;
		PCHMBIN		pdatas;
		size_t		datacnt;
		long		loop_count	= 0;
		start_time				= time(NULL);
		result					= true;
		while(pThis->notify_merge_update){
			// make start hash
			getparam.starthash = nexthash;

			// get target hash datas
			nexthash= 0;
			pdatas	= NULL;
			datacnt	= 0;
			if(!pThis->mgetfunc(handle, &getparam, &nexthash, &pdatas, &datacnt)){
				WAN_CHMPRN("Failed to get hash datas, probably no more data.");
				break;
			}else if(0 == datacnt){
				MSG_CHMPRN("Finish to get datas.");
				break;
			}

			// loop for sending got datas
			for(size_t cnt = 0; cnt < datacnt; ++cnt){
				// transfer
				if(!pThis->PxCltSendResponseUpdateData(pUpdateDataParam->chmpxid, &pdatas[cnt], &(getparam.endts))){
					ERR_CHMPRN("Failed to send PXCLT_RES_UPDATEDATA, but continue...");
					result = false;
				}
			}
			// clean
			CHM_FREE_CHMBINS(pdatas, datacnt);

			// [FIXME]
			// This logic is not good...
			//
			if(0 == ((++loop_count) % 10000) && ChmEventMq::DEFAULT_MERGE_TIMEOUT != pThis->timeout_merge){
				if((start_time + pThis->timeout_merge) < time(NULL)){
					ERR_CHMPRN("Timeouted(%zu) merging.", pThis->timeout_merge);
					result = false;
					break;
				}
			}

			// check reaching end
			if(0 == nexthash){
				MSG_CHMPRN("Finish to get datas by reach end.");
				break;
			}
		}

		// Finish to update data, so send result
		if(!pThis->PxCltSendResultUpdateData(pUpdateDataParam->chmpxid, (pThis->notify_merge_update && result))){
			ERR_CHMPRN("Failed to send CHMPX_CLT_RESULT_UPDATEDATA, but continue...");
		}

		// clean
		CHM_Delete(pUpdateDataParam);
	}
	MSG_CHMPRN("Finish merge thread in client(MQ).");
	return true;
}

bool ChmEventMq::ReceiveWorkerProc(void* common_param, chmthparam_t wp_param)
{
	ChmEventMq*	pMqObj	= reinterpret_cast<ChmEventMq*>(common_param);
	mqd_t		mqfd	= static_cast<mqd_t>(wp_param);
	if(!pMqObj || CHM_INVALID_HANDLE == mqfd){
		ERR_CHMPRN("Parameters are wrong.");
		return true;		// sleep thread
	}

	// get composed msgid
	COMPOSEDMSGID	composed;
	while(pMqObj->ReceiveComposedMsgid(mqfd, composed)){
		// Processing
		if(false == pMqObj->RawReceive(mqfd, composed)){
			WAN_CHMPRN("Failed receiving and to processing for msgid(0x%016" PRIx64 ") at MQ(%d) in worker thread, probably nothing to receive.", composed.msgid, mqfd);
		}
	}
	//MSG_CHMPRN("There is no read composed msgid from MQ(%d).", pProcParam->mqfd);

	return true;		// always return true for continue to work.
}

//---------------------------------------------------------
// Class Methods - Lock map
//---------------------------------------------------------
bool ChmEventMq::RcvfdIdMapCallback(const mq_msgid_map_t::iterator& iter, void* pparam)
{
	ChmEventMq*	pMqObj = reinterpret_cast<ChmEventMq*>(pparam);
	if(!pMqObj){
		ERR_CHMPRN("Parameter is wrong.");
		return true;										// do not stop loop.
	}
	mqd_t	mqfd	= iter->first;
	msgid_t	msgid	= iter->second;

	// close MQ & delete epoll event & msgid free
	if(!pMqObj->FreeMsgIds(msgid, mqfd)){
		WAN_CHMPRN("Failed some phase to close Failed MQ fd(%d)/msgid(0x%016" PRIx64 ").", mqfd, msgid);
	}

	// remove recv_idfd_map mapping, because this callback is for recv_fdid_map.
	pMqObj->recv_idfd_map.erase(msgid);

	return true;
}

bool ChmEventMq::DestfdIdMapCallback(const mq_msgid_map_t::iterator& iter, void* pparam)
{
	ChmEventMq*	pMqObj = reinterpret_cast<ChmEventMq*>(pparam);
	if(!pMqObj){
		ERR_CHMPRN("Parameter is wrong.");
		return true;										// do not stop loop.
	}
	mqd_t	mqfd	= iter->first;
	msgid_t	msgid	= iter->second;

	// close MQ
	if(-1 == mq_close(mqfd)){
		ERR_CHMPRN("MQ fd(%d) for msgid(0x%016" PRIx64 ") could not be closed: errno=%d.", mqfd, msgid, errno);
	}

	// remove dest_idfd_map mapping, because this callback is for dest_fdid_map.
	pMqObj->dest_idfd_map.erase(msgid);

	return true;
}

//---------------------------------------------------------
// Methods
//---------------------------------------------------------
ChmEventMq::ChmEventMq(int eventqfd, ChmCntrl* pcntrl, chm_merge_get_cb mgetfp, chm_merge_set_cb msetfp, chm_merge_lastts_cb mlastfp) :
	ChmEventBase(eventqfd, pcntrl),
	recv_idfd_map(CHM_INVALID_HANDLE),
	recv_fdid_map(CHM_INVALID_MSGID, ChmEventMq::RcvfdIdMapCallback, this),
	reserve_idfd_map(CHM_INVALID_HANDLE),
	dest_idfd_map(CHM_INVALID_HANDLE),
	dest_fdid_map(CHM_INVALID_MSGID, ChmEventMq::DestfdIdMapCallback, this),
	serial_num(MIN_MQ_SERIAL_NUMBER), actmq_lockval(FLCK_NOSHARED_MUTEX_VAL_UNLOCKED),
	procthreads(CHMEVMQ_PROC_THREAD_NAME), mergethread(CHMEVMQ_MERGE_THREAD_NAME), mgetfunc(mgetfp), msetfunc(msetfp), mlastfunc(mlastfp),
	notify_merge_get(true), pres_lasttime(NULL), mparam_list_lockval(FLCK_NOSHARED_MUTEX_VAL_UNLOCKED), notify_merge_update(false),
	retry_count(ChmEventMq::DEFAULT_RETRYCOUNT), timeout_us(ChmEventMq::DEFAULT_TIMEOUT_US), timeout_merge(ChmEventMq::DEFAULT_MERGE_TIMEOUT),
	is_server_mode(false), use_mq_ack(true)
{
	assert(pChmCntrl);

	// make a seed for serial number
	struct timespec	tmpts;
	if(-1 == clock_gettime(CLOCK_BOOTTIME, &tmpts)){
		WAN_CHMPRN("Failed to make seed to serial number, but use time() instead of it.");
		serial_num = static_cast<serial_t>(time(NULL) & MASK64_LOWBIT);
	}else{
		serial_num = static_cast<serial_t>(tmpts.tv_nsec & MASK64_LOWBIT);
	}

	// initialize cache value and threads
	//
	// [NOTE]
	// ChmEventMq must be initialized after initialize ChmIMData
	//
	if(!ChmEventMq::UpdateInternalData()){
		ERR_CHMPRN("Could not initialize cache data for ImData, but continue...");
	}
}

ChmEventMq::~ChmEventMq()
{
	ChmEventMq::Clean();
}

//---------------------------------------------------------
// Methods for Serial number
//---------------------------------------------------------
//
// [NOTE]
// Do not need to lock for serial_num, because NUM will not be allocated to
// the same MQ at same time.
//
serial_t ChmEventMq::GetSerialNumber(void)
{
	serial_t	number = serial_num;
	if(MAX_MQ_SERIAL_NUMBER < ++serial_num){
		serial_num = MIN_MQ_SERIAL_NUMBER;
	}
	return number;
}

msgid_t ChmEventMq::ComposeSerialMsgid(const COMPOSEDMSGID& composed)
{
	return ComposeSerialMsgid(composed.msgid, composed.number, composed.is_ack, composed.is_ack_success);
}

msgid_t ChmEventMq::ComposeSerialMsgid(const msgid_t msgid, const serial_t serial, bool is_ack, bool ack_success)
{
	if(IsEmpty()){
		return CHM_INVALID_MSGID;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	return COMPOSE64(static_cast<msgid_t>(serial | (is_ack ? (ack_success ? MQ_ACK_SUCCESS : MQ_ACK_FAILURE) : 0L)), (msgid - pImData->GetBaseMsgId()));
}

bool ChmEventMq::DecomposeSerialMsgid(const msgid_t msgid, COMPOSEDMSGID& composed)
{
	if(IsEmpty()){
		return false;
	}
	if(CHM_INVALID_MSGID == msgid){
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	serial_t	tmp		= (static_cast<serial_t>(msgid) & MASK64_HIBIT) >> 32;
	composed.msgid		= pImData->GetBaseMsgId() + (msgid & MASK64_LOWBIT);
	composed.number		= GET_MQ_SERIAL_NUMBER(tmp);

	if(IS_MQ_ACK(tmp)){
		composed.is_ack			= true;
		composed.is_ack_success	= IS_MQ_ACK_SUCCESS(tmp);
	}else{
		composed.is_ack			= false;
		composed.is_ack_success	= false;
	}
	return true;
}

//---------------------------------------------------------
// Methods for msgids
//---------------------------------------------------------
bool ChmEventMq::Clean(void)
{
	if(IsEmpty()){
		return true;
	}
	// exit merge thread assap
	notify_merge_update = false;
	if(!mergethread.ExitAllThreads()){
		ERR_CHMPRN("Failed to exit thread for merging.");
	}
	// If waiting to receive command, exit assap
	notify_merge_get = true;

	bool	result	= true;
	ChmLock	AutoLock(CHMLT_MQOBJ, CHMLT_WRITE);				// Lock

	// get all mqfd
	msgidlist_t	freed_msgids;
	recv_idfd_map.get_keys(freed_msgids);

	// close all MQ/msgid for receiving.
	recv_fdid_map.clear();				// remove all with recv_idfd_map

	// remove all reserve map
	reserve_idfd_map.clear();

	// Send PX(C) close notify msgids which are disactivated.
	if(0 < freed_msgids.size()){
		if(!PxCltSendCloseNotify(freed_msgids)){
			ERR_CHMPRN("Failed to send close notify, but continue...");
			result = false;
		}
	}

	// close all cache mqfd for destination.
	dest_fdid_map.clear();				// remove with dest_idfd_map

	AutoLock.UnLock();										// Unlock

	if(!ChmEventBase::Clean()){
		result = false;
	}
	return result;
}

//
// initialize/reinitialize cache value and threads
//
bool ChmEventMq::UpdateInternalData(void)
{
	ChmIMData*	pImData;
	if(!pChmCntrl || NULL == (pImData = pChmCntrl->GetImDataObj())){
		ERR_CHMPRN("Object is not pre-initialized.");
		return false;
	}

	// processing thread
	int	conf_thread_cnt	= (pImData->IsChmpxProcess() ? pImData->GetMQThreadCount() : DEFAULT_MQ_THREAD_CNT);	// we do not need to cache this value.
	int	now_thread_cnt	= procthreads.GetThreadCount();
	if(conf_thread_cnt < now_thread_cnt){
		if(DEFAULT_MQ_THREAD_CNT == conf_thread_cnt){
			// stop all
			if(!procthreads.ExitAllThreads()){
				ERR_CHMPRN("Failed to exit thread for MQ processing.");
			}else{
				MSG_CHMPRN("stop all MQ processing thread(%d).", now_thread_cnt);
			}
		}else{
			// need to stop some thread
			if(!procthreads.ExitThreads(now_thread_cnt - conf_thread_cnt)){
				ERR_CHMPRN("Failed to exit thread for MQ processing.");
			}else{
				MSG_CHMPRN("stop MQ processing thread(%d - %d = %d).", now_thread_cnt, conf_thread_cnt, now_thread_cnt - conf_thread_cnt);
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
		if(!procthreads.CreateThreads(conf_thread_cnt - now_thread_cnt, ChmEventMq::ReceiveWorkerProc, NULL, this, 0, true, false, false, false)){
			ERR_CHMPRN("Failed to create thread for MQ processing, but continue...");
		}else{
			MSG_CHMPRN("start to run MQ processing thread(%d + %d = %d).", now_thread_cnt, conf_thread_cnt - now_thread_cnt, conf_thread_cnt);
		}
	}else{
		// nothing to do because of same thread count
	}

	// others
	int		tmp_retry_count		= pImData->GetMQRetryCnt();
	long	tmp_timeout_us		= pImData->GetMQTimeout();
	time_t	tmp_timeout_merge	= pImData->GetMergeTimeout();
	retry_count			= (CHMEVENTMQ_RETRY_DEFAULT == tmp_retry_count ? ChmEventMq::DEFAULT_RETRYCOUNT : tmp_retry_count);
	timeout_us			= (CHMEVENTMQ_TIMEOUT_DEFAULT == tmp_timeout_us ? ChmEventMq::DEFAULT_TIMEOUT_US : tmp_timeout_us);
	timeout_merge		= (CHMEVENTMQ_TIMEOUT_DEFAULT == tmp_timeout_merge ? ChmEventMq::DEFAULT_MERGE_TIMEOUT : tmp_timeout_merge);
	is_server_mode		= pImData->IsServerMode();
	use_mq_ack			= pImData->IsAckMQ();

	return true;
}

int ChmEventMq::GetEventQueueFd(void)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return CHM_INVALID_HANDLE;
	}
	// get all MQ fds
	mqfd_list_t	mqfds;
	recv_fdid_map.get_keys(mqfds);

	if(1 < mqfds.size()){
		MSG_CHMPRN("This object has many MQ fd.");
	}
	return (mqfds.empty() ? CHM_INVALID_HANDLE : mqfds.front());
}

bool ChmEventMq::GetEventQueueFds(event_fds_t& fds)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	fds.clear();

	// get all MQ fds
	mqfd_list_t	mqfds;
	recv_fdid_map.get_keys(mqfds);

	for(mqfd_list_t::const_iterator iter = mqfds.begin(); iter != mqfds.end(); ++iter){
		fds.push_back(*iter);
	}
	return true;
}

//
// This method assigned one(default) MQ for receiving.
// Assigned MQ count by each calling SetEventQueue() is changed by MQPERATTACH
// configuration(which get by ChmImData::GetMQPerAttach() method).
// Assigned MQ's msgid is stored in CHMSHM and set status ASSIGNED.
// If this method called by CHMPX process, assigned one MQ with
// large queue size. If called by client process, with 1 length queue.
//
bool ChmEventMq::SetEventQueue(void)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData			= pChmCntrl->GetImDataObj();
	long		mqcnt_per_attach= pChmCntrl->IsChmpxType() ? pImData->GetChmpxMQCount() : pImData->GetMQPerAttach();

	ChmLock	AutoLock(CHMLT_MQOBJ, CHMLT_WRITE);				// Lock

	// Loop by mqcnt_per_attach
	for(int cnt = 0; cnt < mqcnt_per_attach; cnt++){
		// Get new msghead & msgid
		//
		// For chmpx process, assigned msgid is activated.
		// The server side process, assigned msgid is activated.
		// The client side process, assigned msgid is DISactivated.
		//
		msgid_t	newmsgid;
		if(pChmCntrl->IsChmpxType()){
			newmsgid = pImData->AssignMsgOnChmpx();
		}else if(pChmCntrl->IsClientOnSvrType()){
			newmsgid = pImData->AssignMsgOnServer();
		}else{	// pChmCntrl->IsClientOnSlvType()
			newmsgid = pImData->AssignMsgOnSlave(false);	// disactivated
		}
		if(CHM_INVALID_MSGID == newmsgid){
			ERR_CHMPRN("Could not get new msgid area.");
			return false;
		}

		// Make MQ path
		string	mqpath;
		if(!MakeMqPath(newmsgid, mqpath)){
			ERR_CHMPRN("Failed MQ path from 0x%016" PRIx64 ".", newmsgid);
			pImData->FreeMsg(newmsgid);								// msgid status is set NOTASSIGNED
			return false;
		}

		// set umask
		mode_t	old_umask = umask(0);

		// Open(Create) MQ
		mqd_t				mqfd;
		struct mq_attr		mqattr;
		mqattr.mq_flags		= O_RDONLY | O_NONBLOCK | O_CREAT | O_EXCL;
		mqattr.mq_maxmsg	= pChmCntrl->IsChmpxType() ? pImData->GetMaxQueuePerChmpxMQ() : pImData->GetMaxQueuePerClientMQ();
		mqattr.mq_msgsize	= sizeof(msgid_t);										// mq's data is msgid_t
		mqattr.mq_curmsgs	= 0;

		if(CHM_INVALID_HANDLE == (mqfd = mq_open(mqpath.c_str(), O_RDONLY | O_NONBLOCK | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH, &mqattr))){
			if(EEXIST == errno){
				WAN_CHMPRN("Failed to open MQ(%s) by errno=EEXIST, so retry after removing it.", mqpath.c_str());

				if(-1 == mq_unlink(mqpath.c_str())){
					ERR_CHMPRN("Failed to unlink MQ(%s) by errno(%d), but retry to open...", mqpath.c_str(), errno);
				}
				// retry
				mqfd = mq_open(mqpath.c_str(), O_RDONLY | O_NONBLOCK | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH, &mqattr);

			}else if(ENOSPC == errno){
				ERR_CHMPRN("Failed to open MQ(%s) by errno=ENOSPC, maybe there is not enough \"queues_max\".", mqpath.c_str());
				ERR_CHMPRN("You can change \"queues_max\" by following example command.");
				ERR_CHMPRN("  # echo 1024 > /proc/sys/fs/mqueue/queues_max");

			}else if(EINVAL == errno){
				ERR_CHMPRN("Failed to open MQ(%s) by errno=EINVAL, maybe there is not enough \"msg_max\"(require mq_maxmsg is %ld).", mqpath.c_str(), mqattr.mq_maxmsg);
				ERR_CHMPRN("You can change \"msg_max\" by following example command.");
				ERR_CHMPRN("  # echo %ld > /proc/sys/fs/mqueue/msg_max", mqattr.mq_maxmsg);

			}else{
				ERR_CHMPRN("Failed to open MQ(%s) by errno=%d.", mqpath.c_str(), errno);
			}
			if(CHM_INVALID_HANDLE == mqfd){
				pImData->FreeMsg(newmsgid);							// msgid status is set NOTASSIGNED
				umask(old_umask);
				return false;
			}
		}

		// For debug
		memset(&mqattr, 0, sizeof(struct mq_attr));
		if(-1 == mq_getattr(mqfd, &mqattr)){
			WAN_CHMPRN("Failed to get mqattr for debugging: errno=%d, but continue.... ", errno);
		}else{
			MSG_CHMPRN("mqattr { flag=%ld, maxmsg=%ld, msgsize=%ld, curmsgs=%ld }", mqattr.mq_flags, mqattr.mq_maxmsg, mqattr.mq_msgsize, mqattr.mq_curmsgs);
		}

		umask(old_umask);

		// Add event fd
		struct epoll_event	eqevent;
		memset(&eqevent, 0, sizeof(struct epoll_event));
		eqevent.data.fd	= mqfd;
		eqevent.events	= EPOLLIN | EPOLLET | EPOLLRDHUP;			// EPOLLRDHUP is set
		if(-1 == epoll_ctl(eqfd, EPOLL_CTL_ADD, mqfd, &eqevent)){
			ERR_CHMPRN("Failed to add mqfd(%s: %d) into epoll event(%d), errno=%d", mqpath.c_str(), mqfd, eqfd, errno);
			mq_close(mqfd);
			mq_unlink(mqpath.c_str());
			pImData->FreeMsg(newmsgid);								// msgid status is set NOTASSIGNED
			return false;
		}

		// Set internal mapping.
		recv_idfd_map.set(newmsgid, mqfd, true);
		recv_fdid_map.set(mqfd, newmsgid, true);

		if(pChmCntrl->IsClientOnSlvType()){
			// disactivated MQ list
			reserve_idfd_map.set(newmsgid, mqfd);
		}
	}
	return true;
}

bool ChmEventMq::UnsetEventQueue(void)
{
	return Clean();
}

bool ChmEventMq::IsEventQueueFd(int fd)
{
	if(CHM_INVALID_HANDLE == fd){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	return recv_fdid_map.find(fd);
}

msgid_t ChmEventMq::GetAssignedMsgId(int fd)
{
	if(CHM_INVALID_HANDLE == fd){
		ERR_CHMPRN("Parameter is wrong.");
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return CHM_INVALID_MSGID;
	}
	return recv_fdid_map.get(fd);	// if there is not fd, get() returns default value which is specified constructor.
}

bool ChmEventMq::MakeMqPath(msgid_t msgid, string& path)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	if(CHM_INVALID_MSGID == msgid){
		ERR_CHMPRN("Invalid MSGID.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();
	string		group;
	if(!pImData->GetGroup(group)){
		ERR_CHMPRN("Could not get Group name.");
		return false;
	}
	char	szBuff[24];		// need over 17 bytes
	path = "/";
	path += group;
	path += ".";
	path += cvt_hexstring(szBuff, msgid);

	return true;
}

//---------------------------------------------------------
// Methods for Managing MQ status
//---------------------------------------------------------
msgid_t ChmEventMq::ActivatedMsgId(void)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return CHM_INVALID_MSGID;
	}
	if(!pChmCntrl->IsClientOnSlvType()){
		ERR_CHMPRN("This process is not a process joining slave chmpx.");
		return CHM_INVALID_MSGID;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	while(!fullock::flck_trylock_noshared_mutex(&actmq_lockval));	// LOCK

	// does have reserve MQ?
	if(0 == reserve_idfd_map.count()){
		// get reserve MQ
		if(!SetEventQueue() || 0 == reserve_idfd_map.count()){
			ERR_CHMPRN("Failed to increase reserve MQ.");
			fullock::flck_unlock_noshared_mutex(&actmq_lockval);	// UNLOCK
			return CHM_INVALID_MSGID;
		}
	}

	// get msgid from reserve MQ
	msgidlist_t	msgids;
	reserve_idfd_map.get_keys(msgids);
	if(msgids.empty() || CHM_INVALID_MSGID == msgids.front()){
		ERR_CHMPRN("There is no reserve MQ.");
		fullock::flck_unlock_noshared_mutex(&actmq_lockval);		// UNLOCK
		return CHM_INVALID_MSGID;
	}
	msgid_t	msgid = msgids.front();

	// set activated flag.
	if(!pImData->ActivateMsg(msgid)){
		ERR_CHMPRN("Could not set activated status to msgid(0x%016" PRIx64 ").", msgid);
		fullock::flck_unlock_noshared_mutex(&actmq_lockval);		// UNLOCK
		return CHM_INVALID_MSGID;
	}

	// remove msgid from reserve list.
	reserve_idfd_map.erase(msgid);
	fullock::flck_unlock_noshared_mutex(&actmq_lockval);		// UNLOCK

	return msgid;
}

bool ChmEventMq::DisactivatedMsgId(msgid_t msgid)
{
	if(CHM_INVALID_MSGID == msgid){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	if(!pChmCntrl->IsClientOnSlvType()){
		ERR_CHMPRN("This process is not a process joining slave chmpx.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	ChmLock	AutoLock(CHMLT_MQOBJ, CHMLT_WRITE);				// Lock

	// get MQ fd
	mqd_t	mqfd = recv_idfd_map.find(msgid);
	if(CHM_INVALID_HANDLE == mqfd){
		ERR_CHMPRN("There is no msgid(0x%016" PRIx64 ") in MQ list.", msgid);
		return false;
	}

	// set disactivated flag.
	if(!pImData->DisactivateMsg(msgid)){
		ERR_CHMPRN("Could not set disactivated status to msgid(0x%016" PRIx64 ").", msgid);
		return false;
	}

	// set msgid to reserve MQ list.
	if(reserve_idfd_map.find(msgid)){
		WAN_CHMPRN("Already has msgid(0x%016" PRIx64 ") in reserve MQ list, but over write it.", msgid);
	}
	reserve_idfd_map.set(msgid, mqfd, true);		// allow over write

	// check & free msgid, if over reserve MQ limit.
	if(!FreeReserveMsgIds()){
		WAN_CHMPRN("Something error occured to unassign MQ, maybe leaking MQ, but continue...");
	}
	return true;
}

bool ChmEventMq::FreeReserveMsgIds(void)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	if(!pChmCntrl->IsClientOnSlvType()){
		ERR_CHMPRN("This process is not a process joining slave chmpx.");
		return false;
	}
	ChmIMData*	pImData			= pChmCntrl->GetImDataObj();

	long		mqcnt_per_attach= pImData->GetMQPerAttach();
	if(mqcnt_per_attach <= 0){
		mqcnt_per_attach = 1L;		// why?
	}

	ChmLock	AutoLock(CHMLT_MQOBJ, CHMLT_WRITE);				// Lock

	// make count for closing mq
	long	to_close_cnt;
	{
		long	activated_cnt	= static_cast<long>(recv_idfd_map.count() - reserve_idfd_map.count());
		long	lest_cnt		= ((activated_cnt / mqcnt_per_attach) * mqcnt_per_attach) + ((0 == (activated_cnt % mqcnt_per_attach)) ? 0 : 1L);
		to_close_cnt			= static_cast<long>(recv_idfd_map.count()) - lest_cnt;
		if(static_cast<long>(reserve_idfd_map.count()) < to_close_cnt){
			to_close_cnt = static_cast<long>(reserve_idfd_map.count());	// why?
		}
	}

	// do closing (use reserve_idfd_map directly)
	msgidlist_t	freed_msgids;
	while(!fullock::flck_trylock_noshared_mutex(&reserve_idfd_map.lockval));	// LOCK
	for(msgid_mq_map_t::iterator iter = reserve_idfd_map.basemap.begin(); iter != reserve_idfd_map.basemap.end() && 0 < to_close_cnt; reserve_idfd_map.basemap.erase(iter++), --to_close_cnt){
		// close MQ/msgid.
		recv_fdid_map.erase(iter->second);		// remove all with recv_idfd_map

		// backup freed msgid.
		freed_msgids.push_back(iter->first);
	}
	fullock::flck_unlock_noshared_mutex(&reserve_idfd_map.lockval);				// UNLOCK

	// Send PX(C) close notify msgids which are disactivated.
	bool	result = true;
	if(0 < freed_msgids.size()){
		if(!PxCltSendCloseNotify(freed_msgids)){
			ERR_CHMPRN("Failed to send close notify.");
			result = false;
		}
	}
	return result;
}

bool ChmEventMq::FreeMsgIds(msgid_t msgid, mqd_t mqfd)
{
	if(CHM_INVALID_MSGID == msgid || CHM_INVALID_HANDLE == mqfd){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData	= pChmCntrl->GetImDataObj();
	bool		result	= true;

	// remove mqfd from eq
	if(-1 == epoll_ctl(eqfd, EPOLL_CTL_DEL, mqfd, NULL)){
		ERR_CHMPRN("MQ fd(%d) could not be removed from eventfd(%d): errno=%d, but continue...", mqfd, eqfd, errno);
		result = false;
	}

	// Close MQ
	if(-1 == mq_close(mqfd)){
		ERR_CHMPRN("MQ fd(%d) could not be closed: errno=%d, but continue...", mqfd, errno);
		result = false;
	}

	// Remove MQ path
	string	mqpath;
	if(!MakeMqPath(msgid, mqpath)){
		ERR_CHMPRN("Failed MQ path from 0x%016" PRIx64 ", but continue...", msgid);
		result = false;
	}else{
		if(-1 == mq_unlink(mqpath.c_str())){
			ERR_CHMPRN("Failed to unlink MQ(%s) by errno(%d), but continue...", mqpath.c_str(), errno);
		}
	}

	// msgid is freed
	if(!pImData->FreeMsg(msgid)){			// msgid status is set NOTASSIGNED
		ERR_CHMPRN("Failed to remove msgid(0x%016" PRIx64 ") from assigned list.", msgid);
		result = false;
	}
	return result;
}

mqd_t ChmEventMq::OpenDestMQ(msgid_t msgid)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return CHM_INVALID_HANDLE;
	}

	ChmLock	AutoLock(CHMLT_MQOBJ, CHMLT_WRITE);			// Write Lock

	// get MQ fd from cache
	mqd_t	mqfd = dest_idfd_map.get(msgid);
	if(CHM_INVALID_HANDLE != mqfd){
		return mqfd;
	}

	// make MQ path
	string	mq_path;
	if(!MakeMqPath(msgid, mq_path)){
		ERR_CHMPRN("Failed to make mqueue path for msgid(0x%016" PRIx64 ")", msgid);
		return CHM_INVALID_HANDLE;
	}

	// open
	if(CHM_INVALID_HANDLE == (mqfd = mq_open(mq_path.c_str(), O_WRONLY | O_NONBLOCK))){
		ERR_CHMPRN("Failed to open mqueue for %s(error=%d)", mq_path.c_str(), errno);
		return CHM_INVALID_HANDLE;
	}

	// set cache
	dest_idfd_map.set(msgid, mqfd);
	dest_fdid_map.set(mqfd, msgid);

	return mqfd;
}

bool ChmEventMq::CloseDestMQ(msgid_t msgid)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmLock	AutoLock(CHMLT_MQOBJ, CHMLT_WRITE);		// Write Lock

	// search msgid in cache
	mqd_t	mqfd = dest_idfd_map.get(msgid);
	if(CHM_INVALID_HANDLE == mqfd){
		MSG_CHMPRN("Could not found MQ for msgid(0x%016" PRIx64 "), maybe already closed it.", msgid);
		return true;
	}

	// close cache mqfd for destination.
	dest_fdid_map.erase(mqfd);						// remove with dest_idfd_map

	return true;
}

//---------------------------------------------------------
// Methods for COMHEAD
//---------------------------------------------------------
//
// Make COMPKT for replying from all client.
//
bool ChmEventMq::BuildC2CResponseHead(PCOMHEAD pComHead, PCOMHEAD pReqComHead)
{
	return BuildC2CHeadEx(pComHead, pReqComHead, CHM_INVALID_CHMPXID, 0L, COM_C2C_NORMAL, CHM_INVALID_MSGID);
}

//
// Make COMPKT for sending from all client.
//
bool ChmEventMq::BuildC2CSendHead(PCOMHEAD pComHead, chmpxid_t tochmpxid, chmhash_t hash, c2ctype_t c2ctype, msgid_t frommsgid)
{
	return BuildC2CHeadEx(pComHead, NULL, tochmpxid, hash, c2ctype, frommsgid);
}

bool ChmEventMq::BuildC2CHeadEx(PCOMHEAD pComHead, PCOMHEAD pReqComHead, chmpxid_t tochmpxid, chmhash_t hash, c2ctype_t c2ctype, msgid_t frommsgid)
{
	if(!pComHead){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	if(pChmCntrl->IsChmpxType()){
		// This method for client processes.
		ERR_CHMPRN("Starting to send/reply C2C message from CHMPX process.");
		return false;
	}

	if(!pReqComHead){
		// Send New C2C Message type(departure is this)
		if(pChmCntrl->IsChmpxType()){
			// must start client, peer end chmpx
			ERR_CHMPRN("Something wrong, because starting C2C message from CHMPX process.");
			return false;
		}
		if(CHM_INVALID_MSGID == frommsgid){
			if(pChmCntrl->IsClientOnSlvType()){
				// client on slave chmpx proc, must set frommsgid.
				ERR_CHMPRN("Invalid msgid for starting to send C2C.");
				return false;
			}
			if(CHM_INVALID_MSGID == (frommsgid = pImData->GetRandomClientMsgId())){
				ERR_CHMPRN("Could not get new msgid for starting to send C2C.");
				return false;
			}
		}else{
			if(!pImData->IsMsgidActivated(frommsgid)){
				ERR_CHMPRN("Departure msgid(0x%016" PRIx64 ") is not activated.", frommsgid);
				return false;
			}
		}

		chmpxid_t	selfchmpxid		= pImData->GetSelfChmpxId();
		msgid_t		term_msgid		= CHM_INVALID_MSGID;					// C2C cannot decide msgid.
		msgid_t		peer_term_msgid	= pImData->GetRandomChmpxMsgId();		// Get msgid in chmpx process

		if(CHM_INVALID_CHMPXID == selfchmpxid || CHM_INVALID_MSGID == peer_term_msgid){
			ERR_CHMPRN("Could not get self chmpxid(0x%016" PRIx64 ") or peer msgid(0x%016" PRIx64 ")", selfchmpxid, peer_term_msgid);
			return false;
		}
		pComHead->version			= COM_VERSION_1;
		pComHead->type				= COM_C2C;
		pComHead->c2ctype			= c2ctype;
		pComHead->dept_ids.chmpxid	= selfchmpxid;
		pComHead->dept_ids.msgid	= frommsgid;
		pComHead->term_ids.chmpxid	= tochmpxid;		// allow tochmpxid is CHM_INVALID_CHMPXID on not server mode.
		pComHead->term_ids.msgid	= term_msgid;
		pComHead->peer_dept_msgid	= frommsgid;
		pComHead->peer_term_msgid	= peer_term_msgid;
		pComHead->mq_serial_num		= GetSerialNumber();
		pComHead->hash				= hash;

	}else{
		// COMHEAD is response for pReqComHead
		if(COM_C2C != pReqComHead->type){
			ERR_CHMPRN("For response, the request type(%s) is not COM_C2C.", STRCOMTYPE(pReqComHead->type));
			return false;
		}

		// make peer terminate/departure msgid.
		msgid_t		peer_dept_msgid = pReqComHead->term_ids.msgid;
		msgid_t		peer_term_msgid = pImData->GetRandomChmpxMsgId();		// one of chmpx msgid(because start response msg)
		if(CHM_INVALID_MSGID == peer_term_msgid){
			ERR_CHMPRN("Could not get peer terminal msgid.");
			return false;
		}
		pComHead->version			= COM_VERSION_1;
		pComHead->type				= COM_C2C;
		pComHead->c2ctype			= c2ctype;
		pComHead->dept_ids.chmpxid	= pReqComHead->term_ids.chmpxid;
		pComHead->dept_ids.msgid	= pReqComHead->term_ids.msgid;
		pComHead->term_ids.chmpxid	= pReqComHead->dept_ids.chmpxid;
		pComHead->term_ids.msgid	= pReqComHead->dept_ids.msgid;
		pComHead->peer_dept_msgid	= peer_dept_msgid;
		pComHead->peer_term_msgid	= peer_term_msgid;
		pComHead->mq_serial_num		= GetSerialNumber();
		pComHead->hash				= pReqComHead->hash;
	}
	if(-1 == clock_gettime(CLOCK_REALTIME_COARSE, &(pComHead->reqtime))){
		WAN_CHMPRN("Could not get timespec(errno=%d), but continue...", errno);
		INIT_TIMESPEC(&(pComHead->reqtime));
	}

	return true;
}

//
// If ext_length is not PXCLTPKT_AUTO_LENGTH, allocate memory length as
// COMPKT + type + ext_length size.
//
PCOMPKT ChmEventMq::AllocatePxCltPacket(pxclttype_t type, ssize_t ext_length)
{
	ssize_t	length		= sizeof(COMPKT) + (PXCLTPKT_AUTO_LENGTH == ext_length ? 0L : ext_length);
	size_t	typelength	= SIZEOF_CHMPX_CLT(type);
	if(0 == typelength){
		WAN_CHMPRN("ComPacket type is %s(%" PRIu64 ") which is unknown. so does not convert contents.", STRPXCLTTYPE(type), type);
	}
	length += typelength;

	PCOMPKT	rtnptr;
	if(NULL == (rtnptr = reinterpret_cast<PCOMPKT>(malloc(length)))){
		ERR_CHMPRN("Could not allocate memory as %zd length for %s(%" PRIu64 ").", length, STRPXCLTTYPE(type), type);
	}
	return rtnptr;
}

//---------------------------------------------------------
// Methods for Communication
//---------------------------------------------------------
bool ChmEventMq::NotifyHup(int fd)
{
	MSG_CHMPRN("Event for MQ, not set EPOLLRDHUP type. So this function do nothing.");
	return true;
}

//
// This method is checking COM_C2C type, if is not same it, processing it.
// (for client on chmpx slave)
//
bool ChmEventMq::CheckProcessing(PCOMPKT pComPkt)
{
	if(!pComPkt){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}

	if(COM_C2C == pComPkt->head.type){
		// do not nothing here.
		return false;
	}

	if(!Processing(pComPkt)){
		ERR_CHMPRN("Failed processing COM_XXX.");
		// So doing processing, return true.
	}
	return true;
}

bool ChmEventMq::Processing(PCOMPKT pComPkt)
{
	if(!pComPkt){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	DUMPCOM_COMPKT("MQ::Processing", pComPkt);

	if(COM_C2C == pComPkt->head.type){
		// This case is received MQ data to client.
		// (The other case is error.)
		//
		if(pComPkt->head.term_ids.chmpxid != pImData->GetSelfChmpxId()){
			ERR_CHMPRN("Why does not same chmpxid? COMPKT is received from MQ, it should do processing socket event object.");
			return false;
		}

		// [NOTICE]
		// Come here, following pattern:
		//
		// 1) socket -> chmpx(slave)  -> MQ
		// 2) socket -> chmpx(server) -> MQ
		//
		// Case 1) Must be set end point of msgid
		// Case 2) Set or not set end point of msgid
		//         If not set that, decide that here.
		//

		// set deliver head in COMPKT
		if(CHM_INVALID_MSGID == pComPkt->head.term_ids.msgid){
			if(!is_server_mode){
				WAN_CHMPRN("On slave chmpx, do not send COM_C2C to any msgid, but continue...");
			}
			// Random
			pComPkt->head.term_ids.msgid = pImData->GetRandomClientMsgId();
		}
		if(CHM_INVALID_MSGID == pComPkt->head.term_ids.msgid){
			ERR_CHMPRN("Could not transfer pComPkt because msgid is INVALID.");
			return false;
		}
		pComPkt->head.peer_term_msgid = pComPkt->head.term_ids.msgid;

		// set departure msgid is self waiting msgid
		if(CHM_INVALID_MSGID != pComPkt->head.peer_dept_msgid){
			MSG_CHMPRN("Here is the message is after receiving from socket, but it has peer_dept_msgid. so reset it by myself.");
		}
		pComPkt->head.peer_dept_msgid = pImData->GetRandomChmpxMsgId();

		// set serial number
		pComPkt->head.mq_serial_num = GetSerialNumber();

		// make packet for MQ
		COMPKT	SendComPkt;
		COPY_COMPKT(&SendComPkt, pComPkt);
		// cppcheck-suppress unmatchedSuppression
		// cppcheck-suppress redundantAssignment
		SendComPkt.length = sizeof(COMPKT);
		// cppcheck-suppress unmatchedSuppression
		// cppcheck-suppress redundantAssignment
		SendComPkt.offset = static_cast<off_t>(sizeof(COMPKT));

		unsigned char*	pbody	= reinterpret_cast<unsigned char*>(pComPkt) + pComPkt->offset;
		size_t			blength	= pComPkt->length - sizeof(COMPKT);

		// for trace
		struct timespec	start_time;
		bool			is_trace = pImData->IsTraceEnable();
		if(is_trace){
			STR_MATE_TIMESPEC(&start_time);
		}

		// Send
		if(!Send(&SendComPkt, pbody, blength)){
			ERR_CHMPRN("Sending ComPkt type(%" PRIu64 ":%s) against ComPkt type(%" PRIu64 ":%s), Something error occured.", SendComPkt.head.type, STRCOMTYPE(SendComPkt.head.type), pComPkt->head.type, STRCOMTYPE(pComPkt->head.type));
			return false;
		}

		// set trace
		if(is_trace){
			struct timespec	fin_time;
			STR_MATE_TIMESPEC(&fin_time);
			if(!pImData->AddTrace(CHMLOG_TYPE_OUT_MQ, blength, start_time, fin_time)){
				ERR_CHMPRN("Failed to add trace log.");
			}
		}

	}else if(COM_PX2C == pComPkt->head.type || COM_C2PX == pComPkt->head.type){
		// This case is that data received from MQ, this data is notify to MQ client.
		//
		PPXCLT_ALL	pCltAll = CVT_CLT_ALL_PTR_PXCOMPKT(pComPkt);

		if(CHMPX_CLT_CLOSE_NOTIFY == pCltAll->val_head.type){
			// received close notify
			//
			if(!PxCltReceiveCloseNotify(&(pComPkt->head), pCltAll)){
				ERR_CHMPRN("Received PXCLT type(%" PRIu64 ":%s). Something error occured.", pCltAll->val_head.type, STRPXCLTTYPE(pCltAll->val_head.type));
				return false;
			}

		}else if(CHMPX_CLT_MERGE_GETLASTTS == pCltAll->val_head.type){
			// received get lastest update time
			//
			if(!PxCltReceiveMergeGetLastTime(&(pComPkt->head), pCltAll)){
				ERR_CHMPRN("Received PXCLT type(%" PRIu64 ":%s). Something error occured.", pCltAll->val_head.type, STRPXCLTTYPE(pCltAll->val_head.type));
				return false;
			}

		}else if(CHMPX_CLT_MERGE_RESLASTTS == pCltAll->val_head.type){
			// received response lastest update time
			//
			if(!PxCltReceiveMergeResponseLastTime(&(pComPkt->head), pCltAll)){
				ERR_CHMPRN("Received PXCLT type(%" PRIu64 ":%s). Something error occured.", pCltAll->val_head.type, STRPXCLTTYPE(pCltAll->val_head.type));
				return false;
			}

		}else if(CHMPX_CLT_REQ_UPDATEDATA == pCltAll->val_head.type){
			// received request update data lastest update time
			//
			if(!PxCltReceiveRequestUpdateData(&(pComPkt->head), pCltAll)){
				ERR_CHMPRN("Received PXCLT type(%" PRIu64 ":%s). Something error occured.", pCltAll->val_head.type, STRPXCLTTYPE(pCltAll->val_head.type));
				return false;
			}

		}else if(CHMPX_CLT_RES_UPDATEDATA == pCltAll->val_head.type){
			// received response update data lastest update time
			//
			if(!PxCltReceiveResponseUpdateData(&(pComPkt->head), pCltAll)){
				ERR_CHMPRN("Received PXCLT type(%" PRIu64 ":%s). Something error occured.", pCltAll->val_head.type, STRPXCLTTYPE(pCltAll->val_head.type));
				return false;
			}

		}else if(CHMPX_CLT_SET_UPDATEDATA == pCltAll->val_head.type){
			// received set update data lastest update time
			//
			if(!PxCltReceiveSetUpdateData(&(pComPkt->head), pCltAll)){
				ERR_CHMPRN("Received PXCLT type(%" PRIu64 ":%s). Something error occured.", pCltAll->val_head.type, STRPXCLTTYPE(pCltAll->val_head.type));
				return false;
			}

		}else if(CHMPX_CLT_RESULT_UPDATEDATA == pCltAll->val_head.type){
			// received result of update data
			//
			if(!PxCltReceiveResultUpdateData(&(pComPkt->head), pCltAll)){
				ERR_CHMPRN("Received PXCLT type(%" PRIu64 ":%s). Something error occured.", pCltAll->val_head.type, STRPXCLTTYPE(pCltAll->val_head.type));
				return false;
			}

		}else if(CHMPX_CLT_ABORT_UPDATEDATA == pCltAll->val_head.type){
			// received abort update data
			//
			if(!PxCltReceiveAbortUpdateData(&(pComPkt->head), pCltAll)){
				ERR_CHMPRN("Received PXCLT type(%" PRIu64 ":%s). Something error occured.", pCltAll->val_head.type, STRPXCLTTYPE(pCltAll->val_head.type));
				return false;
			}

		}else{
			ERR_CHMPRN("Could not handle PXCLT type(%" PRIu64 ":%s) in ComPkt type(%" PRIu64 ":%s).", pCltAll->val_head.type, STRPXCLTTYPE(pCltAll->val_head.type), pComPkt->head.type, STRCOMTYPE(pComPkt->head.type));
			return false;
		}

	}else{
		ERR_CHMPRN("Could not handle ComPkt type(%" PRIu64 ":%s).", pComPkt->head.type, STRCOMTYPE(pComPkt->head.type));
		return false;
	}
	return true;
}

bool ChmEventMq::Send(PCOMPKT pComPkt, const unsigned char* pbody, size_t blength)
{
	if(!pComPkt){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(COM_C2C == pComPkt->head.type){
		if(pComPkt->length != sizeof(COMPKT) || pComPkt->offset != sizeof(COMPKT)){
			ERR_CHMPRN("pComPkt(%p) is %s type, but its length or offset is wrong value.", pComPkt, STRCOMTYPE(pComPkt->head.type));
			return false;
		}
	}else if(COM_C2PX == pComPkt->head.type || COM_PX2C == pComPkt->head.type){
		if(pComPkt->length < sizeof(COMPKT) || pComPkt->offset != sizeof(COMPKT) || pbody || 0 < blength){
			ERR_CHMPRN("pComPkt(%p) is %s type, but its length or offset or body(not null) or body length(not 0) is wrong value.", pComPkt, STRCOMTYPE(pComPkt->head.type));
			return false;
		}
	}else{
		ERR_CHMPRN("pComPkt(%p) is unsupported type(%s)", pComPkt, STRCOMTYPE(pComPkt->head.type));
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();

	DUMPCOM_COMPKT("MQ::Send", pComPkt);
	DUMPCOM_PXCLT("MQ::Send", CVT_CLT_ALL_PTR_PXCOMPKT(pComPkt));

	// Make priority
	// cppcheck-suppress unmatchedSuppression
	// cppcheck-suppress duplicateValueTernary
	unsigned int	priority = ((COM_C2C == pComPkt->head.type) ? MQ_PRIORITY_MSG : MQ_PRIORITY_NOTICE);

	// Get k2hash & key names
	K2HShm*	pK2hash = pImData->GetK2hashObj();
	if(!pK2hash){
		ERR_CHMPRN("K2hash object is NULL.");
		return false;
	}
	string		headkey;
	string		bodykey;
	msgid_t		composed= ComposeSerialMsgid(pComPkt->head.peer_dept_msgid, pComPkt->head.mq_serial_num);
	if(!ChmEventMq::MakeK2hashKey(pComPkt->head.peer_dept_msgid, pComPkt->head.peer_term_msgid, pComPkt->head.mq_serial_num, headkey, bodykey)){
		ERR_CHMPRN("Failed to make k2hash key name for msgid(0x%016" PRIx64 "-0x%016" PRIx64 "-0x%016" PRIx64 ").", pComPkt->head.peer_dept_msgid, pComPkt->head.peer_term_msgid, pComPkt->head.mq_serial_num);
		return false;
	}

	// make head & body data in k2hash
	unsigned char*	phead	= reinterpret_cast<unsigned char*>(pComPkt);
	size_t			hlength	= pComPkt->length;
	if(	!pK2hash->Set(reinterpret_cast<const unsigned char*>(headkey.c_str()), headkey.length() + 1, phead, hlength, NULL, false) ||
		(pbody && 0 < blength && !pK2hash->Set(reinterpret_cast<const unsigned char*>(bodykey.c_str()), bodykey.length() + 1, pbody, blength, NULL, false)) )
	{
		ERR_CHMPRN("Failed to write head/body data to k2hash for msgid(0x%016" PRIx64 "-0x%016" PRIx64 "-0x%016" PRIx64 ").", pComPkt->head.peer_dept_msgid, pComPkt->head.peer_term_msgid, pComPkt->head.mq_serial_num);

		// remove keys
		pK2hash->Remove(reinterpret_cast<const unsigned char*>(headkey.c_str()), headkey.length() + 1);
		if(pbody && 0 < blength){
			pK2hash->Remove(reinterpret_cast<const unsigned char*>(bodykey.c_str()), bodykey.length() + 1);
		}
		return false;
	}

	// send signal to receiver mqueue
	mqd_t	term_mqfd;
	if(CHM_INVALID_HANDLE == (term_mqfd = OpenDestMQ(pComPkt->head.peer_term_msgid))){
		ERR_CHMPRN("Failed to open mqueue for msgid(0x%016" PRIx64 "), error=%d", pComPkt->head.peer_term_msgid, errno);

		// remove keys
		pK2hash->Remove(reinterpret_cast<const unsigned char*>(headkey.c_str()), headkey.length() + 1);
		if(pbody && 0 < blength){
			pK2hash->Remove(reinterpret_cast<const unsigned char*>(bodykey.c_str()), bodykey.length() + 1);
		}
		return false;
	}
	struct timespec	sleeptime;
	SET_TIMESPEC(&sleeptime, (static_cast<time_t>(timeout_us / (1000 * 1000))), ((timeout_us % (1000 * 1000)) * 1000));

	int	cnt;
	for(cnt = 0; cnt < retry_count; cnt++){
		if(-1 == mq_send(term_mqfd, reinterpret_cast<const char*>(&composed), sizeof(msgid_t), priority)){
			if(errno != EAGAIN){
				ERR_CHMPRN("Failed to send mqueue for msgid(0x%016" PRIx64 "), error=%d", pComPkt->head.peer_term_msgid, errno);

				// remove k2hash
				pK2hash->Remove(reinterpret_cast<const unsigned char*>(headkey.c_str()), headkey.length() + 1);
				if(pbody && 0 < blength){
					pK2hash->Remove(reinterpret_cast<const unsigned char*>(bodykey.c_str()), bodykey.length() + 1);
				}
				return false;
			}
			nanosleep(&sleeptime, NULL);
		}else{
			break;
		}
	}
	if(cnt >= retry_count){
		ERR_CHMPRN("Failed to send mqueue for msgid(0x%016" PRIx64 ") by timeouted(%ldus * %d)", pComPkt->head.peer_term_msgid, timeout_us, cnt);

		// remove k2hash
		pK2hash->Remove(reinterpret_cast<const unsigned char*>(headkey.c_str()), headkey.length() + 1);
		if(pbody && 0 < blength){
			pK2hash->Remove(reinterpret_cast<const unsigned char*>(bodykey.c_str()), bodykey.length() + 1);
		}
		return false;
	}

	// wait for ack, only client on slave(server)
	if(COM_C2C == pComPkt->head.type && use_mq_ack && !pChmCntrl->IsChmpxType()){
		if(!ReceiveAck(pComPkt->head.peer_dept_msgid)){
			ERR_CHMPRN("Could not receive ACK from msgid(0x%016" PRIx64 ") and mqfd(%d)", pComPkt->head.peer_term_msgid, term_mqfd);
			return false;
		}
	}
	return true;
}

bool ChmEventMq::SendAck(const COMPOSEDMSGID& composed, bool is_success)
{
	if(CHM_INVALID_MSGID == composed.msgid){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	mqd_t	mqfd;
	if(CHM_INVALID_HANDLE == (mqfd = OpenDestMQ(composed.msgid))){
		ERR_CHMPRN("Not found mqfd as msgid(0x%016" PRIx64 ") for sending ack.", composed.msgid);
		return false;
	}

	// make response ack data
	msgid_t			composed_msgid	= ComposeSerialMsgid(composed.msgid, composed.number, true, is_success);
	unsigned int	priority		= MQ_PRIORITY_MSG;		// make priority
	struct timespec	sleeptime;
	SET_TIMESPEC(&sleeptime, (static_cast<time_t>(timeout_us / (1000 * 1000))), ((timeout_us % (1000 * 1000)) * 1000));

	// send loop
	int	cnt;
	for(cnt = 0; cnt < retry_count; cnt++){
		if(-1 == mq_send(mqfd, reinterpret_cast<const char*>(&composed_msgid), sizeof(msgid_t), priority)){
			if(errno != EAGAIN){
				ERR_CHMPRN("Failed to response ack to composed msgid(0x%016" PRIx64 "), error=%d", composed_msgid, errno);
				return false;
			}
			nanosleep(&sleeptime, NULL);
		}else{
			return true;
		}
	}
	WAN_CHMPRN("Failed to send mqueue for response ack to composed msgid(0x%016" PRIx64 ") by timeouted(%ldus * %d)", composed_msgid, timeout_us, cnt);
	return false;
}

bool ChmEventMq::ReceiveAck(msgid_t msgid)
{
	if(CHM_INVALID_MSGID == msgid){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	mqd_t	mqfd = recv_idfd_map.get(msgid);
	if(CHM_INVALID_HANDLE == mqfd){
		ERR_CHMPRN("There is no mqfd in mapping for msgid(0x%016" PRIx64 ").", msgid);
		return false;
	}

	COMPOSEDMSGID	composed;
	bool			CanContinueWait;
	struct timespec	sleeptime;
	SET_TIMESPEC(&sleeptime, (static_cast<time_t>(timeout_us / (1000 * 1000))), ((timeout_us % (1000 * 1000)) * 1000));

	// receive ack loop
	for(int cnt = 0; cnt < retry_count; cnt++){
		CanContinueWait = false;
		if(!ReceiveComposedMsgid(mqfd, composed, &CanContinueWait)){
			if(!CanContinueWait){
				WAN_CHMPRN("Failed to receive ACK from MQ(%d).", mqfd);
				return false;
			}
			nanosleep(&sleeptime, NULL);
			continue;
		}
		if(!composed.is_ack){
			WAN_CHMPRN("Received another request during the ACK waiting from MQ(%d), so continue to wait after processing this interrupt request.", mqfd);
			if(false == RawReceive(mqfd, composed)){
				ERR_CHMPRN("Failed receiving and to processing for MQ(%d) directly.", mqfd);
			}
			// continue to wait
			if(0 < cnt){
				--cnt;
			}
		}else{
			return composed.is_ack_success;
		}
	}
	WAN_CHMPRN("Timeouted for receiving ACK from MQ(%d).", mqfd);
	return false;
}

//
// After receiving, this method is processing own work. And if it needs to dispatch
// processing event after own processing, do it by calling control object.
//
// [NOTE]
// Receive method read full information(COMPKT + PXCLT_COM) from MQ,
// If you need to read head & body by each, you should call ReceiveComposedMsgid() + ReceiveHead() and ReceiveBody() methods.
//
bool ChmEventMq::Receive(int fd)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}

	// Processing...
	bool	result = false;
	if(procthreads.HasThread()){
		// Has processing threads, so run(do work) processing thread.
		chmthparam_t	wp_param = static_cast<chmthparam_t>(fd);

		if(false == (result = procthreads.DoWorkThread(wp_param))){
			ERR_CHMPRN("Failed to wake up thread for processing(receiving) for MQ(%d).", fd);
		}
	}else{
		// Does not have processing thread, so call directly.

		// get composed msgid
		COMPOSEDMSGID	composed;
		if(!ReceiveComposedMsgid(fd, composed)){
			MSG_CHMPRN("Failed to read composed msgid from MQ(%d), probably nothing to receive data.", fd);
			return false;
		}
		if(false == (result = RawReceive(fd, composed))){
			ERR_CHMPRN("Failed receiving and to processing for MQ(%d) directly.", fd);
		}
	}
	return result;
}

bool ChmEventMq::RawReceive(mqd_t mqfd, const COMPOSEDMSGID& composed)
{
	if(CHM_INVALID_HANDLE == mqfd || CHM_INVALID_MSGID == composed.msgid){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}

	// read head
	PCOMPKT	pComPkt = NULL;
	if(!ReceiveHead(mqfd, composed, &pComPkt) || !pComPkt){
		MSG_CHMPRN("Failed to read msg head data from msgid(0x%016" PRIx64 ") received MQ(%d), probably nothing to receive data.", composed.msgid, mqfd);
		return false;
	}
	// read body
	PCOMPKT	pTmp;
	if(NULL == (pTmp = ReceiveBody(pComPkt, true))){
		ERR_CHMPRN("Failed to read msg body data from k2hash for MQ(%d).", mqfd);
		CHM_Free(pComPkt);
		return false;
	}
	pComPkt = pTmp;

	// Dispatching
	bool	is_need_ack	= (COM_C2C == pComPkt->head.type && use_mq_ack && pChmCntrl->IsChmpxType());

	if(pChmCntrl && !pChmCntrl->Processing(pComPkt, ChmCntrl::EVOBJ_TYPE_EVMQ)){
		// send failure ack
		if(is_need_ack && !SendAck(composed, false)){
			ERR_CHMPRN("Failed sending failure ACK to to MQ(%d).", mqfd);
		}
		ERR_CHMPRN("Failed processing after receiving from MQ(%d).", mqfd);
		CHM_Free(pComPkt);
		return false;
	}
	CHM_Free(pComPkt);

	// send success ack
	if(is_need_ack && !SendAck(composed, true)){
		ERR_CHMPRN("Failed sending success ACK to to MQ(%d).", mqfd);
		return false;
	}
	return true;
}

bool ChmEventMq::ReceiveHeadByMsgId(msgid_t msgid, PCOMPKT* ppComPkt, bool* pCanContinueWait)
{
	mqd_t	mqfd = recv_idfd_map.get(msgid);
	if(CHM_INVALID_HANDLE == mqfd){
		ERR_CHMPRN("msgid(0x%016" PRIx64 ") is not this client msgid on slave.", msgid);
		return false;
	}
	// get composed msgid
	COMPOSEDMSGID	composed;
	if(!ReceiveComposedMsgid(mqfd, composed, pCanContinueWait)){
		MSG_CHMPRN("Failed to read composed msgid from MQ(%d), probably nothing to receive data.", mqfd);
		return false;
	}

	// read head
	return ReceiveHead(mqfd, composed, ppComPkt);
}

bool ChmEventMq::ReceiveComposedMsgid(int fd, COMPOSEDMSGID& composed, bool* pCanContinueWait)
{
	if(pCanContinueWait){
		*pCanContinueWait = false;
	}
	if(!IsEventQueueFd(fd)){
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}

	// receive from mq
	while(true){
		unsigned int	priority= 0;
		msgid_t			msgid	= CHM_INVALID_MSGID;
		ssize_t			length	= mq_receive(fd, reinterpret_cast<char*>(&msgid), sizeof(msgid_t), &priority);		// MQ is set O_NON_BLOCK
		if(-1 == length){
			if(EINTR == errno){
				MSG_CHMPRN("Break receive mqfd(%d) by signal, so retry.", fd);
			}else if(EAGAIN == errno || ETIMEDOUT == errno){
				//MSG_CHMPRN("Timeouted to receive mqfd(%d), errno=%d", fd, errno);
				if(pCanContinueWait){
					*pCanContinueWait = true;
				}
				break;
			}else{
				ERR_CHMPRN("Failed to receive mqfd(%d), errno=%d", fd, errno);
				break;
			}

		}else if(sizeof(msgid_t) != static_cast<size_t>(length)){
			ERR_CHMPRN("Received unknown data(length=%zd) from mqfd(%d)", length, fd);
			break;
		}else{
			// success
			if(!DecomposeSerialMsgid(msgid, composed)){
				ERR_CHMPRN("Received composed msgid(0x%016" PRIx64 ") from mqfd(%d) is invalid.", msgid, fd);
				break;
			}
			return true;
		}
	}
	return false;
}

bool ChmEventMq::ReceiveHead(int fd, const COMPOSEDMSGID& composed, PCOMPKT* ppComPkt)
{
	if(!ppComPkt || CHM_INVALID_MSGID == composed.msgid){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData		= pChmCntrl->GetImDataObj();
	*ppComPkt				= NULL;

	// check
	msgid_t		pdept_msgid	= composed.msgid;
	serial_t	serial		= composed.number;
	if(CHM_INVALID_MSGID == pdept_msgid){
		ERR_CHMPRN("received msgid(0x%016" PRIx64 ") is invalid.", pdept_msgid);
		return false;
	}

	msgid_t	pterm_msgid	= GetAssignedMsgId(fd);		// get self msgid from fd
	if(CHM_INVALID_MSGID == pterm_msgid){
		ERR_CHMPRN("specified fd(%d) is not self msgid(0x%016" PRIx64 ").", fd, pterm_msgid);
		return false;
	}

	// Build k2hash key names
	K2HShm*	pK2hash = pImData->GetK2hashObj();
	if(!pK2hash){
		ERR_CHMPRN("K2hash object is NULL.");
		return false;
	}
	string	headkey;
	string	bodykey;		// not used
	if(!ChmEventMq::MakeK2hashKey(pdept_msgid, pterm_msgid, serial, headkey, bodykey)){
		ERR_CHMPRN("Failed to make k2hash key name for msgid(0x%016" PRIx64 "-0x%016" PRIx64 "-0x%016" PRIx64 ").", pdept_msgid, pterm_msgid, serial);
		return false;
	}

	// Read msg head from k2hash
	ssize_t			hlength;
	unsigned char*	phead = NULL;
	if(-1 == (hlength = pK2hash->Get(reinterpret_cast<const unsigned char*>(headkey.c_str()), headkey.length() + 1, &phead)) || !phead){
		ERR_CHMPRN("Failed to read msg head(%s) data.", headkey.c_str());
		return false;
	}
	*ppComPkt = reinterpret_cast<PCOMPKT>(phead);

	// always remove head key
	if(!pK2hash->Remove(reinterpret_cast<const unsigned char*>(headkey.c_str()), headkey.length() + 1)){
		ERR_CHMPRN("Failed to remove msg head(%s) data in k2hash, but continue...", headkey.c_str());
	}

	if(COM_C2C == (*ppComPkt)->head.type){
		if((*ppComPkt)->length != sizeof(COMPKT) || (*ppComPkt)->offset != sizeof(COMPKT)){
			ERR_CHMPRN("read msg head(%s) length(%zd) should be %zu.", headkey.c_str(), hlength, sizeof(COMPKT));
			CHM_Free(phead);
			*ppComPkt = NULL;
			return false;
		}
	}else if(COM_C2PX == (*ppComPkt)->head.type || COM_PX2C == (*ppComPkt)->head.type){
		if((*ppComPkt)->length < sizeof(COMPKT) || (*ppComPkt)->offset != sizeof(COMPKT)){
			ERR_CHMPRN("*ppComPkt(%p) is %s type, but its length or offset is wrong value.", *ppComPkt, STRCOMTYPE((*ppComPkt)->head.type));
			CHM_Free(phead);
			*ppComPkt = NULL;
			return false;
		}
	}else{
		ERR_CHMPRN("*ppComPkt(%p) is unsupported type(%s)", *ppComPkt, STRCOMTYPE((*ppComPkt)->head.type));
		CHM_Free(phead);
		*ppComPkt = NULL;
		return false;
	}
	DUMPCOM_COMPKT("MQ::ReceiveHead", *ppComPkt);
	DUMPCOM_PXCLT("MQ::ReceiveHead", CVT_CLT_ALL_PTR_PXCOMPKT(*ppComPkt));

	return true;
}

//
// This method is only reading body and return it.
//
bool ChmEventMq::ReceiveBody(PCOMPKT pComPkt, unsigned char** ppbody, size_t& blength, bool is_remove_body)
{
	if(!pComPkt || !ppbody){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	if(COM_C2C != pComPkt->head.type){
		//MSG_CHMPRN("pComPkt(%p) is %s type, this type does not have body.", pComPkt, STRCOMTYPE(pComPkt->head.type));
		*ppbody	= NULL;
		blength	= 0L;
		return true;
	}
	ChmIMData*	pImData = pChmCntrl->GetImDataObj();
	*ppbody	= NULL;
	blength	= 0L;

	// for trace
	struct timespec	start_time;
	bool			is_trace = (pChmCntrl->IsChmpxType() && pImData->IsTraceEnable());
	if(is_trace){
		STR_MATE_TIMESPEC(&start_time);
	}

	// Build k2hash key names
	K2HShm*	pK2hash = pImData->GetK2hashObj();
	if(!pK2hash){
		ERR_CHMPRN("K2hash object is NULL.");
		return false;
	}
	string	headkey;
	string	bodykey;
	if(!ChmEventMq::MakeK2hashKey(pComPkt->head.peer_dept_msgid, pComPkt->head.peer_term_msgid, pComPkt->head.mq_serial_num, headkey, bodykey)){
		ERR_CHMPRN("Failed to make k2hash key name for msgid(0x%016" PRIx64 "-0x%016" PRIx64 "-0x%016" PRIx64 ").", pComPkt->head.peer_dept_msgid, pComPkt->head.peer_term_msgid, pComPkt->head.mq_serial_num);
		return false;
	}

	// Read msg body from k2hash
	ssize_t	bodylength;
	if(-1 == (bodylength = pK2hash->Get(reinterpret_cast<const unsigned char*>(bodykey.c_str()), bodykey.length() + 1, ppbody)) || !(*ppbody)){
		ERR_CHMPRN("Failed to read msg body(%s) data.", bodykey.c_str());
		return false;
	}
	blength = static_cast<size_t>(bodylength);

	// set trace
	if(is_trace){
		struct timespec	fin_time;
		STR_MATE_TIMESPEC(&fin_time);
		if(!pImData->AddTrace(CHMLOG_TYPE_IN_MQ, blength, start_time, fin_time)){
			ERR_CHMPRN("Failed to add trace log.");
		}
	}

	// Remove k2hash
	if(is_remove_body){
		if(!RemoveReceivedBody(pComPkt)){
			WAN_CHMPRN("Failed to ACKED for head & body msgid(0x%016" PRIx64 "-0x%016" PRIx64 "-0x%016" PRIx64 "), but continue...", pComPkt->head.peer_dept_msgid, pComPkt->head.peer_term_msgid, pComPkt->head.mq_serial_num);
		}
	}
	return true;
}

//
// Read message body by specified PCOMPKT head, and makes, returns full PCOMPKT.
//
// [NOTICE]
// Returned PCOMPKT is needed to free, but must not free pComPkt.
// So pComPkt is reallocate in this method or return it.
//
PCOMPKT ChmEventMq::ReceiveBody(PCOMPKT pComPkt, bool is_remove_body)
{
	if(!pComPkt){
		ERR_CHMPRN("Parameter is wrong.");
		return NULL;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return NULL;
	}

	// read body
	unsigned char*	pbody	= NULL;
	size_t			blength = 0L;
	if(!ReceiveBody(pComPkt, &pbody, blength, is_remove_body)){
		// failed, but continue...
		WAN_CHMPRN("Failed to read msg body, so set null data.");
		// set length & offset
		pComPkt->length = sizeof(COMPKT);
		pComPkt->offset = static_cast<off_t>(sizeof(COMPKT));
	}else{
		if(pbody && 0 < blength){
			// reallocate
			PCOMPKT	pTmp;
			if(NULL == (pTmp = reinterpret_cast<PCOMPKT>(realloc(pComPkt, sizeof(COMPKT) + blength)))){
				WAN_CHMPRN("Could not allocate memory.");
				return NULL;
			}
			// set length & offset
			pTmp->length = sizeof(COMPKT) + blength;
			pTmp->offset = static_cast<off_t>(sizeof(COMPKT));

			// copy data
			memcpy(reinterpret_cast<unsigned char*>(pTmp) + pTmp->offset, pbody, blength);
			K2H_Free(pbody);

			pComPkt = pTmp;
		}else{
			// there is no body, probably type is COM_PX2C or COM_C2PX
		}
	}
	return pComPkt;
}

//
// Only remove body key in k2hash.
// The head key is removed after reading it automatically, then this method does not care for the head key.
//
bool ChmEventMq::RemoveReceivedBody(PCOMPKT pComPkt)
{
	if(!pComPkt){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(COM_C2C != pComPkt->head.type){
		//MSG_CHMPRN("ComPkt does not COM_C2C type, that does not have body key in k2hash, so return true.");
		return true;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	ChmIMData*	pImData	= pChmCntrl->GetImDataObj();
	bool		result	= true;

	// Remove head & body data in k2hash
	K2HShm*	pK2hash = pImData->GetK2hashObj();
	if(!pK2hash){
		ERR_CHMPRN("K2hash object is NULL.");
		result = false;
	}else{
		string	headkey;
		string	bodykey;
		if(!ChmEventMq::MakeK2hashKey(pComPkt->head.peer_dept_msgid, pComPkt->head.peer_term_msgid, pComPkt->head.mq_serial_num, headkey, bodykey)){
			ERR_CHMPRN("Failed to make k2hash key name for msgid(0x%016" PRIx64 "-0x%016" PRIx64 "-0x%016" PRIx64 ").", pComPkt->head.peer_dept_msgid, pComPkt->head.peer_term_msgid, pComPkt->head.mq_serial_num);
			result = false;
		}else{
			// remove body
			if(!pK2hash->Remove(reinterpret_cast<const unsigned char*>(bodykey.c_str()), bodykey.length() + 1)){
				ERR_CHMPRN("Failed to remove msg body(%s) data in k2hash.", bodykey.c_str());
				result = false;
			}
		}
	}
	return result;
}

//---------------------------------------------------------
// Methods for Sending/Receiving command on Merge logic
//---------------------------------------------------------
// Get lastest update time before merging
//
// chmpx(server node) ---- [ PXCLT_MERGE_GETLASTTS ] ---> client chmpx library ---> call chm_merge_get_cb function
// chmpx(server node) <--- [ PXCLT_MERGE_RESLASTTS ] ---- client chmpx library <--- call chm_merge_get_cb function
//
bool ChmEventMq::PxCltSendMergeGetLastTime(struct timespec& lastts)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	if(!pChmCntrl->IsChmpxType()){
		ERR_CHMPRN("PXCLT_MERGE_GETLASTTS command MUST be sent from chmpx server node.");
		return false;
	}
	if(!notify_merge_get){
		ERR_CHMPRN("Already run getting last update time logic.");
		return false;
	}
	ChmIMData*	pImData	= pChmCntrl->GetImDataObj();

	// Allocation
	PCOMPKT	pComPkt;
	if(NULL == (pComPkt = ChmEventMq::AllocatePxCltPacket(CHMPX_CLT_MERGE_GETLASTTS))){
		ERR_CHMPRN("Could not allocate memory for COMPKT.");
		return false;
	}

	// msgid
	msgid_t	dept_msgid = pImData->GetRandomChmpxMsgId();		// Get msgid in chmpx process
	if(CHM_INVALID_MSGID == dept_msgid){
		ERR_CHMPRN("Could not get own dept msgid(chmpx assigned).");
		CHM_Free(pComPkt);
		return false;
	}
	msgid_t	term_msgid = pImData->GetRandomClientMsgId();		// Get client assigned msgid
	if(CHM_INVALID_MSGID == term_msgid){
		ERR_CHMPRN("Could not get own terminal msgid(client assigned).");
		CHM_Free(pComPkt);
		return false;
	}

	// set common datas
	PPXCLT_ALL				pCltAll		= CVT_CLT_ALL_PTR_PXCOMPKT(pComPkt);
	PPXCLT_MERGE_GETLASTTS	pMergeGetLTS= CVT_CLTPTR_MERGE_GETLASTTS(pCltAll);
	SET_PXCLTPKT(pComPkt, true, CHMPX_CLT_MERGE_GETLASTTS, dept_msgid, term_msgid, GetSerialNumber(), 0);

	pMergeGetLTS->head.type		= CHMPX_CLT_MERGE_GETLASTTS;
	pMergeGetLTS->head.length	= SIZEOF_CHMPX_CLT(CHMPX_CLT_MERGE_GETLASTTS);

	ChmLock	AutoLock(CHMLT_MQOBJ, CHMLT_READ);				// Lock

	// send
	//
	// [NOTICE]
	// Send to one of client's msgid
	//
	if(!Send(pComPkt, NULL, 0)){
		ERR_CHMPRN("Could not send CHMPX_CLT_MERGE_GETLASTTS to msgid(0x%016" PRIx64 ").", term_msgid);
		CHM_Free(pComPkt);
		return false;
	}
	CHM_Free(pComPkt);
	AutoLock.UnLock();										// Unlock

	// [LOOP]
	//
	// wait for receiving result
	//
	notify_merge_get			= false;									// force
	pres_lasttime				= &lastts;
	struct timespec	sleeptime	= {0L, ChmEventMq::DEFAULT_GETLTS_SLEEP};	// 1us
	for(long lcnt = 0; !notify_merge_get && lcnt < ChmEventMq::DEFAULT_GETLTS_LOOP; ++lcnt){
		nanosleep(&sleeptime, NULL);
	}
	if(!notify_merge_get){
		ERR_CHMPRN("No response from client, so getting lastest update time is TIMEOUT(1s).");
		notify_merge_get= true;		// reset
		pres_lasttime	= NULL;
		return false;
	}
	notify_merge_get= true;			// reset
	pres_lasttime	= NULL;

	return true;
}

bool ChmEventMq::PxCltReceiveMergeGetLastTime(PCOMHEAD pComHead, PPXCLT_ALL pCltAll)
{
	if(!pComHead || !pCltAll){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(COM_PX2C != pComHead->type){
		ERR_CHMPRN("COMHEAD type(%s) is wrong.", STRCOMTYPE(pComHead->type));
		return false;
	}
	if(CHMPX_CLT_MERGE_GETLASTTS != pCltAll->val_head.type){
		ERR_CHMPRN("PXCLT_HEAD type(%s) is wrong.", STRPXCLTTYPE(pCltAll->val_head.type));
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}

	// get lastest update time
	struct timespec	updatets = {0, 0};
	if(mlastfunc){
		chmpx_h	handle = reinterpret_cast<chmpx_h>(pChmCntrl);
		if(!mlastfunc(handle, &updatets)){
			ERR_CHMPRN("Failed to get lastest update time, but continue with zero value for merging.");
			INIT_TIMESPEC(&updatets);
		}
	}

	// response the result
	return PxCltSendMergeResponseLastTime(updatets);
}

bool ChmEventMq::PxCltSendMergeResponseLastTime(const struct timespec& lastts)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	if(pChmCntrl->IsChmpxType()){
		ERR_CHMPRN("PXCLT_MERGE_RESLASTTS command MUST be sent from client on chmpx server node.");
		return false;
	}
	ChmIMData*	pImData	= pChmCntrl->GetImDataObj();

	// Allocation
	PCOMPKT	pComPkt;
	if(NULL == (pComPkt = ChmEventMq::AllocatePxCltPacket(CHMPX_CLT_MERGE_RESLASTTS))){
		ERR_CHMPRN("Could not allocate memory for COMPKT.");
		return false;
	}

	// msgid
	msgid_t	dept_msgid = pImData->GetRandomClientMsgId();		// Get client assigned msgid
	if(CHM_INVALID_MSGID == dept_msgid){
		ERR_CHMPRN("Could not get own dept msgid(chmpx assigned).");
		CHM_Free(pComPkt);
		return false;
	}
	msgid_t	term_msgid = pImData->GetRandomChmpxMsgId();		// Get msgid in chmpx process
	if(CHM_INVALID_MSGID == term_msgid){
		ERR_CHMPRN("Could not get own terminal msgid(client assigned).");
		CHM_Free(pComPkt);
		return false;
	}

	// set common datas
	PPXCLT_ALL				pCltAll		= CVT_CLT_ALL_PTR_PXCOMPKT(pComPkt);
	PPXCLT_MERGE_RESLASTTS	pMergeResLTS= CVT_CLTPTR_MERGE_RESLASTTS(pCltAll);
	SET_PXCLTPKT(pComPkt, false, CHMPX_CLT_MERGE_RESLASTTS, dept_msgid, term_msgid, GetSerialNumber(), 0);

	pMergeResLTS->head.type				= CHMPX_CLT_MERGE_RESLASTTS;
	pMergeResLTS->head.length			= SIZEOF_CHMPX_CLT(CHMPX_CLT_MERGE_RESLASTTS);
	pMergeResLTS->lastupdatets.tv_nsec	= lastts.tv_nsec;
	pMergeResLTS->lastupdatets.tv_sec	= lastts.tv_sec;

	ChmLock	AutoLock(CHMLT_MQOBJ, CHMLT_READ);				// Lock

	// send
	//
	// [NOTICE]
	// Send to one of client's msgid
	//
	if(!Send(pComPkt, NULL, 0)){
		ERR_CHMPRN("Could not send CHMPX_CLT_MERGE_RESLASTTS to msgid(0x%016" PRIx64 ").", term_msgid);
		CHM_Free(pComPkt);
		return false;
	}
	CHM_Free(pComPkt);

	return true;
}

bool ChmEventMq::PxCltReceiveMergeResponseLastTime(PCOMHEAD pComHead, PPXCLT_ALL pCltAll)
{
	if(!pComHead || !pCltAll){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(COM_C2PX != pComHead->type){
		ERR_CHMPRN("COMHEAD type(%s) is wrong.", STRCOMTYPE(pComHead->type));
		return false;
	}
	if(CHMPX_CLT_MERGE_RESLASTTS != pCltAll->val_head.type){
		ERR_CHMPRN("PXCLT_HEAD type(%s) is wrong.", STRPXCLTTYPE(pCltAll->val_head.type));
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}

	// get data
	PPXCLT_MERGE_RESLASTTS	pMergeResLTS = CVT_CLTPTR_MERGE_RESLASTTS(pCltAll);

	// set data
	//
	// [NOTICE]
	// we do not lock this value, because already blocking another threads.
	//
	if(pres_lasttime){
		pres_lasttime->tv_nsec	= pMergeResLTS->lastupdatets.tv_nsec;
		pres_lasttime->tv_sec	= pMergeResLTS->lastupdatets.tv_sec;
	}
	notify_merge_get = true;

	return true;
}

bool ChmEventMq::PxCltSendRequestUpdateData(const PPXCOMMON_MERGE_PARAM pmerge_param)
{
	if(!pmerge_param){
		ERR_CHMPRN("Parameter is wrong.");
		return false;

	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	if(!pChmCntrl->IsChmpxType()){
		ERR_CHMPRN("PXCLT_REQ_UPDATEDATA command MUST be sent from chmpx server node.");
		return false;
	}
	ChmIMData*	pImData	= pChmCntrl->GetImDataObj();

	// Allocation
	PCOMPKT	pComPkt;
	if(NULL == (pComPkt = ChmEventMq::AllocatePxCltPacket(CHMPX_CLT_REQ_UPDATEDATA))){
		ERR_CHMPRN("Could not allocate memory for COMPKT.");
		return false;
	}

	// msgid
	msgid_t	dept_msgid = pImData->GetRandomChmpxMsgId();		// Get msgid in chmpx process
	if(CHM_INVALID_MSGID == dept_msgid){
		ERR_CHMPRN("Could not get own dept msgid(chmpx assigned).");
		CHM_Free(pComPkt);
		return false;
	}
	msgid_t	term_msgid = pImData->GetRandomClientMsgId();		// Get client assigned msgid
	if(CHM_INVALID_MSGID == term_msgid){
		ERR_CHMPRN("Could not get own terminal msgid(client assigned).");
		CHM_Free(pComPkt);
		return false;
	}

	// set common datas
	PPXCLT_ALL				pCltAll			= CVT_CLT_ALL_PTR_PXCOMPKT(pComPkt);
	PPXCLT_REQ_UPDATEDATA	pReqUpdateData	= CVT_CLTPTR_REQ_UPDATEDATA(pCltAll);
	SET_PXCLTPKT(pComPkt, true, CHMPX_CLT_REQ_UPDATEDATA, dept_msgid, term_msgid, GetSerialNumber(), 0);

	pReqUpdateData->head.type	= CHMPX_CLT_REQ_UPDATEDATA;
	pReqUpdateData->head.length	= SIZEOF_CHMPX_CLT(CHMPX_CLT_REQ_UPDATEDATA);
	COPY_COMMON_MERGE_PARAM(&(pReqUpdateData->merge_param), pmerge_param);

	ChmLock	AutoLock(CHMLT_MQOBJ, CHMLT_READ);				// Lock

	// send
	//
	// [NOTICE]
	// Send to one of client's msgid
	//
	if(!Send(pComPkt, NULL, 0)){
		ERR_CHMPRN("Could not send CHMPX_CLT_REQ_UPDATEDATA to msgid(0x%016" PRIx64 ").", term_msgid);
		CHM_Free(pComPkt);
		return false;
	}
	CHM_Free(pComPkt);

	return true;
}

bool ChmEventMq::PxCltReceiveRequestUpdateData(PCOMHEAD pComHead, PPXCLT_ALL pCltAll)
{
	if(!pComHead || !pCltAll){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(COM_PX2C != pComHead->type){
		ERR_CHMPRN("COMHEAD type(%s) is wrong.", STRCOMTYPE(pComHead->type));
		return false;
	}
	if(CHMPX_CLT_REQ_UPDATEDATA != pCltAll->val_head.type){
		ERR_CHMPRN("PXCLT_HEAD type(%s) is wrong.", STRPXCLTTYPE(pCltAll->val_head.type));
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}

	// request data
	PPXCLT_REQ_UPDATEDATA	pReqUpdateData	= CVT_CLTPTR_REQ_UPDATEDATA(pCltAll);

	if(!mgetfunc){
		// not register merge get function.
		MSG_CHMPRN("mgetfunc is not set, so nothing to do for merge.");

		// send finish to update data
		if(!PxCltSendResultUpdateData(pReqUpdateData->merge_param.chmpxid, true)){		// result = true
			ERR_CHMPRN("Failed to send CHMPX_CLT_RESULT_UPDATEDATA, but continue...");
		}
		return true;
	}

	// set merge parameter
	PCHM_UPDATA_PARAM		pUpdateDataParam= new CHM_UPDATA_PARAM;
	pUpdateDataParam->chmpxid				= pReqUpdateData->merge_param.chmpxid;
	pUpdateDataParam->startts.tv_sec		= pReqUpdateData->merge_param.startts.tv_sec;
	pUpdateDataParam->startts.tv_nsec		= pReqUpdateData->merge_param.startts.tv_nsec;
	pUpdateDataParam->max_pending_hash		= pReqUpdateData->merge_param.max_pending_hash;
	pUpdateDataParam->pending_hash			= pReqUpdateData->merge_param.pending_hash;
	pUpdateDataParam->max_base_hash			= pReqUpdateData->merge_param.max_base_hash;
	pUpdateDataParam->base_hash				= pReqUpdateData->merge_param.base_hash;
	pUpdateDataParam->replica_count			= pReqUpdateData->merge_param.replica_count;
	pUpdateDataParam->is_expire_check		= pReqUpdateData->merge_param.is_expire_check;

	if(!RT_TIMESPEC(&(pUpdateDataParam->endts))){
		pUpdateDataParam->endts.tv_sec		= time(NULL);
		pUpdateDataParam->endts.tv_nsec		= 0;
	}

	// stack merge parameter
	while(!fullock::flck_trylock_noshared_mutex(&mparam_list_lockval));	// LOCK
	merge_param_list.push_back(pUpdateDataParam);
	if(!notify_merge_update){
		notify_merge_update = true;
	}
	fullock::flck_unlock_noshared_mutex(&mparam_list_lockval);			// UNLOCK

	// run thread
	if(!mergethread.IsThreadRun()){
		// run merge thread
		//
		//	- parameter is this object
		//	- not sleep at starting
		//	- at once(one shot)                	(*1)
		//	- sleep after every working			    do not care this value because (*1)
		//	- not keep event count					do not care this value because (*1)
		//
		if(!mergethread.CreateThreads(1, ChmEventMq::MergeWorkerFunc, NULL, this, 0, false, true, false, false)){
			ERR_CHMPRN("Could not start merge thread in MQ.");
			CHM_Delete(pUpdateDataParam);
			return false;
		}
	}else{
		// already run thread, so nothing to do.
	}
	return true;
}

bool ChmEventMq::PxCltSendResponseUpdateData(chmpxid_t requester_chmpxid, const PCHMBIN pbindata, const struct timespec* pts)
{
	if(CHM_INVALID_CHMPXID == requester_chmpxid || !pbindata || !pbindata->byptr || 0 == pbindata->length || !pts){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	if(pChmCntrl->IsChmpxType()){
		ERR_CHMPRN("PXCLT_RES_UPDATEDATA command MUST be sent from client on chmpx server node.");
		return false;
	}
	ChmIMData*	pImData	= pChmCntrl->GetImDataObj();

	// Allocation
	PCOMPKT	pComPkt;
	if(NULL == (pComPkt = ChmEventMq::AllocatePxCltPacket(CHMPX_CLT_RES_UPDATEDATA, pbindata->length))){
		ERR_CHMPRN("Could not allocate memory for COMPKT.");
		return false;
	}

	// msgid
	msgid_t	dept_msgid = pImData->GetRandomClientMsgId();		// Get client assigned msgid
	if(CHM_INVALID_MSGID == dept_msgid){
		ERR_CHMPRN("Could not get own dept msgid(chmpx assigned).");
		CHM_Free(pComPkt);
		return false;
	}
	msgid_t	term_msgid = pImData->GetRandomChmpxMsgId();		// Get msgid in chmpx process
	if(CHM_INVALID_MSGID == term_msgid){
		ERR_CHMPRN("Could not get own terminal msgid(client assigned).");
		CHM_Free(pComPkt);
		return false;
	}

	// set common datas
	PPXCLT_ALL				pCltAll			= CVT_CLT_ALL_PTR_PXCOMPKT(pComPkt);
	PPXCLT_RES_UPDATEDATA	pResUpdateData	= CVT_CLTPTR_RES_UPDATEDATA(pCltAll);
	SET_PXCLTPKT(pComPkt, false, CHMPX_CLT_RES_UPDATEDATA, dept_msgid, term_msgid, GetSerialNumber(), pbindata->length);

	pResUpdateData->head.type	= CHMPX_CLT_RES_UPDATEDATA;
	pResUpdateData->head.length	= SIZEOF_CHMPX_CLT(CHMPX_CLT_RES_UPDATEDATA) + pbindata->length;
	pResUpdateData->chmpxid		= requester_chmpxid;
	pResUpdateData->ts.tv_sec	= pts->tv_sec;
	pResUpdateData->ts.tv_nsec	= pts->tv_nsec;
	pResUpdateData->length		= pbindata->length;
	pResUpdateData->byptr		= reinterpret_cast<unsigned char*>(SIZEOF_CHMPX_CLT(CHMPX_CLT_RES_UPDATEDATA));

	unsigned char*	psetbase	= CHM_OFFSET(pResUpdateData, SIZEOF_CHMPX_CLT(CHMPX_CLT_RES_UPDATEDATA), unsigned char*);
	memcpy(psetbase, pbindata->byptr, pbindata->length);

	ChmLock	AutoLock(CHMLT_MQOBJ, CHMLT_READ);				// Lock

	// send
	//
	// [NOTICE]
	// Send to one of client's msgid
	//
	if(!Send(pComPkt, NULL, 0)){
		ERR_CHMPRN("Could not send CHMPX_CLT_RES_UPDATEDATA to msgid(0x%016" PRIx64 ").", term_msgid);
		CHM_Free(pComPkt);
		return false;
	}
	CHM_Free(pComPkt);

	return true;
}

bool ChmEventMq::PxCltReceiveResponseUpdateData(PCOMHEAD pComHead, PPXCLT_ALL pCltAll)
{
	if(!pComHead || !pCltAll){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(COM_C2PX != pComHead->type){
		ERR_CHMPRN("COMHEAD type(%s) is wrong.", STRCOMTYPE(pComHead->type));
		return false;
	}
	if(CHMPX_CLT_RES_UPDATEDATA != pCltAll->val_head.type){
		ERR_CHMPRN("PXCLT_HEAD type(%s) is wrong.", STRPXCLTTYPE(pCltAll->val_head.type));
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}

	// check data
	PPXCLT_RES_UPDATEDATA	pResUpdateData	= CVT_CLTPTR_RES_UPDATEDATA(pCltAll);
	if(CHM_INVALID_CHMPXID == pResUpdateData->chmpxid || 0 == pResUpdateData->length || NULL == pResUpdateData->byptr){
		ERR_CHMPRN("Received CHMPX_CLT_RES_UPDATEDATA is wrong.");
		return false;
	}
	unsigned char*	pbindata = CHM_ABS(pResUpdateData, pResUpdateData->byptr, unsigned char*);

	// transfer
	return pChmCntrl->MergeResponseUpdateData(pResUpdateData->chmpxid, pResUpdateData->length, pbindata, &(pResUpdateData->ts));
}

bool ChmEventMq::PxCltSendSetUpdateData(size_t length, const unsigned char* pdata, const struct timespec* pts)
{
	if(0 == length || !pdata || !pts){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	if(!pChmCntrl->IsChmpxType()){
		ERR_CHMPRN("PXCLT_SET_UPDATEDATA command MUST be sent from chmpx server node.");
		return false;
	}
	ChmIMData*	pImData	= pChmCntrl->GetImDataObj();

	// Allocation
	PCOMPKT	pComPkt;
	if(NULL == (pComPkt = ChmEventMq::AllocatePxCltPacket(CHMPX_CLT_SET_UPDATEDATA, length))){
		ERR_CHMPRN("Could not allocate memory for COMPKT.");
		return false;
	}

	// msgid
	msgid_t	dept_msgid = pImData->GetRandomChmpxMsgId();		// Get msgid in chmpx process
	if(CHM_INVALID_MSGID == dept_msgid){
		ERR_CHMPRN("Could not get own dept msgid(chmpx assigned).");
		CHM_Free(pComPkt);
		return false;
	}
	msgid_t	term_msgid = pImData->GetRandomClientMsgId();		// Get client assigned msgid
	if(CHM_INVALID_MSGID == term_msgid){
		ERR_CHMPRN("Could not get own terminal msgid(client assigned).");
		CHM_Free(pComPkt);
		return false;
	}

	// set common datas
	PPXCLT_ALL				pCltAll			= CVT_CLT_ALL_PTR_PXCOMPKT(pComPkt);
	PPXCLT_SET_UPDATEDATA	pSetUpdateData	= CVT_CLTPTR_SET_UPDATEDATA(pCltAll);
	SET_PXCLTPKT(pComPkt, true, CHMPX_CLT_SET_UPDATEDATA, dept_msgid, term_msgid, GetSerialNumber(), length);

	pSetUpdateData->head.type	= CHMPX_CLT_SET_UPDATEDATA;
	pSetUpdateData->head.length	= SIZEOF_CHMPX_CLT(CHMPX_CLT_SET_UPDATEDATA) + length;
	pSetUpdateData->ts.tv_sec	= pts->tv_sec;
	pSetUpdateData->ts.tv_nsec	= pts->tv_nsec;
	pSetUpdateData->length		= length;
	pSetUpdateData->byptr		= reinterpret_cast<unsigned char*>(SIZEOF_CHMPX_CLT(CHMPX_CLT_SET_UPDATEDATA));

	unsigned char*	psetbase	= CHM_OFFSET(pSetUpdateData, SIZEOF_CHMPX_CLT(CHMPX_CLT_SET_UPDATEDATA), unsigned char*);
	memcpy(psetbase, pdata, length);

	ChmLock	AutoLock(CHMLT_MQOBJ, CHMLT_READ);				// Lock

	// send
	//
	// [NOTICE]
	// Send to one of client's msgid
	//
	if(!Send(pComPkt, NULL, 0)){
		ERR_CHMPRN("Could not send CHMPX_CLT_SET_UPDATEDATA to msgid(0x%016" PRIx64 ").", term_msgid);
		CHM_Free(pComPkt);
		return false;
	}
	CHM_Free(pComPkt);

	return true;
}

bool ChmEventMq::PxCltReceiveSetUpdateData(PCOMHEAD pComHead, PPXCLT_ALL pCltAll)
{
	if(!pComHead || !pCltAll){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(COM_PX2C != pComHead->type){
		ERR_CHMPRN("COMHEAD type(%s) is wrong.", STRCOMTYPE(pComHead->type));
		return false;
	}
	if(CHMPX_CLT_SET_UPDATEDATA != pCltAll->val_head.type){
		ERR_CHMPRN("PXCLT_HEAD type(%s) is wrong.", STRPXCLTTYPE(pCltAll->val_head.type));
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	if(!msetfunc){
		ERR_CHMPRN("This object does not register merging update data function.");
		return true;
	}

	// check data
	PPXCLT_SET_UPDATEDATA	pSetUpdateData	= CVT_CLTPTR_SET_UPDATEDATA(pCltAll);
	if(0 == pSetUpdateData->length || NULL == pSetUpdateData->byptr){
		ERR_CHMPRN("Received CHMPX_CLT_SET_UPDATEDATA is wrong.");
		return false;
	}
	unsigned char*	pbindata = CHM_ABS(pSetUpdateData, pSetUpdateData->byptr, unsigned char*);

	// run callback function
	return msetfunc(reinterpret_cast<chmpx_h>(pChmCntrl), pSetUpdateData->length, pbindata, &(pSetUpdateData->ts));
}

bool ChmEventMq::PxCltSendResultUpdateData(chmpxid_t chmpxid, bool result)
{
	if(CHM_INVALID_CHMPXID == chmpxid){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	if(pChmCntrl->IsChmpxType()){
		ERR_CHMPRN("PXCLT_RESULT_UPDATEDATA command MUST be sent from client on chmpx server node.");
		return false;
	}
	ChmIMData*	pImData	= pChmCntrl->GetImDataObj();

	// Allocation
	PCOMPKT	pComPkt;
	if(NULL == (pComPkt = ChmEventMq::AllocatePxCltPacket(CHMPX_CLT_RESULT_UPDATEDATA))){
		ERR_CHMPRN("Could not allocate memory for COMPKT.");
		return false;
	}

	// msgid
	msgid_t	dept_msgid = pImData->GetRandomClientMsgId();		// Get client assigned msgid
	if(CHM_INVALID_MSGID == dept_msgid){
		ERR_CHMPRN("Could not get own dept msgid(chmpx assigned).");
		CHM_Free(pComPkt);
		return false;
	}
	msgid_t	term_msgid = pImData->GetRandomChmpxMsgId();		// Get msgid in chmpx process
	if(CHM_INVALID_MSGID == term_msgid){
		ERR_CHMPRN("Could not get own terminal msgid(client assigned).");
		CHM_Free(pComPkt);
		return false;
	}

	// set common datas
	PPXCLT_ALL					pCltAll				= CVT_CLT_ALL_PTR_PXCOMPKT(pComPkt);
	PPXCLT_RESULT_UPDATEDATA	pResultUpdateData	= CVT_CLTPTR_RESULT_UPDATEDATA(pCltAll);
	SET_PXCLTPKT(pComPkt, false, CHMPX_CLT_RESULT_UPDATEDATA, dept_msgid, term_msgid, GetSerialNumber(), 0);

	pResultUpdateData->head.type	= CHMPX_CLT_RESULT_UPDATEDATA;
	pResultUpdateData->head.length	= SIZEOF_CHMPX_CLT(CHMPX_CLT_RESULT_UPDATEDATA);
	pResultUpdateData->chmpxid		= chmpxid;
	pResultUpdateData->result		= (result ? CHMPX_COM_REQ_UPDATE_SUCCESS : CHMPX_COM_REQ_UPDATE_FAIL);

	ChmLock	AutoLock(CHMLT_MQOBJ, CHMLT_READ);				// Lock

	// send
	//
	// [NOTICE]
	// Send to one of client's msgid
	//
	if(!Send(pComPkt, NULL, 0)){
		ERR_CHMPRN("Could not send CHMPX_CLT_RESULT_UPDATEDATA to msgid(0x%016" PRIx64 ").", term_msgid);
		CHM_Free(pComPkt);
		return false;
	}
	CHM_Free(pComPkt);

	return true;
}

bool ChmEventMq::PxCltReceiveResultUpdateData(PCOMHEAD pComHead, PPXCLT_ALL pCltAll)
{
	if(!pComHead || !pCltAll){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(COM_C2PX != pComHead->type){
		ERR_CHMPRN("COMHEAD type(%s) is wrong.", STRCOMTYPE(pComHead->type));
		return false;
	}
	if(CHMPX_CLT_RESULT_UPDATEDATA != pCltAll->val_head.type){
		ERR_CHMPRN("PXCLT_HEAD type(%s) is wrong.", STRPXCLTTYPE(pCltAll->val_head.type));
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	PPXCLT_RESULT_UPDATEDATA	pResultUpdateData = CVT_CLTPTR_RESULT_UPDATEDATA(pCltAll);

	// check
	if(CHM_INVALID_CHMPXID == pResultUpdateData->chmpxid){
		ERR_CHMPRN("Received PXCLT_RESULT_UPDATEDATA command is something wrong.");
		return false;
	}

	// transfer
	return pChmCntrl->MergeResultUpdateData(pResultUpdateData->chmpxid, pResultUpdateData->result);
}

bool ChmEventMq::PxCltSendAbortUpdateData(void)
{
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	if(!pChmCntrl->IsChmpxType()){
		ERR_CHMPRN("PXCLT_ABORT_UPDATEDATA command MUST be sent from chmpx server node.");
		return false;
	}
	ChmIMData*	pImData	= pChmCntrl->GetImDataObj();

	// Allocation
	PCOMPKT	pComPkt;
	if(NULL == (pComPkt = ChmEventMq::AllocatePxCltPacket(CHMPX_CLT_ABORT_UPDATEDATA))){
		ERR_CHMPRN("Could not allocate memory for COMPKT.");
		return false;
	}

	// msgid
	msgid_t	dept_msgid = pImData->GetRandomChmpxMsgId();		// Get msgid in chmpx process
	if(CHM_INVALID_MSGID == dept_msgid){
		ERR_CHMPRN("Could not get own dept msgid(chmpx assigned).");
		CHM_Free(pComPkt);
		return false;
	}
	msgidlist_t	term_msgids;
	if(!pImData->GetMsgidListByUniqPid(term_msgids)){			// Get msgids for each client process
		ERR_CHMPRN("Could not get own terminal activated msgids(by each client).");
		CHM_Free(pComPkt);
		return false;
	}
	if(0 == term_msgids.size()){
		MSG_CHMPRN("There is no terminal activated msgids(by each client).");
		CHM_Free(pComPkt);
		return true;
	}

	// set common datas
	PPXCLT_ALL				pCltAll			= CVT_CLT_ALL_PTR_PXCOMPKT(pComPkt);
	PPXCLT_ABORT_UPDATEDATA	pAbortUpdateData= CVT_CLTPTR_ABORT_UPDATEDATA(pCltAll);
	SET_PXCLTPKT(pComPkt, true, CHMPX_CLT_ABORT_UPDATEDATA, dept_msgid, term_msgids.front(), GetSerialNumber(), 0);	// terminal msgid it temporary

	pAbortUpdateData->head.type		= CHMPX_CLT_ABORT_UPDATEDATA;
	pAbortUpdateData->head.length	= SIZEOF_CHMPX_CLT(CHMPX_CLT_ABORT_UPDATEDATA);

	ChmLock	AutoLock(CHMLT_MQOBJ, CHMLT_READ);				// Lock

	// send loop
	for(msgidlist_t::const_iterator iter = term_msgids.begin(); iter != term_msgids.end(); ++iter){
		// set terminate msgid( uses same serial number because of terminated msgid is different )
		pComPkt->head.term_ids.msgid	= *iter;
		pComPkt->head.peer_term_msgid	= *iter;

		// send
		if(!Send(pComPkt, NULL, 0)){
			WAN_CHMPRN("Could not send CHMPX_CLT_CLOSE_NOTIFY to msgid(0x%016" PRIx64 "), but continue....", *iter);
		}
	}
	CHM_Free(pComPkt);

	return true;
}

bool ChmEventMq::PxCltReceiveAbortUpdateData(PCOMHEAD pComHead, PPXCLT_ALL pCltAll)
{
	if(!pComHead || !pCltAll){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(COM_PX2C != pComHead->type){
		ERR_CHMPRN("COMHEAD type(%s) is wrong.", STRCOMTYPE(pComHead->type));
		return false;
	}
	if(CHMPX_CLT_ABORT_UPDATEDATA != pCltAll->val_head.type){
		ERR_CHMPRN("PXCLT_HEAD type(%s) is wrong.", STRPXCLTTYPE(pCltAll->val_head.type));
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}

	// check data -> Nothing to do
	//PPXCLT_ABORT_UPDATEDATA	pAbortUpdateData = CVT_CLTPTR_ABORT_UPDATEDATA(pCltAll);

	// remove all stacked parameters.
	while(!fullock::flck_trylock_noshared_mutex(&mparam_list_lockval));	// LOCK
	for(mqmparamlist_t::iterator iter = merge_param_list.begin(); iter != merge_param_list.end(); ++iter){
		PCHM_UPDATA_PARAM	pUpdateDataParam = *iter;
		CHM_Delete(pUpdateDataParam);
	}
	merge_param_list.clear();
	fullock::flck_unlock_noshared_mutex(&mparam_list_lockval);			// UNLOCK

	// break merge loop in thread
	notify_merge_update = false;

	return true;
}

//---------------------------------------------------------
// Methods for Sending/Receiving command
//---------------------------------------------------------
bool ChmEventMq::PxCltSendCloseNotify(msgidlist_t& msgids)
{
	if(0 == msgids.size()){
		ERR_CHMPRN("There is no msgid in closed msgid list.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}

	// Allocation
	PCOMPKT	pComPkt;
	ssize_t	ext_length = static_cast<ssize_t>(msgids.size() * sizeof(msgid_t));
	if(NULL == (pComPkt = ChmEventMq::AllocatePxCltPacket(CHMPX_CLT_CLOSE_NOTIFY, ext_length))){
		ERR_CHMPRN("Could not allocate memory for COMPKT.");
		return false;
	}

	// set common datas
	PPXCLT_ALL			pCltAll		= CVT_CLT_ALL_PTR_PXCOMPKT(pComPkt);
	PPXCLT_CLOSE_NOTIFY	pCloseNotify= CVT_CLTPTR_CLOSE_NOTIFY(pCltAll);
	SET_PXCLTPKT(pComPkt, pChmCntrl->IsChmpxType(), CHMPX_CLT_CLOSE_NOTIFY, msgids.front(), CHM_INVALID_MSGID, GetSerialNumber(), ext_length);	// departure msgid is one of msgids

	pCloseNotify->head.type		= CHMPX_CLT_CLOSE_NOTIFY;
	pCloseNotify->head.length	= SIZEOF_CHMPX_CLT(CHMPX_CLT_CLOSE_NOTIFY) + ext_length;
	pCloseNotify->count			= static_cast<long>(msgids.size());
	pCloseNotify->pmsgids		= reinterpret_cast<msgid_t*>(SIZEOF_CHMPX_CLT(CHMPX_CLT_CLOSE_NOTIFY));

	long		pos				= 0;
	msgid_t*	psetbase		= CHM_OFFSET(pCloseNotify, SIZEOF_CHMPX_CLT(CHMPX_CLT_CLOSE_NOTIFY), msgid_t*);
	for(msgidlist_t::const_iterator iter = msgids.begin(); iter != msgids.end(); ++iter, ++pos){
		psetbase[pos] = *iter;
	}

	ChmLock	AutoLock(CHMLT_MQOBJ, CHMLT_READ);				// Lock

	// get all msgids
	msgidlist_t	dest_msgids;
	dest_idfd_map.get_keys(dest_msgids);

	// send loop
	for(msgidlist_t::const_iterator iter = dest_msgids.begin(); iter != dest_msgids.end(); ++iter){
		// set terminate msgid( uses same serial number because of terminated msgid is different )
		pComPkt->head.term_ids.msgid	= *iter;
		pComPkt->head.peer_term_msgid	= *iter;

		// send
		if(!Send(pComPkt, NULL, 0)){
			WAN_CHMPRN("Could not send CHMPX_CLT_CLOSE_NOTIFY to msgid(0x%016" PRIx64 "), but continue....", *iter);
		}else{
			// [NOTE]
			// If the client on slave, enough to sending once.
			if(pChmCntrl->IsClientOnSlvType()){
				break;
			}
		}
	}
	CHM_Free(pComPkt);

	return true;
}

bool ChmEventMq::PxCltReceiveCloseNotify(PCOMHEAD pComHead, PPXCLT_ALL pCltAll)
{
	if(!pComHead || !pCltAll){
		ERR_CHMPRN("Parameter are wrong.");
		return false;
	}
	if(COM_C2PX != pComHead->type && COM_PX2C != pComHead->type){
		ERR_CHMPRN("COMHEAD type(%s) is wrong.", STRCOMTYPE(pComHead->type));
		return false;
	}
	if(CHMPX_CLT_CLOSE_NOTIFY != pCltAll->val_head.type){
		ERR_CHMPRN("PXCLT_HEAD type(%s) is wrong.", STRPXCLTTYPE(pCltAll->val_head.type));
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}

	// check data
	PPXCLT_CLOSE_NOTIFY	pCloseNotify = CVT_CLTPTR_CLOSE_NOTIFY(pCltAll);
	if(pCloseNotify->count <= 0 || !pCloseNotify->pmsgids){
		ERR_CHMPRN("msgids pointer or count is empty.");
		return false;
	}

	// if msgid in cache, so close and remove it from cache.
	msgid_t*	pmsgids = CHM_ABS(pCloseNotify, pCloseNotify->pmsgids, msgid_t*);
	for(long cnt = 0; cnt < pCloseNotify->count; cnt++){
		if(!CloseDestMQ(pmsgids[cnt])){
			ERR_CHMPRN("Failed to close and remove cache for msgid(0x%016" PRIx64 ").", pmsgids[cnt]);
		}
	}
	return true;
}

bool ChmEventMq::PxCltSendJoinNotify(pid_t pid)
{
	if(CHM_INVALID_PID == pid){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(IsEmpty()){
		ERR_CHMPRN("Object is not initialized.");
		return false;
	}
	if(pChmCntrl->IsChmpxType()){
		ERR_CHMPRN("Could not send CHMPX_CLT_JOIN_NOTIFY from CHMPX process.");
		return false;
	}
	ChmIMData*	pImData	= pChmCntrl->GetImDataObj();

	// Allocation
	PCOMPKT	pComPkt;
	if(NULL == (pComPkt = ChmEventMq::AllocatePxCltPacket(CHMPX_CLT_JOIN_NOTIFY))){
		ERR_CHMPRN("Could not allocate memory for COMPKT.");
		return false;
	}

	// terminal msgid
	msgid_t	term_msgid = pImData->GetRandomChmpxMsgId();		// Get msgid in chmpx process
	if(CHM_INVALID_MSGID == term_msgid){
		ERR_CHMPRN("Could not get own msgid(terminal).");
		CHM_Free(pComPkt);
		return false;
	}

	// departure msgid
	msgid_t	dept_msgid;
	if(pChmCntrl->IsClientOnSlvType()){
		dept_msgid = ActivatedMsgId();
	}else{
		dept_msgid = pImData->GetRandomClientMsgId();
	}
	if(CHM_INVALID_MSGID == dept_msgid){
		ERR_CHMPRN("Could not get own msgid(departure).");
		CHM_Free(pComPkt);
		return false;
	}

	// set common datas
	chmpxid_t			selfchmpxid	= pImData->GetSelfChmpxId();
	PPXCLT_ALL			pCltAll		= CVT_CLT_ALL_PTR_PXCOMPKT(pComPkt);
	PPXCLT_JOIN_NOTIFY	pJoinNotify	= CVT_CLTPTR_JOIN_NOTIFY(pCltAll);
	SET_PXCLTPKT(pComPkt, pChmCntrl->IsChmpxType(), CHMPX_CLT_JOIN_NOTIFY, dept_msgid, term_msgid, GetSerialNumber(), 0);

	// cppcheck-suppress unmatchedSuppression
	// cppcheck-suppress redundantAssignment
	pComPkt->head.dept_ids.chmpxid	= selfchmpxid;					// do not case for this value.
	// cppcheck-suppress unmatchedSuppression
	// cppcheck-suppress redundantAssignment
	pComPkt->head.term_ids.chmpxid	= selfchmpxid;
	pJoinNotify->head.type			= CHMPX_CLT_JOIN_NOTIFY;
	pJoinNotify->head.length		= SIZEOF_CHMPX_CLT(CHMPX_CLT_JOIN_NOTIFY);
	pJoinNotify->pid				= pid;

	ChmLock	AutoLock(CHMLT_MQOBJ, CHMLT_READ);				// Lock

	// send
	if(!Send(pComPkt, NULL, 0)){
		WAN_CHMPRN("Could not send CHMPX_CLT_JOIN_NOTIFY to msgid(0x%016" PRIx64 ").", dept_msgid);
	}

	if(pChmCntrl->IsClientOnSlvType()){
		DisactivatedMsgId(dept_msgid);		// Do not need to wait the response.
	}
	CHM_Free(pComPkt);

	return true;
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
