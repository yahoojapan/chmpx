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
#ifndef	CHMHASH_H
#define	CHMHASH_H

#include "chmcommon.h"
#include "chmstructure.h"

DECL_EXTERN_C_START

//---------------------------------------------------------
// Utilities
//---------------------------------------------------------
chmhash_t MakeChmpxId(const char* group, CHMPXID_SEED_TYPE type, const char* hostname, short ctlport, const char* cuk, const char* ctleps, const char* custom);
chmhash_t MakeBaseMsgId(const char* group, CHMPXID_SEED_TYPE type, const char* hostname, short ctlport, const char* cuk, const char* ctleps, const char* custom);

DECL_EXTERN_C_END

#endif	// CHMHASH_H

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noexpandtab sw=4 ts=4 fdm=marker
 * vim<600: noexpandtab sw=4 ts=4
 */
