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
#ifndef	CHMCOMSTRUCTURE_H
#define	CHMCOMSTRUCTURE_H

#include "chmcommon.h"
#include "chmstructure.h"
#include "chmdbg.h"

//---------------------------------------------------------
// Symbols
//---------------------------------------------------------
#define	MAX_K2HASH_MQ_KEY_LENGTH	128						// (uint64_t(32) + '-' + uint64_t(32) + '-' + uint64_t(32) + suffix(5) + null) + some bytes.
#define	MQ_PRIORITY_NOTICE			10						// COM_PX2C, COM_C2PX
#define	MQ_PRIORITY_MSG				10						// COM_C2C

// 
// The two higher bits in serial number is for the ACK on MQ.
//
#define	MQ_ACK_TYPE_MASK			(static_cast<serial_t>(0x80000000) & MASK64_LOWBIT)
#define	MQ_ACK_CODE_MASK			(static_cast<serial_t>(0x40000000) & MASK64_LOWBIT)
#define	MQ_ACK_MASK					(MQ_ACK_TYPE_MASK | MQ_ACK_CODE_MASK)
#define	MQ_ACK_SUCCESS				(MQ_ACK_TYPE_MASK)
#define	MQ_ACK_FAILURE				(MQ_ACK_TYPE_MASK | MQ_ACK_CODE_MASK)

#define	MIN_MQ_SERIAL_NUMBER		(0L)
#define	MAX_MQ_SERIAL_NUMBER		(static_cast<serial_t>(0x3FFFFFFF) & MASK64_LOWBIT)

#define	IS_MQ_ACK(val)				(MQ_ACK_TYPE_MASK == (((val) & MASK64_LOWBIT) & MQ_ACK_TYPE_MASK))
#define	IS_MQ_ACK_SUCCESS(val)		(MQ_ACK_SUCCESS == (((val) & MASK64_LOWBIT) & MQ_ACK_MASK))
#define	IS_MQ_ACK_FAILURE(val)		(MQ_ACK_FAILURE == (((val) & MASK64_LOWBIT) & MQ_ACK_MASK))
#define	GET_MQ_SERIAL_NUMBER(val)	(((val) & MASK64_LOWBIT) & ~MQ_ACK_MASK)

//---------------------------------------------------------
// Common structure
//---------------------------------------------------------
typedef struct chmpx_common_merge_param{
	chmpxid_t		chmpxid;									// requester chmpx
	struct timespec	startts;									// start time for update datas
	chmhash_t		max_pending_hash;							// max pending hash value
	chmhash_t		pending_hash;								// new(pending) hash value
	chmhash_t		max_base_hash;								// max base hash value
	chmhash_t		base_hash;									// base hash value
	long			replica_count;								// replica count
	bool			is_expire_check;							// whether checking expire time
}CHMPX_ATTR_PACKED PXCOMMON_MERGE_PARAM, *PPXCOMMON_MERGE_PARAM;

#define	COPY_COMMON_MERGE_PARAM(pdest, psrc)	\
		{ \
			(pdest)->chmpxid			= (psrc)->chmpxid;			\
			(pdest)->startts.tv_sec		= (psrc)->startts.tv_sec;	\
			(pdest)->startts.tv_nsec	= (psrc)->startts.tv_nsec;	\
			(pdest)->max_pending_hash	= (psrc)->max_pending_hash;	\
			(pdest)->pending_hash		= (psrc)->pending_hash;		\
			(pdest)->max_base_hash		= (psrc)->max_base_hash;	\
			(pdest)->base_hash			= (psrc)->base_hash;		\
			(pdest)->replica_count		= (psrc)->replica_count;	\
			(pdest)->is_expire_check	= (psrc)->is_expire_check;	\
		}

//---------------------------------------------------------
// COM_C2C: Communication Packets
//---------------------------------------------------------
// This type does not have structure. so data is binary.
//

//---------------------------------------------------------
// COM_C2PX, COM_PX2C: Communication Packets
//---------------------------------------------------------
// These packets are used for communicating on MQ.
// So do not change bit order.
//
typedef uint64_t						pxclttype_t;		// type for packet
typedef uint64_t						reqidmapflag_t;		// flag type for request update status for each chmpxid

// Type for COM_C2PX, COM_PX2C
#define	CHMPX_CLT_UNKNOWN				0					// Unknown
#define	CHMPX_CLT_CLOSE_NOTIFY			1					// Notification of closing MQ
#define	CHMPX_CLT_JOIN_NOTIFY			2					// Notification of joining MQ from client to chmpx
#define	CHMPX_CLT_MERGE_GETLASTTS		3					// Command for getting lastest update time(chmpx server node to server client)
#define	CHMPX_CLT_MERGE_RESLASTTS		4					// Response for getting lastest update time(server client to chmpx server node)
#define	CHMPX_CLT_REQ_UPDATEDATA		5					// Command for request update datas(chmpx server node to server client)
#define	CHMPX_CLT_RES_UPDATEDATA		6					// Response for request update datas(server client to chmpx server node)
#define	CHMPX_CLT_SET_UPDATEDATA		7					// Command for set update datas(chmpx server node to server client)
#define	CHMPX_CLT_RESULT_UPDATEDATA		8					// Response for result of update data(server client to chmpx server node)
#define	CHMPX_CLT_ABORT_UPDATEDATA		9					// Command for abort update data(chmpx server node to server client)

#define	STRPXCLTTYPE(type)			(	CHMPX_CLT_UNKNOWN			== type ? "CHMPX_CLT_UNKNOWN"			: \
										CHMPX_CLT_CLOSE_NOTIFY		== type ? "CHMPX_CLT_CLOSE_NOTIFY"		: \
										CHMPX_CLT_JOIN_NOTIFY		== type ? "CHMPX_CLT_JOIN_NOTIFY"		: \
										CHMPX_CLT_MERGE_GETLASTTS	== type ? "CHMPX_CLT_MERGE_GETLASTTS"	: \
										CHMPX_CLT_MERGE_RESLASTTS	== type ? "CHMPX_CLT_MERGE_RESLASTTS"	: \
										CHMPX_CLT_REQ_UPDATEDATA	== type ? "CHMPX_CLT_REQ_UPDATEDATA"	: \
										CHMPX_CLT_RES_UPDATEDATA	== type ? "CHMPX_CLT_RES_UPDATEDATA"	: \
										CHMPX_CLT_SET_UPDATEDATA	== type ? "CHMPX_CLT_SET_UPDATEDATA"	: \
										CHMPX_CLT_RESULT_UPDATEDATA	== type ? "CHMPX_CLT_RESULT_UPDATEDATA"	: \
										CHMPX_CLT_ABORT_UPDATEDATA	== type ? "CHMPX_CLT_ABORT_UPDATEDATA"	: \
										"NOT_DEFINED_TYPE"	)

//-------------------------------
// Structures
//
typedef struct chmpx_client_head{
	pxclttype_t		type;					// Command type
	size_t			length;					// Length for this packet
}CHMPX_ATTR_PACKED PXCLT_HEAD, *PPXCLT_HEAD;

typedef struct chmpx_client_close_notify{
	PXCLT_HEAD		head;
	long			count;
	msgid_t*		pmsgids;				// out of this structure area.
}CHMPX_ATTR_PACKED PXCLT_CLOSE_NOTIFY, *PPXCLT_CLOSE_NOTIFY;

typedef struct chmpx_client_join_notify{
	PXCLT_HEAD		head;
	pid_t			pid;					// client pid
}CHMPX_ATTR_PACKED PXCLT_JOIN_NOTIFY, *PPXCLT_JOIN_NOTIFY;

typedef struct chmpx_client_merge_getlastts{
	PXCLT_HEAD		head;
}CHMPX_ATTR_PACKED PXCLT_MERGE_GETLASTTS, *PPXCLT_MERGE_GETLASTTS;

typedef struct chmpx_client_merge_reslastts{
	PXCLT_HEAD		head;
	struct timespec	lastupdatets;			// using by only response
}CHMPX_ATTR_PACKED PXCLT_MERGE_RESLASTTS, *PPXCLT_MERGE_RESLASTTS;

typedef struct chmpx_client_req_updatedata{
	PXCLT_HEAD				head;
	PXCOMMON_MERGE_PARAM	merge_param;
}CHMPX_ATTR_PACKED PXCLT_REQ_UPDATEDATA, *PPXCLT_REQ_UPDATEDATA;

typedef struct chmpx_client_res_updatedata{
	PXCLT_HEAD		head;
	chmpxid_t		chmpxid;				// requester chmpx
	struct timespec	ts;						// updated time is also in this time at least
	size_t			length;
	unsigned char*	byptr;					// out of this structure area.
}CHMPX_ATTR_PACKED PXCLT_RES_UPDATEDATA, *PPXCLT_RES_UPDATEDATA;

typedef struct chmpx_client_set_updatedata{
	PXCLT_HEAD		head;
	struct timespec	ts;						// updated time is also in this time at least
	size_t			length;
	unsigned char*	byptr;					// out of this structure area.
}CHMPX_ATTR_PACKED PXCLT_SET_UPDATEDATA, *PPXCLT_SET_UPDATEDATA;

typedef struct chmpx_client_result_updatedata{
	PXCLT_HEAD		head;
	chmpxid_t		chmpxid;
	reqidmapflag_t	result;					// result
}CHMPX_ATTR_PACKED PXCLT_RESULT_UPDATEDATA, *PPXCLT_RESULT_UPDATEDATA;

typedef struct chmpx_client_abort_updatedata{
	PXCLT_HEAD		head;
}CHMPX_ATTR_PACKED PXCLT_ABORT_UPDATEDATA, *PPXCLT_ABORT_UPDATEDATA;

typedef union chmpx_clt_all{
	PXCLT_HEAD				val_head;
	PXCLT_CLOSE_NOTIFY		val_close_notify;
	PXCLT_JOIN_NOTIFY		val_join_notify;
	PXCLT_MERGE_GETLASTTS	val_merge_getlastts;
	PXCLT_MERGE_RESLASTTS	val_merge_reslastts;
	PXCLT_REQ_UPDATEDATA	val_req_updatedata;
	PXCLT_RES_UPDATEDATA	val_res_updatedata;
	PXCLT_SET_UPDATEDATA	val_set_updatedata;
	PXCLT_RESULT_UPDATEDATA	val_result_updatedata;
	PXCLT_ABORT_UPDATEDATA	val_abort_updatedata;
}CHMPX_ATTR_PACKED PXCLT_ALL, *PPXCLT_ALL;

//------------
// Utility Macros
//------------
#define	CVT_CLTPTR_CLOSE_NOTIFY(pCltAll)		&((pCltAll)->val_close_notify)
#define	CVT_CLTPTR_JOIN_NOTIFY(pCltAll)			&((pCltAll)->val_join_notify)
#define	CVT_CLTPTR_MERGE_GETLASTTS(pCltAll)		&((pCltAll)->val_merge_getlastts)
#define	CVT_CLTPTR_MERGE_RESLASTTS(pCltAll)		&((pCltAll)->val_merge_reslastts)
#define	CVT_CLTPTR_REQ_UPDATEDATA(pCltAll)		&((pCltAll)->val_req_updatedata)
#define	CVT_CLTPTR_RES_UPDATEDATA(pCltAll)		&((pCltAll)->val_res_updatedata)
#define	CVT_CLTPTR_SET_UPDATEDATA(pCltAll)		&((pCltAll)->val_set_updatedata)
#define	CVT_CLTPTR_RESULT_UPDATEDATA(pCltAll)	&((pCltAll)->val_result_updatedata)
#define	CVT_CLTPTR_ABORT_UPDATEDATA(pCltAll)	&((pCltAll)->val_abort_updatedata)
#define	CVT_CLT_ALL_PTR_PXCOMPKT(pComPkt)		(	CHM_OFFSET((pComPkt), sizeof(COMPKT), PPXCLT_ALL)	)

#define	SIZEOF_CHMPX_CLT(type)					(	CHMPX_CLT_CLOSE_NOTIFY		== type ? sizeof(PXCLT_CLOSE_NOTIFY)		: \
													CHMPX_CLT_JOIN_NOTIFY		== type ? sizeof(PXCLT_JOIN_NOTIFY)			: \
													CHMPX_CLT_MERGE_GETLASTTS	== type ? sizeof(PXCLT_MERGE_GETLASTTS)		: \
													CHMPX_CLT_MERGE_RESLASTTS	== type ? sizeof(PXCLT_MERGE_RESLASTTS)		: \
													CHMPX_CLT_REQ_UPDATEDATA	== type ? sizeof(PXCLT_REQ_UPDATEDATA)		: \
													CHMPX_CLT_RES_UPDATEDATA	== type ? sizeof(PXCLT_RES_UPDATEDATA)		: \
													CHMPX_CLT_SET_UPDATEDATA	== type ? sizeof(PXCLT_SET_UPDATEDATA)		: \
													CHMPX_CLT_RESULT_UPDATEDATA	== type ? sizeof(PXCLT_RESULT_UPDATEDATA)	: \
													CHMPX_CLT_ABORT_UPDATEDATA	== type ? sizeof(PXCLT_ABORT_UPDATEDATA)	: \
													0L)

#define	IS_PXCLT_TYPE(pCltAll, clttype)			(	pCltAll && pCltAll->val_head.type == clttype	)

#define	SET_PXCLTPKT(pComPkt, is_chmpx_proc, clttype, dept_msgid, term_msgid, serial_num, extlength)	\
		{ \
			(pComPkt)->length				= sizeof(COMPKT) + SIZEOF_CHMPX_CLT(clttype) + extlength; 	\
			(pComPkt)->offset				= sizeof(COMPKT); \
			(pComPkt)->head.version			= COM_VERSION_1; \
			(pComPkt)->head.type			= is_chmpx_proc ? COM_PX2C : COM_C2PX; \
			(pComPkt)->head.c2ctype			= COM_C2C_IGNORE; \
			(pComPkt)->head.dept_ids.chmpxid= CHM_INVALID_CHMPXID; \
			(pComPkt)->head.dept_ids.msgid	= dept_msgid; \
			(pComPkt)->head.term_ids.chmpxid= CHM_INVALID_CHMPXID; \
			(pComPkt)->head.term_ids.msgid	= term_msgid; \
			(pComPkt)->head.peer_dept_msgid	= dept_msgid; \
			(pComPkt)->head.peer_term_msgid	= term_msgid; \
			(pComPkt)->head.mq_serial_num	= serial_num; \
			(pComPkt)->head.hash			= CHM_INVALID_HASHVAL; \
			if(!RT_TIMESPEC(&((pComPkt)->head.reqtime))){ \
				WAN_CHMPRN("Could not get timespec(errno=%d), but continue...", errno); \
				INIT_TIMESPEC(&((pComPkt)->head.reqtime)); \
			} \
		}

//---------------------------------------------------------
// COM_S2PX: Communication Packets
//---------------------------------------------------------
typedef struct chmpx_signal_com{
	int				signalno;
}CHMPX_ATTR_PACKED PXSIG_COM, *PPXSIG_COM;

//---------------------------------------------------------
// COM_F2PX: Communication Packets
//---------------------------------------------------------
typedef struct chmpx_inotify_com{
	int				wfd;
	uint32_t		mask;
}CHMPX_ATTR_PACKED PXFILE_COM, *PPXFILE_COM;

//---------------------------------------------------------
// COM_PX2PX: Communication Packets
//---------------------------------------------------------
// Following pxcomtype_t is long(64) instead of enum because 
// PXCOM_HEAD should be alignment for datas after it.
//
typedef uint64_t						pxcomtype_t;		// type for packet
typedef uint64_t						pxcomres_t;			// result code for PXCOM_XXX

// [NOTICE]
// CHMPX_COM_STATUS_REQ & CHMPX_COM_STATUS_RES is pull-type for status and server information updating.
// CHMPX_COM_STATUS_UPDATE is push to all server for updating by sending server's all status.
// So CHMPX_COM_STATUS_UPDATE is not used now because of taking very careful(no recovering if failed).
//
#define	CHMPX_COM_UNKNOWN				0					// Unknown
#define	CHMPX_COM_STATUS_REQ			1					// Get server nodes status
#define	CHMPX_COM_STATUS_RES			2					// Result(Response) server nodes status
#define	CHMPX_COM_CONINIT_REQ			3					// Send initial connect information
#define	CHMPX_COM_CONINIT_RES			4					// Result(Response) initial connect information
#define	CHMPX_COM_JOIN_RING				5					// [loop in RING] server up & join ring
#define	CHMPX_COM_STATUS_UPDATE			6					// [loop in RING] status update
#define	CHMPX_COM_STATUS_CONFIRM		7					// [loop in RING] status confirm
#define	CHMPX_COM_STATUS_CHANGE			8					// [loop in RING] status change(notice)
#define	CHMPX_COM_MERGE_START			9					// [loop in RING] start merging
#define	CHMPX_COM_MERGE_ABORT			10					// [loop in RING] abort merging
#define	CHMPX_COM_MERGE_COMPLETE		11					// [loop in RING] complete merging
#define	CHMPX_COM_SERVER_DOWN			12					// [loop in RING] notify server down
#define	CHMPX_COM_REQ_UPDATEDATA		13					// [loop in RING] request update datas
#define	CHMPX_COM_RES_UPDATEDATA		14					// Result(Response) request update datas
#define	CHMPX_COM_RESULT_UPDATEDATA		15					// Result(Response) request update datas
#define	CHMPX_COM_MERGE_SUSPEND			16					// [loop in RING] suspend auto merging(v1.0.54)
#define	CHMPX_COM_MERGE_NOSUSPEND		17					// [loop in RING] reset(nosuspend) auto merging(v1.0.54)
#define	CHMPX_COM_MERGE_SUSPEND_GET		18					// Request suspend merging suspend at initializing(v1.0.57)
#define	CHMPX_COM_MERGE_SUSPEND_RES		19					// Response suspend merging status at initializing(v1.0.57)

#define	STRPXCOMTYPE(type)			(	CHMPX_COM_UNKNOWN			== type ? "CHMPX_COM_UNKNOWN"			: \
										CHMPX_COM_STATUS_REQ		== type ? "CHMPX_COM_STATUS_REQ"		: \
										CHMPX_COM_STATUS_RES		== type ? "CHMPX_COM_STATUS_RES"		: \
										CHMPX_COM_CONINIT_REQ		== type ? "CHMPX_COM_CONINIT_REQ"		: \
										CHMPX_COM_CONINIT_RES		== type ? "CHMPX_COM_CONINIT_RES"		: \
										CHMPX_COM_JOIN_RING			== type ? "CHMPX_COM_JOIN_RING"			: \
										CHMPX_COM_STATUS_UPDATE		== type ? "CHMPX_COM_STATUS_UPDATE"		: \
										CHMPX_COM_STATUS_CONFIRM	== type ? "CHMPX_COM_STATUS_CONFIRM"	: \
										CHMPX_COM_STATUS_CHANGE		== type ? "CHMPX_COM_STATUS_CHANGE"		: \
										CHMPX_COM_MERGE_START		== type ? "CHMPX_COM_MERGE_START"		: \
										CHMPX_COM_MERGE_ABORT		== type ? "CHMPX_COM_MERGE_ABORT"		: \
										CHMPX_COM_MERGE_COMPLETE	== type ? "CHMPX_COM_MERGE_COMPLETE"	: \
										CHMPX_COM_SERVER_DOWN		== type ? "CHMPX_COM_SERVER_DOWN"		: \
										CHMPX_COM_REQ_UPDATEDATA	== type ? "CHMPX_COM_REQ_UPDATEDATA"	: \
										CHMPX_COM_RES_UPDATEDATA	== type ? "CHMPX_COM_RES_UPDATEDATA"	: \
										CHMPX_COM_RESULT_UPDATEDATA	== type ? "CHMPX_COM_RESULT_UPDATEDATA"	: \
										CHMPX_COM_MERGE_SUSPEND		== type ? "CHMPX_COM_MERGE_SUSPEND"		: \
										CHMPX_COM_MERGE_NOSUSPEND	== type ? "CHMPX_COM_MERGE_NOSUSPEND"	: \
										CHMPX_COM_MERGE_SUSPEND_GET	== type ? "CHMPX_COM_MERGE_SUSPEND_GET"	: \
										CHMPX_COM_MERGE_SUSPEND_RES	== type ? "CHMPX_COM_MERGE_SUSPEND_RES"	: \
										"NOT_DEFINED_TYPE"	)

#define	CHMPX_COM_RES_SUCCESS			0					// no error
#define	CHMPX_COM_RES_ERROR				1					// error

//-------------------------------
// status for each chmpxid in CHMPX_COM_REQ_UPDATEDATA
//
#define	CHMPX_COM_REQ_UPDATE_INIVAL		0					// initial value
#define	CHMPX_COM_REQ_UPDATE_DO			1					// target can run to update data
#define	CHMPX_COM_REQ_UPDATE_NOACT		2					// target can not run to update data
#define	CHMPX_COM_REQ_UPDATE_SUCCESS	3					// succeed to update data
#define	CHMPX_COM_REQ_UPDATE_FAIL		4					// failed to update data

#define	IS_PXCOM_REQ_UPDATE_RESULT(res)	((CHMPX_COM_REQ_UPDATE_NOACT == res) || (CHMPX_COM_REQ_UPDATE_SUCCESS == res) || (CHMPX_COM_REQ_UPDATE_FAIL == res))
#define	STRPXCOM_REQ_UPDATEDATA(res) (	CHMPX_COM_REQ_UPDATE_INIVAL		== res ? "INIVAL"	: \
										CHMPX_COM_REQ_UPDATE_DO			== res ? "DO"		: \
										CHMPX_COM_REQ_UPDATE_NOACT		== res ? "NOACT"	: \
										CHMPX_COM_REQ_UPDATE_SUCCESS	== res ? "SUCCESS"	: \
										CHMPX_COM_REQ_UPDATE_FAIL		== res ? "FAIL"		: \
										"UNKNOWN"	)

//-------------------------------
// Structures
//
typedef struct chmpx_com_head{
	pxcomtype_t		type;					// Command type
	pxcomres_t		result;					// Command result
	size_t			length;					// Length for this packet
}CHMPX_ATTR_PACKED PXCOM_HEAD, *PPXCOM_HEAD;

typedef struct chmpx_com_status_req{
	PXCOM_HEAD		head;
}CHMPX_ATTR_PACKED PXCOM_STATUS_REQ, *PPXCOM_STATUS_REQ;

typedef struct chmpx_com_status_res{
	PXCOM_HEAD		head;
	long			count;
	off_t			pchmpxsvr_offset;
}CHMPX_ATTR_PACKED PXCOM_STATUS_RES, *PPXCOM_STATUS_RES;

typedef struct chmpx_com_coninit_req{
	PXCOM_HEAD		head;
	chmpxid_t		chmpxid;
	short			ctlport;
}CHMPX_ATTR_PACKED PXCOM_CONINIT_REQ, *PPXCOM_CONINIT_REQ;

typedef struct chmpx_com_coninit_res{
	PXCOM_HEAD		head;
}CHMPX_ATTR_PACKED PXCOM_CONINIT_RES, *PPXCOM_CONINIT_RES;

typedef struct chmpx_com_join_ring{
	PXCOM_HEAD		head;
	CHMPXSVR		server;
}CHMPX_ATTR_PACKED PXCOM_JOIN_RING, *PPXCOM_JOIN_RING;

typedef struct chmpx_com_status_update{							// same as STATUS_RES
	PXCOM_HEAD		head;
	long			count;
	off_t			pchmpxsvr_offset;
}CHMPX_ATTR_PACKED PXCOM_STATUS_UPDATE, *PPXCOM_STATUS_UPDATE;

typedef struct chmpx_com_status_confirm{						// same as STATUS_RES
	PXCOM_HEAD		head;
	long			count;
	off_t			pchmpxsvr_offset;
}CHMPX_ATTR_PACKED PXCOM_STATUS_CONFIRM, *PPXCOM_STATUS_CONFIRM;

typedef struct chmpx_com_status_change{							// same as JOIN_RING
	PXCOM_HEAD		head;
	CHMPXSVR		server;
}CHMPX_ATTR_PACKED PXCOM_STATUS_CHANGE, *PPXCOM_STATUS_CHANGE;

typedef struct chmpx_com_merge_start{							// same as STATUS_REQ
	PXCOM_HEAD		head;
}CHMPX_ATTR_PACKED PXCOM_MERGE_START, *PPXCOM_MERGE_START;

typedef struct chmpx_com_merge_abort{							// same as STATUS_REQ
	PXCOM_HEAD		head;
}CHMPX_ATTR_PACKED PXCOM_MERGE_ABORT, *PPXCOM_MERGE_ABORT;

typedef struct chmpx_com_merge_complete{						// same as STATUS_REQ
	PXCOM_HEAD		head;
}CHMPX_ATTR_PACKED PXCOM_MERGE_COMPLETE, *PPXCOM_MERGE_COMPLETE;

typedef struct chmpx_com_server_down{
	PXCOM_HEAD		head;
	chmpxid_t		chmpxid;
}CHMPX_ATTR_PACKED PXCOM_SERVER_DOWN, *PPXCOM_SERVER_DOWN;

typedef struct chmpx_com_req_idmap{
	chmpxid_t		chmpxid;
	reqidmapflag_t	req_status;
}CHMPX_ATTR_PACKED PXCOM_REQ_IDMAP, *PPXCOM_REQ_IDMAP;

typedef struct chmpx_com_req_updatedata{
	PXCOM_HEAD				head;
	PXCOMMON_MERGE_PARAM	merge_param;						// common parameter for update data
	long					count;								// extended data count
	off_t					plist_offset;						// offset for PPXCOM_REQ_IDMAP
}CHMPX_ATTR_PACKED PXCOM_REQ_UPDATEDATA, *PPXCOM_REQ_UPDATEDATA;

typedef struct chmpx_com_res_updatedata{
	PXCOM_HEAD		head;
	struct timespec	ts;											// updated time is also in this time at least
	size_t			length;
	off_t			pdata_offset;								// offset for binary data
}CHMPX_ATTR_PACKED PXCOM_RES_UPDATEDATA, *PPXCOM_RES_UPDATEDATA;

typedef struct chmpx_com_result_updatedata{
	PXCOM_HEAD		head;
	chmpxid_t		chmpxid;
	reqidmapflag_t	result;										// result
}CHMPX_ATTR_PACKED PXCOM_RESULT_UPDATEDATA, *PPXCOM_RESULT_UPDATEDATA;

typedef struct chmpx_com_merge_suspend{							// same as STATUS_REQ
	PXCOM_HEAD		head;
}CHMPX_ATTR_PACKED PXCOM_MERGE_SUSPEND, *PPXCOM_MERGE_SUSPEND;

typedef struct chmpx_com_merge_nosuspend{						// same as STATUS_REQ
	PXCOM_HEAD		head;
}CHMPX_ATTR_PACKED PXCOM_MERGE_NOSUSPEND, *PPXCOM_MERGE_NOSUSPEND;

typedef struct chmpx_com_merge_suspend_get{						// same as STATUS_REQ
	PXCOM_HEAD		head;
}CHMPX_ATTR_PACKED PXCOM_MERGE_SUSPEND_GET, *PPXCOM_MERGE_SUSPEND_GET;

typedef struct chmpx_com_merge_suspend_res{
	PXCOM_HEAD		head;
	bool			is_suspend;
}CHMPX_ATTR_PACKED PXCOM_MERGE_SUSPEND_RES, *PPXCOM_MERGE_SUSPEND_RES;

typedef union chmpx_com_all{
	PXCOM_HEAD				val_head;
	PXCOM_STATUS_REQ		val_status_req;
	PXCOM_STATUS_RES		val_status_res;
	PXCOM_CONINIT_REQ		val_coninit_req;
	PXCOM_CONINIT_RES		val_coninit_res;
	PXCOM_JOIN_RING			val_join_ring;
	PXCOM_STATUS_UPDATE		val_status_update;
	PXCOM_STATUS_CONFIRM	val_status_confirm;
	PXCOM_STATUS_CHANGE		val_status_change;
	PXCOM_MERGE_START		val_merge_start;
	PXCOM_MERGE_ABORT		val_merge_abort;
	PXCOM_MERGE_COMPLETE	val_merge_complete;
	PXCOM_SERVER_DOWN		val_server_down;
	PXCOM_REQ_UPDATEDATA	val_req_updatedata;
	PXCOM_RES_UPDATEDATA	val_res_updatedata;
	PXCOM_RESULT_UPDATEDATA	val_result_updatedata;
	PXCOM_MERGE_SUSPEND		val_merge_suspend;
	PXCOM_MERGE_NOSUSPEND	val_merge_nosuspend;
	PXCOM_MERGE_SUSPEND_GET	val_merge_suspend_get;
	PXCOM_MERGE_SUSPEND_RES	val_merge_suspend_res;
}CHMPX_ATTR_PACKED PXCOM_ALL, *PPXCOM_ALL;

//------------
// Converter
//------------
#define	HTON_PPXCOM_HEAD(pdata)	\
		{ \
			(pdata)->type	= htobe64((pdata)->type); \
			(pdata)->result	= htobe64((pdata)->result); \
			(pdata)->length	= htobe64((pdata)->length); \
		}

#define	NTOH_PPXCOM_HEAD(pdata)	\
		{ \
			(pdata)->type	= be64toh((pdata)->type); \
			(pdata)->result	= be64toh((pdata)->result); \
			(pdata)->length	= be64toh((pdata)->length); \
		}

#define	HTON_PPXCOM_STATUS_REQ(pdata)	\
		{ \
			HTON_PPXCOM_HEAD(&((pdata)->head)); \
		}

#define	NTOH_PPXCOM_STATUS_REQ(pdata)	\
		{ \
			NTOH_PPXCOM_HEAD(&((pdata)->head)); \
		}

#define	HTON_PPXCOM_STATUS_RES(pdata)	\
		{ \
			HTON_PPXCOM_HEAD(&((pdata)->head)); \
			(pdata)->count				= htobe64((pdata)->count); \
			(pdata)->pchmpxsvr_offset	= htobe64((pdata)->pchmpxsvr_offset); \
		}

#define	NTOH_PPXCOM_STATUS_RES(pdata)	\
		{ \
			NTOH_PPXCOM_HEAD(&((pdata)->head)); \
			(pdata)->count				= be64toh((pdata)->count); \
			(pdata)->pchmpxsvr_offset	= be64toh((pdata)->pchmpxsvr_offset); \
		}

#define	HTON_PPXCOM_CONINIT_REQ(pdata)	\
		{ \
			HTON_PPXCOM_HEAD(&((pdata)->head)); \
			(pdata)->chmpxid		= htobe64((pdata)->chmpxid); \
			(pdata)->ctlport		= htobe16((pdata)->ctlport); \
		}

#define	NTOH_PPXCOM_CONINIT_REQ(pdata)	\
		{ \
			NTOH_PPXCOM_HEAD(&((pdata)->head)); \
			(pdata)->chmpxid		= be64toh((pdata)->chmpxid); \
			(pdata)->ctlport		= be16toh((pdata)->ctlport); \
		}

#define	HTON_PPXCOM_CONINIT_RES(pdata)	\
		{ \
			HTON_PPXCOM_HEAD(&((pdata)->head)); \
		}

#define	NTOH_PPXCOM_CONINIT_RES(pdata)	\
		{ \
			NTOH_PPXCOM_HEAD(&((pdata)->head)); \
		}

#define	HTON_PPXCOM_JOIN_RING(pdata)	\
		{ \
			HTON_PPXCOM_HEAD(&((pdata)->head)); \
			HTON_PCHMPXSVR(&((pdata)->server)); \
		}

#define	NTOH_PPXCOM_JOIN_RING(pdata)	\
		{ \
			NTOH_PPXCOM_HEAD(&((pdata)->head)); \
			NTOH_PCHMPXSVR(&((pdata)->server)); \
		}

#define	HTON_PPXCOM_STATUS_UPDATE(pdata)	\
		{ \
			HTON_PPXCOM_HEAD(&((pdata)->head)); \
			(pdata)->count				= htobe64((pdata)->count); \
			(pdata)->pchmpxsvr_offset	= htobe64((pdata)->pchmpxsvr_offset); \
		}

#define	NTOH_PPXCOM_STATUS_UPDATE(pdata)	\
		{ \
			NTOH_PPXCOM_HEAD(&((pdata)->head)); \
			(pdata)->count				= be64toh((pdata)->count); \
			(pdata)->pchmpxsvr_offset	= be64toh((pdata)->pchmpxsvr_offset); \
		}

#define	HTON_PPXCOM_STATUS_CONFIRM(pdata)	\
		{ \
			HTON_PPXCOM_HEAD(&((pdata)->head)); \
			(pdata)->count				= htobe64((pdata)->count); \
			(pdata)->pchmpxsvr_offset	= htobe64((pdata)->pchmpxsvr_offset); \
		}

#define	NTOH_PPXCOM_STATUS_CONFIRM(pdata)	\
		{ \
			NTOH_PPXCOM_HEAD(&((pdata)->head)); \
			(pdata)->count				= be64toh((pdata)->count); \
			(pdata)->pchmpxsvr_offset	= be64toh((pdata)->pchmpxsvr_offset); \
		}

#define	HTON_PPXCOM_STATUS_CHANGE(pdata)	\
		{ \
			HTON_PPXCOM_HEAD(&((pdata)->head)); \
			HTON_PCHMPXSVR(&((pdata)->server)); \
		}

#define	NTOH_PPXCOM_STATUS_CHANGE(pdata)	\
		{ \
			NTOH_PPXCOM_HEAD(&((pdata)->head)); \
			NTOH_PCHMPXSVR(&((pdata)->server)); \
		}

#define	HTON_PPXCOM_MERGE_START(pdata)	\
		{ \
			HTON_PPXCOM_HEAD(&((pdata)->head)); \
		}

#define	NTOH_PPXCOM_MERGE_START(pdata)	\
		{ \
			NTOH_PPXCOM_HEAD(&((pdata)->head)); \
		}

#define	HTON_PPXCOM_MERGE_ABORT(pdata)	\
		{ \
			HTON_PPXCOM_HEAD(&((pdata)->head)); \
		}

#define	NTOH_PPXCOM_MERGE_ABORT(pdata)	\
		{ \
			NTOH_PPXCOM_HEAD(&((pdata)->head)); \
		}

#define	HTON_PPXCOM_MERGE_COMPLETE(pdata)	\
		{ \
			HTON_PPXCOM_HEAD(&((pdata)->head)); \
		}

#define	NTOH_PPXCOM_MERGE_COMPLETE(pdata)	\
		{ \
			NTOH_PPXCOM_HEAD(&((pdata)->head)); \
		}

#define	HTON_PPXCOM_SERVER_DOWN(pdata)	\
		{ \
			HTON_PPXCOM_HEAD(&((pdata)->head)); \
			(pdata)->chmpxid= htobe64((pdata)->chmpxid); \
		}

#define	NTOH_PPXCOM_SERVER_DOWN(pdata)	\
		{ \
			NTOH_PPXCOM_HEAD(&((pdata)->head)); \
			(pdata)->chmpxid= be64toh((pdata)->chmpxid); \
		}

#define	HTON_PPXCOM_REQ_IDMAP(pdata)	\
		{ \
			(pdata)->chmpxid	= htobe64((pdata)->chmpxid);	\
			(pdata)->req_status	= htobe64((pdata)->req_status);	\
		}

#define	NTOH_PPXCOM_REQ_IDMAP(pdata)	\
		{ \
			(pdata)->chmpxid	= be64toh((pdata)->chmpxid);	\
			(pdata)->req_status	= be64toh((pdata)->req_status);	\
		}

#define	HTON_PXCOMMON_MERGE_PARAM(pdata)	\
		{ \
			(pdata)->chmpxid			= htobe64((pdata)->chmpxid);			\
			HTON_TIMESPEC(&((pdata)->startts));									\
			(pdata)->max_pending_hash	= htobe64((pdata)->max_pending_hash);	\
			(pdata)->pending_hash		= htobe64((pdata)->pending_hash);		\
			(pdata)->max_base_hash		= htobe64((pdata)->max_base_hash);		\
			(pdata)->base_hash			= htobe64((pdata)->base_hash);			\
			(pdata)->replica_count		= htobe64((pdata)->replica_count);		\
		}

#define	NTOH_PXCOMMON_MERGE_PARAM(pdata)	\
		{ \
			(pdata)->chmpxid			= be64toh((pdata)->chmpxid);			\
			NTOH_TIMESPEC(&((pdata)->startts));									\
			(pdata)->max_pending_hash	= be64toh((pdata)->max_pending_hash);	\
			(pdata)->pending_hash		= be64toh((pdata)->pending_hash);		\
			(pdata)->max_base_hash		= be64toh((pdata)->max_base_hash);		\
			(pdata)->base_hash			= be64toh((pdata)->base_hash);			\
			(pdata)->replica_count		= be64toh((pdata)->replica_count);		\
		}

#define	HTON_PPXCOM_REQ_UPDATEDATA(pdata)	\
		{ \
			HTON_PPXCOM_HEAD(&((pdata)->head));									\
			HTON_PXCOMMON_MERGE_PARAM(&((pdata)->merge_param));					\
			(pdata)->count				= htobe64((pdata)->count);				\
			(pdata)->plist_offset		= htobe64((pdata)->plist_offset);		\
		}

#define	NTOH_PPXCOM_REQ_UPDATEDATA(pdata)	\
		{ \
			NTOH_PPXCOM_HEAD(&((pdata)->head)); 								\
			NTOH_PXCOMMON_MERGE_PARAM(&((pdata)->merge_param));					\
			(pdata)->count				= be64toh((pdata)->count);				\
			(pdata)->plist_offset		= be64toh((pdata)->plist_offset);		\
		}

#define	HTON_PPXCOM_RES_UPDATEDATA(pdata)	\
		{ \
			HTON_PPXCOM_HEAD(&((pdata)->head));									\
			HTON_TIMESPEC(&((pdata)->ts)); 										\
			(pdata)->length				= htobe64((pdata)->length);				\
			(pdata)->pdata_offset		= htobe64((pdata)->pdata_offset);		\
		}

#define	NTOH_PPXCOM_RES_UPDATEDATA(pdata)	\
		{ \
			NTOH_PPXCOM_HEAD(&((pdata)->head)); 								\
			NTOH_TIMESPEC(&((pdata)->ts)); 										\
			(pdata)->length				= be64toh((pdata)->length);				\
			(pdata)->pdata_offset		= be64toh((pdata)->pdata_offset);		\
		}

#define	HTON_PPXCOM_RESULT_UPDATEDATA(pdata)	\
		{ \
			HTON_PPXCOM_HEAD(&((pdata)->head));									\
			(pdata)->chmpxid			= htobe64((pdata)->chmpxid);			\
			(pdata)->result				= htobe64((pdata)->result);				\
		}

#define	NTOH_PPXCOM_RESULT_UPDATEDATA(pdata)	\
		{ \
			NTOH_PPXCOM_HEAD(&((pdata)->head)); 								\
			(pdata)->chmpxid			= be64toh((pdata)->chmpxid);			\
			(pdata)->result				= be64toh((pdata)->result);				\
		}

#define	HTON_PPXCOM_MERGE_SUSPEND(pdata)		\
		{ \
			HTON_PPXCOM_HEAD(&((pdata)->head)); \
		}

#define	NTOH_PPXCOM_MERGE_SUSPEND(pdata)		\
		{ \
			NTOH_PPXCOM_HEAD(&((pdata)->head)); \
		}

#define	HTON_PPXCOM_MERGE_NOSUSPEND(pdata)		\
		{ \
			HTON_PPXCOM_HEAD(&((pdata)->head)); \
		}

#define	NTOH_PPXCOM_MERGE_NOSUSPEND(pdata)		\
		{ \
			NTOH_PPXCOM_HEAD(&((pdata)->head)); \
		}

#define	HTON_PPXCOM_MERGE_SUSPEND_GET(pdata)	\
		{ \
			HTON_PPXCOM_HEAD(&((pdata)->head)); \
		}

#define	NTOH_PPXCOM_MERGE_SUSPEND_GET(pdata)	\
		{ \
			NTOH_PPXCOM_HEAD(&((pdata)->head)); \
		}

// is_suspend is bool(8bit) which is not needed to change byte order
#define	HTON_PPXCOM_MERGE_SUSPEND_RES(pdata)	\
		{ \
			HTON_PPXCOM_HEAD(&((pdata)->head)); \
		}

// is_suspend is bool(8bit) which is not needed to change byte order
#define	NTOH_PPXCOM_MERGE_SUSPEND_RES(pdata)	\
		{ \
			NTOH_PPXCOM_HEAD(&((pdata)->head)); \
		}

//---------------------------------------------------------
// Communication Packets
//---------------------------------------------------------
// Following pxcomtype_t is long(64) instead of enum because 
// PXCOM_HEAD should be alignment for datas after it.
//
typedef uint64_t			comtype_t;			// type for packet
typedef uint64_t			c2ctype_t;			// COM_C2C type for duplication
typedef uint64_t			comver_t;			// Protocol version

// Version
#define	COM_VERSION_1		1L					// First version 1

// Type for packet
#define	COM_UNKNOWN			0					// Unknown
#define	COM_C2C				1					// Client Process to Client Process
#define	COM_PX2PX			2					// CHMPX to CHMPX(Internal communication)
#define	COM_S2PX			3					// Signal to CHMPX
#define	COM_F2PX			4					// Inotify to CHMPX
#define	COM_C2PX			5					// Client Process to CHMPX(Notification from Client process)
#define	COM_PX2C			6					// CHMPX to Client Process(Notification from CHMPX)

#define	STRCOMTYPE(type)	(	COM_UNKNOWN	== type ? "COM_UNKNOWN" : \
								COM_C2C		== type ? "COM_C2C" : \
								COM_PX2PX	== type ? "COM_PX2PX" : \
								COM_S2PX	== type ? "COM_S2PX" : \
								COM_F2PX	== type ? "COM_F2PX" : \
								COM_C2PX	== type ? "COM_C2PX" : \
								COM_PX2C	== type ? "COM_PX2C" : "NOT_DEFINED_TYPE"	)

// COM_C2C Type for duplication
#define	COM_C2C_IGNORE				0x0			// ignore(not COM_C2C packet)
#define	COM_C2C_NORMAL				0x1			// send to single routing
#define	COM_C2C_ROUTING				0x2			// send to duplicated routing
#define	COM_C2C_BROADCAST			0x3			// send broadcast
#define	COM_C2C_RBROADCAST			0x4			// send broadcast to duplicate in routing
#define	COM_C2C_MODE_MASK			0x7			// mask for C2C TYPE
#define	COM_C2C_NOT_SELF			0x10		// flag for excepting self

#define	GET_C2CTYPE(type)			(COM_C2C_MODE_MASK & type)
#define	SET_C2CTYPE_SELF(type)		(type = GET_C2CTYPE(type))
#define	SET_C2CTYPE_NOT_SELF(type)	(type = GET_C2CTYPE(type) | COM_C2C_NOT_SELF)
#define	IS_C2CTYPE_IGNORE(type)		(COM_C2C_IGNORE		== GET_C2CTYPE(type))
#define	IS_C2CTYPE_NORMAL(type)		(COM_C2C_NORMAL		== GET_C2CTYPE(type))
#define	IS_C2CTYPE_ROUTING(type)	(COM_C2C_ROUTING	== GET_C2CTYPE(type))
#define	IS_C2CTYPE_BROADCAST(type)	(COM_C2C_BROADCAST	== GET_C2CTYPE(type))
#define	IS_C2CTYPE_RBROADCAST(type)	(COM_C2C_RBROADCAST	== GET_C2CTYPE(type))
#define	IS_C2CTYPE_SELF(type)		(COM_C2C_NOT_SELF	!= (COM_C2C_NOT_SELF & type))
#define	IS_C2CTYPE_NOT_SELF(type)	(COM_C2C_NOT_SELF	== (COM_C2C_NOT_SELF & type))
#define	STRCOMC2CTYPE(type)			(	IS_C2CTYPE_IGNORE(type)		? 								"COM_C2C_IGNORE"										: \
										IS_C2CTYPE_NORMAL(type)		? ((IS_C2CTYPE_SELF(type)	?	"COM_C2C_NORMAL"	: "COM_C2C_NORMAL_NOT_SELF"))		: \
										IS_C2CTYPE_ROUTING(type)	? ((IS_C2CTYPE_SELF(type)	?	"COM_C2C_ROUTING"	: "COM_C2C_ROUTING_NOT_SELF"))		: \
										IS_C2CTYPE_BROADCAST(type)	? ((IS_C2CTYPE_SELF(type)	?	"COM_C2C_BROADCAST"	: "COM_C2C_BROADCAST_NOT_SELF"))	: \
										IS_C2CTYPE_RBROADCAST(type)	? ((IS_C2CTYPE_SELF(type)	?	"COM_C2C_RBROADCAST": "COM_C2C_RBROADCAST_NOT_SELF"))	: "NOT_DEFINED_TYPE" )

typedef struct packed_ids{
	chmpxid_t		chmpxid;
	msgid_t			msgid;
}CHMPX_ATTR_PACKED PCKID, *PPCKID;

typedef struct com_head{
	comver_t		version;				// version
	comtype_t		type;
	c2ctype_t		c2ctype;
	PCKID			dept_ids;				// "From" of End to end(departure)
	PCKID			term_ids;				// "To" of End to end(terminus)
	msgid_t			peer_dept_msgid;		// "From" of Peer to Peer
	msgid_t			peer_term_msgid;		// "To" of Peer to Peer
	serial_t		mq_serial_num;			// serial number on MQ
	chmhash_t		hash;
	struct timespec	reqtime;
}CHMPX_ATTR_PACKED COMHEAD, *PCOMHEAD;

typedef struct com_packet{
	COMHEAD			head;
	size_t			length;					// this structure length and detail data length after this structure
	off_t			offset;					// offset for detail data after this structure from this structure top
}CHMPX_ATTR_PACKED COMPKT, *PCOMPKT;

//------------
// Macros
//------------
#define	COPY_PKID(pdest, psrc)	\
		{ \
			(pdest)->chmpxid	= (psrc)->chmpxid; \
			(pdest)->msgid		= (psrc)->msgid; \
		}

#define	COPY_COMHEAD(pdest, psrc)	\
		{ \
			COPY_PKID(&((pdest)->dept_ids), &((psrc)->dept_ids)); \
			COPY_PKID(&((pdest)->term_ids), &((psrc)->term_ids)); \
			(pdest)->version			= (psrc)->version; \
			(pdest)->type				= (psrc)->type; \
			(pdest)->c2ctype			= (psrc)->c2ctype; \
			(pdest)->peer_dept_msgid	= (psrc)->peer_dept_msgid; \
			(pdest)->peer_term_msgid	= (psrc)->peer_term_msgid; \
			(pdest)->mq_serial_num		= (psrc)->mq_serial_num; \
			(pdest)->hash				= (psrc)->hash; \
			COPY_TIMESPEC(&((pdest)->reqtime), &((psrc)->reqtime)); \
		}

#define	COPY_COMPKT(pdest, psrc)	\
		{ \
			COPY_COMHEAD(&((pdest)->head), &((psrc)->head));	\
			(pdest)->length	= (psrc)->length; \
			(pdest)->offset	= (psrc)->offset; \
		}

//------------
// Converter
//------------
#define	HTON_PPCKID(pdata)	\
		{ \
			(pdata)->chmpxid= htobe64((pdata)->chmpxid); \
			(pdata)->msgid	= htobe64((pdata)->msgid); \
		}

#define	NTOH_PPCKID(pdata)	\
		{ \
			(pdata)->chmpxid= be64toh((pdata)->chmpxid); \
			(pdata)->msgid	= be64toh((pdata)->msgid); \
		}

#define	HTON_TIMESPEC(ptr)	\
		{ \
			(ptr)->tv_sec	= htobe64((ptr)->tv_sec); \
			(ptr)->tv_nsec	= htobe64((ptr)->tv_nsec); \
		} \

#define	NTOH_TIMESPEC(ptr)	\
		{ \
			(ptr)->tv_sec	= be64toh((ptr)->tv_sec); \
			(ptr)->tv_nsec	= be64toh((ptr)->tv_nsec); \
		} \

#define	HTON_PCOMHEAD(pdata)	\
		{ \
			HTON_PPCKID(&((pdata)->dept_ids)); \
			HTON_PPCKID(&((pdata)->term_ids)); \
			(pdata)->version			= htobe64((pdata)->version); \
			(pdata)->type				= htobe64((pdata)->type); \
			(pdata)->c2ctype			= htobe64((pdata)->c2ctype); \
			(pdata)->peer_dept_msgid	= htobe64((pdata)->peer_dept_msgid); \
			(pdata)->peer_term_msgid	= htobe64((pdata)->peer_term_msgid); \
			(pdata)->mq_serial_num		= htobe64((pdata)->mq_serial_num); \
			(pdata)->hash				= htobe64((pdata)->hash); \
			HTON_TIMESPEC(&((pdata)->reqtime)); \
		}

#define	NTOH_PCOMHEAD(pdata)	\
		{ \
			NTOH_PPCKID(&((pdata)->dept_ids)); \
			NTOH_PPCKID(&((pdata)->term_ids)); \
			(pdata)->version			= be64toh((pdata)->version); \
			(pdata)->type				= be64toh((pdata)->type); \
			(pdata)->c2ctype			= be64toh((pdata)->c2ctype); \
			(pdata)->peer_dept_msgid	= be64toh((pdata)->peer_dept_msgid); \
			(pdata)->peer_term_msgid	= be64toh((pdata)->peer_term_msgid); \
			(pdata)->mq_serial_num		= be64toh((pdata)->mq_serial_num); \
			(pdata)->hash				= be64toh((pdata)->hash); \
			NTOH_TIMESPEC(&((pdata)->reqtime)); \
		}

#define	HTON_PCOMPKT(pdata)	\
		{ \
			HTON_PCOMHEAD(&((pdata)->head)); \
			(pdata)->length	= htobe64((pdata)->length); \
			(pdata)->offset	= htobe64((pdata)->offset); \
		}

#define	NTOH_PCOMPKT(pdata)	\
		{ \
			NTOH_PCOMHEAD(&((pdata)->head)); \
			(pdata)->length	= be64toh((pdata)->length); \
			(pdata)->offset	= be64toh((pdata)->offset); \
		}

//------------
// Utility Macros
//------------
#define	CVT_COMPTR_STATUS_REQ(pComAll)			&((pComAll)->val_status_req)
#define	CVT_COMPTR_STATUS_RES(pComAll)			&((pComAll)->val_status_res)
#define	CVT_COMPTR_CONINIT_REQ(pComAll)			&((pComAll)->val_coninit_req)
#define	CVT_COMPTR_CONINIT_RES(pComAll)			&((pComAll)->val_coninit_res)
#define	CVT_COMPTR_JOIN_RING(pComAll)			&((pComAll)->val_join_ring)
#define	CVT_COMPTR_STATUS_UPDATE(pComAll)		&((pComAll)->val_status_update)
#define	CVT_COMPTR_STATUS_CONFIRM(pComAll)		&((pComAll)->val_status_confirm)
#define	CVT_COMPTR_STATUS_CHANGE(pComAll)		&((pComAll)->val_status_change)
#define	CVT_COMPTR_MERGE_START(pComAll)			&((pComAll)->val_merge_start)
#define	CVT_COMPTR_MERGE_ABORT(pComAll)			&((pComAll)->val_merge_abort)
#define	CVT_COMPTR_MERGE_COMPLETE(pComAll)		&((pComAll)->val_merge_complete)
#define	CVT_COMPTR_SERVER_DOWN(pComAll)			&((pComAll)->val_server_down)
#define	CVT_COMPTR_REQ_UPDATEDATA(pComAll)		&((pComAll)->val_req_updatedata)
#define	CVT_COMPTR_RES_UPDATEDATA(pComAll)		&((pComAll)->val_res_updatedata)
#define	CVT_COMPTR_RESULT_UPDATEDATA(pComAll)	&((pComAll)->val_result_updatedata)
#define	CVT_COMPTR_MERGE_SUSPEND(pComAll)		&((pComAll)->val_merge_suspend)
#define	CVT_COMPTR_MERGE_NOSUSPEND(pComAll)		&((pComAll)->val_merge_nosuspend)
#define	CVT_COMPTR_MERGE_SUSPEND_GET(pComAll)	&((pComAll)->val_merge_suspend_get)
#define	CVT_COMPTR_MERGE_SUSPEND_RES(pComAll)	&((pComAll)->val_merge_suspend_res)

#define	SIZEOF_CHMPX_COM(type)					(	CHMPX_COM_STATUS_REQ		 == type ? sizeof(PXCOM_STATUS_REQ)			: \
													CHMPX_COM_STATUS_RES		 == type ? sizeof(PXCOM_STATUS_RES)			: \
													CHMPX_COM_CONINIT_REQ		 == type ? sizeof(PXCOM_CONINIT_REQ)		: \
													CHMPX_COM_CONINIT_RES		 == type ? sizeof(PXCOM_CONINIT_RES)		: \
													CHMPX_COM_JOIN_RING			 == type ? sizeof(PXCOM_JOIN_RING)			: \
													CHMPX_COM_STATUS_UPDATE		 == type ? sizeof(PXCOM_STATUS_UPDATE)		: \
													CHMPX_COM_STATUS_CONFIRM	 == type ? sizeof(PXCOM_STATUS_CONFIRM)		: \
													CHMPX_COM_STATUS_CHANGE		 == type ? sizeof(PXCOM_STATUS_CHANGE)		: \
													CHMPX_COM_MERGE_START		 == type ? sizeof(PXCOM_MERGE_START)		: \
													CHMPX_COM_MERGE_ABORT		 == type ? sizeof(PXCOM_MERGE_ABORT)		: \
													CHMPX_COM_MERGE_COMPLETE	 == type ? sizeof(PXCOM_MERGE_COMPLETE)		: \
													CHMPX_COM_SERVER_DOWN		 == type ? sizeof(PXCOM_SERVER_DOWN)		: \
													CHMPX_COM_REQ_UPDATEDATA	 == type ? sizeof(PXCOM_REQ_UPDATEDATA)		: \
													CHMPX_COM_RES_UPDATEDATA	 == type ? sizeof(PXCOM_RES_UPDATEDATA)		: \
													CHMPX_COM_RESULT_UPDATEDATA	 == type ? sizeof(PXCOM_RESULT_UPDATEDATA)	: \
													CHMPX_COM_MERGE_SUSPEND		 == type ? sizeof(PXCOM_MERGE_SUSPEND)		: \
													CHMPX_COM_MERGE_NOSUSPEND	 == type ? sizeof(PXCOM_MERGE_NOSUSPEND)	: \
													CHMPX_COM_MERGE_SUSPEND_GET	 == type ? sizeof(PXCOM_MERGE_SUSPEND_GET)	: \
													CHMPX_COM_MERGE_SUSPEND_RES	 == type ? sizeof(PXCOM_MERGE_SUSPEND_RES)	: \
													0L)

#define	CVT_COM_ALL_PTR_PXCOMPKT(pComPkt)		(	CHM_OFFSET((pComPkt), sizeof(COMPKT), PPXCOM_ALL)	)

#define	SET_PXCOMPKT(pComPkt, comtype, dept_chmpxid, term_chmpxid, is_timespec, extlength)	\
		{ \
			(pComPkt)->length				= sizeof(COMPKT) + SIZEOF_CHMPX_COM(comtype) + extlength; \
			(pComPkt)->offset				= sizeof(COMPKT); \
			(pComPkt)->head.version			= COM_VERSION_1; \
			(pComPkt)->head.type			= COM_PX2PX; \
			(pComPkt)->head.c2ctype			= COM_C2C_IGNORE; \
			(pComPkt)->head.dept_ids.chmpxid= dept_chmpxid; \
			(pComPkt)->head.dept_ids.msgid	= CHM_INVALID_MSGID; \
			(pComPkt)->head.term_ids.chmpxid= term_chmpxid; \
			(pComPkt)->head.term_ids.msgid	= CHM_INVALID_MSGID; \
			(pComPkt)->head.peer_dept_msgid	= CHM_INVALID_MSGID; \
			(pComPkt)->head.peer_term_msgid	= CHM_INVALID_MSGID; \
			(pComPkt)->head.mq_serial_num	= MIN_MQ_SERIAL_NUMBER; \
			(pComPkt)->head.hash			= CHM_INVALID_HASHVAL; \
			if(is_timespec){ \
				if(!RT_TIMESPEC(&((pComPkt)->head.reqtime))){ \
					WAN_CHMPRN("Could not get timespec(errno=%d), but continue...", errno); \
					INIT_TIMESPEC(&((pComPkt)->head.reqtime)); \
				} \
			}else{ \
				INIT_TIMESPEC(&((pComPkt)->head.reqtime)); \
			} \
		}

//------------
// Debug Macros
//------------
#define	DUMPCOM_COMPKT_TYPE(action, pComPkt)	\
		if(chm_debug_mode >= CHMDBG_DUMP){ \
			if(COM_PX2PX == (pComPkt)->head.type){ \
				PPXCOM_ALL	pComAll = CVT_COM_ALL_PTR_PXCOMPKT((pComPkt)); \
				MSG_CHMPRN("%s COMPKT type(%s) : PXCOM_ALL type(%s).", action ? action : "", STRCOMTYPE((pComPkt)->head.type), STRPXCOMTYPE(pComAll->val_head.type)); \
			}else if(COM_C2C == (pComPkt)->head.type){ \
				MSG_CHMPRN("%s COMPKT type(%s) : COM_C2C type(%s).", action ? action : "", STRCOMTYPE((pComPkt)->head.type), STRCOMC2CTYPE((pComPkt)->head.c2ctype)); \
			}else{ \
				MSG_CHMPRN("%s COMPKT type(%s).", action ? action : "", STRCOMTYPE((pComPkt)->head.type)); \
			} \
		}

#define	DUMPCOM_COMPKT(headmsg, pComPkt)	\
		if(chm_debug_mode >= CHMDBG_DUMP){ \
			fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "DUMP COMPKT(%s) = {\n",					headmsg ? headmsg : "notag"); \
			fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "  head              = {\n"); \
			fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "    version         = 0x%016" PRIx64 "\n",	(pComPkt)->head.version); \
			fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "    type            = 0x%016" PRIx64 "\n",	(pComPkt)->head.type); \
			fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "    c2ctype         = 0x%016" PRIx64 "\n",	(pComPkt)->head.c2ctype); \
			fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "    dept_ids        = {\n"); \
			fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "      chmpxid       = 0x%016" PRIx64 "\n",	(pComPkt)->head.dept_ids.chmpxid); \
			fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "      msgid         = 0x%016" PRIx64 "\n",	(pComPkt)->head.dept_ids.msgid); \
			fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "    }\n"); \
			fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "    term_ids        = {\n"); \
			fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "      chmpxid       = 0x%016" PRIx64 "\n",	(pComPkt)->head.term_ids.chmpxid); \
			fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "      msgid         = 0x%016" PRIx64 "\n",	(pComPkt)->head.term_ids.msgid); \
			fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "    }\n"); \
			fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "    peer_dept_msgid = 0x%016" PRIx64 "\n",	(pComPkt)->head.peer_dept_msgid); \
			fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "    peer_term_msgid = 0x%016" PRIx64 "\n",	(pComPkt)->head.peer_term_msgid); \
			fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "    mq_serial_num   = 0x%016" PRIx64 "\n",	(pComPkt)->head.mq_serial_num); \
			fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "    hash            = 0x%016" PRIx64 "\n",	(pComPkt)->head.hash); \
			fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "    reqtime         = {\n"); \
			fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "      tv_sec        = %jd\n",				static_cast<intmax_t>((pComPkt)->head.reqtime.tv_sec)); \
			fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "      tv_nsec       = %ld\n",				(pComPkt)->head.reqtime.tv_nsec); \
			fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "    }\n"); \
			fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "  }\n"); \
			fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "  length            = %zu\n",				(pComPkt)->length); \
			fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "  offset            = %jd\n",				static_cast<intmax_t>((pComPkt)->offset)); \
			fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "}\n"); \
		}

#define	DUMPCOM_PXCLT(headmsg, pCltAll)	\
		if(chm_debug_mode >= CHMDBG_DUMP){ \
			fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "DUMP PXCTL(%s) = {\n",						headmsg ? headmsg : "notag"); \
			fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "  val_head          = {\n"); \
			fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "    type            = 0x%016" PRIx64 "\n",	(pCltAll)->val_head.type); \
			fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "    length          = 0x%016" PRIx64 "\n",	(pCltAll)->val_head.length); \
			fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "  }\n"); \
			fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "}\n"); \
		}

#endif	// CHMCOMSTRUCTURE_H

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
