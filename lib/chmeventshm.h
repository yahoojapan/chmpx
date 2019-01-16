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
#ifndef	CHMEVENTSHM_H
#define	CHMEVENTSHM_H

#include <string>

#include "chmcommon.h"
#include "chmutil.h"
#include "chmeventbase.h"
#include "chmthread.h"

//---------------------------------------------------------
// Class ChmEventShm
//---------------------------------------------------------
// This class is to catch client process down realtime for
// cleaning MQ management data in CHMSHM.
//
class ChmEventShm : public ChmEventBase
{
	protected:
		std::string		chmshmfile;
		int				inotifyfd;
		int				watchfd;
		ChmThread		checkthread;

	protected:
		static bool IsProcessRunning(pid_t pid);

		bool IsWatching(void) const { return (CHM_INVALID_HANDLE != watchfd); }
		bool CheckNotifyEvent(void) const;
		bool ResetEventQueue(void);

	public:
		static bool CheckProcessRunning(void* common_param, chmthparam_t wp_param);

		ChmEventShm(int eventqfd = CHM_INVALID_HANDLE, ChmCntrl* pcntrl = NULL, const char* file = NULL);
		virtual ~ChmEventShm();

		virtual bool Clean(void);

		virtual bool GetEventQueueFds(event_fds_t& fds);
		virtual bool SetEventQueue(void);
		virtual bool UnsetEventQueue(void);
		virtual bool IsEventQueueFd(int fd);

		virtual bool Send(PCOMPKT pComPkt, const unsigned char* pbody, size_t blength);
		virtual bool Receive(int fd);
		virtual bool NotifyHup(int fd);
		virtual bool Processing(PCOMPKT pComPkt);
};

#endif	// CHMEVENTSHM_H

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
