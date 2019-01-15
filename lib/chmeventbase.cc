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
#include <string>

#include "chmcommon.h"
#include "chmeventbase.h"
#include "chmutil.h"
#include "chmdbg.h"

using namespace	std;

//---------------------------------------------------------
// ChmEventBase Methods
//---------------------------------------------------------
ChmEventBase::ChmEventBase(int eventqfd, ChmCntrl* pcntrl) : eqfd(eventqfd), pChmCntrl(pcntrl)
{
}

ChmEventBase::~ChmEventBase()
{
	ChmEventBase::Clean();
}

bool ChmEventBase::Clean(void)
{
	pChmCntrl	= NULL;
	eqfd		= CHM_INVALID_HANDLE;
	return true;
}

bool ChmEventBase::UpdateInternalData(void)
{
	return true;
}

bool ChmEventBase::SetEventQueueFd(int eventqfd)
{
	// do we need check this eqfd?
	eqfd = eventqfd;
	return true;
}

bool ChmEventBase::SetChmCntrlObj(ChmCntrl* pcntrl)
{
	pChmCntrl = pcntrl;
	return true;
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
