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
#ifndef	CHMSTRUCTURE_H
#define	CHMSTRUCTURE_H

#include <netdb.h>
#include <k2hash/k2hash.h>

#include "chmcommon.h"

DECL_EXTERN_C_START

//---------------------------------------------------------
// Variable types
//---------------------------------------------------------
typedef uint64_t		chmpxid_t;
typedef uint64_t		msgid_t;
typedef uint64_t		serial_t;
typedef unsigned char	msgflag_t;
typedef k2h_hash_t		chmhash_t;
typedef uint64_t		chmpxsts_t;
typedef uint64_t		chmpxpos_t;
typedef uint64_t		logtype_t;
typedef int				chmss_ver_t;				// SSL/TLS minimum version

typedef enum chmpx_mode{
	CHMPX_UNKNOWN = 0,
	CHMPX_SERVER,
	CHMPX_SLAVE
}CHMPXMODE;

typedef enum chmpxid_seed_type{
	CHMPXID_SEED_NAME = 0,							// default, seed from name and control port(before v1.0.71)
	CHMPXID_SEED_CUK,								// seed from cuk
	CHMPXID_SEED_CTLENDPOINTS,						// seed from control endpoints(no particular order)
	CHMPXID_SEED_CUSTOM								// seed from CUSTOM_ID_SEED
}CHMPXID_SEED_TYPE;

//---------------------------------------------------------
// Symbols for CHMINFO structure version
//---------------------------------------------------------
// Version 1.0	after chmpx version 1.0.59
// Version 1.1	after chmpx version 1.0.71
//
// [NOTE]
// Before CHMPX version 1.0.59, those do not have the CHMINFO structure version.
//
#define	CHM_CHMINFO_VERSION_BUFLEN		32
#define	CHM_CHMINFO_VERSION_PREFIX		"CHMINFO_VERSION"
#define	CHM_CHMINFO_OLD_VERSION_1_0		"1.0"
#define	CHM_CHMINFO_CUR_VERSION			"1.1"
#define	CHM_CHMINFO_CUR_VERSION_STR		CHM_CHMINFO_VERSION_PREFIX " " CHM_CHMINFO_CUR_VERSION

//---------------------------------------------------------
// Symbols for SSL/TLS minimum version
//---------------------------------------------------------
// [NOTE]
// These symbols is used in chmconf.cc for analyzing option.
//
#define	CHM_SSLTLS_VER_ERROR				-1
#define	CHM_SSLTLS_VER_DEFAULT				0
#define	CHM_SSLTLS_VER_SSLV2				(CHM_SSLTLS_VER_DEFAULT	+ 1)
#define	CHM_SSLTLS_VER_SSLV3				(CHM_SSLTLS_VER_SSLV2	+ 1)
#define	CHM_SSLTLS_VER_TLSV1_0				(CHM_SSLTLS_VER_SSLV3	+ 1)
#define	CHM_SSLTLS_VER_TLSV1_1				(CHM_SSLTLS_VER_TLSV1_0	+ 1)
#define	CHM_SSLTLS_VER_TLSV1_2				(CHM_SSLTLS_VER_TLSV1_1	+ 1)
#define	CHM_SSLTLS_VER_TLSV1_3				(CHM_SSLTLS_VER_TLSV1_2	+ 1)
#define	CHM_SSLTLS_VER_MAX					CHM_SSLTLS_VER_TLSV1_3

#define	CHM_GET_STR_SSLTLS_VERSION(ver)	(	CHM_SSLTLS_VER_DEFAULT	== ver ? "(default)":			\
											CHM_SSLTLS_VER_SSLV2	== ver ? "SSL v2"	:			\
											CHM_SSLTLS_VER_SSLV3	== ver ? "SSL v3"	:			\
											CHM_SSLTLS_VER_TLSV1_0	== ver ? "TLS v1.0"	:			\
											CHM_SSLTLS_VER_TLSV1_1	== ver ? "TLS v1.1"	:			\
											CHM_SSLTLS_VER_TLSV1_2	== ver ? "TLS v1.2"	:			\
											CHM_SSLTLS_VER_TLSV1_3	== ver ? "TLS v1.3"	: "Unknown"	\
										)

//---------------------------------------------------------
// Symbols & Macros
//---------------------------------------------------------
#define	CHM_INVALID_ID					(-1)
#define	CHM_INVALID_PORT				(-1)
#define	CHM_INVALID_CHMPXLISTPOS		0L
#define	CHM_INVALID_PID					0

DECL_EXTERN_C_END

#if defined(__cplusplus)
#define	CHM_INVALID_CHMPXID				static_cast<chmpxid_t>(CHM_INVALID_ID)
#define	CHM_INVALID_MSGID				static_cast<msgid_t>(CHM_INVALID_ID)
#define	CHM_INVALID_HASHVAL				static_cast<chmhash_t>(-1)	// when hash range is under 64bit.
#else	// __cplusplus
#define	CHM_INVALID_CHMPXID				(chmpxid_t)(CHM_INVALID_ID)
#define	CHM_INVALID_MSGID				(msgid_t)(CHM_INVALID_ID)
#define	CHM_INVALID_HASHVAL				(chmhash_t)(-1)				// when hash range is under 64bit.
#endif	// __cplusplus

DECL_EXTERN_C_START

#define	MAX_GROUP_LENGTH				256
#define	DEFAULT_CHMPX_COUNT				1024					// default maximum node(server & slave) count
#define	MAX_CHMPX_COUNT					(1024 * 32)				// maximum node(server & slave) count
#define	DEFAULT_REPLICA_COUNT			0						// default replica count(always random mode sets this value)
#define	CHMPXID_MAP_SIZE				4096					// chmpxid map array size(maximum average conflict MAX_CHMPX_COUNT / CHMPXID_MAP_SIZE)
#define	CHMPXID_MAP_MASK				(CHMPXID_MAP_SIZE - 1)	// 0xFFF

#define	MAX_MQUEUE_COUNT				32768											// Maximum MQ count in boxes
#define	MAX_SERVER_MQ_CNT				128												// Maximum MQ count server used
#define	DEFAULT_SERVER_MQ_CNT			1												// 
#define	MAX_CLIENT_MQ_CNT				(MAX_MQUEUE_COUNT - MAX_SERVER_MQ_CNT)			// Maximum MQ count all client processes used
#define	DEFAULT_CLIENT_MQ_CNT			8192											// 
#define	MAX_QUEUE_PER_SERVERMQ			128												// Maximum Queue count by each server/slave node MQ
#define	DEFAULT_QUEUE_PER_SERVERMQ		8												// 
#define	MAX_QUEUE_PER_CLIENTMQ			128												// Maximum Queue count by each client processes MQ
#define	DEFAULT_QUEUE_PER_CLIENTMQ		1												// 
#define	MAX_MQ_PER_CLIENT				MAX_SERVER_MQ_CNT								// Maximum MQ count by each client processes
#define	DEFAULT_MQ_PER_CLIENT			1												// 
#define	MAX_MQ_PER_ATTACH				MAX_SERVER_MQ_CNT								// Maximum MQ count by attaching
#define	DEFAULT_MQ_PER_ATTACH			1												// 
#define	DEFAULT_MQUEUE_COUNT			(DEFAULT_CLIENT_MQ_CNT + DEFAULT_SERVER_MQ_CNT)	// Default MQ count in boxes

#define	DEFAULT_HISTLOG_COUNT			8192					// default
#define	MAX_HISTLOG_COUNT				32768					// upper limit

//---------------------
// CHMPX Status(chmpxsts_t)
//---------------------
// Status value is following bit parts.
// 
//-------------------------------------------------------------------------------
// 	LIVE		RING/SERVICE	ACTION		OPERATE		SUSPEND(NOT JOIN CLIENT)
//-------------------------------------------------------------------------------
// 	UP			SLAVE			NOACT		NOTHING		NOSUP
// 	DOWN		SERVICE IN		ADD			PENDING		SUSPEND
// 				SERVICE OUT		DELETE		DOING
// 											DONE
//-------------------------------------------------------------------------------
//
#define	CHMPXSTS_MASK_LIVE					0x1			//
#define	CHMPXSTS_MASK_RING					0x30		//
#define	CHMPXSTS_MASK_ACTION				0x300		//
#define	CHMPXSTS_MASK_OPERATE				0x3000		//
#define	CHMPXSTS_MASK_SUSPEND				0x10000		//
#define	CHMPXSTS_MASK_ALL					(CHMPXSTS_MASK_LIVE | CHMPXSTS_MASK_RING | CHMPXSTS_MASK_ACTION | CHMPXSTS_MASK_OPERATE | CHMPXSTS_MASK_SUSPEND)

//---------------------									// (status & CHMPXSTS_MASK_LIVE)
#define	CHMPXSTS_VAL_DOWN					0x0			//		Server/Slave is DOWN
#define	CHMPXSTS_VAL_UP						0x1			//		Server/Slave is UP
														// (status & CHMPXSTS_MASK_RING)
#define	CHMPXSTS_VAL_SLAVE					0x00		//		SLAVE					(not on RING)
#define	CHMPXSTS_VAL_SRVOUT					0x10		//		SERVER is SERVICE OUT	(on RING)
#define	CHMPXSTS_VAL_SRVIN					0x20		//		SERVER is SERVICE IN	(on RING)
														// (status & CHMPXSTS_MASK_ACTION)
#define	CHMPXSTS_VAL_NOACT					0x000		//		No Action
#define	CHMPXSTS_VAL_ADD					0x100		//		Action is ADD
#define	CHMPXSTS_VAL_DELETE					0x200		//		Action is DELETE
														// (status & CHMPXSTS_MASK_ACTION)
#define	CHMPXSTS_VAL_NOTHING				0x0000		//		No Operating
#define	CHMPXSTS_VAL_PENDING				0x1000		//		PENDING
#define	CHMPXSTS_VAL_DOING					0x2000		//		DOING
#define	CHMPXSTS_VAL_DONE					0x3000		//		DONE
														// (status & CHMPXSTS_MASK_SUSPEND)
#define	CHMPXSTS_VAL_NOSUP					0x00000		//		NO SUSPEND
#define	CHMPXSTS_VAL_SUSPEND				0x10000		//		SUSPEND

//---------------------
#define	CHMPXSTS_SLAVE_UP_NORMAL			(CHMPXSTS_VAL_UP	| CHMPXSTS_VAL_SLAVE	| CHMPXSTS_VAL_NOACT	| CHMPXSTS_VAL_NOTHING)
#define	CHMPXSTS_SLAVE_DOWN_NORMAL			(CHMPXSTS_VAL_DOWN	| CHMPXSTS_VAL_SLAVE	| CHMPXSTS_VAL_NOACT	| CHMPXSTS_VAL_NOTHING)

#define	CHMPXSTS_SRVOUT_UP_NORMAL			(CHMPXSTS_VAL_UP	| CHMPXSTS_VAL_SRVOUT	| CHMPXSTS_VAL_NOACT	| CHMPXSTS_VAL_NOTHING)
#define	CHMPXSTS_SRVOUT_DOWN_NORMAL			(CHMPXSTS_VAL_DOWN	| CHMPXSTS_VAL_SRVOUT	| CHMPXSTS_VAL_NOACT	| CHMPXSTS_VAL_NOTHING)

#define	CHMPXSTS_SRVIN_UP_NORMAL			(CHMPXSTS_VAL_UP	| CHMPXSTS_VAL_SRVIN	| CHMPXSTS_VAL_NOACT	| CHMPXSTS_VAL_NOTHING)
#define	CHMPXSTS_SRVIN_UP_PENDING			(CHMPXSTS_VAL_UP	| CHMPXSTS_VAL_SRVIN	| CHMPXSTS_VAL_NOACT	| CHMPXSTS_VAL_PENDING)
#define	CHMPXSTS_SRVIN_UP_MERGING			(CHMPXSTS_VAL_UP	| CHMPXSTS_VAL_SRVIN	| CHMPXSTS_VAL_NOACT	| CHMPXSTS_VAL_DOING)
#define	CHMPXSTS_SRVIN_UP_MERGED			(CHMPXSTS_VAL_UP	| CHMPXSTS_VAL_SRVIN	| CHMPXSTS_VAL_NOACT	| CHMPXSTS_VAL_DONE)

#define	CHMPXSTS_SRVIN_UP_ADDPENDING		(CHMPXSTS_VAL_UP	| CHMPXSTS_VAL_SRVIN	| CHMPXSTS_VAL_ADD		| CHMPXSTS_VAL_PENDING)
#define	CHMPXSTS_SRVIN_UP_ADDING			(CHMPXSTS_VAL_UP	| CHMPXSTS_VAL_SRVIN	| CHMPXSTS_VAL_ADD		| CHMPXSTS_VAL_DOING)
#define	CHMPXSTS_SRVIN_UP_ADDED				(CHMPXSTS_VAL_UP	| CHMPXSTS_VAL_SRVIN	| CHMPXSTS_VAL_ADD		| CHMPXSTS_VAL_DONE)

#define	CHMPXSTS_SRVIN_UP_DELPENDING		(CHMPXSTS_VAL_UP	| CHMPXSTS_VAL_SRVIN	| CHMPXSTS_VAL_DELETE	| CHMPXSTS_VAL_PENDING)
#define	CHMPXSTS_SRVIN_UP_DELETING			(CHMPXSTS_VAL_UP	| CHMPXSTS_VAL_SRVIN	| CHMPXSTS_VAL_DELETE	| CHMPXSTS_VAL_DOING)
#define	CHMPXSTS_SRVIN_UP_DELETED			(CHMPXSTS_VAL_UP	| CHMPXSTS_VAL_SRVIN	| CHMPXSTS_VAL_DELETE	| CHMPXSTS_VAL_DONE)

#define	CHMPXSTS_SRVIN_DOWN_NORMAL			(CHMPXSTS_VAL_DOWN	| CHMPXSTS_VAL_SRVIN	| CHMPXSTS_VAL_NOACT	| CHMPXSTS_VAL_NOTHING)
#define	CHMPXSTS_SRVIN_DOWN_DELPENDING		(CHMPXSTS_VAL_DOWN	| CHMPXSTS_VAL_SRVIN	| CHMPXSTS_VAL_DELETE	| CHMPXSTS_VAL_PENDING)
#define	CHMPXSTS_SRVIN_DOWN_DELETED			(CHMPXSTS_VAL_DOWN	| CHMPXSTS_VAL_SRVIN	| CHMPXSTS_VAL_DELETE	| CHMPXSTS_VAL_DONE)

//---------------------
#define	SET_CHMPXSTS_DOWN(status)			(status = (status & ~CHMPXSTS_MASK_LIVE)	)
#define	SET_CHMPXSTS_UP(status)				(status = (status & ~CHMPXSTS_MASK_LIVE)	| CHMPXSTS_VAL_UP)

#define	SET_CHMPXSTS_SLAVE(status)			(status = (status & ~CHMPXSTS_MASK_RING)	)
#define	SET_CHMPXSTS_SRVOUT(status)			(status = (status & ~CHMPXSTS_MASK_RING)	| CHMPXSTS_VAL_SRVOUT)
#define	SET_CHMPXSTS_SRVIN(status)			(status = (status & ~CHMPXSTS_MASK_RING)	| CHMPXSTS_VAL_SRVIN)

#define	SET_CHMPXSTS_NOACT(status)			(status = (status & ~CHMPXSTS_MASK_ACTION)	)
#define	SET_CHMPXSTS_ADD(status)			(status = (status & ~CHMPXSTS_MASK_ACTION)	| CHMPXSTS_VAL_ADD)
#define	SET_CHMPXSTS_DELETE(status)			(status = (status & ~CHMPXSTS_MASK_ACTION)	| CHMPXSTS_VAL_DELETE)

#define	SET_CHMPXSTS_NOTHING(status)		(status = (status & ~CHMPXSTS_MASK_OPERATE)	)
#define	SET_CHMPXSTS_PENDING(status)		(status = (status & ~CHMPXSTS_MASK_OPERATE)	| CHMPXSTS_VAL_PENDING)
#define	SET_CHMPXSTS_DOING(status)			(status = (status & ~CHMPXSTS_MASK_OPERATE)	| CHMPXSTS_VAL_DOING)
#define	SET_CHMPXSTS_DONE(status)			(status = (status & ~CHMPXSTS_MASK_OPERATE)	| CHMPXSTS_VAL_DONE)

#define	SET_CHMPXSTS_NOSUP(status)			(status = (status & ~CHMPXSTS_MASK_SUSPEND)	)
#define	SET_CHMPXSTS_SUSPEND(status)		(status = (status & ~CHMPXSTS_MASK_SUSPEND)	| CHMPXSTS_VAL_SUSPEND)

//---------------------
#define	IS_CHMPXSTS_DOWN(status)			(CHMPXSTS_VAL_DOWN		== (status & CHMPXSTS_MASK_LIVE))
#define	IS_CHMPXSTS_UP(status)				(CHMPXSTS_VAL_UP		== (status & CHMPXSTS_MASK_LIVE))

#define	IS_CHMPXSTS_SLAVE(status)			(CHMPXSTS_VAL_SLAVE		== (status & CHMPXSTS_MASK_RING))
#define	IS_CHMPXSTS_SRVOUT(status)			(CHMPXSTS_VAL_SRVOUT	== (status & CHMPXSTS_MASK_RING))
#define	IS_CHMPXSTS_SRVIN(status)			(CHMPXSTS_VAL_SRVIN		== (status & CHMPXSTS_MASK_RING))
#define	IS_CHMPXSTS_SERVER(status)			(IS_CHMPXSTS_SRVOUT(status) || IS_CHMPXSTS_SRVIN(status))
#define	IS_CHMPXSTS_ONRING(status)			IS_CHMPXSTS_SERVER(status)

#define	IS_CHMPXSTS_NOACT(status)			(CHMPXSTS_VAL_NOACT		== (status & CHMPXSTS_MASK_ACTION))
#define	IS_CHMPXSTS_ADD(status)				(CHMPXSTS_VAL_ADD		== (status & CHMPXSTS_MASK_ACTION))
#define	IS_CHMPXSTS_DELETE(status)			(CHMPXSTS_VAL_DELETE	== (status & CHMPXSTS_MASK_ACTION))

#define	IS_CHMPXSTS_NOTHING(status)			(CHMPXSTS_VAL_NOTHING	== (status & CHMPXSTS_MASK_OPERATE))
#define	IS_CHMPXSTS_PENDING(status)			(CHMPXSTS_VAL_PENDING	== (status & CHMPXSTS_MASK_OPERATE))
#define	IS_CHMPXSTS_DOING(status)			(CHMPXSTS_VAL_DOING		== (status & CHMPXSTS_MASK_OPERATE))
#define	IS_CHMPXSTS_DONE(status)			(CHMPXSTS_VAL_DONE		== (status & CHMPXSTS_MASK_OPERATE))

#define	IS_CHMPXSTS_NOSUP(status)			(CHMPXSTS_VAL_NOSUP		== (status & CHMPXSTS_MASK_SUSPEND))
#define	IS_CHMPXSTS_SUSPEND(status)			(CHMPXSTS_VAL_SUSPEND	== (status & CHMPXSTS_MASK_SUSPEND))

#define	IS_CHMPXSTS_OPERATING(status)		(IS_CHMPXSTS_DOING(status) || IS_CHMPXSTS_DONE(status))

#define	IS_SAFE_CHMPXSTS_EX(status)		(	CHMPXSTS_SLAVE_UP_NORMAL		== status || \
											CHMPXSTS_SLAVE_DOWN_NORMAL		== status || \
											CHMPXSTS_SRVIN_UP_NORMAL		== status || \
											CHMPXSTS_SRVIN_UP_PENDING		== status || \
											CHMPXSTS_SRVIN_UP_MERGING		== status || \
											CHMPXSTS_SRVIN_UP_MERGED		== status || \
											CHMPXSTS_SRVIN_UP_ADDPENDING	== status || \
											CHMPXSTS_SRVIN_UP_ADDING		== status || \
											CHMPXSTS_SRVIN_UP_ADDED			== status || \
											CHMPXSTS_SRVIN_UP_DELPENDING	== status || \
											CHMPXSTS_SRVIN_UP_DELETING		== status || \
											CHMPXSTS_SRVIN_UP_DELETED		== status || \
											CHMPXSTS_SRVIN_DOWN_NORMAL		== status || \
											CHMPXSTS_SRVIN_DOWN_DELPENDING	== status || \
											CHMPXSTS_SRVIN_DOWN_DELETED		== status || \
											CHMPXSTS_SRVOUT_UP_NORMAL		== status || \
											CHMPXSTS_SRVOUT_DOWN_NORMAL		== status )

#define	IS_SAFE_CHMPXSTS(status)		(	(0 == (status & ~CHMPXSTS_MASK_ALL)) && \
											IS_SAFE_CHMPXSTS_EX((status & ~CHMPXSTS_MASK_SUSPEND))	)

//---------------------
#define	IS_CHMPXSTS_SERVICING(status)	(	CHMPXSTS_SRVIN_UP_NORMAL		== (status & ~CHMPXSTS_MASK_SUSPEND) || \
                                            CHMPXSTS_SRVIN_UP_DELPENDING	== (status & ~CHMPXSTS_MASK_SUSPEND) || \
                                            CHMPXSTS_SRVIN_UP_DELETING		== (status & ~CHMPXSTS_MASK_SUSPEND) )

#define	IS_CHMPXSTS_BASEHASH(status)	(	CHMPXSTS_SRVIN_UP_NORMAL		== (status & ~CHMPXSTS_MASK_SUSPEND) || \
											CHMPXSTS_SRVIN_UP_PENDING		== (status & ~CHMPXSTS_MASK_SUSPEND) || \
											CHMPXSTS_SRVIN_UP_MERGING		== (status & ~CHMPXSTS_MASK_SUSPEND) || \
											CHMPXSTS_SRVIN_UP_MERGED		== (status & ~CHMPXSTS_MASK_SUSPEND) || \
											CHMPXSTS_SRVIN_UP_DELPENDING	== (status & ~CHMPXSTS_MASK_SUSPEND) || \
											CHMPXSTS_SRVIN_UP_DELETING		== (status & ~CHMPXSTS_MASK_SUSPEND) || \
											CHMPXSTS_SRVIN_UP_DELETED		== (status & ~CHMPXSTS_MASK_SUSPEND) || \
											CHMPXSTS_SRVIN_DOWN_NORMAL		== (status & ~CHMPXSTS_MASK_SUSPEND) || \
											CHMPXSTS_SRVIN_DOWN_DELPENDING	== (status & ~CHMPXSTS_MASK_SUSPEND) || \
											CHMPXSTS_SRVIN_DOWN_DELETED		== (status & ~CHMPXSTS_MASK_SUSPEND) )

#define	IS_CHMPXSTS_PENDINGHASH(status)	(	CHMPXSTS_SRVIN_UP_NORMAL		== (status & ~CHMPXSTS_MASK_SUSPEND) || \
											CHMPXSTS_SRVIN_UP_PENDING		== (status & ~CHMPXSTS_MASK_SUSPEND) || \
											CHMPXSTS_SRVIN_UP_MERGING		== (status & ~CHMPXSTS_MASK_SUSPEND) || \
											CHMPXSTS_SRVIN_UP_MERGED		== (status & ~CHMPXSTS_MASK_SUSPEND) || \
											CHMPXSTS_SRVIN_UP_ADDPENDING	== (status & ~CHMPXSTS_MASK_SUSPEND) || \
											CHMPXSTS_SRVIN_UP_ADDING		== (status & ~CHMPXSTS_MASK_SUSPEND) || \
											CHMPXSTS_SRVIN_UP_ADDED			== (status & ~CHMPXSTS_MASK_SUSPEND) || \
											CHMPXSTS_SRVIN_DOWN_NORMAL		== (status & ~CHMPXSTS_MASK_SUSPEND) )

//---------------------
#define	STR_CHMPXSTS_ISSAFE(status)		(	IS_SAFE_CHMPXSTS(status)	? "SAFE"			: "NOT SAFE")
#define	STR_CHMPXSTS_LIVE(status)		(	IS_CHMPXSTS_DOWN(status)	? "[DOWN]"			: "[UP]" )
#define	STR_CHMPXSTS_RING(status)		(	IS_CHMPXSTS_SLAVE(status)	? "[SLAVE]"			: \
											IS_CHMPXSTS_SRVOUT(status)	? "[SERVICE OUT]"	: \
											IS_CHMPXSTS_SRVIN(status)	? "[SERVICE IN]"	: "[Unknown RING]")
#define	STR_CHMPXSTS_ACTION(status)		(	IS_CHMPXSTS_NOACT(status)	? "[n/a]"			: \
											IS_CHMPXSTS_ADD(status)		? "[ADD]"			: \
											IS_CHMPXSTS_DELETE(status)	? "[DELETE]"		: "[unknown ACTION]")
#define	STR_CHMPXSTS_OPERATE(status)	(	IS_CHMPXSTS_NOTHING(status)	? "[Nothing]"		: \
											IS_CHMPXSTS_PENDING(status)	? "[Pending]"		: \
											IS_CHMPXSTS_DOING(status)	? "[Doing]"			: \
											IS_CHMPXSTS_DONE(status)	? "[Done]"			: "[unknown OPERATION]")
#define	STR_CHMPXSTS_SUSPEND(status)	(	IS_CHMPXSTS_NOSUP(status)	? "[NoSuspend]"		: \
											IS_CHMPXSTS_SUSPEND(status)	? "[Suspend]"		: "[unknown SUSPEND]")

//---------------------
#define	CHANGE_CHMPXSTS_TO_DOWN(status)	\
		{ \
			if(IS_CHMPXSTS_SRVIN(status)){ \
				if(IS_CHMPXSTS_UP(status)){ \
					if(IS_CHMPXSTS_NOACT(status)){ \
						status = CHMPXSTS_SRVIN_DOWN_NORMAL; \
					}else if(IS_CHMPXSTS_ADD(status)){ \
						status = CHMPXSTS_SRVOUT_DOWN_NORMAL; \
					}else if(IS_CHMPXSTS_DELETE(status)){ \
						if(IS_CHMPXSTS_OPERATING(status)){ \
							status = CHMPXSTS_SRVIN_DOWN_DELETED; \
						}else{ \
							status = CHMPXSTS_SRVIN_DOWN_DELPENDING; \
						} \
					} \
				}else if(IS_CHMPXSTS_DOWN(status)){ \
					if(IS_CHMPXSTS_DELETE(status)){ \
						status = CHMPXSTS_SRVIN_DOWN_DELPENDING; \
					}else{ \
						status = CHMPXSTS_SRVIN_DOWN_NORMAL; \
					} \
				} \
			}else if(IS_CHMPXSTS_SRVOUT(status)){ \
				status = CHMPXSTS_SRVOUT_DOWN_NORMAL; \
			} \
		}

#define	CHANGE_CHMPXSTS_TO_SRVOUT(status)	\
		{ \
			if(IS_CHMPXSTS_SRVIN(status)){ \
				if(IS_CHMPXSTS_UP(status)){ \
					if(IS_CHMPXSTS_NOACT(status)){ \
						if(!IS_CHMPXSTS_OPERATING(status)){ \
							status = CHMPXSTS_SRVIN_UP_DELPENDING; \
							if(IS_CHMPXSTS_NOSUP(status)){ \
								CHANGE_CHMPXSTS_TO_NOSUP(status); \
							}else{ \
								CHANGE_CHMPXSTS_TO_SUSPEND(status); \
							} \
						} \
					}else if(IS_CHMPXSTS_ADD(status)){ \
						if(!IS_CHMPXSTS_OPERATING(status)){ \
							status = CHMPXSTS_SRVOUT_UP_NORMAL; \
							if(IS_CHMPXSTS_NOSUP(status)){ \
								CHANGE_CHMPXSTS_TO_NOSUP(status); \
							}else{ \
								CHANGE_CHMPXSTS_TO_SUSPEND(status); \
							} \
						} \
					} \
				}else if(IS_CHMPXSTS_DOWN(status)){ \
					if(IS_CHMPXSTS_NOACT(status)){ \
						status = CHMPXSTS_SRVIN_DOWN_DELPENDING; \
					} \
				} \
			} \
		}

#define	CHANGE_CHMPXSTS_TO_SRVIN(status)	\
		{ \
			if(IS_CHMPXSTS_SRVIN(status)){ \
				if(IS_CHMPXSTS_UP(status)){ \
					if(IS_CHMPXSTS_DELETE(status)){ \
						if(IS_CHMPXSTS_PENDING(status)){ \
							if(IS_CHMPXSTS_NOSUP(status)){ \
								status = CHMPXSTS_SRVIN_UP_PENDING; \
							}else{ \
								status = CHMPXSTS_SRVIN_UP_NORMAL; \
								CHANGE_CHMPXSTS_TO_SUSPEND(status); \
							} \
						} \
					} \
				} \
			}else if(IS_CHMPXSTS_SRVOUT(status)){ \
				if(IS_CHMPXSTS_UP(status)){ \
					if(IS_CHMPXSTS_NOACT(status)){ \
						if(IS_CHMPXSTS_NOSUP(status)){ \
							status = CHMPXSTS_SRVIN_UP_ADDPENDING; \
						} \
					} \
				} \
			} \
		}

#define	CHANGE_CHMPXSTS_SUSPEND_FLAG(status, is_suspend)	\
		{ \
			if(IS_CHMPXSTS_DOWN(status) || IS_CHMPXSTS_SLAVE(status)){ \
				SET_CHMPXSTS_NOSUP(status); \
			}else{ \
				if(is_suspend){ \
					SET_CHMPXSTS_SUSPEND(status); \
				}else{ \
					SET_CHMPXSTS_NOSUP(status); \
				} \
			} \
		}

#define	CHANGE_CHMPXSTS_TO_SUSPEND(status)		CHANGE_CHMPXSTS_SUSPEND_FLAG(status, true)
#define	CHANGE_CHMPXSTS_TO_NOSUP(status)		CHANGE_CHMPXSTS_SUSPEND_FLAG(status, false)

//---------------------
// MQMSGHEAD flag
//---------------------
// This flag has two status.
// 1) Assigned or not assigned.
// 2) Activated or disactivated in assigned MQ
// * Not assigned flag must not be set with activated flag.
//
#define	MQFLAG_VAL_NOTASSIGNED			0x0						// not assigned
#define	MQFLAG_VAL_ASSIGNED				0x1						// assigned
#define	MQFLAG_VAL_ACTIVATED			0x2						// activated
#define	MQFLAG_VAL_CHMPXPROC			0x4						// chmpx process used
#define	MQFLAG_VAL_CLIENTPROC			0x8						// client process used

#define	MQFLAG_MASK_ASSIGNED			(MQFLAG_VAL_ASSIGNED)
#define	MQFLAG_MASK_ACTIVATED			(MQFLAG_VAL_ACTIVATED)
#define	MQFLAG_MASK_KIND				(MQFLAG_VAL_CHMPXPROC | MQFLAG_VAL_CLIENTPROC)
#define	MQFLAG_MASK_STATUS				(MQFLAG_MASK_ASSIGNED | MQFLAG_MASK_ACTIVATED)
#define	MQFLAG_MASK_ALL					(MQFLAG_MASK_ASSIGNED | MQFLAG_MASK_ACTIVATED | MQFLAG_MASK_KIND)

#define	SET_MQFLAG_INITIALIZED(flag)	(flag = 0x0)
#define	SET_MQFLAG_NOTASSIGNED(flag)	(flag = (flag & ~MQFLAG_MASK_STATUS)	)
#define	SET_MQFLAG_ASSIGNED(flag)		(flag = (flag & ~MQFLAG_MASK_STATUS)	| MQFLAG_VAL_ASSIGNED)
#define	SET_MQFLAG_ACTIVATED(flag)		(flag = (flag & ~MQFLAG_MASK_STATUS)	| MQFLAG_VAL_ASSIGNED | MQFLAG_VAL_ACTIVATED)
#define	SET_MQFLAG_DISACTIVATED(flag)	SET_MQFLAG_ASSIGNED(flag)
#define	SET_MQFLAG_CHMPXPROC(flag)		(flag = (flag & ~MQFLAG_MASK_KIND)		| MQFLAG_VAL_CHMPXPROC)
#define	SET_MQFLAG_CLIENTPROC(flag)		(flag = (flag & ~MQFLAG_MASK_KIND)		| MQFLAG_VAL_CLIENTPROC)

#define	IS_MQFLAG_NOTASSIGNED(flag)		(MQFLAG_VAL_NOTASSIGNED	== (flag & MQFLAG_MASK_ASSIGNED))
#define	IS_MQFLAG_ASSIGNED(flag)		(MQFLAG_VAL_ASSIGNED	== (flag & MQFLAG_MASK_ASSIGNED))
#define	IS_MQFLAG_ACTIVATED(flag)		(MQFLAG_VAL_ACTIVATED	== (flag & MQFLAG_MASK_ACTIVATED))
#define	IS_MQFLAG_DISACTIVATED(flag)	(MQFLAG_VAL_ACTIVATED	!= (flag & MQFLAG_MASK_ACTIVATED))
#define	IS_MQFLAG_CHMPXPROC(flag)		(MQFLAG_VAL_CHMPXPROC	== (flag & MQFLAG_MASK_KIND))
#define	IS_MQFLAG_CLIENTPROC(flag)		(MQFLAG_VAL_CLIENTPROC	== (flag & MQFLAG_MASK_KIND))

#define	STR_MQFLAG_ASSIGNED(flag)		(IS_MQFLAG_ASSIGNED(flag)	? "[Assigned]"	: "[Not Assigned]"	)
#define	STR_MQFLAG_ACTIVATED(flag)		(IS_MQFLAG_ACTIVATED(flag)	? "[Activated]"	: "[Disactivated]"	)
#define	STR_MQFLAG_KIND(flag)			(IS_MQFLAG_CHMPXPROC(flag)	? "[Chmpx]"		: IS_MQFLAG_CLIENTPROC(flag) ? "[Client]" : "[Free]"	)

//---------------------
// CHMLOG type
//---------------------
#define	CHMLOG_TYPE_UNKOWN				0x0
#define	CHMLOG_TYPE_ASSIGNED			0x1
#define	CHMLOG_TYPE_OUTPUT				0x2
#define	CHMLOG_TYPE_INPUT				0x4
#define	CHMLOG_TYPE_SOCKET				0x8
#define	CHMLOG_TYPE_MQ					0x10

#define	CHMLOG_MASK_ASSIGNED			(CHMLOG_TYPE_ASSIGNED)
#define	CHMLOG_MASK_DIRECTION			(CHMLOG_TYPE_OUTPUT | CHMLOG_TYPE_INPUT)
#define	CHMLOG_MASK_DEVICE				(CHMLOG_TYPE_SOCKET | CHMLOG_TYPE_MQ)

#define	CHMLOG_TYPE_NOTASSIGNED			(	CHMLOG_TYPE_UNKOWN	)
#define	CHMLOG_TYPE_OUT_SOCKET			(	CHMLOG_TYPE_ASSIGNED	| CHMLOG_TYPE_OUTPUT	| CHMLOG_TYPE_SOCKET	)
#define	CHMLOG_TYPE_IN_SOCKET			(	CHMLOG_TYPE_ASSIGNED	| CHMLOG_TYPE_INPUT		| CHMLOG_TYPE_SOCKET	)
#define	CHMLOG_TYPE_OUT_MQ				(	CHMLOG_TYPE_ASSIGNED	| CHMLOG_TYPE_OUTPUT	| CHMLOG_TYPE_MQ		)
#define	CHMLOG_TYPE_IN_MQ				(	CHMLOG_TYPE_ASSIGNED	| CHMLOG_TYPE_INPUT		| CHMLOG_TYPE_MQ		)

#define	IS_CHMLOG_ASSIGNED(logtype)		(	CHMLOG_TYPE_ASSIGNED							== ( logtype & CHMLOG_MASK_ASSIGNED)						)
#define	IS_CHMLOG_INPUT(logtype)		(	(CHMLOG_TYPE_ASSIGNED	| CHMLOG_TYPE_INPUT)	== ( logtype & (CHMLOG_MASK_ASSIGNED | CHMLOG_TYPE_INPUT))	)
#define	IS_CHMLOG_OUTPUT(logtype)		(	(CHMLOG_TYPE_ASSIGNED	| CHMLOG_TYPE_OUTPUT)	== ( logtype & (CHMLOG_MASK_ASSIGNED | CHMLOG_TYPE_OUTPUT))	)
#define	IS_CHMLOG_SOCKET(logtype)		(	(CHMLOG_TYPE_ASSIGNED	| CHMLOG_TYPE_SOCKET)	== ( logtype & (CHMLOG_MASK_ASSIGNED | CHMLOG_TYPE_SOCKET))	)
#define	IS_CHMLOG_MQ(logtype)			(	(CHMLOG_TYPE_ASSIGNED	| CHMLOG_TYPE_MQ)		== ( logtype & (CHMLOG_MASK_ASSIGNED | CHMLOG_TYPE_MQ))		)

#define	IS_SAFE_CHMLOG_TYPE(logtype)	(	CHMLOG_TYPE_NOTASSIGNED	== logtype || \
											CHMLOG_TYPE_OUT_SOCKET	== logtype || \
											CHMLOG_TYPE_IN_SOCKET	== logtype || \
											CHMLOG_TYPE_OUT_MQ		== logtype || \
											CHMLOG_TYPE_IN_MQ		== logtype )

#define	IS_SAFE_CHMLOG_MASK(logtype)	(	0 != (logtype & CHMLOG_MASK_ASSIGNED)	|| \
											0 != (logtype & CHMLOG_MASK_DIRECTION)	|| \
											0 != (logtype & CHMLOG_MASK_DEVICE)		)

#define	STR_CHMLOG_TYPE(logtype)		(	CHMLOG_TYPE_NOTASSIGNED	== logtype	? "[NotAssigned]"	: \
											CHMLOG_TYPE_OUT_SOCKET	== logtype	? "[Output Socket]"	: \
											CHMLOG_TYPE_IN_SOCKET	== logtype	? "[Input Socket]"	: \
											CHMLOG_TYPE_OUT_MQ		== logtype	? "[Output MQ]"		: \
											CHMLOG_TYPE_IN_MQ		== logtype	? "[Input MQ]"		: "[Unsafe type]"	)

//---------------------
// Set/Get sock flag
//---------------------
#define	SOCKTG_SOCK						0x1
#define	SOCKTG_CTLSOCK					0x2
#define	SOCKTG_SELFSOCK					0x4
#define	SOCKTG_SELFCTLSOCK				0x8
#define	SOCKTG_BOTH						(SOCKTG_SOCK | SOCKTG_CTLSOCK)
#define	SOCKTG_BOTHSELF					(SOCKTG_SELFSOCK | SOCKTG_SELFCTLSOCK)
#define	SOCKTG_ALL						(SOCKTG_SOCK | SOCKTG_CTLSOCK | SOCKTG_SELFSOCK | SOCKTG_SELFCTLSOCK)

#define	IS_SOCKTG_SOCK(type)			(0 != (type & SOCKTG_SOCK))
#define	IS_SOCKTG_CTLSOCK(type)			(0 != (type & SOCKTG_CTLSOCK))
#define	IS_SOCKTG_SELFSOCK(type)		(0 != (type & SOCKTG_SELFSOCK))
#define	IS_SOCKTG_SELFCTLSOCK(type)		(0 != (type & SOCKTG_SELFCTLSOCK))

//---------------------
// Close sock flag
//---------------------
#define	CLOSETG_SERVERS					1
#define	CLOSETG_SLAVES					2
#define	CLOSETG_BOTH					(CLOSETG_SERVERS | CLOSETG_SLAVES)

#define	IS_CLOSETG_SERVERS(type)		(0 != (type & CLOSETG_SERVERS))
#define	IS_CLOSETG_SLAVES(type)			(0 != (type & CLOSETG_SLAVES))

//---------------------
// Set/Get hash flag
//---------------------
#define	HASHTG_BASE						1
#define	HASHTG_PENDING					2
#define	HASHTG_BOTH						(HASHTG_BASE | HASHTG_PENDING)

#define	IS_HASHTG_BASE(type)			(0 != (type & HASHTG_BASE))
#define	IS_HASHTG_PENDING(type)			(0 != (type & HASHTG_PENDING))

//---------------------
// CLTPROCLIST
//---------------------
#define	MAX_CLTPROCLIST_COUNT(max_client_mq_cnt, mqcnt_per_attach)		(1 + (max_client_mq_cnt / mqcnt_per_attach) + 1)

//---------------------
// Others
//---------------------
#define	MASK64_HIBIT					0xFFFFFFFF00000000
#define	MASK64_LOWBIT					0x00000000FFFFFFFF
#define	COMPOSE64(hi, low)				(((hi & MASK64_LOWBIT) << 32) | (low & MASK64_LOWBIT))

// CHMPXMODE enum
#define	STRCHMPXMODE(mode)				(CHMPX_SERVER == mode ? "server" : CHMPX_SLAVE == mode ? "slave" : CHMPX_UNKNOWN == mode ? "n/a" : "unknown")

//---------------------------------------------------------
// Structure : Shared memory
//---------------------------------------------------------
// CHMSHM
//   +-----------------------------------------+
//   | CHMINFO                                 |
//   |   +---------------------------------+   |
//   |   | ....                            |   |
//   |   | CHMPXMAN                        |   |
//   |   |   +-------------------------+   |   |
//   |   |   | ....                    |   |   |
//   |   |   | PCHMPX                  |   |   |------------+
//   |   |   | PCHMPXLIST              |   |   |------------+
//   |   |   | PCHMPXLIST[ARRAY]       |   |   |------------+
//   |   |   | PCHMPX[ARRAY]           |   |   |--------------+
//   |   |   | PCHMPX[ARRAY]           |   |   |--------------+
//   |   |   | CHMSTAT                 |   |   |            | |
//   |   |   |   +-----------------+   |   |   |            | |
//   |   |   |   | ....            |   |   |   |            | |
//   |   |   |   +-----------------+   |   |   |            | |
//   |   |   | CHMSTAT                 |   |   |            | |
//   |   |   |   +-----------------+   |   |   |            | |
//   |   |   |   | ....            |   |   |   |            | |
//   |   |   |   +-----------------+   |   |   |            | |
//   |   |   +-------------------------+   |   |            | |
//   |   | ....                            |   |            | |
//   |   | PMQMSGHEADLIST                  |   |----------+ | |
//   |   | PMQMSGHEADLIST                  |   |----------+ | |
//   |   +---------------------------------+   |          | | |
//   | PCHMPXLIST                              |--------+ | | |
//   | MQMSGHEADLIST                           |------+ | | | |
//   | CLTPROCLIST                             |----+ | | | | |
//   | CHMLOG                                  |    | | | | | |
//   |   +---------------------------------+   |    | | | | | |
//   |   | ....                            |   |    | | | | | |
//   |   | PCHMLOGRAW                      |   |--+ | | | | | |
//   |   | PCHMLOGRAW                      |   |--+ | | | | | |
//   |   | PCHMLOGRAW                      |   |--+ | | | | | |
//   |   +---------------------------------+   |  | | | | | | |
//   +-----------------------------------------+  | | | | | | |
//   +-----------------------------------------+  | | | | | | |
//   | CHMPXLIST[XXX]                          |<-------+---+ |
//   |                                         |  | | |   |   |
//   +-----------------------------------------+  | | |   |   |
//   +-----------------------------------------+  | | |   |   |
//   | PCHMPX[XXX]                             |<-------------+
//   |                                         |  | | |   |
//   +-----------------------------------------+  | | |   |
//   +-----------------------------------------+  | | |   |
//   | MQMSGHEADLIST[XXX]                      |<-----+---+
//   |                                         |  | |
//   +-----------------------------------------+  | |
//   +-----------------------------------------+  | |
//   | CLTPROCLIST[XXX]                        |<---+
//   |                                         |  |
//   +-----------------------------------------+  |
//   +-----------------------------------------+  |
//   | CHMLOGRAW[XXX]                          |<-+
//   |                                         |
//   +-----------------------------------------+
//   +-----------------------------------------+
//   | CHMSOCKLIST[XXX]                        |
//   | (pointed from CHMPX)                    |
//   +-----------------------------------------+
//
// CHMSHM has all data for chmpx servers list, stats, mq list, and logs.
// We can have the chmpx servers list in class, but stats and status
// should be read from another process. And server's data should be had
// one place. So that, CHMPX managed datas are in one CHMSHM.
//
// [NOTICE]
// These structure is not set packed attribute, because chmpx
// uses each member with locking it. Then each member is aligned
// 64bit byte order.
//
//---------------------------------
// Socket List
typedef struct chm_sock_list{
	struct chm_sock_list*	prev;
	struct chm_sock_list*	next;
	int						sock;
}CHMSOCKLIST, *PCHMSOCKLIST;

//---------------------------------
// Peer Information(Host + Port)
typedef struct chmpx_host_port_raw_pair{
	char			name[NI_MAXHOST];					// = 1025 for getnameinfo()
	short			port;
}CHMPX_ATTR_PACKED CHMPXHP_RAWPAIR, *PCHMPXHP_RAWPAIR;

//---------------------------------
// Chmpx
typedef struct chmpx{
	chmpxid_t		chmpxid;
	char			name[NI_MAXHOST];					// = 1025 for getnameinfo()
	CHMPXMODE		mode;
	chmhash_t		base_hash;
	chmhash_t		pending_hash;
	short			port;
	short			ctlport;
	bool			is_ssl;								// ssl mode
	bool			verify_peer;						// verify ssl client peer on server(always false on client chmpx)
	bool			is_ca_file;							// flag for CA path is directory or file
	char			capath[CHM_MAX_PATH_LEN];			// CA directory path
	char			server_cert[CHM_MAX_PATH_LEN];		// signed server cert file path
	char			server_prikey[CHM_MAX_PATH_LEN];	// signed server private key file path
	char			slave_cert[CHM_MAX_PATH_LEN];		// signed slave cert file path
	char			slave_prikey[CHM_MAX_PATH_LEN];		// signed slave private key file path
	PCHMSOCKLIST	socklist;							// sock means session connecting to server port.
	int				ctlsock;							// ctlsock means session connecting to server ctlport.
	int				selfsock;							// If only self chmpx, selfsock means listening port. not self does not use.
	int				selfctlsock;						// If only self chmpx, selfctlsock means listening ctlport. not self does not use.
	time_t			last_status_time;
	chmpxsts_t		status;
														// [NOTE] adding after v1.0.71
	char			cuk[CUK_MAX];						// cuk
	char			custom_seed[CUSTOM_ID_SEED_MAX];	// custom id seed
	CHMPXHP_RAWPAIR	endpoints[EXTERNAL_EP_MAX];			// external endpoint host/port information
	CHMPXHP_RAWPAIR	ctlendpoints[EXTERNAL_EP_MAX];		// external endpoint host/control port information
	CHMPXHP_RAWPAIR	forward_peers[FORWARD_PEER_MAX];	// forward gateway(NAT) peer hostname(ip address)
	CHMPXHP_RAWPAIR	reverse_peers[REVERSE_PEER_MAX];	// reverse gateway(NAT) peer hostname(ip address)
}CHMPX, *PCHMPX;

// Chmpx list
typedef struct chmpx_list{
	struct chmpx_list*	prev;			// For RING list
	struct chmpx_list*	next;			// For RING list
	struct chmpx_list*	same;			// For conflicting Hash list
	CHMPX				chmpx;
}CHMPXLIST, *PCHMPXLIST;

// Stats for chmpx process
typedef struct chm_stat{
	long			total_sent_count;
	long			total_received_count;
	size_t			total_body_bytes;
	size_t			min_body_bytes;
	size_t			max_body_bytes;
	struct timespec	total_elapsed_time;
	struct timespec	min_elapsed_time;
	struct timespec	max_elapsed_time;
}CHMSTAT, *PCHMSTAT;

// All Chmpx manage structure
// 
// chmpxid_map		mapping for chmpxid to chmpxlist pointer(offset)
//					care for some same chmpxid in this mapping array and
//					server/slave chmpxid in this.
// chmpx_base_srvs	This is sequence, and holds a pointer to chmpx* in
//					order of base hash value.
// chmpx_pend_srvs	This is sequence, and holds a pointer to chmpx* in
//					order of pending hash value.
// chmpx_self		for own bind port/ctlport
// chmpx_servers	server mode(service in/out) on RING, information for
//					connection to servers(self -> other server)
// chmpx_slaves		slaves information which is connection from other
//					chmpx to own(other server/slave -> self)
// 
typedef struct chmpx_manage{
	char			group[MAX_GROUP_LENGTH];
	long			cfg_revision;
	time_t			cfg_date;
	long			replica_count;					// replication count
	long			chmpx_count;					// = max CHMPX count(server + slave)
	PCHMPXLIST		chmpxid_map[CHMPXID_MAP_SIZE];	// 32KB = 4K * pointer(8byte)
	PCHMPX*			chmpx_base_srvs;				// offset pointer to CHMPX array for base hash servers
	PCHMPX*			chmpx_pend_srvs;				// offset pointer to CHMPX array for pending hash servers
	PCHMPXLIST		chmpx_self;						// own chmpx, if server mode, this is included pointer in servers list.
	long			chmpx_bhash_count;				// base hash chmpx count
	long			chmpx_server_count;
	PCHMPXLIST		chmpx_servers;					// servers chmpx list( with self chmpx list if server mode )
	long			chmpx_slave_count;
	PCHMPXLIST		chmpx_slaves;					// slaves chmpx list( with self chmpx list if slave mode )
	long			chmpx_free_count;
	PCHMPXLIST		chmpx_frees;
	long			sock_free_count;
	PCHMSOCKLIST	sock_frees;						// free socket list
	bool			is_operating;					// operating
	chmpxid_t		last_chmpxid;					// last random chmpxid
	CHMSTAT			server_stat;
	CHMSTAT			slave_stat;
}CHMPXMAN, *PCHMPXMAN;

//---------------------------------
// Head for Message queue
typedef struct mq_msg_head{
	msgid_t		msgid;
	msgflag_t	flag;
	pid_t		pid;
}MQMSGHEAD, *PMQMSGHEAD;

// Message queue head list
typedef struct mq_msg_head_list{
	struct mq_msg_head_list*	prev;
	struct mq_msg_head_list*	next;
	MQMSGHEAD					msghead;
}MQMSGHEADLIST, *PMQMSGHEADLIST;

//---------------------------------
// Client processes list
typedef struct client_proc_list{
	struct client_proc_list*	prev;
	struct client_proc_list*	next;
	pid_t						pid;
}CLTPROCLIST, *PCLTPROCLIST;

//---------------------------------
// chmpx information
//
// [NOTE]
// ssl_min_ver and nssdb_dir members should be a member of CHMPX structure, but these are
// extended option for SSL/TLS, and there are too many changes to set these in the CHMPX
// structure.
// In particular, since the CHMPX structure is used for CHMPX interprocess communication
// packets, there are many changes. In addition, ssl_min_ver and nssdb_dir are values that
// can be restricted throughout the CHMPX process, then these are member of CHMINFO structure
// as a setting value of the entire CHMPX process.
//
typedef struct chminfo{
	char				chminfo_version[CHM_CHMINFO_VERSION_BUFLEN];// version number for CHMINFO(old version does not have this member)
	size_t				chminfo_size;								// CHMINFO structure size(old version does not have this member)
	pid_t				pid;
	time_t				start_time;
	CHMPXMAN			chmpx_man;
	CHMPXID_SEED_TYPE	chmpxid_type;								// Seed type for creating chmpxid
	chmss_ver_t			ssl_min_ver;								// SSL/TLS minimum version(old version does not have this member, and see NOTE)
	char				nssdb_dir[CHM_MAX_PATH_LEN];				// NSSDB directory path like "SSL_DIR" environment for NSS
	int					evsock_thread_cnt;							// socket thread count
	int					evmq_thread_cnt;							// MQ thread count
	bool				is_random_deliver;
	bool				is_auto_merge;
	bool				is_auto_merge_suspend;						// suspend auto merging flag for that is_auto_merge is true
	bool				is_do_merge;
	long				max_mqueue;									// = max MQ count
	long				chmpx_mqueue;								// = Chmpx MQ count
	long				max_q_per_chmpxmq;							// = max queue per chmpx MQ
	long				max_q_per_cltmq;							// = max queue per client MQ
	long				max_mq_per_client;							// = MQ per client
	long				mq_per_attach;								// = MQ per attach
	bool				mq_ack;										// MQ ACK
	int					max_sock_pool;								// max socket count per chmpx
	time_t				sock_pool_timeout;							// timeout value till closing for not used socket in pool
	int					sock_retrycnt;								// retry count for socket
	suseconds_t			timeout_wait_socket;						// wait timeout for read/write socket
	suseconds_t			timeout_wait_connect;						// wait timeout for connect socket
	int					mq_retrycnt;								// retry count for MQ
	long				timeout_wait_mq;							// wait timeout for MQ
	time_t				timeout_merge;								// wait timeout for merging
	msgid_t				base_msgid;
	long				chmpx_msg_count;							// MQ count for chmpx process
	PMQMSGHEADLIST		chmpx_msgs;									// MQ list for chmpx process
	long				activated_msg_count;						// Activated MQ count for client process
	PMQMSGHEADLIST		activated_msgs;								// Activated MQ list for client process
	long				assigned_msg_count;							// Assigned(NOT activated) MQ count for client process
	PMQMSGHEADLIST		assigned_msgs;								// Assigned(NOT activated) MQ list for client process
	long				free_msg_count;
	PMQMSGHEADLIST		free_msgs;
	msgid_t				last_msgid_chmpx;							// last random msgid for chmpx process
	msgid_t				last_msgid_activated;						// last random activated msgid for client process
	msgid_t				last_msgid_assigned;						// last random assigned msgid for client process
	PMQMSGHEADLIST		rel_chmpxmsgarea;							// same as in CHMSHM
	PCLTPROCLIST		client_pids;								// Client process(pid) list
	PCLTPROCLIST		free_pids;
	bool				k2h_fullmap;
	int					k2h_mask_bitcnt;
	int					k2h_cmask_bitcnt;
	int					k2h_max_element;
	long				histlog_count;								// history log count
}CHMINFO, *PCHMINFO;

//---------------------------------
// Login History Raw
typedef struct chm_log_raw{
	logtype_t		log_type;
	size_t			length;
	struct timespec	start_time;				// time for starting sending/receiving
	struct timespec	fin_time;				// time for end of sending/receiving
}CHMLOGRAW, *PCHMLOGRAW;

// Login History Area
typedef struct chm_log{
	bool			enable;
	time_t			start_time;				// time for enabled log
	time_t			stop_time;				// time for disabled log
	long			max_log_count;
	long			next_pos;
	PCHMLOGRAW		start_log_rel_area;
}CHMLOG, *PCHMLOG;

//---------------------------------
// Shared memory for chmpx
typedef struct chmshm{
	CHMINFO			info;
	PCHMPXLIST		rel_chmpxarea;
	PCHMPX*			rel_pchmpxarrarea;
	PMQMSGHEADLIST	rel_chmpxmsgarea;
	PCLTPROCLIST	rel_chmpxpidarea;
	PCHMSOCKLIST	rel_chmsockarea;
	CHMLOG			chmpxlog;
}CHMSHM, *PCHMSHM;

//---------------------------------------------------------
// Structure for communication
//---------------------------------------------------------
// Chmpx ssl information(notice: all member is byte order)
typedef struct chmpx_ssl{
	bool			is_ssl;
	bool			verify_peer;
	bool			is_ca_file;
	char			capath[CHM_MAX_PATH_LEN];
	char			server_cert[CHM_MAX_PATH_LEN];
	char			server_prikey[CHM_MAX_PATH_LEN];
	char			slave_cert[CHM_MAX_PATH_LEN];
	char			slave_prikey[CHM_MAX_PATH_LEN];
}CHMPX_ATTR_PACKED CHMPXSSL, *PCHMPXSSL;

// Chmpx server information
typedef struct chmpx_server{
	chmpxid_t		chmpxid;
	char			name[NI_MAXHOST];					// = 1025 for getnameinfo()
	chmhash_t		base_hash;
	chmhash_t		pending_hash;
	short			port;
	short			ctlport;
	CHMPXSSL		ssl;
	time_t			last_status_time;
	chmpxsts_t		status;
	char			cuk[CUK_MAX];						// adding after v1.0.71
	char			custom_seed[CUSTOM_ID_SEED_MAX];	// adding after v1.0.71
	CHMPXHP_RAWPAIR	endpoints[EXTERNAL_EP_MAX];			// adding after v1.0.71
	CHMPXHP_RAWPAIR	ctlendpoints[EXTERNAL_EP_MAX];		// adding after v1.0.71
	CHMPXHP_RAWPAIR	forward_peers[FORWARD_PEER_MAX];	// adding after v1.0.71
	CHMPXHP_RAWPAIR	reverse_peers[REVERSE_PEER_MAX];	// adding after v1.0.71
}CHMPX_ATTR_PACKED CHMPXSVR, *PCHMPXSVR;

// Old chmpx server information for backward compatibility
typedef struct chmpx_server_v1{
	chmpxid_t		chmpxid;
	char			name[NI_MAXHOST];					// = 1025 for getnameinfo()
	chmhash_t		base_hash;
	chmhash_t		pending_hash;
	short			port;
	short			ctlport;
	CHMPXSSL		ssl;
	time_t			last_status_time;
	chmpxsts_t		status;
}CHMPX_ATTR_PACKED CHMPXSVRV1, *PCHMPXSVRV1;

#define	COPY_PCHMPXSVR(pdest, psrc)	\
		{ \
			(pdest)->chmpxid					= (psrc)->chmpxid; \
			(pdest)->base_hash					= (psrc)->base_hash; \
			(pdest)->pending_hash				= (psrc)->pending_hash; \
			(pdest)->port						= (psrc)->port; \
			(pdest)->ctlport					= (psrc)->ctlport; \
			(pdest)->ssl.is_ssl					= (psrc)->ssl.is_ssl; \
			(pdest)->ssl.verify_peer			= (psrc)->ssl.verify_peer; \
			(pdest)->ssl.is_ca_file				= (psrc)->ssl.is_ca_file; \
			(pdest)->last_status_time			= (psrc)->last_status_time; \
			(pdest)->status						= (psrc)->status; \
			memcpy((pdest)->name,				(psrc)->name,				NI_MAXHOST); \
			memcpy((pdest)->ssl.capath,			(psrc)->ssl.capath,			CHM_MAX_PATH_LEN); \
			memcpy((pdest)->ssl.server_cert,	(psrc)->ssl.server_cert,	CHM_MAX_PATH_LEN); \
			memcpy((pdest)->ssl.server_prikey,	(psrc)->ssl.server_prikey,	CHM_MAX_PATH_LEN); \
			memcpy((pdest)->ssl.slave_cert,		(psrc)->ssl.slave_cert,		CHM_MAX_PATH_LEN); \
			memcpy((pdest)->ssl.slave_prikey,	(psrc)->ssl.slave_prikey,	CHM_MAX_PATH_LEN); \
			memcpy((pdest)->cuk,				(psrc)->cuk,				CUK_MAX); \
			memcpy((pdest)->custom_seed,		(psrc)->custom_seed,		CUSTOM_ID_SEED_MAX); \
			{ \
				int	_copy_chmpxsvr_cnt; \
				for(_copy_chmpxsvr_cnt = 0; _copy_chmpxsvr_cnt < EXTERNAL_EP_MAX; ++_copy_chmpxsvr_cnt){ \
					memcpy((pdest)->endpoints[_copy_chmpxsvr_cnt].name, (psrc)->endpoints[_copy_chmpxsvr_cnt].name, NI_MAXHOST); \
					(pdest)->endpoints[_copy_chmpxsvr_cnt].port = (psrc)->endpoints[_copy_chmpxsvr_cnt].port; \
				} \
				for(_copy_chmpxsvr_cnt = 0; _copy_chmpxsvr_cnt < EXTERNAL_EP_MAX; ++_copy_chmpxsvr_cnt){ \
					memcpy((pdest)->ctlendpoints[_copy_chmpxsvr_cnt].name, (psrc)->ctlendpoints[_copy_chmpxsvr_cnt].name, NI_MAXHOST); \
					(pdest)->ctlendpoints[_copy_chmpxsvr_cnt].port = (psrc)->ctlendpoints[_copy_chmpxsvr_cnt].port; \
				} \
				for(_copy_chmpxsvr_cnt = 0; _copy_chmpxsvr_cnt < FORWARD_PEER_MAX; ++_copy_chmpxsvr_cnt){ \
					memcpy((pdest)->forward_peers[_copy_chmpxsvr_cnt].name, (psrc)->forward_peers[_copy_chmpxsvr_cnt].name, NI_MAXHOST); \
					(pdest)->forward_peers[_copy_chmpxsvr_cnt].port = (psrc)->forward_peers[_copy_chmpxsvr_cnt].port; \
				} \
				for(_copy_chmpxsvr_cnt = 0; _copy_chmpxsvr_cnt < REVERSE_PEER_MAX; ++_copy_chmpxsvr_cnt){ \
					memcpy((pdest)->reverse_peers[_copy_chmpxsvr_cnt].name, (psrc)->reverse_peers[_copy_chmpxsvr_cnt].name, NI_MAXHOST); \
					(pdest)->reverse_peers[_copy_chmpxsvr_cnt].port = (psrc)->reverse_peers[_copy_chmpxsvr_cnt].port; \
				} \
			} \
		}

#define	COPY_PCHMPXSVRV1(pdest, psrc)	\
		{ \
			(pdest)->chmpxid					= (psrc)->chmpxid; \
			(pdest)->base_hash					= (psrc)->base_hash; \
			(pdest)->pending_hash				= (psrc)->pending_hash; \
			(pdest)->port						= (psrc)->port; \
			(pdest)->ctlport					= (psrc)->ctlport; \
			(pdest)->ssl.is_ssl					= (psrc)->ssl.is_ssl; \
			(pdest)->ssl.verify_peer			= (psrc)->ssl.verify_peer; \
			(pdest)->ssl.is_ca_file				= (psrc)->ssl.is_ca_file; \
			(pdest)->last_status_time			= (psrc)->last_status_time; \
			(pdest)->status						= (psrc)->status; \
			memcpy((pdest)->name,				(psrc)->name,				NI_MAXHOST); \
			memcpy((pdest)->ssl.capath,			(psrc)->ssl.capath,			CHM_MAX_PATH_LEN); \
			memcpy((pdest)->ssl.server_cert,	(psrc)->ssl.server_cert,	CHM_MAX_PATH_LEN); \
			memcpy((pdest)->ssl.server_prikey,	(psrc)->ssl.server_prikey,	CHM_MAX_PATH_LEN); \
			memcpy((pdest)->ssl.slave_cert,		(psrc)->ssl.slave_cert,		CHM_MAX_PATH_LEN); \
			memcpy((pdest)->ssl.slave_prikey,	(psrc)->ssl.slave_prikey,	CHM_MAX_PATH_LEN); \
		}

#define	HTON_PCHMPXSVR(pdata)	\
		{ \
			(pdata)->chmpxid			= htobe64((pdata)->chmpxid); \
			(pdata)->base_hash			= htobe64((pdata)->base_hash); \
			(pdata)->pending_hash		= htobe64((pdata)->pending_hash); \
			(pdata)->port				= htobe16((pdata)->port); \
			(pdata)->ctlport			= htobe16((pdata)->ctlport); \
			(pdata)->last_status_time	= htobe64((pdata)->last_status_time); \
			(pdata)->status				= htobe64((pdata)->status); \
			{ \
				int	_hton_hostport_pair_cnt; \
				for(_hton_hostport_pair_cnt = 0; _hton_hostport_pair_cnt < EXTERNAL_EP_MAX; ++_hton_hostport_pair_cnt){ \
					(pdata)->endpoints[_hton_hostport_pair_cnt].port = htobe16((pdata)->endpoints[_hton_hostport_pair_cnt].port); \
				} \
				for(_hton_hostport_pair_cnt = 0; _hton_hostport_pair_cnt < EXTERNAL_EP_MAX; ++_hton_hostport_pair_cnt){ \
					(pdata)->ctlendpoints[_hton_hostport_pair_cnt].port = htobe16((pdata)->ctlendpoints[_hton_hostport_pair_cnt].port); \
				} \
				for(_hton_hostport_pair_cnt = 0; _hton_hostport_pair_cnt < FORWARD_PEER_MAX; ++_hton_hostport_pair_cnt){ \
					(pdata)->forward_peers[_hton_hostport_pair_cnt].port = htobe16((pdata)->forward_peers[_hton_hostport_pair_cnt].port); \
				} \
				for(_hton_hostport_pair_cnt = 0; _hton_hostport_pair_cnt < REVERSE_PEER_MAX; ++_hton_hostport_pair_cnt){ \
					(pdata)->reverse_peers[_hton_hostport_pair_cnt].port = htobe16((pdata)->reverse_peers[_hton_hostport_pair_cnt].port); \
				} \
			} \
		}

#define	HTON_PCHMPXSVRV1(pdata)	\
		{ \
			(pdata)->chmpxid			= htobe64((pdata)->chmpxid); \
			(pdata)->base_hash			= htobe64((pdata)->base_hash); \
			(pdata)->pending_hash		= htobe64((pdata)->pending_hash); \
			(pdata)->port				= htobe16((pdata)->port); \
			(pdata)->ctlport			= htobe16((pdata)->ctlport); \
			(pdata)->last_status_time	= htobe64((pdata)->last_status_time); \
			(pdata)->status				= htobe64((pdata)->status); \
		}

#define	NTOH_PCHMPXSVR(pdata)	\
		{ \
			(pdata)->chmpxid			= be64toh((pdata)->chmpxid); \
			(pdata)->base_hash			= be64toh((pdata)->base_hash); \
			(pdata)->pending_hash		= be64toh((pdata)->pending_hash); \
			(pdata)->port				= be16toh((pdata)->port); \
			(pdata)->ctlport			= be16toh((pdata)->ctlport); \
			(pdata)->last_status_time	= be64toh((pdata)->last_status_time); \
			(pdata)->status				= be64toh((pdata)->status); \
			{ \
				int	_hton_hostport_pair_cnt; \
				for(_hton_hostport_pair_cnt = 0; _hton_hostport_pair_cnt < EXTERNAL_EP_MAX; ++_hton_hostport_pair_cnt){ \
					(pdata)->endpoints[_hton_hostport_pair_cnt].port = be16toh((pdata)->endpoints[_hton_hostport_pair_cnt].port); \
				} \
				for(_hton_hostport_pair_cnt = 0; _hton_hostport_pair_cnt < EXTERNAL_EP_MAX; ++_hton_hostport_pair_cnt){ \
					(pdata)->ctlendpoints[_hton_hostport_pair_cnt].port = be16toh((pdata)->ctlendpoints[_hton_hostport_pair_cnt].port); \
				} \
				for(_hton_hostport_pair_cnt = 0; _hton_hostport_pair_cnt < FORWARD_PEER_MAX; ++_hton_hostport_pair_cnt){ \
					(pdata)->forward_peers[_hton_hostport_pair_cnt].port = be16toh((pdata)->forward_peers[_hton_hostport_pair_cnt].port); \
				} \
				for(_hton_hostport_pair_cnt = 0; _hton_hostport_pair_cnt < REVERSE_PEER_MAX; ++_hton_hostport_pair_cnt){ \
					(pdata)->reverse_peers[_hton_hostport_pair_cnt].port = be16toh((pdata)->reverse_peers[_hton_hostport_pair_cnt].port); \
				} \
			} \
		}

#define	NTOH_PCHMPXSVRV1(pdata)	\
		{ \
			(pdata)->chmpxid			= be64toh((pdata)->chmpxid); \
			(pdata)->base_hash			= be64toh((pdata)->base_hash); \
			(pdata)->pending_hash		= be64toh((pdata)->pending_hash); \
			(pdata)->port				= be16toh((pdata)->port); \
			(pdata)->ctlport			= be16toh((pdata)->ctlport); \
			(pdata)->last_status_time	= be64toh((pdata)->last_status_time); \
			(pdata)->status				= be64toh((pdata)->status); \
		}

#define	CVT_PCHMPXSVRV1_TO_PCHMPXSVR(pv2, pv1)	\
		{ \
			(pv2)->chmpxid					= (pv1)->chmpxid; \
			(pv2)->base_hash				= (pv1)->base_hash; \
			(pv2)->pending_hash				= (pv1)->pending_hash; \
			(pv2)->port						= (pv1)->port; \
			(pv2)->ctlport					= (pv1)->ctlport; \
			(pv2)->ssl.is_ssl				= (pv1)->ssl.is_ssl; \
			(pv2)->ssl.verify_peer			= (pv1)->ssl.verify_peer; \
			(pv2)->ssl.is_ca_file			= (pv1)->ssl.is_ca_file; \
			(pv2)->last_status_time			= (pv1)->last_status_time; \
			(pv2)->status					= (pv1)->status; \
			memcpy((pv2)->name,				(pv1)->name,				NI_MAXHOST); \
			memcpy((pv2)->ssl.capath,		(pv1)->ssl.capath,			CHM_MAX_PATH_LEN); \
			memcpy((pv2)->ssl.server_cert,	(pv1)->ssl.server_cert,		CHM_MAX_PATH_LEN); \
			memcpy((pv2)->ssl.server_prikey,(pv1)->ssl.server_prikey,	CHM_MAX_PATH_LEN); \
			memcpy((pv2)->ssl.slave_cert,	(pv1)->ssl.slave_cert,		CHM_MAX_PATH_LEN); \
			memcpy((pv2)->ssl.slave_prikey,	(pv1)->ssl.slave_prikey,	CHM_MAX_PATH_LEN); \
			memset((pv2)->cuk,				0,							CUK_MAX); \
			memset((pv2)->custom_seed,		0,							CUSTOM_ID_SEED_MAX); \
			{ \
				int	_copy_hostport_pair_cnt; \
				for(_copy_hostport_pair_cnt = 0; _copy_hostport_pair_cnt < EXTERNAL_EP_MAX; ++_copy_hostport_pair_cnt){ \
					memset((pv2)->endpoints[_copy_hostport_pair_cnt].name, 0, NI_MAXHOST); \
					(pv2)->endpoints[_copy_hostport_pair_cnt].port = CHM_INVALID_PORT; \
				} \
				for(_copy_hostport_pair_cnt = 0; _copy_hostport_pair_cnt < EXTERNAL_EP_MAX; ++_copy_hostport_pair_cnt){ \
					memset((pv2)->ctlendpoints[_copy_hostport_pair_cnt].name, 0, NI_MAXHOST); \
					(pv2)->ctlendpoints[_copy_hostport_pair_cnt].port = CHM_INVALID_PORT; \
				} \
				for(_copy_hostport_pair_cnt = 0; _copy_hostport_pair_cnt < FORWARD_PEER_MAX; ++_copy_hostport_pair_cnt){ \
					memset((pv2)->forward_peers[_copy_hostport_pair_cnt].name, 0, NI_MAXHOST); \
					(pv2)->forward_peers[_copy_hostport_pair_cnt].port = CHM_INVALID_PORT; \
				} \
				for(_copy_hostport_pair_cnt = 0; _copy_hostport_pair_cnt < REVERSE_PEER_MAX; ++_copy_hostport_pair_cnt){ \
					memset((pv2)->reverse_peers[_copy_hostport_pair_cnt].name, 0, NI_MAXHOST); \
					(pv2)->reverse_peers[_copy_hostport_pair_cnt].port = CHM_INVALID_PORT; \
				} \
			} \
		}

#define	CVT_PCHMPXSVR_TO_PCHMPXSVRV1(pv1, pv2)	\
		{ \
			(pv1)->chmpxid					= (pv2)->chmpxid; \
			(pv1)->base_hash				= (pv2)->base_hash; \
			(pv1)->pending_hash				= (pv2)->pending_hash; \
			(pv1)->port						= (pv2)->port; \
			(pv1)->ctlport					= (pv2)->ctlport; \
			(pv1)->ssl.is_ssl				= (pv2)->ssl.is_ssl; \
			(pv1)->ssl.verify_peer			= (pv2)->ssl.verify_peer; \
			(pv1)->ssl.is_ca_file			= (pv2)->ssl.is_ca_file; \
			(pv1)->last_status_time			= (pv2)->last_status_time; \
			(pv1)->status					= (pv2)->status; \
			memcpy((pv1)->name,				(pv2)->name,				NI_MAXHOST); \
			memcpy((pv1)->ssl.capath,		(pv2)->ssl.capath,			CHM_MAX_PATH_LEN); \
			memcpy((pv1)->ssl.server_cert,	(pv2)->ssl.server_cert,		CHM_MAX_PATH_LEN); \
			memcpy((pv1)->ssl.server_prikey,(pv2)->ssl.server_prikey,	CHM_MAX_PATH_LEN); \
			memcpy((pv1)->ssl.slave_cert,	(pv2)->ssl.slave_cert,		CHM_MAX_PATH_LEN); \
			memcpy((pv1)->ssl.slave_prikey,	(pv2)->ssl.slave_prikey,	CHM_MAX_PATH_LEN); \
		}

DECL_EXTERN_C_END

#endif	// CHMSTRUCTURE_H

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
