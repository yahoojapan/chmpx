/*
 * CHMPX
 *
 * Copyright 2014 Yahoo! JAPAN corporation.
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
 * the LICENSE file that was distributed with this source code.
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
// Class variable
//---------------------------------------------------------
ChmSigCntrl		ChmSigCntrl::singleton;
sigset_map_t	ChmSigCntrl::signalmap;
int				ChmSigCntrl::lockval = FLCK_NOSHARED_MUTEX_VAL_UNLOCKED;

//---------------------------------------------------------
// Methods
//---------------------------------------------------------
ChmSigCntrl::ChmSigCntrl()
{
}

ChmSigCntrl::~ChmSigCntrl()
{
	if(this == (&ChmSigCntrl::singleton)){
		// Do not need to unset signal handler.
		// There is a case that already ChmSigCntrl::signalmap object is deleted,
		// so invalide access to ChmSigCntrl::signalmap address.
		//
		//ResetAllHandler();
	}
}

bool ChmSigCntrl::Initialize(int* psignums, int count)
{
	while(!fullock::flck_trylock_noshared_mutex(&ChmSigCntrl::lockval));

	ChmSigCntrl::signalmap.clear();

	// static signals
	ChmSigCntrl::signalmap[SIGILL]	= false;
	ChmSigCntrl::signalmap[SIGABRT]	= false;
	ChmSigCntrl::signalmap[SIGFPE]	= false;
	ChmSigCntrl::signalmap[SIGKILL]	= false;
	ChmSigCntrl::signalmap[SIGSEGV]	= false;
	ChmSigCntrl::signalmap[SIGBUS]	= false;
	ChmSigCntrl::signalmap[SIGCONT]	= false;
	ChmSigCntrl::signalmap[SIGSTOP]	= false;
	ChmSigCntrl::signalmap[SIGTSTP]	= false;

	if(!psignums || 0 >= count){
		fullock::flck_unlock_noshared_mutex(&ChmSigCntrl::lockval);
		return true;
	}
	for(int cnt = 0; cnt < count; cnt++){
		signalmap[psignums[cnt]] = false;
	}
	fullock::flck_unlock_noshared_mutex(&ChmSigCntrl::lockval);

	return true;
}

bool ChmSigCntrl::GetSignalMask(sigset_t& sigset)
{
	if(0 != sigfillset(&sigset)){
		ERR_CHMPRN("Failed to fill to sigset.");
		return false;
	}

	while(!fullock::flck_trylock_noshared_mutex(&ChmSigCntrl::lockval));

	for(sigset_map_t::const_iterator iter = ChmSigCntrl::signalmap.begin(); iter != ChmSigCntrl::signalmap.end(); ++iter){
		if(0 != sigdelset(&sigset, iter->first)){
			ERR_CHMPRN("Failed to unset signal(%d).", iter->first);
			fullock::flck_unlock_noshared_mutex(&ChmSigCntrl::lockval);
			return false;
		}
	}
	fullock::flck_unlock_noshared_mutex(&ChmSigCntrl::lockval);

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
		while(!fullock::flck_trylock_noshared_mutex(&ChmSigCntrl::lockval));
	}

	bool	result = true;
	if(ChmSigCntrl::signalmap.end() == ChmSigCntrl::signalmap.find(signum)){
		result = false;
	}

	if(!is_locked){
		fullock::flck_unlock_noshared_mutex(&ChmSigCntrl::lockval);
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
		while(!fullock::flck_trylock_noshared_mutex(&ChmSigCntrl::lockval));
	}
	if(enable == ChmSigCntrl::signalmap[signum]){
		MSG_CHMPRN("Signal(%d) handler is already %s.", signum, enable ? "set" : "unset");
		if(!enable){
			if(!is_locked){
				fullock::flck_unlock_noshared_mutex(&ChmSigCntrl::lockval);
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
			fullock::flck_unlock_noshared_mutex(&ChmSigCntrl::lockval);
		}
		return false;
	}
	ChmSigCntrl::signalmap[signum] = enable;

	if(!is_locked){
		fullock::flck_unlock_noshared_mutex(&ChmSigCntrl::lockval);
	}
	return true;
}

bool ChmSigCntrl::ResetAllHandler(void)
{
	while(!fullock::flck_trylock_noshared_mutex(&ChmSigCntrl::lockval));

	for(sigset_map_t::iterator iter = ChmSigCntrl::signalmap.begin(); iter != ChmSigCntrl::signalmap.end(); ++iter){
		if(iter->second){
			if(!SetHandlerEx(iter->first, SIG_DFL, true)){
				ERR_CHMPRN("Could not reset signal(%d) handler.", iter->first);
				continue;
			}
			iter->second = false;
		}
	}
	fullock::flck_unlock_noshared_mutex(&ChmSigCntrl::lockval);

	return true;
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
