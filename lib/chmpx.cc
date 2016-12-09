/*
 * CHMPX
 *
 * Copyright 2014 Yahoo! JAPAN corporation.
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
 * the LICENSE file that was distributed with this source code.
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
#include "chmcntrl.h"
#include "chmkvp.h"
#include "chmpx.h"

using namespace	std;

//---------------------------------------------------------
// Functions - Debug
//---------------------------------------------------------
void chmpx_bump_debug_level(void)
{
	::BumpupChmDbgMode();
}

void chmpx_set_debug_level_silent(void)
{
	::SetChmDbgMode(CHMDBG_SILENT);
}

void chmpx_set_debug_level_error(void)
{
	::SetChmDbgMode(CHMDBG_ERR);
}

void chmpx_set_debug_level_warning(void)
{
	::SetChmDbgMode(CHMDBG_WARN);
}

void chmpx_set_debug_level_message(void)
{
	::SetChmDbgMode(CHMDBG_MSG);
}

void chmpx_set_debug_level_dump(void)
{
	::SetChmDbgMode(CHMDBG_DUMP);
}

bool chmpx_set_debug_file(const char* filepath)
{
	bool result;
	if(CHMEMPTYSTR(filepath)){
		result = ::UnsetChmDbgFile();
	}else{
		result = ::SetChmDbgFile(filepath);
	}
	return result;
}

bool chmpx_unset_debug_file(void)
{
	return ::UnsetChmDbgFile();
}

bool chmpx_load_debug_env(void)
{
	return ::LoadChmDbgEnv();
}

//---------------------------------------------------------
// Functions - Load hash library
//---------------------------------------------------------
bool chmpx_load_hash_library(const char* libpath)
{
	return ::k2h_load_hash_library(libpath);
}

bool chmpx_unload_hash_library(void)
{
	return ::k2h_unload_hash_library();
}

//---------------------------------------------------------
// Functions - Create/Destroy
//---------------------------------------------------------
chmpx_h chmpx_create_ex(const char* conffile, bool is_on_server, bool is_auto_rejoin, chm_merge_get_cb getfp, chm_merge_set_cb setfp, chm_merge_lastts_cb lastupdatefp)
{
	if(CHMEMPTYSTR(conffile)){
		ERR_CHMPRN("Configration file path is empty.");
		return CHM_INVALID_CHMPXHANDLE;
	}

	ChmCntrl*	pCntrlObj = new ChmCntrl();
	bool		result;
	if(is_on_server){
		result = pCntrlObj->InitializeOnServer(conffile, is_auto_rejoin, getfp, setfp, lastupdatefp);
	}else{
		result = pCntrlObj->InitializeOnSlave(conffile, is_auto_rejoin);
	}
	if(!result){
		return CHM_INVALID_CHMPXHANDLE;
	}
	return reinterpret_cast<chmpx_h>(pCntrlObj);
}

chmpx_h chmpx_create(const char* conffile, bool is_on_server, bool is_auto_rejoin)
{
	return chmpx_create_ex(conffile, is_on_server, is_auto_rejoin, NULL, NULL, NULL);
}

bool chmpx_destroy(chmpx_h handle)
{
	ChmCntrl*	pCntrlObj = reinterpret_cast<ChmCntrl*>(handle);
	if(!pCntrlObj){
		ERR_CHMPRN("Invalid chmpx handle.");
		return false;
	}
	CHM_Delete(pCntrlObj);

	return true;
}

bool chmpx_pkth_destroy(chmpx_pkt_h pckthandle)
{
	PCOMPKT	pComPkt = reinterpret_cast<PCOMPKT>(pckthandle);
	if(!pComPkt){
		return false;
	}
	CHM_Free(pComPkt);
	return true;
}

//---------------------------------------------------------
// Functions - Send/Receive for server side
//---------------------------------------------------------
bool chmpx_svr_send(chmpx_h handle, const unsigned char* pbody, size_t blength, chmhash_t hash, bool is_routing)
{
	ChmCntrl*	pCntrlObj = reinterpret_cast<ChmCntrl*>(handle);
	if(!pCntrlObj){
		ERR_CHMPRN("Invalid chmpx handle.");
		return false;
	}
	return pCntrlObj->Send(pbody, blength, hash, is_routing);
}

bool chmpx_svr_send_kvp(chmpx_h handle, const PCHMKVP pkvp, bool is_routing)
{
	return chmpx_svr_send_kvp_ex(handle, pkvp, is_routing, false);
}

bool chmpx_svr_send_kvp_ex(chmpx_h handle, const PCHMKVP pkvp, bool is_routing, bool without_self)
{
	if(!pkvp){
		ERR_CHMPRN("CHMKVP is NULL.");
		return false;
	}
	ChmCntrl*	pCntrlObj = reinterpret_cast<ChmCntrl*>(handle);
	if(!pCntrlObj){
		ERR_CHMPRN("Invalid chmpx handle.");
		return false;
	}

	ChmKVPair		kvpair(pkvp);
	unsigned char*	pbody;
	size_t			length = 0L;
	if(NULL == (pbody = kvpair.Put(length))){
		ERR_CHMPRN("Could not convert CHMKVP to Object.");
		return false;
	}
	chmhash_t	hash	= kvpair.GetHash();
	bool		result	= pCntrlObj->Send(pbody, length, hash, is_routing, without_self);
	CHM_Free(pbody);

	return result;
}

bool chmpx_svr_send_kv(chmpx_h handle, unsigned char* pkey, size_t keylen, unsigned char* pval, size_t vallen, bool is_routing)
{
	return chmpx_svr_send_kv_ex(handle, pkey, keylen, pval, vallen, is_routing, false);
}

bool chmpx_svr_send_kv_ex(chmpx_h handle, unsigned char* pkey, size_t keylen, unsigned char* pval, size_t vallen, bool is_routing, bool without_self)
{
	if(!pkey || 0L == keylen){
		ERR_CHMPRN("Key is invalid.");
		return false;
	}
	ChmCntrl*	pCntrlObj = reinterpret_cast<ChmCntrl*>(handle);
	if(!pCntrlObj){
		ERR_CHMPRN("Invalid chmpx handle.");
		return false;
	}

	ChmKVPair		kvpair(pkey, keylen, pval, vallen);
	unsigned char*	pbody;
	size_t			length = 0L;
	if(NULL == (pbody = kvpair.Put(length))){
		ERR_CHMPRN("Could not convert CHMKVP to Object.");
		return false;
	}
	chmhash_t	hash	= kvpair.GetHash();
	bool		result	= pCntrlObj->Send(pbody, length, hash, is_routing, without_self);
	CHM_Free(pbody);

	return result;
}

bool chmpx_svr_broadcast(chmpx_h handle, const unsigned char* pbody, size_t blength, chmhash_t hash)
{
	return chmpx_svr_broadcast_ex(handle, pbody, blength, hash, false);
}

bool chmpx_svr_broadcast_ex(chmpx_h handle, const unsigned char* pbody, size_t blength, chmhash_t hash, bool without_self)
{
	ChmCntrl*	pCntrlObj = reinterpret_cast<ChmCntrl*>(handle);
	if(!pCntrlObj){
		ERR_CHMPRN("Invalid chmpx handle.");
		return false;
	}
	return pCntrlObj->Broadcast(pbody, blength, hash, without_self);
}

bool chmpx_svr_replicate(chmpx_h handle, const unsigned char* pbody, size_t blength, chmhash_t hash)
{
	return chmpx_svr_replicate_ex(handle, pbody, blength, hash, true);		// default: without self
}

bool chmpx_svr_replicate_ex(chmpx_h handle, const unsigned char* pbody, size_t blength, chmhash_t hash, bool without_self)
{
	ChmCntrl*	pCntrlObj = reinterpret_cast<ChmCntrl*>(handle);
	if(!pCntrlObj){
		ERR_CHMPRN("Invalid chmpx handle.");
		return false;
	}
	return pCntrlObj->Replicate(pbody, blength, hash, without_self);
}

bool chmpx_svr_receive(chmpx_h handle, chmpx_pkt_h* ppckthandle, unsigned char** ppbody, size_t* plength, int timeout_ms, bool no_giveup_rejoin)
{
	ChmCntrl*	pCntrlObj = reinterpret_cast<ChmCntrl*>(handle);
	if(!pCntrlObj){
		ERR_CHMPRN("Invalid chmpx handle.");
		return false;
	}
	PCOMPKT*	ppComPkt = reinterpret_cast<PCOMPKT*>(ppckthandle);
	if(!ppComPkt){
		ERR_CHMPRN("Invalid packet handle pointer.");
		return false;
	}
	return pCntrlObj->Receive(ppComPkt, ppbody, plength, timeout_ms, no_giveup_rejoin);
}

//---------------------------------------------------------
// Functions - Send/Receive for slave side
//---------------------------------------------------------
msgid_t chmpx_open(chmpx_h handle, bool no_giveup_rejoin)
{
	ChmCntrl*	pCntrlObj = reinterpret_cast<ChmCntrl*>(handle);
	if(!pCntrlObj){
		ERR_CHMPRN("Invalid chmpx handle.");
		return CHM_INVALID_MSGID;
	}
	return pCntrlObj->Open(no_giveup_rejoin);
}

bool chmpx_close(chmpx_h handle, msgid_t msgid)
{
	ChmCntrl*	pCntrlObj = reinterpret_cast<ChmCntrl*>(handle);
	if(!pCntrlObj){
		ERR_CHMPRN("Invalid chmpx handle.");
		return false;
	}
	return pCntrlObj->Close(msgid);
}

bool chmpx_msg_send(chmpx_h handle, msgid_t msgid, const unsigned char* pbody, size_t blength, chmhash_t hash, long* preceivercnt, bool is_routing)
{
	ChmCntrl*	pCntrlObj = reinterpret_cast<ChmCntrl*>(handle);
	if(!pCntrlObj){
		ERR_CHMPRN("Invalid chmpx handle.");
		return false;
	}
	return pCntrlObj->Send(msgid, pbody, blength, hash, preceivercnt, is_routing);
}

bool chmpx_msg_send_kvp(chmpx_h handle, msgid_t msgid, const PCHMKVP pkvp, long* preceivercnt, bool is_routing)
{
	if(!pkvp){
		ERR_CHMPRN("CHMKVP is NULL.");
		return false;
	}
	ChmCntrl*	pCntrlObj = reinterpret_cast<ChmCntrl*>(handle);
	if(!pCntrlObj){
		ERR_CHMPRN("Invalid chmpx handle.");
		return false;
	}
	ChmKVPair		kvpair(pkvp);
	unsigned char*	pbody;
	size_t			length = 0L;
	if(NULL == (pbody = kvpair.Put(length))){
		ERR_CHMPRN("Could not convert CHMKVP to Object.");
		return false;
	}
	chmhash_t	hash	= kvpair.GetHash();
	bool		result	= pCntrlObj->Send(msgid, pbody, length, hash, preceivercnt, is_routing);
	CHM_Free(pbody);

	return result;
}

bool chmpx_msg_send_kv(chmpx_h handle, msgid_t msgid, unsigned char* pkey, size_t keylen, unsigned char* pval, size_t vallen, long* preceivercnt, bool is_routing)
{
	if(!pkey || 0L == keylen){
		ERR_CHMPRN("Key is invalid.");
		return false;
	}
	ChmCntrl*	pCntrlObj = reinterpret_cast<ChmCntrl*>(handle);
	if(!pCntrlObj){
		ERR_CHMPRN("Invalid chmpx handle.");
		return false;
	}
	ChmKVPair		kvpair(pkey, keylen, pval, vallen);
	unsigned char*	pbody;
	size_t			length = 0L;
	if(NULL == (pbody = kvpair.Put(length))){
		ERR_CHMPRN("Could not convert CHMKVP to Object.");
		return false;
	}
	chmhash_t	hash	= kvpair.GetHash();
	bool		result	= pCntrlObj->Send(msgid, pbody, length, hash, preceivercnt, is_routing);
	CHM_Free(pbody);

	return result;
}

bool chmpx_msg_broadcast(chmpx_h handle, msgid_t msgid, const unsigned char* pbody, size_t blength, chmhash_t hash, long* preceivercnt)
{
	ChmCntrl*	pCntrlObj = reinterpret_cast<ChmCntrl*>(handle);
	if(!pCntrlObj){
		ERR_CHMPRN("Invalid chmpx handle.");
		return false;
	}
	return pCntrlObj->Broadcast(msgid, pbody, blength, hash, preceivercnt);
}

bool chmpx_msg_replicate(chmpx_h handle, msgid_t msgid, const unsigned char* pbody, size_t blength, chmhash_t hash, long* preceivercnt)
{
	ChmCntrl*	pCntrlObj = reinterpret_cast<ChmCntrl*>(handle);
	if(!pCntrlObj){
		ERR_CHMPRN("Invalid chmpx handle.");
		return false;
	}
	return pCntrlObj->Replicate(msgid, pbody, blength, hash, preceivercnt);
}

bool chmpx_msg_receive(chmpx_h handle, msgid_t msgid, chmpx_pkt_h* ppckthandle, unsigned char** ppbody, size_t* plength, int timeout_ms)
{
	ChmCntrl*	pCntrlObj = reinterpret_cast<ChmCntrl*>(handle);
	if(!pCntrlObj){
		ERR_CHMPRN("Invalid chmpx handle.");
		return false;
	}
	PCOMPKT*	ppComPkt = reinterpret_cast<PCOMPKT*>(ppckthandle);
	if(!ppComPkt){
		ERR_CHMPRN("Invalid packet handle pointer.");
		return false;
	}
	return pCntrlObj->Receive(msgid, ppComPkt, ppbody, plength, timeout_ms);
}

//---------------------------------------------------------
// Functions - Reply for both server and slave side
//---------------------------------------------------------
bool chmpx_msg_reply(chmpx_h handle, chmpx_pkt_h pckthandle, const unsigned char* pbody, size_t blength)
{
	ChmCntrl*	pCntrlObj = reinterpret_cast<ChmCntrl*>(handle);
	if(!pCntrlObj){
		ERR_CHMPRN("Invalid chmpx handle.");
		return false;
	}
	PCOMPKT	pComPkt = reinterpret_cast<PCOMPKT>(pckthandle);
	if(!pComPkt){
		ERR_CHMPRN("Invalid packet handle.");
		return false;
	}
	return pCntrlObj->Reply(pComPkt, pbody, blength);
}

bool chmpx_msg_reply_kvp(chmpx_h handle, chmpx_pkt_h pckthandle, const PCHMKVP pkvp)
{
	if(!pkvp){
		ERR_CHMPRN("CHMKVP is NULL.");
		return false;
	}
	ChmCntrl*	pCntrlObj = reinterpret_cast<ChmCntrl*>(handle);
	if(!pCntrlObj){
		ERR_CHMPRN("Invalid chmpx handle.");
		return false;
	}
	PCOMPKT	pComPkt = reinterpret_cast<PCOMPKT>(pckthandle);
	if(!pComPkt){
		ERR_CHMPRN("Invalid packet handle.");
		return false;
	}

	ChmKVPair		kvpair(pkvp);
	unsigned char*	pbody;
	size_t			length = 0L;
	if(NULL == (pbody = kvpair.Put(length))){
		ERR_CHMPRN("Could not convert CHMKVP to Object.");
		return false;
	}

	bool	result = pCntrlObj->Reply(pComPkt, pbody, length);
	CHM_Free(pbody);

	return result;
}

bool chmpx_msg_reply_kv(chmpx_h handle, chmpx_pkt_h pckthandle, unsigned char* pkey, size_t keylen, unsigned char* pval, size_t vallen)
{
	if(!pkey || 0L == keylen){
		ERR_CHMPRN("Key is invalid.");
		return false;
	}
	ChmCntrl*	pCntrlObj = reinterpret_cast<ChmCntrl*>(handle);
	if(!pCntrlObj){
		ERR_CHMPRN("Invalid chmpx handle.");
		return false;
	}
	PCOMPKT	pComPkt = reinterpret_cast<PCOMPKT>(pckthandle);
	if(!pComPkt){
		ERR_CHMPRN("Invalid packet handle.");
		return false;
	}

	ChmKVPair		kvpair(pkey, keylen, pval, vallen);
	unsigned char*	pbody;
	size_t			length = 0L;
	if(NULL == (pbody = kvpair.Put(length))){
		ERR_CHMPRN("Could not convert CHMKVP to Object.");
		return false;
	}

	bool	result = pCntrlObj->Reply(pComPkt, pbody, length);
	CHM_Free(pbody);

	return result;
}

//---------------------------------------------------------
// Functions - Chmpx Process exists
//---------------------------------------------------------
bool is_chmpx_proc_exists(chmpx_h handle)
{
	ChmCntrl*	pCntrlObj = reinterpret_cast<ChmCntrl*>(handle);
	if(!pCntrlObj){
		ERR_CHMPRN("Invalid chmpx handle.");
		return false;
	}
	return !pCntrlObj->IsChmpxExit();
}

//---------------------------------------------------------
// Functions - Version
//---------------------------------------------------------
extern char chmpx_commit_hash[];

void chmpx_print_version(FILE* stream)
{
	static const char format[] =
		"\n"
		"CHMPX Version %s (commit: %s)\n"
		"\n"
		"Copyright 2014 Yahoo! JAPAN corporation.\n"
		"\n"
		"CHMPX is inprocess data exchange by MQ with consistent hashing.\n"
		"CHMPX is made for the purpose of the construction of original\n"
		"messaging system and the offer of the client library. CHMPX\n"
		"transfers messages between the client and the server/slave. CHMPX\n"
		"based servers are dispersed by consistent hashing and are\n"
		"automatically layouted. As a result, it provides a high performance,\n"
		"a high scalability.\n"
		"\n";
	fprintf(stream, format, VERSION, chmpx_commit_hash);
}

//---------------------------------------------------------
// Key Value Pair Utilities
//---------------------------------------------------------
unsigned char* cvt_kvp_bin(const PCHMKVP pkvp, size_t* plength)
{
	if(!pkvp || !plength){
		ERR_CHMPRN("Parameters are wrong.");
		return NULL;
	}
	*plength = pkvp->key.length + pkvp->val.length + sizeof(size_t) * 2;

	unsigned char*	pResult;
	if(NULL == (pResult = reinterpret_cast<unsigned char*>(malloc(*plength)))){
		ERR_CHMPRN("Could not allocate memory.");
		return NULL;
	}

	// set key
	size_t			setpos	= 0;
	size_t			tmplen	= htobe64(pkvp->key.length);			// To network byte order
	unsigned char*	bylen	= reinterpret_cast<unsigned char*>(&tmplen);
	for(size_t cnt = 0; cnt < sizeof(size_t); ++cnt){
		pResult[setpos++] = bylen[cnt];
	}
	if(0 < pkvp->key.length){
		memcpy(&pResult[setpos], pkvp->key.byptr, pkvp->key.length);
		setpos += pkvp->key.length;
	}

	// set value
	tmplen	= htobe64(pkvp->val.length);							// To network byte order
	for(size_t cnt = 0; cnt < sizeof(size_t); ++cnt){
		pResult[setpos++] = bylen[cnt];
	}
	if(0 < pkvp->val.length){
		memcpy(&pResult[setpos], pkvp->val.byptr, pkvp->val.length);
	}
	return pResult;
}

bool cvt_bin_kvp(PCHMKVP pkvp, unsigned char* bydata, size_t length)
{
	if(!pkvp || !bydata || length < (sizeof(size_t) * 2)){
		ERR_CHMPRN("Parameters are wrong.");
		return false;
	}

	// set key
	size_t	readpos		= 0;
	size_t*	ptmplen		= reinterpret_cast<size_t*>(&bydata[readpos]);
	pkvp->key.length	= be64toh(*ptmplen);						// To host byte order
	readpos				+= sizeof(size_t);
	if(0 < pkvp->key.length){
		pkvp->key.byptr	= &bydata[readpos];
		readpos			+= pkvp->key.length;
	}else{
		pkvp->key.byptr	= NULL;
	}

	// set value
	ptmplen				= reinterpret_cast<size_t*>(&bydata[readpos]);
	pkvp->val.length	= be64toh(*ptmplen);						// To host byte order
	readpos				+= sizeof(size_t);
	if(0 < pkvp->val.length){
		pkvp->val.byptr	= &bydata[readpos];
	}else{
		pkvp->val.byptr	= NULL;
	}
	return true;
}

chmhash_t make_chmbin_hash(const PCHMBIN pchmbin)
{
	if(!pchmbin){
		ERR_CHMPRN("Parameters are wrong.");
		return 0L;
	}
	return K2H_HASH_FUNC(reinterpret_cast<const void*>(pchmbin->byptr), pchmbin->length);
}

chmhash_t make_kvp_hash(const PCHMKVP pkvp)
{
	if(!pkvp || !pkvp->key.byptr || 0L == pkvp->key.length){
		ERR_CHMPRN("Parameters are wrong.");
		return 0L;
	}
	return make_chmbin_hash(&(pkvp->key));
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
