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
static bool MakeBaseSeed(const char* group, CHMPXID_SEED_TYPE type, const char* hostname, short ctlport, const char* cuk, const char* ctleps, const char* custom, string& seed)
{
	if(CHMEMPTYSTR(group)){
		ERR_CHMPRN("Parameter group is wrong.");
		return false;
	}
	seed = group;
	seed += ":";

	if(CHMPXID_SEED_NAME == type){
		if(CHMEMPTYSTR(hostname)){
			ERR_CHMPRN("Parameter hostname is wrong.");
			return false;
		}
		seed += hostname;
		seed += ":";
		seed += to_hexstring(ctlport);

	}else if(CHMPXID_SEED_CUK == type){
		if(CHMEMPTYSTR(cuk)){
			ERR_CHMPRN("Parameter cuk is wrong.");
			return false;
		}
		seed += cuk;

	}else if(CHMPXID_SEED_CTLENDPOINTS == type){
		if(!CHMEMPTYSTR(ctleps)){
			seed += ctleps;
		}else{
			if(CHMEMPTYSTR(hostname)){
				ERR_CHMPRN("Parameter ctleps and hostname are wrong.");
				return false;
			}
			seed += hostname;
			seed += ":";
			seed += to_hexstring(ctlport);
		}

	}else if(CHMPXID_SEED_CUSTOM == type){
		if(CHMEMPTYSTR(custom)){
			ERR_CHMPRN("Parameter custom is wrong.");
			return false;
		}
		seed += custom;

	}else{
		ERR_CHMPRN("Parameter chmpxid seed type is wrong.");
		return false;
	}
	return true;
}

chmhash_t MakeChmpxId(const char* group, CHMPXID_SEED_TYPE type, const char* hostname, short ctlport, const char* cuk, const char* ctleps, const char* custom)
{
	if(CHMEMPTYSTR(group) || CHMEMPTYSTR(hostname)){
		ERR_CHMPRN("Parameters are wrong.");
		return 0;
	}
	string	strseed;
	if(!MakeBaseSeed(group, type, hostname, ctlport, cuk, ctleps, custom, strseed)){
		return 0;
	}

	chmhash_t	hash1 = K2H_HASH_FUNC(reinterpret_cast<const void*>(strseed.c_str()), strseed.length());
	chmhash_t	hash2 = K2H_2ND_HASH_FUNC(reinterpret_cast<const void*>(strseed.c_str()), strseed.length());
	chmhash_t	result= COMPOSE64(hash1, hash2);

	// If hash result is CHM_INVALID_CHMPXID, force to set -2. :-p
	if(result == CHM_INVALID_CHMPXID){
		result = static_cast<chmhash_t>(-2);
	}
	return result;
}

chmhash_t MakeBaseMsgId(const char* group, CHMPXID_SEED_TYPE type, const char* hostname, short ctlport, const char* cuk, const char* ctleps, const char* custom)
{
	if(CHMEMPTYSTR(group)){
		ERR_CHMPRN("Parameter group is wrong.");
		return 0;
	}
	string	strseed;
	if(!MakeBaseSeed(group, type, hostname, ctlport, cuk, ctleps, custom, strseed)){
		return 0;
	}
	chmhash_t	hash	= K2H_HASH_FUNC(reinterpret_cast<const void*>(strseed.c_str()), strseed.length());
	chmhash_t	result	= COMPOSE64(hash, 0L);

	return result;
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
