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
#ifndef	CHMIMDATA_H
#define	CHMIMDATA_H

#include <k2hash/k2hash.h>
#include <k2hash/k2hshm.h>
#include <fullock/rwlockrcsv.h>

#include "chmstructure.tcc"

//---------------------------------------------------------
// Prototype
//---------------------------------------------------------
class CHMConf;

//---------------------------------------------------------
// Structures
//---------------------------------------------------------
typedef struct chminfo_ex{
	PCHMINFO	pchminfo;
	char		shmpath[CHM_MAX_PATH_LEN];
	size_t		shmsize;
	char		k2hashpath[CHM_MAX_PATH_LEN];
}CHMINFOEX, *PCHMINFOEX;

//---------------------------------------------------------
// Class ChmIMData
//---------------------------------------------------------
// Chm Internal Management Data Class
//
class ChmIMData
{
	protected:
		typedef enum mkfpath_mode{
			MKFILEPATH_SHM = 0,
			MKFILEPATH_K2H
		}MKFPMODE;

	protected:
		static const int	SYSPAGE_SIZE = 4096;

		std::string			ShmPath;		// ChmShm file path
		PCHMSHM				pChmShm;		// SHM mmap base
		int					ShmFd;			// SHM fd
		size_t				ShmSize;		// SHM size
		CHMConf*			pConfObj;		// CHMConf file object
		K2HShm*				pK2hash;		// K2HASH object
		int					eqfd;			// backup
		bool				isChmpxProc;	// CHMPX process or Client library
		pid_t				ChmpxPid;		// backup chmpx pid for checking process down on slave
		hnamesslmap_t		hnamesslmap;	// Cache for hostname:ctlport mapping(using for checking existed host:port).

	protected:
		static bool MakeFilePath(const char* groupname, short port, MKFPMODE mode, std::string& shmpath);
		static bool MakeShmFilePath(const char* groupname, short port, std::string& shmpath);
		static bool MakeK2hashFilePath(const char* groupname, short port, std::string& shmpath);
		static bool CompareChmpxSvrs(PCHMPXSVR pbase, long bcount, PCHMPXSVR pmerge, long mcount, bool is_status = true);

		off_t GetLockOffsetForMQ(void) const;
		bool CloseShm(void);
		bool InitializeShm(void);
		bool InitializeShmEx(const CHMCFGINFO* pchmcfg, const CHMNODE_CFGINFO* pself);
		bool IsAttachedShm(void) const { return (NULL != pChmShm); }
		bool AttachShm(void);													// For client process library
		bool InitializeOther(void);
		bool CloseK2hash(void);
		bool InitializeK2hash(void);
		bool IsAttachedK2hash(void) const { return (NULL != pK2hash); }
		bool AttachK2hash(void);												// For client process library

		msgid_t AssignMsg(bool is_chmpx, bool is_activated);					// Assign MQ
		bool ActivateMsgEx(msgid_t msgid, bool is_activate);					// set msgid activated status
		msgid_t GetRandomMsgId(bool is_chmpx);

		// Process running
		bool LockChmpxPid(void);
		bool UnlockChmpxPid(void);
		bool IsChmpxProcessRunning(pid_t& pid) const;

		// Client pid list
		bool RawLockClientPidList(FLRwlRcsv& lockobj, bool is_read) const;
		bool ReadLockClientPidList(FLRwlRcsv& lockobj) const { return RawLockClientPidList(lockobj, true); }
		bool WriteLockClientPidList(FLRwlRcsv& lockobj) const { return RawLockClientPidList(lockobj, false); }

		// Do not call these method, must call SetSlaveAll
		bool SetSlaveBase(chmpxid_t chmpxid, const char* hostname, short ctlport, const PCHMPXSSL pssl);
		bool SetSlaveSock(chmpxid_t chmpxid, int sock);
		bool SetSlaveStatus(chmpxid_t chmpxid, chmpxsts_t status);

		bool RemoveSlave(chmpxid_t chmpxid);

	public:
		static void FreeDupAllChmInfo(PCHMINFOEX ptr);								// for free memory allocated by DupAllChmInfo()
		static void FreeDupSelfChmpxInfo(PCHMPX ptr);								// for free memory allocated by DupSelfChmpxInfo()
		static bool IsSafeCHMINFO(const PCHMINFO pchminfo);

		ChmIMData(bool is_chmpx_proc = true);
		virtual ~ChmIMData();

		bool Close(void);
		bool CloseSocks(int type = CLOSETG_BOTH);
		bool IsInitialized(void) const { return (pChmShm && CHM_INVALID_HANDLE != ShmFd && pConfObj && pK2hash); }
		bool Initialize(CHMConf* cfgobj, int eventqfd, bool is_chmpx_proc);
		bool ReloadConfiguration(void);												// now reload without server/slave list
		const CHMSHM* GetShmBase(void) const { return pChmShm; }
		const char* GetShmPath(void) const { return ShmPath.c_str(); }
		K2HShm* GetK2hashObj(void) { return pK2hash; }

		bool Dump(std::stringstream& sstream) const;
		bool DumpSelfChmpxSvr(std::stringstream& sstream) const;
		PCHMINFOEX DupAllChmInfo(void);
		PCHMPX DupSelfChmpxInfo(void);

		// MQ
		msgid_t GetBaseMsgId(void) const;
		bool FreeMsg(msgid_t msgid);												// Free msgid
		bool IsMsgidActivated(msgid_t msgid) const;
		bool FreeMsgs(const pidlist_t& pidlist, msgidlist_t& freedmsgids);			// Free msgids by pid
		msgid_t AssignMsgOnChmpx(void) { return AssignMsg(true, true); }
		msgid_t AssignMsgOnServer(void) { return AssignMsg(false, true); }
		msgid_t AssignMsgOnSlave(bool is_activated = true) { return AssignMsg(false, is_activated); }
		bool ActivateMsg(msgid_t msgid) { return ActivateMsgEx(msgid, true); }		// Activate Msgid
		bool DisactivateMsg(msgid_t msgid) { return ActivateMsgEx(msgid, false); }	// Disactivate Msgid
		msgid_t GetRandomChmpxMsgId(void) { return GetRandomMsgId(true); }			// get msgid from assigned list for chmpx
		msgid_t GetRandomClientMsgId(void) { return GetRandomMsgId(false); }		// get msgid from assigned list for not chmpx
		bool GetMsgidListByPid(pid_t pid, msgidlist_t& list);						// get all msgid list by pid
		bool GetMsgidListByUniqPid(msgidlist_t& list);								// get msgid list for all uniq pid

		// CHMSHM
		bool GetSelfChmpxSvr(PCHMPXSVR chmpxsvr) const;
		bool GetChmpxSvr(chmpxid_t chmpxid, PCHMPXSVR chmpxsvr) const;
		bool GetChmpxSvrs(PCHMPXSVR* ppchmpxsvrs, long& count) const;
		bool CompareChmpxSvrs(PCHMPXSVR pchmpxsvrs, long count);
		bool MergeChmpxSvrs(PCHMPXSVR pchmpxsvrs, long count, bool is_remove = true, bool is_init_process = false, int eqfd = CHM_INVALID_HANDLE);
		bool MergeChmpxSvrsForStatusUpdate(PCHMPXSVR pchmpxsvrs, long count, int eqfd);

		bool GetGroup(std::string& group) const;
		bool IsRandomDeliver(void) const;
		bool IsAutoMerge(void) const;
		bool IsDoMerge(void) const;
		bool SuspendAutoMerge(void);
		bool ResetAutoMerge(void);
		bool GetAutoMergeMode(void) const;
		chmss_ver_t GetSslMinVersion(void) const;
		const char* GetNssdbDir(void) const;
		int GetSocketThreadCount(void) const;
		int GetMQThreadCount(void) const;
		int GetMaxSockPool(void) const;
		time_t GetSockPoolTimeout(void) const;
		long GetMaxMQCount(void) const;
		long GetChmpxMQCount(void) const;
		long GetMaxQueuePerChmpxMQ(void) const;
		long GetMaxQueuePerClientMQ(void) const;
		long GetMQPerClient(void) const;
		long GetMQPerAttach(void) const;
		bool IsAckMQ(void) const;
		int GetSockRetryCnt(void) const;
		suseconds_t GetSockTimeout(void) const;
		suseconds_t GetConnectTimeout(void) const;
		int GetMQRetryCnt(void) const;
		long GetMQTimeout(void) const;
		time_t GetMergeTimeout(void) const;
		bool IsServerMode(void) const;
		bool IsSlaveMode(void) const;
		long GetReplicaCount(void) const;
		long GetMaxHistoryLogCount(void) const;
		bool IsChmpxProcess(void) const { return isChmpxProc; }
		chmpxid_t GetSelfChmpxId(void) const;

		long GetServerCount(void) const;
		long GetSlaveCount(void) const;
		long GetUpServerCount(void) const;
		chmpxpos_t GetSelfServerPos(void) const;
		chmpxpos_t GetNextServerPos(chmpxpos_t startpos, chmpxpos_t nowpos, bool is_skip_self, bool is_cycle) const;
		bool GetNextServerBase(std::string& name, chmpxid_t& chmpxid, short& port, short& ctlport) const;
		chmpxid_t GetNextServerChmpxId(void) const;
		chmpxid_t GetNextRingChmpxId(chmpxid_t chmpxid = CHM_INVALID_CHMPXID) const;

		bool IsOperating(void);
		bool IsServerChmpxId(chmpxid_t chmpxid) const;
		chmpxid_t GetChmpxIdBySock(int sock, int type = CLOSETG_BOTH) const;
		chmpxid_t GetChmpxIdByToServerSock(int sock) const { return GetChmpxIdBySock(sock, CLOSETG_SERVERS); }
		chmpxid_t GetChmpxIdByFromSlaveSock(int sock) const { return GetChmpxIdBySock(sock, CLOSETG_SLAVES); }
		chmpxid_t GetChmpxIdByToServerName(const char* hostname, short ctlport) const;
		chmpxid_t GetChmpxIdByStatus(chmpxsts_t status, bool part_match = false) const;
		chmpxid_t GetRandomServerChmpxId(bool without_suspend = false);
		chmpxid_t GetServerChmpxIdByHash(chmhash_t hash) const;
		bool GetServerChmHashsByHashs(chmhash_t hash, chmhashlist_t& basehashs, bool with_pending = true, bool without_down = true, bool without_suspend = true);
		bool GetServerChmpxIdByHashs(chmhash_t hash, chmpxidlist_t& chmpxids, bool with_pending = true, bool without_down = true, bool without_suspend = true);
		long GetServerChmpxIdByBaseHash(chmhash_t basehash, chmpxidlist_t& chmpxids) const;
		long GetServerChmpxIdForMerge(chmpxidlist_t& list) const;
		long GetServerChmpxIds(chmpxidlist_t& list) const;
		bool GetServerBase(long pos, std::string& name, chmpxid_t& chmpxid, short& port, short& ctlport) const;
		bool GetServerBase(chmpxid_t chmpxid, std::string& name, short& port, short& ctlport) const;
		bool GetServerBase(chmpxid_t chmpxid, CHMPXSSL& ssl) const;
		bool GetServerSocks(chmpxid_t chmpxid, socklist_t& socklist, int& ctlsock) const;
		bool GetServerSock(chmpxid_t chmpxid, socklist_t& socklist) const { int tmp; return GetServerSocks(chmpxid, socklist, tmp); }
		bool IsConnectServer(chmpxid_t chmpxid) const { socklist_t tmpsocklist; return (GetServerSock(chmpxid, tmpsocklist) && 0 < tmpsocklist.size()); }
		bool GetServerCtlSock(chmpxid_t chmpxid, int& ctlsock) const { socklist_t tmplist; return GetServerSocks(chmpxid, tmplist, ctlsock); }
		bool GetServerHash(chmpxid_t chmpxid, chmhash_t& base, chmhash_t& pending) const;
		bool GetServerBaseHash(chmpxid_t chmpxid, chmhash_t& hash) const { chmhash_t tmp; return GetServerHash(chmpxid, hash, tmp); }
		bool GetServerPendingHash(chmpxid_t chmpxid, chmhash_t& hash) const { chmhash_t tmp; return GetServerHash(chmpxid, tmp, hash); }
		bool GetMaxHashCount(chmhash_t& basehash, chmhash_t& pendinghash) const;
		bool GetMaxBaseHashCount(chmhash_t& basehash) const { chmhash_t tmp; return GetMaxHashCount(basehash, tmp); }
		bool GetMaxPendingHashCount(chmhash_t& pendinghash) const { chmhash_t tmp; return GetMaxHashCount(tmp, pendinghash); }
		bool IsPendingExchangeData(void) const;
		long GetReceiverChmpxids(chmhash_t hash, c2ctype_t c2ctype, chmpxidlist_t& chmpxids);

		chmpxsts_t GetServerStatus(chmpxid_t chmpxid) const;
		bool GetSelfPorts(short& port, short& ctlport) const;
		bool GetSelfSocks(int& sock, int& ctlsock) const;
		bool GetSelfHash(chmhash_t& base, chmhash_t& pending) const;
		bool GetSelfBaseHash(chmhash_t& hash) const { chmhash_t tmp; return GetSelfHash(hash, tmp); }
		bool GetSelfPendingHash(chmhash_t& hash) const { chmhash_t tmp; return GetSelfHash(tmp, hash); }
		chmpxsts_t GetSelfStatus(void) const;
		bool GetSelfSsl(CHMPXSSL& ssl) const;
		long GetSlaveChmpxIds(chmpxidlist_t& list) const;
		bool GetSlaveBase(chmpxid_t chmpxid, std::string& name, short& ctlport) const;
		bool GetSlaveSock(chmpxid_t chmpxid, socklist_t& socklist) const;
		bool IsConnectSlave(chmpxid_t chmpxid) const { socklist_t tmplist; return (GetSlaveSock(chmpxid, tmplist) && 0 < tmplist.size()); }
		chmpxsts_t GetSlaveStatus(chmpxid_t chmpxid) const;

		bool SetServerSocks(chmpxid_t chmpxid, int sock, int ctlsock, int type);
		bool SetServerHash(chmpxid_t chmpxid, chmhash_t base, chmhash_t pending, int type);
		bool SetServerBaseHash(chmpxid_t chmpxid, chmhash_t hash) { return SetServerHash(chmpxid, hash, 0L, HASHTG_BASE); }
		bool SetServerPendingHash(chmpxid_t chmpxid, chmhash_t hash) { return SetServerHash(chmpxid, 0L, hash, HASHTG_PENDING); }
		bool SetServerStatus(chmpxid_t chmpxid, chmpxsts_t status);
		bool UpdateSelfLastStatusTime(void) { return UpdateLastStatusTime(CHM_INVALID_CHMPXID); }
		bool UpdateLastStatusTime(chmpxid_t chmpxid);
		bool RemoveServerSock(chmpxid_t chmpxid, int sock);
		bool UpdateHash(int type, bool is_allow_operating = true);
		bool SetSelfSocks(int sock, int ctlsock);
		bool SetSelfHash(chmhash_t base, chmhash_t pending, int type);
		bool SetSelfBaseHash(chmhash_t hash) { return SetSelfHash(hash, 0L, HASHTG_BASE); }
		bool SetSelfPendingHash(chmhash_t hash) { return SetSelfHash(0L, hash, HASHTG_PENDING); }
		bool SetSelfStatus(chmpxsts_t status);
		bool SetSlaveAll(chmpxid_t chmpxid, const char* hostname, short ctlport, const PCHMPXSSL pssl, int sock, chmpxsts_t status);
		bool RemoveSlaveSock(chmpxid_t chmpxid, int sock);
		bool CheckSockInAllChmpx(int sock) const;

		// Stats
		bool AddStat(chmpxid_t chmpxid, bool is_sent, size_t bodylength, const struct timespec& elapsed_time);
		bool GetStat(PCHMSTAT pserver, PCHMSTAT pslave) const;

		// Trace(History)
		long GetTraceCount(void) const;
		bool IsTraceEnable(void) const;
		bool EnableTrace(void);
		bool DisableTrace(void);
		bool AddTrace(logtype_t logtype, size_t length, const struct timespec& start, const struct timespec& fin);
		bool GetTrace(PCHMLOGRAW plograwarr, long& arrsize, logtype_t dirmask, logtype_t devmask) const;

		// Process running
		bool RetrieveClientPid(pid_t pid);
		bool AddClientPid(pid_t pid);
		bool GetAllPids(pidlist_t& list);
		bool IsClientPids(void) const;
		bool IsNeedDetach(void) const;

		// Others
		bool IsAllowHostname(const char* hostname, const short* pport = NULL, PCHMPXSSL* ppssl = NULL);
};

#endif	// CHMIMDATA_H

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
