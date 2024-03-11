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
#define	STR_CHMDBG_SILENT	"SLT"
#define	STR_CHMDBG_ERR		"ERR"
#define	STR_CHMDBG_WARN		"WAN"
#define	STR_CHMDBG_MSG		"MSG"
#define	STR_CHMDBG_DUMP		"DMP"

#define	ChmDbgMode_STR(mode)	CHMDBG_SILENT	== mode ? STR_CHMDBG_SILENT	: \
								CHMDBG_ERR		== mode ? STR_CHMDBG_ERR	: \
								CHMDBG_WARN		== mode ? STR_CHMDBG_WARN	: \
								CHMDBG_MSG		== mode ? STR_CHMDBG_MSG	: \
								CHMDBG_DUMP		== mode ? STR_CHMDBG_DUMP	: ""

#define	LOW_CHMPRINT(strmode, fmt, ...) \
		fprintf((chm_dbg_fp ? chm_dbg_fp : stderr), "[CHMPX-%s] %s(%d) : " fmt "%s\n", strmode, __func__, __LINE__, __VA_ARGS__);

#define	CHMPRINT(mode, strmode, ...) \
		if((static_cast<int>(chm_debug_mode) & static_cast<int>(mode)) == static_cast<int>(mode)){ \
			LOW_CHMPRINT(strmode, __VA_ARGS__); \
		}

#define	SLT_CHMPRN(...)			CHMPRINT(CHMDBG_SILENT,	STR_CHMDBG_SILENT,	##__VA_ARGS__, "")	// This means nothing...
#define	ERR_CHMPRN(...)			CHMPRINT(CHMDBG_ERR,	STR_CHMDBG_ERR,		##__VA_ARGS__, "")
#define	WAN_CHMPRN(...)			CHMPRINT(CHMDBG_WARN,	STR_CHMDBG_WARN,	##__VA_ARGS__, "")
#define	MSG_CHMPRN(...)			CHMPRINT(CHMDBG_MSG,	STR_CHMDBG_MSG,		##__VA_ARGS__, "")
#define	DMP_CHMPRN(...)			CHMPRINT(CHMDBG_DUMP,	STR_CHMDBG_DUMP,	##__VA_ARGS__, "")

//
// If using following macro, need to specify include time.h.
//
#define	CLOCKTIME_CHMPRN(phead)	\
		if(((static_cast<int>(chm_debug_mode) & (static_cast<int>(CHMDBG_DUMP)) == (static_cast<int>(CHMDBG_DUMP)){ \
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
