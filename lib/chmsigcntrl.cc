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
#include <signal.h>
#include <errno.h>

#include "chmcommon.h"
#include "chmsigcntrl.h"
#include "chmutil.h"
#include "chmdbg.h"

using namespace	std;

//---------------------------------------------------------
// Typedefs
//---------------------------------------------------------
typedef std::map<int, bool>		sigset_map_t;

//---------------------------------------------------------
// Class ChmSigCntrlHelper
//---------------------------------------------------------
// [NOTE]
// To avoid static object initialization order problem(SIOF)
//
class ChmSigCntrlHelper
{
	protected:
		int				lockval;									// like mutex
		sigset_map_t	signalmap;

	public:
		ChmSigCntrlHelper(void) : lockval(FLCK_NOSHARED_MUTEX_VAL_UNLOCKED) {}
		virtual ~ChmSigCntrlHelper(void) {}

		sigset_map_t& GetSigMap(void)
		{
			return signalmap;
		}

		void Lock(void)
		{
			while(!fullock::flck_trylock_noshared_mutex(&lockval));	// no call sched_yield()
		}

		void Unlock(void)
		{
			fullock::flck_unlock_noshared_mutex(&lockval);
		}
};

//---------------------------------------------------------
// Class Methods
//---------------------------------------------------------
ChmSigCntrlHelper& ChmSigCntrl::GetHelper(void)
{
	static ChmSigCntrlHelper	helper;
	return helper;
}

//---------------------------------------------------------
// Methods
//---------------------------------------------------------
ChmSigCntrl::ChmSigCntrl()
{
}

ChmSigCntrl::~ChmSigCntrl()
{
}

bool ChmSigCntrl::Initialize(const int* psignums, int count)
{
	ChmSigCntrl::GetHelper().Lock();

	// static signals
	ChmSigCntrl::GetHelper().GetSigMap().clear();
	ChmSigCntrl::GetHelper().GetSigMap()[SIGILL]	= false;
	ChmSigCntrl::GetHelper().GetSigMap()[SIGABRT]	= false;
	ChmSigCntrl::GetHelper().GetSigMap()[SIGFPE]	= false;
	ChmSigCntrl::GetHelper().GetSigMap()[SIGKILL]	= false;
	ChmSigCntrl::GetHelper().GetSigMap()[SIGSEGV]	= false;
	ChmSigCntrl::GetHelper().GetSigMap()[SIGBUS]	= false;
	ChmSigCntrl::GetHelper().GetSigMap()[SIGCONT]	= false;
	ChmSigCntrl::GetHelper().GetSigMap()[SIGSTOP]	= false;
	ChmSigCntrl::GetHelper().GetSigMap()[SIGTSTP]	= false;

	if(!psignums || 0 >= count){
		ChmSigCntrl::GetHelper().Unlock();
		return true;
	}
	for(int cnt = 0; cnt < count; cnt++){
		ChmSigCntrl::GetHelper().GetSigMap()[psignums[cnt]] = false;
	}
	ChmSigCntrl::GetHelper().Unlock();

	return true;
}

bool ChmSigCntrl::GetSignalMask(sigset_t& sigset)
{
	if(0 != sigfillset(&sigset)){
		ERR_CHMPRN("Failed to fill to sigset.");
		return false;
	}

	ChmSigCntrl::GetHelper().Lock();

	for(sigset_map_t::const_iterator iter = ChmSigCntrl::GetHelper().GetSigMap().begin(); iter != ChmSigCntrl::GetHelper().GetSigMap().end(); ++iter){
		if(0 != sigdelset(&sigset, iter->first)){
			ERR_CHMPRN("Failed to unset signal(%d).", iter->first);
			ChmSigCntrl::GetHelper().Unlock();
			return false;
		}
	}
	ChmSigCntrl::GetHelper().Unlock();

	return true;
}

bool ChmSigCntrl::SetSignalProcMask(void)
{
	sigset_t	sigset;
	if(!GetSignalMask(sigset)){
		ERR_CHMPRN("Failed to set sigset value.");
		return false;
	}
	if(0 != sigprocmask(SIG_SETMASK, &sigset, NULL)){
		ERR_CHMPRN("Failed to set signal proc mask(errno=%d).", errno);
		return false;
	}
	return true;
}

bool ChmSigCntrl::FindSignalEx(int signum, bool is_locked)
{
	if(!is_locked){
		ChmSigCntrl::GetHelper().Lock();
	}

	bool	result = true;
	if(ChmSigCntrl::GetHelper().GetSigMap().end() == ChmSigCntrl::GetHelper().GetSigMap().find(signum)){
		result = false;
	}

	if(!is_locked){
		ChmSigCntrl::GetHelper().Unlock();
	}
	return result;
}

bool ChmSigCntrl::SetHandlerEx(int signum, sighandler_t handler, bool is_locked)
{
	if(!FindSignalEx(signum, is_locked)){
		ERR_CHMPRN("Signal(%d) is not set this class.", signum);
		return false;
	}
	bool	enable = (NULL != handler && SIG_DFL != handler && SIG_IGN != handler);		// Simplify

	if(!is_locked){
		ChmSigCntrl::GetHelper().Lock();
	}
	if(enable == ChmSigCntrl::GetHelper().GetSigMap()[signum]){
		MSG_CHMPRN("Signal(%d) handler is already %s.", signum, enable ? "set" : "unset");
		if(!enable){
			if(!is_locked){
				ChmSigCntrl::GetHelper().Unlock();
			}
			return true;
		}
		// if enable, over set signal
	}

	struct sigaction	sa;

	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, signum);
	sa.sa_flags		= enable ? 0 : SA_RESETHAND;
	sa.sa_handler	= enable ? handler : SIG_DFL;

	if(0 > sigaction(signum, &sa, NULL)){
		ERR_CHMPRN("Could not %s signal(%d) handler, errno=%d", enable ? "set" : "unset", signum, errno);
		if(!is_locked){
			ChmSigCntrl::GetHelper().Unlock();
		}
		return false;
	}
	ChmSigCntrl::GetHelper().GetSigMap()[signum] = enable;

	if(!is_locked){
		ChmSigCntrl::GetHelper().Unlock();
	}
	return true;
}

bool ChmSigCntrl::ResetAllHandler(void)
{
	ChmSigCntrl::GetHelper().Lock();

	for(sigset_map_t::iterator iter = ChmSigCntrl::GetHelper().GetSigMap().begin(); iter != ChmSigCntrl::GetHelper().GetSigMap().end(); ++iter){
		if(iter->second){
			if(!SetHandlerEx(iter->first, SIG_DFL, true)){
				ERR_CHMPRN("Could not reset signal(%d) handler.", iter->first);
				continue;
			}
			iter->second = false;
		}
	}
	ChmSigCntrl::GetHelper().Unlock();

	return true;
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
