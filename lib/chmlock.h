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
#ifndef	CHMLOCK_H
#define	CHMLOCK_H

#include <pthread.h>
#include <vector>
#include <fullock/rwlockrcsv.h>

//---------------------------------------------------------
// Symbols(enum)
//---------------------------------------------------------
typedef enum chm_lock_kind{
	CHMLT_IMDATA,
	CHMLT_SOCKET,
	CHMLT_MQ,
	CHMLT_MQOBJ				// use internal MQ event object
}CHMLOCKKIND;

typedef enum chm_lock_type{
	CHMLT_READ,
	CHMLT_WRITE
}CHMLOCKTYPE;

#define	CHM_STR_CHMLOCKKIND(kind)		(	CHMLT_IMDATA	== kind ? "IMDATA"	: \
											CHMLT_SOCKET	== kind ? "SOCKET"	: \
											CHMLT_MQ		== kind ? "MQ"		: "MQOBJ")
#define	CHM_STR_CHMLOCKTYPE(type)		(	CHMLT_READ		== type ? "READ"	: "WRITE" )

//---------------------------------------------------------
// ChmLock Class
//---------------------------------------------------------
// This class wraps FLRwlRcsv class
//
class ChmLock : public FLRwlRcsv
{
	protected:
		CHMLOCKKIND		kind;			// Lock target kind

	protected:
		ChmLock(void);					// Not use by any

	public:
		ChmLock(CHMLOCKKIND LockKind, CHMLOCKTYPE LockType);
		ChmLock(CHMLOCKTYPE LockType, int TargetFd, off_t FileOffset);
		virtual ~ChmLock();

		bool IsLocked(void) const { return is_locked; }
		bool Lock(void);
		bool UnLock(void);
};

#endif	// CHMLOCK_H

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
