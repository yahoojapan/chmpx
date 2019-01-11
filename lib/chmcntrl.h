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
 * hashing and are automatically layouted. As a result, it
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
#ifndef	CHMCNTRL_H
#define	CHMCNTRL_H

#include <signal.h>
#include <map>

#include "chmconf.h"
#include "chmimdata.h"
#include "chmeventmq.h"
#include "chmeventsock.h"
#include "chmeventshm.h"

//---------------------------------------------------------
// ChmCntrl Class
//---------------------------------------------------------
class ChmCntrl
{
	friend class ChmEventBase;
	friend class CHMConf;
	friend class ChmEventMq;
	friend class ChmEventSock;
	friend class ChmEventShm;

	protected:
		typedef enum _chmcntrl_type{
			CHMCHNTL_TYPE_CHMPXPROC,
			CHMCHNTL_TYPE_CLIENT_ONSERVER,
			CHMCHNTL_TYPE_CLIENT_ONSLAVE
		}CHMCNTRLTYPE;

		typedef enum _evobj_type{
			EVOBJ_TYPE_CONF,
			EVOBJ_TYPE_EVSOCK,
			EVOBJ_TYPE_EVMQ,
			EVOBJ_TYPE_EVSHM
		}EVOBJTYPE;

		typedef std::map<int, ChmEventBase*>	evobj_map_t;	// Key is number which is kind of event object.

		static const int		DEFAULT_MAX_EVENT_CNT	= 32;	// receive max event at once for CHMCHNTL_TYPE_CHMPXPROC
		static const int		DEFAULT_EVENT_TIMEOUT	= 100;	// timeout for epoll wait(100ms) for CHMCHNTL_TYPE_CHMPXPROC
		static const int		EVENT_NOWAIT 			= 0;	// no wait for epoll
		static volatile bool	DoLoop;							// Break loop flag

		CHMCNTRLTYPE			chmcntrltype;					// process type
		int						eqfd;							// event queue fd
		ChmIMData				ImData;							// 
		CHMConf*				pConfObj;						// 
		ChmEventMq*				pEventMq;						// 
		ChmEventSock*			pEventSock;						// [NOTICE] chmpx only has this value.
		ChmEventShm*			pEventShm;						// [NOTICE] chmpx only has this value.
		bool					auto_rejoin;					// retry to join if chmpx process is down.
		std::string				bup_cfg;						// backup for configuration file path or json string.
		evobj_map_t				evobjmap;						// Object mapping
		volatile bool			is_close_notify;				// client closing notify flag

	protected:
		ChmEventBase* GetEventBaseObj(EVOBJTYPE type);
		ChmEventBase* FindEventBaseObj(int fd);
		bool GetEventBaseObjType(const ChmEventBase* pEvObj, EVOBJTYPE& type);

		ChmIMData* GetImDataObj(void) { return (ImData.IsInitialized() ? &ImData : NULL); }
		CHMConf* GetConfObj(void) { return pConfObj; }
		ChmEventMq* GetEventMqObj(void) { return pEventMq; }
		ChmEventSock* GetEventSockObj(void) { return pEventSock; }
		ChmEventShm* GetEventShmObj(void) { return pEventShm; }

		bool Initialize(const char* cfgfile, CHMCNTRLTYPE type, bool is_auto_rejoin = false, chm_merge_get_cb getfp = NULL, chm_merge_set_cb setfp = NULL, chm_merge_lastts_cb lastupdatefp = NULL, short ctlport = CHM_INVALID_PORT);
		bool ReInitialize(long waitms = 0L, int trycnt = 1);
		bool IsInitialized(void) const;
		bool InitializeEventFd(void);
		bool ConfigurationUpdateNotify(void);

		bool Processing(PCOMPKT pComPkt, EVOBJTYPE call_ev_type);

		long PlaningReceiverCount(chmhash_t hash, c2ctype_t c2ctype);
		bool RawSendOnSlave(msgid_t msgid, const unsigned char* pbody, size_t blength, chmhash_t hash, long* preceivercnt, c2ctype_t c2ctype);
		bool RawSendOnServer(const unsigned char* pbody, size_t blength, chmhash_t hash, c2ctype_t c2ctype, bool without_self);
		bool RawSend(msgid_t msgid, PCOMPKT pComPkt, const unsigned char* pbody, size_t blength, chmhash_t hash, c2ctype_t c2ctype);

		// For merging
		bool MergeGetLastTime(struct timespec& lastts);
		bool MergeRequestUpdateData(const PPXCOMMON_MERGE_PARAM pmerge_param);
		bool MergeResponseUpdateData(chmpxid_t chmpxid, size_t length, const unsigned char* pdata, const struct timespec* pts);
		bool MergeSetUpdateData(size_t length, const unsigned char* pdata, const struct timespec* pts);
		bool MergeResultUpdateData(chmpxid_t chmpxid, reqidmapflag_t result_updatedata);
		bool MergeAbortUpdateData(void);

	public:
		static void LoopBreakHandler(int signum);
		static void FreeDupAllChmInfo(PCHMINFOEX ptr);					// for free memory allocated by DupAllChmInfo()
		static void FreeDupSelfChmpxInfo(PCHMPX ptr);					// for free memory allocated by DupSelfChmpxInfo()

		ChmCntrl(void);
		virtual ~ChmCntrl();

		bool Clean(bool is_clean_bup = true);
																												// For chmpx process
		bool InitializeOnChmpx(const char* cfgfile, short ctlport = CHM_INVALID_PORT) { return Initialize(cfgfile, CHMCHNTL_TYPE_CHMPXPROC, false, NULL, NULL, NULL, ctlport); }
																												// For server process on library
		bool InitializeOnServer(const char* cfgfile, bool is_auto_rejoin = false, chm_merge_get_cb getfp = NULL, chm_merge_set_cb setfp = NULL, chm_merge_lastts_cb lastupdatefp = NULL, short ctlport = CHM_INVALID_PORT) { return Initialize(cfgfile, CHMCHNTL_TYPE_CLIENT_ONSERVER, is_auto_rejoin, getfp, setfp, lastupdatefp, ctlport); }
																												// For client process on library
		bool InitializeOnSlave(const char* cfgfile, bool is_auto_rejoin = false, short ctlport = CHM_INVALID_PORT) { return Initialize(cfgfile, CHMCHNTL_TYPE_CLIENT_ONSLAVE, is_auto_rejoin, NULL, NULL, NULL, ctlport); }
		bool OnlyAttachInitialize(const char* cfgfile, short ctlport = CHM_INVALID_PORT);

		void AllowSelfCert(void);			// For only debugging

		// where process running on
		bool IsChmpxType(void) const { return (CHMCHNTL_TYPE_CHMPXPROC == chmcntrltype); }
		bool IsClientOnSvrType(void) const { return (CHMCHNTL_TYPE_CLIENT_ONSERVER == chmcntrltype); }
		bool IsClientOnSlvType(void) const { return (CHMCHNTL_TYPE_CLIENT_ONSLAVE == chmcntrltype); }

		// Receive for chmpx process
		bool EventLoop(void);

		// Receive/Send for client on server chmpx process
		bool Receive(PCOMPKT* ppComPkt, unsigned char** ppbody = NULL, size_t* plength = NULL, int timeout_ms = ChmCntrl::EVENT_NOWAIT, bool no_giveup_rejoin = false);
		bool Send(const unsigned char* pbody, size_t blength, chmhash_t hash, bool is_routing = true, bool without_self = false) { return RawSendOnServer(pbody, blength, hash, is_routing ? COM_C2C_ROUTING : COM_C2C_NORMAL, without_self); }
		bool Broadcast(const unsigned char* pbody, size_t blength, chmhash_t hash, bool without_self = false) { return RawSendOnServer(pbody, blength, hash, COM_C2C_BROADCAST, without_self); }
		bool Replicate(const unsigned char* pbody, size_t blength, chmhash_t hash, bool without_self = true) { return RawSendOnServer(pbody, blength, hash, COM_C2C_RBROADCAST, without_self); }

		// Open/Close/Receive/Send for client on slave chmpx process
		msgid_t Open(bool no_giveup_rejoin = false);
		bool Close(msgid_t msgid);
		bool Receive(msgid_t msgid, PCOMPKT* ppComPkt, unsigned char** ppbody, size_t* plength, int timeout_ms = 0);	// 0 is no wait
		bool Send(msgid_t msgid, const unsigned char* pbody, size_t blength, chmhash_t hash, long* preceivercnt = NULL, bool is_routing = true) { return RawSendOnSlave(msgid, pbody, blength, hash, preceivercnt, is_routing ? COM_C2C_ROUTING : COM_C2C_NORMAL); }
		bool Broadcast(msgid_t msgid, const unsigned char* pbody, size_t blength, chmhash_t hash, long* preceivercnt = NULL) { return RawSendOnSlave(msgid, pbody, blength, hash, preceivercnt, COM_C2C_BROADCAST); }
		bool Replicate(msgid_t msgid, const unsigned char* pbody, size_t blength, chmhash_t hash, long* preceivercnt = NULL) { return RawSendOnSlave(msgid, pbody, blength, hash, preceivercnt, COM_C2C_RBROADCAST); }

		// Reply for all client
		bool Send(PCOMPKT pComPkt, const unsigned char* pbody, size_t blength);
		bool Reply(PCOMPKT pComPkt, const unsigned char* pbody, size_t blength) { return Send(pComPkt, pbody, blength); }

		// Check error type as Chmpx process exiting
		bool IsChmpxExit(void);

		// Utility for manual replication
		bool GetAllReplicateChmHashs(chmhash_t hash, chmhashlist_t& basehashs, bool with_pending = true, bool without_down = true, bool without_suspend = true);

		// Dump/Get internal information
		bool DumpSelfChmpxSvr(std::stringstream& sstream) const;
		PCHMINFOEX DupAllChmInfo(void);
		PCHMPX DupSelfChmpxInfo(void);
};

#endif	// CHMCNTRL_H

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
