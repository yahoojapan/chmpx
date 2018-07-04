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
#ifndef	CHMHASH_H
#define	CHMHASH_H

#include "chmcommon.h"
#include "chmstructure.h"

DECL_EXTERN_C_START

//---------------------------------------------------------
// Utilities
//---------------------------------------------------------
chmhash_t MakeChmpxId(const char* group, const char* hostname, short port);
chmhash_t MakeBaseMsgId(const char* group, const char* hostname, short port);

DECL_EXTERN_C_END

#endif	// CHMHASH_H

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
