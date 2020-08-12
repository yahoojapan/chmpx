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
#include <sys/epoll.h>
#include <signal.h>

#include "chmcommon.h"
#include "chmcntrl.h"
#include "chmsigcntrl.h"
#include "chmconfutil.h"
#include "chmdbg.h"

using namespace	std;

//---------------------------------------------------------
// Symbols
//---------------------------------------------------------
// The MQ object is allowed making/registering many.
//
#define	EVMAP_KEY_CONF					0
#define	EVMAP_KEY_SOCK					1
#define	EVMAP_KEY_MQ					2
#define	EVMAP_KEY_SHM					3
#define	EVMAP_MAX_SIZE					4

#define	CVT_EVOBJTYPE_TO_EVMAP(type)	(	EVOBJ_TYPE_CONF		== type ? EVMAP_KEY_CONF: \
											EVOBJ_TYPE_EVSOCK	== type ? EVMAP_KEY_SOCK: \
											EVOBJ_TYPE_EVMQ		== type ? EVMAP_KEY_MQ	: EVMAP_KEY_SHM		)

#define	CVT_EVMAP_TO_EVOBJTYPE(key)		(	EVMAP_KEY_CONF	== key ? EVOBJ_TYPE_CONF	: \
											EVMAP_KEY_SOCK	== key ? EVOBJ_TYPE_EVSOCK	: \
											EVMAP_KEY_MQ	== key ? EVOBJ_TYPE_EVMQ 	: EVOBJ_TYPE_EVSHM	)

//---------------------------------------------------------
// Class Variable
//---------------------------------------------------------
const int		ChmCntrl::DEFAULT_MAX_EVENT_CNT;
const int		ChmCntrl::DEFAULT_EVENT_TIMEOUT;
const int		ChmCntrl::EVENT_NOWAIT;
volatile bool	ChmCntrl::DoLoop = true;

//---------------------------------------------------------
// Class Methods
//---------------------------------------------------------
void ChmCntrl::LoopBreakHandler(int signum)
{
	ChmCntrl::DoLoop = false;
}

//---------------------------------------------------------
// Constructor/Destructor
//---------------------------------------------------------
ChmCntrl::ChmCntrl(void) : chmcntrltype(CHMCHNTL_TYPE_CHMPXPROC), eqfd(CHM_INVALID_HANDLE), pConfObj(NULL), pEventMq(NULL), pEventSock(NULL), pEventShm(NULL), auto_rejoin(false), bup_cfg(""), is_close_notify(false)
{
}

ChmCntrl::~ChmCntrl(void)
{
	Clean();
}

//---------------------------------------------------------
// Methods
//---------------------------------------------------------
bool ChmCntrl::Clean(bool is_clean_bup)
{
	if(pEventShm){
		pEventShm->Clean();
		CHM_Delete(pEventShm);
	}
	if(pEventSock){
		pEventSock->Clean();
		CHM_Delete(pEventSock);
	}
	if(pEventMq){
		pEventMq->Clean();
		CHM_Delete(pEventMq);
	}
	if(pConfObj){
		pConfObj->Clean();
		CHM_Delete(pConfObj);
	}
	ImData.Close();
	CHM_CLOSE(eqfd);

	if(is_clean_bup){
		bup_cfg.erase();
	}
	return true;
}

bool ChmCntrl::IsInitialized(void) const
{
	if(	CHM_INVALID_SOCK == eqfd			||
		!ImData.IsInitialized()				||
		!pConfObj || pConfObj->IsEmpty()	||
		(CHMCHNTL_TYPE_CHMPXPROC == chmcntrltype && EVMAP_MAX_SIZE != evobjmap.size())	||
		(CHMCHNTL_TYPE_CHMPXPROC != chmcntrltype && 1 != evobjmap.size())				)
	{
		return false;
	}
	return true;
}

ChmEventBase* ChmCntrl::GetEventBaseObj(EVOBJTYPE type)
{
	int				key		= CVT_EVOBJTYPE_TO_EVMAP(type);
	ChmEventBase*	pEvObj	= NULL;

	if(evobjmap.end() != evobjmap.find(key)){
		pEvObj = evobjmap.find(key)->second;
	}
	return pEvObj;
}

ChmEventBase* ChmCntrl::FindEventBaseObj(int fd)
{
	if(CHM_INVALID_HANDLE == fd){
		return NULL;
	}

	ChmEventBase*	pEvObj = NULL;
	for(evobj_map_t::const_iterator iter = evobjmap.begin(); iter != evobjmap.end(); ++iter){
		if((iter->second)->IsEventQueueFd(fd)){
			pEvObj = iter->second;
			break;
		}
	}
	return pEvObj;
}

bool ChmCntrl::GetEventBaseObjType(const ChmEventBase* pEvObj, EVOBJTYPE& type)
{
	if(!pEvObj){
		return false;
	}

	bool	result = false;
	for(evobj_map_t::const_iterator iter = evobjmap.begin(); iter != evobjmap.end(); ++iter){
		if(iter->second == pEvObj){
			type = CVT_EVMAP_TO_EVOBJTYPE(iter->first);
			result = true;
			break;
		}
	}
	return result;
}

// [NOTE]
// The cfgfile parameter can be specified two type variable.
// One is configuration file(.ini/.json/.yaml) path, the other is JSON string.
// It will be judged automatically in this method.
// This specification has been added because of support JSON format later added.
//
bool ChmCntrl::Initialize(const char* cfgfile, CHMCNTRLTYPE type, bool is_auto_rejoin, chm_merge_get_cb mgetfp, chm_merge_set_cb setfp, chm_merge_lastts_cb mlastfp, short ctlport, const char* cuk)
{
	// set type
	chmcntrltype = type;

	// epoll
	if(!InitializeEventFd()){
		ERR_CHMPRN("Could not initialize epoll fd.");
		Clean(bup_cfg.empty());
		return false;
	}

	// Load configuration
	string	confval("");
	if(NULL == (pConfObj = CHMConf::GetCHMConf(eqfd, this, cfgfile, ctlport, cuk, true, &confval))){
		ERR_CHMPRN("Failed to make configuration object from %s", ISEMPTYSTR(cfgfile) ? "empty" : cfgfile);
		Clean(bup_cfg.empty());
		return false;
	}
	CHMCFGINFO	chmcfg;
	if(!pConfObj->GetConfiguration(chmcfg)){
		ERR_CHMPRN("Failed to load configuration from %s", cfgfile);
		Clean(bup_cfg.empty());
		return false;
	}
	if(CHMCHNTL_TYPE_CHMPXPROC == chmcntrltype){
		// set inotify(only chmpx proc)
		if(!pConfObj->SetEventQueue()){
			ERR_CHMPRN("Failed to set/initialize confobj.");
			Clean(bup_cfg.empty());
			return false;
		}
	}
	if(CHMCHNTL_TYPE_CLIENT_ONSLAVE == chmcntrltype){
		// If client process joining on slave chmpx, MQPERATTACH and MAXQPERCLIENTMQ should be "1".
		// But you can set not "1", so put a message here.
		//
		if(1 != chmcfg.mqcnt_per_attach || 1 != chmcfg.max_q_per_clientmq){
			MSG_CHMPRN("Client process on slave chmpx must be 1 for MQPERATTACH and MAXQPERCLIENTMQ in configuration.");
		}
	}

	// Initialize Internal Memory data
	if(!ImData.Initialize(pConfObj, eqfd, (CHMCHNTL_TYPE_CHMPXPROC == chmcntrltype))){
		ERR_CHMPRN("Failed to initialize internal memory data.");
		Clean(bup_cfg.empty());
		return false;
	}

	// Initialize Sockets
	if(CHMCHNTL_TYPE_CHMPXPROC == chmcntrltype){
		pEventSock = new ChmEventSock(eqfd, this, pConfObj->IsSsl());
		if(!pEventSock->SetEventQueue()){
			ERR_CHMPRN("Failed to set/initialize socket event obj.");
			Clean(bup_cfg.empty());
			return false;
		}
	}else{
		pEventSock = NULL;
	}

	// Initialize MQ
	if(CHMCHNTL_TYPE_CHMPXPROC == chmcntrltype){
		pEventMq = new ChmEventMq(eqfd, this);
	}else{
		pEventMq = new ChmEventMq(eqfd, this, mgetfp, setfp, mlastfp);
	}
	if(!pEventMq->SetEventQueue()){
		ERR_CHMPRN("Failed to set/initialize mq event obj.");
		Clean(bup_cfg.empty());
		return false;
	}

	// Initialize client process HUP
	pid_t	pid = getpid();
	if(CHMCHNTL_TYPE_CHMPXPROC == chmcntrltype){
		pEventShm = new ChmEventShm(eqfd, this, ImData.GetShmPath());
		if(!pEventShm->SetEventQueue()){
			ERR_CHMPRN("Failed to set/initialize chmshm event obj.");
			Clean(bup_cfg.empty());
			return false;
		}
	}else{
		pEventShm = NULL;

		// add own(client) pid to CHMSHM
		ImData.AddClientPid(pid);
	}

	if(CHMCHNTL_TYPE_CHMPXPROC == chmcntrltype){
		evobjmap[EVMAP_KEY_CONF]	= pConfObj;
		evobjmap[EVMAP_KEY_SOCK]	= pEventSock;
		evobjmap[EVMAP_KEY_SHM]		= pEventShm;
	}
	evobjmap[EVOBJ_TYPE_EVMQ]		= pEventMq;

	// backup configuration file path.
	bup_cfg = confval;

	// rejoin mode
	if(CHMCHNTL_TYPE_CHMPXPROC == chmcntrltype){
		auto_rejoin = false;
	}else{
		auto_rejoin = is_auto_rejoin;
	}

	// Send notification for client joining.
	if(CHMCHNTL_TYPE_CHMPXPROC != chmcntrltype){
		if(!pEventMq->PxCltSendJoinNotify(pid)){
			ERR_CHMPRN("Failed to send notification for client joining to chmpx process(server/slave), but continue...");
		}
	}

#if	0
	// Dump for DEBUG
	if(CHMDBG_MSG == GetChmDbgMode()){
		stringstream	ss;
		ImData.Dump(ss);
		fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "[DUMP] IMDATA{\n");
		fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "%s", ss.str().c_str());
		fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "}\n");
	}
#endif

	return true;
}

bool ChmCntrl::ReInitialize(long waitms, int trycnt)
{
	if(CHMCHNTL_TYPE_CHMPXPROC == chmcntrltype){
		MSG_CHMPRN("This is chmpx process, so could not re-initialize.");
		return false;
	}
	if(IsInitialized()){
		MSG_CHMPRN("Already initialized, so do not re-initialize.");
		return false;
	}
	if(!IsChmpxExit()){
		MSG_CHMPRN("There is no backup cfgfile path, so could not re-initialize.");
		return false;
	}
	if(!auto_rejoin){
		MSG_CHMPRN("Not retry to join mode.");
		return false;
	}

	struct timespec	sleeptime = {0, 0};
	if(0 < waitms){
		SET_TIMESPEC(&sleeptime, (waitms / 1000), ((waitms % 1000) * 1000 * 1000));
	}

	bool	no_give_uo = false;
	if(0 == trycnt){
		no_give_uo = true;
	}

	do{
		if(0 < waitms){
			nanosleep(&sleeptime, NULL);
		}
		// try to join
		if(Initialize(bup_cfg.c_str(), chmcntrltype, auto_rejoin)){
			return true;
		}
	}while(no_give_uo || 0 < --trycnt);

	return false;
}

// [NOTE]
// This method initializes ChmCntrl object for only reading chmshm.
// Initialized object by this methods can be used only dumping chmshm
//
// And the cfgfile parameter can be specified two type variable.
// One is configuration file(.ini/.json/.yaml) path, the other is JSON string.
// It will be judged automatically in this method.
// This specification has been added because of support JSON format later added.
//
bool ChmCntrl::OnlyAttachInitialize(const char* cfgfile, short ctlport, const char* cuk)
{
	// set type
	chmcntrltype = CHMCHNTL_TYPE_CLIENT_ONSLAVE;	// Dummy

	// Load configuration
	string	confval("");
	if(NULL == (pConfObj = CHMConf::GetCHMConf(eqfd, this, cfgfile, ctlport, cuk, true, &confval))){
		ERR_CHMPRN("Failed to make configuration object from %s", ISEMPTYSTR(cfgfile) ? "empty" : cfgfile);
		Clean(bup_cfg.empty());
		return false;
	}

	// Initialize Internal Memory data
	if(!ImData.Initialize(pConfObj, CHM_INVALID_HANDLE, false)){
		ERR_CHMPRN("Failed to initialize internal memory data.");
		Clean(bup_cfg.empty());
		return false;
	}

	// Initialize Sockets
	pEventSock = NULL;

	// Initialize MQ
	pEventMq = NULL;

	// Initialize client process HUP
	pEventShm = NULL;

	// backup configuration file path.
	bup_cfg = confval;

	// rejoin mode
	auto_rejoin = false;

#if	0
	// Dump for DEBUG
	if(CHMDBG_MSG == GetChmDbgMode()){
		stringstream	ss;
		ImData.Dump(ss);
		fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "[DUMP] IMDATA{\n");
		fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "%s", ss.str().c_str());
		fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "}\n");
	}
#endif

	return true;
}

bool ChmCntrl::InitializeEventFd(void)
{
	CHM_CLOSE(eqfd);

	// create epoll event fd
	if(-1 == (eqfd = epoll_create1(EPOLL_CLOEXEC))){
		ERR_CHMPRN("Failed to create epoll fd, errno=%d", errno);
		return false;
	}
	return true;
}

//
// This method is called from CHMConf object when configuration file changing.
//
bool ChmCntrl::ConfigurationUpdateNotify(void)
{
	if(!IsInitialized()){
		ERR_CHMPRN("This object is not initialized yet.");
		return false;
	}

	// First, reload configuration for ImData data
	if(!ImData.ReloadConfiguration()){
		ERR_CHMPRN("Failed to reinitialize internal data for ImData.");
		return false;
	}

	// Second, update all objects without conf.
	bool	result = true;
	if(!pEventSock->UpdateInternalData()){
		ERR_CHMPRN("Failed to reinitialize internal data for Socket Object.");
		result = false;
	}
	if(!pEventMq->UpdateInternalData()){
		ERR_CHMPRN("Failed to reinitialize internal data for MQ Object.");
		result = false;
	}
	if(!pEventShm->UpdateInternalData()){
		ERR_CHMPRN("Failed to reinitialize internal data for Shm Object.");
		result = false;
	}
	return result;
}

// [NOTICE]
// This method is only for debugging SSL.
//
void ChmCntrl::AllowSelfCert(void)
{
	if(pEventSock){
		pEventSock->AllowSelfSignedCert();
	}
}

//
// Event loop for chmpx process.
//
bool ChmCntrl::EventLoop(void)
{
	if(!IsInitialized()){
		ERR_CHMPRN("This object is not initialized yet.");
		return false;
	}
	if(CHMCHNTL_TYPE_CHMPXPROC != chmcntrltype){
		ERR_CHMPRN("This object is not initialized for CHMPX process.");
		return false;
	}

	// Get signal mask
	ChmSigCntrl	sigcntrl;
	sigset_t	sigset;
	if(!sigcntrl.GetSignalMask(sigset)){
		ERR_CHMPRN("Could not get signal mask value.");
		return false;
	}

	// Loop
	struct epoll_event	events[ChmCntrl::DEFAULT_MAX_EVENT_CNT];
	int					max_events	= ChmCntrl::DEFAULT_MAX_EVENT_CNT;
	int					timeout		= ChmCntrl::DEFAULT_EVENT_TIMEOUT;
	bool				is_slave	= ImData.IsSlaveMode();
	while(ChmCntrl::DoLoop){
		// check all client closing.
		if(is_close_notify && pEventSock){
			is_close_notify = false;

			// Set status SUSPEND(if merging, stop it)
			if(!pEventSock->DoSuspend()){
				ERR_CHMPRN("Failed to set status SUSPEND.");
			}
		}

		// wait event
		int eventcnt = epoll_pwait(eqfd, events, max_events, timeout, &sigset);

		if(-1 == eventcnt){
			if(EINTR != errno){
				ERR_CHMPRN("Failed to wait epoll by errno=%d", errno);
				break;
			}
			MSG_CHMPRN("Break waiting epoll by SIGNAL, continue to loop.");
			continue;
		}else if(0 == eventcnt){
			//MSG_CHMPRN("Timeouted for waiting epoll event.");
			if(is_slave && pEventSock){
				if(0L == ImData.GetUpServerCount()){
					// Check any server up
					if(pEventSock->InitialAllServerStatus()){
						if(0L < ImData.GetUpServerCount()){
							// Try to connect servers
							//MSG_CHMPRN("Try to connect servers on RING(mode is SLAVE).");
							pEventSock->ConnectServers();	// not need to check result.
						}else{
							// sleep
							struct timespec	sleeptime = {0, 100 * 1000 * 1000};		// 100ms
							nanosleep(&sleeptime, NULL);
						}
					}
				}
			}else if(pEventSock){
				// Server node
				if(1L >= ImData.GetUpServerCount()){
					// [NOTE]
					// This case is that there are no other server node up.
					if(pEventSock->InitialAllServerStatus()){
						if(1L < ImData.GetUpServerCount()){
							// Try to connect servers
							//MSG_CHMPRN("Try to connect servers on RING(mode is SERVER).");
							pEventSock->ConnectRing();		// not need to check result.
						}else{
							// sleep
							struct timespec	sleeptime = {0, 100 * 1000 * 1000};		// 100ms
							nanosleep(&sleeptime, NULL);
						}
					}
				}
			}
			continue;
		}

		// loop for dispatching event
		for(int cnt = 0; cnt < eventcnt; cnt++){
			//MSG_CHMPRN("Event type(%lu) on event fd(%d).", events[cnt].events, events[cnt].data.fd);

			ChmEventBase*	pEvObj = FindEventBaseObj(events[cnt].data.fd);
			if(!pEvObj){
				MSG_CHMPRN("Unknown event(%d) on fd(%d), so skip this event and close this fd.", events[cnt].events, events[cnt].data.fd);
				// Most of this case is occured on the server side socket.
				// Because that socket set epoll with EPOLLIN and EPOLLRDHUP, then maybe those event
				// are occured at same time. So that, one of event occured after the other event.
				// Thus we close it here.
				epoll_ctl(eqfd, EPOLL_CTL_DEL, events[cnt].data.fd, NULL);
				close(events[cnt].data.fd);
				continue;
			}
			//MSG_CHMPRN("Get event fd(%d) - type(0x%x).", events[cnt].data.fd, events[cnt].events);

			// do event
			if(	EPOLLRDHUP	== (events[cnt].events & EPOLLRDHUP)||
				EPOLLERR	== (events[cnt].events & EPOLLERR)	||
				EPOLLHUP	== (events[cnt].events & EPOLLHUP)	)
			{
				// Hungup fd
				//
				if(!pEvObj->NotifyHup(events[cnt].data.fd)){
					ERR_CHMPRN("Failed to notify HUP from event fd(%d).", events[cnt].data.fd);
				}
			}else{
				// Receive & Processing(EPOLLIN, etc...)
				//
				// [NOTE]
				// If the receive data needs processing, the event object calls Processing()
				// method by itself.
				//
				if(!pEvObj->Receive(events[cnt].data.fd)){
					ERR_CHMPRN("Failed to receive data from event fd(%d).", events[cnt].data.fd);
				}
			}
		}
	}
	return true;
}

//
// Event Processing
//
// [NOTE]
// This method is called from Event objects for processing events after receiving.
// If the data is received on chmpx process and the process uses multi-thread,
// this method is called not main thread bu another thread.
//
bool ChmCntrl::Processing(PCOMPKT pComPkt, EVOBJTYPE call_ev_type)
{
	if(!IsInitialized()){
		ERR_CHMPRN("This object is not initialized yet.");
		return false;
	}
	if(!pComPkt){
		MSG_CHMPRN("pComPkt is NULL.");
		return true;
	}

	// processing object
	ChmEventBase*	pProcessObj = NULL;

	if(COM_C2C == pComPkt->head.type){
		// set processing event object by input event type
		if(EVOBJ_TYPE_EVMQ == call_ev_type){
			// Input from MQ, so processing do to socket
			//
			pProcessObj = GetEventBaseObj(EVOBJ_TYPE_EVSOCK);

		}else if(EVOBJ_TYPE_EVSOCK == call_ev_type){
			// Input from Socket, so processing do to MQ
			//
			pProcessObj = GetEventBaseObj(EVOBJ_TYPE_EVMQ);
		}

	}else if(COM_PX2PX == pComPkt->head.type){
		pProcessObj = GetEventBaseObj(EVOBJ_TYPE_EVSOCK);

	}else if(COM_S2PX == pComPkt->head.type){
		//
		ERR_CHMPRN("This library does not implement for transfer signal by ComPkt type=%" PRIu64 "(dept=%" PRIu64 ":%" PRIu64 ", term=%" PRIu64 ":%" PRIu64 "), so skip this.", 
			pComPkt->head.type, pComPkt->head.dept_ids.chmpxid, pComPkt->head.dept_ids.msgid, pComPkt->head.term_ids.chmpxid, pComPkt->head.term_ids.msgid);

	}else if(COM_F2PX == pComPkt->head.type){
		//
		ERR_CHMPRN("This library does not implement for transfer inotify by ComPkt type=%" PRIu64 "(dept=%" PRIu64 ":%" PRIu64 ", term=%" PRIu64 ":%" PRIu64 "), so skip this.",
			pComPkt->head.type, pComPkt->head.dept_ids.chmpxid, pComPkt->head.dept_ids.msgid, pComPkt->head.term_ids.chmpxid, pComPkt->head.term_ids.msgid);

	}else if(COM_C2PX == pComPkt->head.type || COM_PX2C == pComPkt->head.type){
		// Input from MQ, so processing do on MQ
		//
		PPXCLT_ALL pCltAll = CVT_CLT_ALL_PTR_PXCOMPKT(pComPkt);
		if(IS_PXCLT_TYPE(pCltAll, CHMPX_CLT_JOIN_NOTIFY)){
			// CHMPX_CLT_JOIN_NOTIFY type is processing by ChmEventSock.
			pProcessObj = GetEventBaseObj(EVOBJ_TYPE_EVSOCK);
		}else{
			pProcessObj = GetEventBaseObj(EVOBJ_TYPE_EVMQ);
		}

	}else{	// COM_UNKNOWN
		ERR_CHMPRN("Unknown COMPKT type(%" PRIu64 ").", pComPkt->head.type);
	}

	// dispatching
	if(!pProcessObj){
		MSG_CHMPRN("Nothing to do for ComPkt type=%" PRIu64 "(dept=%" PRIu64 ":%" PRIu64 ", term=%" PRIu64 ":%" PRIu64 ") after receiving.",
			pComPkt->head.type, pComPkt->head.dept_ids.chmpxid, pComPkt->head.dept_ids.msgid, pComPkt->head.term_ids.chmpxid, pComPkt->head.term_ids.msgid);

	}else if(!pProcessObj->Processing(pComPkt)){
		ERR_CHMPRN("Failed to processing for ComPkt type=%" PRIu64 "(dept=%" PRIu64 ":%" PRIu64 ", term=%" PRIu64 ":%" PRIu64 ") after receiving.",
			pComPkt->head.type, pComPkt->head.dept_ids.chmpxid, pComPkt->head.dept_ids.msgid, pComPkt->head.term_ids.chmpxid, pComPkt->head.term_ids.msgid);
		return false;
	}
	return true;
}

//
// Receive COMPKT for client joining server chmpx.
//
// [Return value]
// true:	succeed to read / could not read event.
// false:	something error occured
//
// * If returned true, must check *ppComPkt value which is not NULL for succeed.
//   *ppComPkt is NULL, it means timeout.
//
// [Usage]
// If you get COMPKT and body data for each, specify ppbody and plength argument.
// If you get COMPKT for all of head and body, specify these are NULL.
//
bool ChmCntrl::Receive(PCOMPKT* ppComPkt, unsigned char** ppbody, size_t* plength, int timeout_ms, bool no_giveup_rejoin)
{
	if(!ppComPkt || (!ppbody && plength) || (ppbody && !plength)){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	if(CHMCHNTL_TYPE_CLIENT_ONSERVER != chmcntrltype){
		ERR_CHMPRN("This object is not initialized for server.");
		return false;
	}
	if(!IsInitialized()){
		if(!ReInitialize(no_giveup_rejoin ? timeout_ms : 0L, no_giveup_rejoin ? 0 : 1)){
			ERR_CHMPRN("This object is not initialized yet.");
			return false;
		}
	}

	// initialize values
	*ppComPkt	= NULL;
	if(ppbody){
		*ppbody	= NULL;
		*plength= 0L;
	}

	// Get signal mask
	ChmSigCntrl	sigcntrl;
	sigset_t	sigset;
	if(!sigcntrl.GetSignalMask(sigset)){
		ERR_CHMPRN("Could not get signal mask value.");
		return false;
	}

	// Loop for read not expected event
	do{
		struct epoll_event	readevent;			// One event

		// wait event
		int eventcnt = epoll_pwait(eqfd, &readevent, 1, timeout_ms, &sigset);
		if(-1 == eventcnt){
			if(EINTR == errno){
				MSG_CHMPRN("Break waiting epoll by SIGNAL, continue to loop.");
				continue;
			}
			ERR_CHMPRN("Failed to wait epoll by errno=%d", errno);
			return false;

		}else if(0 == eventcnt){
			// there is no event now, check chmpx process down
			if(!ImData.IsNeedDetach()){
				return true;
			}
			MSG_CHMPRN("Found notification, Chmpx process down.");

			// Detach
			Clean(false);

			// Re-join
			if(!ReInitialize(timeout_ms, no_giveup_rejoin ? 0 : 1)){		// timeout(ms)
				MSG_CHMPRN("Try to rejoin, but could not join.");
				return false;
			}
			continue;
		}

		//MSG_CHMPRN("Event type(%lu) on event fd(%d).", readevent.events, readevent.data.fd);

		ChmEventBase*	pEvObj = FindEventBaseObj(readevent.data.fd);
		if(!pEvObj){
			MSG_CHMPRN("Unknown event(%d) on fd(%d), so skip this event and close this fd.", readevent.events, readevent.data.fd);
			// Most of this case is occured on the server side socket.
			// Because that socket set epoll with EPOLLIN and EPOLLRDHUP, then maybe those event
			// are occured at same time. So that, one of event occured after the other event.
			// Thus we close it here.
			epoll_ctl(eqfd, EPOLL_CTL_DEL, readevent.data.fd, NULL);
			close(readevent.data.fd);
			continue;
		}
		MSG_CHMPRN("Get event fd(%d) - type(%d).", readevent.data.fd, readevent.events);

		// do event
		if(EPOLLRDHUP == (readevent.events & EPOLLRDHUP)){
			// Hungup fd
			if(!pEvObj->NotifyHup(readevent.data.fd)){
				ERR_CHMPRN("event fd(%d) is hangup, but failed to do.", readevent.data.fd);
			}
			// retry to read.
			continue;
		}

		// EPOLLIN, etc...
		EVOBJTYPE	type = EVOBJ_TYPE_EVMQ;		// tmp
		if(!GetEventBaseObjType(pEvObj, type)){
			ERR_CHMPRN("Why? something wrong with event type.");
			return false;
		}

		// Receive
		if(EVOBJ_TYPE_EVMQ == type){
			ChmEventMq*	pEvMqObj = reinterpret_cast<ChmEventMq*>(pEvObj);

			// get composed msgid
			COMPOSEDMSGID	composed;
			if(!pEvMqObj->ReceiveComposedMsgid(readevent.data.fd, composed)){
				ERR_CHMPRN("Failed to read composed msgid from MQ(%d).", readevent.data.fd);
				return false;
			}
			// read only head
			if(!pEvMqObj->ReceiveHead(readevent.data.fd, composed, ppComPkt)){
				ERR_CHMPRN("Failed to receive head data from msgid(0x%016" PRIx64 ") by MQ(%d).", composed.msgid, readevent.data.fd);
				return false;
			}

			// check COM_C2C packet
			if(pEvMqObj->CheckProcessing(*ppComPkt)){
				// Processing COM_PX2C/COM_C2PX/etc packet, so wait next packet.
				CHM_Free(*ppComPkt);
				continue;

			}else{
				// COM_C2C : read body
				if(ppbody){
					if(!pEvMqObj->ReceiveBody(*ppComPkt, ppbody, *plength, true)){			// remove k2hash
						ERR_CHMPRN("Failed to receive data from event fd(%d).", readevent.data.fd);
						CHM_Free(*ppComPkt);
						return false;
					}
				}else{
					PCOMPKT	pTmp = pEventMq->ReceiveBody(*ppComPkt, true);
					if(!pTmp){
						ERR_CHMPRN("Something error occured reading body from event fd(%d).", readevent.data.fd);
						CHM_Free(*ppComPkt);
						return false;
					}
					*ppComPkt = pTmp;
				}
				break;
			}

		}else{
			// why unsupport read event object type
			WAN_CHMPRN("Got a event is not MQ event.");

			if(!pEvObj->Receive(readevent.data.fd)){
				ERR_CHMPRN("Failed to receive data from event fd(%d).", readevent.data.fd);
				return false;
			}
			break;
		}
	}while(ChmCntrl::DoLoop);

	return true;
}

//
// Receive COMPKT for client joining server/slave chmpx.
//
msgid_t ChmCntrl::Open(bool no_giveup_rejoin)
{
	if(CHMCHNTL_TYPE_CLIENT_ONSLAVE != chmcntrltype){
		ERR_CHMPRN("This object is not initialized for client.");
		return CHM_INVALID_MSGID;
	}
	if(!IsInitialized()){
		if(!ReInitialize(no_giveup_rejoin ? ChmCntrl::DEFAULT_EVENT_TIMEOUT : 0L, no_giveup_rejoin ? 0 : 1)){
			ERR_CHMPRN("This object is not initialized yet.");
			return CHM_INVALID_MSGID;
		}
	}

	// activating
	msgid_t	msgid;
	if(CHM_INVALID_MSGID == (msgid = pEventMq->ActivatedMsgId())){
		ERR_CHMPRN("Failed to activating MQ.");
	}

	return msgid;
}

//
// Receive COMPKT for client joining server/slave chmpx.
//
bool ChmCntrl::Close(msgid_t msgid)
{
	if(CHM_INVALID_MSGID == msgid){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}
	if(CHMCHNTL_TYPE_CLIENT_ONSLAVE != chmcntrltype){
		ERR_CHMPRN("This object is not initialized for client.");
		return false;
	}
	if(!IsInitialized()){
		ERR_CHMPRN("This object is not initialized yet.");
		return false;
	}

	// disactivating
	if(!pEventMq->DisactivatedMsgId(msgid)){
		ERR_CHMPRN("Failed to disactivating MQ msgid(0x%016" PRIx64 ").", msgid);
		return false;
	}
	return true;
}

//
// Receive COMPKT for client joining slave chmpx.
//
// [NOTICE]
// This method access to MQ fd directly, thus not use event queue.
// This class on "client on library" mode sets MQ fd into event queue,
// but not use it.(be careful)
//
// [Return value]
// true:	succeed to read / could not read event.
// false:	something error occured
//
// * If returned true, must check *ppComPkt value which is not NULL for succeed.
//   *ppComPkt is NULL, it means timeout.
//
// [Usage]
// If you get COMPKT and body data for each, specify ppbody and plength argument.
// If you get COMPKT for all of head and body, specify these are NULL.
//
bool ChmCntrl::Receive(msgid_t msgid, PCOMPKT* ppComPkt, unsigned char** ppbody, size_t* plength, int timeout_ms)
{
	if(CHM_INVALID_MSGID == msgid || !ppComPkt || (!ppbody && plength) || (ppbody && !plength)){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	if(CHMCHNTL_TYPE_CLIENT_ONSLAVE != chmcntrltype){
		ERR_CHMPRN("This object is not initialized for client.");
		return false;
	}
	if(!IsInitialized()){
		ERR_CHMPRN("This object is not initialized yet.");
		return false;
	}

	// initialize values
	*ppComPkt	= NULL;
	if(ppbody){
		*ppbody	= NULL;
		*plength= 0L;
	}

	// read loop
	bool			CanContinueWait;
	struct timespec	sleeptime	= {0, 10 * 1000};									// 10us
	int				retrycnt	= timeout_ms <= 0 ? 1 : (timeout_ms * 1000 / 10);	// retrycnt * 10us = timeout_ms;
	for(int cnt = 0; cnt < retrycnt && ChmCntrl::DoLoop; ){
		// Read Header
		// if not readable data, retry after sleep 100us.
		CanContinueWait = false;
		if(!pEventMq->ReceiveHeadByMsgId(msgid, ppComPkt, &CanContinueWait) || NULL == (*ppComPkt)){
			// check chmpx process down
			if(ImData.IsNeedDetach()){
				ERR_CHMPRN("Found notification, Chmpx process down.");
				// Detach
				Clean(false);
				return false;
			}
			if(CanContinueWait){
				if(cnt + 1 < retrycnt){
					nanosleep(&sleeptime, NULL);
				}
				cnt++;
				continue;
			}
			ERR_CHMPRN("Something error occured reading head from MQ msgid(0x%016" PRIx64 ").", msgid);
			return false;
		}

		// check COM_C2C packet
		if(!pEventMq->CheckProcessing(*ppComPkt)){
			// Read after
			if(!ppbody){
				PCOMPKT	pTmp = pEventMq->ReceiveBody(*ppComPkt, true);
				if(!pTmp){
					ERR_CHMPRN("Something error occured reading body from MQ msgid(0x%016" PRIx64 ").", msgid);;
					CHM_Free(*ppComPkt);
					return false;
				}
				*ppComPkt = pTmp;
			}else{
				if(!pEventMq->ReceiveBody(*ppComPkt, ppbody, (*plength), true)){
					ERR_CHMPRN("Something error occured reading body from MQ msgid(0x%016" PRIx64 ").", msgid);
					CHM_Free(*ppComPkt);
					return false;
				}
			}
			// Succeed receive C2C packet.
			break;
		}

		// Processing COM_PX2C/COM_C2PX/etc packet, so wait next packet.
		// and cnt is not increment.
		CHM_Free(*ppComPkt);
	}

	// means timeout or break loop.
	return true;
}

long ChmCntrl::PlaningReceiverCount(chmhash_t hash, c2ctype_t c2ctype)
{
	if(!IsInitialized()){
		ERR_CHMPRN("This object is not initialized yet.");
		return -1L;
	}
	// get target chmpxid list
	chmpxidlist_t	chmpxids;
	long			chmpxcnt = ImData.GetReceiverChmpxids(hash, c2ctype, chmpxids);
	if(0 > chmpxcnt){
		ERR_CHMPRN("Failed to get target chmpxids by something error occurred.");
	}else if(0 == chmpxcnt){
		WAN_CHMPRN("There are no target chmpxids, but method returns with success.");
	}
	return chmpxcnt;
}

//
// Get all assigned chmpx's chmhash list for manual replication.
//
bool ChmCntrl::GetAllReplicateChmHashs(chmhash_t hash, chmhashlist_t& basehashs, bool with_pending, bool without_down, bool without_suspend)
{
	if(!IsInitialized()){
		ERR_CHMPRN("This object is not initialized yet.");
		return false;
	}
	return ImData.GetServerChmHashsByHashs(hash, basehashs, with_pending, without_down, without_suspend);
}

//
// Send COMPKT for client joining slave chmpx.
//
bool ChmCntrl::RawSendOnSlave(msgid_t msgid, const unsigned char* pbody, size_t blength, chmhash_t hash, long* preceivercnt, c2ctype_t c2ctype)
{
	if(!IsClientOnSlvType()){
		ERR_CHMPRN("This is not client joining slave chmpx process.");
		return false;
	}
	if(preceivercnt){
		if(0 > ((*preceivercnt) = PlaningReceiverCount(hash, c2ctype))){
			ERR_CHMPRN("Could not plan receiver chmpx server count.");
			return false;
		}
	}
	return RawSend(msgid, NULL, pbody, blength, hash, c2ctype);
}

//
// Send COMPKT for client joining server chmpx proc.
//
bool ChmCntrl::RawSendOnServer(const unsigned char* pbody, size_t blength, chmhash_t hash, c2ctype_t c2ctype, bool without_self)
{
	if(!IsClientOnSvrType()){
		ERR_CHMPRN("This is not client joining server chmpx process.");
		return false;
	}
	// Supported C2C from server to server now.
	// (after 1.0.19)
	if(IS_C2CTYPE_IGNORE(c2ctype)){
		ERR_CHMPRN("COM_C2C type(%s) is invalid.", STRCOMC2CTYPE(c2ctype));
		return false;
	}
	if(without_self){
		SET_C2CTYPE_NOT_SELF(c2ctype);
	}else{
		SET_C2CTYPE_SELF(c2ctype);
	}
	return RawSend(CHM_INVALID_MSGID, NULL, pbody, blength, hash, c2ctype);
}

//
// Reply COMPKT for all client.
//
bool ChmCntrl::Send(PCOMPKT pComPkt, const unsigned char* pbody, size_t blength)
{
	if(IsChmpxType()){
		ERR_CHMPRN("This is chmpx process.");
		return false;
	}
	return RawSend(CHM_INVALID_MSGID, pComPkt, pbody, blength, 0L, COM_C2C_NORMAL);
}

//
// RawSend COMPKT
//
bool ChmCntrl::RawSend(msgid_t msgid, PCOMPKT pComPkt, const unsigned char* pbody, size_t blength, chmhash_t hash, c2ctype_t c2ctype)
{
	if(!pbody || 0L == blength){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}
	if(CHMCHNTL_TYPE_CHMPXPROC == chmcntrltype){
		ERR_CHMPRN("This object is not initialized for chmpx.");
		return false;
	}
	if(IS_C2CTYPE_IGNORE(c2ctype)){
		ERR_CHMPRN("COM_C2C type(%s) is invalid.", STRCOMC2CTYPE(c2ctype));
		return false;
	}
	if(!IsInitialized()){
		ERR_CHMPRN("This object is not initialized yet.");
		return false;
	}

	// make head
	COMPKT	NewComPkt;
	if(!pComPkt){
		// new message
		//
		if(!pEventMq->BuildC2CSendHead(&(NewComPkt.head), CHM_INVALID_CHMPXID, hash, c2ctype, msgid)){
			ERR_CHMPRN("Could not make COMHEAD for sending new message through MQ.");
			return false;
		}
	}else{
		// response message
		if(!pEventMq->BuildC2CResponseHead(&(NewComPkt.head), &(pComPkt->head))){
			ERR_CHMPRN("Could not make COMHEAD for responding message through MQ.");
			return false;
		}
	}
	NewComPkt.length = sizeof(COMPKT);
	NewComPkt.offset = sizeof(COMPKT);

	if(!pEventMq->Send(&NewComPkt, pbody, blength)){
		// check chmpx process down
		if(ImData.IsNeedDetach()){
			MSG_CHMPRN("Found notification, Chmpx process down.");
			// Detach
			Clean(false);
		}
		return false;
	}
	return true;
}

bool ChmCntrl::IsChmpxExit(void)
{
	if(IsInitialized()){
		return false;
	}
	if(bup_cfg.empty()){
		return false;
	}
	return true;
}

bool ChmCntrl::MergeGetLastTime(struct timespec& lastts)
{
	if(!IsInitialized()){
		ERR_CHMPRN("This object is not initialized yet.");
		return CHM_INVALID_PID;
	}
	if(CHMCHNTL_TYPE_CHMPXPROC != chmcntrltype){
		ERR_CHMPRN("This object is not initialized for chmpx process.");
		return false;
	}
	return pEventMq->PxCltSendMergeGetLastTime(lastts);
}

bool ChmCntrl::MergeRequestUpdateData(const PPXCOMMON_MERGE_PARAM pmerge_param)
{
	if(!IsInitialized()){
		ERR_CHMPRN("This object is not initialized yet.");
		return CHM_INVALID_PID;
	}
	if(CHMCHNTL_TYPE_CHMPXPROC != chmcntrltype){
		ERR_CHMPRN("This object is not initialized for chmpx process.");
		return false;
	}
	return pEventMq->PxCltSendRequestUpdateData(pmerge_param);
}

bool ChmCntrl::MergeResponseUpdateData(chmpxid_t chmpxid, size_t length, const unsigned char* pdata, const struct timespec* pts)
{
	if(!IsInitialized()){
		ERR_CHMPRN("This object is not initialized yet.");
		return CHM_INVALID_PID;
	}
	if(CHMCHNTL_TYPE_CHMPXPROC != chmcntrltype){
		ERR_CHMPRN("This object is not initialized for chmpx process.");
		return false;
	}
	return pEventSock->PxComSendResUpdateData(chmpxid, length, pdata, pts);
}

bool ChmCntrl::MergeSetUpdateData(size_t length, const unsigned char* pdata, const struct timespec* pts)
{
	if(!IsInitialized()){
		ERR_CHMPRN("This object is not initialized yet.");
		return CHM_INVALID_PID;
	}
	if(CHMCHNTL_TYPE_CHMPXPROC != chmcntrltype){
		ERR_CHMPRN("This object is not initialized for chmpx process.");
		return false;
	}
	return pEventMq->PxCltSendSetUpdateData(length, pdata, pts);
}

bool ChmCntrl::MergeResultUpdateData(chmpxid_t chmpxid, reqidmapflag_t result_updatedata)
{
	if(!IsInitialized()){
		ERR_CHMPRN("This object is not initialized yet.");
		return CHM_INVALID_PID;
	}
	if(CHMCHNTL_TYPE_CHMPXPROC != chmcntrltype){
		ERR_CHMPRN("This object is not initialized for chmpx process.");
		return false;
	}
	return pEventSock->PxComSendResultUpdateData(chmpxid, result_updatedata);
}

bool ChmCntrl::MergeAbortUpdateData(void)
{
	if(!IsInitialized()){
		ERR_CHMPRN("This object is not initialized yet.");
		return CHM_INVALID_PID;
	}
	if(CHMCHNTL_TYPE_CHMPXPROC != chmcntrltype){
		ERR_CHMPRN("This object is not initialized for chmpx process.");
		return false;
	}
	return pEventMq->PxCltSendAbortUpdateData();
}

bool ChmCntrl::DumpSelfChmpxSvr(stringstream& sstream) const
{
	if(!IsInitialized()){
		ERR_CHMPRN("This object is not initialized yet.");
		return false;
	}
	if(!ImData.DumpSelfChmpxSvr(sstream)){
		ERR_CHMPRN("Could not dump self chmpx information.");
		return false;
	}
	return true;
}

PCHMINFOEX ChmCntrl::DupAllChmInfo(void)
{
	if(!ImData.IsInitialized()){
		ERR_CHMPRN("This object is not initialized yet.");
		return NULL;
	}
	return ImData.DupAllChmInfo();
}

PCHMPX ChmCntrl::DupSelfChmpxInfo(void)
{
	if(!ImData.IsInitialized()){
		ERR_CHMPRN("This object is not initialized yet.");
		return NULL;
	}
	return ImData.DupSelfChmpxInfo();
}

void ChmCntrl::FreeDupAllChmInfo(PCHMINFOEX ptr)
{
	ChmIMData::FreeDupAllChmInfo(ptr);
}

void ChmCntrl::FreeDupSelfChmpxInfo(PCHMPX ptr)
{
	ChmIMData::FreeDupSelfChmpxInfo(ptr);
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
