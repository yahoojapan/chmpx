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
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <list>

#include "chmcommon.h"
#include "chmutil.h"
#include "chmthread.h"
#include "chmdbg.h"

using namespace	std;

//---------------------------------------------------------
// Utilities
//---------------------------------------------------------
inline bool IS_CHMTHREAD_EXIT(const volatile chmthsts_t& status)
{
	return (ChmThread::CHMTHCOM_EXIT == status);
}

inline bool IS_CHMTHREAD_SLEEP(const volatile chmthsts_t& status)
{
	return (ChmThread::CHMTHCOM_SLEEP == status);
}

inline bool IS_CHMTHREAD_WORKING(const volatile chmthsts_t& status)
{
	return (ChmThread::CHMTHCOM_WORK <= status);
}

inline void SET_CHMTHREAD_STATUS(volatile chmthsts_t& status, chmthsts_t newstatus, pthread_mutex_t& mutex)
{
	pthread_mutex_lock(&mutex);
	status = newstatus;
	pthread_mutex_unlock(&mutex);
}

inline void DECREMENT_CHMTHREAD_STATUS(volatile chmthsts_t& status, pthread_mutex_t& mutex)
{
	pthread_mutex_lock(&mutex);
	--status;
	pthread_mutex_unlock(&mutex);
}

inline void SET_CHMTHREAD_EXIT(volatile chmthsts_t& status, pthread_mutex_t& mutex)
{
	SET_CHMTHREAD_STATUS(status, ChmThread::CHMTHCOM_EXIT, mutex);
}

inline void SET_CHMTHREAD_SLEEP(volatile chmthsts_t& status, pthread_mutex_t& mutex)
{
	SET_CHMTHREAD_STATUS(status, ChmThread::CHMTHCOM_SLEEP, mutex);
}

inline void SET_CHMTHREAD_WORK(volatile chmthsts_t& status, pthread_mutex_t& mutex)
{
	SET_CHMTHREAD_STATUS(status, ChmThread::CHMTHCOM_WORK, mutex);
}

//---------------------------------------------------------
// Class Variable
//---------------------------------------------------------
const chmthsts_t	ChmThread::CHMTHCOM_EXIT;
const chmthsts_t	ChmThread::CHMTHCOM_SLEEP;
const chmthsts_t	ChmThread::CHMTHCOM_WORK;

//---------------------------------------------------------
// Class Method
//---------------------------------------------------------
void* ChmThread::WorkerProc(void* param)
{
	PCHMTHWP_PARAM	pchmthparam = reinterpret_cast<PCHMTHWP_PARAM>(param);

	// check
	if(!pchmthparam || !pchmthparam->pchmthread || !pchmthparam->work_proc){
		ERR_CHMPRN("Could not run worker thread(%lu), some parameter is wrong.", pthread_self());
		pthread_exit(NULL);
	}
	MSG_CHMPRN("Worker thread(%s: %ld) start up now.", pchmthparam->thread_name.c_str(), pthread_self());

	// loop
	while(true){
		if(IS_CHMTHREAD_EXIT(pchmthparam->status)){
			pthread_mutex_unlock(&(pchmthparam->cond_mutex));
			break;

		}else if(IS_CHMTHREAD_SLEEP(pchmthparam->status)){
			pthread_mutex_lock(&(pchmthparam->cond_mutex));

			// recheck status with locking.
			if(!IS_CHMTHREAD_SLEEP(pchmthparam->status)){
				pthread_mutex_unlock(&(pchmthparam->cond_mutex));
				continue;
			}

			// wait cond
			pthread_cond_wait(&(pchmthparam->cond_val), &(pchmthparam->cond_mutex));
			pthread_mutex_unlock(&(pchmthparam->cond_mutex));

			if(IS_CHMTHREAD_EXIT(pchmthparam->status)){
				break;
			}
		}

		// do work
		if(!pchmthparam->work_proc(pchmthparam->common_param, pchmthparam->wp_param)){
			// exit thread
			while(!fullock::flck_trylock_noshared_mutex(&(pchmthparam->pchmthread->list_lockval)));				// LOCK LIST

			// remove from working map
			chmthmap_t::iterator	miter = pchmthparam->pchmthread->chmthworkmap.find(pchmthparam->wp_param);
			if(pchmthparam->pchmthread->chmthworkmap.end() != miter){
				pchmthparam->pchmthread->chmthworkmap.erase(miter);
			}

			// change status to exit
			SET_CHMTHREAD_EXIT(pchmthparam->status, pchmthparam->cond_mutex);

			// [NOTE]
			// do not need to remove sleeping list and all list.

			fullock::flck_unlock_noshared_mutex(&(pchmthparam->pchmthread->list_lockval));						// UNLOCK LIST

		}else{
			// finish to work normal
			if(pchmthparam->is_onece){
				// finish to work, so exit thread
				while(!fullock::flck_trylock_noshared_mutex(&(pchmthparam->pchmthread->list_lockval)));			// LOCK LIST

				// remove from working map
				chmthmap_t::iterator	miter = pchmthparam->pchmthread->chmthworkmap.find(pchmthparam->wp_param);
				if(pchmthparam->pchmthread->chmthworkmap.end() != miter){
					pchmthparam->pchmthread->chmthworkmap.erase(miter);
				}

				// change status to exit
				SET_CHMTHREAD_EXIT(pchmthparam->status, pchmthparam->cond_mutex);

				// [NOTE]
				// do not need to remove sleeping list and all list.

				fullock::flck_unlock_noshared_mutex(&(pchmthparam->pchmthread->list_lockval));					// UNLOCK LIST

			}else if(pchmthparam->is_nosleep){
				// next to work assp, so set work status
				if(IS_CHMTHREAD_EXIT(pchmthparam->status)){
					// exit status, so remove from working map
					while(!fullock::flck_trylock_noshared_mutex(&(pchmthparam->pchmthread->list_lockval)));		// LOCK LIST

					// remove from working map
					chmthmap_t::iterator	miter = pchmthparam->pchmthread->chmthworkmap.find(pchmthparam->wp_param);
					if(pchmthparam->pchmthread->chmthworkmap.end() != miter){
						pchmthparam->pchmthread->chmthworkmap.erase(miter);
					}

					fullock::flck_unlock_noshared_mutex(&(pchmthparam->pchmthread->list_lockval));				// UNLOCK LIST

				}else{
					// keep working
					SET_CHMTHREAD_WORK(pchmthparam->status, pchmthparam->cond_mutex);
				}

			}else if(!pchmthparam->is_keep_evcnt){
				// if working count is over 1, set work status(keep working)
				if(IS_CHMTHREAD_WORKING(pchmthparam->status)){					// nolocking
					// check working count
					if(ChmThread::CHMTHCOM_WORK == pchmthparam->status){		// nolocking
						// not stack working count, so next to sleep
						while(!fullock::flck_trylock_noshared_mutex(&(pchmthparam->pchmthread->list_lockval)));	// LOCK LIST

						// remove from working map
						chmthmap_t::iterator	miter = pchmthparam->pchmthread->chmthworkmap.find(pchmthparam->wp_param);
						if(pchmthparam->pchmthread->chmthworkmap.end() != miter){
							pchmthparam->pchmthread->chmthworkmap.erase(miter);
						}
						// set status
						SET_CHMTHREAD_SLEEP(pchmthparam->status, pchmthparam->cond_mutex);

						// push sleeping list
						pchmthparam->pchmthread->chmthsleeps.push_back(pchmthparam);

						fullock::flck_unlock_noshared_mutex(&(pchmthparam->pchmthread->list_lockval));			// UNLOCK LIST

					}else{
						// stacked working count, so next to work
						SET_CHMTHREAD_WORK(pchmthparam->status, pchmthparam->cond_mutex);
					}

				}else if(IS_CHMTHREAD_EXIT(pchmthparam->status)){				// nolocking
					// exit status, so remove from working map
					while(!fullock::flck_trylock_noshared_mutex(&(pchmthparam->pchmthread->list_lockval)));		// LOCK LIST

					// remove from working map
					chmthmap_t::iterator	miter = pchmthparam->pchmthread->chmthworkmap.find(pchmthparam->wp_param);
					if(pchmthparam->pchmthread->chmthworkmap.end() != miter){
						pchmthparam->pchmthread->chmthworkmap.erase(miter);
					}

					fullock::flck_unlock_noshared_mutex(&(pchmthparam->pchmthread->list_lockval));				// UNLOCK LIST

				}else{
					// WHY? (already sleep)
					// next to sleep, so remove from working map
					while(!fullock::flck_trylock_noshared_mutex(&(pchmthparam->pchmthread->list_lockval)));		// LOCK LIST

					// remove from working map
					chmthmap_t::iterator	miter = pchmthparam->pchmthread->chmthworkmap.find(pchmthparam->wp_param);
					if(pchmthparam->pchmthread->chmthworkmap.end() != miter){
						pchmthparam->pchmthread->chmthworkmap.erase(miter);
					}

					// check sleeping list
					bool	is_need_push = true;
					for(chmthlist_t::iterator siter = pchmthparam->pchmthread->chmthsleeps.begin(); siter != pchmthparam->pchmthread->chmthsleeps.begin(); ++siter){
						if((*siter) == pchmthparam){
							// already set
							is_need_push = false;
						}
					}
					// push sleeping list
					if(is_need_push){
						pchmthparam->pchmthread->chmthsleeps.push_back(pchmthparam);
					}

					fullock::flck_unlock_noshared_mutex(&(pchmthparam->pchmthread->list_lockval));				// UNLOCK LIST
				}

			}else{
				// normal decrement working count
				if(ChmThread::CHMTHCOM_WORK == pchmthparam->status){			// nolocking
					while(!fullock::flck_trylock_noshared_mutex(&(pchmthparam->pchmthread->list_lockval)));		// LOCK LIST

					// next to sleep, so remove from working map
					chmthmap_t::iterator	miter = pchmthparam->pchmthread->chmthworkmap.find(pchmthparam->wp_param);
					if(pchmthparam->pchmthread->chmthworkmap.end() != miter){
						pchmthparam->pchmthread->chmthworkmap.erase(miter);
					}
					// set status
					SET_CHMTHREAD_SLEEP(pchmthparam->status, pchmthparam->cond_mutex);

					// push sleeping list
					pchmthparam->pchmthread->chmthsleeps.push_back(pchmthparam);

					fullock::flck_unlock_noshared_mutex(&(pchmthparam->pchmthread->list_lockval));				// UNLOCK LIST

				}else if(IS_CHMTHREAD_WORKING(pchmthparam->status)){			// nolocking
					// next to work, nothing to do
					DECREMENT_CHMTHREAD_STATUS(pchmthparam->status, pchmthparam->cond_mutex);

				}else if(IS_CHMTHREAD_EXIT(pchmthparam->status)){
					// exit status, so remove from working map
					while(!fullock::flck_trylock_noshared_mutex(&(pchmthparam->pchmthread->list_lockval)));		// LOCK LIST

					chmthmap_t::iterator	miter = pchmthparam->pchmthread->chmthworkmap.find(pchmthparam->wp_param);
					if(pchmthparam->pchmthread->chmthworkmap.end() != miter){
						pchmthparam->pchmthread->chmthworkmap.erase(miter);
					}

					fullock::flck_unlock_noshared_mutex(&(pchmthparam->pchmthread->list_lockval));				// UNLOCK LIST

				}else{
					// already sleep, nothing to do
				}
			}
		}
	}
	// if mapped in working, remove from working map
	while(!fullock::flck_trylock_noshared_mutex(&(pchmthparam->pchmthread->list_lockval)));						// LOCK LIST

	chmthmap_t::iterator	miter = pchmthparam->pchmthread->chmthworkmap.find(pchmthparam->wp_param);
	if(pchmthparam->pchmthread->chmthworkmap.end() != miter){
		pchmthparam->pchmthread->chmthworkmap.erase(miter);
	}
	fullock::flck_unlock_noshared_mutex(&(pchmthparam->pchmthread->list_lockval));								// UNLOCK LIST

	MSG_CHMPRN("Worker thread(%s: %ld) exit now.", pchmthparam->thread_name.c_str(), pthread_self());
	pthread_exit(NULL);

	return NULL;
}

//---------------------------------------------------------
// Methods
//---------------------------------------------------------
ChmThread::ChmThread(const char* pname) : thread_name(CHMEMPTYSTR(pname) ? "" : pname), free_proc(NULL), list_lockval(FLCK_NOSHARED_MUTEX_VAL_UNLOCKED)
{
}

ChmThread::~ChmThread()
{
	ExitAllThreads();
}

int ChmThread::CreateThreads(int thread_cnt, Tfp_Chm_WorkerProc work_func, Tfp_Chm_FreeParameter free_func, void* common_param, chmthparam_t wp_param, bool is_sleep_at_start, bool is_onece, bool is_nosleep, bool is_keep_evcnt)
{
	if(thread_cnt <= 0 || !work_func || (is_onece && is_nosleep)){
		ERR_CHMPRN("Parameter are wrong.");
		return 0;
	}

	// set common function
	free_proc = free_func;

	fullock::flck_lock_noshared_mutex(&list_lockval);			// LOCK

	// create thread loop
	for(int cnt = 0; cnt < thread_cnt; ++cnt){
		// thread parameter
		PCHMTHWP_PARAM	pthparam	= new CHMTHWP_PARAM;
		pthparam->thread_name		= thread_name + string("(") + to_string(cnt) + string(")");
		pthparam->pchmthread		= this;
		pthparam->status			= is_sleep_at_start ? ChmThread::CHMTHCOM_SLEEP : ChmThread::CHMTHCOM_WORK;
		pthparam->is_onece			= is_onece;
		pthparam->is_nosleep		= is_nosleep;
		pthparam->is_keep_evcnt		= is_keep_evcnt;
		pthparam->work_proc			= work_func;
		pthparam->common_param		= common_param;
		pthparam->wp_param			= wp_param;

		// create thread
		int	result;
		if(0 != (result = pthread_create(&(pthparam->threadid), NULL, ChmThread::WorkerProc, pthparam))){
			ERR_CHMPRN("Failed to create thread. return code(error) = %d", result);
			CHM_Delete(pthparam);
			fullock::flck_unlock_noshared_mutex(&list_lockval);	// UNLOCK
			return cnt;				// break loop
		}

		// add list
		chmthlist.push_back(pthparam);
		if(is_sleep_at_start){
			chmthsleeps.push_back(pthparam);
		}else{
			chmthworkmap[wp_param] = pthparam;
		}
	}
	fullock::flck_unlock_noshared_mutex(&list_lockval);			// UNLOCK

	return thread_cnt;
}

int ChmThread::ExitThreads(int thread_cnt, bool is_wait)
{
	if(thread_cnt <= 0){
		ERR_CHMPRN("Parameter is wrong.");
		return 0;
	}

	PCHMTHWP_PARAM	exit_pthparam;
	int				exited_cnt;
	for(exited_cnt = 0; exited_cnt < thread_cnt; ){
		fullock::flck_lock_noshared_mutex(&list_lockval);		// LOCK

		// search sleep thread.
		exit_pthparam	= NULL;
		for(chmthlist_t::iterator iter = chmthlist.begin(); iter != chmthlist.end(); ++iter){
			PCHMTHWP_PARAM	pthparam = *iter;

			// success to lock thread param
			pthread_mutex_lock(&(pthparam->cond_mutex));

			if(IS_CHMTHREAD_SLEEP(pthparam->status)){
				pthparam->status= ChmThread::CHMTHCOM_EXIT;
				exit_pthparam	= pthparam;

				// cond signal
				pthread_cond_signal(&(pthparam->cond_val));
				pthread_mutex_unlock(&(pthparam->cond_mutex));
				break;

			}else if(IS_CHMTHREAD_EXIT(pthparam->status)){
				// already exited...
				exit_pthparam	= pthparam;

				// cond signal
				pthread_cond_signal(&(pthparam->cond_val));
				pthread_mutex_unlock(&(pthparam->cond_mutex));
				break;
			}
			pthread_mutex_unlock(&(pthparam->cond_mutex));
		}
		fullock::flck_unlock_noshared_mutex(&list_lockval);		// UNLOCK

		// exit thread and cleanup
		if(exit_pthparam){
			if(!WaitExitThread(exit_pthparam)){
				ERR_CHMPRN("Could not stop sub thread, but continue...");
			}
			++exited_cnt;
		}else if(!is_wait){
			break;
		}
	}
	return exited_cnt;
}

bool ChmThread::WaitExitThread(PCHMTHWP_PARAM thread_param)
{
	if(!thread_param){
		ERR_CHMPRN("Parameter is wrong.");
		return false;
	}

	// wait for thread exiting
	//
	// [NOTICE]
	// If you have pthread_tryjoin_np function, should be instead of pthread_join.
	// (But pthread_tryjoin_np is not unsupport on some system.)
	//
	void*	pretval = NULL;
	int		result;
	if(0 != (result = pthread_join(thread_param->threadid, &pretval))){
		ERR_CHMPRN("Failed to wait exiting thread. return code(error) = %d", result);
		return false;
	}
	MSG_CHMPRN("Succeed to wait exiting thread. return value ptr = %p(expect=NULL)", pretval);

	// remove from list
	fullock::flck_lock_noshared_mutex(&list_lockval);		// LOCK

	// check working map
	chmthmap_t::iterator	miter = chmthworkmap.find(thread_param->wp_param);
	if(chmthworkmap.end() != miter){
		// why?
		WAN_CHMPRN("Why is thread(param = 0x%016" PRIx64 " ) not removed from mapping yet, do remove here.", thread_param->wp_param);
		chmthworkmap.erase(miter);
	}
	// check sleeping list
	for(chmthlist_t::iterator iter = chmthsleeps.begin(); iter != chmthsleeps.end(); ++iter){
		if(thread_param == (*iter)){
			chmthsleeps.erase(iter);
			break;
		}
	}
	// remove from all list
	for(chmthlist_t::iterator iter = chmthlist.begin(); iter != chmthlist.end(); ++iter){
		if(thread_param == (*iter)){
			chmthlist.erase(iter);
			break;
		}
	}
	fullock::flck_unlock_noshared_mutex(&list_lockval);		// UNLOCK

	// clear parameter
	if(free_proc){
		if(!free_proc(thread_param->common_param, thread_param->wp_param)){
			MSG_CHMPRN("Failed to free parameter for thread, but continue...");
		}
		thread_param->common_param	= NULL;
		thread_param->wp_param		= 0;
	}
	CHM_Delete(thread_param);

	return true;
}

bool ChmThread::ExitAllThreads(void)
{
	int	req_cnt;
	int	exited_cnt;

	fullock::flck_lock_noshared_mutex(&list_lockval);		// LOCK
	req_cnt = static_cast<int>(chmthlist.size());
	fullock::flck_unlock_noshared_mutex(&list_lockval);		// UNLOCK

	if(0 == req_cnt){
		return true;
	}
	if(req_cnt != (exited_cnt = ExitThreads(req_cnt, true))){
		ERR_CHMPRN("Could not stop all sub thread(%d), exited only %d thread.", req_cnt, exited_cnt);
		return false;
	}
	return true;
}

bool ChmThread::IsThreadRun(void)
{
	PCHMTHWP_PARAM	exit_pthparam;
	bool			result = false;
	while(true){
		while(!fullock::flck_trylock_noshared_mutex(&list_lockval));// LOCK

		// search exit thread and wait for exiting it.
		exit_pthparam	= NULL;
		for(chmthlist_t::iterator iter = chmthlist.begin(); iter != chmthlist.end(); ++iter){
			PCHMTHWP_PARAM	pthparam = *iter;
			if(IS_CHMTHREAD_EXIT(pthparam->status)){
				exit_pthparam	= pthparam;
				break;
			}
		}
		if(!exit_pthparam){
			// there is no exit status thread.
			result = (0 < chmthlist.size());
			fullock::flck_unlock_noshared_mutex(&list_lockval);		// UNLOCK
			break;
		}
		fullock::flck_unlock_noshared_mutex(&list_lockval);			// UNLOCK

		// exit thread and cleanup
		if(!WaitExitThread(exit_pthparam)){
			ERR_CHMPRN("Could not stop sub thread, but continue...");
		}
	}
	return result;
}

bool ChmThread::DoWorkThread(chmthparam_t req_param)
{
	return (NULL != DispatchThread(req_param, true, NULL));
}

PCHMTHWP_PARAM ChmThread::DispatchThread(chmthparam_t req_param, bool is_wait, bool* pis_already_working)
{
	static struct timespec	sleeptime = {0, 1};		// minimum value = 1ns

	if(pis_already_working){
		*pis_already_working = false;
	}

	// thread search loop(if needs)
	do{
		while(!fullock::flck_trylock_noshared_mutex(&list_lockval));	// LOCK

		// check working thread
		chmthmap_t::iterator	miter = chmthworkmap.find(req_param);
		if(chmthworkmap.end() != miter){
			// same parameter thread is working now.
			PCHMTHWP_PARAM	thread_param = miter->second;

			// check status
			if(IS_CHMTHREAD_WORKING(thread_param->status)){
				pthread_mutex_lock(&(thread_param->cond_mutex));

				// count up for continue to work
				thread_param->status++;

				if(pis_already_working){
					*pis_already_working = true;
				}
				fullock::flck_unlock_noshared_mutex(&list_lockval);		// UNLOCK

				// cond signal(just in case)
				pthread_cond_signal(&(thread_param->cond_val));
				pthread_mutex_unlock(&(thread_param->cond_mutex));

				return thread_param;
			}
		}

		// check sleep thread
		for(chmthlist_t::iterator iter = chmthsleeps.begin(); iter != chmthsleeps.end(); ++iter){
			PCHMTHWP_PARAM	thread_param = *iter;

			// check status
			if(IS_CHMTHREAD_SLEEP(thread_param->status)){
				// remove 
				chmthsleeps.erase(iter);

				// lock before status changing
				pthread_mutex_lock(&(thread_param->cond_mutex));

				// set status to work
				thread_param->wp_param	= req_param;
				thread_param->status	= ChmThread::CHMTHCOM_WORK;		// for blocking under flow

				// add working map
				chmthworkmap[thread_param->wp_param] = thread_param;

				fullock::flck_unlock_noshared_mutex(&list_lockval);		// UNLOCK

				// cond signal
				pthread_cond_signal(&(thread_param->cond_val));
				pthread_mutex_unlock(&(thread_param->cond_mutex));

				return thread_param;
			}
		}
		fullock::flck_unlock_noshared_mutex(&list_lockval);				// UNLOCK

		if(is_wait){
			nanosleep(&sleeptime, NULL);		// minimum sleep
		}
	}while(is_wait);

	MSG_CHMPRN("Could not find sleep or same thread.");
	return NULL;
}

bool ChmThread::ClearSelfWorkerStatus(void)
{
	pthread_t	threadid= pthread_self();
	bool		result	= false;

	while(!fullock::flck_trylock_noshared_mutex(&list_lockval));		// LOCK

	// At first, search working thread with same parameter
	for(chmthlist_t::iterator iter = chmthlist.begin(); iter != chmthlist.end(); ++iter){
		PCHMTHWP_PARAM	thread_param = *iter;

		if(pthread_equal(thread_param->threadid, threadid)){
			// found same thread
			if(IS_CHMTHREAD_WORKING(thread_param->status)){
				thread_param->status = ChmThread::CHMTHCOM_SLEEP;		// status to SLEEP
			}
			result = true;
			break;
		}
	}
	fullock::flck_unlock_noshared_mutex(&list_lockval);					// UNLOCK

	return result;
}

chmthparam_t ChmThread::GetSelfWorkerParam(void)
{
	pthread_t	threadid= pthread_self();

	while(!fullock::flck_trylock_noshared_mutex(&list_lockval));		// LOCK

	// At first, search working thread with same parameter
	for(chmthlist_t::iterator iter = chmthlist.begin(); iter != chmthlist.end(); ++iter){
		PCHMTHWP_PARAM	thread_param = *iter;

		if(pthread_equal(thread_param->threadid, threadid)){
			// found same thread
			fullock::flck_unlock_noshared_mutex(&list_lockval);			// UNLOCK
			return thread_param->wp_param;
		}
	}
	fullock::flck_unlock_noshared_mutex(&list_lockval);					// UNLOCK

	return 0;
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
