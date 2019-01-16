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
#ifndef	CHMPX_H
#define	CHMPX_H

#include <k2hash/k2hash.h>

#include "chmcommon.h"
#include "chmstructure.h"

DECL_EXTERN_C_START

//---------------------------------------------------------
// Typedefs
//---------------------------------------------------------
typedef	uint64_t		chmpx_h;					// ChmCntrl object handle
typedef	uint64_t		chmpx_pkt_h;				// COMPKT handle

//---------------------------------------------------------
// Symbols
//---------------------------------------------------------
#define	RCV_NO_WAIT		0							// no wait for receiving(=EVENT_NOWAIT)

//---------------------------------------------------------
// Structure & Utilities
//---------------------------------------------------------
// Utility structure for binary message which has key and value.
// 
// Some function gets CHMKVP pointer for sending message.
// These structure and functions help you make binary message pack
// and make hash code.
//
// This structure types are based and defined in k2hash.
//
typedef K2HBIN				CHMBIN;
typedef PK2HBIN				PCHMBIN;

#define	CHM_FREE_CHMBIN		free_k2hbin
#define	CHM_FREE_CHMBINS	free_k2hbins

typedef struct chmpx_kv_pair{
	CHMBIN	key;
	CHMBIN	val;
}CHMKVP, *PCHMKVP;

typedef struct chm_merge_getparam{
	chmhash_t		starthash;
	struct timespec	startts;					// start time for update datas
	struct timespec	endts;						// end time for update datas
	chmhash_t		target_hash;				// target hash value(0 ... target_max_hash)
	chmhash_t		target_max_hash;			// max target hash value
	chmhash_t		old_hash;					// old hash value(0 ... old_max_hash)
	chmhash_t		old_max_hash;				// max old target hash value
	long			target_hash_range;			// range for hash value(use both target_hash and old_hash)
	bool			is_expire_check;			// whether checking expire time
}CHM_MERGE_GETPARAM, *PCHM_MERGE_GETPARAM;

//---------------------------------------------------------
// Prototype functions
//---------------------------------------------------------
// Callback prototype function for merging
// 
// chm_merge_lastts_cb
// 	This callback function is called for getting lastest update time in
// 	client dadabase before merging.
// 	The return value(time) from this function will be used start time of
// 	target data for merging.
// 	If you did not specify this callback at chmpx_create_ex, the start
// 	time should be zero for merging. It means the target data is all of
// 	database.
// 
// chm_merge_get_cb
// 	This callback function is called for getting target data for merging.
// 	The parameter CHM_MERGE_GETPARAM means start of hash value, range
// 	from start hash value and time range(start to end).
// 	This callback function should return the datas which has minimum hash
//	value in range of hash value.
// 	If there are some target hash datas, you have to return all of them.
// 	The return value for pnexthash should be set next start hash value.
// 	When there is no more data, this function have to return the zero in
//	pdatacnt.
// 	If you did not specify this callback at chmpx_create_ex, no data is
//	merged.
// 
// chm_merge_set_cb
// 	This callback function is called for setting one data for merging.
// 	The parameters are datas which must be merged into database.
// 	If you did not specify this callback at chmpx_create_ex, no data is
//	merged.
// 
typedef bool (*chm_merge_lastts_cb)(chmpx_h handle, struct timespec* pts);
typedef bool (*chm_merge_get_cb)(chmpx_h handle, const PCHM_MERGE_GETPARAM pparam, chmhash_t* pnexthash, PCHMBIN* ppdatas, size_t* pdatacnt);
typedef bool (*chm_merge_set_cb)(chmpx_h handle, size_t length, const unsigned char* pdata, const struct timespec* pts);

//---------------------------------------------------------
// Functions
//---------------------------------------------------------
// [debug]
//
// chmpx_bump_debug_level			bumpup debugging level(silent -> error -> warning -> messages ->...)
// chmpx_set_debug_level_silent		set silent for debugging level
// chmpx_set_debug_level_error		set error for debugging level
// chmpx_set_debug_level_warning	set warning for debugging level
// chmpx_set_debug_level_message	set message for debugging level
// chmpx_set_debug_level_dump		set dump for debugging level
// chmpx_set_debug_file				set file path for debugging message
// chmpx_unset_debug_file			unset file path for debugging message to stderr(default)
// chmpx_load_debug_env				set debugging level and file path by loading environment.
//
extern void chmpx_bump_debug_level(void);
extern void chmpx_set_debug_level_silent(void);
extern void chmpx_set_debug_level_error(void);
extern void chmpx_set_debug_level_warning(void);
extern void chmpx_set_debug_level_message(void);
extern void chmpx_set_debug_level_dump(void);
extern bool chmpx_set_debug_file(const char* filepath);
extern bool chmpx_unset_debug_file(void);
extern bool chmpx_load_debug_env(void);

// [load hash library]
//
// chmpx_load_hash_library			load extra hash library(lap k2hash function)
// chmpx_unload_hash_library		unload extra hash library(lap k2hash function)
// 
extern bool chmpx_load_hash_library(const char* libpath);
extern bool chmpx_unload_hash_library(void);

// [create/destroy]
//
// chmpx_create_ex					create(join) chmpx process as client with callback function(only join to server)
// chmpx_create						create(join) chmpx process as client
// chmpx_destroy					destroy(leave) chmpx process
// chmpx_pkth_destroy				destroy compkt handle
// 
extern chmpx_h chmpx_create_ex(const char* conffile, bool is_on_server, bool is_auto_rejoin, chm_merge_get_cb getfp, chm_merge_set_cb setfp, chm_merge_lastts_cb lastupdatefp);
extern chmpx_h chmpx_create(const char* conffile, bool is_on_server, bool is_auto_rejoin);
extern bool chmpx_destroy(chmpx_h handle);
extern bool chmpx_pkth_destroy(chmpx_pkt_h pckthandle);

// [send/receive for server side]
//
// chmpx_svr_send					send message
// chmpx_svr_send_kvp				send message by CHMKVP structure
// chmpx_svr_send_kvp_ex			send message with(out) self chmpx mode by CHMKVP structure
// chmpx_svr_send_kv				send message by key and value
// chmpx_svr_send_kv_ex				send message with(out) self chmpx mode by key and value
// chmpx_svr_broadcast				send(broadcast) message
// chmpx_svr_broadcast_ex			send(broadcast) message with(out) self chmpx mode by key and value
// chmpx_svr_replicate				send(replicate) message without self chmpx mode
// chmpx_svr_replicate_ex			send(replicate) message with(out) self chmpx mode by key and value
// chmpx_svr_receive				receive message(ppbody, plength are allowed NULL, timeout_ms can be set RCV_NO_WAIT)
//
extern bool chmpx_svr_send(chmpx_h handle, const unsigned char* pbody, size_t blength, chmhash_t hash, bool is_routing);
extern bool chmpx_svr_send_kvp(chmpx_h handle, const PCHMKVP pkvp, bool is_routing);
extern bool chmpx_svr_send_kvp_ex(chmpx_h handle, const PCHMKVP pkvp, bool is_routing, bool without_self);
extern bool chmpx_svr_send_kv(chmpx_h handle, unsigned char* pkey, size_t keylen, unsigned char* pval, size_t vallen, bool is_routing);
extern bool chmpx_svr_send_kv_ex(chmpx_h handle, unsigned char* pkey, size_t keylen, unsigned char* pval, size_t vallen, bool is_routing, bool without_self);
extern bool chmpx_svr_broadcast(chmpx_h handle, const unsigned char* pbody, size_t blength, chmhash_t hash);
extern bool chmpx_svr_broadcast_ex(chmpx_h handle, const unsigned char* pbody, size_t blength, chmhash_t hash, bool without_self);
extern bool chmpx_svr_replicate(chmpx_h handle, const unsigned char* pbody, size_t blength, chmhash_t hash);
extern bool chmpx_svr_replicate_ex(chmpx_h handle, const unsigned char* pbody, size_t blength, chmhash_t hash, bool without_self);
extern bool chmpx_svr_receive(chmpx_h handle, chmpx_pkt_h* ppckthandle, unsigned char** ppbody, size_t* plength, int timeout_ms, bool no_giveup_rejoin);

// [send/receive for slave side]
//
// chmpx_open						open message handle
// chmpx_close						close message handle
// chmpx_msg_send					send message
// chmpx_msg_send_kvp				send message by CHMKVP
// chmpx_msg_send_kv				send message by key and value
// chmpx_msg_broadcast				send(broadcast) message
// chmpx_msg_replicate				send(replicate) message
// chmpx_msg_receive				receive message(timeout_ms can be set RCV_NO_WAIT)
//
extern msgid_t chmpx_open(chmpx_h handle, bool no_giveup_rejoin);
extern bool chmpx_close(chmpx_h handle, msgid_t msgid);
extern bool chmpx_msg_send(chmpx_h handle, msgid_t msgid, const unsigned char* pbody, size_t blength, chmhash_t hash, long* preceivercnt, bool is_routing);
extern bool chmpx_msg_send_kvp(chmpx_h handle, msgid_t msgid, const PCHMKVP pkvp, long* preceivercnt, bool is_routing);
extern bool chmpx_msg_send_kv(chmpx_h handle, msgid_t msgid, unsigned char* pkey, size_t keylen, unsigned char* pval, size_t vallen, long* preceivercnt, bool is_routing);
extern bool chmpx_msg_broadcast(chmpx_h handle, msgid_t msgid, const unsigned char* pbody, size_t blength, chmhash_t hash, long* preceivercnt);
extern bool chmpx_msg_replicate(chmpx_h handle, msgid_t msgid, const unsigned char* pbody, size_t blength, chmhash_t hash, long* preceivercnt);
extern bool chmpx_msg_receive(chmpx_h handle, msgid_t msgid, chmpx_pkt_h* ppckthandle, unsigned char** ppbody, size_t* plength, int timeout_ms);

// [reply for both server and slave side]
//
// chmpx_msg_reply					reply message
// chmpx_msg_reply_kvp				reply message by CHMKVP
// chmpx_msg_reply_kv				reply message by key and value
//
extern bool chmpx_msg_reply(chmpx_h handle, chmpx_pkt_h pckthandle, const unsigned char* pbody, size_t blength);
extern bool chmpx_msg_reply_kvp(chmpx_h handle, chmpx_pkt_h pckthandle, const PCHMKVP pkvp);
extern bool chmpx_msg_reply_kv(chmpx_h handle, chmpx_pkt_h pckthandle, unsigned char* pkey, size_t keylen, unsigned char* pval, size_t vallen);

// [check chmpx process]
//
// is_chmpx_proc_exists				check chmpx process exists
//
extern bool is_chmpx_proc_exists(chmpx_h handle);

//---------------------------------------------------------
// Version
//---------------------------------------------------------
extern void chmpx_print_version(FILE* stream);
extern const char* chmpx_get_version(void);

//---------------------------------------------------------
// Utility Functions(Key Value Pair)
//---------------------------------------------------------
extern unsigned char* cvt_kvp_bin(const PCHMKVP pkvp, size_t* plength);
extern bool cvt_bin_kvp(PCHMKVP pkvp, unsigned char* bydata, size_t length);
extern chmhash_t make_chmbin_hash(const PCHMBIN pchmbin);
extern chmhash_t make_kvp_hash(const PCHMKVP pkvp);

DECL_EXTERN_C_END

#endif	// CHMPX_H

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
