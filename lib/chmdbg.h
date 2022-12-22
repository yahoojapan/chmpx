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
#ifndef	CHMDBG_H
#define CHMDBG_H

#include "chmcommon.h"

DECL_EXTERN_C_START

//---------------------------------------------------------
// Debug
//---------------------------------------------------------
typedef enum chm_dbg_mode{
	CHMDBG_SILENT	= 0,
	CHMDBG_ERR		= 1,
	CHMDBG_WARN		= 3,
	CHMDBG_MSG		= 7,
	CHMDBG_DUMP		= 15
}ChmDbgMode;

extern ChmDbgMode	chm_debug_mode;		// Do not use directly this variable.
extern FILE*		chm_dbg_fp;

ChmDbgMode SetChmDbgMode(ChmDbgMode mode);
ChmDbgMode BumpupChmDbgMode(void);
ChmDbgMode GetChmDbgMode(void);
bool LoadChmDbgEnv(void);
bool SetChmDbgFile(const char* filepath);
bool UnsetChmDbgFile(void);

//---------------------------------------------------------
// Debugging Macros
//---------------------------------------------------------
#define	ChmDbgMode_STR(mode)	CHMDBG_SILENT	== mode ? "SLT" : \
								CHMDBG_ERR		== mode ? "ERR" : \
								CHMDBG_WARN		== mode ? "WAN" : \
								CHMDBG_MSG		== mode ? "MSG" : \
								CHMDBG_DUMP		== mode ? "DMP" : ""

#define	LOW_CHMPRINT(mode, fmt, ...) \
		fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "[CHMPX-%s] %s(%d) : " fmt "%s\n", ChmDbgMode_STR(mode), __func__, __LINE__, __VA_ARGS__);

#define	CHMPRINT(mode, ...) \
		if((chm_debug_mode & mode) == mode){ \
			LOW_CHMPRINT(mode, __VA_ARGS__); \
		}

#define	SLT_CHMPRN(...)			CHMPRINT(CHMDBG_SILENT,	##__VA_ARGS__, "")	// This means nothing...
#define	ERR_CHMPRN(...)			CHMPRINT(CHMDBG_ERR,	##__VA_ARGS__, "")
#define	WAN_CHMPRN(...)			CHMPRINT(CHMDBG_WARN,	##__VA_ARGS__, "")
#define	MSG_CHMPRN(...)			CHMPRINT(CHMDBG_MSG,	##__VA_ARGS__, "")
#define	DMP_CHMPRN(...)			CHMPRINT(CHMDBG_DUMP,	##__VA_ARGS__, "")

//
// If using following macro, need to specify include time.h.
//
#define	CLOCKTIME_CHMPRN(phead)	\
		if((chm_debug_mode & CHMDBG_DUMP) == CHMDBG_DUMP){ \
			struct timespec	reqtime; \
			if(-1 == clock_gettime(CLOCK_REALTIME_COARSE, &reqtime)){ \
				fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "[CHMPX-TIME-%s] ---s ---ms ---us ---ns\n", (phead ? phead : "DBG")); \
			}else{ \
				fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "[CHMPX-TIME-%s] %03jds %03ldms %03ldus %03ldns\n", (phead ? phead : "DBG"), static_cast<intmax_t>(reqtime.tv_sec), (reqtime.tv_nsec / (1000 * 1000)), ((reqtime.tv_nsec % (1000 * 1000)) / 1000), (reqtime.tv_nsec % 1000)); \
			} \
		}

DECL_EXTERN_C_END

#endif	// CHMDBG_H

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noexpandtab sw=4 ts=4 fdm=marker
 * vim<600: noexpandtab sw=4 ts=4
 */
