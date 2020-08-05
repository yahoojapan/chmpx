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
#include <assert.h>

#include "chmcommon.h"
#include "chmutil.h"
#include "chmlock.h"
#include "chmstructure.h"
#include "chmdbg.h"

using namespace	std;

//---------------------------------------------------------
// Utility Macros
//---------------------------------------------------------
// [NOTE]
// The lock target is the file descriptor of ChmShm.
// Locks other than CHMLT_MQ lock and share the first 3 bytes
// of ChmShm.
// This area is a buffer for the version number and does not
// affect operations other than this lock. Therefore, these 3
// bytes are used.
//
#define	CHMLOCK_OFFSET_LOCKTYPE(LockType)		(	(CHMLT_IMDATA	== LockType) ? 0L : \
													(CHMLT_SOCKET	== LockType) ? 1L : \
													(CHMLT_MQOBJ	== LockType) ? 2L : \
													0L )

//---------------------------------------------------------
// Class Variables
//---------------------------------------------------------
int	ChmLock::chmshmfd = CHM_INVALID_HANDLE;

//---------------------------------------------------------
// Class Methods
//---------------------------------------------------------
int ChmLock::SetChmShmFd(int fd)
{
	int	oldfd			= ChmLock::chmshmfd;
	ChmLock::chmshmfd	= fd;
	return oldfd;
}

int ChmLock::UnsetChmShmFd(void)
{
	return ChmLock::SetChmShmFd(CHM_INVALID_HANDLE);
}

//---------------------------------------------------------
// Methods
//---------------------------------------------------------
ChmLock::ChmLock(void) : FLRwlRcsv(ChmLock::chmshmfd, CHMLOCK_OFFSET_LOCKTYPE(CHMLT_IMDATA), 1L), kind(CHMLT_IMDATA)
{
}

ChmLock::ChmLock(CHMLOCKKIND LockKind, CHMLOCKTYPE LockType) : FLRwlRcsv(ChmLock::chmshmfd, CHMLOCK_OFFSET_LOCKTYPE(LockKind), 1L, (CHMLT_READ == LockType)), kind(LockKind)
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
