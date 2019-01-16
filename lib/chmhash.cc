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
#include <k2hash/k2hashfunc.h>

#include "chmcommon.h"
#include "chmutil.h"
#include "chmdbg.h"
#include "chmhash.h"
#include "chmstructure.h"

using namespace	std;

//---------------------------------------------------------
// Symbols
//---------------------------------------------------------
#define	CHMPXID_PREFIX_HASH_STR		"CHMPX"

//---------------------------------------------------------
// Utilities
//---------------------------------------------------------
chmhash_t MakeChmpxId(const char* group, const char* hostname, short port)
{
	if(CHMEMPTYSTR(group) || CHMEMPTYSTR(hostname)){
		ERR_CHMPRN("Parameters are wrong.");
		return 0;
	}
	string	strbase1 = group;
	strbase1 += ":";
	strbase1 += hostname;
	strbase1 += ":";
	strbase1 += to_hexstring(port);

	chmhash_t	hash1 = K2H_HASH_FUNC(reinterpret_cast<const void*>(strbase1.c_str()), strbase1.length());
	chmhash_t	hash2 = K2H_2ND_HASH_FUNC(reinterpret_cast<const void*>(strbase1.c_str()), strbase1.length());
	chmhash_t	result= COMPOSE64(hash1, hash2);

	// If hash result is CHM_INVALID_CHMPXID, force to set -2. :-p
	if(result == CHM_INVALID_CHMPXID){
		result = static_cast<chmhash_t>(-2);
	}
	return result;
}

chmhash_t MakeBaseMsgId(const char* group, const char* hostname, short port)
{
	if(CHMEMPTYSTR(group) || CHMEMPTYSTR(hostname)){
		ERR_CHMPRN("Parameters are wrong.");
		return 0;
	}
	string	strbase1 = group;
	strbase1 += ":";
	strbase1 += hostname;
	strbase1 += ":";
	strbase1 += to_hexstring(port);

	chmhash_t	hash1 = K2H_HASH_FUNC(reinterpret_cast<const void*>(strbase1.c_str()), strbase1.length());
	chmhash_t	result= COMPOSE64(hash1, 0L);

	return result;
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
