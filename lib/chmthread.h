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
#ifndef	CHMTHREAD_H
#define	CHMTHREAD_H

#include <pthread.h>
#include <vector>

#include <fullock/flckstructure.h>
#include <fullock/flckbaselist.tcc>

class ChmThread;

//---------------------------------------------------------
// Utility inline function
//---------------------------------------------------------
inline bool CHMTHREAD_INITIALIZE_CONDVAL(pthread_mutex_t& cond_mutex, pthread_cond_t& cond_val)
{
	pthread_mutex_init(&cond_mutex, NULL);
	if(0 != pthread_cond_init(&cond_val, NULL)){
		return false;
	}
	return true;
}

inline bool CHMTHREAD_DESTROY_CONDVAL(pthread_mutex_t& cond_mutex, pthread_cond_t& cond_val)
{
	bool	result = true;
	if(0 != pthread_cond_destroy(&cond_val)){
		result = false;
	}
	if(0 != pthread_mutex_destroy(&cond_mutex)){
		result = false;
	}
	return result;
}

//---------------------------------------------------------
// Typedefs
//---------------------------------------------------------
typedef int			chmthsts_t;
typedef uint64_t	chmthparam_t;

typedef bool (*Tfp_Chm_WorkerProc)(void*, chmthparam_t);				// result: true -> continue, false -> exit thread
typedef bool (*Tfp_Chm_FreeParameter)(void*, chmthparam_t);

typedef struct chmth_wp_param{
	std::string				thread_name;
	ChmThread*				pchmthread;				// pointer to ChmThread
	volatile chmthsts_t		status;					// exit, sleep, working...
	bool					is_once;				// if true, thread exits after working once
	bool					is_nosleep;				// if true, thread never sleep
	bool					is_keep_evcnt;			// if false, no event stacking
	Tfp_Chm_WorkerProc		work_proc;
	void*					common_param;
	chmthparam_t			wp_param;
	pthread_mutex_t			cond_mutex;
	pthread_cond_t			cond_val;
	pthread_t				threadid;				// thread id

	chmth_wp_param() : thread_name(""), pchmthread(NULL), status(0), is_once(false), is_nosleep(false), is_keep_evcnt(true), work_proc(NULL), common_param(NULL), wp_param(0)	// status = CHMTHCOM_SLEEP
	{
		CHMTHREAD_INITIALIZE_CONDVAL(cond_mutex, cond_val);
	}

	~chmth_wp_param()
	{
		CHMTHREAD_DESTROY_CONDVAL(cond_mutex, cond_val);
	}
}CHMTHWP_PARAM, *PCHMTHWP_PARAM;

typedef std::vector<PCHMTHWP_PARAM>				chmthlist_t;
typedef std::map<chmthparam_t, PCHMTHWP_PARAM>	chmthmap_t;

//---------------------------------------------------------
// ChmThread Class
//---------------------------------------------------------
class ChmThread
{
	public:
		static const chmthsts_t	CHMTHCOM_EXIT	= -1;
		static const chmthsts_t	CHMTHCOM_SLEEP	= 0;
		static const chmthsts_t	CHMTHCOM_WORK	= 1;	// 1 and over 1

	protected:
		std::string				thread_name;
		Tfp_Chm_FreeParameter	free_proc;
		volatile int			list_lockval;			// like mutex
		chmthlist_t				chmthlist;				// all threads
		chmthlist_t				chmthsleeps;			// sleeping threads
		chmthmap_t				chmthworkmap;			// working threads

	protected:
		static void* WorkerProc(void* param);

		PCHMTHWP_PARAM DispatchThread(chmthparam_t req_param, bool is_wait, bool* pis_already_working);
		bool WaitExitThread(PCHMTHWP_PARAM thread_param);

	public:
		explicit ChmThread(const char* pname = NULL);
		virtual ~ChmThread();

		bool HasThread(void) const { return (0 < chmthlist.size()); }
		int GetThreadCount(void) const { return chmthlist.size(); }
		int CreateThreads(int thread_cnt, Tfp_Chm_WorkerProc work_func, Tfp_Chm_FreeParameter free_func = NULL, void* common_param = NULL, chmthparam_t wp_param = 0, bool is_sleep_at_start = true, bool is_once = false, bool is_nosleep = false, bool is_keep_evcnt = true);
		int ExitThreads(int thread_cnt, bool is_wait = true);
		bool ExitAllThreads(void);
		bool IsThreadRun(void);
		bool DoWorkThread(chmthparam_t req_param = 0);
		bool ClearSelfWorkerStatus(void);
		chmthparam_t GetSelfWorkerParam(void);
};

#endif	// CHMTHREAD_H

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noexpandtab sw=4 ts=4 fdm=marker
 * vim<600: noexpandtab sw=4 ts=4
 */
