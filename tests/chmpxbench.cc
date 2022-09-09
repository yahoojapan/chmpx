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
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <stdarg.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <libgen.h>
#include <string>
#include <iostream>
#include <map>
#include <vector>

#include "chmcommon.h"
#include "chmstructure.h"
#include "chmutil.h"
#include "chmconfutil.h"
#include "chmdbg.h"
#include "chmcntrl.h"
#include "chmopts.h"

using namespace std;

//---------------------------------------------------------
// Symbol & Macros
//---------------------------------------------------------
#define	MINIMUM_DATA_LENGTH			64
#define	FILL_CHARACTOR				'@'
#define	BENCH_TMP_FILE_FORM			"/tmp/chmpxbench-%d.dat"
#define	BENCH_CONF_LENGTH			(1024 * 64)			// 64K

//---------------------------------------------------------
// Structure
//---------------------------------------------------------
//
// All option
//
typedef struct bench_opts{
	bool			isServerMode;
	char			szConfig[BENCH_CONF_LENGTH];
	int				LoopCnt;
	int				proccnt;
	int				threadcnt;
	bool			isPrint;
	bool			isTrunAround;
	size_t			DataLength;
	bool			isBroadcast;
}BOPTS, *PBOPTS;

//
// For child process(thread)
//
typedef struct child_control{
	int				procid;				// process id for child
	int				threadid;			// thread id(gettid)
	pthread_t		pthreadid;			// pthread id(pthread_create)
	bool			is_ready;			// 
	bool			is_exit;			// 
	struct timespec	ts;					// result time
	int				errorcnt;			// error count
}CHLDCNTL, *PCHLDCNTL;

//
// For parent(main) process, common data
//
typedef struct exec_control{
	int				procid;				// = parent id
	BOPTS			opt;
	bool			is_suspend;
	bool			is_wait_doing;
	bool			is_exit;
}EXECCNTL, *PEXECCNTL;

//
// For thread function
//
typedef struct thread_param{
	ChmCntrl*		pCntlObj;
	PEXECCNTL		pexeccntl;
	PCHLDCNTL		pmycntl;
}THPARAM, *PTHPARAM;

//---------------------------------------------------------
// Utility Functions
//---------------------------------------------------------
static inline void PRN(const char* format, ...)
{
	if(format){
		va_list ap;
		va_start(ap, format);
		vfprintf(stdout, format, ap); 
		va_end(ap);
	}
	fprintf(stdout, "\n");
}

#define	PRNFLAG(flag, ...)		if(flag){ PRN(__VA_ARGS__); }

static inline void ERR(const char* format, ...)
{
	fprintf(stderr, "[ERR] ");
	if(format){
		va_list ap;
		va_start(ap, format);
		vfprintf(stderr, format, ap); 
		va_end(ap);
	}
	fprintf(stderr, "\n");
}

static inline char* programname(char* prgpath)
{
	if(!prgpath){
		return NULL;
	}
	char*	pprgname = basename(prgpath);
	if(0 == strncmp(pprgname, "lt-", strlen("lt-"))){
		pprgname = &pprgname[strlen("lt-")];
	}
	return pprgname;
}

static bool SetStringBytes(string& str, size_t totallength, char ch)
{
	if(totallength == 0){
		return false;
	}
	if((str.length() + 1) == totallength){
		// nothing to do
	}else if((str.length() + 1) < totallength){
		for(size_t cnt = totallength - (str.length() + 1); 0 < cnt; cnt--){
			str += ch;
		}
	}else{	// (str.length() + 1) > totallength
		str = str.substr(0, (totallength - 1));
	}
	return true;
}

inline string to_short(string& base)
{
	return base.substr(0, MINIMUM_DATA_LENGTH);
}

static inline void init_bench_opts(BOPTS& benchopts)
{
	memset(&benchopts.szConfig[0], 0, BENCH_CONF_LENGTH);

	benchopts.isServerMode		= false;
	benchopts.LoopCnt			= 1;
	benchopts.proccnt			= 1;
	benchopts.threadcnt			= 1;
	benchopts.isPrint			= true;
	benchopts.isTrunAround		= true;
	benchopts.DataLength		= MINIMUM_DATA_LENGTH;
	benchopts.isBroadcast		= false;
}

static inline void copy_bench_opts(BOPTS& dest, const BOPTS& src)
{
	memcpy(&dest.szConfig[0], &src.szConfig[0], BENCH_CONF_LENGTH);

	dest.isServerMode		= src.isServerMode;
	dest.LoopCnt			= src.LoopCnt;
	dest.proccnt			= src.proccnt;
	dest.threadcnt			= src.threadcnt;
	dest.isPrint			= src.isPrint;
	dest.isTrunAround		= src.isTrunAround;
	dest.DataLength			= src.DataLength;
	dest.isBroadcast		= src.isBroadcast;
}

static inline void get_nomotonic_time(struct timespec& ts)
{
	if(-1 == clock_gettime(CLOCK_MONOTONIC, &ts)){
		ERR("Could not get CLOCK_MONOTONIC timespec, but continue...(errno=%d)", errno);
		ts.tv_sec	= 0;
		ts.tv_nsec	= 0;
	}
}

static inline void get_nomotonic_time(struct timespec& diffts, const struct timespec& startts)
{
	struct timespec	endts;
	if(-1 == clock_gettime(CLOCK_MONOTONIC, &endts)){
		ERR("Could not get CLOCK_MONOTONIC timespec, but continue...(errno=%d)", errno);
		endts.tv_sec	= 0;
		endts.tv_nsec	= 0;
	}

	if(endts.tv_nsec < startts.tv_nsec){
		if(0 < endts.tv_sec){
			endts.tv_sec--;
			endts.tv_nsec += 1000 * 1000 * 1000;
		}else{
			ERR("Start time > End time, so result is ZERO.");
			diffts.tv_sec	= 0;
			diffts.tv_nsec	= 0;
			return;
		}
	}
	if(endts.tv_sec < startts.tv_sec){
		ERR("Start time > End time, so result is ZERO.");
		diffts.tv_sec	= 0;
		diffts.tv_nsec	= 0;
		return;
	}
	diffts.tv_nsec	= endts.tv_nsec - startts.tv_nsec;
	diffts.tv_sec	= endts.tv_sec - startts.tv_sec;
}

//---------------------------------------------------------
// Utility Functions
//---------------------------------------------------------
// Parse parameters
//
static void Help(char* progname)
{
	PRN("");
	PRN("Usage: %s [ -s | -c ] [options]", progname ? programname(progname) : "program");
	PRN("");
	PRN("Option  [ -s | -c ]       process mode: server or client");
	PRN("        -conf [file name] Configuration file( .ini / .json / .yaml ) path");
	PRN("        -json [json]      Configuration JSON string\n");
	PRN("        -l [loop count]   Loop count(default 1), 0 means no limit.");
	PRN("        -proccnt          process count.");
	PRN("        -threadcnt        thread count per one process.");
	PRN("        -ta               Turn around mode.(message is turned around on server to slave.)");
	PRN("        -b                Broadcast message(only client mode)");
	PRN("        -dl [bytes]       send message size.(minimum bytes is %d)", MINIMUM_DATA_LENGTH);
	PRN("        -pr               print sending/receiving message.");
	PRN("        -mergedump        display merge request/data.");
	PRN("        -d [debug level]  \"ERR\" / \"WAN\" / \"INF\" / \"DUMP\"");
	PRN("        -h                display help");
	PRN("");
}

//---------------------------------------------------------
// Parameter
//---------------------------------------------------------
static bool SetBenchOptions(ChmOpts& opts, BOPTS& benchopts)
{
	// configuration option
	string	config("");
	if(opts.Get("conf", config) || opts.Get("f", config)){
		MSG_CHMPRN("Configuration file is %s.", config.c_str());
	}else if(opts.Get("json", config)){
		MSG_CHMPRN("Configuration JSON is \"%s\"", config.c_str());
	}else{
		// check environment
		if(!getenv_chm_conffile(config) || !getenv_chm_jsonconf(config)){
			ERR("ERROR: There is no parameter \"-conf\"(\"-f\") or \"-json\".");
			exit(EXIT_FAILURE);
		}
	}
	strncpy(&benchopts.szConfig[0], config.c_str(), BENCH_CONF_LENGTH - 1);

	// loop count
	benchopts.LoopCnt = 1;
	string	strloop;
	if(opts.Get("l", strloop)){
		if(0 > (benchopts.LoopCnt = atoi(strloop.c_str()))){
			ERR("ERROR: \"-l\" option value is wrong, must set 0 - xxxx.");
			exit(EXIT_FAILURE);
		}
	}

	// process mode.
	if(opts.Find("s")){
		benchopts.isServerMode = true;
	}else if(opts.Find("c")){
		benchopts.isServerMode = false;
	}else{
		ERR("ERROR: There is no parameter \"-s\" or \"-c\".");
		exit(EXIT_FAILURE);
	}

	// parallel count.
	benchopts.proccnt	= 1;
	benchopts.threadcnt	= 1;
	string	strproccnt;
	string	strthreadcnt;
	if(opts.Get("proccnt", strproccnt)){
		if(1 > (benchopts.proccnt = atoi(strproccnt.c_str()))){
			ERR("ERROR: \"-proccnt\" option value is wrong, must set 1 - xxxx.");
			exit(EXIT_FAILURE);
		}
	}
	if(opts.Get("threadcnt", strthreadcnt)){
		if(1 > (benchopts.threadcnt = atoi(strthreadcnt.c_str()))){
			ERR("ERROR: \"-threadcnt\" option value is wrong, must set 1 - xxxx.");
			exit(EXIT_FAILURE);
		}
	}

	// turn-around mode.
	if(opts.Find("ta")){
		benchopts.isTrunAround = true;
	}else{
		benchopts.isTrunAround = false;
	}

	// print mode.
	if(opts.Find("pr")){
		benchopts.isPrint = true;
	}else{
		benchopts.isPrint = false;
	}

	// data length.
	string	strdatalen;
	if(opts.Get("dl", strdatalen)){
		int	tmp;
		if(MINIMUM_DATA_LENGTH > (tmp = atoi(strdatalen.c_str()))){
			ERR("ERROR: \"-dl\" option value is wrong, must set %d - xxxx.", MINIMUM_DATA_LENGTH);
			exit(EXIT_FAILURE);
		}
		benchopts.DataLength = static_cast<size_t>(tmp);
	}else{
		benchopts.DataLength = MINIMUM_DATA_LENGTH;
	}

	// broadcast mode.
	if(opts.Find("b")){
		if(benchopts.isServerMode){
			ERR("ERROR: Could not specify \"-b(broadcast)\" option with \"-s\" server mode.");
			exit(EXIT_FAILURE);
		}
		benchopts.isBroadcast = true;
	}else{
		benchopts.isBroadcast = false;
	}
	return true;
}

//---------------------------------------------------------
// Common Mmap file
//---------------------------------------------------------
static int OpenBenchFile(const string& filepath, size_t& totalsize, bool is_create)
{
	int	fd;

	if(is_create){
		// remove exist file
		struct stat	st;
		if(0 == stat(filepath.c_str(), &st)){
			// file exists
			if(0 != unlink(filepath.c_str())){
				ERR("Could not remove exist file(%s), errno=%d", filepath.c_str(), errno);
				return -1;
			}
		}

		// create file
		if(-1 == (fd = open(filepath.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH))){
			ERR("Could not make file(%s), errno=%d", filepath.c_str(), errno);
			return -1;
		}

		// truncate & clean up
		if(0 != ftruncate(fd, totalsize)){
			ERR("Could not truncate file(%s) to %zu, errno = %d", filepath.c_str(), totalsize, errno);
			CHM_CLOSE(fd);
			return -1;
		}
		unsigned char	szBuff[1024];
		memset(szBuff, 0, sizeof(szBuff));
		for(ssize_t wrote = 0, onewrote = 0; static_cast<size_t>(wrote) < totalsize; wrote += onewrote){
			onewrote = min(static_cast<size_t>(sizeof(unsigned char) * 1024), (totalsize - static_cast<size_t>(wrote)));
			if(-1 == (onewrote = pwrite(fd, szBuff, static_cast<size_t>(onewrote), static_cast<off_t>(wrote)))){
				ERR("Failed to write initializing file(%s), errno = %d", filepath.c_str(), errno);
				CHM_CLOSE(fd);
				return -1;
			}
		}
	}else{
		struct stat	st;
		if(0 != stat(filepath.c_str(), &st)){
			// file does not exist
			ERR("Could not find file(%s)", filepath.c_str());
			return -1;
		}
		if(-1 == (fd = open(filepath.c_str(), O_RDWR))){
			ERR("Could not open file(%s), errno = %d", filepath.c_str(), errno);
			return -1;
		}
		totalsize = static_cast<size_t>(st.st_size);
	}
	return fd;
}

static void* MmapBenchFile(bool is_create, int proccnt, int threadcnt, string& filepath, int& fd, PEXECCNTL& pexeccntl, PCHLDCNTL& pchldcntl, size_t& totalsize)
{
	// file path
	if(is_create){
		char	szBuff[32];
		sprintf(szBuff, BENCH_TMP_FILE_FORM, gettid());
		filepath	= szBuff;
		totalsize	= sizeof(EXECCNTL) + (sizeof(CHLDCNTL) * proccnt * threadcnt);
	}else{
		totalsize	= 0;
	}

	// open
	if(-1 == (fd = OpenBenchFile(filepath, totalsize, is_create))){
		ERR("Could not open(initialize) file(%s)", filepath.c_str());
		return NULL;
	}

	// mmap
	void*	pShm;
	if(MAP_FAILED == (pShm = mmap(NULL, totalsize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0))){
		ERR("Could not mmap to file(%s), errno = %d", filepath.c_str(), errno);
		CHM_CLOSE(fd);
		return NULL;
	}

	// set pointer
	pexeccntl = reinterpret_cast<PEXECCNTL>(pShm);
	pchldcntl = reinterpret_cast<PCHLDCNTL>(reinterpret_cast<off_t>(pShm) + sizeof(EXECCNTL));

	return pShm;
}

//---------------------------------------------------------
// Merge test functions
//---------------------------------------------------------
#define	MERGE_DUMMY_DATA_FORM		"%d - MERGE DATA"

static bool	IsMergeDump = false;

static bool MergeGetLastTimeCallback(chmpx_h handle, struct timespec* pts)
{
	if(!pts){
		ERR("Merge MergeGetLastTimeCallback parameter is wrong.");
		return false;
	}
	pts->tv_sec	= 0;
	pts->tv_nsec= 0;

	if(IsMergeDump){
		PRN("CALLBACK: MergeGetLastTimeCallback(chmpx_h(0x%016" PRIx64 "), timespec*(%p))", handle, pts);
	}
	return true;
}

static bool MergeGetCallback(chmpx_h handle, const PCHM_MERGE_GETPARAM pparam, chmhash_t* pnexthash, PCHMBIN* ppdatas, size_t* pdatacnt)
{
	if(!pparam || !pnexthash || !ppdatas || !pdatacnt){
		ERR("Merge MergeGetCallback parameter is wrong.");
		return false;
	}

	if(IsMergeDump){
		PRN("CALLBACK: MergeGetCallback(chmpx_h(0x%016" PRIx64 "), pparam(%p), chmhash_t(%p), PCHMBIN(%p), size_t(%p))", handle, pparam, pnexthash, ppdatas, pdatacnt);
		PRN("    PARAM  = {");
		PRN("        starthash           = 0x%016" PRIx64 ",",	pparam->starthash);
		PRN("        startts.tv_sec      = %zu.",				pparam->startts.tv_sec);
		PRN("        startts.tv_nsec     = %ld.", 				pparam->startts.tv_nsec);
		PRN("        endts.tv_sec        = %zu.", 				pparam->endts.tv_sec);
		PRN("        endts.tv_nsec       = %ld.",				pparam->endts.tv_nsec);
		PRN("        target_hash         = 0x%016" PRIx64 ",",	pparam->target_hash);
		PRN("        target_max_hash     = 0x%016" PRIx64 ",",	pparam->target_max_hash);
		PRN("        target_hash_range   = 0x%016" PRIx64 ,		pparam->target_hash_range);
		PRN("    }");
	}

	if(0x5555 == pparam->starthash){
		// second call -> end
		*pnexthash	= CHM_INVALID_HASHVAL;
		*pdatacnt	= 0;
		*ppdatas	= NULL;

		if(IsMergeDump){
			PRN("    RESULT(END OF MERGE) = {");
			PRN("        nexthash            = -1");
			PRN("        datacnt             = 0");
			PRN("        pdatas              = NULL");
			PRN("        pdatas->byptr       = NULL");
			PRN("        pdatas->length      = 0");
			PRN("    }");
		}
	}else{
		// first call
		char	szBuff[256];
		sprintf(szBuff, MERGE_DUMMY_DATA_FORM, getpid());

		*pnexthash			= 0x5555;
		*pdatacnt			= 1;
		*ppdatas			= reinterpret_cast<PCHMBIN>(malloc(sizeof(CHMBIN)));
		(*ppdatas)->byptr	= reinterpret_cast<unsigned char*>(malloc(strlen(szBuff) + 1));
		(*ppdatas)->length	= strlen(szBuff) + 1;
		strcpy(reinterpret_cast<char*>((*ppdatas)->byptr), szBuff);

		if(IsMergeDump){
			PRN("    RESULT(DO MERGE DATA) = {");
			PRN("        nexthash            = 0x5555(dummy hash)");
			PRN("        datacnt             = 1");
			PRN("        pdatas              = %p",		*ppdatas);
			PRN("        pdatas->byptr       = %p(%s)",	(*ppdatas ? (*ppdatas)->byptr : NULL), ((*ppdatas && (*ppdatas)->byptr) ? reinterpret_cast<char*>((*ppdatas)->byptr) : "null"));
			PRN("        pdatas->length      = %zu",	(*ppdatas ? (*ppdatas)->length : 0));
			PRN("    }");
		}
	}
	return true;
}

static bool MergeSetCallback(chmpx_h handle, size_t length, const unsigned char* pdata, const struct timespec* pts)
{
	if(!pdata || !pts){
		ERR("Merge MergeSetCallback parameter is wrong.");
		return false;
	}

	if(IsMergeDump){
		PRN("CALLBACK: MergeSetCallback(chmpx_h(0x%016" PRIx64 "), length(%zu), pdata(%p: \"%s\"), pts(sec=%zu, nsec=%ld))", handle, length, pdata, reinterpret_cast<const char*>(pdata), pts->tv_sec, pts->tv_nsec);
	}
	return true;
}

//---------------------------------------------------------
// Execute Functions
//---------------------------------------------------------
static void* RunServerModeThread(void* param)
{
	PTHPARAM	pThParam = reinterpret_cast<PTHPARAM>(param);
	if(!pThParam || !pThParam->pmycntl){
		ERR("Parameter for thread is wrong.");
		pthread_exit(NULL);
	}

	pThParam->pmycntl->threadid = gettid();
	if(!pThParam->pCntlObj || !pThParam->pexeccntl){
		ERR("Parameter for thread is wrong.");
		pThParam->pmycntl->is_ready = true;
		pThParam->pmycntl->is_exit = true;
		pthread_exit(NULL);
	}

	// wait for suspend flag off
	while(pThParam->pexeccntl->is_suspend && !pThParam->pexeccntl->is_exit){
		struct timespec	sleeptime = {0L, 10 * 1000 * 1000};	// 10ms
		nanosleep(&sleeptime, NULL);
	}
	if(pThParam->pexeccntl->is_exit){
		ERR("Exit thread ASSAP.");
		pThParam->pmycntl->is_ready	= true;
		pThParam->pmycntl->is_exit	= true;
		pthread_exit(NULL);
	}

	//---------------------------
	// Initialize internal datas.
	//---------------------------
	string	strbase = to_hexstring(gettid()) + "-";

	// wait for start
	// cppcheck-suppress unmatchedSuppression
	// cppcheck-suppress redundantAssignment
	pThParam->pmycntl->is_ready	= true;
	while(pThParam->pexeccntl->is_wait_doing && !pThParam->pexeccntl->is_exit){
		struct timespec	sleeptime = {0L, 1 * 1000 * 1000};	// 1ms
		nanosleep(&sleeptime, NULL);
	}
	if(pThParam->pexeccntl->is_exit){
		ERR("Exit thread ASSAP.");
		pThParam->pmycntl->is_exit = true;
		pthread_exit(NULL);
	}

	// set start timespec
	struct timespec	start;
	get_nomotonic_time(start);

	//---------------------------
	// Loop: Do read/write to k2hash
	//---------------------------
	for(int cnt = 0; 0 == pThParam->pexeccntl->opt.LoopCnt || cnt < pThParam->pexeccntl->opt.LoopCnt; ++cnt){
		PCOMPKT			pComPkt	= NULL;
		unsigned char*	pbody	= NULL;
		size_t			length	= 0L;

		if(!pThParam->pCntlObj->Receive(&pComPkt, &pbody, &length, 1000, true)){		// timeout 1s, auto rejoin
			ERR_CHMPRN("Something error occured at waiting message.");
			pThParam->pmycntl->errorcnt += (0 == pThParam->pexeccntl->opt.LoopCnt ? 0 : (pThParam->pexeccntl->opt.LoopCnt - cnt));
			pthread_exit(NULL);
		}
		if(!pComPkt){
			MSG_CHMPRN("Receiving message is timeouted(1s).");
			pThParam->pmycntl->errorcnt++;
		}else{
			MSG_CHMPRN("Succeed to receive message.");

			string	strReceive = pbody ? reinterpret_cast<char*>(pbody) : "";
			PRNFLAG(pThParam->pexeccntl->opt.isPrint, "Receive : %s", to_short(strReceive).c_str());

			if(pThParam->pexeccntl->opt.isTrunAround){
				strReceive += ":";
				strReceive += strbase;
				strReceive += to_hexstring(cnt);		// max "ffffffff-ffffffff:ffffffff-ffffffff" 18 + 17 + 1 bytes
				SetStringBytes(strReceive, pThParam->pexeccntl->opt.DataLength, FILL_CHARACTOR);

				// reply
				if(!pThParam->pCntlObj->Reply(pComPkt, reinterpret_cast<const unsigned char*>(strReceive.c_str()), strReceive.length() + 1)){
					if(!pThParam->pCntlObj->IsChmpxExit()){
						ERR_CHMPRN("Something error occured at replying message.");
						pThParam->pmycntl->errorcnt += (0 == pThParam->pexeccntl->opt.LoopCnt ? 0 : (pThParam->pexeccntl->opt.LoopCnt - cnt));
						CHM_Free(pComPkt);
						CHM_Free(pbody);
						pthread_exit(NULL);
					}
					pThParam->pmycntl->errorcnt++;
				}
			}
			CHM_Free(pComPkt);
			CHM_Free(pbody);
		}
	}

	// set timespec and exit flag
	get_nomotonic_time(pThParam->pmycntl->ts, start);
	// cppcheck-suppress unmatchedSuppression
	// cppcheck-suppress redundantAssignment
	pThParam->pmycntl->is_exit = true;

	pthread_exit(NULL);
	return NULL;
}

static void* RunSlaveModeThread(void* param)
{
	PTHPARAM	pThParam = reinterpret_cast<PTHPARAM>(param);
	if(!pThParam || !pThParam->pmycntl){
		ERR("Parameter for thread is wrong.");
		pthread_exit(NULL);
	}

	pThParam->pmycntl->threadid = gettid();
	if(!pThParam->pCntlObj || !pThParam->pexeccntl){
		ERR("Parameter for thread is wrong.");
		pThParam->pmycntl->is_ready = true;
		pThParam->pmycntl->is_exit = true;
		pthread_exit(NULL);
	}

	// wait for suspend flag off
	while(pThParam->pexeccntl->is_suspend && !pThParam->pexeccntl->is_exit){
		struct timespec	sleeptime = {0L, 10 * 1000 * 1000};	// 10ms
		nanosleep(&sleeptime, NULL);
	}
	if(pThParam->pexeccntl->is_exit){
		ERR("Exit thread ASSAP.");
		pThParam->pmycntl->is_ready	= true;
		pThParam->pmycntl->is_exit	= true;
		pthread_exit(NULL);
	}

	//---------------------------
	// Initialize internal datas.
	//---------------------------
	// open msgid
	msgid_t	msgid = pThParam->pCntlObj->Open(true);
	if(CHM_INVALID_MSGID == msgid){
		ERR("Could not open message queue id.");
		pThParam->pmycntl->errorcnt += pThParam->pexeccntl->opt.LoopCnt;
		pthread_exit(NULL);
	}
	string	strbase = to_hexstring(gettid()) + "-";
	string	strmessage;

	// wait for start
	// cppcheck-suppress unmatchedSuppression
	// cppcheck-suppress redundantAssignment
	pThParam->pmycntl->is_ready	= true;
	while(pThParam->pexeccntl->is_wait_doing && !pThParam->pexeccntl->is_exit){
		struct timespec	sleeptime = {0L, 1 * 1000 * 1000};	// 1ms
		nanosleep(&sleeptime, NULL);
	}
	if(pThParam->pexeccntl->is_exit){
		ERR("Exit thread ASSAP.");
		pThParam->pmycntl->is_exit = true;
		pthread_exit(NULL);
	}

	// set start timespec
	struct timespec	start;
	get_nomotonic_time(start);

	//---------------------------
	// Loop: Do read/write to k2hash
	//---------------------------
	for(int cnt = 0; 0 == pThParam->pexeccntl->opt.LoopCnt || cnt < pThParam->pexeccntl->opt.LoopCnt; ++cnt){
		PCOMPKT			pComPkt	= NULL;
		unsigned char*	pbody	= NULL;
		size_t			length	= 0L;
		long			receiver= 0L;
		bool			result;

		strmessage  = strbase;
		strmessage += to_hexstring(cnt);		// max "ffffffff-ffffffff" 17+1 bytes
		SetStringBytes(strmessage, pThParam->pexeccntl->opt.DataLength, FILL_CHARACTOR);

		if(pThParam->pexeccntl->opt.isBroadcast){
			result = pThParam->pCntlObj->Broadcast(msgid, reinterpret_cast<const unsigned char*>(strmessage.c_str()), strmessage.length() + 1, static_cast<chmhash_t>(cnt), &receiver);
		}else{
			result = pThParam->pCntlObj->Send(msgid, reinterpret_cast<const unsigned char*>(strmessage.c_str()), strmessage.length() + 1, static_cast<chmhash_t>(cnt), &receiver);
		}
		if(!result){
			if(!pThParam->pCntlObj->IsChmpxExit()){
				PRNFLAG(pThParam->pexeccntl->opt.isPrint, "Send : %s -> Failed", to_short(strmessage).c_str());
				ERR_CHMPRN("Something error occured at sending message.");
			}else{
				PRNFLAG(pThParam->pexeccntl->opt.isPrint, "Send : %s -> Failed(chmpx process down)", to_short(strmessage).c_str());
				// rejoin
				msgid = pThParam->pCntlObj->Open(true);
			}
			pThParam->pmycntl->errorcnt++;
			continue;
		}
		PRNFLAG(pThParam->pexeccntl->opt.isPrint, "Send : %s -> Succeed(receiver count:%ld)", to_short(strmessage).c_str(), receiver);

		if(pThParam->pexeccntl->opt.isTrunAround){
			for(long rcnt = 0; ((0 >= receiver) ? (rcnt < 1) : (rcnt < receiver)); ++rcnt){
				if(!pThParam->pCntlObj->Receive(msgid, &pComPkt, &pbody, &length, 1000)){	// timeout 1s
					if(!pThParam->pCntlObj->IsChmpxExit()){
						PRNFLAG(pThParam->pexeccntl->opt.isPrint, "Receive reply : -> 1 Failed");
						ERR_CHMPRN("Something error occured at waiting reply message.");
					}else{
						PRNFLAG(pThParam->pexeccntl->opt.isPrint, "Receive reply : -> 1 Failed(chmpx process down)");
						// rejoin
						msgid = pThParam->pCntlObj->Open(true);
					}
					pThParam->pmycntl->errorcnt++;
					break;
				}

				if(!pComPkt){
					PRNFLAG(pThParam->pexeccntl->opt.isPrint, "Receive reply : -> 1 Failed(timeouted - 1s)");
					MSG_CHMPRN("Receiving message is timeouted(1s).");
					pThParam->pmycntl->errorcnt++;
				}else{
					string	strReply = pbody ? reinterpret_cast<char*>(pbody) : "";
					PRNFLAG(pThParam->pexeccntl->opt.isPrint, "Receive reply : -> 1 Succeed(%s)", to_short(strReply).c_str());
					MSG_CHMPRN("Succeed to receive message.");
					CHM_Free(pComPkt);
					CHM_Free(pbody);
				}
			}
		}
	}

	// set timespec and exit flag
	get_nomotonic_time(pThParam->pmycntl->ts, start);
	pThParam->pmycntl->is_exit = true;

	pthread_exit(NULL);
	return NULL;
}

static int RunChild(string cntlfile)
{
	pid_t	pid = getpid();

	// mmap(attach) control file
	int			cntlfd		= -1;
	void*		pShmBase	= NULL;
	PEXECCNTL	pexeccntl	= NULL;
	PCHLDCNTL	pchldcntl	= NULL;
	size_t		totalsize	= 0;
	if(NULL == (pShmBase = MmapBenchFile(false, 0, 0, cntlfile, cntlfd, pexeccntl, pchldcntl, totalsize))){
		ERR("Could not mmap to file(%s), errno = %d", cntlfile.c_str(), errno);
		return EXIT_FAILURE;
	}

	// wait for suspend flag off
	while(pexeccntl->is_suspend && !pexeccntl->is_exit){
		struct timespec	sleeptime = {0L, 10 * 1000 * 1000};	// 10ms
		nanosleep(&sleeptime, NULL);
	}
	if(pexeccntl->is_exit){
		ERR("Exit process ASSAP.");
		munmap(pShmBase, totalsize);
		CHM_CLOSE(cntlfd);
		return EXIT_FAILURE;
	}

	// search my start pos of PCHLDCNTL.
	PCHLDCNTL	pmycntl = NULL;
	for(int cnt = 0; cnt < pexeccntl->opt.proccnt * pexeccntl->opt.threadcnt; cnt += pexeccntl->opt.threadcnt){
		if(pchldcntl[cnt].procid == pid){
			pmycntl = &pchldcntl[cnt];
			break;
		}
	}
	if(!pmycntl){
		ERR("Could not find my procid in PCHLDCNTL.");
		munmap(pShmBase, totalsize);
		CHM_CLOSE(cntlfd);
		return EXIT_FAILURE;
	}

	// Control Object
	ChmCntrl*	pCntlObj= new ChmCntrl();
	bool		bresult;
	if(pexeccntl->opt.isServerMode){
		bresult = pCntlObj->InitializeOnServer(&pexeccntl->opt.szConfig[0], true, MergeGetCallback, MergeSetCallback, MergeGetLastTimeCallback);	// auto rejoin
	}else{
		bresult = pCntlObj->InitializeOnSlave(&pexeccntl->opt.szConfig[0], true);		// auto rejoin
	}
	if(!bresult){
		ERR("Could not initialize internal object/data/etc.");
		CHM_Delete(pCntlObj);
		munmap(pShmBase, totalsize);
		CHM_CLOSE(cntlfd);
		return EXIT_FAILURE;
	}

	// create threads
	PTHPARAM	pthparam = new THPARAM[pexeccntl->opt.threadcnt];
	for(int cnt = 0; cnt < pexeccntl->opt.threadcnt; ++cnt){
		// initialize
		pmycntl[cnt].procid		= pid;
		pmycntl[cnt].threadid	= 0;
		pmycntl[cnt].pthreadid	= 0;
		pmycntl[cnt].is_ready	= false;
		pmycntl[cnt].is_exit	= false;		// *1
		pmycntl[cnt].ts.tv_sec	= 0;
		pmycntl[cnt].ts.tv_nsec	= 0;
		pmycntl[cnt].errorcnt	= 0;

		// create thread
		pthparam[cnt].pCntlObj	= pCntlObj;
		pthparam[cnt].pexeccntl	= pexeccntl;
		pthparam[cnt].pmycntl	= &pmycntl[cnt];
		if(0 != pthread_create(&(pmycntl[cnt].pthreadid), NULL, pexeccntl->opt.isServerMode ? RunServerModeThread : RunSlaveModeThread, &(pthparam[cnt]))){
			ERR("Could not create thread.");
			pmycntl[cnt].is_exit = true;		// *1
			break;
		}
	}

	// wait all thread exit
	for(int cnt = 0; cnt < pexeccntl->opt.threadcnt; ++cnt){
		if(0 == pmycntl[cnt].pthreadid){
			continue;
		}
		void*		pretval = NULL;
		int			result;
		if(0 != (result = pthread_join(pmycntl[cnt].pthreadid, &pretval))){
			ERR("Failed to wait thread exit. return code(error) = %d", result);
			continue;
		}
	}
	delete [] pthparam;

	// exit
	return EXIT_SUCCESS;
}

//---------------------------------------------------------
// Print Result
//---------------------------------------------------------
static void PrintResult(const PEXECCNTL pexeccntl, const PCHLDCNTL pchldcntl, const struct timespec& realtime)
{
	if(!pexeccntl || !pchldcntl){
		ERR("Parameters are wrong.");
		return;
	}

	// calc total
	struct timespec	addtotalts	= {0, 0};
	int				totalerr	= 0;
	for(int cnt = 0; cnt < pexeccntl->opt.proccnt * pexeccntl->opt.threadcnt; ++cnt){
		totalerr			+= pchldcntl[cnt].errorcnt;
		addtotalts.tv_sec	+= pchldcntl[cnt].ts.tv_sec;
		addtotalts.tv_nsec	+= pchldcntl[cnt].ts.tv_nsec;
		if((1000 * 1000 * 1000) < addtotalts.tv_nsec){
			addtotalts.tv_nsec	-= 1000 * 1000 * 1000;
			addtotalts.tv_sec	+= 1;
		}
	}
	// additional measurement total time
	long	addtotalfns	= (addtotalts.tv_sec * 1000 * 1000 * 1000) + addtotalts.tv_nsec;
	long	addtotalsec	= static_cast<long>(addtotalts.tv_sec);
	long	addtotalms	= (addtotalfns / (1000 * 1000)) % 1000;
	long	addtotalus	= (addtotalfns / 1000) % 1000;
	long	addtotalns	= addtotalfns % 1000;

	// additional measurement average time
	long	addavrgfns	= addtotalfns / static_cast<long>(pexeccntl->opt.proccnt * pexeccntl->opt.threadcnt * (0 == pexeccntl->opt.LoopCnt ? 1 : pexeccntl->opt.LoopCnt));		// Loop count
	long	addavrgsec	= addavrgfns / (1000 * 1000 * 1000);
	long	addavrgms	= (addavrgfns / (1000 * 1000)) % 1000;
	long	addavrgus	= (addavrgfns / 1000) % 1000;
	long	addavrgns	= addavrgfns % 1000;

	// additional measurement total time
	long	realtotalfns= (realtime.tv_sec * 1000 * 1000 * 1000) + realtime.tv_nsec;
	long	realtotalsec= static_cast<long>(realtime.tv_sec);
	long	realtotalms	= (realtotalfns / (1000 * 1000)) % 1000;
	long	realtotalus	= (realtotalfns / 1000) % 1000;
	long	realtotalns	= realtotalfns % 1000;

	// additional measurement average time
	long	realavrgfns	= realtotalfns / static_cast<long>(pexeccntl->opt.proccnt * pexeccntl->opt.threadcnt * (0 == pexeccntl->opt.LoopCnt ? 1 : pexeccntl->opt.LoopCnt));		// Loop count
	long	realavrgsec	= realavrgfns / (1000 * 1000 * 1000);
	long	realavrgms	= (realavrgfns / (1000 * 1000)) % 1000;
	long	realavrgus	= (realavrgfns / 1000) % 1000;
	long	realavrgns	= realavrgfns % 1000;

	PRN("===========================================================");
	PRN("CHMPX bench mark");
	PRN("-----------------------------------------------------------");
	PRN("CHMPX mode                    %s",			pexeccntl->opt.isServerMode ? "Server mode" : "Slave mode");
	PRN("CHMPX configuration           %s",			pexeccntl->opt.szConfig);
	PRN("-----------------------------------------------------------");
	PRN("Total loop count              %ld",		0 == pexeccntl->opt.LoopCnt ? 0 : pexeccntl->opt.proccnt * pexeccntl->opt.threadcnt * pexeccntl->opt.LoopCnt);
	PRN("  Process count               %ld",		pexeccntl->opt.proccnt);
	PRN("  Thread count                %ld",		pexeccntl->opt.threadcnt);
	PRN("  Loop count                  %ld",		pexeccntl->opt.LoopCnt);
	PRN("Data length                   %zu",		pexeccntl->opt.DataLength);
	PRN("Turn Around mode              %s",			pexeccntl->opt.isTrunAround ? "yes" : "no");
	PRN("Broadcast mode                %s",			pexeccntl->opt.isBroadcast ? "yes" : "no");
	if(0 == pexeccntl->opt.LoopCnt){
		PRN("");
		PRN("* If loop count is 0, it means no limited loop. So average times is calculated by 1 loop.");
		PRN("  Then average times can not be the correct value. So not display those.");
	}
	PRN("-----------------------------------------------------------");
	PRN("Total error count             %ld",		totalerr);
	PRN("Total time(addition thread)   %03lds %03ldms %03ldus %03ldns (%ldns)", addtotalsec, addtotalms, addtotalus, addtotalns, addtotalfns);
	if(0 != pexeccntl->opt.LoopCnt){
		PRN("Average time(addition thread) %03lds %03ldms %03ldus %03ldns (%ldns)", addavrgsec, addavrgms, addavrgus, addavrgns, addavrgfns);
	}
	PRN("Total time(real)              %03lds %03ldms %03ldus %03ldns (%ldns)", realtotalsec, realtotalms, realtotalus, realtotalns, realtotalfns);
	if(0 != pexeccntl->opt.LoopCnt){
		PRN("Average time(real)            %03lds %03ldms %03ldus %03ldns (%ldns)", realavrgsec, realavrgms, realavrgus, realavrgns, realavrgfns);
	}
	PRN("-----------------------------------------------------------");
}

//---------------------------------------------------------
// Main
//---------------------------------------------------------
int main(int argc, char** argv)
{
	// parse parameters
	ChmOpts	opts((argc - 1), &argv[1]);

	// help
	if(opts.Find("h") || opts.Find("help")){
		Help(argv[0]);
		exit(EXIT_SUCCESS);
	}

	// DBG Mode
	string	dbgmode;
	SetChmDbgMode(CHMDBG_SILENT);
	if(opts.Get("g", dbgmode) || opts.Get("d", dbgmode)){
		if(0 == strcasecmp(dbgmode.c_str(), "ERR")){
			SetChmDbgMode(CHMDBG_ERR);
		}else if(0 == strcasecmp(dbgmode.c_str(), "WAN")){
			SetChmDbgMode(CHMDBG_WARN);
		}else if(0 == strcasecmp(dbgmode.c_str(), "INF") || 0 == strcasecmp(dbgmode.c_str(), "MSG")){
			SetChmDbgMode(CHMDBG_MSG);
		}else if(0 == strcasecmp(dbgmode.c_str(), "DUMP")){
			SetChmDbgMode(CHMDBG_DUMP);
		}else{
			ERR("ERROR: Wrong parameter value \"-d\"(\"-g\") %s.", dbgmode.c_str());
			exit(EXIT_FAILURE);
		}
	}
	if(opts.Find("mergedump")){
		IsMergeDump = true;
	}

	// set all option
	BOPTS	benchopts;
	init_bench_opts(benchopts);
	if(!SetBenchOptions(opts, benchopts)){
		exit(EXIT_FAILURE);
	}

	// Initialize control file
	string		cntlfile;
	int			cntlfd		= -1;
	void*		pShmBase	= NULL;
	PEXECCNTL	pexeccntl	= NULL;
	PCHLDCNTL	pchldcntl	= NULL;
	size_t		totalsize	= 0;
	if(NULL == (pShmBase = MmapBenchFile(true, benchopts.proccnt, benchopts.threadcnt, cntlfile, cntlfd, pexeccntl, pchldcntl, totalsize))){
		ERR("Could not mmap to file(%s), errno = %d", cntlfile.c_str(), errno);
		exit(EXIT_SUCCESS);
	}
	pexeccntl->procid		= getpid();
	pexeccntl->is_suspend	= true;
	pexeccntl->is_wait_doing= true;
	pexeccntl->is_exit		= false;
	copy_bench_opts(pexeccntl->opt, benchopts);
	for(int cnt = 0; cnt < (benchopts.proccnt * benchopts.threadcnt); ++cnt){
		pchldcntl[cnt].procid		= 0;
		pchldcntl[cnt].threadid		= 0;
		pchldcntl[cnt].pthreadid	= 0;
		pchldcntl[cnt].is_ready		= false;
		pchldcntl[cnt].is_exit		= true;
		pchldcntl[cnt].ts.tv_sec	= 0;
		pchldcntl[cnt].ts.tv_nsec	= 0;
		pchldcntl[cnt].errorcnt		= 0;
	}

	// timespec for real time measurement
	struct timespec	realstart;
	struct timespec	realtime;

	//---------------------------------------
	// Run child process/threads
	//---------------------------------------
	int	childcnt = 0;
	for(childcnt = 0; childcnt < benchopts.proccnt; ++childcnt){
		// initialize
		for(int pcnt = 0; pcnt < benchopts.threadcnt; ++pcnt){
			pchldcntl[childcnt * benchopts.threadcnt + pcnt].procid		= 0;
			pchldcntl[childcnt * benchopts.threadcnt + pcnt].threadid	= 0;
			pchldcntl[childcnt * benchopts.threadcnt + pcnt].pthreadid	= 0;
			pchldcntl[childcnt * benchopts.threadcnt + pcnt].is_ready	= false;
			pchldcntl[childcnt * benchopts.threadcnt + pcnt].is_exit	= false;	// *1
			pchldcntl[childcnt * benchopts.threadcnt + pcnt].ts.tv_sec	= 0;
			pchldcntl[childcnt * benchopts.threadcnt + pcnt].ts.tv_nsec	= 0;
			pchldcntl[childcnt * benchopts.threadcnt + pcnt].errorcnt	= 0;
		}

		pid_t	pid = fork();
		if(-1 == pid){
			ERR("Could not fork child process, errno = %d", errno);
			pexeccntl->is_exit = true;

			for(int pcnt = 0; pcnt < benchopts.threadcnt; ++pcnt){
				pchldcntl[childcnt * benchopts.threadcnt + pcnt].is_exit = true;	// *1
			}
			break;
		}
		if(0 == pid){
			// child process
			int	result = RunChild(cntlfile);
			exit(result);

		}else{
			// parent process
			for(int pcnt = 0; pcnt < benchopts.threadcnt; ++pcnt){
				pchldcntl[childcnt * benchopts.threadcnt + pcnt].procid = pid;
			}
		}
	}

	// start children initializing
	// cppcheck-suppress unmatchedSuppression
	// cppcheck-suppress redundantAssignment
	pexeccntl->is_suspend	= false;

	// wait all children ready
	if(!pexeccntl->is_exit){
		// wait for all children is ready
		for(bool is_all_initialized = false; !is_all_initialized; ){
			is_all_initialized = true;
			for(int cnt = 0; cnt < (benchopts.proccnt * benchopts.threadcnt); ++cnt){
				if(!pchldcntl[cnt].is_exit && !pchldcntl[cnt].is_ready){
					is_all_initialized = false;
					break;
				}
			}
			if(!is_all_initialized){
				struct timespec	sleeptime = {0L, 10 * 1000 * 1000};	// 10ms
				nanosleep(&sleeptime, NULL);
			}
		}
	}

	// set start time point
	get_nomotonic_time(realstart);

	// start(no blocking) doing
	// cppcheck-suppress unmatchedSuppression
	// cppcheck-suppress redundantAssignment
	pexeccntl->is_wait_doing = false;

	// wait all process exit
	pid_t	exitpid;
	int		status = 0;
	while(0 < (exitpid = waitpid(-1, &status, 0))){
		--childcnt;
		if(0 >= childcnt){
			break;
		}
	}

	// set measurement timespec
	get_nomotonic_time(realtime, realstart);

	//---------------------------------------
	// End
	//---------------------------------------

	// display result
	PrintResult(pexeccntl, pchldcntl, realtime);

	// cleanup
	munmap(pShmBase, totalsize);
	CHM_CLOSE(cntlfd);
	unlink(cntlfile.c_str());

	exit(EXIT_SUCCESS);
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
