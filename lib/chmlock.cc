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
#include <assert.h>

#include "chmcommon.h"
#include "chmutil.h"
#include "chmlock.h"
#include "chmdbg.h"

using namespace	std;

//---------------------------------------------------------
// Utility Macros
//---------------------------------------------------------
#define	CHMLOCK_NO_FD_LOCKTYPE(LockType)		FLCK_RWLOCK_NO_FD( \
													LockType == CHMLT_IMDATA	? (getpid() | 0x20000000) : \
													LockType == CHMLT_SOCKET	? (getpid() | 0x40000000) : \
													LockType == CHMLT_MQOBJ		? (getpid() | 0x60000000) : \
													0 )

//---------------------------------------------------------
// Methods
//---------------------------------------------------------
ChmLock::ChmLock(void) : FLRwlRcsv(FLCK_INVALID_HANDLE, 0L, 1L), kind(CHMLT_IMDATA)
{
}

ChmLock::ChmLock(CHMLOCKKIND LockKind, CHMLOCKTYPE LockType) : FLRwlRcsv(CHMLOCK_NO_FD_LOCKTYPE(LockKind), 0L, 1L, (CHMLT_READ == LockType)), kind(LockKind)
{
	assert(CHMLT_MQ != LockKind);
}

ChmLock::ChmLock(CHMLOCKTYPE LockType, int TargetFd, off_t FileOffset) : FLRwlRcsv(TargetFd, FileOffset, 1L, (CHMLT_READ == LockType)), kind(CHMLT_MQ)
{
	assert(CHM_INVALID_HANDLE != TargetFd);
}

ChmLock::~ChmLock()
{
}

bool ChmLock::Lock(void)
{
	if(is_locked){
		//MSG_CHMPRN("Already locked.");
		return true;
	}
	// check
	if(CHM_INVALID_HANDLE == lock_fd){
		ERR_CHMPRN("kind is %s, but fd is %d and type is %s.", CHM_STR_CHMLOCKKIND(kind), lock_fd, STR_FLCKLOCKTYPE(lock_type));
		return false;
	}
	return FLRwlRcsv::Lock();
}

bool ChmLock::UnLock(void)
{
	if(!is_locked){
		//MSG_CHMPRN("Already unlocked.");
		return true;
	}
	// check
	if(CHM_INVALID_HANDLE == lock_fd){
		ERR_CHMPRN("kind is %s, but fd is %d and type is %s.", CHM_STR_CHMLOCKKIND(kind), lock_fd, STR_FLCKLOCKTYPE(lock_type));
		return false;
	}
	return FLRwlRcsv::Unlock();
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
