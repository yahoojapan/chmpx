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
#ifndef	CHMEVENTBASE_H
#define	CHMEVENTBASE_H

#include <vector>

#include "chmcomstructure.h"

//---------------------------------------------------------
// Prototype
//---------------------------------------------------------
class ChmCntrl;

//---------------------------------------------------------
// Typedefs
//---------------------------------------------------------
typedef std::vector<int>			event_fds_t;

//---------------------------------------------------------
// ChmEventBase Class
//---------------------------------------------------------
class ChmEventBase
{
	protected:
		int			eqfd;				// backup
		ChmCntrl*	pChmCntrl;

	public:
		ChmEventBase(int eventqfd = CHM_INVALID_HANDLE, ChmCntrl* pcntrl = NULL);
		virtual ~ChmEventBase();

		bool IsEmpty(void) const { return (!pChmCntrl || CHM_INVALID_HANDLE == eqfd); }
		bool SetEventQueueFd(int eventqfd);
		bool SetChmCntrlObj(ChmCntrl* pcntrl);

		virtual bool Clean(void);
		virtual bool UpdateInternalData(void);

		virtual bool GetEventQueueFds(event_fds_t& fds) = 0;
		virtual bool SetEventQueue(void) = 0;
		virtual bool UnsetEventQueue(void) = 0;
		virtual bool IsEventQueueFd(int fd) = 0;

		virtual bool Send(PCOMPKT pComPkt, const unsigned char* pbody, size_t blength) = 0;
		virtual bool Receive(int fd) = 0;
		virtual bool NotifyHup(int fd) = 0;
		virtual bool Processing(PCOMPKT pComPkt) = 0;
};

#endif	// CHMEVENTBASE_H

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
