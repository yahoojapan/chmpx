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
#ifndef	CHMSIGCNTRL_H
#define	CHMSIGCNTRL_H

#include <signal.h>
#include <map>

#include <fullock/flckstructure.h>
#include <fullock/flckbaselist.tcc>

//---------------------------------------------------------
// ChmSigCntrl Class
//---------------------------------------------------------
class ChmSigCntrlHelper;

class ChmSigCntrl
{
	protected:
		static ChmSigCntrlHelper& GetHelper(void);

		bool SetHandlerEx(int signum, sighandler_t handler, bool is_locked);	// can set NULL/SIG_DFL/SIG_IGN
		bool FindSignalEx(int signum, bool is_locked);
		bool ResetAllHandler(void);

	public:
		ChmSigCntrl(void);
		virtual ~ChmSigCntrl();

		bool Initialize(const int* psignums, int count);
		bool GetSignalMask(sigset_t& sigset);
		bool SetSignalProcMask(void);
		bool SetHandler(int signum, sighandler_t handler) { return SetHandlerEx(signum, handler, false); }
};

#endif	// CHMSIGCNTRL_H

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noexpandtab sw=4 ts=4 fdm=marker
 * vim<600: noexpandtab sw=4 ts=4
 */
